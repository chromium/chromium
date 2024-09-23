// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/iir_dsp_kernel.h"

#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

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

}  // namespace blink
