// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <arm_neon.h>

#include <algorithm>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/delay.h"

namespace blink {

#if defined(CPU_ARM_NEON)
ALWAYS_INLINE static int32x4_t WrapIndexVector(int32x4_t v_write_index,
                                               int32x4_t v_buffer_length) {
  // Wrap the write_index if any index is past the end of the buffer.
  // This implements
  //
  //   if (write_index >= buffer_length)
  //     write_index -= buffer_length

  // If write_index >= buffer_length, cmp = 0xffffffff.  Otherwise 0.
  int32x4_t cmp =
      reinterpret_cast<int32x4_t>(vcgeq_s32(v_write_index, v_buffer_length));

  // Bitwise-and cmp with buffer length to get buffer length or 0 depending on
  // whether write_index >= buffer_length or not.  Subtract this from the index
  // to wrap the index appropriately.
  return vsubq_s32(v_write_index, vandq_s32(cmp, v_buffer_length));
}

ALWAYS_INLINE static float32x4_t WrapPositionVector(
    float32x4_t v_position,
    float32x4_t v_buffer_length) {
  // Wrap the read position if it exceed the buffer length.
  // This implements
  //
  //   if (position >= buffer_length)
  //     read_position -= buffer_length

  // If position >= buffer length, set cmp = 0xffffffff.  Otherwise 0.
  uint32x4_t cmp = vcgeq_f32(v_position, v_buffer_length);

  // Bitwise-and buffer_length with cmp to get buffer_length or 0 depending on
  // whether read_position >= buffer length or not.  Then subtract from the
  // position to wrap it around if needed.
  return vsubq_f32(v_position,
                   reinterpret_cast<float32x4_t>(vandq_u32(
                       reinterpret_cast<uint32x4_t>(v_buffer_length), cmp)));
}

std::tuple<unsigned, int> Delay::ProcessARateVector(
    float* destination,
    uint32_t frames_to_process) const {
  const int buffer_length = buffer_.size();
  const float* buffer = buffer_.Data();

  const float sample_rate = sample_rate_;
  const float* delay_times = delay_times_.Data();

  int w_index = write_index_;

  const float32x4_t v_sample_rate = vdupq_n_f32(sample_rate);
  const float32x4_t v_all_zeros = vdupq_n_f32(0);

  // The buffer length as a float and as an int so we don't need to constant
  // convert from one to the other.
  const float32x4_t v_buffer_length_float = vdupq_n_f32(buffer_length);
  const int32x4_t v_buffer_length_int = vdupq_n_s32(buffer_length);

  // How much to increment the write index each time through the loop.
  const int32x4_t v_incr = vdupq_n_s32(4);

  // Temp arrays for storing the samples needed for interpolation
  float sample1[4] __attribute((aligned(16)));
  float sample2[4] __attribute((aligned(16)));

  // Temp array for holding the indices so we can access them
  // individually.
  int read_index1[4] __attribute((aligned(16)));
  int read_index2[4] __attribute((aligned(16)));

  // Initialize the write index vector, and  wrap the values if needed.
  int32x4_t v_write_index = {w_index + 0, w_index + 1, w_index + 2,
                             w_index + 3};
  v_write_index = WrapIndexVector(v_write_index, v_buffer_length_int);

  int number_of_loops = frames_to_process / 4;
  int k = 0;

  for (int n = 0; n < number_of_loops; ++n, k += 4) {
    const float32x4_t v_delay_time = vmaxq_f32(vld1q_f32(delay_times + k),
                                               v_all_zeros);
    const float32x4_t v_desired_delay_frames =
        vmulq_f32(v_delay_time, v_sample_rate);

    // read_position = write_index + buffer_length - desired_delay_frames.  Wrap
    // the position if needed.
    float32x4_t v_read_position =
        vaddq_f32(vcvtq_f32_s32(v_write_index),
                  vsubq_f32(v_buffer_length_float, v_desired_delay_frames));
    v_read_position =
        WrapPositionVector(v_read_position, v_buffer_length_float);

    // Get indices into the buffer for the samples we need for interpolation.
    const int32x4_t v_read_index1 = WrapIndexVector(
        vcvtq_s32_f32(v_read_position), v_buffer_length_int);
    const int32x4_t v_read_index2 = WrapIndexVector(
        vaddq_s32(v_read_index1, vdupq_n_s32(1)), v_buffer_length_int);

    const float32x4_t interpolation_factor =
        vsubq_f32(v_read_position, vcvtq_f32_s32(v_read_index1));

    // Save indices so we can access the components individually for
    // getting the aamples from the buffer.
    vst1q_s32(read_index1, v_read_index1);
    vst1q_s32(read_index2, v_read_index2);

    for (int m = 0; m < 4; ++m) {
      sample1[m] = buffer[read_index1[m]];
      sample2[m] = buffer[read_index2[m]];
    }

    const float32x4_t v_sample1 = vld1q_f32(sample1);
    const float32x4_t v_sample2 = vld1q_f32(sample2);

    v_write_index = vaddq_s32(v_write_index, v_incr);
    v_write_index = WrapIndexVector(v_write_index, v_buffer_length_int);

    // Linear interpolation between samples.
    const float32x4_t sample = vaddq_f32(
        v_sample1,
        vmulq_f32(interpolation_factor, vsubq_f32(v_sample2, v_sample1)));
    vst1q_f32(destination + k, sample);
  }

  // Update |w_index| based on how many frames we processed here, wrapping
  // around if needed.
  w_index = write_index_ + k;
  if (w_index >= buffer_length) {
    w_index -= buffer_length;
  }

  return std::make_tuple(k, w_index);
}

void Delay::HandleNaN(float* delay_times,
                      uint32_t frames_to_process,
                      float max_time) {
  unsigned k = 0;
  int number_of_loops = frames_to_process / 4;

  float32x4_t v_max_time = vdupq_n_f32(max_time);

  // This is approximately 4 times faster than the scalar version.
  for (int loop = 0; loop < number_of_loops; ++loop, k += 4) {
    float32x4_t x = vld1q_f32(delay_times + k);
    // x == x only fails when x is NaN.  Then cmp is set to 0. Otherwise
    // 0xffffffff
    uint32x4_t cmp = vceqq_f32(x, x);

    // Use cmp as a mask to set a component of x to 0 if x is NaN.
    // Otherwise, preserve x.  We pun the types here so we can apply
    // the  mask to the floating point numbers.  A integer value of
    // 0 corresponds to a floating-point +0.0, which is what we want.
    uint32x4_t xint = vandq_u32(cmp, reinterpret_cast<uint32x4_t>(x));

    // Invert the mask.
    cmp = vmvnq_u32(cmp);

    // More punning of the types so we can apply the complement mask
    // to set cmp to either max_time (if NaN) or 0 (otherwise)
    cmp = vandq_u32(cmp, reinterpret_cast<uint32x4_t>(v_max_time));

    // Merge i (bitwise or) x and cmp.  This makes x = max_time if x was NaN and
    // preserves x if not.  More type punning to do bitwise or the results
    // together.
    xint = vorrq_u32(xint, cmp);

    // Finally, save the float result.
    vst1q_f32(delay_times + k, reinterpret_cast<float32x4_t>(xint));
  }

  // Handle any frames not done in the loop above.
  for (; k < frames_to_process; ++k) {
    if (std::isnan(delay_times[k])) {
      delay_times[k] = max_time;
    }
  }
}
#endif

}  // namespace blink
