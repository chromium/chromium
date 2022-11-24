/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel_processor.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// setNumberOfChannels() may later be called if the object is not yet in an
// "initialized" state.
AudioDSPKernelProcessor::AudioDSPKernelProcessor(float sample_rate,
                                                 unsigned number_of_channels,
                                                 unsigned render_quantum_frames)
    : AudioProcessor(sample_rate, number_of_channels, render_quantum_frames) {}

void AudioDSPKernelProcessor::Initialize() {
  if (IsInitialized()) {
    return;
  }

  base::AutoLock locker(process_lock_);
  DCHECK(!kernels_.size());

  // Create processing kernels, one per channel.
  for (unsigned i = 0; i < NumberOfChannels(); ++i) {
    kernels_.push_back(CreateKernel());
  }

  initialized_ = true;
}

void AudioDSPKernelProcessor::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  base::AutoLock locker(process_lock_);
  kernels_.clear();

  initialized_ = false;
}

void AudioDSPKernelProcessor::Process(const AudioBus* source,
                                      AudioBus* destination,
                                      uint32_t frames_to_process) {
  DCHECK(source);
  DCHECK(destination);

  if (!IsInitialized()) {
    destination->Zero();
    return;
  }

  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    DCHECK_EQ(source->NumberOfChannels(), destination->NumberOfChannels());
    DCHECK_EQ(source->NumberOfChannels(), kernels_.size());

    for (unsigned i = 0; i < kernels_.size(); ++i) {
      kernels_[i]->Process(source->Channel(i)->Data(),
                           destination->Channel(i)->MutableData(),
                           frames_to_process);
    }
  } else {
    // Unfortunately, the kernel is being processed by another thread.
    // See also ConvolverNode::process().
    destination->Zero();
  }
}

void AudioDSPKernelProcessor::ProcessOnlyAudioParams(
    uint32_t frames_to_process) {
  if (!IsInitialized()) {
    return;
  }

  base::AutoTryLock try_locker(process_lock_);
  // Only update the AudioParams if we can get the lock.  If not, some
  // other thread is updating the kernels, so we'll have to skip it
  // this time.
  if (try_locker.is_acquired()) {
    for (auto& kernel : kernels_) {
      kernel->ProcessOnlyAudioParams(frames_to_process);
    }
  }
}

// Resets filter state
void AudioDSPKernelProcessor::Reset() {
  DCHECK(IsMainThread());
  if (!IsInitialized()) {
    return;
  }

  base::AutoLock locker(process_lock_);
  for (auto& kernel : kernels_) {
    kernel->Reset();
  }
}

void AudioDSPKernelProcessor::SetNumberOfChannels(unsigned number_of_channels) {
  if (number_of_channels == number_of_channels_) {
    return;
  }

  DCHECK(!IsInitialized());
  number_of_channels_ = number_of_channels;
}

bool AudioDSPKernelProcessor::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

double AudioDSPKernelProcessor::TailTime() const {
  DCHECK(!IsMainThread());
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    // It is expected that all the kernels have the same tailTime.
    return !kernels_.empty() ? kernels_.front()->TailTime() : 0;
  }
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

double AudioDSPKernelProcessor::LatencyTime() const {
  DCHECK(!IsMainThread());
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    // It is expected that all the kernels have the same latencyTime.
    return !kernels_.empty() ? kernels_.front()->LatencyTime() : 0;
  }
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

}  // namespace blink
