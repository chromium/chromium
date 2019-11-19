// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/simple_fft_convolver.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

SimpleFFTConvolver::SimpleFFTConvolver(
    size_t input_block_size,
    const std::unique_ptr<AudioFloatArray>& convolution_kernel)
    : convolution_kernel_size_(convolution_kernel->size()),
      fft_kernel_(2 * input_block_size),
      frame_(2 * input_block_size),
      input_buffer_(2 *
                    input_block_size),  // 2nd half of buffer is always zeroed
      output_buffer_(2 * input_block_size),
      last_overlap_buffer_(input_block_size) {
  DCHECK_LE(convolution_kernel_size_, FftSize() / 2);
  // Do padded FFT to get frequency-domain version of the convolution kernel.
  // This FFT and caching is done once in here so that it does not have to be
  // done repeatedly in |Process|.
  fft_kernel_.DoPaddedFFT(convolution_kernel->Data(), convolution_kernel_size_);
}

void SimpleFFTConvolver::Process(const float* source_p,
                                 float* dest_p,
                                 uint32_t frames_to_process) {
  size_t half_size = FftSize() / 2;

  // frames_to_process must be exactly half_size.
  DCHECK(source_p);
  DCHECK(dest_p);
  DCHECK_EQ(frames_to_process, half_size);

  // Do padded FFT (get frequency-domain version) by copying samples to the 1st
  // half of the input buffer (the second half is always zero), multiply in
  // frequency-domain and do inverse FFT to get output samples.
  input_buffer_.CopyToRange(source_p, 0, half_size);
  frame_.DoFFT(input_buffer_.Data());
  frame_.Multiply(fft_kernel_);
  frame_.DoInverseFFT(output_buffer_.Data());

  // Overlap-add 1st half with 2nd half from previous time and write
  // to destination.
  vector_math::Vadd(output_buffer_.Data(), 1, last_overlap_buffer_.Data(), 1,
                    dest_p, 1, half_size);

  // Finally, save 2nd half for the next time.
  last_overlap_buffer_.CopyToRange(output_buffer_.Data() + half_size, 0,
                                   half_size);
}

void SimpleFFTConvolver::Reset() {
  last_overlap_buffer_.Zero();
}

}  // namespace blink
