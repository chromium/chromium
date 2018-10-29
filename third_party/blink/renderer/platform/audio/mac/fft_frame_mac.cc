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

// Mac OS X - specific FFTFrame implementation

#include "build/build_config.h"

#if defined(OS_MACOSX)

#include "third_party/blink/renderer/platform/audio/fft_frame.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"

namespace blink {

const int kMaxFFTPow2Size = 24;

FFTSetup* FFTFrame::fft_setups_ = nullptr;

// Normal constructor: allocates for a given fftSize
FFTFrame::FFTFrame(unsigned fft_size)
    : real_data_(fft_size), imag_data_(fft_size) {
  fft_size_ = fft_size;
  log2fft_size_ = static_cast<unsigned>(log2(fft_size));

  // We only allow power of two
  DCHECK_EQ(1UL << log2fft_size_, fft_size_);

  // Lazily create and share fftSetup with other frames
  fft_setup_ = FftSetupForSize(fft_size);

  // Setup frame data
  frame_.realp = real_data_.Data();
  frame_.imagp = imag_data_.Data();
}

// Creates a blank/empty frame (interpolate() must later be called)
FFTFrame::FFTFrame() : real_data_(0), imag_data_(0) {
  // Later will be set to correct values when interpolate() is called
  frame_.realp = 0;
  frame_.imagp = 0;

  fft_size_ = 0;
  log2fft_size_ = 0;
}

// Copy constructor
FFTFrame::FFTFrame(const FFTFrame& frame)
    : fft_size_(frame.fft_size_),
      log2fft_size_(frame.log2fft_size_),
      real_data_(frame.fft_size_),
      imag_data_(frame.fft_size_),
      fft_setup_(frame.fft_setup_) {
  // Setup frame data
  frame_.realp = real_data_.Data();
  frame_.imagp = imag_data_.Data();

  // Copy/setup frame data
  unsigned nbytes = sizeof(float) * fft_size_;
  memcpy(RealData(), frame.frame_.realp, nbytes);
  memcpy(ImagData(), frame.frame_.imagp, nbytes);
}

FFTFrame::~FFTFrame() {}

void FFTFrame::DoFFT(const float* data) {
  AudioFloatArray scaled_data(fft_size_);
  // veclib fft returns a result that is twice as large as would be expected.
  // Compensate for that by scaling the input by half so the FFT has the
  // correct scaling.
  float scale = 0.5f;
  vector_math::Vsmul(data, 1, &scale, scaled_data.Data(), 1, fft_size_);

  vDSP_ctoz((DSPComplex*)scaled_data.Data(), 2, &frame_, 1, fft_size_ / 2);
  vDSP_fft_zrip(fft_setup_, &frame_, 1, log2fft_size_, FFT_FORWARD);
}

void FFTFrame::DoInverseFFT(float* data) {
  vDSP_fft_zrip(fft_setup_, &frame_, 1, log2fft_size_, FFT_INVERSE);
  vDSP_ztoc(&frame_, 1, (DSPComplex*)data, 2, fft_size_ / 2);

  // Do final scaling so that x == IFFT(FFT(x))
  float scale = 1.0f / fft_size_;
  vector_math::Vsmul(data, 1, &scale, data, 1, fft_size_);
}

FFTSetup FFTFrame::FftSetupForSize(unsigned fft_size) {
  if (!fft_setups_) {
    fft_setups_ = (FFTSetup*)malloc(sizeof(FFTSetup) * kMaxFFTPow2Size);
    memset(fft_setups_, 0, sizeof(FFTSetup) * kMaxFFTPow2Size);
  }

  int pow2size = static_cast<int>(log2(fft_size));
  DCHECK_LT(pow2size, kMaxFFTPow2Size);
  if (!fft_setups_[pow2size])
    fft_setups_[pow2size] = vDSP_create_fftsetup(pow2size, FFT_RADIX2);

  return fft_setups_[pow2size];
}

void FFTFrame::Initialize() {}

void FFTFrame::Cleanup() {
  if (!fft_setups_)
    return;

  for (int i = 0; i < kMaxFFTPow2Size; ++i) {
    if (fft_setups_[i])
      vDSP_destroy_fftsetup(fft_setups_[i]);
  }

  free(fft_setups_);
  fft_setups_ = nullptr;
}

}  // namespace blink

#endif  // #if defined(OS_MACOSX)
