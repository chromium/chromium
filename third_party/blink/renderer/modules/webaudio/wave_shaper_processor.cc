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

#include <memory>
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_dsp_kernel.h"
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_processor.h"

namespace blink {

WaveShaperProcessor::WaveShaperProcessor(float sample_rate,
                                         unsigned number_of_channels)
    : AudioDSPKernelProcessor(sample_rate, number_of_channels),
      oversample_(kOverSampleNone) {}

WaveShaperProcessor::~WaveShaperProcessor() {
  if (IsInitialized())
    Uninitialize();
}

std::unique_ptr<AudioDSPKernel> WaveShaperProcessor::CreateKernel() {
  return std::make_unique<WaveShaperDSPKernel>(this);
}

void WaveShaperProcessor::SetCurve(const float* curve_data,
                                   unsigned curve_length) {
  DCHECK(IsMainThread());

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);

  if (curve_length == 0 || !curve_data) {
    curve_ = nullptr;
    return;
  }

  // Copy the curve data, if any, to our internal buffer.
  curve_ = std::make_unique<Vector<float>>(curve_length);
  memcpy(curve_->data(), curve_data, sizeof(float) * curve_length);

  DCHECK_GE(kernels_.size(), 1ULL);

  // Compute the curve output for a zero input, and set the tail time for all
  // the kernels.
  WaveShaperDSPKernel* kernel =
      static_cast<WaveShaperDSPKernel*>(kernels_[0].get());
  double output = kernel->WaveShaperCurveValue(0.0, curve_data, curve_length);
  double tail_time = output == 0 ? 0 : std::numeric_limits<double>::infinity();

  for (unsigned k = 0; k < kernels_.size(); ++k) {
    kernel = static_cast<WaveShaperDSPKernel*>(kernels_[k].get());
    kernel->SetTailTime(tail_time);
  }
}

void WaveShaperProcessor::SetOversample(OverSampleType oversample) {
  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);

  oversample_ = oversample;

  if (oversample != kOverSampleNone) {
    for (unsigned i = 0; i < kernels_.size(); ++i) {
      WaveShaperDSPKernel* kernel =
          static_cast<WaveShaperDSPKernel*>(kernels_[i].get());
      kernel->LazyInitializeOversampling();
    }
  }
}

void WaveShaperProcessor::Process(const AudioBus* source,
                                  AudioBus* destination,
                                  uint32_t frames_to_process) {
  if (!IsInitialized()) {
    destination->Zero();
    return;
  }

  DCHECK_EQ(source->NumberOfChannels(), destination->NumberOfChannels());
  DCHECK_EQ(source->NumberOfChannels(), kernels_.size());

  // The audio thread can't block on this lock, so we call tryLock() instead.
  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked()) {
    // For each channel of our input, process using the corresponding
    // WaveShaperDSPKernel into the output channel.
    for (unsigned i = 0; i < kernels_.size(); ++i)
      kernels_[i]->Process(source->Channel(i)->Data(),
                           destination->Channel(i)->MutableData(),
                           frames_to_process);
  } else {
    // Too bad - the tryLock() failed. We must be in the middle of a setCurve()
    // call.
    destination->Zero();
  }
}

}  // namespace blink
