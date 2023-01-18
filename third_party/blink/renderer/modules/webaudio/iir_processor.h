// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_PROCESSOR_H_

#include <memory>

#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel_processor.h"
#include "third_party/blink/renderer/platform/audio/iir_filter.h"

namespace blink {

class IIRDSPKernel;

class IIRProcessor final : public AudioDSPKernelProcessor {
 public:
  IIRProcessor(float sample_rate,
               uint32_t number_of_channels,
               unsigned render_quantum_frames,
               const Vector<double>& feedforward_coef,
               const Vector<double>& feedback_coef,
               bool is_filter_stable);
  ~IIRProcessor() override;

  std::unique_ptr<AudioDSPKernel> CreateKernel() override;

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void GetFrequencyResponse(int n_frequencies,
                            const float* frequency_hz,
                            float* mag_response,
                            float* phase_response);

  AudioDoubleArray* Feedback() { return &feedback_; }
  AudioDoubleArray* Feedforward() { return &feedforward_; }
  bool IsFilterStable() const { return is_filter_stable_; }

 private:
  // The feedback and feedforward filter coefficients for the IIR filter.
  AudioDoubleArray feedback_;
  AudioDoubleArray feedforward_;
  bool is_filter_stable_;

  // This holds the IIR kernel for computing the frequency response.
  std::unique_ptr<IIRDSPKernel> response_kernel_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_PROCESSOR_H_
