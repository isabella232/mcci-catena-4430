/*

Module: Catena4430_Sensor.ino

Function:
    Remote sensor example for the Catena 4430.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#include <Arduino.h>
#include <Wire.h>
#include <Catena.h>
#include <Catena_Mx25v8035f.h>
#include <RTClib.h>
#include <SPI.h>
#include <arduino_lmic.h>
#include <Catena_Timer.h>
#include <Catena4430-cPCA9570.h>
#include <Catena4430-c4430Gpios.h>
#include <Catena4430-cPIRdigital.h>
#include "Catena4430_cMeasurementLoop.h"

extern McciCatena::Catena gCatena;
using namespace McciCatena4430;
using namespace McciCatena;

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/


cPCA9570 i2cgpio    { &Wire };
c4430Gpios gpio     { &i2cgpio };
Catena gCatena;
cTimer ledTimer;
Catena::LoRaWAN gLoRaWAN;
StatusLed gLed (Catena::PIN_STATUS_LED);

RTC_PCF8523 rtc;

SPIClass gSPI2(
    Catena::PIN_SPI2_MOSI,
    Catena::PIN_SPI2_MISO,
    Catena::PIN_SPI2_SCK
    );

cMeasurementLoop gMeasurementLoop;

//   The flash
Catena_Mx25v8035f gFlash;

unsigned ledCount;

/****************************************************************************\
|
|   Setup
|
\****************************************************************************/

void setup()
    {
    setup_platform();
    setup_printSignOn();

    setup_flash();
    setup_measurement();
    setup_gpio();
    setup_rtc();
    setup_radio();
    setup_commands();
    setup_start();
    }

void setup_platform()
    {
    gCatena.begin();

    // if running unattended, don't wait for USB connect.
    if (! (gCatena.GetOperatingFlags() &
            static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)))
            {
            while (!Serial)
                    /* wait for USB attach */
                    yield();
            }
    }

static constexpr const char *filebasename(const char *s)
    {
    const char *pName = s;

    for (auto p = s; *p != '\0'; ++p)
        {
        if (*p == '/' || *p == '\\')
            pName = p + 1;
        }
    return pName;
    }

void setup_printSignOn()
    {
    static const char dashes[] = "------------------------------------";

    gCatena.SafePrintf("\n%s%s\n", dashes, dashes);

    gCatena.SafePrintf("This is %s.\n", filebasename(__FILE__));
        {
        char sRegion[16];
        gCatena.SafePrintf("Target network: %s / %s\n",
                        gLoRaWAN.GetNetworkName(),
                        gLoRaWAN.GetRegionString(sRegion, sizeof(sRegion))
                        );
        }

    gCatena.SafePrintf("System clock rate is %u.%03u MHz\n",
        ((unsigned)gCatena.GetSystemClockRate() / (1000*1000)),
        ((unsigned)gCatena.GetSystemClockRate() / 1000 % 1000)
        );
    gCatena.SafePrintf("Enter 'help' for a list of commands.\n");
    gCatena.SafePrintf("(remember to select 'Line Ending: Newline' at the bottom of the monitor window.)\n");

    gCatena.SafePrintf("%s%s\n" "\n", dashes, dashes);
    }

void setup_gpio()
    {
    if (! gpio.begin())
        Serial.println("GPIO failed to initialize");

    ledTimer.begin(400);

    // set up the LED
    gLed.begin();
    gCatena.registerObject(&gLed);
    gLed.Set(LedPattern::FastFlash);
    }

void setup_rtc()
    {
    if (! rtc.begin())
        Serial.println("RTC failed to intiailize");

    DateTime now = rtc.now();
    gCatena.SafePrintf("RTC is %s. Date: %d-%02d-%02d %02d:%02d:%02d\n",
        rtc.initialized() ? "running" : "not initialized",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second()
        );
    }

void setup_flash(void)
    {
    if (gFlash.begin(&gSPI2, Catena::PIN_SPI2_FLASH_SS))
        {
        gMeasurementLoop.registerSecondSpi(&gSPI2);
        gFlash.powerDown();
        gCatena.SafePrintf("FLASH found, put power down\n");
        }
    else
        {
        gFlash.end();
        gSPI2.end();
        gCatena.SafePrintf("No FLASH found: check hardware\n");
        }
    }

void setup_radio()
    {
    gLoRaWAN.begin(&gCatena);
    gCatena.registerObject(&gLoRaWAN);
    LMIC_setClockError(5 * MAX_CLOCK_ERROR / 100);
    }

void setup_measurement()
    {
    gMeasurementLoop.begin();
    }

void setup_commands()
    {
    // none yet.  Add commands for setting clock, etc.
    }

void setup_start()
    {
    if (gLoRaWAN.IsProvisioned())
        gMeasurementLoop.requestActive(true);
    else
        {
        gCatena.SafePrintf("not provisioned, idling\n");
        gMeasurementLoop.requestActive(false);
        }
    }

/****************************************************************************\
|
|   Loop
|
\****************************************************************************/

void loop()
    {
    gCatena.poll();

    // copy current PIR state to the blue LED.
    gpio.setBlue(digitalRead(A0));

    // if it's time to update the LEDs, advance to the next step.
    if (ledTimer.isready())
        {
        const std::uint8_t ledMask = (gpio.kRedMask | gpio.kGreenMask);
        std::uint8_t ledValue;

        unsigned iGpio = 2 - ledCount;

        ledCount = (ledCount + 1) % 2;

        if (ledCount == 0)
            ledValue = gpio.kRedMask;
        else
            ledValue = gpio.kGreenMask;
    
        gpio.setLeds(ledMask, ledValue);
        }
    }