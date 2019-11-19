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

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

#if defined(OS_MACOSX)
#include <Accelerate/Accelerate.h>
#endif

#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_MACOSX)
#include <emmintrin.h>
#endif

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
  PrepareFilterForConv(convolution_kernel_->Data() + kernel_size - 1, -1,
                       kernel_size, &prepared_convolution_kernel_);
}

void DirectConvolver::Process(const float* source_p,
                              float* dest_p,
                              uint32_t frames_to_process) {
  DCHECK_EQ(frames_to_process, input_block_size_);

  size_t kernel_size = ConvolutionKernelSize();
  DCHECK_LE(kernel_size, input_block_size_);

  float* kernel_p = convolution_kernel_->Data();

  DCHECK(kernel_p);
  DCHECK(source_p);
  DCHECK(dest_p);
  DCHECK(buffer_.Data());

  float* input_p = buffer_.Data() + input_block_size_;

  // Copy samples to 2nd half of input buffer.
  memcpy(input_p, source_p, sizeof(float) * frames_to_process);

  Conv(input_p - kernel_size + 1, 1, kernel_p + kernel_size - 1, -1, dest_p, 1,
       frames_to_process, kernel_size, &prepared_convolution_kernel_);

  // Copy 2nd half of input buffer to 1st half.
  memcpy(buffer_.Data(), input_p, sizeof(float) * frames_to_process);
}

void DirectConvolver::Reset() {
  buffer_.Zero();
}

}  // namespace blink
