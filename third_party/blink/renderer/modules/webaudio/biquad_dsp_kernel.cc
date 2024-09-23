/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/biquad_dsp_kernel.h"

#include <limits.h>

#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#ifdef __SSE2__
#include <immintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

namespace blink {

namespace {

bool HasConstantValues(float* values, int frames_to_process) {
  // Load the initial value
  const float value = values[0];
  // This initialization ensures that we correctly handle the first frame and
  // start the processing from the second frame onwards, effectively excluding
  // the first frame from the subsequent comparisons in the non-SIMD paths
  // it guarantees that we don't redundantly compare the first frame again
  // during the loop execution.
  int processed_frames = 1;

#if defined(__SSE2__)
  // Process 4 floats at a time using SIMD
  __m128 value_vec = _mm_set1_ps(value);
  // Start at 0 for byte alignment
  for (processed_frames = 0; processed_frames < frames_to_process - 3;
       processed_frames += 4) {
    // Load 4 floats from memory
    __m128 input_vec = _mm_loadu_ps(&values[processed_frames]);
    // Compare the 4 floats with the value
    __m128 cmp_vec = _mm_cmpneq_ps(input_vec, value_vec);
    // Check if any of the floats are not equal to the value
    if (_mm_movemask_ps(cmp_vec) != 0) {
      return false;
    }
  }
#elif defined(__ARM_NEON__)
  // Process 4 floats at a time using SIMD
  float32x4_t value_vec = vdupq_n_f32(value);
  // Start at 0 for byte alignment
  for (processed_frames = 0; processed_frames < frames_to_process - 3;
       processed_frames += 4) {
    // Load 4 floats from memory
    float32x4_t input_vec = vld1q_f32(&values[processed_frames]);
    // Compare the 4 floats with the value
    uint32x4_t cmp_vec = vceqq_f32(input_vec, value_vec);
    // Accumulate the elements of the cmp_vec vector using bitwise AND
    uint32x2_t cmp_reduced_32 =
        vand_u32(vget_low_u32(cmp_vec), vget_high_u32(cmp_vec));
    // Check if any of the floats are not equal to the value
    if (vget_lane_u32(vpmin_u32(cmp_reduced_32, cmp_reduced_32), 0) == 0) {
      return false;
    }
  }
#endif
  // Fallback implementation without SIMD optimization
  while (processed_frames < frames_to_process) {
    if (values[processed_frames] != value) {
      return false;
    }
    processed_frames++;
  }
  return true;
}

}  // namespace

bool BiquadDSPKernel::HasConstantValuesForTesting(float* values,
                                                  int frames_to_process) {
  return HasConstantValues(values, frames_to_process);
}

void BiquadDSPKernel::UpdateCoefficientsIfNecessary(int frames_to_process) {
  if (GetBiquadProcessor()->FilterCoefficientsDirty()) {
    // TODO(crbug.com/40637820): Eventually, the render quantum size will no
    // longer be hardcoded as 128. At that point, we'll need to switch from
    // stack allocation to heap allocation.
    constexpr unsigned render_quantum_frames_expected = 128;
    CHECK_EQ(RenderQuantumFrames(), render_quantum_frames_expected);
    float cutoff_frequency[render_quantum_frames_expected];
    float q[render_quantum_frames_expected];
    float gain[render_quantum_frames_expected];
    float detune[render_quantum_frames_expected];  // in Cents

    SECURITY_CHECK(static_cast<unsigned>(frames_to_process) <=
                   RenderQuantumFrames());

    if (GetBiquadProcessor()->HasSampleAccurateValues() &&
        GetBiquadProcessor()->IsAudioRate()) {
      GetBiquadProcessor()->Parameter1().CalculateSampleAccurateValues(
          cutoff_frequency, frames_to_process);
      GetBiquadProcessor()->Parameter2().CalculateSampleAccurateValues(
          q, frames_to_process);
      GetBiquadProcessor()->Parameter3().CalculateSampleAccurateValues(
          gain, frames_to_process);
      GetBiquadProcessor()->Parameter4().CalculateSampleAccurateValues(
          detune, frames_to_process);

      // If all the values are actually constant for this render (or the
      // automation rate is "k-rate" for all of the AudioParams), we don't need
      // to compute filter coefficients for each frame since they would be the
      // same as the first.
      bool isConstant =
          HasConstantValues(cutoff_frequency, frames_to_process) &&
          HasConstantValues(q, frames_to_process) &&
          HasConstantValues(gain, frames_to_process) &&
          HasConstantValues(detune, frames_to_process);

      UpdateCoefficients(isConstant ? 1 : frames_to_process, cutoff_frequency,
                         q, gain, detune);
    } else {
      cutoff_frequency[0] = GetBiquadProcessor()->Parameter1().FinalValue();
      q[0] = GetBiquadProcessor()->Parameter2().FinalValue();
      gain[0] = GetBiquadProcessor()->Parameter3().FinalValue();
      detune[0] = GetBiquadProcessor()->Parameter4().FinalValue();
      UpdateCoefficients(1, cutoff_frequency, q, gain, detune);
    }
  }
}

void BiquadDSPKernel::UpdateCoefficients(int number_of_frames,
                                         const float* cutoff_frequency,
                                         const float* q,
                                         const float* gain,
                                         const float* detune) {
  // Convert from Hertz to normalized frequency 0 -> 1.
  double nyquist = Nyquist();

  biquad_.SetHasSampleAccurateValues(number_of_frames > 1);

  for (int k = 0; k < number_of_frames; ++k) {
    double normalized_frequency = cutoff_frequency[k] / nyquist;

    // Offset frequency by detune.
    if (detune[k]) {
      // Detune multiplies the frequency by 2^(detune[k] / 1200).
      normalized_frequency *= exp2(detune[k] / 1200);
    }

    // Configure the biquad with the new filter parameters for the appropriate
    // type of filter.
    switch (GetBiquadProcessor()->GetType()) {
      case BiquadProcessor::FilterType::kLowPass:
        biquad_.SetLowpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::FilterType::kHighPass:
        biquad_.SetHighpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::FilterType::kBandPass:
        biquad_.SetBandpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::FilterType::kLowShelf:
        biquad_.SetLowShelfParams(k, normalized_frequency, gain[k]);
        break;

      case BiquadProcessor::FilterType::kHighShelf:
        biquad_.SetHighShelfParams(k, normalized_frequency, gain[k]);
        break;

      case BiquadProcessor::FilterType::kPeaking:
        biquad_.SetPeakingParams(k, normalized_frequency, q[k], gain[k]);
        break;

      case BiquadProcessor::FilterType::kNotch:
        biquad_.SetNotchParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::FilterType::kAllpass:
        biquad_.SetAllpassParams(k, normalized_frequency, q[k]);
        break;
    }
  }

  UpdateTailTime(number_of_frames - 1);
}

void BiquadDSPKernel::UpdateTailTime(int coef_index) {
  // TODO(crbug.com/1447095): A reasonable upper limit for the tail time.  While
  // it's easy to create biquad filters whose tail time can be much larger than
  // this, limit the maximum to this value so that we don't keep such nodes
  // alive "forever". Investigate if we can adjust this to a smaller value.
  constexpr double kMaxTailTime = 30.0;

  double sample_rate = SampleRate();
  double tail =
      biquad_.TailFrame(coef_index, kMaxTailTime * sample_rate) / sample_rate;

  tail_time_ = ClampTo(tail, 0.0, kMaxTailTime);
}

void BiquadDSPKernel::Process(const float* source,
                              float* destination,
                              uint32_t frames_to_process) {
  DCHECK(source);
  DCHECK(destination);
  DCHECK(GetBiquadProcessor());

  // Recompute filter coefficients if any of the parameters have changed.
  // FIXME: as an optimization, implement a way that a Biquad object can simply
  // copy its internal filter coefficients from another Biquad object.  Then
  // re-factor this code to only run for the first BiquadDSPKernel of each
  // BiquadProcessor.

  // The audio thread can't block on this lock; skip updating the coefficients
  // for this block if necessary. We'll get them the next time around.
  {
    base::AutoTryLock try_locker(process_lock_);
    if (try_locker.is_acquired()) {
      UpdateCoefficientsIfNecessary(frames_to_process);
    }
  }

  biquad_.Process(source, destination, frames_to_process);
}

void BiquadDSPKernel::GetFrequencyResponse(BiquadDSPKernel& kernel,
                                           int n_frequencies,
                                           const float* frequency_hz,
                                           float* mag_response,
                                           float* phase_response) {
  // Only allow on the main thread because we don't want the audio thread to be
  // updating `kernel` while we're computing the response.
  DCHECK(IsMainThread());

  DCHECK_GE(n_frequencies, 0);
  DCHECK(frequency_hz);
  DCHECK(mag_response);
  DCHECK(phase_response);

  Vector<float> frequency(n_frequencies);
  double nyquist = kernel.Nyquist();

  // Convert from frequency in Hz to normalized frequency (0 -> 1),
  // with 1 equal to the Nyquist frequency.
  for (int k = 0; k < n_frequencies; ++k) {
    frequency[k] = frequency_hz[k] / nyquist;
  }

  kernel.biquad_.GetFrequencyResponse(n_frequencies, frequency.data(),
                                      mag_response, phase_response);
}

bool BiquadDSPKernel::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both
  // be zero. This is for simplicity and because TailTime() is 0
  // basically only when the filter response H(z) = 0 or H(z) = 1. And
  // it's ok to return true. It just means the node lives a little
  // longer than strictly necessary.
  return true;
}

double BiquadDSPKernel::TailTime() const {
  return tail_time_;
}

double BiquadDSPKernel::LatencyTime() const {
  return 0;
}

}  // namespace blink
