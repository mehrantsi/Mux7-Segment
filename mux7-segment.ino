#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13

void setReadMode(bool isRead){
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) {
    pinMode(pin, isRead ? INPUT : OUTPUT);
  }
}

void setAddress(int address, bool outputEnabled) {
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address >> 8 | (outputEnabled ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);
  
  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

byte readEEPROM(int address){
  setReadMode(true);
  setAddress(address, true);
  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}

void writeEEPROM(int address, byte data){
  //setting pin mode should happen after setting the address when writing to avoid 
  //a possible contention on the bus, in case OE is active and both arduino
  //and EEPROM try to drive the bus at the same time. in such case if one is 
  //driving a 1 while the other is driving a 0, you effectively have a short
  //from the positive rail to (let's say) EEPROM, through the arduino and to the ground.
  //Since the output impedance of these devices are quite low, a large current like
  //that can damage the output drivers of both devices. In this program, it probably
  //happens in matter of nanoseconds and so the devices probably would be ok, but it's
  //better to be on the safe side of course.
  setAddress(address, false);
  setReadMode(false);
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
  
  digitalWrite(WRITE_EN, LOW);
  delayMicroseconds(1);
  digitalWrite(WRITE_EN, HIGH);
}

void printContents(int length) {
  Serial.println("Reading EEPROM...");
  for (int base = 0; base < length; base += 16) {
    
    byte data[16];
    for (int offset = 0; offset <= 15; offset++){
      data[offset] = readEEPROM(base + offset);
    }
    
    char buf[80];
    sprintf(buf, "%03x:   %02x %02x %02x %02x %02x %02x %02x %02x     %02x %02x %02x %02x %02x %02x %02x %02x",
            base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8],
            data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

    Serial.println(buf);
  }
  Serial.println("Read complete!");
}

void eraseEEPROM(int address, int length)
{
  char data[80];
  sprintf(data, "Erasing EEPROM, starting at 0x%04x,  with length of 0x%04x", address, length);
  Serial.println(data);
  for (int offset = 0x0000; offset < length; offset++) {
    writeEEPROM(address + offset, 0xff);

    if (offset % 0x40 == 0x00) {
      Serial.print(".");
    }
  }
  delay(10);
  Serial.println();
  Serial.println("Erase complete!");
  Serial.println();
}

byte digits[] = {0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b, 0x77, 0x1f, 0x4e, 0x3d, 0x4f, 0x47};
byte minusSign = 0x01;
byte blank = 0x00;

void writeMultiplexedDisplayData()
{
  Serial.println("Writing Multiplexed Display Data...");
  
  // loop over all possible inputs
  for (int i = 0x0000; i < 0x0100; i++)
  {

    if (i % 0x10 == 0x00)
    {
      Serial.print((i / 256.0) * 100.0);
      Serial.println("%");
    }
    
    // unsigned byte as decimal
    {
      int d2 = (i / 100) % 10; // 2nd 7segment display (hundreds)
      int d3 = (i / 10) % 10; // 3rd 7segment display (tenths)
      writeEEPROM(i + 0x0300, blank); // clear first 7segment display
      writeEEPROM(i + 0x0200, (d2 == 0) ? blank : digits[d2]); // write the digit or clear if 0
      writeEEPROM(i + 0x0100, (d2 == 0 && d3 == 0) ? blank : digits[d3]); // write the digit or clear if d3 and d2 are 0
      writeEEPROM(i + 0x0000, digits[i % 10]); // write the digit even 0
    }

    // Two's complement
    if (i < 0x80)
    {
      // positive numbers, same as above
      int d2 = (i / 100) % 10; // 2nd 7segment display (hundreds)
      int d3 = (i / 10) % 10; // 3rd 7segment display (tenths)
      writeEEPROM(i + 0x0700, blank); // clear first 7segment display
      writeEEPROM(i + 0x0600, (d2 == 0) ? blank : digits[d2]); // write the digit or clear if 0
      writeEEPROM(i + 0x0500, (d2 == 0 && d3 == 0) ? blank : digits[d3]); // write the digit or clear if d3 and d2 are 0
      writeEEPROM(i + 0x0400, digits[i % 10]); // write the digit including 0
    }
    else
    {
      // Example
      // d1 d2 d3 d4
      //  -  1  2  3
      //     -  4  2
      //        -  7

      int value = i - 0x0100; //real value as two's complement
      int d2 = (abs(value) / 100) % 10; //hundreds
      int d3 = (abs(value) / 10) % 10; //tenths
      int d4 = abs(value) % 10; //ones

      //first 7segment display
      writeEEPROM(i + 0x0700, d2 == 0 ? blank : minusSign); //write a minus sign to the first 7segment display if the second 7segment display isn't clear

      //second 7segment display
      if (d2 == 0 && d3 == 0) { // if the minus sign is further right
        writeEEPROM(i + 0x0600, blank); // clear second 7segment display
      } else {
        writeEEPROM(i + 0x0600, d2 == 0 ? minusSign : digits[d2]);
      }

      // third 7segment display
      writeEEPROM(i + 0x0500, (d2 == 0 && d3 == 0) ? minusSign : digits[d3]); // write a minus sign if both the second and the third 7segment display are clear

      // last 7segment display
      writeEEPROM(i + 0x0400, digits[d4]); // always write the last digit (ones)
    }
  }
  delay(10);
  Serial.println("Write complete!");
  Serial.println();
}

// 4-bit hex decoder for common anode 7-segment display
//byte sevenSegementHexDecoder[] = { 0x81, 0xcf, 0x92, 0x86, 0xcc, 0xa4, 0xa0, 0x8f, 0x80, 0x84, 0x88, 0xe0, 0xb1, 0xc2, 0xb0, 0xb8 };

// 4-bit hex decoder for common cathode 7-segment display
//byte sevenSegementHexDecoder[] = { 0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b, 0x77, 0x1f, 0x4e, 0x3d, 0x4f, 0x47 };

void setup() {
  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_DATA, OUTPUT);
  //Writing high to the pin before setting the pin mode, will cause
  //the arduino to set a pull-up resistor, so if the arduino is restarting
  //for example, it would ensure a high on the WE pin of EEPROM, while
  //setting the pin mode.
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);
  
  Serial.begin(57600);

  //eraseEEPROM(0x00, 2048);
  
  writeMultiplexedDisplayData();
  
  printContents(2048);
}

void loop() {
  
}
