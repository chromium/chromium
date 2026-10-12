// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/speech_mask_estimator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/numerics/safe_conversions.h"

namespace media {

namespace {

// WindowedFft processes two DFTs per call, with a hop of half a DFT.
constexpr size_t kNumDfts = 2;

// PFFFT ordered real layout: [0] = DC, [1] = Nyquist, then (re, im) pairs.
constexpr size_t kNyquistIndex = 1;

// Input bins below -120 dB relative to an unnormalized DFT magnitude of 1.0
// (roughly -148 to -158 dBFS for a 320-point Hann DFT) keep a mask value of 1
// rather than a ratio of rounding noise.
constexpr float kSilentBinMagnitude = 1e-6f;

}  // namespace

SpeechMaskEstimator::SpeechMaskEstimator(size_t dft_size)
    : dft_size_(dft_size),
      windowed_fft_(base::checked_cast<int>(dft_size)),
      denoised_waveform_(base::HeapArray<float>::WithSize(dft_size)),
      reanalyzed_dfts_(base::HeapArray<float>::WithSize(kNumDfts * dft_size)),
      mask_(base::HeapArray<float>::WithSize(dft_size / 2 + 1)) {
  std::fill(mask_.begin(), mask_.end(), 1.0f);
}

SpeechMaskEstimator::~SpeechMaskEstimator() = default;

void SpeechMaskEstimator::Estimate(base::span<const float> input_dfts,
                                   base::span<const float> denoised_dfts) {
  CHECK_EQ(input_dfts.size(), kNumDfts * dft_size_);
  CHECK_EQ(denoised_dfts.size(), kNumDfts * dft_size_);
  CHECK(base::IsAligned(denoised_dfts.data(), kDftAlignment));

  // Overlap-add the denoised DFTs into the waveform of the window of the first
  // DFT.
  windowed_fft_.InverseTransform(denoised_dfts, denoised_waveform_);

  // Analyze that waveform again with a single Hann-windowed DFT: the second
  // DFT of ForwardTransform() spans exactly its input. The first one, which
  // also spans the previous input, is not needed.
  windowed_fft_.ForwardTransform(denoised_waveform_, reanalyzed_dfts_);
  const base::span<const float> denoised_dft =
      base::span(reanalyzed_dfts_).last(dft_size_);
  const base::span<const float> input_dft = input_dfts.first(dft_size_);

  // Mask value of one bin: the magnitude ratio, capped at 1. Silent input bins
  // keep 1, which also rules out a division by zero.
  const auto bin_mask = [](float input_magnitude, float denoised_magnitude) {
    if (input_magnitude < kSilentBinMagnitude) {
      return 1.0f;
    } else {
      return std::min(denoised_magnitude / input_magnitude, 1.0f);
    }
  };

  // DC and Nyquist are real-valued and live in the first two slots of the
  // PFFFT ordered layout, so handle them outside the loop over complex bins.
  const size_t nyquist_bin = dft_size_ / 2;
  mask_[0] = bin_mask(std::abs(input_dft[0]), std::abs(denoised_dft[0]));
  mask_[nyquist_bin] = bin_mask(std::abs(input_dft[kNyquistIndex]),
                                std::abs(denoised_dft[kNyquistIndex]));

  // Complex bins are stored as (re, im) pairs from index 2 on.
  for (size_t bin = 1; bin < nyquist_bin; ++bin) {
    const float input_re = input_dft[2 * bin];
    const float input_im = input_dft[2 * bin + 1];
    const float denoised_re = denoised_dft[2 * bin];
    const float denoised_im = denoised_dft[2 * bin + 1];
    mask_[bin] = bin_mask(
        std::sqrt(input_re * input_re + input_im * input_im),
        std::sqrt(denoised_re * denoised_re + denoised_im * denoised_im));
  }
}

void SpeechMaskEstimator::Clear() {
  windowed_fft_.Clear();
  std::fill(mask_.begin(), mask_.end(), 1.0f);
}

}  // namespace media
