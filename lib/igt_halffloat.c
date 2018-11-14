/*
 * Copyright (c) 2005-2017, NumPy Developers.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *     * Neither the name of the NumPy Developers nor the names of any
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "igt_halffloat.h"

uint16_t igt_floatbits_to_halfbits(uint32_t fp32)
{
	uint32_t f_exp, f_sig;
	uint16_t h_sgn, h_exp, h_sig;

	h_sgn = (uint16_t)((fp32 & 0x80000000) >> 16);
	f_exp = (fp32 & 0x7f800000);

	/* Exponent overflow/NaN converts to signed inf/NaN */
	if (f_exp >= 0x47800000) {
		if (f_exp == 0x7f800000) {
			/* Inf or NaN */
			f_sig = (fp32 & 0x007fffff);
			if (f_sig != 0) {
				/* NaN - propagate the flag in the significand... */
				uint16_t ret = (uint16_t)(0x7c00 + (f_sig >> 13));
				/* ...but make sure it stays a NaN */
				if (ret == 0x7c00) {
					ret++;
				}
				return h_sgn + ret;
			} else {
				/* signed inf */
				return (uint16_t)(h_sgn + 0x7c00);
			}
		} else {
			/* overflow to signed inf */
			return (uint16_t)(h_sgn + 0x7c00);
		}
	}

	/* Exponent underflow converts to a subnormal half or signed zero */
	if (f_exp <= 0x38000000) {
		/*
		 * Signed zeros, subnormal floats, and floats with small
		 * exponents all convert to signed zero half-floats.
		 */
		if (f_exp < 0x33000000) {
			return h_sgn;
		}
		/* Make the subnormal significand */
		f_exp >>= 23;
		f_sig = (0x00800000 + (fp32 & 0x007fffff));

		f_sig >>= (113 - f_exp);
		/* Handle rounding by adding 1 to the bit beyond half precision */
		f_sig += 0x00001000;

		h_sig = (uint16_t)(f_sig >> 13);
		/*
		 * If the rounding causes a bit to spill into h_exp, it will
		 * increment h_exp from zero to one and h_sig will be zero.
		 * This is the correct result.
		 */
		return (uint16_t)(h_sgn + h_sig);
	}

	/* Regular case with no overflow or underflow */
	h_exp = (uint16_t)((f_exp - 0x38000000) >> 13);
	/* Handle rounding by adding 1 to the bit beyond half precision */
	f_sig = (fp32 & 0x007fffff);

	f_sig += 0x00001000;

	h_sig = (uint16_t)(f_sig >> 13);

	/*
	 * If the rounding causes a bit to spill into h_exp, it will
	 * increment h_exp by one and h_sig will be zero.  This is the
	 * correct result.  h_exp may increment to 15, at greatest, in
	 * which case the result overflows to a signed inf.
	 */
	return h_sgn + h_exp + h_sig;
}

uint32_t igt_halfbits_to_floatbits(uint16_t fp16)
{
	uint16_t h_exp, h_sig;
	uint32_t f_sgn, f_exp, f_sig;

	h_exp = (fp16 & 0x7c00);
	f_sgn = ((uint32_t)fp16 & 0x8000) << 16;
	switch (h_exp) {
		case 0x0000: /* 0 or subnormal */
			h_sig = (fp16 & 0x03ff);
			/* Signed zero */
			if (h_sig == 0) {
				return f_sgn;
			}
			/* Subnormal */
			h_sig <<= 1;
			while ((h_sig & 0x0400) == 0) {
				h_sig <<= 1;
				h_exp++;
			}
			f_exp = ((uint32_t)(127 - 15 - h_exp)) << 23;
			f_sig = ((uint32_t)(h_sig & 0x03ffu)) << 13;
			return f_sgn + f_exp + f_sig;
		case 0x7c00: /* inf or NaN */
			/* All-ones exponent and a copy of the significand */
			return f_sgn + 0x7f800000 + (((uint32_t)(fp16 & 0x03ff)) << 13);
		default: /* normalized */
			/* Just need to adjust the exponent and shift */
			return f_sgn + (((uint32_t)(fp16 & 0x7fff) + 0x1c000) << 13);
	}
}

float igt_half_to_float(igt_half h)
{
	union { float ret; uint32_t retbits; } conv;
	conv.retbits = igt_halfbits_to_floatbits(h);
	return conv.ret;
}

igt_half igt_float_to_half(float f)
{
	union { float f; uint32_t fbits; } conv;
	conv.f = f;
	return igt_floatbits_to_halfbits(conv.fbits);
}

int igt_half_isnan(igt_half h)
{
	return ((h&0x7c00u) == 0x7c00u) && ((h&0x03ffu) != 0x0000u);
}

int igt_half_isinf(igt_half h)
{
	return ((h&0x7fffu) == 0x7c00u);
}

int igt_half_isfinite(igt_half h)
{
	return ((h&0x7c00u) != 0x7c00u);
}

