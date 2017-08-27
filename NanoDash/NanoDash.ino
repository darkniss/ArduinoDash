/*
// Include libraries
// NeoPixel https://github.com/adafruit/Adafruit_NeoPixel
// MCP2515 https://github.com/coryjfowler/MCP2515_lib
// MPU6050 https://github.com/jrowberg/i2cdevlib/tree/master/Arduino/MPU6050
*/

#include "Adafruit_NeoPixel.h"
#include "mcp_can.h"
#include <SPI.h>
//#include "I2Cdev.h"
//#include "MPU6050_6Axis_MotionApps20.h"
//#include "Wire.h"
#include "led.h"
#include "LedControl.h"


// Uncomment this to enable test-mode where CAN is disabled and errLEDs and RPM
// LEDs blink
// Comment this to disable test mode where CAN is used to update the parameters
// for the LEDs
//#define IN_TESTMODE

Colour colBlue(255, 0, 0);
Colour colRed(255, 255, 0);
Colour colGreen(0, 255, 0);
Colour colYellow(255, 255, 0);
Colour colOff(0, 0, 0);

// Enable or disable serial, needed for testMode!
//#define SERIAL_ENABLED

/*
// Defines for LED strip
*/
static const int errorLEDs = 5;
static const int rpmLEDs = 23;
static const int totalLEDs = errorLEDs + rpmLEDs;
#define CTRL_PIN 7
#define REFRESH_FREQUENCY 50
#define REFRESH_PERIOD (1 / REFRESH_FREQUENCY) * 1000
#define DEFALT_BRIGHTNESS 25

Colour ledColArray[totalLEDs];

int16_t rpmRef = 0;
int16_t gearPosition = 0;

static const int16_t rpmHigh = 9500; //chnage to 10000 ? //was 16000 // was 12000
LED rpm[rpmLEDs];

Adafruit_NeoPixel strip
    = Adafruit_NeoPixel(totalLEDs, CTRL_PIN, NEO_GRB + NEO_KHZ800);

/*
// Defines and variables for MCP2515 CAN controller
*/
#define MCP_CHIP_SELECT 10
#define MCP_INTERRUPT 3

long unsigned int rxId;
byte len = 0;
byte rxBuf[8];
byte txBuf[8];

#ifdef IN_TESTMODE
int16_t val1 = 0;
int16_t val2 = 16384;
int16_t val3 = -16384;
int16_t val4 = 8192;
#endif

int16_t cycle = 0;
boolean started = false;

MCP_CAN CAN0(MCP_CHIP_SELECT);

/*
// Function definitions
*/

//led setup

const int DIN_PIN = 2;
const int CS_PIN = 5;
const int CLK_PIN = 6;

const uint64_t EMPTY = {0x0000000000000000};


const uint64_t IMAGES[] = {
  0x7c66666666666600, //u
  0x66361e3e66663e00, //r
  0xc6c6e6f6decec600, //n
  0x7e1818181c181800, //1
  0x7e060c3060663c00, //2
  0x3c66603860663c00, //3
  0x30307e3234383000, //4
  0x3c6660603e067e00, //5
  0x3c66663e06663c00, //6
  0x1818183030667e00, //7
  0x3c66663c66663c00,  //8
  0x60f018181819ffff //tau
};
const int IMAGES_LEN = sizeof(IMAGES) / sizeof(uint64_t);

LedControl display = LedControl(DIN_PIN, CLK_PIN, CS_PIN);


void updateStrip()
{
    for (size_t i = 0; i < totalLEDs; i++)
    {
        strip.setPixelColor(i, ledColArray[i].r, ledColArray[i].g,
                            ledColArray[i].b);
        strip.show();
    }
}

void setup()
{
    /*
    // Initialize the LED strip and clear it
    */
    strip.begin();
    strip.show();

    // Delay needed during startup - Not sure why, but should be OK for now
    delay(500);

// initialize serial communication
#ifdef SERIAL_ENABLED
    Serial.begin(115200);
#endif

    for (int i = 0; i < rpmLEDs; ++i)
    {
        if (i == 0)
        {
            rpm[i] = LED(ledColArray + errorLEDs + i, &rpmRef, -1, -1, rpmHigh,
                         &colOff, &colGreen, &colOff, &colBlue);
        }
        else if (i < 4)
        {
            rpm[i] = LED(ledColArray + errorLEDs + i, &rpmRef,
                         i * (rpmHigh / rpmLEDs), i * (rpmHigh / rpmLEDs),
                         rpmHigh, &colOff, &colGreen, &colOff, &colBlue);
        }
        else if (i < 19)
        {
            rpm[i] = LED(ledColArray + errorLEDs + i, &rpmRef,
                         i * (rpmHigh / rpmLEDs), i * (rpmHigh / rpmLEDs),
                         rpmHigh, &colOff, &colRed, &colOff, &colBlue);
        }
        else
        {
            rpm[i] = LED(ledColArray + errorLEDs + i, &rpmRef,
                         i * (rpmHigh / rpmLEDs), i * (rpmHigh / rpmLEDs),
                         rpmHigh, &colOff, &colBlue, &colOff, &colBlue);
        }
        rpm[i].setDeltaTime(0);

        //LED setup

          display.clearDisplay(0);
          display.shutdown(0, false);
          display.setIntensity(0, 10);

          displayImage(IMAGES[11]);

    }

#ifndef IN_TESTMODE
#ifdef SERIAL_ENABLED
    Serial.println("Init CAN");
#endif // SERIAL_ENABLED

    CAN0.begin(CAN_1000KBPS);      // Initiate CAN to 1000KBPS
    pinMode(MCP_INTERRUPT, INPUT); // Setting pin for MCP interrupt

#ifdef SERIAL_ENABLED
    Serial.println("Done init CAN");
        Serial.println("will test");

#endif // SERIAL_ENABLED

#endif // IN_TESTMODE
}

void displayImage(uint64_t image) {
  for (int i = 0; i < 8; i++) {
    byte row = (image >> i * 8) & 0xFF;
    for (int j = 0; j < 8; j++) {
      display.setLed(0, i, j, bitRead(row, j));
    }
  }
}

void loop()
{
#ifdef IN_TESTMODE
    static int upDown = 1;
    static unsigned int lastUpdate = 0;

    displayImage(IMAGES[11]);
    
    if ((millis() - lastUpdate) > 1)
    {
        rpmRef = rpmRef + upDown * random(1, 300);
        lastUpdate = millis();
        if (rpmRef > 20000)
        {
            upDown = upDown * -1;
            rpmRef = 20000;
        }
        else if (rpmRef <= 0)
        {
            upDown = upDown * -1;
            rpmRef = 0;
        }
    }

#endif // IN_TESTMODE

#ifndef IN_TESTMODE

if (cycle < 21){

  if (started==false){}
  
  static int upDown = 1;
    static unsigned int lastUpdate = 0;
    if ((millis() - lastUpdate) > 1)
    {
        rpmRef = rpmRef + upDown * 2000;//random(1, 300);
        lastUpdate = millis();
        if (rpmRef > 20000)
        {
            upDown = upDown * -1;
            rpmRef = 20000;
        }
        else if (rpmRef <= 0)
        {
            upDown = upDown * -1;
            rpmRef = 0;
        }
        cycle=cycle +1;
    }
    started = true;
    
    }else{
      if (started==true){
        displayImage(EMPTY);
        started =false;
      }}


    

    // If MCP pin is low read data from recieved over CAN bus
    if (!digitalRead(MCP_INTERRUPT))
    {
      Serial.println("digital read");
        // Serial.println("Got an interrupt.");
        CAN0.readMsgBuf(&len, rxBuf);
        rxId = CAN0.getCanId(); // Get message ID
        if (rxId == 0x600)
        {
            // Create a 16 bit number from two 8 bit data slots, store RPM
            rpmRef = (int16_t)((rxBuf[0] << 8) + rxBuf[1]);
            // Store the vbat
//            voltageErr4Ref = (int16_t)((rxBuf[4] << 8) + rxBuf[5]);
//            Serial.println(voltageErr4Ref);
              Serial.println(rpmRef);
        }
        else if (rxId == 0x601)
        {
            // Store the coolant temp
//            coolantErr3Ref = (int16_t)((rxBuf[2] << 8) + rxBuf[3]);
        }
        else if (rxId == 0x602)
        {
          // Store the sensor error flags
//          sensorErr1Ref = (int16_t)((rxBuf[0] << 8) + rxBuf[1]);
          // Store the sync state
//          engineErr2Ref = (int16_t)((rxBuf[2] << 8) + rxBuf[3]);
          // Store the launch state
//          launchErr5Ref = (int16_t)((rxBuf[4] << 8) + rxBuf[5]);

          
          
        }

        //rx data will

         else if (rxId == 0x60e)
        {

          gearPosition = (int16_t)((rxBuf[2] << 8) + rxBuf[3]);
          
          // print the sensor error flags
          //Serial.println("0x60e");
          Serial.println(gearPosition);

          displayImage(IMAGES[gearPosition]); //gearPosition
          
        }
    }

#endif // NOT IN_TESTMODE

    for (int i = 0; i < rpmLEDs; ++i)
    {
        rpm[i].checkAndUpdate();
    }
//    err1.checkAndUpdate();
//    err2.checkAndUpdate();
//    err3.checkAndUpdate();
//    err4.checkAndUpdate();
//    err5.checkAndUpdate();
    updateStrip();
}
