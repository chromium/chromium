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

#include <memory>
#include "third_party/blink/renderer/modules/webaudio/biquad_dsp_kernel.h"
#include "third_party/blink/renderer/modules/webaudio/biquad_processor.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"

namespace blink {

BiquadProcessor::BiquadProcessor(float sample_rate,
                                 uint32_t number_of_channels,
                                 AudioParamHandler& frequency,
                                 AudioParamHandler& q,
                                 AudioParamHandler& gain,
                                 AudioParamHandler& detune)
    : AudioDSPKernelProcessor(sample_rate, number_of_channels),
      type_(kLowPass),
      parameter1_(&frequency),
      parameter2_(&q),
      parameter3_(&gain),
      parameter4_(&detune),
      filter_coefficients_dirty_(true),
      has_sample_accurate_values_(false) {}

BiquadProcessor::~BiquadProcessor() {
  if (IsInitialized())
    Uninitialize();
}

std::unique_ptr<AudioDSPKernel> BiquadProcessor::CreateKernel() {
  return std::make_unique<BiquadDSPKernel>(this);
}

void BiquadProcessor::CheckForDirtyCoefficients() {
  // Deal with smoothing / de-zippering. Start out assuming filter parameters
  // are not changing.

  // The BiquadDSPKernel objects rely on this value to see if they need to
  // re-compute their internal filter coefficients.
  filter_coefficients_dirty_ = false;
  has_sample_accurate_values_ = false;

  if (parameter1_->HasSampleAccurateValues() ||
      parameter2_->HasSampleAccurateValues() ||
      parameter3_->HasSampleAccurateValues() ||
      parameter4_->HasSampleAccurateValues()) {
    filter_coefficients_dirty_ = true;
    has_sample_accurate_values_ = true;
  } else {
    if (has_just_reset_) {
      // Snap to exact values first time after reset, then smooth for subsequent
      // changes.
      parameter1_->ResetSmoothedValue();
      parameter2_->ResetSmoothedValue();
      parameter3_->ResetSmoothedValue();
      parameter4_->ResetSmoothedValue();
      filter_coefficients_dirty_ = true;
      has_just_reset_ = false;
    } else {
      // TODO(crbug.com/763994): With dezippering removed, we don't want to use
      // these methods.  We need to implement another way of noticing if one of
      // the parameters has changed.  We do this as an optimization because
      // computing the filter coefficients from these parameters is fairly
      // expensive.  NB: The calls to Smooth() don't actually cause the
      // coefficients to be dezippered.  This is just a way to notice that the
      // coefficient values have changed.  |UpdateCoefficientsIfNecessary()|
      // checks to see if the filter coefficients are dirty and sets the filter
      // to the new value, without smoothing.
      //
      // Smooth all of the filter parameters. If they haven't yet converged to
      // their target value then mark coefficients as dirty.
      bool is_stable1 = parameter1_->Smooth();
      bool is_stable2 = parameter2_->Smooth();
      bool is_stable3 = parameter3_->Smooth();
      bool is_stable4 = parameter4_->Smooth();
      if (!(is_stable1 && is_stable2 && is_stable3 && is_stable4))
        filter_coefficients_dirty_ = true;
    }
  }
}

void BiquadProcessor::Process(const AudioBus* source,
                              AudioBus* destination,
                              uint32_t frames_to_process) {
  if (!IsInitialized()) {
    destination->Zero();
    return;
  }

  // Synchronize with possible dynamic changes to the impulse response.
  MutexTryLocker try_locker(process_lock_);
  if (!try_locker.Locked()) {
    // Can't get the lock. We must be in the middle of changing something.
    destination->Zero();
    return;
  }

  CheckForDirtyCoefficients();

  // For each channel of our input, process using the corresponding
  // BiquadDSPKernel into the output channel.
  for (unsigned i = 0; i < kernels_.size(); ++i)
    kernels_[i]->Process(source->Channel(i)->Data(),
                         destination->Channel(i)->MutableData(),
                         frames_to_process);
}

void BiquadProcessor::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  DCHECK_LE(frames_to_process, audio_utilities::kRenderQuantumFrames);

  float values[audio_utilities::kRenderQuantumFrames];

  parameter1_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter2_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter3_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter4_->CalculateSampleAccurateValues(values, frames_to_process);
}

void BiquadProcessor::SetType(FilterType type) {
  if (type != type_) {
    type_ = type;
    Reset();  // The filter state must be reset only if the type has changed.
  }
}

void BiquadProcessor::GetFrequencyResponse(int n_frequencies,
                                           const float* frequency_hz,
                                           float* mag_response,
                                           float* phase_response) {
  DCHECK(IsMainThread());

  // Compute the frequency response on a separate temporary kernel
  // to avoid interfering with the processing running in the audio
  // thread on the main kernels.

  std::unique_ptr<BiquadDSPKernel> response_kernel =
      std::make_unique<BiquadDSPKernel>(this);

  float cutoff_frequency;
  float q;
  float gain;
  float detune;  // in Cents

  {
    // Get a copy of the current biquad filter coefficients so we can update
    // |response_kernel| with these values.  We need to synchronize with
    // |Process()| to prevent process() from updating the filter coefficients
    // while we're trying to access them.  Since this is on the main thread, we
    // can wait.  The audio thread will update the coefficients the next time
    // around, it it were blocked.
    MutexLocker process_locker(process_lock_);

    cutoff_frequency = Parameter1().Value();
    q = Parameter2().Value();
    gain = Parameter3().Value();
    detune = Parameter4().Value();
  }

  response_kernel->UpdateCoefficients(1, &cutoff_frequency, &q, &gain, &detune);
  BiquadDSPKernel::GetFrequencyResponse(*response_kernel, n_frequencies,
                                        frequency_hz, mag_response,
                                        phase_response);
}

}  // namespace blink
