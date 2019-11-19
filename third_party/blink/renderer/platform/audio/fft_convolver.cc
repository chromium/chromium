/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/audio/fft_convolver.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

FFTConvolver::FFTConvolver(size_t fft_size)
    : frame_(fft_size),
      read_write_index_(0),
      input_buffer_(fft_size),  // 2nd half of buffer is always zeroed
      output_buffer_(fft_size),
      last_overlap_buffer_(fft_size / 2) {}

void FFTConvolver::Process(const FFTFrame* fft_kernel,
                           const float* source_p,
                           float* dest_p,
                           uint32_t frames_to_process) {
  size_t half_size = FftSize() / 2;

  // framesToProcess must be an exact multiple of halfSize,
  // or halfSize is a multiple of framesToProcess when halfSize >
  // framesToProcess.
  bool is_good =
      !(half_size % frames_to_process && frames_to_process % half_size);
  DCHECK(is_good);

  size_t number_of_divisions =
      half_size <= frames_to_process ? (frames_to_process / half_size) : 1;
  size_t division_size =
      number_of_divisions == 1 ? frames_to_process : half_size;

  for (size_t i = 0; i < number_of_divisions;
       ++i, source_p += division_size, dest_p += division_size) {
    // Copy samples to input buffer (note contraint above!)
    float* input_p = input_buffer_.Data();

    DCHECK(source_p);
    DCHECK(input_p);
    DCHECK_LE(read_write_index_ + division_size, input_buffer_.size());

    memcpy(input_p + read_write_index_, source_p,
           sizeof(float) * division_size);

    // Copy samples from output buffer
    float* output_p = output_buffer_.Data();

    DCHECK(dest_p);
    DCHECK(output_p);
    DCHECK_LE(read_write_index_ + division_size, output_buffer_.size());

    memcpy(dest_p, output_p + read_write_index_, sizeof(float) * division_size);
    read_write_index_ += division_size;

    // Check if it's time to perform the next FFT
    if (read_write_index_ == half_size) {
      // The input buffer is now filled (get frequency-domain version)
      frame_.DoFFT(input_buffer_.Data());
      frame_.Multiply(*fft_kernel);
      frame_.DoInverseFFT(output_buffer_.Data());

      // Overlap-add 1st half from previous time
      vector_math::Vadd(output_buffer_.Data(), 1, last_overlap_buffer_.Data(),
                        1, output_buffer_.Data(), 1, half_size);

      // Finally, save 2nd half of result
      DCHECK_EQ(output_buffer_.size(), 2 * half_size);
      DCHECK_EQ(last_overlap_buffer_.size(), half_size);

      memcpy(last_overlap_buffer_.Data(), output_buffer_.Data() + half_size,
             sizeof(float) * half_size);

      // Reset index back to start for next time
      read_write_index_ = 0;
    }
  }
}

void FFTConvolver::Reset() {
  last_overlap_buffer_.Zero();
  read_write_index_ = 0;
}

}  // namespace blink
