// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "build/build_config.h"
#include "skia/ext/convolver.h"
#include "skia/ext/convolver_LSX.h"
#include "third_party/skia/include/core/SkTypes.h"

#include <lsxintrin.h>
#define LSX_LD(psrc) *((__m128i *)(psrc))
#define LSX_ST(in, pdst) *((__m128i *)(pdst)) = (in)
#define _MM_SHUFFLE(z, y, x, w) (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))

namespace skia {
static __m128i emulate_lsx_set_epi16(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
                                     uint16_t e, uint16_t f, uint16_t g, uint16_t h) {
    v8u16 retv = {h, g, f, e, d, c, b, a};
    return (__m128i)retv;
}
static __m128i emulate_lsx_loadl_epi64(const void* mem_addr) {
    __m128i tmp = __lsx_vldi(0);
    __m128i ptr_lsx = __lsx_vldrepl_d((void *)(mem_addr), 0);
    return __lsx_vilvl_d(tmp, ptr_lsx);
}
static __m128i emulate_lsx_shufflelo_epi16_0(__m128i data) {
    v4i32 v0 = {0, 0, -1, -1};
    __m128i v_hi = __lsx_vand_v(data, (__m128i)v0);
    data = __lsx_vshuf4i_h(data, _MM_SHUFFLE(1, 1, 0, 0));
    v0 = (v4i32)__lsx_vnor_v((__m128i)v0, (__m128i)v0);
    data = __lsx_vand_v(data, (__m128i)v0);
    return __lsx_vor_v(data, v_hi);
}
static __m128i emulate_lsx_shufflelo_epi16_1(__m128i data) {
    v4i32 v0 = {0, 0, -1, -1};
    __m128i v_hi = __lsx_vand_v(data, (__m128i)v0);
    data = __lsx_vshuf4i_h(data, _MM_SHUFFLE(3, 3, 2, 2));
    v0 = (v4i32)__lsx_vnor_v((__m128i)v0, (__m128i)v0);
    data = __lsx_vand_v(data, (__m128i)v0);
    return __lsx_vor_v(data, v_hi);
}

static __m128i emulate_lsx_packs_epi32(__m128i a, __m128i b) {
    __m128i tmp0 = __lsx_vsat_w(a, 15);
    __m128i tmp1 = __lsx_vsat_w(b, 15);
    return __lsx_vpickev_h(tmp1, tmp0);
}
static __m128i emulate_lsx_packus_epi16(__m128i a, __m128i b) {
    a = __lsx_vmaxi_h(a, 0);
    b = __lsx_vmaxi_h(b, 0);
    __m128i tmp0 = __lsx_vsat_hu(a, 7);
    __m128i tmp1 = __lsx_vsat_hu(b, 7);
    return __lsx_vpickev_b(tmp1, tmp0);
}
static __m128i emulate_lsx_srai_epi32(__m128i a, int imm8) {
    __m128i tmp0 = __lsx_vldrepl_w(&imm8, 0);
    return __lsx_vsra_w(a, tmp0);
}

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the num_values() of the filter.
void ConvolveHorizontally_LSX(const unsigned char* src_data,
                              const ConvolutionFilter1D& filter,
                              unsigned char* out_row,
                              bool /*has_alpha*/) {

  int num_values = filter.num_values();

  int filter_offset, filter_length;
  __m128i zero = __lsx_vldi(0);
  __m128i mask[4];
  // |mask| will be used to decimate all extra filter coefficients that are
  // loaded by SIMD when |filter_length| is not divisible by 4.
  // mask[0] is not used in following algorithm.
  mask[1] = emulate_lsx_set_epi16(0, 0, 0, 0, 0, 0, 0, -1);
  mask[2] = emulate_lsx_set_epi16(0, 0, 0, 0, 0, 0, -1, -1);
  mask[3] = emulate_lsx_set_epi16(0, 0, 0, 0, 0, -1, -1, -1);

  // Output one pixel each iteration, calculating all channels (RGBA) together.
  for (int out_x = 0; out_x < num_values; out_x++) {
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    __m128i accum = __lsx_vldi(0);

    // Compute the first pixel in this row that the filter affects. It will
    // touch |filter_length| pixels (4 bytes each) after this.
    const __m128i* row_to_filter =
        reinterpret_cast<const __m128i*>(&src_data[filter_offset << 2]);

    // We will load and accumulate with four coefficients per iteration.
    for (int filter_x = 0; filter_x < filter_length >> 2; filter_x++) {

      // Load 4 coefficients => duplicate 1st and 2nd of them for all channels.
      __m128i coeff, coeff16;
      // [16] xx xx xx xx c3 c2 c1 c0
      coeff = emulate_lsx_loadl_epi64(reinterpret_cast<const __m128i*>(filter_values));
      // [16] xx xx xx xx c1 c1 c0 c0
      coeff16 = emulate_lsx_shufflelo_epi16_0(coeff);
      // [16] c1 c1 c1 c1 c0 c0 c0 c0
      coeff16 = __lsx_vilvl_h(coeff16, coeff16);

      // Load four pixels => unpack the first two pixels to 16 bits =>
      // multiply with coefficients => accumulate the convolution result.
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src8 = LSX_LD(row_to_filter);
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = __lsx_vilvl_b(zero, src8);
      __m128i mul_hi = __lsx_vmuh_h(src16, coeff16);
      __m128i mul_lo = __lsx_vmul_h(src16, coeff16);
      // [32]  a0*c0 b0*c0 g0*c0 r0*c0
      __m128i t = __lsx_vilvl_h(mul_hi, mul_lo);
      accum = __lsx_vadd_w(accum, t);
      // [32]  a1*c1 b1*c1 g1*c1 r1*c1
      t = __lsx_vilvh_h(mul_hi, mul_lo);
      accum = __lsx_vadd_w(accum, t);

      // Duplicate 3rd and 4th coefficients for all channels =>
      // unpack the 3rd and 4th pixels to 16 bits => multiply with coefficients
      // => accumulate the convolution results.
      // [16] xx xx xx xx c3 c3 c2 c2
      coeff16 = emulate_lsx_shufflelo_epi16_1(coeff);
      // [16] c3 c3 c3 c3 c2 c2 c2 c2
      coeff16 = __lsx_vilvl_h(coeff16, coeff16);
      // [16] a3 g3 b3 r3 a2 g2 b2 r2
      src16 = __lsx_vilvh_b(zero, src8);
      mul_hi = __lsx_vmuh_h(src16, coeff16);
      mul_lo = __lsx_vmul_h(src16, coeff16);
      // [32]  a2*c2 b2*c2 g2*c2 r2*c2
      t = __lsx_vilvl_h(mul_hi, mul_lo);
      accum = __lsx_vadd_w(accum, t);
      // [32]  a3*c3 b3*c3 g3*c3 r3*c3
      t = __lsx_vilvh_h(mul_hi, mul_lo);
      accum = __lsx_vadd_w(accum, t);

      // Advance the pixel and coefficients pointers.
      row_to_filter += 1;
      filter_values += 4;
    }

    // When |filter_length| is not divisible by 4, we need to decimate some of
    // the filter coefficient that was loaded incorrectly to zero; Other than
    // that the algorithm is same with above, exceot that the 4th pixel will be
    // always absent.
    int r = filter_length&3;
    if (r) {
      // Note: filter_values must be padded to align_up(filter_offset, 8).
      __m128i coeff, coeff16;
      coeff = emulate_lsx_loadl_epi64(reinterpret_cast<const __m128i*>(filter_values));
      // Mask out extra filter taps.
      coeff = __lsx_vand_v(coeff, mask[r]);
      coeff16 = emulate_lsx_shufflelo_epi16_0(coeff);
      coeff16 = __lsx_vilvl_h(coeff16, coeff16);

      // Note: line buffer must be padded to align_up(filter_offset, 16).
      // We resolve this by use C-version for the last horizontal line.
      __m128i src8 = LSX_LD(row_to_filter);
      __m128i src16 = __lsx_vilvl_b(zero, src8);
      __m128i mul_hi = __lsx_vmuh_h(src16, coeff16);
      __m128i mul_lo = __lsx_vmul_h(src16, coeff16);
      __m128i t = __lsx_vilvl_h(mul_hi, mul_lo);
      accum = __lsx_vadd_w(accum, t);
      t = __lsx_vilvh_h(mul_hi, mul_lo);
      accum = __lsx_vadd_w(accum, t);

      src16 = __lsx_vilvh_b(zero, src8);
      coeff16 = emulate_lsx_shufflelo_epi16_1(coeff);
      coeff16 = __lsx_vilvl_h(coeff16, coeff16);
      mul_hi = __lsx_vmuh_h(src16, coeff16);
      mul_lo = __lsx_vmul_h(src16, coeff16);
      t = __lsx_vilvl_h(mul_hi, mul_lo);
      accum = __lsx_vadd_w(accum, t);
    }

    // Shift right for fixed point implementation.
    accum = emulate_lsx_srai_epi32(accum, ConvolutionFilter1D::kShiftBits);

    // Packing 32 bits |accum| to 16 bits per channel (signed saturation).
    accum = emulate_lsx_packs_epi32(accum, zero);
    // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
    accum = emulate_lsx_packus_epi16(accum, zero);

    // Store the pixel value of 32 bits.
    *(reinterpret_cast<int*>(out_row)) = __lsx_vpickve2gr_w(accum, 0);
    out_row += 4;
  }
}

// Convolves horizontally along four rows. The row data is given in
// |src_data| and continues for the num_values() of the filter.
// The algorithm is almost same as |ConvolveHorizontally_LSX|. Please
// refer to that function for detailed comments.
void Convolve4RowsHorizontally_LSX(const unsigned char* src_data[4],
                                   const ConvolutionFilter1D& filter,
                                   unsigned char* out_row[4]) {
  int num_values = filter.num_values();
  int filter_offset, filter_length;
  __m128i zero = __lsx_vldi(0);
  __m128i mask[4];
  // |mask| will be used to decimate all extra filter coefficients that are
  // loaded by SIMD when |filter_length| is not divisible by 4.
  // mask[0] is not used in following algorithm.
  mask[1] = emulate_lsx_set_epi16(0, 0, 0, 0, 0, 0, 0, -1);
  mask[2] = emulate_lsx_set_epi16(0, 0, 0, 0, 0, 0, -1, -1);
  mask[3] = emulate_lsx_set_epi16(0, 0, 0, 0, 0, -1, -1, -1);

  // Output one pixel each iteration, calculating all channels (RGBA) together.
  for (int out_x = 0; out_x < num_values; out_x++) {
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    // four pixels in a column per iteration.
    __m128i accum0 = __lsx_vldi(0);
    __m128i accum1 = __lsx_vldi(0);
    __m128i accum2 = __lsx_vldi(0);
    __m128i accum3 = __lsx_vldi(0);
    int start = (filter_offset<<2);
    // We will load and accumulate with four coefficients per iteration.
    for (int filter_x = 0; filter_x < (filter_length >> 2); filter_x++) {
      __m128i coeff, coeff16lo, coeff16hi;
      // [16] xx xx xx xx c3 c2 c1 c0
      coeff = emulate_lsx_loadl_epi64(reinterpret_cast<const __m128i*>(filter_values));
      // [16] xx xx xx xx c1 c1 c0 c0
      coeff16lo = emulate_lsx_shufflelo_epi16_0(coeff);
      // [16] c1 c1 c1 c1 c0 c0 c0 c0
      coeff16lo = __lsx_vilvl_h(coeff16lo, coeff16lo);
      // [16] xx xx xx xx c3 c3 c2 c2
      coeff16hi = emulate_lsx_shufflelo_epi16_1(coeff);
      // [16] c3 c3 c3 c3 c2 c2 c2 c2
      coeff16hi = __lsx_vilvl_h(coeff16hi, coeff16hi);

      __m128i src8, src16, mul_hi, mul_lo, t;

#define ITERATION(src, accum)                                     \
      src8 = LSX_LD(reinterpret_cast<const __m128i*>(src));       \
      src16 = __lsx_vilvl_b(zero, src8);                          \
      mul_hi = __lsx_vmuh_h(src16, coeff16lo);                    \
      mul_lo = __lsx_vmul_h(src16, coeff16lo);                    \
      t = __lsx_vilvl_h(mul_hi, mul_lo);                          \
      accum = __lsx_vadd_w(accum, t);                             \
      t = __lsx_vilvh_h(mul_hi, mul_lo);                          \
      accum = __lsx_vadd_w(accum, t);                             \
      src16 = __lsx_vilvh_b(zero, src8);                          \
      mul_hi = __lsx_vmuh_h(src16, coeff16hi);                    \
      mul_lo = __lsx_vmul_h(src16, coeff16hi);                    \
      t = __lsx_vilvl_h(mul_hi, mul_lo);                          \
      accum = __lsx_vadd_w(accum, t);                             \
      t = __lsx_vilvh_h(mul_hi, mul_lo);                          \
      accum = __lsx_vadd_w(accum, t)

      ITERATION(src_data[0] + start, accum0);
      ITERATION(src_data[1] + start, accum1);
      ITERATION(src_data[2] + start, accum2);
      ITERATION(src_data[3] + start, accum3);

      start += 16;
      filter_values += 4;
    }

    int r = filter_length & 3;
    if (r) {
      // Note: filter_values must be padded to align_up(filter_offset, 8);
      __m128i coeff;
      coeff = emulate_lsx_loadl_epi64(reinterpret_cast<const __m128i*>(filter_values));
      // Mask out extra filter taps.
      coeff = __lsx_vand_v(coeff, mask[r]);

      __m128i coeff16lo = emulate_lsx_shufflelo_epi16_0(coeff);
      /* c1 c1 c1 c1 c0 c0 c0 c0 */
      coeff16lo = __lsx_vilvl_h(coeff16lo, coeff16lo);
      __m128i coeff16hi = emulate_lsx_shufflelo_epi16_1(coeff);
      coeff16hi = __lsx_vilvl_h(coeff16hi, coeff16hi);

      __m128i src8, src16, mul_hi, mul_lo, t;

      ITERATION(src_data[0] + start, accum0);
      ITERATION(src_data[1] + start, accum1);
      ITERATION(src_data[2] + start, accum2);
      ITERATION(src_data[3] + start, accum3);
    }

    accum0 = emulate_lsx_srai_epi32(accum0, ConvolutionFilter1D::kShiftBits);
    accum0 = emulate_lsx_packs_epi32(accum0, zero);
    accum0 = emulate_lsx_packus_epi16(accum0, zero);
    accum1 = emulate_lsx_srai_epi32(accum1, ConvolutionFilter1D::kShiftBits);
    accum1 = emulate_lsx_packs_epi32(accum1, zero);
    accum1 = emulate_lsx_packus_epi16(accum1, zero);
    accum2 = emulate_lsx_srai_epi32(accum2, ConvolutionFilter1D::kShiftBits);
    accum2 = emulate_lsx_packs_epi32(accum2, zero);
    accum2 = emulate_lsx_packus_epi16(accum2, zero);
    accum3 = emulate_lsx_srai_epi32(accum3, ConvolutionFilter1D::kShiftBits);
    accum3 = emulate_lsx_packs_epi32(accum3, zero);
    accum3 = emulate_lsx_packus_epi16(accum3, zero);

    *(reinterpret_cast<int*>(out_row[0])) = __lsx_vpickve2gr_w(accum0, 0);
    *(reinterpret_cast<int*>(out_row[1])) = __lsx_vpickve2gr_w(accum1, 0);
    *(reinterpret_cast<int*>(out_row[2])) = __lsx_vpickve2gr_w(accum2, 0);
    *(reinterpret_cast<int*>(out_row[3])) = __lsx_vpickve2gr_w(accum3, 0);

    out_row[0] += 4;
    out_row[1] += 4;
    out_row[2] += 4;
    out_row[3] += 4;
  }
}

// Does vertical convolution to produce one output row. The filter values and
// length are given in the first two parameters. These are applied to each
// of the rows pointed to in the |source_data_rows| array, with each row
// being |pixel_width| wide.
//
// The output must have room for |pixel_width * 4| bytes.
template<bool has_alpha>
void ConvolveVertically_LSX(const ConvolutionFilter1D::Fixed* filter_values,
                            int filter_length,
                            unsigned char* const* source_data_rows,
                            int pixel_width,
                            unsigned char* out_row) {
  int width = pixel_width & ~3;

  __m128i zero = __lsx_vldi(0);
  __m128i accum0, accum1, accum2, accum3, coeff16;
  const __m128i* src;
  // Output four pixels per iteration (16 bytes).
  for (int out_x = 0; out_x < width; out_x += 4) {

    // Accumulated result for each pixel. 32 bits per RGBA channel.
    accum0 = __lsx_vldi(0);
    accum1 = __lsx_vldi(0);
    accum2 = __lsx_vldi(0);
    accum3 = __lsx_vldi(0);
    int values = 0;
    // Convolve with one filter coefficient per iteration.
    for (int filter_y = 0; filter_y < filter_length; filter_y++) {

      // Duplicate the filter coefficient 8 times.
      // [16] cj cj cj cj cj cj cj cj
      values = filter_values[filter_y];
      coeff16 = __lsx_vldrepl_h(&values, 0);

      // Load four pixels (16 bytes) together.
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      src = reinterpret_cast<const __m128i*>(
          &source_data_rows[filter_y][out_x << 2]);
      __m128i src8 = LSX_LD(src);

      // Unpack 1st and 2nd pixels from 8 bits to 16 bits for each channels =>
      // multiply with current coefficient => accumulate the result.
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = __lsx_vilvl_b(zero, src8);
      __m128i mul_hi = __lsx_vmuh_h(src16, coeff16);
      __m128i mul_lo = __lsx_vmul_h(src16, coeff16);
      // [32] a0 b0 g0 r0
      __m128i t = __lsx_vilvl_h(mul_hi, mul_lo);
      accum0 = __lsx_vadd_w(accum0, t);
      // [32] a1 b1 g1 r1
      t = __lsx_vilvh_h(mul_hi, mul_lo);
      accum1 = __lsx_vadd_w(accum1, t);

      // Unpack 3rd and 4th pixels from 8 bits to 16 bits for each channels =>
      // multiply with current coefficient => accumulate the result.
      // [16] a3 b3 g3 r3 a2 b2 g2 r2
      src16 = __lsx_vilvh_b(zero, src8);
      mul_hi = __lsx_vmuh_h(src16, coeff16);
      mul_lo = __lsx_vmul_h(src16, coeff16);
      // [32] a2 b2 g2 r2
      t = __lsx_vilvl_h(mul_hi, mul_lo);
      accum2 = __lsx_vadd_w(accum2, t);
      // [32] a3 b3 g3 r3
      t = __lsx_vilvh_h(mul_hi, mul_lo);
      accum3 = __lsx_vadd_w(accum3, t);
    }
    // Shift right for fixed point implementation.
    accum0 = emulate_lsx_srai_epi32(accum0, ConvolutionFilter1D::kShiftBits);
    accum1 = emulate_lsx_srai_epi32(accum1, ConvolutionFilter1D::kShiftBits);
    accum2 = emulate_lsx_srai_epi32(accum2, ConvolutionFilter1D::kShiftBits);
    accum3 = emulate_lsx_srai_epi32(accum3, ConvolutionFilter1D::kShiftBits);

    // Packing 32 bits |accum| to 16 bits per channel (signed saturation).
    // [16] a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = emulate_lsx_packs_epi32(accum0, accum1);
    // [16] a3 b3 g3 r3 a2 b2 g2 r2
    accum2 = emulate_lsx_packs_epi32(accum2, accum3);

    // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
    // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = emulate_lsx_packus_epi16(accum0, accum2);

    if (has_alpha) {
      // Compute the max(ri, gi, bi) for each pixel.
      // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
      __m128i a = __lsx_vsrli_w(accum0, 8);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      __m128i b = __lsx_vmax_bu(a, accum0);  // Max of r and g.
      // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
      a = __lsx_vsrli_w(accum0, 16);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      b = __lsx_vmax_bu(a, b);  // Max of r and g and b.
      // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
      b = __lsx_vslli_w(b, 24);

      // Make sure the value of alpha channel is always larger than maximum
      // value of color channels.
      accum0 = __lsx_vmax_bu(b, accum0);
    } else {
      // Set value of alpha channels to 0xFF.
      unsigned int a = 0xff000000;
      __m128i mask = __lsx_vldrepl_w(&a, 0);
      accum0 = __lsx_vor_v(accum0, mask);
    }

    // Store the convolution result (16 bytes) and advance the pixel pointers.
    LSX_ST(accum0, reinterpret_cast<__m128i*>(out_row));
    out_row += 16;
  }

  // When the width of the output is not divisible by 4, We need to save one
  // pixel (4 bytes) each time. And also the fourth pixel is always absent.
  if (pixel_width & 3) {
    accum0 = __lsx_vldi(0);
    accum1 = __lsx_vldi(0);
    accum2 = __lsx_vldi(0);
    int values = 0;
    for (int filter_y = 0; filter_y < filter_length; ++filter_y) {
      values = filter_values[filter_y];
      coeff16 = __lsx_vldrepl_h(&values, 0);
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      src = reinterpret_cast<const __m128i*>(
          &source_data_rows[filter_y][width<<2]);
      __m128i src8 = LSX_LD(src);
      // [16] a1 b1 g1 r1 a0 b0 g0 r0
      __m128i src16 = __lsx_vilvl_b(zero, src8);
      __m128i mul_hi = __lsx_vmuh_h(src16, coeff16);
      __m128i mul_lo = __lsx_vmul_h(src16, coeff16);
      // [32] a0 b0 g0 r0
      __m128i t = __lsx_vilvl_h(mul_hi, mul_lo);
      accum0 = __lsx_vadd_w(t, accum0);
      // [32] a1 b1 g1 r1
      t = __lsx_vilvh_h(mul_hi, mul_lo);
      accum1 = __lsx_vadd_w(accum1, t);
      // [16] a3 b3 g3 r3 a2 b2 g2 r2
      src16 = __lsx_vilvh_b(zero, src8);
      mul_hi = __lsx_vmuh_h(src16, coeff16);
      mul_lo = __lsx_vmul_h(src16, coeff16);
      // [32] a2 b2 g2 r2
      t = __lsx_vilvl_h(mul_hi, mul_lo);
      accum2 = __lsx_vadd_w(accum2, t);
    }

    accum0 = emulate_lsx_srai_epi32(accum0, ConvolutionFilter1D::kShiftBits);
    accum1 = emulate_lsx_srai_epi32(accum1, ConvolutionFilter1D::kShiftBits);
    accum2 = emulate_lsx_srai_epi32(accum2, ConvolutionFilter1D::kShiftBits);
    // [16] a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = emulate_lsx_packs_epi32(accum0, accum1);
    // [16] a3 b3 g3 r3 a2 b2 g2 r2
    accum2 = emulate_lsx_packs_epi32(accum2, zero);
    // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
    accum0 = emulate_lsx_packus_epi16(accum0, accum2);
    if (has_alpha) {
      // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
      __m128i a = __lsx_vsrli_w(accum0, 8);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      __m128i b = __lsx_vmax_bu(a, accum0);  // Max of r and g.
      // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
      a = __lsx_vsrli_w(accum0, 16);
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      b = __lsx_vmax_bu(a, b);  // Max of r and g and b.
      // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
      b = __lsx_vslli_w(b, 24);
      accum0 = __lsx_vmax_bu(b, accum0);
    } else {
      unsigned int a = 0xff000000;
      __m128i mask = __lsx_vldrepl_w(&a, 0);
      accum0 = __lsx_vor_v(accum0, mask);
    }

    for (int out_x = width; out_x < pixel_width; out_x++) {
      *(reinterpret_cast<int*>(out_row)) = __lsx_vpickve2gr_w(accum0, 0);
      accum0 = __lsx_vbsrl_v(accum0, 4);
      out_row += 4;
    }
  }
}

void ConvolveVertically_LSX(const ConvolutionFilter1D::Fixed* filter_values,
                            int filter_length,
                            unsigned char* const* source_data_rows,
                            int pixel_width,
                            unsigned char* out_row,
                            bool has_alpha) {
  if (has_alpha) {
    ConvolveVertically_LSX<true>(filter_values,
                                 filter_length,
                                 source_data_rows,
                                 pixel_width,
                                 out_row);
  } else {
    ConvolveVertically_LSX<false>(filter_values,
                                  filter_length,
                                  source_data_rows,
                                  pixel_width,
                                  out_row);
  }
}
}  // namespace skia
