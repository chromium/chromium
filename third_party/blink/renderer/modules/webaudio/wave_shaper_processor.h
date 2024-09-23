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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_PROCESSOR_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel_processor.h"

namespace blink {

// WaveShaperProcessor is an AudioDSPKernelProcessor which uses
// WaveShaperDSPKernel objects to implement non-linear distortion effects.

class WaveShaperProcessor final : public AudioDSPKernelProcessor {
 public:
  enum OverSampleType { kOverSampleNone, kOverSample2x, kOverSample4x };

  WaveShaperProcessor(float sample_rate,
                      unsigned number_of_channels,
                      unsigned render_quantum_frames);

  ~WaveShaperProcessor() override;

  std::unique_ptr<AudioDSPKernel> CreateKernel() override;

  void Process(const AudioBus* source,
               AudioBus* destination,
               uint32_t frames_to_process) override;

  void SetCurve(const float* curve_data, unsigned curve_length);
  Vector<float>* Curve() const { return curve_.get(); }

  void SetOversample(OverSampleType);
  OverSampleType Oversample() const { return oversample_; }

 private:
  // m_curve represents the non-linear shaping curve.
  std::unique_ptr<Vector<float>> curve_;

  OverSampleType oversample_ = kOverSampleNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_PROCESSOR_H_
