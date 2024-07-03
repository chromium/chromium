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

#include "third_party/blink/renderer/modules/webaudio/biquad_processor.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/modules/webaudio/biquad_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"

namespace blink {

BiquadProcessor::BiquadProcessor(float sample_rate,
                                 uint32_t number_of_channels,
                                 unsigned render_quantum_frames,
                                 AudioParamHandler& frequency,
                                 AudioParamHandler& q,
                                 AudioParamHandler& gain,
                                 AudioParamHandler& detune)
    : AudioDSPKernelProcessor(sample_rate,
                              number_of_channels,
                              render_quantum_frames),
      parameter1_(&frequency),
      parameter2_(&q),
      parameter3_(&gain),
      parameter4_(&detune) {}

BiquadProcessor::~BiquadProcessor() {
  if (IsInitialized()) {
    Uninitialize();
  }
}

std::unique_ptr<AudioDSPKernel> BiquadProcessor::CreateKernel() {
  return std::make_unique<BiquadDSPKernel>(this);
}

void BiquadProcessor::CheckForDirtyCoefficients() {
  // The BiquadDSPKernel objects rely on this value to see if they need to
  // re-compute their internal filter coefficients. Start out assuming filter
  // parameters are not changing.
  filter_coefficients_dirty_ = false;
  has_sample_accurate_values_ = false;

  if (parameter1_->HasSampleAccurateValues() ||
      parameter2_->HasSampleAccurateValues() ||
      parameter3_->HasSampleAccurateValues() ||
      parameter4_->HasSampleAccurateValues()) {
    // Coefficients are dirty if any of them has automations or if there are
    // connections to the AudioParam.
    filter_coefficients_dirty_ = true;
    has_sample_accurate_values_ = true;
    // If any parameter is a-rate, then the filter must do a-rate processing for
    // everything.
    is_audio_rate_ = parameter1_->IsAudioRate() || parameter2_->IsAudioRate() ||
                     parameter3_->IsAudioRate() || parameter4_->IsAudioRate();
  } else {
    if (has_just_reset_) {
      // Snap to exact values first time after reset
      previous_parameter1_ = std::numeric_limits<float>::quiet_NaN();
      previous_parameter2_ = std::numeric_limits<float>::quiet_NaN();
      previous_parameter3_ = std::numeric_limits<float>::quiet_NaN();
      previous_parameter4_ = std::numeric_limits<float>::quiet_NaN();
      filter_coefficients_dirty_ = true;
      has_just_reset_ = false;
    } else {
      // If filter parameters have changed then mark coefficients as dirty.
      const float parameter1_final = parameter1_->FinalValue();
      const float parameter2_final = parameter2_->FinalValue();
      const float parameter3_final = parameter3_->FinalValue();
      const float parameter4_final = parameter4_->FinalValue();
      if ((previous_parameter1_ != parameter1_final) ||
          (previous_parameter2_ != parameter2_final) ||
          (previous_parameter3_ != parameter3_final) ||
          (previous_parameter4_ != parameter4_final)) {
        filter_coefficients_dirty_ = true;
        previous_parameter1_ = parameter1_final;
        previous_parameter2_ = parameter2_final;
        previous_parameter3_ = parameter3_final;
        previous_parameter4_ = parameter4_final;
      }
    }
  }
}

void BiquadProcessor::Initialize() {
  AudioDSPKernelProcessor::Initialize();
  has_just_reset_ = true;
}

void BiquadProcessor::Process(const AudioBus* source,
                              AudioBus* destination,
                              uint32_t frames_to_process) {
  if (!IsInitialized()) {
    destination->Zero();
    return;
  }

  // Synchronize with possible dynamic changes to the impulse response.
  base::AutoTryLock try_locker(process_lock_);
  if (!try_locker.is_acquired()) {
    // Can't get the lock. We must be in the middle of changing something.
    destination->Zero();
    return;
  }

  CheckForDirtyCoefficients();

  // For each channel of our input, process using the corresponding
  // BiquadDSPKernel into the output channel.
  for (unsigned i = 0; i < kernels_.size(); ++i) {
    kernels_[i]->Process(source->Channel(i)->Data(),
                         destination->Channel(i)->MutableData(),
                         frames_to_process);
  }
}

void BiquadProcessor::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  // TODO(crbug.com/40637820): Eventually, the render quantum size will no
  // longer be hardcoded as 128. At that point, we'll need to switch from
  // stack allocation to heap allocation.
  constexpr unsigned render_quantum_frames_expected = 128;
  CHECK_EQ(RenderQuantumFrames(), render_quantum_frames_expected);

  DCHECK_LE(frames_to_process, render_quantum_frames_expected);

  float values[render_quantum_frames_expected];

  parameter1_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter2_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter3_->CalculateSampleAccurateValues(values, frames_to_process);
  parameter4_->CalculateSampleAccurateValues(values, frames_to_process);
}

void BiquadProcessor::Reset() {
  AudioDSPKernelProcessor::Reset();
  has_just_reset_ = true;
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
    // `response_kernel` with these values.  We need to synchronize with
    // `Process()` to prevent process() from updating the filter coefficients
    // while we're trying to access them.  Since this is on the main thread, we
    // can wait.  The audio thread will update the coefficients the next time
    // around, it it were blocked.
    base::AutoLock process_locker(process_lock_);

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
