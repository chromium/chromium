/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/wave_shaper_dsp_kernel.h"

#include <algorithm>
#include <memory>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <xmmintrin.h>
#elif defined(CPU_ARM_NEON)
#include <arm_neon.h>
#endif

namespace blink {

WaveShaperDSPKernel::WaveShaperDSPKernel(WaveShaperProcessor* processor)
    : AudioDSPKernel(processor),
      // 4 times render size to handle 4x oversampling.
      virtual_index_(4 * RenderQuantumFrames()),
      index_(4 * RenderQuantumFrames()),
      v1_(4 * RenderQuantumFrames()),
      v2_(4 * RenderQuantumFrames()),
      f_(4 * RenderQuantumFrames()) {
  if (processor->Oversample() != WaveShaperProcessor::kOverSampleNone) {
    LazyInitializeOversampling();
  }
}

void WaveShaperDSPKernel::LazyInitializeOversampling() {
  if (!temp_buffer_) {
    temp_buffer_ = std::make_unique<AudioFloatArray>(RenderQuantumFrames() * 2);
    temp_buffer2_ =
        std::make_unique<AudioFloatArray>(RenderQuantumFrames() * 4);
    up_sampler_ = std::make_unique<UpSampler>(RenderQuantumFrames());
    down_sampler_ = std::make_unique<DownSampler>(RenderQuantumFrames() * 2);
    up_sampler2_ = std::make_unique<UpSampler>(RenderQuantumFrames() * 2);
    down_sampler2_ = std::make_unique<DownSampler>(RenderQuantumFrames() * 4);
  }
}

void WaveShaperDSPKernel::Process(const float* source,
                                  float* destination,
                                  uint32_t frames_to_process) {
  switch (GetWaveShaperProcessor()->Oversample()) {
    case WaveShaperProcessor::kOverSampleNone:
      ProcessCurve(source, destination, frames_to_process);
      break;
    case WaveShaperProcessor::kOverSample2x:
      ProcessCurve2x(source, destination, frames_to_process);
      break;
    case WaveShaperProcessor::kOverSample4x:
      ProcessCurve4x(source, destination, frames_to_process);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
}

double WaveShaperDSPKernel::WaveShaperCurveValue(float input,
                                                 const float* curve_data,
                                                 int curve_length) const {
  // Calculate a virtual index based on input -1 -> +1 with -1 being curve[0],
  // +1 being curve[curveLength - 1], and 0 being at the center of the curve
  // data. Then linearly interpolate between the two points in the curve.
  double virtual_index = 0.5 * (input + 1) * (curve_length - 1);
  double output;
  if (virtual_index < 0) {
    // input < -1, so use curve[0]
    output = curve_data[0];
  } else if (virtual_index >= curve_length - 1) {
    // input >= 1, so use last curve value
    output = curve_data[curve_length - 1];
  } else {
    // The general case where -1 <= input < 1, where 0 <= virtualIndex <
    // curveLength - 1, so interpolate between the nearest samples on the
    // curve.
    unsigned index1 = static_cast<unsigned>(virtual_index);
    unsigned index2 = index1 + 1;
    double interpolation_factor = virtual_index - index1;

    double value1 = curve_data[index1];
    double value2 = curve_data[index2];

    output =
        (1.0 - interpolation_factor) * value1 + interpolation_factor * value2;
  }

  return output;
}

void WaveShaperDSPKernel::WaveShaperCurveValues(float* destination,
                                                const float* source,
                                                uint32_t frames_to_process,
                                                const float* curve_data,
                                                int curve_length) const {
  DCHECK_LE(frames_to_process, virtual_index_.size());
  // Index into the array computed from the source value.
  float* virtual_index = virtual_index_.Data();

  // virtual_index[k] =
  //   ClampTo(0.5 * (source[k] + 1) * (curve_length - 1),
  //           0.0f,
  //           static_cast<float>(curve_length - 1))

  // Add 1 to source puttting  result in virtual_index
  vector_math::Vsadd(source, 1, 1, virtual_index, 1, frames_to_process);

  // Scale virtual_index in place by (curve_lenth -1)/2
  vector_math::Vsmul(virtual_index, 1, 0.5 * (curve_length - 1), virtual_index,
                     1, frames_to_process);

  // Clip virtual_index, in place.
  vector_math::Vclip(virtual_index, 1, 0, curve_length - 1, virtual_index, 1,
                     frames_to_process);

  // index = floor(virtual_index)
  DCHECK_LE(frames_to_process, index_.size());
  float* index = index_.Data();

  // v1 and v2 hold the curve_data corresponding to the closest curve
  // values to the source sample.  To save memory, v1 will use the
  // destination array.
  DCHECK_LE(frames_to_process, v1_.size());
  DCHECK_LE(frames_to_process, v2_.size());
  float* v1 = v1_.Data();
  float* v2 = v2_.Data();

  // Interpolation factor: virtual_index - index.
  DCHECK_LE(frames_to_process, f_.size());
  float* f = f_.Data();

  int max_index = curve_length - 1;
  unsigned k = 0;
#if defined(ARCH_CPU_X86_FAMILY)
  {
    int loop_limit = frames_to_process / 4;

    // one = 1
    __m128i one = _mm_set1_epi32(1);

    // Do 4 eleemnts at a time
    for (int loop = 0; loop < loop_limit; ++loop, k += 4) {
      // v = virtual_index[k]
      __m128 v = _mm_loadu_ps(virtual_index + k);

      // index1 = static_cast<int>(v);
      __m128i index1 = _mm_cvttps_epi32(v);

      // v = static_cast<float>(index1) and save result to index[k:k+3]
      v = _mm_cvtepi32_ps(index1);
      _mm_storeu_ps(&index[k], v);

      // index2 = index2 + 1;
      __m128i index2 = _mm_add_epi32(index1, one);

      // Convert index1/index2 to arrays of 32-bit int values that are our
      // array indices to use to get the curve data.
      int32_t* i1 = reinterpret_cast<int32_t*>(&index1);
      int32_t* i2 = reinterpret_cast<int32_t*>(&index2);

      // Get the curve_data values and save them in v1 and v2,
      // carfully clamping the values.  If the input is NaN, index1
      // could be 0x8000000.
      v1[k] = curve_data[ClampTo(i1[0], 0, max_index)];
      v2[k] = curve_data[ClampTo(i2[0], 0, max_index)];
      v1[k + 1] = curve_data[ClampTo(i1[1], 0, max_index)];
      v2[k + 1] = curve_data[ClampTo(i2[1], 0, max_index)];
      v1[k + 2] = curve_data[ClampTo(i1[2], 0, max_index)];
      v2[k + 2] = curve_data[ClampTo(i2[2], 0, max_index)];
      v1[k + 3] = curve_data[ClampTo(i1[3], 0, max_index)];
      v2[k + 3] = curve_data[ClampTo(i2[3], 0, max_index)];
    }
  }
#elif defined(CPU_ARM_NEON)
  {
    int loop_limit = frames_to_process / 4;

    // Neon constants:
    //   zero = 0
    //   one  = 1
    //   max  = max_index
    int32x4_t zero = vdupq_n_s32(0);
    int32x4_t one = vdupq_n_s32(1);
    int32x4_t max = vdupq_n_s32(max_index);

    for (int loop = 0; loop < loop_limit; ++loop, k += 4) {
      // v = virtual_index
      float32x4_t v = vld1q_f32(virtual_index + k);

      // index1 = static_cast<int32_t>(v), then clamp to a valid index range for
      // curve_data
      int32x4_t index1 = vcvtq_s32_f32(v);
      index1 = vmaxq_s32(vminq_s32(index1, max), zero);

      // v = static_cast<float>(v) and save it away for later use.
      v = vcvtq_f32_s32(index1);
      vst1q_f32(&index[k], v);

      // index2 = index1 + 1, then clamp to a valid range for curve_data.
      int32x4_t index2 = vaddq_s32(index1, one);
      index2 = vmaxq_s32(vminq_s32(index2, max), zero);

      // Save index1/2 so we can get the individual parts.  Aligned to
      // 16 bytes for vst1q instruction.
      int32_t i1[4] __attribute__((aligned(16)));
      int32_t i2[4] __attribute__((aligned(16)));
      vst1q_s32(i1, index1);
      vst1q_s32(i2, index2);

      // Get curve elements corresponding to the indices.
      v1[k] = curve_data[i1[0]];
      v2[k] = curve_data[i2[0]];
      v1[k + 1] = curve_data[i1[1]];
      v2[k + 1] = curve_data[i2[1]];
      v1[k + 2] = curve_data[i1[2]];
      v2[k + 2] = curve_data[i2[2]];
      v1[k + 3] = curve_data[i1[3]];
      v2[k + 3] = curve_data[i2[3]];
    }
  }
#endif

  // Compute values for index1 and load the curve_data corresponding to indices.
  for (; k < frames_to_process; ++k) {
    unsigned index1 =
        ClampTo(static_cast<unsigned>(virtual_index[k]), 0, max_index);
    unsigned index2 = ClampTo(index1 + 1, 0, max_index);
    index[k] = index1;
    v1[k] = curve_data[index1];
    v2[k] = curve_data[index2];
  }

  // f[k] = virtual_index[k] - index[k]
  vector_math::Vsub(virtual_index, 1, index, 1, f, 1, frames_to_process);

  // Do the linear interpolation of the curve data:
  // destination[k] = v1[k] + f[k]*(v2[k] - v1[k])
  //
  // 1. v2[k] = v2[k] - v1[k]
  // 2. v2[k] = f[k]*v2[k] = f[k]*(v2[k] - v1[k])
  // 3. destination[k] = destination[k] + v2[k]
  //                   = v1[k] + f[k]*(v2[k] - v1[k])
  vector_math::Vsub(v2, 1, v1, 1, v2, 1, frames_to_process);
  vector_math::Vmul(f, 1, v2, 1, v2, 1, frames_to_process);
  vector_math::Vadd(v2, 1, v1, 1, destination, 1, frames_to_process);
}

void WaveShaperDSPKernel::ProcessCurve(const float* source,
                                       float* destination,
                                       uint32_t frames_to_process) {
  DCHECK(source);
  DCHECK(destination);
  DCHECK(GetWaveShaperProcessor());

  Vector<float>* curve = GetWaveShaperProcessor()->Curve();
  if (!curve) {
    // Act as "straight wire" pass-through if no curve is set.
    memcpy(destination, source, sizeof(float) * frames_to_process);
    return;
  }

  float* curve_data = curve->data();
  int curve_length = curve->size();

  DCHECK(curve_data);

  if (!curve_data || !curve_length) {
    memcpy(destination, source, sizeof(float) * frames_to_process);
    return;
  }

  // Apply waveshaping curve.
  WaveShaperCurveValues(destination, source, frames_to_process, curve_data,
                        curve_length);
}

void WaveShaperDSPKernel::ProcessCurve2x(const float* source,
                                         float* destination,
                                         uint32_t frames_to_process) {
  DCHECK_EQ(frames_to_process, RenderQuantumFrames());

  float* temp_p = temp_buffer_->Data();

  up_sampler_->Process(source, temp_p, frames_to_process);

  // Process at 2x up-sampled rate.
  ProcessCurve(temp_p, temp_p, frames_to_process * 2);

  down_sampler_->Process(temp_p, destination, frames_to_process * 2);
}

void WaveShaperDSPKernel::ProcessCurve4x(const float* source,
                                         float* destination,
                                         uint32_t frames_to_process) {
  DCHECK_EQ(frames_to_process, RenderQuantumFrames());

  float* temp_p = temp_buffer_->Data();
  float* temp_p2 = temp_buffer2_->Data();

  up_sampler_->Process(source, temp_p, frames_to_process);
  up_sampler2_->Process(temp_p, temp_p2, frames_to_process * 2);

  // Process at 4x up-sampled rate.
  ProcessCurve(temp_p2, temp_p2, frames_to_process * 4);

  down_sampler2_->Process(temp_p2, temp_p, frames_to_process * 4);
  down_sampler_->Process(temp_p, destination, frames_to_process * 2);
}

void WaveShaperDSPKernel::Reset() {
  if (up_sampler_) {
    up_sampler_->Reset();
    down_sampler_->Reset();
    up_sampler2_->Reset();
    down_sampler2_->Reset();
  }
}

bool WaveShaperDSPKernel::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

double WaveShaperDSPKernel::TailTime() const {
  return tail_time_;
}

double WaveShaperDSPKernel::LatencyTime() const {
  size_t latency_frames = 0;
  WaveShaperDSPKernel* kernel = const_cast<WaveShaperDSPKernel*>(this);

  switch (kernel->GetWaveShaperProcessor()->Oversample()) {
    case WaveShaperProcessor::kOverSampleNone:
      break;
    case WaveShaperProcessor::kOverSample2x:
      latency_frames += up_sampler_->LatencyFrames();
      latency_frames += down_sampler_->LatencyFrames();
      break;
    case WaveShaperProcessor::kOverSample4x: {
      // Account for first stage upsampling.
      latency_frames += up_sampler_->LatencyFrames();
      latency_frames += down_sampler_->LatencyFrames();

      // Account for second stage upsampling.
      // and divide by 2 to get back down to the regular sample-rate.
      size_t latency_frames2 =
          (up_sampler2_->LatencyFrames() + down_sampler2_->LatencyFrames()) / 2;
      latency_frames += latency_frames2;
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return static_cast<double>(latency_frames) / SampleRate();
}

}  // namespace blink
