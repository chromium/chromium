// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(WTF_USE_WEBAUDIO_PFFFT)

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/hrtf_panner.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/pffft/src/pffft.h"
namespace blink {

// Not really clear what the largest size of FFT PFFFT supports, but the docs
// indicate it can go up to at least 1048576 (order 20).  Since we're using
// single-floats, accuracy decreases quite a bit at that size.  Plus we only
// need 32K (order 15) for WebAudio.
const unsigned kMaxFFTPow2Size = 20;

// PFFFT has a minimum real FFT order of 5 (32-point transforms).
const unsigned kMinFFTPow2Size = 5;

FFTFrame::FFTSetup::FFTSetup(unsigned fft_size) {
  DCHECK_LE(fft_size, 1U << kMaxFFTPow2Size);
  DCHECK_GE(fft_size, 1U << kMinFFTPow2Size);

  // All FFTs we need are FFTs of real signals, and the inverse FFTs produce
  // real signals.  Hence |PFFFT_REAL|.
  setup_ = pffft_new_setup(fft_size, PFFFT_REAL);
  DCHECK(setup_);
}

FFTFrame::FFTSetup::~FFTSetup() {
  DCHECK(setup_);

  pffft_destroy_setup(setup_);
}

Vector<std::unique_ptr<FFTFrame::FFTSetup>>& FFTFrame::FFTSetups() {
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
  DEFINE_STATIC_LOCAL(Vector<std::unique_ptr<FFTSetup>>, fft_setups,
                      (kMaxFFTPow2Size));

  return fft_setups;
}

void FFTFrame::InitializeFFTSetupForSize(wtf_size_t log2fft_size) {
  auto& setup = FFTSetups();

  if (!setup[log2fft_size]) {
    // Make sure allocation of a new setup only occurs on the main thread so we
    // don't have a race condition with multiple threads trying to write to the
    // same element of the vector.
    DCHECK(IsMainThread());

    setup[log2fft_size] = std::make_unique<FFTSetup>(1 << log2fft_size);
  }
}

PFFFT_Setup* FFTFrame::FFTSetupForSize(wtf_size_t log2fft_size) {
  auto& setup = FFTSetups();

  DCHECK(setup[log2fft_size]);

  return setup[log2fft_size]->GetSetup();
}

FFTFrame::FFTFrame(unsigned fft_size)
    : fft_size_(fft_size),
      log2fft_size_(static_cast<unsigned>(log2(fft_size))),
      real_data_(fft_size / 2),
      imag_data_(fft_size / 2),
      complex_data_(fft_size),
      pffft_work_(fft_size) {
  // We only allow power of two.
  DCHECK_EQ(1UL << log2fft_size_, fft_size_);

  // Initialize the PFFFT_Setup object here so that it will be ready when we
  // compute FFTs.
  InitializeFFTSetupForSize(log2fft_size_);
}

// Creates a blank/empty frame (interpolate() must later be called).
FFTFrame::FFTFrame() : fft_size_(0), log2fft_size_(0) {}

// Copy constructor.
FFTFrame::FFTFrame(const FFTFrame& frame)
    : fft_size_(frame.fft_size_),
      log2fft_size_(frame.log2fft_size_),
      real_data_(frame.fft_size_ / 2),
      imag_data_(frame.fft_size_ / 2),
      complex_data_(frame.fft_size_),
      pffft_work_(frame.fft_size_) {
  // Initialize the PFFFT_Setup object here wo that it will be ready when we
  // compute FFTs.
  InitializeFFTSetupForSize(log2fft_size_);

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

  DCHECK_GT(hrtf_order, kMinFFTPow2Size);
  DCHECK_LE(hrtf_order, kMaxFFTPow2Size);

  InitializeFFTSetupForSize(hrtf_order);
  InitializeFFTSetupForSize(hrtf_order - 1);
}

void FFTFrame::Cleanup() {
  auto& setups = FFTSetups();

  for (wtf_size_t k = 0; k < setups.size(); ++k) {
    setups[k].reset();
  }
}

FFTFrame::~FFTFrame() {
}

void FFTFrame::DoFFT(const float* data) {
  DCHECK_EQ(pffft_work_.size(), fft_size_);

  PFFFT_Setup* setup = FFTSetupForSize(log2fft_size_);
  DCHECK(setup);

  pffft_transform_ordered(setup, data, complex_data_.Data(), pffft_work_.Data(),
                          PFFFT_FORWARD);

  unsigned len = fft_size_ / 2;

  // Split FFT data into real and imaginary arrays.  PFFFT transform already
  // uses the desired format; we just need to split out the real and imaginary
  // parts.
  const float* c = complex_data_.Data();
  float* real = real_data_.Data();
  float* imag = imag_data_.Data();
  for (unsigned k = 0; k < len; ++k) {
    int index = 2 * k;
    real[k] = c[index];
    imag[k] = c[index + 1];
  }
}

void FFTFrame::DoInverseFFT(float* data) {
  DCHECK_EQ(complex_data_.size(), fft_size_);

  unsigned len = fft_size_ / 2;

  // Pack the real and imaginary data into the complex array format.  PFFFT
  // already uses the desired format; we just need to pack the parts together.
  float* fft_data = complex_data_.Data();
  const float* real = real_data_.Data();
  const float* imag = imag_data_.Data();
  for (unsigned k = 0; k < len; ++k) {
    int index = 2 * k;
    fft_data[index] = real[k];
    fft_data[index + 1] = imag[k];
  }

  PFFFT_Setup* setup = FFTSetupForSize(log2fft_size_);
  DCHECK(setup);

  pffft_transform_ordered(setup, fft_data, data, pffft_work_.Data(),
                          PFFFT_BACKWARD);

  // The inverse transform needs to be scaled because PFFFT doesn't.
  float scale = 1.0 / fft_size_;
  vector_math::Vsmul(data, 1, &scale, data, 1, fft_size_);
}

}  // namespace blink

#endif  // #if defined(WTF_USE_WEBAUDIO_PFFFT)
