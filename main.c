#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
//acsessing 16bit registers
//disable ints before writing currentstep etc?
//sample on fixed point in drive cycle or fir it up, when implementing this dont use delay just deassert cs line on the nth interrupt(change n in encoder int)?
//doesnt the sine interrupt fire to fast?

#define F_CPU 20000000UL
#define a_bit 10
#define measurement_interval 3//in s
#define silence_time_measurement 220//in ms, check how long it actually takes,discard lower bits?
#define screen_intensity 7//0-15
#define max_temp 2000//in 0.25C
#define thermocouple_linearity_adjust 1
#define thermocouple_offset 0//in 0.25C

#define cs_thermocouple PB1
#define trigger_src PB2
#define mosi PB3
#define sclk PB5
#define cs_screen PC3
#define button PC5
#define dir_encoder PD1
#define sense_zerocrossing PD2//switch 2 and 3?
#define interrupt_encoder PD3
#define out_PWM0 PD5
#define out_PWM1 PD6
#define en_coil PD7

typedef enum {noop, segment1, segment2, segment3, segment4, segment5, segment6,
	segment7,segment8, decode, intensity, length, enable} adress;

	void output_to_screen(uint8_t data, adress adress);
	void write_number_to_screen(uint16_t number);
	void read_temp();
	void step_PID();

    const uint8_t sine[256] PROGMEM = {128, 130, 133, 136, 139, 142, 145, 148, 151, 154, 157, 159, 162, 165, 168, 171, 173, 176, 179, 181, 184, 187, 189, 192, 194, 197, 199, 201, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230, 232, 233, 234, 236, 237, 238, 239, 240, 241, 242, 243, 244, 244, 245, 245, 246, 246, 246, 247, 247, 247, 247, 247, 247, 247, 246, 246, 246, 245, 245, 244, 243, 242, 242, 241, 240, 239, 238, 236, 235, 234, 232, 231, 229, 228, 226, 225, 223, 221, 219, 217, 215, 213, 211, 209, 207, 205, 202, 200, 198, 195, 193, 190, 188, 185, 183, 180, 177, 175, 172, 169, 167, 164, 161, 158, 155, 152, 149, 147, 144, 141, 138, 135, 132, 129, 126, 123, 120, 117, 114, 111, 108, 106, 103, 100, 97, 94, 91, 88, 86, 83, 80, 78, 75, 72, 70, 67, 65, 62, 60, 57, 55, 53, 50, 48, 46, 44, 42, 40, 38, 36, 34, 32, 30, 29, 27, 26, 24, 23, 21, 20, 19, 17, 16, 15, 14, 13, 13, 12, 11, 10, 10, 9, 9, 9, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 28, 30, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 54, 56, 58, 61, 63, 66, 68, 71, 74, 76, 79, 82, 84, 87, 90, 93, 96, 98, 101, 104, 107, 110, 113, 116, 119, 122, 125, 127};
	const uint8_t timesforpowers[100] PROGMEM = {};
	const uint8_t font[3] PROGMEM = {0b01000111,0b00001111,0b01011011};//f,t,s(add other characters too so you dont have to switch?)

	/*
	uint16_t prev_error=0;
	uint16_t error_int=0;
	uint8_t kp=0;
	uint8_t ki=0;
	uint8_t kd=0;//use int?
	 */

	uint8_t no_conversion_busy=0;
	uint16_t temp=0;
	uint16_t temp_prev=0;
	uint16_t freq=0;//in 0.25 Hz
	uint8_t current_step=0;

	int main(){
		//disable unused periferals
		PRR=0b10000011;
		//disable certain digital input buffers?
		//enable interrupts
		sei();

		//setup pins
		DDRB = 1<<cs_thermocouple|1<<trigger_src|1<<mosi|1<<sclk;//1 for output
		PORTB= 1<<cs_thermocouple;
		DDRC = 1<<cs_screen;
		DDRC = 1<<cs_thermocouple;
		DDRD = 1<<out_PWM0|1<<out_PWM1|1<<en_coil;

		//setup external interrupts
		EICRA = 0b00001111;//trigger mode
		EIMSK = 0b00000011;//INT en
		PCMSK1=1<<PCINT13;//PCINT pins
		PCMSK2=1<<PCINT18;
		PCICR = 0b00000010;//PCINT en


		//setup pwm counter, can this steer two outputs
		TCCR0A=0b11110011;//fast pwm, set on match
		TCCR0B=0b00000101;//div1024

		//setup fase counter
		OCR1AH=0;
		OCR1AL=4;//top,why is this the same register as match(can this timer only output on one pin?),minimum top?
		OCR1BH=0;
		OCR1BL=0;//match, should be max-time on

		//setup step sine counter
		TCCR2B=0b00000111;
		TIMSK2=0b00000010;
		OCR2A=255;


		//setup spi, en, msb first, trailing falling, read on trailing, f_cpu/8=2.5MHz
		SPCR=0b01010001;
		SPSR=0b00000001;
		//delay?

		//screen setup
		output_to_screen(0b00111111,decode);
		output_to_screen(screen_intensity,intensity);
		output_to_screen(0b00000111,length);
		output_to_screen(1,enable);

		while(1){

		}

		return 1;
	}

	void write_number_to_screen(uint16_t number){
		number=number*25;
		uint8_t a=0;

		while (number & 0xC000)
		{    number -= 10000;
		a += 1;
		}
		if (number >= 10000)
		{    number -= 10000;
		a += 1;
		}
		output_to_screen(a,segment5);

		a = 0;
		while (number & 0x3C00)
		{    number -= 1000;
		a += 1;
		}
		if (number >= 1000)
		{    number -= 1000;
		a += 1; }
		output_to_screen(a,segment4);

		a = 0;
		while (number & 0x0780)
		{    number -= 100;
		a += 1;
		}
		if (number >= 100)
		{    number -= 100;
		a += 1; }
		output_to_screen(a|0b10000000,segment3);

		a = 0;
		while (number & 0x70)
		{    number -= 10;
		a += 1;
		}
		if (number >= 10)
		{    number -= 10;
		a += 1; }
		output_to_screen(a,segment2);

		output_to_screen(number,segment1);
	}

	void output_to_screen(uint8_t data, adress adress){
		//cs
		PORTC &= 0b11110111;
		// set adress
		SPDR = (uint8_t)adress;
		while(!(SPSR & (1<<SPIF)));
		SPDR = data;
		while(!(SPSR & (1<<SPIF)));
		//cs
		PORTC |= 0b00001000;
	}

	void read_temp(){
		temp_prev=temp;
		//turn off sitrrer

		//_delay_ms(1);

		//cs high, start conversion
		PORTB |= 0b00000100;
		_delay_ms(silence_time_measurement);//this time doesnt have to be wasted, pid?(how well does this respond to delays)
		//cs low, send data
		PORTB &= 0b11111011;
		//read data
		SPDR = 0xFF;
		while(!(SPSR & (1<<SPIF)));
		temp=(uint16_t)SPDR;
		SPDR = 0xFF;
		while(!(SPSR & (1<<SPIF)));
		temp=(temp<<5)|(SPDR>>3);

		//turn on stirrer

		step_PID();
	}

	void step_PID(){

	}

	//rotary encoder int
	ISR(INT1_vect)
	{
		//x=x-1 +(PIND&dir_encoder)<<1;
		//update screen
		//update sine timer
	}

	//stepsine timer int
	ISR(TIMER2_COMPA_vect)
	{
		OCR0A=sine[current_step];
		OCR0B=sine[current_step+64];
		current_step++;
		if(current_step == 0 && no_conversion_busy){
			//read_temp();//no break up read temp set line in this one reset and readout in other(after time)
		}
	}

	//button int
	ISR(PCINT1_vect)
	{
		if(PINC==0){

		}
	}

	//zerocrossing int
	ISR(INT0_vect)
	{
		TCNT1=0;//should be match-delay
	}
