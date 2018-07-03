﻿/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2018 STMicroelectronics International N.V. 
  * All rights reserved.
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
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */     

/* ------------------------ LWIP includes --------------------------------- */
#include "lwip/api.h"
#include "lwip/tcpip.h"
#include "lwip/memp.h"
#include "lwip/sockets.h"

/* ------------------------ FreeModbus includes --------------------------- */
#include "mb.h"
#include "iwdg.h"
/* ------------------------ Project includes ------------------------------ */
#include "usart.h"
#include "gpio.h"
#include "iwdg.h"
#include "DAC.h"
#include "R8025t.h"
#include "Relay.h"
#include "ADS1230.h"
#include "recod.h"
#include <memory.h>
#include "crc.h"
/* USER CODE END Includes */

/* Variables -----------------------------------------------------------------*/
osThreadId defaultTaskHandle;
osThreadId mainMB_TASKHandle;
osThreadId monitorTaskHandle;
osThreadId uMBpoll_taskHandle;
osThreadId periodTaskHandle;
osThreadId tcp_severTaskHandle;
osThreadId socketTaskHandle;
osThreadId recvTaskHandle;
osThreadId recv2TaskHandle;
osMutexId MBholdingMutexHandle;

/* USER CODE BEGIN Variables */

//#define mainMB_TASK_PRIORITY    ( tskIDLE_PRIORITY + 3 )
#define PROG                    "FreeModbus"
#define REG_INPUT_START         1000
#define REG_INPUT_NREGS         4
#define REG_HOLDING_START       1
#define REG_HOLDING_NREGS       130

/* ----------------------- Static variables ---------------------------------*/
static USHORT   usRegInputStart = REG_INPUT_START;
static USHORT   usRegInputBuf[REG_INPUT_NREGS];
static USHORT   usRegHoldingStart = REG_HOLDING_START;
static USHORT   usRegHoldingBuf[REG_HOLDING_NREGS];
/* USER CODE END Variables */

/* Function prototypes -------------------------------------------------------*/
void StartDefaultTask(void const * argument);
void vMBServerTask(void const * argument);
void monitor(void const * argument);
void uMBpoll(void const * argument);
void period(void const * argument);
void TCPsever(void const * argument);
void socketsever(void const * argument);
void socket1(void const * argument);
void socket2(void const * argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* USER CODE BEGIN FunctionPrototypes */
extern STDATETIME stDateTime;

void 
rede_adc();

/* USER CODE END FunctionPrototypes */

/* Hook prototypes */

/* Init FreeRTOS */

void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
       
  /* USER CODE END Init */

  /* Create the mutex(es) */
  /* definition and creation of MBholdingMutex */
  osMutexDef(MBholdingMutex);
  MBholdingMutexHandle = osMutexCreate(osMutex(MBholdingMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityHigh, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of mainMB_TASK */
  osThreadDef(mainMB_TASK, vMBServerTask, osPriorityAboveNormal, 0, 512);
  mainMB_TASKHandle = osThreadCreate(osThread(mainMB_TASK), NULL);

  /* definition and creation of monitorTask */
  osThreadDef(monitorTask, monitor, osPriorityNormal, 0, 128);
  monitorTaskHandle = osThreadCreate(osThread(monitorTask), NULL);

  /* definition and creation of uMBpoll_task */
  osThreadDef(uMBpoll_task, uMBpoll, osPriorityNormal, 0, 512);
  uMBpoll_taskHandle = osThreadCreate(osThread(uMBpoll_task), NULL);

  /* definition and creation of periodTask */
  osThreadDef(periodTask, period, osPriorityBelowNormal, 0, 256);
  periodTaskHandle = osThreadCreate(osThread(periodTask), NULL);

  /* definition and creation of tcp_severTask */
  osThreadDef(tcp_severTask, TCPsever, osPriorityAboveNormal, 0, 512);
  tcp_severTaskHandle = osThreadCreate(osThread(tcp_severTask), NULL);

  /* definition and creation of socketTask */
  osThreadDef(socketTask, socketsever, osPriorityAboveNormal, 0, 512);
  socketTaskHandle = osThreadCreate(osThread(socketTask), NULL);

  /* definition and creation of recvTask */
  osThreadDef(recvTask, socket1, osPriorityNormal, 0, 128);
  recvTaskHandle = osThreadCreate(osThread(recvTask), NULL);

  /* definition and creation of recv2Task */
  osThreadDef(recv2Task, socket2, osPriorityNormal, 0, 128);
  recv2TaskHandle = osThreadCreate(osThread(recv2Task), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
}

/* StartDefaultTask function */
void StartDefaultTask(void const * argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();

  /* USER CODE BEGIN StartDefaultTask */
	HAL_GPIO_WritePin(_485DIR_GPIO_Port, _485DIR_Pin, GPIO_PIN_SET);
  /* Infinite loop */
  for(;;)
  {
	  vTaskDelete(NULL);
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* vMBServerTask function */
void vMBServerTask(void const * argument)
{
  /* USER CODE BEGIN vMBServerTask */

	eMBErrorCode    xStatus;
	
	/* Infinite loop */
	for (;;)
	{
		if (eMBTCPInit(MB_TCP_PORT_USE_DEFAULT) != MB_ENOERR)
		{
			//  fprintf(stderr, "%s: can't initialize modbus stack!\r\n", PROG);
		}
		else if (eMBEnable() != MB_ENOERR)
		{
			//  fprintf(stderr, "%s: can't enable modbus stack!\r\n", PROG);
		}
		else
		{
			do
			{
				osDelay(10);
				xSemaphoreTake(MBholdingMutexHandle, (TickType_t)10);
				xStatus = eMBPoll();
			} while (xStatus == MB_ENOERR);
		}
		/* An error occured. Maybe we can restart. */
		(void)eMBDisable();
		(void)eMBClose();
		osDelay(10);
	}
  /* USER CODE END vMBServerTask */
}

/* monitor function */
void monitor(void const * argument)
{
  /* USER CODE BEGIN monitor */
	MX_IWDG_Init();
//	InitADline();
	static portTickType xLastWakeTime;  
	const portTickType xFrequency = pdMS_TO_TICKS(1000);  
   
	
	xLastWakeTime = xTaskGetTickCount();
  /* Infinite loop */
  for(;;)
  {
	  HAL_GPIO_TogglePin(WDI_GPIO_Port, WDI_Pin);
//	  HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
//	  rede_adc();
	  HAL_IWDG_Refresh(&hiwdg);
	  vTaskDelayUntil(&xLastWakeTime, xFrequency); 
  }
  /* USER CODE END monitor */
}

/* uMBpoll function */
void uMBpoll(void const * argument)
{
  /* USER CODE BEGIN uMBpoll */
	uint8_t Relay;
	uint8_t DI;
	uint16_t dac;
	/* Infinite loop */
	for (;;)
	{
	
		if (xSemaphoreTake(MBholdingMutexHandle, (TickType_t)portMAX_DELAY) == pdTRUE)
		{
			/********************************************************************************/		
					/*set relay*/
			for (Relay = 0; Relay < 8; Relay++)
			{
				DI = usRegHoldingBuf[Relay];
				relaycontrol(Relay, DI);	 
			}
		
			/*set 0-10v */
			
			dac = usRegHoldingBuf[12];
			dac = dac * 3.055;
			if (dac > 3055) dac = 3055;
			spi1_dac_write_chb(dac);
			spi1_dac_write_cha(dac);
			
							/************************************************/
									xSemaphoreGive(MBholdingMutexHandle);
			osDelay(10);
		}
		osDelay(10);
	}
  /* USER CODE END uMBpoll */
}

/* period function */
void period(void const * argument)
{
  /* USER CODE BEGIN period */
  /* Infinite loop */
	char  time_buf[64];
	Init8025();
  for(;;)
  {
	  UpdateDateTime();
	  sprintf((char *)time_buf,
		  "%04d-%02d-%02d %02d:%02d:%02d",
		  stDateTime.year + 2000, 
		  stDateTime.month,
		  stDateTime.day,
		  stDateTime.hour,
		  stDateTime.minute,
		  stDateTime.second);
	  
	  printf("%s\n", time_buf);
    osDelay(1000);
  }
  /* USER CODE END period */
}

/* TCPsever function */
void TCPsever(void const * argument)
{
  /* USER CODE BEGIN TCPsever */
  /* Infinite loop */
		struct netconn *conn, *newconn;
		err_t err, accept_err;
		struct netbuf *buf;
		char  tcpbuf;
		uint8_t *data;
	
		u16_t len;
	      
		LWIP_UNUSED_ARG(argument);
	
		/* Create a new connection identifier. */
		conn = netconn_new(NETCONN_TCP);
	  
		if (conn != NULL)
		{  
			/* Bind connection to well known port number 7. */
			err = netconn_bind(conn, NULL,500);
	    
			if (err == ERR_OK)
			{
				/* Tell connection to go into listening mode. */
				netconn_listen(conn);
				for (;;)
				{
					/* */
					accept_err = netconn_accept(conn, &newconn);
	    
					/* Process the new connection. */
					if (accept_err == ERR_OK) 
					{
	
						while (netconn_recv(newconn,&buf) == ERR_OK) 
						{
							
							do 
							{
							//	taskDISABLE_INTERRUPTS(); 	
								netbuf_data(buf, (void * *)&data, &len);
								
//								decoding(data);
								
								netconn_write(newconn, data, len, NETCONN_COPY);
							//	taskENABLE_INTERRUPTS();
	          
							} while (netbuf_next(buf) >= 0);
	          
							netbuf_delete(buf);
						}
	        
						/* Close connection and discard connection identifier. */
						netconn_close(newconn);
						netconn_delete(newconn);
					}
					osDelay(1);
				}
			}
			else
			{
				netconn_delete(newconn);
			}
		}
  /* USER CODE END TCPsever */
}

/* socketsever function */
void socketsever(void const * argument)
{
  /* USER CODE BEGIN socketsever */
	__IO uint32_t uwCRCValue = 0;
	u_int8_t buflen = 32;
	u_int8_t ret;
	unsigned char recv_buffer[buflen];
	int sock, newconn, size;
	struct sockaddr_in address, remotehost;

	/* create a TCP socket */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		printf("can not create socket");
		return;
	}
  
	/* bind to port 80 at any interface */
	address.sin_family = AF_INET;
	address.sin_port = htons(PORT);
	address.sin_addr.s_addr = INADDR_ANY;
	if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		printf("can not bind socket");
		close(sock);
		return;
	}

	/* listen for connections (TCP listen backlog = 3) */
	listen(sock, 5); 
	size = sizeof(remotehost);

	/* Infinite loop */
	for (;;)
	{
		newconn = accept(sock, (struct sockaddr *)&remotehost, (socklen_t *)&size);

		while ((ret =  recv(newconn, recv_buffer, buflen,0)) > 0)
		{
			decoding(recv_buffer);
			memset(recv_buffer, 0, sizeof(recv_buffer));
//			printf("ret:%d\n", ret);
//			osDelay(100);
		}
		//	  while (newconn >= 0)
		//	  {
		//		  memset(recv_buffer, 0, sizeof(recv_buffer));
		//		  ret = read(newconn, recv_buffer, buflen); 
		//		  if (ret <= 0)
		//		  {
		//			  close(newconn);
		//			  printf("read failed\r\n");
		//			  return;
		//		  }
		//		 
		//			
		//			  decoding(recv_buffer);
		////			  memset(recv_buffer, 0, sizeof(recv_buffer));
		//		  } 
		
		//		  uwCRCValue = HAL_CRC_Accumulate(&hcrc, (uint32_t *)recv_buffer,( ret-2));
		//		  printf("crc:%x\n", uwCRCValue);
		
				  close(newconn);
	
	//	  else
	//	  {
	//		  close(newconn);
	//	  }
	    osDelay(1);
	}
  /* USER CODE END socketsever */
}

/* socket1 function */
void socket1(void const * argument)
{
  /* USER CODE BEGIN socket1 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END socket1 */
}

/* socket2 function */
void socket2(void const * argument)
{
  /* USER CODE BEGIN socket2 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END socket2 */
}

/* USER CODE BEGIN Application */
eMBErrorCode
eMBRegInputCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs)
{
	eMBErrorCode    eStatus = MB_ENOERR;
	int             iRegIndex;

	if ((usAddress >= REG_INPUT_START)
	    && (usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS))
	{
		iRegIndex = (int)(usAddress - usRegInputStart);
		while (usNRegs > 0)
		{
			*pucRegBuffer++ = (unsigned char)(usRegInputBuf[iRegIndex] >> 8);
			*pucRegBuffer++ = (unsigned char)(usRegInputBuf[iRegIndex] & 0xFF);
			iRegIndex++;
			usNRegs--;
		}
	}
	else
	{
		eStatus = MB_ENOREG;
	}
	return eStatus;
}

eMBErrorCode
eMBRegHoldingCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode)
{
	eMBErrorCode    eStatus = MB_ENOERR;
	int             iRegIndex;

	if ((usAddress >= REG_HOLDING_START) &&
	    (usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS))
	{
		iRegIndex = (int)(usAddress - usRegHoldingStart);
		switch (eMode)
		{
			/* Pass current register values to the protocol stack. */
		case MB_REG_READ:
			while (usNRegs > 0)
			{
				*pucRegBuffer++ = (UCHAR)(usRegHoldingBuf[iRegIndex] >> 8);
				*pucRegBuffer++ = (UCHAR)(usRegHoldingBuf[iRegIndex] & 0xFF);
				iRegIndex++;
				usNRegs--;
			}
			break;

			/* Update current register values with new values from the
			 * protocol stack. */
		case MB_REG_WRITE:
			if (usNRegs != 0)
			{
				
				while (usNRegs > 0)
				{
					usRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
					usRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
					iRegIndex++;
					usNRegs--;
				}
				xSemaphoreGive(MBholdingMutexHandle);
				
			}
		}
	}
	else
	{
		eStatus = MB_ENOREG;
	}
	return eStatus;
}

eMBErrorCode
eMBRegCoilsCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils, eMBRegisterMode eMode)
{
	return MB_ENOREG;
}

eMBErrorCode
eMBRegDiscreteCB(UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete)
{
	return MB_ENOREG;
}
int32_t adctemp;
void 
rede_adc()
{
	
//	InitADline();
	
	adctemp  = ReadAD();
		  usRegHoldingBuf[11] = adctemp <<16;
		  usRegHoldingBuf[11] |= adctemp;
		  printf("%d\n",(int16_t)adctemp);
	 
}
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
