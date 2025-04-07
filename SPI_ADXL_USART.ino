// Obiective:
// 1. configurare interfata SPI
// 2. scrierea si citirea pe SPI
// 3. utilizarea accelerometru ADXL345

// Componente
// Senzor acelerometru ADXL345

// Documentatie
// ADXL345 Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf

// HINT
// use UART lib from lab2 to send messages to PC
// use itoa to convert int to string



// ------------------- Config -------------------
#define CS_ADXL PD7
#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR ((F_CPU / (16UL * BAUD)) - 1)

// ------------------- ADXL SPI Communication -------------------
void ADXL_enable() {
  // Activăm comunicarea cu ADXL345 (CS -> LOW)
  PORTD &= ~(1 << CS_ADXL);
}

void ADXL_disable() {
  // Dezactivăm comunicarea cu ADXL345 (CS -> HIGH)
  PORTD |= (1 << CS_ADXL);
}

// ------------------- UART Class -------------------
class UART {
  public:
    void init() {
      int ubrr = MYUBRR;

      // Setăm baud rate
      UBRR0H = (unsigned char)(ubrr >> 8);
      UBRR0L = (unsigned char)ubrr;

      // Activăm recepția și transmisia
      UCSR0B = (1 << RXEN0) | (1 << TXEN0);

      // Tx (PD1) ca ieșire, Rx (PD0) ca intrare
      DDRD |= 0b00000010;
      DDRD &= 0b11111110;

      // Format: 8 biți, fără paritate, 1 stop bit
      UCSR0C = (0 << USBS0) | (3 << UCSZ00);
    }

    bool available() {
      return (UCSR0A & (1 << RXC0));
    }

    void writeByte(const char& d) {
      // Așteaptă până buffer-ul e gol, apoi transmite
      while (!(UCSR0A & (1 << UDRE0)));
      UDR0 = d;
    }

    char readByte() {
      // Așteaptă până primește un caracter
      while (!(UCSR0A & (1 << RXC0)));
      return UDR0;
    }

    void writeString(const char* msg) {
      for (int i = 0; i < strlen(msg); i++)
        writeByte(msg[i]);
    }
};

// ------------------- SPI Master Class -------------------
class SpiMaster {
  public:
    void init() {
      // MOSI (PB3) și SCK (PB5) ca ieșiri, MISO (PB4) ca intrare
      DDRB |= (1 << PB3) | (1 << PB5);
      DDRB &= ~(1 << PB4);

      // Activare SPI, master mode, CPOL=1, CPHA=1 (front descendent)
      SPCR = (1 << SPE) | (1 << MSTR) | (1 << CPOL) | (1 << CPHA);

      // CS ca ieșire și setat HIGH
      DDRB |= (1 << PB2);
      PORTB |= (1 << PB2);
    }

    char transmit(char d) {
      // Transmitere byte și așteptare finalizare
      SPDR = d;
      while (!(SPSR & (1 << SPIF)));
      return SPDR;
    }
};

// ------------------- Global Objects -------------------
SpiMaster spi;
UART uart;

// ------------------- ADXL Helper -------------------
char ADXL_cmdBuilder(char addr, bool rw, bool mb = false) {
  // Construcție comandă SPI:
  // bit 7: R/W (1 = read)
  // bit 6: MB (multi-byte)
  // bit 5-0: adresă
  return (addr & 0b00111111) | (mb << 6) | (rw << 7);
}

// ------------------- Setup -------------------
void setup() {
  DDRD |= (1 << CS_ADXL);  // Pin CS ca ieșire
  ADXL_disable();

  spi.init();
  uart.init();

  delay(500);  // Timp de inițializare

  // Citire Device ID
  ADXL_enable();
  spi.transmit(1 << 7);  // Read DEVID (0x00)
  char device_id = spi.transmit(0x00);
  ADXL_disable();

  char buffer[33];
  uart.writeString("ID: ");
  itoa(device_id, buffer, 16);
  uart.writeString(buffer);
  uart.writeString("\n");

  // Activare modul măsurare (scriem în POWER_CTL)
  delay(500);
  ADXL_enable();
  spi.transmit(ADXL_cmdBuilder(0x2D, 0));  // Write la registrul 0x2D
  spi.transmit(0x08);                      // Setăm bitul Measure (D3)
  ADXL_disable();
}

// ------------------- Read ADXL Data -------------------
void ADXL_readData() {
  // Citim 6 octeți de la adresa 0x32 (DATAX0)
  char data[6];
  char buffer[10];

  ADXL_enable();
  spi.transmit(ADXL_cmdBuilder(0x32, 1, true));  // Read multibyte
  for (int i = 0; i < 6; i++) {
    data[i] = spi.transmit(0x00);
  }
  ADXL_disable();

  // Conversie little-endian: LSB + MSB => int16_t
  int16_t X = (int16_t)((data[1] << 8) | (uint8_t)data[0]);
  int16_t Y = (int16_t)((data[3] << 8) | (uint8_t)data[2]);
  int16_t Z = (int16_t)((data[5] << 8) | (uint8_t)data[4]);

  // Transmitere prin UART
  uart.writeString("X: ");
  itoa(X, buffer, 10);
  uart.writeString(buffer);

  uart.writeString(" | Y: ");
  itoa(Y, buffer, 10);
  uart.writeString(buffer);

  uart.writeString(" | Z: ");
  itoa(Z, buffer, 10);
  uart.writeString(buffer);

  uart.writeString("\n");
}

// ------------------- Loop -------------------
void loop() {
  ADXL_readData();
  delay(500);
}
