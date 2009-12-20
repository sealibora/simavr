/*
	sim_tiny2313.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include </usr/include/stdio.h>
#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "avr_timer.h"

static void init(struct avr_t * avr);
static void reset(struct avr_t * avr);

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iotn2313.h"

/*
 * This is a template for all of the tinyx5 devices, hopefully
 */
static struct mcu_t {
	avr_t core;
	avr_eeprom_t 	eeprom;
	avr_extint_t	extint;
	avr_ioport_t	portb, portd;
	avr_uart_t		uart;
	avr_timer_t		timer0,timer1;
} mcu = {
	.core = {
		.mmcu = "attiny2313",
		DEFAULT_CORE(2),

		.init = init,
		.reset = reset,
	},
	AVR_EEPROM_DECLARE_8BIT(EEPROM_READY_vect),
	.extint = {
		AVR_EXTINT_TINY_DECLARE(0, 'D', 2, EIFR),
		AVR_EXTINT_TINY_DECLARE(1, 'D', 3, EIFR),
	},
	.portb = {
		.name = 'B',  .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
		.pcint = {
			.enable = AVR_IO_REGBIT(GIMSK, PCIE),
			.raised = AVR_IO_REGBIT(EIFR, PCIF),
			.vector = PCINT_vect,
		},
		.r_pcint = PCMSK,
	},
	.portd = {	// port D has no PCInts..
		.name = 'D', .r_port = PORTD, .r_ddr = DDRD, .r_pin = PIND,
	},
	.uart = {
		// no PRR register on the 2313
		//.disabled = AVR_IO_REGBIT(PRR,PRUSART0),
		.name = '0',
		.r_udr = UDR,

		.txen = AVR_IO_REGBIT(UCSRB, TXEN),
		.rxen = AVR_IO_REGBIT(UCSRB, RXEN),

		.r_ucsra = UCSRA,
		.r_ucsrb = UCSRB,
		.r_ucsrc = UCSRC,
		.r_ubrrl = UBRRL,
		.r_ubrrh = UBRRH,
		.rxc = {
			.enable = AVR_IO_REGBIT(UCSRB, RXCIE),
			.raised = AVR_IO_REGBIT(UCSRA, RXC),
			.vector = USART_RX_vect,
		},
		.txc = {
			.enable = AVR_IO_REGBIT(UCSRB, TXCIE),
			.raised = AVR_IO_REGBIT(UCSRA, TXC),
			.vector = USART_TX_vect,
		},
		.udrc = {
			.enable = AVR_IO_REGBIT(UCSRB, UDRIE),
			.raised = AVR_IO_REGBIT(UCSRA, UDRE),
			.vector = USART_UDRE_vect,
		},
	},
	.timer0 = {
		.name = '0',
		.wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM(),
			[7] = AVR_TIMER_WGM_FASTPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */ },

		.r_ocra = OCR0A,
		.r_ocrb = OCR0B,
		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR, TOV0),
			.vector = TIMER0_OVF_vect,
		},
		.compa = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE0A),
			.raised = AVR_IO_REGBIT(TIFR, OCF0A),
			.vector = TIMER0_COMPA_vect,
		},
		.compb = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE0B),
			.raised = AVR_IO_REGBIT(TIFR, OCF0B),
			.vector = TIMER0_COMPB_vect,
		},
	},
	.timer1 = {
		.name = '1',
	//	.disabled = AVR_IO_REGBIT(PRR,PRTIM1),
		.wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
					AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL16(),
			[4] = AVR_TIMER_WGM_CTC(),
			[5] = AVR_TIMER_WGM_FASTPWM(),
			[6] = AVR_TIMER_WGM_FASTPWM(),
			[7] = AVR_TIMER_WGM_FASTPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11), AVR_IO_REGBIT(TCCR1B, CS12) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* External clock T1 is not handled */},

		.r_ocra = OCR1AL,
		.r_ocrb = OCR1BL,
		.r_tcnt = TCNT1L,

		.r_ocrah = OCR1AH,	// 16 bits timers have two bytes of it
		.r_ocrbh = OCR1BH,
		.r_tcnth = TCNT1H,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE1),
			.raised = AVR_IO_REGBIT(TIFR, TOV1),
			.vector = TIMER1_OVF_vect,
		},
		.compa = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE1A),
			.raised = AVR_IO_REGBIT(TIFR, OCF1A),
			.vector = TIMER1_COMPA_vect,
		},
		.compb = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE1B),
			.raised = AVR_IO_REGBIT(TIFR, OCF1B),
			.vector = TIMER1_COMPB_vect,
		},
		.icr = {
			.enable = AVR_IO_REGBIT(TIMSK, ICIE1),
			.raised = AVR_IO_REGBIT(TIFR, ICF1),
			.vector = TIMER1_CAPT_vect,
		},
	},
};

static avr_t * make()
{
	return &mcu.core;
}

avr_kind_t tiny2313 = {
	.names = { "attiny2313", "attiny2313v" },
	.make = make
};

static void init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;

	printf("%s init\n", avr->mmcu);

	avr_eeprom_init(avr, &mcu->eeprom);
	avr_ioport_init(avr, &mcu->portb);
	avr_ioport_init(avr, &mcu->portd);
	avr_uart_init(avr, &mcu->uart);
	avr_timer_init(avr, &mcu->timer0);
	avr_timer_init(avr, &mcu->timer1);
}

static void reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}
