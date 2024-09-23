// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/delay.h"

#include <xmmintrin.h>

namespace blink {

ALWAYS_INLINE static __m128i WrapIndexVector(__m128i v_write_index,
                                             __m128i v_buffer_length) {
  // Wrap the write_index if any index is past the end of the buffer.
  // This implements
  //
  //   if (write_index >= buffer_length)
  //     write_index -= buffer_length

  // There's no mm_cmpge_epi32, so we need to use mm_cmplt_epi32.  Thus, the
  // above becomes
  //
  //   if (!(write_index < buffer_length))
  //     write_index -= buffer_length

  // If write_index < buffer_length, set cmp = 0xffffffff.  Otherwise 0.
  __m128i cmp = _mm_cmplt_epi32(v_write_index, v_buffer_length);

  // Invert cmp and bitwise-and with buffer_length to get buffer_length or 0
  // depending on whether write_index >= buffer_length or not.  Subtract from
  // write_index to wrap it.
  return _mm_sub_epi32(v_write_index, _mm_andnot_si128(cmp, v_buffer_length));
}

ALWAYS_INLINE static __m128 WrapPositionVector(__m128 v_position,
                                               __m128 v_buffer_length) {
  // Wrap the read position if it exceed the buffer length.
  // This implements
  //
  //   if (position >= buffer_length)
  //     read_position -= buffer_length

  // If position >= buffer length, set cmp = 0xffffffff.  Otherwise 0.
  __m128 cmp = _mm_cmpge_ps(v_position, v_buffer_length);

  // Bitwise-and buffer_length with cmp to get buffer_length or 0 depending on
  // whether read_position >= buffer length or not.  Then subtract from the
  // position to wrap it.
  return _mm_sub_ps(v_position, _mm_and_ps(v_buffer_length, cmp));
}

std::tuple<unsigned, int> Delay::ProcessARateVector(
    float* destination,
    uint32_t frames_to_process) const {
  const int buffer_length = buffer_.size();
  const float* buffer = buffer_.Data();

  const float sample_rate = sample_rate_;
  const float* delay_times = delay_times_.Data();
  int w_index = write_index_;

  const __m128 v_sample_rate = _mm_set1_ps(sample_rate);
  const __m128 v_all_zeros = _mm_setzero_ps();

  // The buffer length as a float and as an int so we don't need to constant
  // convert from one to the other.
  const __m128 v_buffer_length_float = _mm_set1_ps(buffer_length);
  const __m128i v_buffer_length_int = _mm_set1_epi32(buffer_length);

  // How much to increment the write index each time through the loop.
  const __m128i v_incr = _mm_set1_epi32(4);

  // Temp arrays for storing the samples needed for interpolation
  float sample1[4] __attribute((aligned(16)));
  float sample2[4] __attribute((aligned(16)));

  // Initialize the write index vector, and  wrap the values if needed.
  __m128i v_write_index =
      _mm_set_epi32(w_index + 3, w_index + 2, w_index + 1, w_index + 0);
  v_write_index = WrapIndexVector(v_write_index, v_buffer_length_int);

  const int number_of_loops = frames_to_process / 4;
  int k = 0;

  for (int n = 0; n < number_of_loops; ++n, k += 4) {
    // It's possible that `delay_time` contains negative values. Make sure
    // they are greater than zero.
    const __m128 v_delay_time = _mm_max_ps(_mm_loadu_ps(delay_times + k),
                                           v_all_zeros);
    const __m128 v_desired_delay_frames =
        _mm_mul_ps(v_delay_time, v_sample_rate);

    // read_position = write_index + buffer_length - desired_delay_frames.  Wrap
    // the position if needed.
    __m128 v_read_position =
        _mm_add_ps(_mm_cvtepi32_ps(v_write_index),
                   _mm_sub_ps(v_buffer_length_float, v_desired_delay_frames));
    v_read_position =
        WrapPositionVector(v_read_position, v_buffer_length_float);

    // Get indices into the buffer for the samples we need for interpolation.
    const __m128i v_read_index1 = WrapIndexVector(
        _mm_cvttps_epi32(v_read_position), v_buffer_length_int);
    const __m128i v_read_index2 = WrapIndexVector(
        _mm_add_epi32(v_read_index1, _mm_set1_epi32(1)), v_buffer_length_int);

    const __m128 interpolation_factor =
        _mm_sub_ps(v_read_position, _mm_cvtepi32_ps(v_read_index1));

    const uint32_t* read_index1 =
        reinterpret_cast<const uint32_t*>(&v_read_index1);
    const uint32_t* read_index2 =
        reinterpret_cast<const uint32_t*>(&v_read_index2);

    for (int m = 0; m < 4; ++m) {
      sample1[m] = buffer[read_index1[m]];
      sample2[m] = buffer[read_index2[m]];
    }

    const __m128 v_sample1 = _mm_load_ps(sample1);
    const __m128 v_sample2 = _mm_load_ps(sample2);

    v_write_index = _mm_add_epi32(v_write_index, v_incr);
    v_write_index = WrapIndexVector(v_write_index, v_buffer_length_int);

    const __m128 sample = _mm_add_ps(
        v_sample1,
        _mm_mul_ps(interpolation_factor, _mm_sub_ps(v_sample2, v_sample1)));
    _mm_store_ps(destination + k, sample);
  }

  // Update |w_index|_ based on how many frames we processed here, wrapping
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
  const unsigned number_of_loops = frames_to_process / 4;

  __m128 v_max_time = _mm_set1_ps(max_time);

  // This is approximately 4 times faster than the scalar version.
  for (unsigned loop = 0; loop < number_of_loops; ++loop, k += 4) {
    __m128 x = _mm_loadu_ps(delay_times + k);
    // 0xffffffff if x is NaN. Otherwise 0
    __m128 cmp = _mm_cmpunord_ps(x, x);

    // Use cmp as a mask to set a component of x to 0 if is NaN.  Otherwise,
    // preserve x.
    x = _mm_andnot_ps(cmp, x);

    // Now set cmp to be max_time if the value is 0xffffffff or 0.
    cmp = _mm_and_ps(cmp, v_max_time);

    // Merge i (bitwise or) x and cmp.  This makes x = max_time if x was NaN and
    // preserves x if not.
    x = _mm_or_ps(x, cmp);
    _mm_storeu_ps(delay_times + k, x);
  }

  // Handle any frames not done in the loop above.
  for (; k < frames_to_process; ++k) {
    if (std::isnan(delay_times[k])) {
      delay_times[k] = max_time;
    }
  }
}

}  // namespace blink
