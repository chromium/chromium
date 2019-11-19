// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_DSP_KERNEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_DSP_KERNEL_H_

#include "third_party/blink/renderer/modules/webaudio/iir_processor.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/iir_filter.h"

namespace blink {

class IIRProcessor;

class IIRDSPKernel final : public AudioDSPKernel {
 public:
  explicit IIRDSPKernel(IIRProcessor*);

  // AudioDSPKernel
  void Process(const float* source,
               float* dest,
               uint32_t frames_to_process) override;
  void Reset() override { iir_.Reset(); }

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void GetFrequencyResponse(int n_frequencies,
                            const float* frequency_hz,
                            float* mag_response,
                            float* phase_response);

  double TailTime() const override;
  double LatencyTime() const override;
  bool RequiresTailProcessing() const final;

 protected:
  IIRFilter iir_;

 private:
  double tail_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_DSP_KERNEL_H_
