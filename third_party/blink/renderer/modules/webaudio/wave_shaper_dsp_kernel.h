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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_DSP_KERNEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_DSP_KERNEL_H_

#include <memory>
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_processor.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/down_sampler.h"
#include "third_party/blink/renderer/platform/audio/up_sampler.h"

namespace blink {

class WaveShaperProcessor;

// WaveShaperDSPKernel is an AudioDSPKernel and is responsible for non-linear
// distortion on one channel.

class WaveShaperDSPKernel final : public AudioDSPKernel {
 public:
  explicit WaveShaperDSPKernel(WaveShaperProcessor*);

  // AudioDSPKernel
  void Process(const float* source,
               float* dest,
               uint32_t frames_to_process) override;
  void Reset() override;
  double TailTime() const override;
  double LatencyTime() const override;
  bool RequiresTailProcessing() const final;

  // Oversampling requires more resources, so let's only allocate them if
  // needed.
  void LazyInitializeOversampling();

  // Computes value of the WaveShaper
  double WaveShaperCurveValue(float input,
                              const float* curve_data,
                              int curve_length) const;

  // Like WaveShaperCurveValue, but computes the values for a vector of inputs.
  void WaveShaperCurveValues(float* destination,
                             const float* input,
                             uint32_t frames_to_process,
                             const float* curve_data,
                             int curve_length) const;
  // Set the tail time
  void SetTailTime(double time) { tail_time_ = time; }

 protected:
  // Apply the shaping curve.
  void ProcessCurve(const float* source,
                    float* dest,
                    uint32_t frames_to_process);

  // Use up-sampling, process at the higher sample-rate, then down-sample.
  void ProcessCurve2x(const float* source,
                      float* dest,
                      uint32_t frames_to_process);
  void ProcessCurve4x(const float* source,
                      float* dest,
                      uint32_t frames_to_process);

  WaveShaperProcessor* GetWaveShaperProcessor() {
    return static_cast<WaveShaperProcessor*>(Processor());
  }

  // Oversampling.
  std::unique_ptr<AudioFloatArray> temp_buffer_;
  std::unique_ptr<AudioFloatArray> temp_buffer2_;
  std::unique_ptr<UpSampler> up_sampler_;
  std::unique_ptr<DownSampler> down_sampler_;
  std::unique_ptr<UpSampler> up_sampler2_;
  std::unique_ptr<DownSampler> down_sampler2_;

 private:
  // Tail time for the WaveShaper.  This basically can have two values: 0 and
  // infinity.  It only takes the value of infinity if the wave shaper curve is
  // such that a zero input produces a non-zero output.  In this case, the node
  // has an infinite tail so that silent input continues to produce non-silent
  // output.
  double tail_time_;

  // Work arrays needed by WaveShaperCurveValues().  Mutable so this
  // const function can modify these arrays.  There's no state or
  // anything kept here.  See WaveShaperCurveValues() for details on
  // what these hold.
  mutable AudioFloatArray virtual_index_;
  mutable AudioFloatArray index_;
  mutable AudioFloatArray v1_;
  mutable AudioFloatArray v2_;
  mutable AudioFloatArray f_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_WAVE_SHAPER_DSP_KERNEL_H_
