// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/band_split_voice_isolation.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/trace_event/trace_event.h"
#include "media/webrtc/voice_isolation/spectral_voice_isolation_component.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"

namespace media {

namespace {

// Floats per 48kHz DFT (960) and per 16kHz low-band DFT (320, 0 to 8kHz).
constexpr size_t kFullBandSize =
    BandSplitVoiceIsolation::kFrameSize / BandSplitVoiceIsolation::kNumDfts;
constexpr size_t kLowBandSize = BandSplitVoiceIsolation::kInnerFrameSize /
                                BandSplitVoiceIsolation::kNumDfts;
// PFFFT ordered real layout: [0] = DC, [1] = Nyquist, then (re, im) pairs.
constexpr size_t kNyquistIndex = 1;
constexpr float kForwardScale = 1.0f / BandSplitVoiceIsolation::kBandRatio;
constexpr float kInverseScale = BandSplitVoiceIsolation::kBandRatio;

// Complex bins of a 16kHz DFT, DC to Nyquist (161): the size of the speech
// mask that the internal component reports.
constexpr size_t kNumLowBandBins = kLowBandSize / 2 + 1;

// Width of a DFT bin. Each DFT window lasts one call period (20 ms), so the
// bins of both the 48kHz and the 16kHz DFTs are 1 / 20 ms = 50Hz apart.
constexpr size_t kBinSpacingHz = BandSplitVoiceIsolation::kFramesPerSecond;

// The high band gain averages the speech mask over 6000 to 7600Hz, near the
// top of the band that the model processes: mask bins [120, 152).
constexpr size_t kHighBandGainLowFrequencyHz = 6000;
constexpr size_t kHighBandGainHighFrequencyHz = 7600;
constexpr size_t kHighBandGainStartBin =
    kHighBandGainLowFrequencyHz / kBinSpacingHz;
constexpr size_t kHighBandGainEndBin =
    kHighBandGainHighFrequencyHz / kBinSpacingHz;

// The averaged bins must have a real and an imaginary part, so they must lie
// above DC and below Nyquist.
static_assert(kHighBandGainStartBin > 0);
static_assert(kHighBandGainStartBin < kHighBandGainEndBin);
static_assert(kHighBandGainEndBin <= kLowBandSize / 2);

// Number of Taylor series terms summed by ConstexprSin().
constexpr int kNumSinTaylorTerms = 10;

// Returns sin(x) for |x| <= pi / 2. std::sin() is not constexpr before C++26,
// so this sums the first kNumSinTaylorTerms terms of its Taylor series. The
// truncation error is below 3e-16 in that range, far below float precision.
consteval double ConstexprSin(double x) {
  double term = x;
  double sum = x;
  for (int k = 1; k < kNumSinTaylorTerms; ++k) {
    term *= -x * x / ((2 * k) * (2 * k + 1));
    sum += term;
  }
  return sum;
}

// The top of each 16kHz DFT crossfades from the model output to the scaled
// input over the complex bins strictly between 7500 and 8000Hz (7550..7950Hz).
constexpr size_t kCrossfadeLowFrequencyHz = 7500;
constexpr size_t kCrossfadeHighFrequencyHz = 8000;
constexpr size_t kNumCrossfadeBins =
    (kCrossfadeHighFrequencyHz - kCrossfadeLowFrequencyHz) / kBinSpacingHz - 1;
static_assert(kNumCrossfadeBins == 9);

// Weight of the model output in each crossfade bin, a falling half-Hann ramp:
// weight[bin] = sin^2(pi * (kNumCrossfadeBins - bin) /
//                     (2 * (kNumCrossfadeBins + 1)))
// The ramp is 1 at 7500Hz and 0 at 8kHz, but those two bins take the model
// output and the scaled input as they are, so only the weights in between are
// computed: from 0.976 at 7550Hz down to 0.024 at 7950Hz. The input, scaled by
// the high band gain, gets the rest.
consteval std::array<float, kNumCrossfadeBins> ComputeCrossfadeWeights() {
  std::array<float, kNumCrossfadeBins> weights = {};
  for (size_t bin = 0; bin < kNumCrossfadeBins; ++bin) {
    const double sine = ConstexprSin(
        std::numbers::pi * static_cast<double>(kNumCrossfadeBins - bin) /
        static_cast<double>(2 * (kNumCrossfadeBins + 1)));
    weights[bin] = static_cast<float>(sine * sine);
  }
  return weights;
}
constexpr std::array<float, kNumCrossfadeBins> kCrossfadeWeights =
    ComputeCrossfadeWeights();

// Sanity check of the compile-time evaluation: the middle crossfade bin
// (7750Hz) weights both bands equally.
static_assert(kCrossfadeWeights[kNumCrossfadeBins / 2] == 0.5f);

// Index of the real part of the first crossfade bin in each 16kHz DFT. Each
// complex bin takes two floats, (re, im).
constexpr size_t kCrossfadeStartIndex = kLowBandSize - 2 * kNumCrossfadeBins;

// The crossfade bins must fit in the DFT without reaching its DC and Nyquist
// slots. The first check also keeps kCrossfadeStartIndex from wrapping around.
static_assert(2 * kNumCrossfadeBins < kLowBandSize);
static_assert(kCrossfadeStartIndex > kNyquistIndex);

// Returns the energy of the high band of `dft`, a 48kHz DFT: the sum of the
// squared magnitudes of its bins from 8kHz up to, but not including, the 24kHz
// Nyquist bin.
float HighBandEnergy(base::span<const float> dft) {
  float energy = 0.0f;
  for (const float coefficient : dft.subspan(kLowBandSize)) {
    energy += coefficient * coefficient;
  }
  return energy;
}

// Returns the gain of the high band of each 48kHz DFT in `full_band_dfts`. For
// the first DFT, it is the mean of `speech_mask` over 6000..7600Hz, weighted by
// the magnitude of `first_low_band_dft`, the model input DFT that the mask
// belongs to. The mask does not cover a later DFT, so if its high band is
// louder than that of the first DFT, which may be a noise spike, its gain is
// lowered in proportion.
std::array<float, BandSplitVoiceIsolation::kNumDfts> ComputeHighBandGains(
    base::span<const float> speech_mask,
    base::span<const float> first_low_band_dft,
    base::span<const float> full_band_dfts) {
  CHECK_EQ(speech_mask.size(), kNumLowBandBins);

  // Magnitude-weighted mean of the speech mask, for the first DFT.
  float magnitude_sum = 0.0f;
  float masked_magnitude_sum = 0.0f;
  for (size_t bin = kHighBandGainStartBin; bin < kHighBandGainEndBin; ++bin) {
    const float real = first_low_band_dft[2 * bin];
    const float imag = first_low_band_dft[2 * bin + 1];
    const float magnitude = std::sqrt(real * real + imag * imag);
    magnitude_sum += magnitude;
    masked_magnitude_sum += magnitude * speech_mask[bin];
  }
  std::array<float, BandSplitVoiceIsolation::kNumDfts> gains = {};
  gains[0] = masked_magnitude_sum /
             (magnitude_sum + std::numeric_limits<float>::epsilon());

  // Lower the gain of each later DFT whose high band is louder than that of
  // the first one, in proportion to the ratio of the RMS magnitudes, i.e. the
  // square root of the energy ratio.
  const float first_energy =
      HighBandEnergy(full_band_dfts.first(kFullBandSize));
  for (size_t dft = 1; dft < BandSplitVoiceIsolation::kNumDfts; ++dft) {
    const float energy = HighBandEnergy(
        full_band_dfts.subspan(dft * kFullBandSize, /*count=*/kFullBandSize));

    // `first_energy` is always >= 0. The strict `>` guarantees energy is always
    // bigger than zero and and avoids division by zero in the first branch.
    gains[dft] = energy > first_energy
                     ? gains[0] * std::sqrt(first_energy / energy)
                     : gains[0];
  }
  return gains;
}

}  // namespace

BandSplitVoiceIsolation::BandSplitVoiceIsolation(
    std::unique_ptr<SpectralVoiceIsolationComponent> internal_voice_isolation)
    : internal_voice_isolation_(std::move(internal_voice_isolation)) {
  CHECK(internal_voice_isolation_);
  CHECK_EQ(internal_voice_isolation_->FrameSize(), kInnerFrameSize);
  CHECK_EQ(internal_voice_isolation_->FramesPerSecond(), kFramesPerSecond);
}

BandSplitVoiceIsolation::~BandSplitVoiceIsolation() = default;

void BandSplitVoiceIsolation::ProcessAudio(base::span<const float> dfts_input,
                                           base::span<float> dfts_output) {
  TRACE_EVENT("audio", "BandSplitVoiceIsolation::ProcessAudio");
  CHECK_EQ(dfts_input.size(), kFrameSize);
  CHECK_EQ(dfts_output.size(), kFrameSize);

  base::span<float> internal_in_span(internal_input_buffer_);
  base::span<const float> internal_out_span(internal_output_buffer_);

  // Copy the lower frequency bins (0 to 8kHz) of each 48kHz FFT into the
  // internal 16kHz FFT buffer. In PFFFT's ordered real FFT format, index 1
  // stores the Nyquist component, so populate the 16kHz Nyquist slot (8kHz)
  // using the real part of the 8kHz bin (index `kLowBandSize`) of the 48kHz
  // FFT.
  for (size_t dft = 0; dft < kNumDfts; ++dft) {
    const size_t inner_offset = dft * kLowBandSize;
    const size_t outer_offset = dft * kFullBandSize;

    internal_in_span.subspan(inner_offset, /*count=*/kLowBandSize)
        .copy_from_nonoverlapping(
            dfts_input.subspan(outer_offset, /*count=*/kLowBandSize));
    internal_input_buffer_[inner_offset + kNyquistIndex] =
        dfts_input[outer_offset + kLowBandSize];
  }

  // Scale down DFT amplitudes to match the 3x shorter FFT window length of the
  // 16kHz representation.
  for (float& coeff : internal_input_buffer_) {
    coeff *= kForwardScale;
  }

  // Run the internal 16kHz voice isolation model on the extracted low band.
  internal_voice_isolation_->ProcessAudio(internal_input_buffer_,
                                          internal_output_buffer_);

  // Compute the gain of the high band of each DFT from the speech mask that the
  // model reports for the low band.
  const std::array<float, kNumDfts> high_band_gains =
      ComputeHighBandGains(internal_voice_isolation_->SpeechMask(),
                           internal_in_span.first(kLowBandSize), dfts_input);

  // Scale up DFT amplitudes to match the 48kHz FFT window length before
  // copying into the 48kHz output buffer.
  for (float& coeff : internal_output_buffer_) {
    coeff *= kInverseScale;
  }

  // Rebuild each 48kHz DFT from the model output and the input high band.
  for (size_t dft = 0; dft < kNumDfts; ++dft) {
    const float gain = high_band_gains[dft];
    const base::span<const float> model_output =
        internal_out_span.subspan(dft * kLowBandSize, /*count=*/kLowBandSize);
    const base::span<const float> input =
        dfts_input.subspan(dft * kFullBandSize, /*count=*/kFullBandSize);
    const base::span<float> output =
        dfts_output.subspan(dft * kFullBandSize, /*count=*/kFullBandSize);

    // DC and the bins up to 7500Hz come from the model, which passes DC
    // through. The 16kHz Nyquist slot of the model output is not used.
    output[0] = model_output[0];
    for (size_t i = kNyquistIndex + 1; i < kCrossfadeStartIndex; ++i) {
      output[i] = model_output[i];
    }

    // From 7550 to 7950Hz, crossfade from the model output to the input scaled
    // by the high band gain, so that the two bands join without an edge.
    for (size_t i = kCrossfadeStartIndex; i < kLowBandSize; ++i) {
      const float weight = kCrossfadeWeights[(i - kCrossfadeStartIndex) / 2];
      output[i] = weight * model_output[i] + (1.0f - weight) * input[i] * gain;
    }

    // From 8kHz up, including the 8kHz bin and the 24kHz Nyquist slot, scale
    // the input by the high band gain.
    output[kNyquistIndex] = input[kNyquistIndex] * gain;
    for (size_t i = kLowBandSize; i < kFullBandSize; ++i) {
      output[i] = input[i] * gain;
    }
  }
}

size_t BandSplitVoiceIsolation::FrameSize() const {
  return kFrameSize;
}

size_t BandSplitVoiceIsolation::FramesPerSecond() const {
  return kFramesPerSecond;
}

void BandSplitVoiceIsolation::ClearBuffers() {
  // `internal_input_buffer_` and `internal_output_buffer_` need no reset: every
  // ProcessAudio() call overwrites them completely before reading them, so they
  // carry no state between calls.
  internal_voice_isolation_->ClearBuffers();
}

base::TimeDelta BandSplitVoiceIsolation::AlgorithmicDelay() const {
  return internal_voice_isolation_->AlgorithmicDelay();
}

}  // namespace media
