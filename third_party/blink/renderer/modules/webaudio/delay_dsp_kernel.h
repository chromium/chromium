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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELAY_DSP_KERNEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELAY_DSP_KERNEL_H_

#include "third_party/blink/renderer/modules/webaudio/delay_processor.h"
#include "third_party/blink/renderer/platform/audio/audio_delay_dsp_kernel.h"

namespace blink {

class DelayProcessor;

class DelayDSPKernel final : public AudioDelayDSPKernel {
 public:
  explicit DelayDSPKernel(DelayProcessor*);

 protected:
  bool HasSampleAccurateValues() override;
  void CalculateSampleAccurateValues(float* delay_times,
                                     uint32_t frames_to_process) override;
  double DelayTime(float sample_rate) override;
  bool IsAudioRate() override;

  void ProcessOnlyAudioParams(uint32_t frames_to_process) override;

 private:
  DelayProcessor* GetDelayProcessor() {
    return static_cast<DelayProcessor*>(Processor());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELAY_DSP_KERNEL_H_
