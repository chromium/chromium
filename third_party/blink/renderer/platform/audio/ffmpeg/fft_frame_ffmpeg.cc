/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

// FFTFrame implementation using FFmpeg's RDFT algorithm,
// suitable for use on Windows and Linux.

#if defined(WTF_USE_WEBAUDIO_FFMPEG)

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

#include "third_party/blink/renderer/platform/audio/vector_math.h"

extern "C" {
#include <libavcodec/avfft.h>
}

#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// Max FFT size for FFMPEG.  WebAudio currently only uses FFTs up to size 15
// (2^15 points).
const int kMaxFFTPow2Size = 16;

// Min FFT size for FFMPEG.
const int kMinFFTPow2Size = 2;

// Normal constructor: allocates for a given fftSize.
FFTFrame::FFTFrame(unsigned fft_size)
    : fft_size_(fft_size),
      log2fft_size_(static_cast<unsigned>(log2(fft_size))),
      real_data_(fft_size / 2),
      imag_data_(fft_size / 2),
      forward_context_(nullptr),
      inverse_context_(nullptr),
      complex_data_(fft_size) {
  // We only allow power of two.
  DCHECK_EQ(1UL << log2fft_size_, fft_size_);

  forward_context_ = ContextForSize(fft_size, DFT_R2C);
  inverse_context_ = ContextForSize(fft_size, IDFT_C2R);
}

// Creates a blank/empty frame (interpolate() must later be called).
FFTFrame::FFTFrame()
    : fft_size_(0),
      log2fft_size_(0),
      forward_context_(nullptr),
      inverse_context_(nullptr) {}

// Copy constructor.
FFTFrame::FFTFrame(const FFTFrame& frame)
    : fft_size_(frame.fft_size_),
      log2fft_size_(frame.log2fft_size_),
      real_data_(frame.fft_size_ / 2),
      imag_data_(frame.fft_size_ / 2),
      forward_context_(nullptr),
      inverse_context_(nullptr),
      complex_data_(frame.fft_size_) {
  forward_context_ = ContextForSize(fft_size_, DFT_R2C);
  inverse_context_ = ContextForSize(fft_size_, IDFT_C2R);

  // Copy/setup frame data.
  unsigned nbytes = sizeof(float) * (fft_size_ / 2);
  memcpy(RealData(), frame.RealData(), nbytes);
  memcpy(ImagData(), frame.ImagData(), nbytes);
}

int FFTFrame::MinFFTSize() {
  return 1 << kMinFFTPow2Size;
}

int FFTFrame::MaxFFTSize() {
  return 1 << kMaxFFTPow2Size;
}

void FFTFrame::Initialize(float sample_rate) {}

void FFTFrame::Cleanup() {}

FFTFrame::~FFTFrame() {
  av_rdft_end(forward_context_);
  av_rdft_end(inverse_context_);
}

void FFTFrame::DoFFT(const float* data) {
  // Copy since processing is in-place.
  float* p = complex_data_.Data();
  memcpy(p, data, sizeof(float) * fft_size_);

  // Compute Forward transform.
  av_rdft_calc(forward_context_, p);

  // De-interleave to separate real and complex arrays.
  int len = fft_size_ / 2;

  float* real = real_data_.Data();
  float* imag = imag_data_.Data();
  for (int i = 0; i < len; ++i) {
    int base_complex_index = 2 * i;
    // m_realData[0] is the DC component and m_imagData[0] is the nyquist
    // component since the interleaved complex data is packed.
    real[i] = p[base_complex_index];
    imag[i] = p[base_complex_index + 1];
  }
}

void FFTFrame::DoInverseFFT(float* data) {
  // Prepare interleaved data.
  float* interleaved_data = GetUpToDateComplexData();

  // Compute inverse transform.
  av_rdft_calc(inverse_context_, interleaved_data);

  // Scale so that a forward then inverse FFT yields exactly the original data.
  // For some reason av_rdft_calc above returns values that are half of what I
  // expect. Hence make the scale factor
  // twice as large to compensate for that.
  const float scale = 2.0 / fft_size_;
  vector_math::Vsmul(interleaved_data, 1, &scale, data, 1, fft_size_);
}

float* FFTFrame::GetUpToDateComplexData() {
  // FIXME: if we can't completely get rid of this method, SSE
  // optimization could be considered if it shows up hot on profiles.
  int len = fft_size_ / 2;
  const float* real = real_data_.Data();
  const float* imag = imag_data_.Data();
  float* c = complex_data_.Data();
  for (int i = 0; i < len; ++i) {
    int base_complex_index = 2 * i;
    c[base_complex_index] = real[i];
    c[base_complex_index + 1] = imag[i];
  }
  return c;
}

RDFTContext* FFTFrame::ContextForSize(unsigned fft_size, int trans) {
  // FIXME: This is non-optimal. Ideally, we'd like to share the contexts for
  // FFTFrames of the same size.  But FFmpeg's RDFT uses a scratch buffer
  // inside the context and so they are not thread-safe.  We could improve this
  // by sharing the FFTFrames on a per-thread basis.
  DCHECK(fft_size);
  int pow2size = static_cast<int>(log2(fft_size));
  DCHECK_LT(pow2size, kMaxFFTPow2Size);

  RDFTContext* context = av_rdft_init(pow2size, (RDFTransformType)trans);
  return context;
}

}  // namespace blink

#endif  // defined(WTF_USE_WEBAUDIO_FFMPEG)
