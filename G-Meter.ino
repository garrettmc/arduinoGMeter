/*
 *
 * Uses a 16x2 LCD display to display readings off a ADXL345 Accelerometer.
 * Displays max +/- g's on the three axis.
 * 
 * A push button is used to reset the max's.
 *
 * The circuit:
 *   LCD:
 *    LCD RS pin to digital pin 7
 *    LCD Enable pin to digital pin 8
 *    LCD D4 pin to digital pin 9
 *    LCD D5 pin to digital pin 10
 *    LCD D6 pin to digital pin 11
 *    LCD D7 pin to digital pin 12
 *    LCD R/W pin to ground
 *    LCD VSS pin to ground
 *    LCD VCC pin to 5V
 *    10K pot resistor:
 *       ends to +5V and ground
 *       wiper to LCD VO pin (pin 3)
 *       
 *  ADXL345:
 *     5V  -> 5V
 *     GND -> GND
 *     SCL -> A5
 *     SCA -> A4
 *     
 *  Button
 *     5v -> One side
 *     Gnd -> 10k reistor -> other side -> pin 4
 *    
 */

// include the library code:
#include <LiquidCrystal.h>

// For I2C to the Accellerometer
#include <Wire.h>   

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Button
const int startOverButtonPin = 4;


// Accellerometer (ADXL345) Registers -- See data sheet at 
// http://www.analog.com/media/en/technical-documentation/data-sheets/ADXL345.pdf
#define ADXL345_Register_Power_Control 0x2D
#define ADXL345_Register_Data_Format 0x31
#define ADXL345_Register_X0 0x32
#define ADXL345_Register_X1 0x33
#define ADXL345_Register_Y0 0x34
#define ADXL345_Register_Y1 0x35
#define ADXL345_Register_Z0 0x36
#define ADXL345_Register_Z1 0x37

// Bits sent to ADXL_Register_Power_Control
#define ADXL345_PowerControl_Wakeup0     1
#define ADXL345_PowerControl_Wakeup1     2
#define ADXL345_PowerControl_Sleep       4
#define ADXL345_PowerControl_Measure     8
#define ADXL345_PowerControl_AutoSleep1  16
#define ADXL345_PowerControl_AutoSleep2  32
#define ADXL345_PowerControl_Link        64

// Bits sent to ADXL345_Register_Data_Format
#define ADXL_345_Data_Format_2G         B00000000
#define ADXL_345_Data_Format_4G         B00000001
#define ADXL_345_Data_Format_8G         B00000010
#define ADXL_345_Data_Format_16G        B00000011
// bit 1&2 are the 2G/4G/8G bits.
// bit 4 is the full-res but, want 0 always, so 10bit output
// bit 5 is unused and should be 0, 
#define ADXL_345_Data_Format_Mask       B11100100  

int ADX_Address = 0x53;  //I2C address of ADXL345
int reading = 0;
int val = 0;
int X0, X1, X_out;
int Y0, Y1, Y_out;
int Z1, Z0, Z_out;
double Xg, Yg, Zg;

#define ADXL345_READ_ERROR 1
bool error = false;
byte error_code = 0;

void writeToI2C(byte _device, byte _address, byte _val) {
  Wire.beginTransmission(_device); 
  Wire.write(_address);             
  Wire.write(_val);                 
  Wire.endTransmission();         
}

void readFromI2C(byte device, byte address, int num, byte buff[]) {
  Wire.beginTransmission(device);  
  Wire.write(address);             
  Wire.endTransmission();         
  
  Wire.beginTransmission(device); 
  Wire.requestFrom(ADX_Address, num);  // Request num Bytes
  
  int i = 0;
  while(Wire.available())         
  { 
    buff[i] = Wire.read();       // Receive Byte
    i++;
  }
  if(i != num){
    error = true;
    error_code = ADXL345_READ_ERROR;
  }
  Wire.endTransmission();           
}





// Ask for two IC2 registers, and combine them in to a word.  Sets error to true
// if there was a read error.
int wordFromRegisters(int reg0, int reg1) {
  // Ask for two registers to be returned
  Wire.beginTransmission(ADX_Address);
  Wire.write(reg0);
  Wire.write(reg1);
  Wire.endTransmission();

  // Try to read the two bytes returned
  Wire.requestFrom(ADX_Address, 2);
  if (Wire.available() >= 2) {
    int value0 = Wire.read();
    int value1 = Wire.read();
    return value0 + (value1 << 8);
  }

  // Weird, asked for two bytes and didn't get them
  error = true;
  error_code = ADXL345_READ_ERROR;
  return 0;
}


// I keep these to one digit before and after decimal, so everything
// lines up always with calls to print(value).
double maxX = 0.0;
double minX = 1.0;
double maxY = 0.0;
double minY = 1.0;
double maxZ = 0.0;
double minZ = 1.0;

// Reset the min/max values for the G readings
int resetMinMax() {
  maxX = 0.0;
  minX = 1.0;
  maxY = 0.0;
  minY = 1.0;
  maxZ = 0.0;
  minZ = 1.0;  
}



void setup() {
  byte oldVal;
  byte newVal;

  // Reset the min/max values to initial defaults.
  resetMinMax();
  
  // TODO: Remove
  Serial.begin(9600);//Set the baud rate of serial monitor as 9600bps

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  // Put the fixed x/y/z labels on the display
  lcd.setCursor(0,0);
  lcd.print("X");
  lcd.setCursor(6,0);
  lcd.print("Y");
  lcd.setCursor(12,0);
  lcd.print("Z");

  // Push button
  pinMode(startOverButtonPin, INPUT);

  // Start IC2 
  Wire.begin();

  // Tell it I want +/- 4G's instead of default 2
  // First get current data format
  readFromI2C(ADX_Address, ADXL345_Register_Data_Format, 1, &oldVal);
  newVal = ADXL_345_Data_Format_4G | (oldVal & ADXL_345_Data_Format_Mask);
  writeToI2C(ADX_Address, ADXL345_Register_Data_Format, ADXL_345_Data_Format_4G);

  // TODO: Want to increase data rate -- default is 10hz, can go up to 3200hz

  // Tell the Accelerometer to start measuring
  writeToI2C(ADX_Address, ADXL345_Register_Power_Control, ADXL345_PowerControl_Measure);
}



void loop() {
  // Start over button pushed?
  int buttonState = digitalRead(startOverButtonPin);
  if (buttonState == HIGH) {
    resetMinMax();
  }
  
  // TODO: Data sheet recommends reading all at once, sample app read each x/y/z in three independent reads
  // Ask the ADXL for X/Y/Z data:
  int x = wordFromRegisters(ADXL345_Register_X0, ADXL345_Register_X1);
  int y = wordFromRegisters(ADXL345_Register_Y0, ADXL345_Register_Y1);
  int z = wordFromRegisters(ADXL345_Register_Z0, ADXL345_Register_Z1);

  // Convert to G's
  // << 1 x2 cuz I'm in 4g mode instead of 2g mode.  I get 10 bits of data off the device.
  // To represent 4, need 3 bits, so 2 bits from the MSB + 1 bit from the LSB
  double Xg = (x << 1) / 256.00; //Convert the output result into the acceleration g, accurate to 2 decimal points.
  double Yg = (y << 1) / 256.00;
  double Zg = (z << 1) / 256.00;


  // Remember min/max for each axis
  if (Xg > maxX) maxX = Xg;
  if (Xg < minX) minX = Xg;
  if (Yg > maxY) maxY = Yg;
  if (Yg < minY) minY = Yg;
  if (Zg > maxZ) maxZ = Zg;
  if (Zg < minZ) minZ = Zg;

  // Display min/max for each axis.  Want:
  //       0123456789012345
  //   0   X 1.2 Y 1.2 Z1.2
  //   1    -1.2  -1.2 -1.2

  // Don't print initial value
  if (maxX >= 0) {
    lcd.setCursor(2, 0);
    lcd.print(maxX, 1);  // Should be two decimla places, like -1.2
  }
  if (maxY >= 0) {
    lcd.setCursor(8, 0);
    lcd.print(maxY, 1);
  }
  if (maxZ >= 0) {
    lcd.setCursor(13, 0);
    lcd.print(maxZ, 1);
  }
  if (minX <= 0) {
    lcd.setCursor(1, 1);
    if (minX == 0) lcd.print(" "); // No sign
    lcd.print(minX, 1);
  }
  if (minY <= 0) {
    lcd.setCursor(7, 1);
    if (minY == 0) lcd.print(" "); // No sign
    lcd.print(minY, 1);
  }
  if (minZ <= 0) {
    lcd.setCursor(12,1);
    if (minZ == 0) lcd.print(" "); // No sign
    lcd.print(minZ, 1);
  }
}

