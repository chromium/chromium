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

#if BUILDFLAG(IS_MAC) && !defined(WTF_USE_WEBAUDIO_PFFFT)

#include "third_party/blink/renderer/platform/audio/fft_frame.h"
#include "third_party/blink/renderer/platform/audio/hrtf_panner.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

const int kMaxFFTPow2Size = 24;
const int kMinFFTPow2Size = 2;

FFTFrame::FFTSetupDatum::FFTSetupDatum(unsigned log2fft_size) {
  // We only need power-of-two sized FFTS, so FFT_RADIX2.
  setup_ = vDSP_create_fftsetup(log2fft_size, FFT_RADIX2);
  DCHECK(setup_);
}

FFTFrame::FFTSetupDatum::~FFTSetupDatum() {
  DCHECK(setup_);

  vDSP_destroy_fftsetup(setup_);
}

Vector<std::unique_ptr<FFTFrame::FFTSetupDatum>>& FFTFrame::FFTSetups() {
  // TODO(rtoy): Let this bake for a bit and then remove the assertions after
  // we're confident the first call is from the main thread.
  static bool first_call = true;

  if (first_call) {
    // Make sure we construct the fft_setups vector below on the main thread.
    // Once constructed, we can access it from any thread.
    DCHECK(IsMainThread());
    first_call = false;
  }

  // A vector to hold all of the possible FFT setups we need.  The setups are
  // initialized lazily.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Vector<std::unique_ptr<FFTSetupDatum>>,
                                  fft_setups, (kMaxFFTPow2Size));

  return fft_setups;
}

void FFTFrame::InitializeFFTSetupForSize(wtf_size_t log2fft_size) {
  auto& setup = FFTSetups();

  if (!setup[log2fft_size]) {
    // Make sure allocation of a new setup only occurs on the main thread so we
    // don't have a race condition with multiple threads trying to write to the
    // same element of the vector.
    DCHECK(IsMainThread());

    setup[log2fft_size] = std::make_unique<FFTSetupDatum>(log2fft_size);
  }
}

// Normal constructor: allocates for a given fftSize
FFTFrame::FFTFrame(unsigned fft_size)
    : fft_size_(fft_size),
      log2fft_size_(static_cast<unsigned>(log2(fft_size))),
      real_data_(fft_size),
      imag_data_(fft_size) {
  // We only allow power of two
  DCHECK_EQ(1UL << log2fft_size_, fft_size_);

  // Initialize the PFFFT_Setup object here so that it will be ready when we
  // compute FFTs.
  InitializeFFTSetupForSize(log2fft_size_);

  // Get a copy of the setup from the table.
  fft_setup_ = FftSetupForSize(log2fft_size_);

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
  memcpy(RealData().Data(), frame.frame_.realp, nbytes);
  memcpy(ImagData().Data(), frame.frame_.imagp, nbytes);
}

FFTFrame::~FFTFrame() {}

void FFTFrame::DoFFT(const float* data) {
  vDSP_ctoz((DSPComplex*)data, 2, &frame_, 1, fft_size_ / 2);
  vDSP_fft_zrip(fft_setup_, &frame_, 1, log2fft_size_, FFT_FORWARD);

  // vDSP_FFT_zrip returns a result that is twice as large as would be
  // expected.  (See
  // https://developer.apple.com/documentation/accelerate/1450150-vdsp_fft_zrip)
  // Compensate for that by scaling the input by half so the FFT has
  // the correct scaling.
  float scale = 0.5f;

  vector_math::Vsmul(frame_.realp, 1, &scale, frame_.realp, 1, fft_size_ / 2);
  vector_math::Vsmul(frame_.imagp, 1, &scale, frame_.imagp, 1, fft_size_ / 2);
}

void FFTFrame::DoInverseFFT(float* data) {
  vDSP_fft_zrip(fft_setup_, &frame_, 1, log2fft_size_, FFT_INVERSE);
  vDSP_ztoc(&frame_, 1, (DSPComplex*)data, 2, fft_size_ / 2);

  // Do final scaling so that x == IFFT(FFT(x))
  float scale = 1.0f / fft_size_;
  vector_math::Vsmul(data, 1, &scale, data, 1, fft_size_);
}

FFTSetup FFTFrame::FftSetupForSize(unsigned log2fft_size) {
  auto& setup = FFTSetups();
  return setup[log2fft_size]->GetSetup();
}

unsigned FFTFrame::MinFFTSize() {
  return 1u << kMinFFTPow2Size;
}

unsigned FFTFrame::MaxFFTSize() {
  return 1u << kMaxFFTPow2Size;
}

void FFTFrame::Initialize(float sample_rate) {
  // Initialize the vector now so it's ready for use when we construct
  // FFTFrames.
  FFTSetups();

  // Determine the order of the convolvers used by the HRTF kernel.  Allocate
  // FFT setups for that size and for half that size.  The HRTF kernel uses half
  // size for analysis FFTs.
  //
  // TODO(rtoy): Try to come up with some way so that |Initialize()| doesn't
  // need to know about how the HRTF panner uses FFTs.
  unsigned hrtf_order = static_cast<unsigned>(
      log2(HRTFPanner::FftSizeForSampleRate(sample_rate)));
  InitializeFFTSetupForSize(hrtf_order);
  InitializeFFTSetupForSize(hrtf_order - 1);
}

void FFTFrame::Cleanup() {
  auto& setups = FFTSetups();

  for (wtf_size_t k = 0; k < setups.size(); ++k) {
    setups[k].reset();
  }
}

}  // namespace blink

#endif  // BUILDFLAG(IS_MAC) && !defined(WTF_USE_WEBAUDIO_PFFFT)
