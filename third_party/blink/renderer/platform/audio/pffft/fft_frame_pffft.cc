// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#if defined(WTF_USE_WEBAUDIO_PFFFT)

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

#include "base/synchronization/lock.h"
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

HashMap<unsigned, std::unique_ptr<FFTFrame::FFTSetup>>& FFTFrame::FFTSetups() {
  // TODO(rtoy): Let this bake for a bit and then remove the assertions after
  // we're confident the first call is from the main thread.
  static bool first_call = true;

  // A HashMap to hold all of the possible FFT setups we need.  The setups are
  // initialized lazily.  The key is the fft size, and the value is the setup
  // data.
  typedef HashMap<unsigned, std::unique_ptr<FFTSetup>> FFTHashMap_t;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(FFTHashMap_t, fft_setups, ());

  if (first_call) {
    DEFINE_STATIC_LOCAL(base::Lock, setup_lock, ());

    // Make sure we construct the fft_setups vector below on the main thread.
    // Once constructed, we can access it from any thread.
    DCHECK(IsMainThread());
    first_call = false;

    base::AutoLock locker(setup_lock);

    // Initialize the hash map with all the possible keys (FFT sizes), with a
    // value of nullptr because we want to initialize the setup data lazily. The
    // set of valid FFT sizes for PFFFT are of the form 2^k*3^m*5*n where k >=
    // 5, m >= 0, n >= 0.  We only go up to a max size of 32768, because we need
    // at least an FFT size of 32768 for the convolver node.

    // TODO(crbug.com/988121):  Sync this with kMaxFFTPow2Size.
    const int kMaxConvolverFFTSize = 32768;

    for (int n = 1; n <= kMaxConvolverFFTSize; n *= 5) {
      for (int m = 1; m <= kMaxConvolverFFTSize / n; m *= 3) {
        for (int k = 32; k <= kMaxConvolverFFTSize / (n * m); k *= 2) {
          int size = k * m * n;
          if (size <= kMaxConvolverFFTSize && !fft_setups.Contains(size)) {
            fft_setups.insert(size, nullptr);
          }
        }
      }
    }

    // There should be 87 entries when we're done.
    DCHECK_EQ(fft_setups.size(), 87u);
  }

  return fft_setups;
}

void FFTFrame::InitializeFFTSetupForSize(wtf_size_t fft_size) {
  auto& setup = FFTSetups();

  DCHECK(setup.Contains(fft_size));

  if (setup.find(fft_size)->value == nullptr) {
    DEFINE_STATIC_LOCAL(base::Lock, setup_lock, ());

    // Make sure allocation of a new setup only occurs on the main thread so we
    // don't have a race condition with multiple threads trying to write to the
    // same element of the vector.
    DCHECK(IsMainThread());

    auto fft_data = std::make_unique<FFTSetup>(fft_size);
    base::AutoLock locker(setup_lock);
    setup.find(fft_size)->value = std::move(fft_data);
  }
}

PFFFT_Setup* FFTFrame::FFTSetupForSize(wtf_size_t fft_size) {
  auto& setup = FFTSetups();

  DCHECK(setup.Contains(fft_size));
  DCHECK(setup.find(fft_size)->value);

  return setup.find(fft_size)->value->GetSetup();
}

FFTFrame::FFTFrame(unsigned fft_size)
    : fft_size_(fft_size),
      log2fft_size_(static_cast<unsigned>(log2(fft_size))),
      real_data_(fft_size / 2),
      imag_data_(fft_size / 2),
      complex_data_(fft_size),
      pffft_work_(fft_size) {

  // Initialize the PFFFT_Setup object here so that it will be ready when we
  // compute FFTs.
  InitializeFFTSetupForSize(fft_size);
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
  InitializeFFTSetupForSize(fft_size_);

  // Copy/setup frame data.
  unsigned nbytes = sizeof(float) * (fft_size_ / 2);
  memcpy(RealData().Data(), frame.RealData().Data(), nbytes);
  memcpy(ImagData().Data(), frame.ImagData().Data(), nbytes);
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
  unsigned hrtf_fft_size =
      static_cast<unsigned>(HRTFPanner::FftSizeForSampleRate(sample_rate));

  DCHECK_GT(hrtf_fft_size, 1U << kMinFFTPow2Size);
  DCHECK_LE(hrtf_fft_size, 1U << kMaxFFTPow2Size);

  InitializeFFTSetupForSize(hrtf_fft_size);
  InitializeFFTSetupForSize(hrtf_fft_size / 2);
}

void FFTFrame::Cleanup() {
  for (auto& setup : FFTSetups()) {
    setup.value.reset();
  }
}

FFTFrame::~FFTFrame() {
}

void FFTFrame::DoFFT(const float* data) {
  DCHECK_EQ(pffft_work_.size(), fft_size_);

  PFFFT_Setup* setup = FFTSetupForSize(fft_size_);
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

  PFFFT_Setup* setup = FFTSetupForSize(fft_size_);
  DCHECK(setup);

  pffft_transform_ordered(setup, fft_data, data, pffft_work_.Data(),
                          PFFFT_BACKWARD);

  // The inverse transform needs to be scaled because PFFFT doesn't.
  float scale = 1.0 / fft_size_;
  vector_math::Vsmul(data, 1, &scale, data, 1, fft_size_);
}

}  // namespace blink

#endif  // #if defined(WTF_USE_WEBAUDIO_PFFFT)
