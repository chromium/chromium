/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/down_sampler.h"

#include <memory>

#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

// Computes ideal band-limited half-band filter coefficients.
// In other words, filter out all frequencies higher than 0.25 * Nyquist.
std::unique_ptr<AudioFloatArray> MakeReducedKernel(size_t size) {
  auto reduced_kernel = std::make_unique<AudioFloatArray>(size / 2);

  // Blackman window parameters.
  double alpha = 0.16;
  double a0 = 0.5 * (1.0 - alpha);
  double a1 = 0.5;
  double a2 = 0.5 * alpha;

  int n = size;
  int half_size = n / 2;

  // Half-band filter.
  double sinc_scale_factor = 0.5;

  // Compute only the odd terms because the even ones are zero, except right in
  // the middle at halfSize, which is 0.5 and we'll handle specially during
  // processing after doing the main convolution using m_reducedKernel.
  for (int i = 1; i < n; i += 2) {
    // Compute the sinc() with offset.
    double s = sinc_scale_factor * kPiDouble * (i - half_size);
    double sinc = !s ? 1.0 : sin(s) / s;
    sinc *= sinc_scale_factor;

    // Compute Blackman window, matching the offset of the sinc().
    double x = static_cast<double>(i) / n;
    double window =
        a0 - a1 * cos(kTwoPiDouble * x) + a2 * cos(kTwoPiDouble * 2.0 * x);

    // Window the sinc() function.
    // Then store only the odd terms in the kernel.
    // In a sense, this is shifting forward in time by one sample-frame at the
    // destination sample-rate.
    (*reduced_kernel)[(i - 1) / 2] = sinc * window;
  }

  return reduced_kernel;
}

}  // namespace

DownSampler::DownSampler(size_t input_block_size)
    : input_block_size_(input_block_size),
      convolver_(input_block_size / 2,  // runs at 1/2 source sample-rate
                 MakeReducedKernel(kDefaultKernelSize)),
      temp_buffer_(input_block_size / 2),
      input_buffer_(input_block_size * 2) {}

void DownSampler::Process(const float* source_p,
                          float* dest_p,
                          size_t source_frames_to_process) {
  DCHECK_EQ(source_frames_to_process, input_block_size_);

  size_t dest_frames_to_process = source_frames_to_process / 2;

  DCHECK_EQ(dest_frames_to_process, temp_buffer_.size());
  DCHECK_EQ(convolver_.ConvolutionKernelSize(),
            static_cast<unsigned>(kDefaultKernelSize / 2));

  size_t half_size = kDefaultKernelSize / 2;

  // Copy source samples to 2nd half of input buffer.
  DCHECK_EQ(input_buffer_.size(), source_frames_to_process * 2);
  DCHECK_LE(half_size, source_frames_to_process);

  float* input_p = input_buffer_.Data() + source_frames_to_process;
  memcpy(input_p, source_p, sizeof(float) * source_frames_to_process);

  // Copy the odd sample-frames from sourceP, delayed by one sample-frame
  // (destination sample-rate) to match shifting forward in time in
  // m_reducedKernel.
  float* odd_samples_p = temp_buffer_.Data();
  for (unsigned i = 0; i < dest_frames_to_process; ++i)
    odd_samples_p[i] = *((input_p - 1) + i * 2);

  // Actually process oddSamplesP with m_reducedKernel for efficiency.
  // The theoretical kernel is double this size with 0 values for even terms
  // (except center).
  convolver_.Process(odd_samples_p, dest_p, dest_frames_to_process);

  // Now, account for the 0.5 term right in the middle of the kernel.
  // This amounts to a delay-line of length halfSize (at the source
  // sample-rate), scaled by 0.5.

  // Sum into the destination.
  for (unsigned i = 0; i < dest_frames_to_process; ++i)
    dest_p[i] += 0.5 * *((input_p - half_size) + i * 2);

  // Copy 2nd half of input buffer to 1st half.
  memcpy(input_buffer_.Data(), input_p,
         sizeof(float) * source_frames_to_process);
}

void DownSampler::Reset() {
  convolver_.Reset();
  input_buffer_.Zero();
}

size_t DownSampler::LatencyFrames() const {
  // Divide by two since this is a linear phase kernel and the delay is at the
  // center of the kernel.
  return convolver_.ConvolutionKernelSize() / 2;
}

}  // namespace blink
