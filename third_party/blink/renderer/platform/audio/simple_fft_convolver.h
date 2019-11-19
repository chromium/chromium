// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_SIMPLE_FFT_CONVOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_SIMPLE_FFT_CONVOLVER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/fft_frame.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// The SimpleFFTConvolver does an FFT convolution. It differs from
// the FFTConvolver in that it restricts the maximum size of
// |convolution_kernel| to |input_block_size|. This restriction allows it to do
// an FFT on every Process call. Therefore, the processing delay of
// the SimpleFFTConvolver is the same as that of the DirectConvolver and thus
// smaller than that of the FFTConvolver.
class PLATFORM_EXPORT SimpleFFTConvolver {
  USING_FAST_MALLOC(SimpleFFTConvolver);

 public:
  SimpleFFTConvolver(
      size_t input_block_size,
      const std::unique_ptr<AudioFloatArray>& convolution_kernel);

  void Process(const float* source_p,
               float* dest_p,
               uint32_t frames_to_process);

  void Reset();

  size_t ConvolutionKernelSize() const { return convolution_kernel_size_; }

 private:
  size_t FftSize() const { return frame_.FftSize(); }

  size_t convolution_kernel_size_;
  FFTFrame fft_kernel_;
  FFTFrame frame_;

  // Buffer input until we get fftSize / 2 samples then do an FFT
  AudioFloatArray input_buffer_;

  // Stores output which we read a little at a time
  AudioFloatArray output_buffer_;

  // Saves the 2nd half of the FFT buffer, so we can do an overlap-add with the
  // 1st half of the next one
  AudioFloatArray last_overlap_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SimpleFFTConvolver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_SIMPLE_FFT_CONVOLVER_H_
