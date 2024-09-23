// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "build/build_config.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_handler.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"

#if defined(CPU_ARM_NEON)
#include <arm_neon.h>
#endif

namespace blink {

#if defined(CPU_ARM_NEON)
namespace {

float32x4_t WrapVirtualIndexVector(float32x4_t x,
                                   float32x4_t wave_size,
                                   float32x4_t inv_wave_size) {
  // r = x/wave_size, f = truncate(r), truncating towards 0
  const float32x4_t r = vmulq_f32(x, inv_wave_size);
  int32x4_t f = vcvtq_s32_f32(r);

  // vcltq_f32 returns returns all 0xfffffff (-1) if a < b and if if not.
  const uint32x4_t cmp = vcltq_f32(r, vcvtq_f32_s32(f));
  f = vaddq_s32(f, vreinterpretq_s32_u32(cmp));

  return vsubq_f32(x, vmulq_f32(vcvtq_f32_s32(f), wave_size));
}

ALWAYS_INLINE double WrapVirtualIndex(double virtual_index,
                                      unsigned periodic_wave_size,
                                      double inv_periodic_wave_size) {
  return virtual_index -
         floor(virtual_index * inv_periodic_wave_size) * periodic_wave_size;
}

}  // namespace

std::tuple<int, double> OscillatorHandler::ProcessKRateVector(
    int n,
    float* dest_p,
    double virtual_read_index,
    float frequency,
    float rate_scale) const {
  const unsigned periodic_wave_size = periodic_wave_->PeriodicWaveSize();
  const double inv_periodic_wave_size = 1.0 / periodic_wave_size;

  float* higher_wave_data = nullptr;
  float* lower_wave_data = nullptr;
  float table_interpolation_factor = 0;
  const float incr = frequency * rate_scale;
  DCHECK_GE(incr, kInterpolate2Point);

  periodic_wave_->WaveDataForFundamentalFrequency(
      frequency, lower_wave_data, higher_wave_data, table_interpolation_factor);

  const float32x4_t v_wave_size = vdupq_n_f32(periodic_wave_size);
  const float32x4_t v_inv_wave_size = vdupq_n_f32(1.0f / periodic_wave_size);

  const uint32x4_t v_read_mask = vdupq_n_u32(periodic_wave_size - 1);
  const uint32x4_t v_one = vdupq_n_u32(1);

  const float32x4_t v_table_factor = vdupq_n_f32(table_interpolation_factor);

  const float32x4_t v_incr = vdupq_n_f32(4 * incr);

  float virtual_read_index_flt = virtual_read_index;
  float32x4_t v_virt_index = {
      virtual_read_index_flt + 0 * incr, virtual_read_index_flt + 1 * incr,
      virtual_read_index_flt + 2 * incr, virtual_read_index_flt + 3 * incr};

  // Temporary arrsys to hold the read indices so we can access them
  // individually to get the samples needed for interpolation.
  uint32_t r0[4] __attribute__((aligned(16)));
  uint32_t r1[4] __attribute__((aligned(16)));

  // Temporary arrays where we can gather up the wave data we need for
  // interpolation.  Align these for best efficiency on older CPUs where aligned
  // access is much faster than unaliged.  TODO(rtoy): Is there a faster way to
  // do this?
  float sample1_lower[4] __attribute__((aligned(16)));
  float sample2_lower[4] __attribute__((aligned(16)));
  float sample1_higher[4] __attribute__((aligned(16)));
  float sample2_higher[4] __attribute__((aligned(16)));

  // It's possible that adding the incr above exceeded the bounds, so wrap them
  // if needed.
  v_virt_index =
      WrapVirtualIndexVector(v_virt_index, v_wave_size, v_inv_wave_size);

  int k = 0;
  int n_loops = n / 4;

  for (int loop = 0; loop < n_loops; ++loop, k += 4) {
    // Compute indices for the samples and contain within the valid range.
    const uint32x4_t read_index_0 =
        vandq_u32(vcvtq_u32_f32(v_virt_index), v_read_mask);
    const uint32x4_t read_index_1 =
        vandq_u32(vaddq_u32(read_index_0, v_one), v_read_mask);

    // Extract the components of the indices so we can get the samples
    // associated with the lower and higher wave data.
    vst1q_u32(r0, read_index_0);
    vst1q_u32(r1, read_index_1);

    for (int m = 0; m < 4; ++m) {
      sample1_lower[m] = lower_wave_data[r0[m]];
      sample2_lower[m] = lower_wave_data[r1[m]];
      sample1_higher[m] = higher_wave_data[r0[m]];
      sample2_higher[m] = higher_wave_data[r1[m]];
    }

    const float32x4_t s1_low = vld1q_f32(sample1_lower);
    const float32x4_t s2_low = vld1q_f32(sample2_lower);
    const float32x4_t s1_high = vld1q_f32(sample1_higher);
    const float32x4_t s2_high = vld1q_f32(sample2_higher);

    const float32x4_t interpolation_factor =
        vsubq_f32(v_virt_index, vcvtq_f32_u32(read_index_0));
    const float32x4_t sample_higher = vaddq_f32(
        s1_high, vmulq_f32(interpolation_factor, vsubq_f32(s2_high, s1_high)));
    const float32x4_t sample_lower = vaddq_f32(
        s1_low, vmulq_f32(interpolation_factor, vsubq_f32(s2_low, s1_low)));
    const float32x4_t sample = vaddq_f32(
        sample_higher,
        vmulq_f32(v_table_factor, vsubq_f32(sample_lower, sample_higher)));

    vst1q_f32(dest_p + k, sample);

    // Increment virtual read index and wrap virtualReadIndex into the range
    // 0 -> periodicWaveSize.
    v_virt_index = vaddq_f32(v_virt_index, v_incr);
    v_virt_index =
        WrapVirtualIndexVector(v_virt_index, v_wave_size, v_inv_wave_size);
  }

  // There's a bit of round-off above, so update the index more accurately so at
  // least the next render starts over with a more accurate value.
  virtual_read_index += k * incr;
  virtual_read_index -=
      std::floor(virtual_read_index * inv_periodic_wave_size) *
      periodic_wave_size;

  return std::make_tuple(k, virtual_read_index);
}

double OscillatorHandler::ProcessARateVectorKernel(
    float* destination,
    double virtual_read_index,
    const float* phase_increments,
    unsigned periodic_wave_size,
    const float* const lower_wave_data[4],
    const float* const higher_wave_data[4],
    const float table_interpolation_factor[4]) const {
  // See the scalar version in oscillator_node.cc for the basic algorithm.
  double inv_periodic_wave_size = 1.0 / periodic_wave_size;
  unsigned read_index_mask = periodic_wave_size - 1;

  // Accumulate the phase increments so we can set up the virtual read index
  // vector appropriately.  This must be a double to preserve accuracy and
  // to match the scalar version.
  double incr_sum[4];
  incr_sum[0] = phase_increments[0];
  for (int m = 1; m < 4; ++m) {
    incr_sum[m] = incr_sum[m - 1] + phase_increments[m];
  }

  // It's really important for accuracy that we use doubles instead of
  // floats for the virtual_read_index.  Without this, we can only get some
  // 30-50 dB in the sweep tests instead of 100+ dB.
  //
  // Arm NEON doesn't have float64x2_t so we have to do this.  (Aarch64 has
  // float64x2_t.)
  double virt_index[4];
  virt_index[0] = virtual_read_index;
  virt_index[1] = WrapVirtualIndex(virtual_read_index + incr_sum[0],
                                   periodic_wave_size, inv_periodic_wave_size);
  virt_index[2] = WrapVirtualIndex(virtual_read_index + incr_sum[1],
                                   periodic_wave_size, inv_periodic_wave_size);
  virt_index[3] = WrapVirtualIndex(virtual_read_index + incr_sum[2],
                                   periodic_wave_size, inv_periodic_wave_size);

  // The virtual indices we're working with now.
  const float32x4_t v_virt_index = {
      static_cast<float>(virt_index[0]), static_cast<float>(virt_index[1]),
      static_cast<float>(virt_index[2]), static_cast<float>(virt_index[3])};

  // Convert virtual index to actual index into wave data, wrap the index
  // around if needed.
  const uint32x4_t v_read0 =
      vandq_u32(vcvtq_u32_f32(v_virt_index), vdupq_n_u32(read_index_mask));

  // v_read1 = v_read0 + 1, but wrap the index around, if needed.
  const uint32x4_t v_read1 = vandq_u32(vaddq_u32(v_read0, vdupq_n_u32(1)),
                                       vdupq_n_u32(read_index_mask));

  float sample1_lower[4] __attribute__((aligned(16)));
  float sample2_lower[4] __attribute__((aligned(16)));
  float sample1_higher[4] __attribute__((aligned(16)));
  float sample2_higher[4] __attribute__((aligned(16)));

  uint32_t read0[4] __attribute__((aligned(16)));
  uint32_t read1[4] __attribute__((aligned(16)));

  vst1q_u32(read0, v_read0);
  vst1q_u32(read1, v_read1);

  // Read the samples from the wave tables
  for (int m = 0; m < 4; ++m) {
    DCHECK_LT(read0[m], periodic_wave_size);
    DCHECK_LT(read1[m], periodic_wave_size);

    sample1_lower[m] = lower_wave_data[m][read0[m]];
    sample2_lower[m] = lower_wave_data[m][read1[m]];
    sample1_higher[m] = higher_wave_data[m][read0[m]];
    sample2_higher[m] = higher_wave_data[m][read1[m]];
  }

  // Compute factor for linear interpolation within a wave table.
  const float32x4_t v_factor = vsubq_f32(v_virt_index, vcvtq_f32_u32(v_read0));

  // Linearly interpolate between samples from the higher wave table.
  const float32x4_t sample_higher = vmlaq_f32(
      vld1q_f32(sample1_higher), v_factor,
      vsubq_f32(vld1q_f32(sample2_higher), vld1q_f32(sample1_higher)));

  // Linearly interpolate between samples from the lower wave table.
  const float32x4_t sample_lower =
      vmlaq_f32(vld1q_f32(sample1_lower), v_factor,
                vsubq_f32(vld1q_f32(sample2_lower), vld1q_f32(sample1_lower)));

  // Linearly interpolate between wave tables to get the desired
  // output samples.
  const float32x4_t sample =
      vmlaq_f32(sample_higher, vld1q_f32(table_interpolation_factor),
                vsubq_f32(sample_lower, sample_higher));

  vst1q_f32(destination, sample);

  // Update the virtual_read_index appropriately and return it for the
  // next call.
  virtual_read_index =
      WrapVirtualIndex(virtual_read_index + incr_sum[3], periodic_wave_size,
                       inv_periodic_wave_size);

  return virtual_read_index;
}
#endif

}  // namespace blink
