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

#include "third_party/blink/renderer/modules/webaudio/delay_processor.h"

#include <memory>

#include "third_party/blink/renderer/platform/audio/audio_delay_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"

namespace blink {

namespace {

class DelayDSPKernel final : public AudioDelayDSPKernel {
 public:
  explicit DelayDSPKernel(DelayProcessor* processor)
      : AudioDelayDSPKernel(processor, processor->RenderQuantumFrames()) {
    DCHECK(processor);
    DCHECK_GT(processor->SampleRate(), 0);

    max_delay_time_ = processor->MaxDelayTime();
    DCHECK_GE(max_delay_time_, 0);
    DCHECK(!std::isnan(max_delay_time_));

    buffer_.Allocate(BufferLengthForDelay(max_delay_time_,
                                          processor->SampleRate(),
                                          processor->RenderQuantumFrames()));
    buffer_.Zero();
  }

 protected:
  bool HasSampleAccurateValues() override {
    return GetDelayProcessor()->DelayTime().HasSampleAccurateValues();
  }

  void CalculateSampleAccurateValues(float* delay_times,
                                     uint32_t frames_to_process) override {
    GetDelayProcessor()->DelayTime().CalculateSampleAccurateValues(
        delay_times, frames_to_process);
  }

  double DelayTime(float sample_rate) override {
    return GetDelayProcessor()->DelayTime().FinalValue();
  }

  bool IsAudioRate() override {
    return GetDelayProcessor()->DelayTime().IsAudioRate();
  }

  void ProcessOnlyAudioParams(uint32_t frames_to_process) override {
    DCHECK_LE(frames_to_process, RenderQuantumFrames());

    float values[RenderQuantumFrames()];

    GetDelayProcessor()->DelayTime().CalculateSampleAccurateValues(
        values, frames_to_process);
  }

 private:
  DelayProcessor* GetDelayProcessor() {
    return static_cast<DelayProcessor*>(Processor());
  }
};

}  // namespace

DelayProcessor::DelayProcessor(float sample_rate,
                               unsigned number_of_channels,
                               unsigned render_quantum_frames,
                               AudioParamHandler& delay_time,
                               double max_delay_time)
    : AudioDSPKernelProcessor(sample_rate,
                              number_of_channels,
                              render_quantum_frames),
      delay_time_(&delay_time),
      max_delay_time_(max_delay_time) {}

DelayProcessor::~DelayProcessor() {
  if (IsInitialized()) {
    Uninitialize();
  }
}

std::unique_ptr<AudioDSPKernel> DelayProcessor::CreateKernel() {
  return std::make_unique<DelayDSPKernel>(this);
}

void DelayProcessor::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  DCHECK_LE(frames_to_process, RenderQuantumFrames());

  float values[RenderQuantumFrames()];

  delay_time_->CalculateSampleAccurateValues(values, frames_to_process);
}

}  // namespace blink
