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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DSP_KERNEL_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DSP_KERNEL_PROCESSOR_H_

#include <memory>

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AudioBus;
class AudioDSPKernel;
class AudioProcessor;

// AudioDSPKernelProcessor processes one input -> one output (N channels each)
// It uses one AudioDSPKernel object per channel to do the processing, thus
// there is no cross-channel processing.  Despite this limitation it turns out
// to be a very common and useful type of processor.

class PLATFORM_EXPORT AudioDSPKernelProcessor {
 public:
  // numberOfChannels may be later changed if object is not yet in an
  // "initialized" state
  AudioDSPKernelProcessor(float sample_rate,
                          unsigned number_of_channels,
                          unsigned render_quantum_frames);

  virtual ~AudioDSPKernelProcessor();

  // Subclasses create the appropriate type of processing kernel here.
  // We'll call this to create a kernel for each channel.
  virtual std::unique_ptr<AudioDSPKernel> CreateKernel() = 0;

  // AudioProcessor methods
  virtual void Initialize();
  virtual void Uninitialize();
  virtual void Process(const AudioBus* source,
                       AudioBus* destination,
                       uint32_t frames_to_process);
  virtual void ProcessOnlyAudioParams(uint32_t frames_to_process);
  virtual void Reset();
  virtual void SetNumberOfChannels(unsigned);
  virtual unsigned NumberOfChannels() const { return number_of_channels_; }

  bool IsInitialized() const { return initialized_; }

  float SampleRate() const { return sample_rate_; }

  unsigned RenderQuantumFrames() const { return render_quantum_frames_; }

  virtual double TailTime() const;
  virtual double LatencyTime() const;
  virtual bool RequiresTailProcessing() const;

 protected:
  bool initialized_ = false;
  unsigned number_of_channels_;
  float sample_rate_;
  unsigned render_quantum_frames_;

  Vector<std::unique_ptr<AudioDSPKernel>> kernels_ GUARDED_BY(process_lock_);
  mutable base::Lock process_lock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_AUDIO_DSP_KERNEL_PROCESSOR_H_
