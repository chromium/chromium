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

#include <limits.h>
#include "third_party/blink/renderer/modules/webaudio/biquad_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

static bool hasConstantValues(float* values, int frames_to_process) {
  // TODO(rtoy): Use SIMD to optimize this.  This would speed up
  // processing by a factor of 4 because we can process 4 floats at a
  // time.
  float value = values[0];

  for (int k = 1; k < frames_to_process; ++k) {
    if (values[k] != value) {
      return false;
    }
  }

  return true;
}

void BiquadDSPKernel::UpdateCoefficientsIfNecessary(int frames_to_process) {
  if (GetBiquadProcessor()->FilterCoefficientsDirty()) {
    float cutoff_frequency[audio_utilities::kRenderQuantumFrames];
    float q[audio_utilities::kRenderQuantumFrames];
    float gain[audio_utilities::kRenderQuantumFrames];
    float detune[audio_utilities::kRenderQuantumFrames];  // in Cents

    SECURITY_CHECK(static_cast<unsigned>(frames_to_process) <=
                   audio_utilities::kRenderQuantumFrames);

    if (GetBiquadProcessor()->HasSampleAccurateValues()) {
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
          hasConstantValues(cutoff_frequency, frames_to_process) &&
          hasConstantValues(q, frames_to_process) &&
          hasConstantValues(gain, frames_to_process) &&
          hasConstantValues(detune, frames_to_process);

      UpdateCoefficients(isConstant ? 1 : frames_to_process, cutoff_frequency,
                         q, gain, detune);
    } else {
      cutoff_frequency[0] = GetBiquadProcessor()->Parameter1().Value();
      q[0] = GetBiquadProcessor()->Parameter2().Value();
      gain[0] = GetBiquadProcessor()->Parameter3().Value();
      detune[0] = GetBiquadProcessor()->Parameter4().Value();
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
  double nyquist = this->Nyquist();

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
      case BiquadProcessor::kLowPass:
        biquad_.SetLowpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::kHighPass:
        biquad_.SetHighpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::kBandPass:
        biquad_.SetBandpassParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::kLowShelf:
        biquad_.SetLowShelfParams(k, normalized_frequency, gain[k]);
        break;

      case BiquadProcessor::kHighShelf:
        biquad_.SetHighShelfParams(k, normalized_frequency, gain[k]);
        break;

      case BiquadProcessor::kPeaking:
        biquad_.SetPeakingParams(k, normalized_frequency, q[k], gain[k]);
        break;

      case BiquadProcessor::kNotch:
        biquad_.SetNotchParams(k, normalized_frequency, q[k]);
        break;

      case BiquadProcessor::kAllpass:
        biquad_.SetAllpassParams(k, normalized_frequency, q[k]);
        break;
    }
  }

  UpdateTailTime(number_of_frames - 1);
}

void BiquadDSPKernel::UpdateTailTime(int coef_index) {
  // A reasonable upper limit for the tail time.  While it's easy to
  // create biquad filters whose tail time can be much larger than
  // this, limit the maximum to this value so that we don't keep such
  // nodes alive "forever".
  // TODO: What is a reasonable upper limit?
  const double kMaxTailTime = 30;

  double sample_rate = SampleRate();
  double tail =
      biquad_.TailFrame(coef_index, kMaxTailTime * sample_rate) / sample_rate;

  tail_time_ = clampTo(tail, 0.0, kMaxTailTime);
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
    MutexTryLocker try_locker(process_lock_);
    if (try_locker.Locked())
      UpdateCoefficientsIfNecessary(frames_to_process);
  }

  biquad_.Process(source, destination, frames_to_process);
}

void BiquadDSPKernel::GetFrequencyResponse(BiquadDSPKernel& kernel,
                                           int n_frequencies,
                                           const float* frequency_hz,
                                           float* mag_response,
                                           float* phase_response) {
  // Only allow on the main thread because we don't want the audio thread to be
  // updating |kernel| while we're computing the response.
  DCHECK(IsMainThread());

  DCHECK_GT(n_frequencies, 0);
  DCHECK(frequency_hz);
  DCHECK(mag_response);
  DCHECK(phase_response);

  Vector<float> frequency(n_frequencies);
  double nyquist = kernel.Nyquist();

  // Convert from frequency in Hz to normalized frequency (0 -> 1),
  // with 1 equal to the Nyquist frequency.
  for (int k = 0; k < n_frequencies; ++k)
    frequency[k] = frequency_hz[k] / nyquist;

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
