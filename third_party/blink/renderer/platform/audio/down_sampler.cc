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
#include "third_party/fdlibm/ieee754.h"

namespace blink {

namespace {

constexpr size_t kKernelSize = 256;

// Computes ideal band-limited half-band filter coefficients.
// In other words, filter out all frequencies higher than 0.25 * Nyquist.
std::unique_ptr<AudioFloatArray> MakeReducedKernel(int size) {
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
    double sinc = !s ? 1.0 : fdlibm::sin(s) / s;
    sinc *= sinc_scale_factor;

    // Compute Blackman window, matching the offset of the sinc().
    double x = static_cast<double>(i) / n;
    double window = a0 - a1 * fdlibm::cos(kTwoPiDouble * x) +
                    a2 * fdlibm::cos(kTwoPiDouble * 2.0 * x);

    // Window the sinc() function.
    // Then store only the odd terms in the kernel.
    // In a sense, this is shifting forward in time by one sample-frame at the
    // destination sample-rate.
    (*reduced_kernel)[(i - 1) / 2] = sinc * window;
  }

  return reduced_kernel;
}

}  // namespace

DownSampler::DownSampler(unsigned input_block_size)
    : input_block_size_(input_block_size),
      temp_buffer_(input_block_size / 2),
      input_buffer_(kKernelSize / 2 + input_block_size) {
  std::unique_ptr<AudioFloatArray> convolution_kernel =
      MakeReducedKernel(kKernelSize);
  if (input_block_size_ <= kKernelSize) {
    direct_convolver_ = std::make_unique<DirectConvolver>(
        input_block_size_ / 2, std::move(convolution_kernel));
  } else {
    simple_fft_convolver_ = std::make_unique<SimpleFFTConvolver>(
        input_block_size_ / 2, *convolution_kernel);
  }
}

void DownSampler::Process(base::span<const float> source,
                          base::span<float> dest) {
  const size_t source_frames_to_process = source.size();
  DCHECK_EQ(source_frames_to_process, input_block_size_);

  const size_t dest_frames_to_process = source_frames_to_process / 2;

  DCHECK_EQ(dest.size(), dest_frames_to_process);
  DCHECK_EQ(dest_frames_to_process, temp_buffer_.size());

  size_t half_kernel_size = kKernelSize / 2;

  DCHECK_EQ(input_buffer_.size(), half_kernel_size + source_frames_to_process);

  base::span<float> input_buffer_span = input_buffer_.as_span();

  // Copy source samples to the end of input buffer.
  input_buffer_span.subspan(half_kernel_size, source_frames_to_process)
      .copy_from(source);

  // Copy the odd sample-frames from source, delayed by one sample-frame
  // (destination sample-rate) to match shifting forward in time in
  // m_reducedKernel.
  base::span<float> odd_samples = temp_buffer_.as_span();
  base::span<const float> delayed_input =
      input_buffer_span.subspan(half_kernel_size - 1, source_frames_to_process);
  for (size_t i = 0; i < dest_frames_to_process; ++i) {
    odd_samples[i] = delayed_input[i * 2];
  }

  // Actually process odd_samples with m_reducedKernel for efficiency.
  // The theoretical kernel is double this size with 0 values for even terms
  // (except center).
  if (direct_convolver_) {
    direct_convolver_->Process(odd_samples, dest);
  } else {
    simple_fft_convolver_->Process(odd_samples, dest);
  }

  // Now, account for the 0.5 term right in the middle of the kernel.
  // This amounts to a delay-line of length `half_kernel_size` (at the source
  // sample-rate), scaled by 0.5.

  // Sum into the destination.
  base::span<const float> delayed_half =
      input_buffer_span.first(source_frames_to_process);
  for (size_t i = 0; i < dest_frames_to_process; ++i) {
    dest[i] += 0.5f * delayed_half[i * 2];
  }

  // Shift the history buffer.
  input_buffer_span.first(half_kernel_size)
      .copy_from(input_buffer_span.subspan(source_frames_to_process,
                                           half_kernel_size));
}

void DownSampler::Reset() {
  if (direct_convolver_) {
    direct_convolver_->Reset();
  }
  if (simple_fft_convolver_) {
    simple_fft_convolver_->Reset();
  }
  input_buffer_.Zero();
}

size_t DownSampler::LatencyFrames() const {
  const size_t convolution_kernel_size =
      direct_convolver_ ? direct_convolver_->ConvolutionKernelSize()
                        : simple_fft_convolver_->ConvolutionKernelSize();
  // Divide by two since this is a linear phase kernel and the delay is at the
  // center of the kernel.
  return convolution_kernel_size / 2;
}

}  // namespace blink
