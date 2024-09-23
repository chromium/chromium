
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "skia/ext/convolver_neon.h"

#include <arm_neon.h>

namespace skia {

static SK_ALWAYS_INLINE int32x4_t
AccumRemainder(const unsigned char* pixels_left,
               const ConvolutionFilter1D::Fixed* filter_values,
               int r) {
  int remainder[4] = {0, 0, 0, 0};
  for (int i = 0; i < r; i++) {
    ConvolutionFilter1D::Fixed coeff = filter_values[i];
    remainder[0] += coeff * pixels_left[i * 4 + 0];
    remainder[1] += coeff * pixels_left[i * 4 + 1];
    remainder[2] += coeff * pixels_left[i * 4 + 2];
    remainder[3] += coeff * pixels_left[i * 4 + 3];
  }
  return vld1q_s32(remainder);
}

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the num_values() of the filter.
void ConvolveHorizontally_Neon(const unsigned char* src_data,
                               const ConvolutionFilter1D& filter,
                               unsigned char* out_row,
                               bool /*has_alpha*/) {
  // Loop over each pixel on this row in the output image.
  int num_values = filter.num_values();
  for (int out_x = 0; out_x < num_values; out_x++) {
    // Get the filter that determines the current output pixel.
    int filter_offset, filter_length;
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    // Compute the first pixel in this row that the filter affects. It will
    // touch |filter_length| pixels (4 bytes each) after this.
    const unsigned char* row_to_filter = &src_data[filter_offset * 4];

    // Apply the filter to the row to get the destination pixel in |accum|.
    int32x4_t accum = vdupq_n_s32(0);
    for (int filter_x = 0; filter_x < (filter_length / 4); filter_x++) {
      // Load 4 coefficients.
      int16x4_t coeffs = vld1_s16(filter_values);
      // Load 4 pixels into a q-register.
      uint8x16_t pixels = vld1q_u8(row_to_filter);

      // Expand to 16-bit channels split across two q-registers.
      int16x8_t p01_16 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixels)));
      int16x8_t p23_16 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(pixels)));

      // Scale each pixel (each d-register) by its filter coefficients,
      // accumulating into 32-bit.
      accum = vmlal_lane_s16(accum, vget_low_s16(p01_16), coeffs, 0);
      accum = vmlal_lane_s16(accum, vget_high_s16(p01_16), coeffs, 1);
      accum = vmlal_lane_s16(accum, vget_low_s16(p23_16), coeffs, 2);
      accum = vmlal_lane_s16(accum, vget_high_s16(p23_16), coeffs, 3);

      // Advance to next elements.
      row_to_filter += 16;
      filter_values += 4;
    }

    int remainder = filter_length & 3;
    if (remainder) {
      int remainder_offset = (filter_offset + filter_length - remainder) * 4;
      accum +=
          AccumRemainder(src_data + remainder_offset, filter_values, remainder);
    }

    // Bring this value back in range. All of the filter scaling factors
    // are in fixed point with kShiftBits bits of fractional part.
    int16x4_t accum16 = vqshrn_n_s32(accum, ConvolutionFilter1D::kShiftBits);

    // Pack and store the new pixel.
    uint8x8_t accum8 = vqmovun_s16(vcombine_s16(accum16, accum16));
    vst1_lane_u32(reinterpret_cast<uint32_t*>(out_row),
                  vreinterpret_u32_u8(accum8), 0);
    out_row += 4;
  }
}

// Convolves horizontally along four rows. The row data is given in
// |src_data| and continues for the num_values() of the filter.
// The algorithm is almost same as |convolve_horizontally|. Please
// refer to that function for detailed comments.
void Convolve4RowsHorizontally_Neon(const unsigned char* src_data[4],
                                    const ConvolutionFilter1D& filter,
                                    unsigned char* out_row[4]) {
  // Output one pixel each iteration, calculating all channels (RGBA) together.
  int num_values = filter.num_values();
  for (int out_x = 0; out_x < num_values; out_x++) {
    int filter_offset, filter_length;
    const ConvolutionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    // Four pixels in a column per iteration.
    int32x4_t accum0 = vdupq_n_s32(0);
    int32x4_t accum1 = vdupq_n_s32(0);
    int32x4_t accum2 = vdupq_n_s32(0);
    int32x4_t accum3 = vdupq_n_s32(0);

    int start = filter_offset * 4;

    // Load and accumulate with four coefficients per iteration.
    for (int filter_x = 0; filter_x < (filter_length / 4); filter_x++) {
      // Load 4 coefficients.
      int16x4_t coeffs = vld1_s16(filter_values);

      auto iteration = [=](const uint8_t* src) {
        // c.f. ConvolveHorizontally_Neon() above.
        uint8x16_t pixels = vld1q_u8(src);
        int16x8_t p01_16 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pixels)));
        int16x8_t p23_16 =
            vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(pixels)));
        int32x4_t accum = vdupq_n_s32(0);
        accum = vmlal_lane_s16(accum, vget_low_s16(p01_16), coeffs, 0);
        accum = vmlal_lane_s16(accum, vget_high_s16(p01_16), coeffs, 1);
        accum = vmlal_lane_s16(accum, vget_low_s16(p23_16), coeffs, 2);
        accum = vmlal_lane_s16(accum, vget_high_s16(p23_16), coeffs, 3);
        return accum;
      };

      accum0 += iteration(src_data[0] + start);
      accum1 += iteration(src_data[1] + start);
      accum2 += iteration(src_data[2] + start);
      accum3 += iteration(src_data[3] + start);

      start += 16;
      filter_values += 4;
    }

    int remainder = filter_length & 3;
    if (remainder) {
      int remainder_offset = (filter_offset + filter_length - remainder) * 4;
      accum0 += AccumRemainder(src_data[0] + remainder_offset, filter_values,
                               remainder);
      accum1 += AccumRemainder(src_data[1] + remainder_offset, filter_values,
                               remainder);
      accum2 += AccumRemainder(src_data[2] + remainder_offset, filter_values,
                               remainder);
      accum3 += AccumRemainder(src_data[3] + remainder_offset, filter_values,
                               remainder);
    }

    auto pack_result = [](int32x4_t accum) {
      int16x4_t accum16 = vqshrn_n_s32(accum, ConvolutionFilter1D::kShiftBits);
      return vqmovun_s16(vcombine_s16(accum16, accum16));
    };

    uint8x8_t res0 = pack_result(accum0);
    uint8x8_t res1 = pack_result(accum1);
    uint8x8_t res2 = pack_result(accum2);
    uint8x8_t res3 = pack_result(accum3);

    vst1_lane_u32(reinterpret_cast<uint32_t*>(out_row[0]),
                  vreinterpret_u32_u8(res0), 0);
    vst1_lane_u32(reinterpret_cast<uint32_t*>(out_row[1]),
                  vreinterpret_u32_u8(res1), 0);
    vst1_lane_u32(reinterpret_cast<uint32_t*>(out_row[2]),
                  vreinterpret_u32_u8(res2), 0);
    vst1_lane_u32(reinterpret_cast<uint32_t*>(out_row[3]),
                  vreinterpret_u32_u8(res3), 0);
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
void ConvolveVertically_Neon(const ConvolutionFilter1D::Fixed* filter_values,
                             int filter_length,
                             unsigned char* const* source_data_rows,
                             int pixel_width,
                             unsigned char* out_row,
                             bool has_alpha) {
  int width = pixel_width & ~3;

  // Output four pixels per iteration (16 bytes).
  for (int out_x = 0; out_x < width; out_x += 4) {
    // Accumulated result for each pixel. 32 bits per RGBA channel.
    int32x4_t accum0 = vdupq_n_s32(0);
    int32x4_t accum1 = vdupq_n_s32(0);
    int32x4_t accum2 = vdupq_n_s32(0);
    int32x4_t accum3 = vdupq_n_s32(0);

    // Convolve with one filter coefficient per iteration.
    for (int filter_y = 0; filter_y < filter_length; filter_y++) {
      // Load four pixels (16 bytes) together.
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      uint8x16_t src8 = vld1q_u8(&source_data_rows[filter_y][out_x << 2]);

      int16x8_t src16_01 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(src8)));
      int16x8_t src16_23 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(src8)));

      accum0 =
          vmlal_n_s16(accum0, vget_low_s16(src16_01), filter_values[filter_y]);
      accum1 =
          vmlal_n_s16(accum1, vget_high_s16(src16_01), filter_values[filter_y]);
      accum2 =
          vmlal_n_s16(accum2, vget_low_s16(src16_23), filter_values[filter_y]);
      accum3 =
          vmlal_n_s16(accum3, vget_high_s16(src16_23), filter_values[filter_y]);
    }

    // Shift right for fixed point implementation.
    // Packing 32 bits |accum| to 16 bits per channel (unsigned saturation).
    int16x4_t accum16_0 = vqshrn_n_s32(accum0, ConvolutionFilter1D::kShiftBits);
    int16x4_t accum16_1 = vqshrn_n_s32(accum1, ConvolutionFilter1D::kShiftBits);
    int16x4_t accum16_2 = vqshrn_n_s32(accum2, ConvolutionFilter1D::kShiftBits);
    int16x4_t accum16_3 = vqshrn_n_s32(accum3, ConvolutionFilter1D::kShiftBits);

    // [16] a1 b1 g1 r1 a0 b0 g0 r0
    int16x8_t accum16_low = vcombine_s16(accum16_0, accum16_1);
    // [16] a3 b3 g3 r3 a2 b2 g2 r2
    int16x8_t accum16_high = vcombine_s16(accum16_2, accum16_3);

    // Packing 16 bits |accum| to 8 bits per channel (unsigned saturation).
    // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
    uint8x16_t accum8 =
        vcombine_u8(vqmovun_s16(accum16_low), vqmovun_s16(accum16_high));

    if (has_alpha) {
      // Compute the max(ri, gi, bi) for each pixel.
      // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
      uint8x16_t a =
          vreinterpretq_u8_u32(vshrq_n_u32(vreinterpretq_u32_u8(accum8), 8));
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      uint8x16_t b = vmaxq_u8(a, accum8);  // Max of r and g
      // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
      a = vreinterpretq_u8_u32(vshrq_n_u32(vreinterpretq_u32_u8(accum8), 16));
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      b = vmaxq_u8(a, b);  // Max of r and g and b.
      // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
      b = vreinterpretq_u8_u32(vshlq_n_u32(vreinterpretq_u32_u8(b), 24));

      // Make sure the value of alpha channel is always larger than maximum
      // value of color channels.
      accum8 = vmaxq_u8(b, accum8);
    } else {
      // Set value of alpha channels to 0xFF.
      accum8 = vreinterpretq_u8_u32(vreinterpretq_u32_u8(accum8) |
                                    vdupq_n_u32(0xFF000000));
    }

    // Store the convolution result (16 bytes) and advance the pixel pointers.
    vst1q_u8(out_row, accum8);
    out_row += 16;
  }

  // Process the leftovers when the width of the output is not divisible
  // by 4, that is at most 3 pixels.
  int remainder = pixel_width & 3;
  if (remainder) {
    int32x4_t accum0 = vdupq_n_s32(0);
    int32x4_t accum1 = vdupq_n_s32(0);
    int32x4_t accum2 = vdupq_n_s32(0);

    for (int filter_y = 0; filter_y < filter_length; ++filter_y) {
      // [8] a3 b3 g3 r3 a2 b2 g2 r2 a1 b1 g1 r1 a0 b0 g0 r0
      uint8x16_t src8 = vld1q_u8(&source_data_rows[filter_y][width * 4]);

      int16x8_t src16_01 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(src8)));
      int16x8_t src16_23 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(src8)));

      accum0 =
          vmlal_n_s16(accum0, vget_low_s16(src16_01), filter_values[filter_y]);
      accum1 =
          vmlal_n_s16(accum1, vget_high_s16(src16_01), filter_values[filter_y]);
      accum2 =
          vmlal_n_s16(accum2, vget_low_s16(src16_23), filter_values[filter_y]);
    }

    int16x4_t accum16_0 = vqshrn_n_s32(accum0, ConvolutionFilter1D::kShiftBits);
    int16x4_t accum16_1 = vqshrn_n_s32(accum1, ConvolutionFilter1D::kShiftBits);
    int16x4_t accum16_2 = vqshrn_n_s32(accum2, ConvolutionFilter1D::kShiftBits);

    int16x8_t accum16_low = vcombine_s16(accum16_0, accum16_1);
    int16x8_t accum16_high = vcombine_s16(accum16_2, accum16_2);

    uint8x16_t accum8 =
        vcombine_u8(vqmovun_s16(accum16_low), vqmovun_s16(accum16_high));

    if (has_alpha) {
      // Compute the max(ri, gi, bi) for each pixel.
      // [8] xx a3 b3 g3 xx a2 b2 g2 xx a1 b1 g1 xx a0 b0 g0
      uint8x16_t a =
          vreinterpretq_u8_u32(vshrq_n_u32(vreinterpretq_u32_u8(accum8), 8));
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      uint8x16_t b = vmaxq_u8(a, accum8);  // Max of r and g
      // [8] xx xx a3 b3 xx xx a2 b2 xx xx a1 b1 xx xx a0 b0
      a = vreinterpretq_u8_u32(vshrq_n_u32(vreinterpretq_u32_u8(accum8), 16));
      // [8] xx xx xx max3 xx xx xx max2 xx xx xx max1 xx xx xx max0
      b = vmaxq_u8(a, b);  // Max of r and g and b.
      // [8] max3 00 00 00 max2 00 00 00 max1 00 00 00 max0 00 00 00
      b = vreinterpretq_u8_u32(vshlq_n_u32(vreinterpretq_u32_u8(b), 24));

      // Make sure the value of alpha channel is always larger than maximum
      // value of color channels.
      accum8 = vmaxq_u8(b, accum8);
    } else {
      // Set value of alpha channels to 0xFF.
      accum8 = vreinterpretq_u8_u32(vreinterpretq_u32_u8(accum8) |
                                    vdupq_n_u32(0xFF000000));
    }

    switch (remainder) {
      case 1:
        vst1q_lane_u32(reinterpret_cast<uint32_t*>(out_row),
                       vreinterpretq_u32_u8(accum8), 0);
        break;
      case 2:
        vst1_u32(reinterpret_cast<uint32_t*>(out_row),
                 vreinterpret_u32_u8(vget_low_u8(accum8)));
        break;
      case 3:
        vst1_u32(reinterpret_cast<uint32_t*>(out_row),
                 vreinterpret_u32_u8(vget_low_u8(accum8)));
        vst1q_lane_u32(reinterpret_cast<uint32_t*>(out_row + 8),
                       vreinterpretq_u32_u8(accum8), 2);
        break;
    }
  }
}

}  // namespace skia
