// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/iir_processor.h"

#include <memory>

#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/iir_filter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

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

IIRDSPKernel::IIRDSPKernel(IIRProcessor* processor)
    : AudioDSPKernel(processor),
      iir_(processor->Feedforward(), processor->Feedback()) {
  tail_time_ =
      iir_.TailTime(processor->SampleRate(), processor->IsFilterStable(),
                    processor->RenderQuantumFrames());
}

void IIRDSPKernel::Process(const float* source,
                           float* destination,
                           uint32_t frames_to_process) {
  DCHECK(source);
  DCHECK(destination);

  iir_.Process(source, destination, frames_to_process);
}

void IIRDSPKernel::GetFrequencyResponse(int n_frequencies,
                                        const float* frequency_hz,
                                        float* mag_response,
                                        float* phase_response) {
  DCHECK_GE(n_frequencies, 0);
  DCHECK(frequency_hz);
  DCHECK(mag_response);
  DCHECK(phase_response);

  Vector<float> frequency(n_frequencies);

  double nyquist = Nyquist();

  // Convert from frequency in Hz to normalized frequency (0 -> 1),
  // with 1 equal to the Nyquist frequency.
  for (int k = 0; k < n_frequencies; ++k) {
    frequency[k] = frequency_hz[k] / nyquist;
  }

  iir_.GetFrequencyResponse(n_frequencies, frequency.data(), mag_response,
                            phase_response);
}

bool IIRDSPKernel::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

double IIRDSPKernel::TailTime() const {
  return tail_time_;
}

double IIRDSPKernel::LatencyTime() const {
  return 0;
}

IIRProcessor::IIRProcessor(float sample_rate,
                           uint32_t number_of_channels,
                           unsigned render_quantum_frames,
                           const Vector<double>& feedforward_coef,
                           const Vector<double>& feedback_coef,
                           bool is_filter_stable)
    : AudioDSPKernelProcessor(sample_rate,
                              number_of_channels,
                              render_quantum_frames),
      is_filter_stable_(is_filter_stable) {
  unsigned feedback_length = feedback_coef.size();
  unsigned feedforward_length = feedforward_coef.size();
  DCHECK_GT(feedback_length, 0u);
  DCHECK_GT(feedforward_length, 0u);

  feedforward_.Allocate(feedforward_length);
  feedback_.Allocate(feedback_length);
  feedforward_.CopyToRange(feedforward_coef.data(), 0, feedforward_length);
  feedback_.CopyToRange(feedback_coef.data(), 0, feedback_length);

  // Need to scale the feedback and feedforward coefficients appropriately.
  // (It's up to the caller to ensure feedbackCoef[0] is not 0.)
  DCHECK_NE(feedback_coef[0], 0);

  if (feedback_coef[0] != 1) {
    // The provided filter is:
    //
    //   a[0]*y(n) + a[1]*y(n-1) + ... = b[0]*x(n) + b[1]*x(n-1) + ...
    //
    // We want the leading coefficient of y(n) to be 1:
    //
    //   y(n) + a[1]/a[0]*y(n-1) + ... = b[0]/a[0]*x(n) + b[1]/a[0]*x(n-1) + ...
    //
    // Thus, the feedback and feedforward coefficients need to be scaled by
    // 1/a[0].
    float scale = feedback_coef[0];
    for (unsigned k = 1; k < feedback_length; ++k) {
      feedback_[k] /= scale;
    }

    for (unsigned k = 0; k < feedforward_length; ++k) {
      feedforward_[k] /= scale;
    }

    // The IIRFilter checks to make sure this coefficient is 1, so make it so.
    feedback_[0] = 1;
  }

  response_kernel_ = std::make_unique<IIRDSPKernel>(this);
}

IIRProcessor::~IIRProcessor() {
  if (IsInitialized()) {
    Uninitialize();
  }
}

std::unique_ptr<AudioDSPKernel> IIRProcessor::CreateKernel() {
  return std::make_unique<IIRDSPKernel>(this);
}

void IIRProcessor::GetFrequencyResponse(int n_frequencies,
                                        const float* frequency_hz,
                                        float* mag_response,
                                        float* phase_response) {
  response_kernel_->GetFrequencyResponse(n_frequencies, frequency_hz,
                                         mag_response, phase_response);
}

}  // namespace blink
