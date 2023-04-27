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

#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/delay.h"

namespace blink {

namespace {

class DelayDSPKernel final : public AudioDSPKernel {
 public:
  explicit DelayDSPKernel(DelayProcessor* processor)
      : AudioDSPKernel(processor),
        delay_(processor->MaxDelayTime(),
               processor->SampleRate(),
               processor->RenderQuantumFrames()) {
    DCHECK(processor);
    DCHECK_GT(processor->SampleRate(), 0);
  }

  // Process the delay.  Basically dispatches to either ProcessKRate or
  // ProcessARate.
  void Process(const float* source,
               float* destination,
               uint32_t frames_to_process) override {
    if (HasSampleAccurateValues() && IsAudioRate()) {
      GetDelayProcessor()->DelayTime().CalculateSampleAccurateValues(
          delay_.DelayTimes(), frames_to_process);
      delay_.ProcessARate(source, destination, frames_to_process);
    } else {
      delay_.SetDelayTime(GetDelayProcessor()->DelayTime().FinalValue());
      delay_.ProcessKRate(source, destination, frames_to_process);
    }
  }

  void ProcessOnlyAudioParams(uint32_t frames_to_process) override {
    DCHECK_LE(frames_to_process, RenderQuantumFrames());

    float values[RenderQuantumFrames()];

    GetDelayProcessor()->DelayTime().CalculateSampleAccurateValues(
        values, frames_to_process);
  }

  void Reset() override { delay_.Reset(); }

  double TailTime() const override {
    // Account for worst case delay.
    // Don't try to track actual delay time which can change dynamically.
    return delay_.MaxDelayTime();
  }

  double LatencyTime() const override { return 0; }

  bool RequiresTailProcessing() const override {
    // Always return true even if the tail time and latency might both
    // be zero. This is for simplicity; most interesting delay nodes
    // have non-zero delay times anyway.  And it's ok to return true. It
    // just means the node lives a little longer than strictly
    // necessary.
    return true;
  }

 private:
  bool HasSampleAccurateValues() {
    return GetDelayProcessor()->DelayTime().HasSampleAccurateValues();
  }

  bool IsAudioRate() { return GetDelayProcessor()->DelayTime().IsAudioRate(); }

  DelayProcessor* GetDelayProcessor() {
    return static_cast<DelayProcessor*>(Processor());
  }

  Delay delay_;
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
