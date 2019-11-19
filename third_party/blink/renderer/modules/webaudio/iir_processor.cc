// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/iir_processor.h"

#include <memory>
#include "third_party/blink/renderer/modules/webaudio/iir_dsp_kernel.h"

namespace blink {

IIRProcessor::IIRProcessor(float sample_rate,
                           uint32_t number_of_channels,
                           const Vector<double>& feedforward_coef,
                           const Vector<double>& feedback_coef,
                           bool is_filter_stable)
    : AudioDSPKernelProcessor(sample_rate, number_of_channels),
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
    for (unsigned k = 1; k < feedback_length; ++k)
      feedback_[k] /= scale;

    for (unsigned k = 0; k < feedforward_length; ++k)
      feedforward_[k] /= scale;

    // The IIRFilter checks to make sure this coefficient is 1, so make it so.
    feedback_[0] = 1;
  }

  response_kernel_ = std::make_unique<IIRDSPKernel>(this);
}

IIRProcessor::~IIRProcessor() {
  if (IsInitialized())
    Uninitialize();
}

std::unique_ptr<AudioDSPKernel> IIRProcessor::CreateKernel() {
  return std::make_unique<IIRDSPKernel>(this);
}

void IIRProcessor::Process(const AudioBus* source,
                           AudioBus* destination,
                           uint32_t frames_to_process) {
  if (!IsInitialized()) {
    destination->Zero();
    return;
  }

  // For each channel of our input, process using the corresponding IIRDSPKernel
  // into the output channel.
  for (unsigned i = 0; i < kernels_.size(); ++i)
    kernels_[i]->Process(source->Channel(i)->Data(),
                         destination->Channel(i)->MutableData(),
                         frames_to_process);
}

void IIRProcessor::GetFrequencyResponse(int n_frequencies,
                                        const float* frequency_hz,
                                        float* mag_response,
                                        float* phase_response) {
  response_kernel_->GetFrequencyResponse(n_frequencies, frequency_hz,
                                         mag_response, phase_response);
}

}  // namespace blink
