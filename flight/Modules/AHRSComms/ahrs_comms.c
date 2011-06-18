/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{
 * @addtogroup AHRSCommsModule AHRSComms Module
 * @brief Handles communication with AHRS and updating position
 * Specifically updates the the @ref AttitudeActual "AttitudeActual" and @ref AttitudeRaw "AttitudeRaw" settings objects
 * @{
 *
 * @file       ahrs_comms.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      Module to handle all comms to the AHRS on a periodic basis.
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 ******************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/**
 * Input objects: As defined in PiOS/inc/pios_ahrs_comms.h
 * Output objects: As defined in PiOS/inc/pios_ahrs_comms.h
 *
 * This module will periodically update the values of latest attitude solution
 * and other objects that are transferred to and from the AHRS
 * The module settings can configure how often AHRS is polled for a new solution.
 *
 * The module executes in its own thread.
 *
 * UAVObjects are automatically generated by the UAVObjectGenerator from
 * the object definition XML file.
 *
 * Modules have no API, all communication to other modules is done through UAVObjects.
 * However modules may use the API exposed by shared libraries.
 * See the OpenPilot wiki for more details.
 * http://www.openpilot.org/OpenPilot_Application_Architecture
 *
 */

#include "ahrs_comms.h"
#include "ahrs_spi_comm.h"

// Private constants
#define STACK_SIZE configMINIMAL_STACK_SIZE-128
#define TASK_PRIORITY (tskIDLE_PRIORITY+4)

// Private types

// Private variables
static xTaskHandle taskHandle;

// Private functions
static void ahrscommsTask(void *parameters);

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t AHRSCommsInitialize(void)
{
	// Start main task
	AHRSStatusInitialize();
	AHRSCalibrationInitialize();
	AttitudeRawInitialize();
	
	xTaskCreate(ahrscommsTask, (signed char *)"AHRSComms", STACK_SIZE, NULL, TASK_PRIORITY, &taskHandle);
	TaskMonitorAdd(TASKINFO_RUNNING_AHRSCOMMS, taskHandle);
	PIOS_WDG_RegisterFlag(PIOS_WDG_AHRS);

	return 0;
}

/**
 * Module thread, should not return.
 */
static void ahrscommsTask(void *parameters)
{
	portTickType lastSysTime;

	AlarmsSet(SYSTEMALARMS_ALARM_AHRSCOMMS, SYSTEMALARMS_ALARM_CRITICAL);

	// Main task loop
	while (1) {
		PIOS_WDG_UpdateFlag(PIOS_WDG_AHRS);
		
		AHRSSettingsData settings;
		AHRSSettingsGet(&settings);

		AhrsSendObjects();
		AhrsCommStatus stat = AhrsGetStatus();
		if (stat.linkOk) {
			AlarmsClear(SYSTEMALARMS_ALARM_AHRSCOMMS);
		} else {
			AlarmsSet(SYSTEMALARMS_ALARM_AHRSCOMMS, SYSTEMALARMS_ALARM_WARNING);
		}
		AhrsStatusData sData;
		AhrsStatusGet(&sData);

		sData.LinkRunning = stat.linkOk;
		sData.AhrsKickstarts = stat.remote.kickStarts;
		sData.AhrsCrcErrors = stat.remote.crcErrors;
		sData.AhrsRetries = stat.remote.retries;
		sData.AhrsInvalidPackets = stat.remote.invalidPacket;
		sData.OpCrcErrors = stat.local.crcErrors;
		sData.OpRetries = stat.local.retries;
		sData.OpInvalidPackets = stat.local.invalidPacket;

		AhrsStatusSet(&sData);
		/* Wait for the next update interval */
		vTaskDelayUntil(&lastSysTime, settings.UpdatePeriod / portTICK_RATE_MS);

	}
}

/**
  * @}
  * @}
  */
