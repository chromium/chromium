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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_FFT_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_FFT_FRAME_H_

#include <memory>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/rustfft_ffi.rs.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace blink {

// Defines the interface for an "FFT frame", an object which is able to perform
// a forward and reverse FFT, internally storing the resultant frequency-domain
// data.

class PLATFORM_EXPORT FFTFrame final {
  USING_FAST_MALLOC(FFTFrame);

 public:
  explicit FFTFrame(unsigned fft_size);
  FFTFrame() = delete;
  FFTFrame(const FFTFrame&) = delete;
  FFTFrame& operator=(const FFTFrame&) = delete;
  ~FFTFrame() = default;

  // Returns the smallest and largest supported FFT lengths.
  static constexpr unsigned MinFFTSize() { return 2; }
  static constexpr unsigned MaxFFTSize() { return 1u << 30; }

  // Compute the FFT of |data|, storing the resulting FFT in |real_data_| and
  // |imag_data_|.  |data| MUST have size at least |fft_size_| elements.
  void DoFFT(base::span<const float> data);

  // Compute the inverse FFT using the FFT data in |real_data_| and
  // |imag_data_|.  The inverse is saved in |data|.  |data| MUST have size at
  // least |fft_size_| elements.
  void DoInverseFFT(base::span<float> data);

  AudioFloatArray& RealData() { return real_data_; }
  const AudioFloatArray& RealData() const { return real_data_; }
  AudioFloatArray& ImagData() { return imag_data_; }
  const AudioFloatArray& ImagData() const { return imag_data_; }

  unsigned FftSize() const { return fft_size_; }

  // Interpolates from frame1 -> frame2 as x goes from 0.0 -> 1.0
  static std::unique_ptr<FFTFrame> CreateInterpolatedFrame(
      const FFTFrame& frame1,
      const FFTFrame& frame2,
      double x);
  // zero-padding with dataSize <= fftSize
  void DoPaddedFFT(base::span<const float> data);
  double ExtractAverageGroupDelay();
  void AddConstantGroupDelay(double sample_frame_delay);
  // multiplies ourself with frame : effectively operator*=()
  void Multiply(const FFTFrame&);
  // Scale the FFT data by the given scaling factor
  void ScaleFFT(float factor);

 private:
  void InterpolateFrequencyComponents(const FFTFrame& frame1,
                                      const FFTFrame& frame2,
                                      double x);

  unsigned fft_size_ = 0;

  // These two arrays contain the transformed data.  Instead of a single array
  // of complex numbers, we split the complex data into an array of the real
  // part and the imaginary part.
  //
  // Let the forward transform, X[k], of the real signal x[n] be defined by
  //
  //   X[k] = sum(x[n]*W^(k*n)) for n = 0 to N-1
  //
  // where W = exp(-2*pi*i/N), and N is the FFT size.
  //
  // Since x[n] is assumed to be real, X[k] has complex conjugate symmetry with
  // X[N-k] = conj(X[k]).  Thus, we only need to keep X[k] for k = 0 to N/2.
  // But since X[0] is purely real and X[N/2] is also purely real, so we could
  // place the real part of X[N/2] in the imaginary part of X[0].  Thus
  // for k = 1 to N/2:
  //
  //   real_data[k] = Re(X[k])
  //   imag_data[k] = Im(X[k])
  //
  // and
  //
  //   real_data[0] = Re(X[0]);
  //   imag_data[0] = Re(X[N/2])
  //
  // The routine |DoFFT| must produce transformed data in this format, and the
  // routine |DoInverseFFT| must expect transformed data in this format.
  AudioFloatArray real_data_;
  AudioFloatArray imag_data_;

  rust::Box<blink::rust_fft::RustFft> rust_fft_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_FFT_FRAME_H_
