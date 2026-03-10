/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/direct_convolver.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

namespace {
using vector_math::Conv;
using vector_math::PrepareFilterForConv;
}  // namespace

DirectConvolver::DirectConvolver(
    size_t input_block_size,
    std::unique_ptr<AudioFloatArray> convolution_kernel)
    : input_block_size_(input_block_size),
      buffer_(input_block_size * 2),
      convolution_kernel_(std::move(convolution_kernel)) {
  size_t kernel_size = ConvolutionKernelSize();
  PrepareFilterForConv(
      convolution_kernel_->as_span().subspan(kernel_size - 1).data(), -1,
      kernel_size, &prepared_convolution_kernel_);
}

void DirectConvolver::Process(base::span<const float> source,
                              base::span<float> destination) {
  const uint32_t frames_to_process = destination.size();
  DCHECK_EQ(frames_to_process, input_block_size_);

  const size_t kernel_size = ConvolutionKernelSize();
  DCHECK_LE(kernel_size, input_block_size_);

  DCHECK(buffer_.Data());

  // Copy samples to 2nd half of input buffer.
  buffer_.as_span()
      .subspan(input_block_size_, frames_to_process)
      .copy_from(source.first(frames_to_process));

  Conv(buffer_.as_span().subspan(input_block_size_ - kernel_size + 1).data(), 1,
       convolution_kernel_->as_span().subspan(kernel_size - 1).data(), -1,
       destination.data(), 1, frames_to_process, kernel_size,
       &prepared_convolution_kernel_);

  // Copy 2nd half of input buffer to 1st half.
  buffer_.as_span()
      .first(frames_to_process)
      .copy_from(
          buffer_.as_span().subspan(input_block_size_, frames_to_process));
}

void DirectConvolver::Reset() {
  buffer_.Zero();
}

}  // namespace blink
