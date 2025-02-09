/**
  ******************************************************************************
  * @file    sto_speed_pos_fdbk.c
  * @author  Motor Control SDK Team, ST Microelectronics
  * @brief   This file provides firmware functions that implement the features
  *          of the State Observer + PLL Speed & Position Feedback component of the
  *          Motor Control SDK.
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "sto_speed_pos_fdbk.h"
#include "mc_math.h"



/** @addtogroup MCSDK
  * @{
  */

/** @addtogroup SpeednPosFdbk
  * @{
  */

/** @defgroup SpeednPosFdbk_STO State Observer Speed & Position Feedback
  * @brief State Observer with PLL Speed & Position Feedback implementation
  *
  * This component uses a State Observer coupled with a software PLL to provide an estimation of
  * the speed and the position of the rotor of the motor.
  *
  * @todo Document the State Observer + PLL Speed & Position Feedback "module".
  * @{
  */
    
/* Private defines -----------------------------------------------------------*/   
#define C6_COMP_CONST1  (int32_t) 1043038
#define C6_COMP_CONST2  (int32_t) 10430

/* Private function prototypes -----------------------------------------------*/
 static void STO_Store_Rotor_Speed(STO_Handle_t *pHandle, int16_t hRotor_Speed);
 static int16_t STO_ExecutePLL(STO_Handle_t *pHandle, int16_t hBemf_alfa_est, 
                               int16_t hBemf_beta_est);
 static void STO_InitSpeedBuffer(STO_Handle_t *pHandle);
 

/**
  * @brief  It initializes the state observer component 
  * @param  pHandle: handler of the current instance of the STO component
  * @retval none
  */
void STO_Init(STO_Handle_t *pHandle)
{
  int16_t htempk;
  int32_t wAux;
   
  
  pHandle->ConsistencyCounter = pHandle->StartUpConsistThreshold;
  pHandle->EnableDualCheck = true;
  
  wAux = (int32_t)1;
  pHandle->F3POW2 = 0u;
  
  htempk = (int16_t)(C6_COMP_CONST1/(pHandle->hF2));
  
  while (htempk != 0)
  {
    htempk /= (int16_t)2;
    wAux *=(int32_t)2;
    pHandle->F3POW2++;
  }
  
  pHandle->hF3 = (int16_t)wAux;
  wAux = (int32_t)(pHandle->hF2) * pHandle->hF3;
  pHandle->hC6 = (int16_t)(wAux/C6_COMP_CONST2);
  
  STO_Clear(pHandle);
  
  PID_HandleInit( & pHandle->PIRegulator );
  
  /* Acceleration measurement set to zero */
  pHandle->_Super.hMecAccel01HzP = 0;
  
  return;
}

/**
  * @brief  It only returns, necessary to implement fictitious IRQ_Handler
  * @param  pHandle: handler of the current instance of the STO component
  * @param  uint8_t Fictitious interrupt flag
  * @retval none
  */

void STO_Return(STO_Handle_t *pHandle, uint8_t flag)
{
  return;
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM)
__attribute__((section ("ccmram")))
#endif
#endif
/**
  * @brief  This method executes Luenberger state observer equations and calls 
  *         PLL with the purpose of computing a new speed estimation and 
  *         updating the estimated electrical angle. 
  * @param  pHandle: handler of the current instance of the STO component
  * @param  pInputVars_str pointer to the observer inputs structure 
  * @retval int16_t rotor electrical angle (s16Degrees)
  */
int16_t STO_CalcElAngle(STO_Handle_t *pHandle, Observer_Inputs_t *pInputs)
{
  int32_t wAux, wDirection;
  int32_t wIalfa_est_Next,wIbeta_est_Next;
  int32_t wBemf_alfa_est_Next, wBemf_beta_est_Next;
  int16_t hAux, hAux_Alfa, hAux_Beta, hIalfa_err, hIbeta_err, hRotor_Speed, 
                                                                 hValfa, hVbeta;
  
  
  if (pHandle->wBemf_alfa_est > (int32_t)(pHandle->hF2)*INT16_MAX)
  {
    pHandle->wBemf_alfa_est = INT16_MAX * (int32_t)(pHandle->hF2);
  }
  else if (pHandle->wBemf_alfa_est <= -INT16_MAX * (int32_t)(pHandle->hF2))
  {
    pHandle->wBemf_alfa_est = -INT16_MAX * (int32_t)(pHandle->hF2);
  }
  else
  {
  }
#ifdef FULL_MISRA_C_COMPLIANCY
  hAux_Alfa = (int16_t)(pHandle->wBemf_alfa_est/pHandle->hF2);
#else
  hAux_Alfa = (int16_t)(pHandle->wBemf_alfa_est>>pHandle->F2LOG);
#endif
  
  if (pHandle->wBemf_beta_est > INT16_MAX * (int32_t)(pHandle->hF2))
  {
    pHandle->wBemf_beta_est = INT16_MAX * (int32_t)(pHandle->hF2);
  }
  else if (pHandle->wBemf_beta_est <= -INT16_MAX * (int32_t)(pHandle->hF2))
  {
    pHandle->wBemf_beta_est = -INT16_MAX * (int32_t)(pHandle->hF2);
  }
  else
  {
  }
#ifdef FULL_MISRA_C_COMPLIANCY
  hAux_Beta = (int16_t)(pHandle->wBemf_beta_est/pHandle->hF2);
#else
  hAux_Beta = (int16_t)(pHandle->wBemf_beta_est>>pHandle->F2LOG);
#endif
  
  if (pHandle->Ialfa_est > INT16_MAX * (int32_t)(pHandle->hF1))
  {
    pHandle->Ialfa_est = INT16_MAX * (int32_t)(pHandle->hF1);
  }
  else if (pHandle->Ialfa_est <= -INT16_MAX * (int32_t)(pHandle->hF1))
  {
    pHandle->Ialfa_est = -INT16_MAX * (int32_t)(pHandle->hF1);
  }
  else
  {
  } 
    
  if (pHandle->Ibeta_est > INT16_MAX * (int32_t)(pHandle->hF1))
  {
    pHandle->Ibeta_est = INT16_MAX * (int32_t)(pHandle->hF1);
  }
  else if (pHandle->Ibeta_est <= -INT16_MAX * (int32_t)(pHandle->hF1))
  {
    pHandle->Ibeta_est = -INT16_MAX * (int32_t)(pHandle->hF1);
  }   
  else
  {
  }
  
#ifdef FULL_MISRA_C_COMPLIANCY
  hIalfa_err = (int16_t)(pHandle->Ialfa_est/pHandle->hF1);
#else
  hIalfa_err = (int16_t)(pHandle->Ialfa_est>>pHandle->F1LOG);
#endif
  
  hIalfa_err = hIalfa_err - pInputs->Ialfa_beta.qI_Component1;
  
#ifdef FULL_MISRA_C_COMPLIANCY
  hIbeta_err = (int16_t)(pHandle->Ibeta_est/pHandle->hF1);
#else
  hIbeta_err = (int16_t)(pHandle->Ibeta_est>>pHandle->F1LOG);
#endif
  
  hIbeta_err = hIbeta_err - pInputs->Ialfa_beta.qI_Component2;  
  
  wAux = (int32_t)(pInputs->Vbus) * pInputs->Valfa_beta.qV_Component1; 
#ifdef FULL_MISRA_C_COMPLIANCY
  hValfa = (int16_t) (wAux /65536);  
#else
  hValfa = (int16_t) (wAux >> 16);
#endif
  
  wAux = (int32_t)(pInputs->Vbus) * pInputs->Valfa_beta.qV_Component2; 
#ifdef FULL_MISRA_C_COMPLIANCY
  hVbeta = (int16_t) (wAux /65536);  
#else  
  hVbeta = (int16_t) (wAux >>16);  
#endif
  
  /*alfa axes observer*/
#ifdef FULL_MISRA_C_COMPLIANCY
  hAux = (int16_t) (pHandle->Ialfa_est/pHandle->hF1);
#else
  hAux = (int16_t) (pHandle->Ialfa_est>>pHandle->F1LOG);
#endif
  
  wAux = (int32_t) (pHandle->hC1)* hAux; 
  wIalfa_est_Next = pHandle->Ialfa_est - wAux;
  
  wAux = (int32_t) (pHandle->hC2) * hIalfa_err;
  wIalfa_est_Next += wAux;
  
  wAux = (int32_t) (pHandle->hC5) * hValfa;
  wIalfa_est_Next += wAux;
  
  wAux = (int32_t)  (pHandle->hC3)* hAux_Alfa;
  wIalfa_est_Next -=wAux;
  
  wAux = (int32_t)(pHandle->hC4) *hIalfa_err;
  wBemf_alfa_est_Next = pHandle->wBemf_alfa_est + wAux;

#ifdef FULL_MISRA_C_COMPLIANCY
  wAux = (int32_t) hAux_Beta/pHandle->hF3;
#else
	wAux = (int32_t) hAux_Beta >> pHandle->F3POW2;
#endif
  
  wAux = wAux * pHandle->hC6;
  wAux = pHandle->_Super.hElSpeedDpp * wAux;
  wBemf_alfa_est_Next += wAux;
      
 /*beta axes observer*/  
#ifdef FULL_MISRA_C_COMPLIANCY
  hAux = (int16_t) (pHandle->Ibeta_est/pHandle->hF1);
#else
  hAux = (int16_t) (pHandle->Ibeta_est>>pHandle->F1LOG);
#endif
  
  wAux = (int32_t)  (pHandle->hC1)* hAux;
  wIbeta_est_Next = pHandle->Ibeta_est - wAux;
  
  wAux = (int32_t) (pHandle->hC2) * hIbeta_err;
  wIbeta_est_Next += wAux;
  
  wAux = (int32_t) (pHandle->hC5) * hVbeta;
  wIbeta_est_Next += wAux;  
  
  wAux = (int32_t)  (pHandle->hC3)* hAux_Beta;
  wIbeta_est_Next -=wAux;
  
  wAux = (int32_t)(pHandle->hC4) *hIbeta_err;
  wBemf_beta_est_Next = pHandle->wBemf_beta_est + wAux;
  
#ifdef FULL_MISRA_C_COMPLIANCY
  wAux = (int32_t)hAux_Alfa / pHandle->hF3;
#else
	wAux = (int32_t) hAux_Alfa >> pHandle->F3POW2;
#endif
  
  wAux = wAux * pHandle->hC6;
  wAux = pHandle->_Super.hElSpeedDpp * wAux;
  wBemf_beta_est_Next -= wAux;
  
  if (pHandle->_Super.hElSpeedDpp >=0)
  {
    wDirection = 1;
  }
  else
  {
    wDirection = -1;
  }  
    
  /*Calls the PLL blockset*/
  pHandle->hBemf_alfa_est = hAux_Alfa;
  pHandle->hBemf_beta_est = hAux_Beta;
  
  hAux_Alfa = (int16_t)(hAux_Alfa * wDirection);
  hAux_Beta = (int16_t)(hAux_Beta * wDirection);
  
  hRotor_Speed = STO_ExecutePLL(pHandle, hAux_Alfa, -hAux_Beta);
  
  STO_Store_Rotor_Speed(pHandle, hRotor_Speed);
  
  pHandle->_Super.hElAngle +=hRotor_Speed;
        
  /*storing previous values of currents and bemfs*/
  pHandle->Ialfa_est = wIalfa_est_Next;
  pHandle->wBemf_alfa_est = wBemf_alfa_est_Next;
  
  pHandle->Ibeta_est = wIbeta_est_Next;
  pHandle->wBemf_beta_est = wBemf_beta_est_Next;
  
  return (pHandle->_Super.hElAngle);
}

/**
  * @brief  This method must be called - at least - with the same periodicity
  *         on which speed control is executed. It computes and returns - through
  *         parameter hMecSpeed01Hz - the rotor average mechanical speed,
  *         expressed in 01Hz. Average is computed considering a FIFO depth 
  *         equal to bSpeedBufferSize01Hz. Moreover it also computes and returns
  *         the reliability state of the sensor.
  * @param  pHandle: handler of the current instance of the STO component
  * @param  pMecSpeed01Hz pointer to int16_t, used to return the rotor average
  *         mechanical speed (01Hz)
  * @retval bool speed sensor reliability, measured with reference to parameters 
  *         bReliability_hysteresys, hVariancePercentage and bSpeedBufferSize
  *         true = sensor information is reliable
  *         false = sensor information is not reliable
  */

bool STO_CalcAvrgMecSpeed01Hz(STO_Handle_t *pHandle, int16_t *pMecSpeed01Hz)
{
  int32_t wAvrSpeed_dpp = (int32_t)0;
  int32_t wError, wAux, wAvrSquareSpeed, wAvrQuadraticError=0;
  uint8_t i, bSpeedBufferSize01Hz = pHandle->SpeedBufferSize01Hz;
  int32_t wObsBemf, wEstBemf;
  int32_t wObsBemfSq = 0, wEstBemfSq = 0;
  int32_t wEstBemfSqLo;
  
  bool bIs_Speed_Reliable = false, bAux = false;
  bool bIs_Bemf_Consistent = false;
  
  for (i=0u; i<bSpeedBufferSize01Hz; i++)
  {
    wAvrSpeed_dpp += (int32_t)(pHandle->Speed_Buffer[i]);
  }
  
  wAvrSpeed_dpp = wAvrSpeed_dpp / (int16_t)bSpeedBufferSize01Hz;
 
  for (i=0u; i < bSpeedBufferSize01Hz; i++)
  {
    wError = (int32_t)(pHandle->Speed_Buffer[i]) - wAvrSpeed_dpp;
    wError = (wError * wError);
    wAvrQuadraticError += wError;
  }
  
  /*It computes the measurement variance   */
  wAvrQuadraticError = wAvrQuadraticError / (int16_t)bSpeedBufferSize01Hz;
  
  /* The maximum variance acceptable is here calculated as a function of average 
     speed                                                                    */
  wAvrSquareSpeed = wAvrSpeed_dpp * wAvrSpeed_dpp;
  wAvrSquareSpeed = (wAvrSquareSpeed/(int16_t)128) * (int32_t)(pHandle->
                                                           VariancePercentage);
  
  if (wAvrQuadraticError < wAvrSquareSpeed)
  {
    bIs_Speed_Reliable = true;
  }

  /*Computation of Mechanical speed 01Hz*/
  wAux = wAvrSpeed_dpp * (int32_t)(pHandle->_Super.hMeasurementFrequency);
  wAux = wAux * (int32_t) 10;
  wAux = wAux/(int32_t)65536;
  wAux = wAux /(int16_t)(pHandle->_Super.bElToMecRatio);
  
  *pMecSpeed01Hz = (int16_t)wAux;
  pHandle->_Super.hAvrMecSpeed01Hz = (int16_t)wAux;
  
  pHandle->IsSpeedReliable = bIs_Speed_Reliable;
  
  /*Bemf Consistency Check algorithm*/
  if (pHandle->EnableDualCheck == true)  /*do algorithm if it's enabled*/
  {
    wAux = (wAux < 0 ? (-wAux) : (wAux));  /* wAux abs value   */
    if (wAux < (int32_t)(pHandle->MaxAppPositiveMecSpeed01Hz))
    {    
      /*Computation of Observed back-emf*/
      wObsBemf = (int32_t)(pHandle->hBemf_alfa_est);
      wObsBemfSq = wObsBemf * wObsBemf;
      wObsBemf = (int32_t)(pHandle->hBemf_beta_est);
      wObsBemfSq += wObsBemf * wObsBemf;
      
      /*Computation of Estimated back-emf*/    
      wEstBemf = (wAux * 32767)/(int16_t)(pHandle->_Super.hMaxReliableMecSpeed01Hz);
      wEstBemfSq = (wEstBemf * (int32_t)(pHandle->BemfConsistencyGain))/64;
      wEstBemfSq *= wEstBemf;
      
      /*Computation of threshold*/
      wEstBemfSqLo = wEstBemfSq -
        (wEstBemfSq/64)*(int32_t)(pHandle->BemfConsistencyCheck);
      
      /*Check*/
      if (wObsBemfSq > wEstBemfSqLo)
      {
        bIs_Bemf_Consistent = true;
      }
    }
    
    pHandle->IsBemfConsistent = bIs_Bemf_Consistent;
    pHandle->Obs_Bemf_Level = wObsBemfSq;
    pHandle->Est_Bemf_Level = wEstBemfSq;
  }
  else
  {
    bIs_Bemf_Consistent = true;
  }    
  
  /*Decision making*/
  if(pHandle->IsAlgorithmConverged == false)
  {
    bAux = SPD_IsMecSpeedReliable ( &pHandle->_Super, pMecSpeed01Hz);
  }
  else
  {
    if((pHandle->IsSpeedReliable == false)||(bIs_Bemf_Consistent == false))
    {
      pHandle->ReliabilityCounter++;
      if (pHandle->ReliabilityCounter >= pHandle->Reliability_hysteresys)
      {
        pHandle->ReliabilityCounter = 0u;
        pHandle->_Super.bSpeedErrorNumber = pHandle->_Super.bMaximumSpeedErrorsNumber;
        bAux = false;
      }
      else
      {
        bAux = SPD_IsMecSpeedReliable ( &pHandle->_Super, pMecSpeed01Hz);
      }
    }
    else
    {
      pHandle->ReliabilityCounter = 0u;
      bAux = SPD_IsMecSpeedReliable ( &pHandle->_Super, pMecSpeed01Hz);
    }
  }
  return(bAux);  
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM)
__attribute__((section ("ccmram")))
#endif
#endif
/**
  * @brief  This method must be called - at least - with the same periodicity
  *         on which speed control is executed. It computes and update component 
  *         variable hElSpeedDpp that is estimated average electrical speed
  *         expressed in dpp used for instance in observer equations. 
  *         Average is computed considering a FIFO depth equal to 
  *         bSpeedBufferSizedpp. 
  * @param  pHandle: handler of the current instance of the STO component
  * @retval none
  */
void STO_CalcAvrgElSpeedDpp(STO_Handle_t *pHandle)
{

  int16_t hIndexNew = (int16_t)pHandle->Speed_Buffer_Index;
  int16_t hIndexOld;
  int16_t hIndexOldTemp;
  int32_t wSum = pHandle->DppBufferSum;
  int32_t wAvrSpeed_dpp;
  int16_t hSpeedBufferSizedpp = (int16_t)(pHandle->SpeedBufferSizedpp);
  int16_t hSpeedBufferSize01Hz = (int16_t)(pHandle->SpeedBufferSize01Hz);
  int16_t hBufferSizeDiff;
  
  hBufferSizeDiff = hSpeedBufferSize01Hz - hSpeedBufferSizedpp;
  
  if (hBufferSizeDiff == 0)
  {
    wSum = wSum + pHandle->Speed_Buffer[hIndexNew] -
      pHandle->SpeedBufferOldestEl;
  }
  else
  {    
    hIndexOldTemp = hIndexNew + hBufferSizeDiff;
    
    if (hIndexOldTemp >= hSpeedBufferSize01Hz)
    {
      hIndexOld = hIndexOldTemp - hSpeedBufferSize01Hz;
    }
    else
    {
      hIndexOld = hIndexOldTemp;
    } 
    
    wSum = wSum + pHandle->Speed_Buffer[hIndexNew] -
      pHandle->Speed_Buffer[hIndexOld];
  }
  
#ifdef FULL_MISRA_C_COMPLIANCY
  wAvrSpeed_dpp = wSum / hSpeedBufferSizedpp;
#else
	wAvrSpeed_dpp = wSum >> pHandle->SpeedBufferSizedppLOG;
#endif
  
  pHandle->_Super.hElSpeedDpp = (int16_t)wAvrSpeed_dpp;
  pHandle->DppBufferSum = wSum;
}
   
/**
  * @brief  It clears state observer component by re-initializing private variables
  * @param  pHandle related object of class CSTO_SPD
  * @retval none
  */
void STO_Clear(STO_Handle_t *pHandle)
{
  pHandle->Ialfa_est = (int32_t)0;
  pHandle->Ibeta_est = (int32_t)0;
  pHandle->wBemf_alfa_est = (int32_t)0;
  pHandle->wBemf_beta_est = (int32_t)0;
  pHandle->_Super.hElAngle = (int16_t)0;
  pHandle->_Super.hElSpeedDpp = (int16_t)0;
  pHandle->ConsistencyCounter = 0u;  
  pHandle->ReliabilityCounter = 0u;  
  pHandle->IsAlgorithmConverged = false;
  pHandle->IsBemfConsistent = false;
  pHandle->Obs_Bemf_Level = (int32_t)0;
  pHandle->Est_Bemf_Level = (int32_t)0;
  pHandle->DppBufferSum = (int32_t)0;
  pHandle->ForceConvergency = false;
  pHandle->ForceConvergency2 = false;

  STO_InitSpeedBuffer(pHandle);
  PID_SetIntegralTerm( & pHandle->PIRegulator, (int32_t)0 );
}

/**
  * @brief  It stores in estimated speed FIFO latest calculated value of motor 
  *         speed
  * @param  pHandle: handler of the current instance of the STO component
  * @retval none
  */
#pragma inline
static void STO_Store_Rotor_Speed(STO_Handle_t *pHandle, int16_t hRotor_Speed)
{
 
  uint8_t bBuffer_index = pHandle->Speed_Buffer_Index;
  
  bBuffer_index++;
  if (bBuffer_index == pHandle->SpeedBufferSize01Hz) 
  {
    bBuffer_index = 0u;
  }

  pHandle->SpeedBufferOldestEl = pHandle->Speed_Buffer[bBuffer_index];  

  pHandle->Speed_Buffer[bBuffer_index] = hRotor_Speed;  
  pHandle->Speed_Buffer_Index = bBuffer_index; 
}

/**
  * @brief  It executes PLL algorithm for rotor position extraction from B-emf  
  *         alpha and beta
  * @param  pHandle: handler of the current instance of the STO component
  *         hBemf_alfa_est estimated Bemf alpha on the stator reference frame
  *         hBemf_beta_est estimated Bemf beta on the stator reference frame
  * @retval none
  */
#pragma inline
static int16_t STO_ExecutePLL(STO_Handle_t *pHandle, int16_t hBemf_alfa_est, int16_t 
                                                                 hBemf_beta_est)
{
  int32_t wAlfa_Sin_tmp, wBeta_Cos_tmp;
  int16_t hOutput;
  Trig_Components Local_Components;
  int16_t hAux1, hAux2;
    
  Local_Components = MCM_Trig_Functions(pHandle->_Super.hElAngle);
  
  /* Alfa & Beta BEMF multiplied by Cos & Sin*/
  wAlfa_Sin_tmp = (int32_t)(hBemf_alfa_est)*(int32_t)Local_Components.hSin;
  wBeta_Cos_tmp = (int32_t)(hBemf_beta_est)*(int32_t)Local_Components.hCos;
  
#ifdef FULL_MISRA_C_COMPLIANCY
  hAux1 = (int16_t)(wBeta_Cos_tmp/32768);
#else
  hAux1 = (int16_t)(wBeta_Cos_tmp >> 15);
#endif
  
#ifdef FULL_MISRA_C_COMPLIANCY
  hAux2 = (int16_t)(wAlfa_Sin_tmp/32768); 
#else
  hAux2 = (int16_t)(wAlfa_Sin_tmp >> 15);
#endif
  
  /* Speed PI regulator */
  hOutput = PI_Controller( & pHandle->PIRegulator, (int32_t)(hAux1)-hAux2 );
 
  return (hOutput);
}

/**
  * @brief  It clears the estimated speed buffer 
  * @param  pHandle: handler of the current instance of the STO component
  * @retval none
  */
static void STO_InitSpeedBuffer(STO_Handle_t *pHandle)
{
  uint8_t b_i;
  uint8_t bSpeedBufferSize01Hz = pHandle->SpeedBufferSize01Hz;
  
  /*init speed buffer*/
  for (b_i = 0u; b_i<bSpeedBufferSize01Hz; b_i++)
  {
    pHandle->Speed_Buffer[b_i] = (int16_t)0;
  }
  pHandle->Speed_Buffer_Index = 0u;
  pHandle->SpeedBufferOldestEl = (int16_t)0;
  
  return;
}

/**
  * @brief  It internally performs a set of checks necessary to state whether
  *         the state observer algorithm converged. To be periodically called 
  *         during motor open-loop ramp-up (e.g. at the same frequency of 
  *         SPD_CalcElAngle), it returns true if the estimated angle and speed 
  *         can be considered reliable, false otherwise
  * @param  pHandle: handler of the current instance of the STO component
  * @param  hForcedMecSpeed01Hz Mechanical speed in 0.1Hz unit as forced by VSS
  * @retval bool sensor reliability state
  */
bool STO_IsObserverConverged(STO_Handle_t *pHandle, int16_t hForcedMecSpeed01Hz)
{
  int16_t hEstimatedSpeed01Hz, hUpperThreshold, hLowerThreshold;
  int32_t wAux;
  bool bAux = false;
  int32_t wtemp;
  
  if (pHandle->ForceConvergency2 == true)
  {
    hForcedMecSpeed01Hz = pHandle->_Super.hAvrMecSpeed01Hz;
  }
  
  if (pHandle->ForceConvergency == true)
  {
    bAux = true;
    pHandle->IsAlgorithmConverged= true;
    pHandle->_Super.bSpeedErrorNumber = 0u;
  }
  else    
  {
    hEstimatedSpeed01Hz = pHandle->_Super.hAvrMecSpeed01Hz;
    
    wtemp = (int32_t)hEstimatedSpeed01Hz * (int32_t)hForcedMecSpeed01Hz;
    
    if (wtemp > 0)
    {    
      if(hEstimatedSpeed01Hz < 0)
      {
        hEstimatedSpeed01Hz = -hEstimatedSpeed01Hz;   
      }
      
      if (hForcedMecSpeed01Hz<0)
      {
        hForcedMecSpeed01Hz = -hForcedMecSpeed01Hz;
      }
      wAux = (int32_t) (hForcedMecSpeed01Hz) * (int16_t)pHandle->SpeedValidationBand_H;
      hUpperThreshold = (int16_t)(wAux/(int32_t)16);
      
      wAux = (int32_t) (hForcedMecSpeed01Hz) * (int16_t)pHandle->SpeedValidationBand_L;
      hLowerThreshold = (int16_t)(wAux/(int32_t)16);
      
      /* If the variance of the estimated speed is low enough...*/
      if(pHandle->IsSpeedReliable == true)
      { 
        if((uint16_t)hEstimatedSpeed01Hz > pHandle->MinStartUpValidSpeed)
        {
          /*...and the estimated value is quite close to the expected value... */
          if(hEstimatedSpeed01Hz >= hLowerThreshold)
          {
            if(hEstimatedSpeed01Hz <= hUpperThreshold)
            {    
              pHandle->ConsistencyCounter++;
              
              /*... for hConsistencyThreshold consecutive times... */   
              if (pHandle->ConsistencyCounter >= 
                  pHandle->StartUpConsistThreshold)
              {           
                
                /* the algorithm converged.*/
                bAux = true;
                pHandle->IsAlgorithmConverged= true;
                pHandle->_Super.bSpeedErrorNumber = 0u;
              }          
            }
            else
            { 
              pHandle->ConsistencyCounter = 0u;
            }              
          }
          else
          { 
            pHandle->ConsistencyCounter = 0u;
          } 
        }
        else
        { 
          pHandle->ConsistencyCounter = 0u;
        } 
      }
      else
      { 
        pHandle->ConsistencyCounter = 0u;
      }
    }
  }
  
  return (bAux);
}

/**
  * @brief  It exports estimated Bemf alpha-beta in Volt_Components format
  * @param  pHandle: handler of the current instance of the STO component
  * @retval Volt_Components Bemf alpha-beta 
  */
Volt_Components STO_GetEstimatedBemf(STO_Handle_t *pHandle)
{
  Volt_Components Vaux;  
  Vaux.qV_Component1 = pHandle->hBemf_alfa_est;
  Vaux.qV_Component2 = pHandle->hBemf_beta_est;
  return (Vaux);
}


/**
  * @brief  It exports the stator current alpha-beta as estimated by state 
  *         observer
  * @param  pHandle: handler of the current instance of the STO component
  * @retval Curr_Components State observer estimated stator current Ialpha-beta 
  */
Curr_Components STO_GetEstimatedCurrent(STO_Handle_t *pHandle)
{
  Curr_Components Iaux;  
  
#ifdef FULL_MISRA_C_COMPLIANCY
  Iaux.qI_Component1 = (int16_t)(pHandle->Ialfa_est/(pHandle->hF1));
#else
  Iaux.qI_Component1 = (int16_t)(pHandle->Ialfa_est >> pHandle->F1LOG);
#endif
  
#ifdef FULL_MISRA_C_COMPLIANCY
  Iaux.qI_Component2 = (int16_t)(pHandle->Ibeta_est/(pHandle->hF1));
#else
  Iaux.qI_Component2 = (int16_t)(pHandle->Ibeta_est >> pHandle->F1LOG);
#endif
  
  return (Iaux);
}

/**
  * @brief  It exports current observer gains through parameters hhC2 and hhC4
  * @param  pHandle: handler of the current instance of the STO component
  * @param  phC2 pointer to int16_t used to return parameters hhC2
  * @param  phC4 pointer to int16_t used to return parameters hhC4
  * @retval none 
  */
void STO_GetObserverGains(STO_Handle_t *pHandle, int16_t *phC2, int16_t *phC4)
{
  *phC2 = pHandle->hC2;
  *phC4 = pHandle->hC4; 
}


/**
  * @brief  It allows setting new values for observer gains
  * @param  pHandle: handler of the current instance of the STO component
  * @param  wK1 new value for observer gain hhC1
  * @param  wK2 new value for observer gain hhC2
  * @retval none 
  */
void STO_SetObserverGains(STO_Handle_t *pHandle, int16_t hhC1, int16_t hhC2)
{
 
  pHandle->hC2 = hhC1;
  pHandle->hC4 = hhC2;
}

/**
  * @brief  It exports current PLL gains through parameters pPgain and pIgain
  * @param  pHandle: handler of the current instance of the STO component
  * @param  pPgain pointer to int16_t used to return PLL proportional gain
  * @param  pIgain pointer to int16_t used to return PLL integral gain
  * @retval none 
  */
void STO_GetPLLGains(STO_Handle_t *pHandle, int16_t *pPgain, int16_t *pIgain)
{

  *pPgain = PID_GetKP( & pHandle->PIRegulator);
  *pIgain = PID_GetKI( & pHandle->PIRegulator);
}


/**
  * @brief  It allows setting new values for PLL gains
  * @param  pHandle: handler of the current instance of the STO component
  * @param  hPgain new value for PLL proportional gain 
  * @param  hIgain new value for PLL integral gain 
  * @retval none 
  */
void STO_SetPLLGains( STO_Handle_t *pHandle, int16_t hPgain, int16_t hIgain)
{  
  PID_SetKP( & pHandle->PIRegulator, hPgain);
  PID_SetKI( & pHandle->PIRegulator, hIgain);
}


/**
  * @brief  It could be used to set istantaneous information on rotor mechanical
  *         angle.
  *         Note: Mechanical angle management is not implemented in this 
  *         version of State observer sensor class.
  * @param  pHandle: handler of the current instance of the STO component
  * @param  hMecAngle istantaneous measure of rotor mechanical angle
  * @retval none
  */
void STO_SetMecAngle(STO_Handle_t *pHandle, int16_t hMecAngle)
{
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM)
__attribute__((section ("ccmram")))
#endif
#endif
/**
  * @brief  It resets integral term of PLL
  * @param  pHandle: handler of the current instance of the STO component
  * @retval none 
  */
void STO_ResetPLL(STO_Handle_t *pHandle)
{
  PID_SetIntegralTerm( & pHandle->PIRegulator, (int32_t)0);  
}

/**
  * @brief  It sends locking info for PLL
  * @param  pHandle: handler of the current instance of the STO component
  * @param  hElSpeedDpp:
  * @param  hElAngle:
  * @retval none 
  */
void STO_SetPLL(STO_Handle_t *pHandle, int16_t hElSpeedDpp, int16_t hElAngle)
{
  PID_SetIntegralTerm( & pHandle->PIRegulator,
                     (int32_t)hElSpeedDpp*(int32_t)PID_GetKIDivisor( & pHandle->PIRegulator));
  pHandle->_Super.hElAngle = hElAngle;
}

/**
  * @brief  It exports estimated Bemf squared level
  * @param  pHandle: handler of the current instance of the STO component
  * @retval int32_t 
  */
int32_t STO_GetEstimatedBemfLevel(STO_Handle_t *pHandle)
{
  return (pHandle->Est_Bemf_Level);
}

/**
  * @brief  It exports observed Bemf squared level
  * @param  pHandle: handler of the current instance of the STO component
  * @retval int32_t 
  */
int32_t STO_GetObservedBemfLevel(STO_Handle_t *pHandle)
{
  return (pHandle->Obs_Bemf_Level);
}

/**
  * @brief  It enables/disables the bemf consistency check
  * @param  pHandle: handler of the current instance of the STO component
  * @param  bSel boolean; true enables check; false disables check
  */
void STO_BemfConsistencyCheckSwitch(STO_Handle_t *pHandle, bool bSel)
{
  pHandle->EnableDualCheck = bSel;
}

/**
  * @brief  It returns the result of the Bemf consistency check
  * @param  pHandle: handler of the current instance of the STO component
  * @retval bool Bemf consistency state
  */
bool STO_IsBemfConsistent(STO_Handle_t *pHandle)
{
  return (pHandle->IsBemfConsistent);
}

/**
  * @brief  It returns the result of the last variance check
  * @param  pHandle: handler of the current instance of the STO component
  * @retval bool Variance state
  */
bool STO_IsVarianceTight(STO_Handle_t *pHandle)
{
  return (pHandle->IsSpeedReliable);
}

/**
  * @brief  It forces the state-observer to declare convergencyu
  * @param  pHandle: handler of the current instance of the STO component
  */
void STO_ForceConvergency1(STO_Handle_t *pHandle)
{
  pHandle->ForceConvergency = true;
}

/**
  * @brief  It forces the state-observer to declare convergencyu
  * @param  pHandle: handler of the current instance of the STO component
  */
void STO_ForceConvergency2(STO_Handle_t *pHandle)
{
  pHandle->ForceConvergency2 = true;
}

/**
  * @brief  Set the Absolute value of minimum mechanical speed (expressed in 
  *         01Hz) required to validate the start-up.
  * @param  pHandle: handler of the current instance of the STO component
  * @param  hMinStartUpValidSpeed: Absolute value of minimum mechanical speed 
  */
void STO_SetMinStartUpValidSpeed01HZ(STO_Handle_t *pHandle, uint16_t hMinStartUpValidSpeed)
{
  pHandle->MinStartUpValidSpeed = hMinStartUpValidSpeed;
}

/**
  * @}
  */
  
/**
  * @}
  */

/** @} */

/******************* (C) COPYRIGHT 2017 STMicroelectronics *****END OF FILE****/
