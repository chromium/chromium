// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/speech_mask_estimator.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/memory/aligned_memory.h"
#include "base/test/gtest_util.h"
#include "media/webrtc/voice_isolation/stft_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using ::testing::Each;
using ::testing::FloatNear;

// Layout of TfLiteVoiceIsolation: two 50%-overlapping 16 kHz DFTs of 20 ms
// per call.
constexpr size_t kDftSize = 320;
constexpr size_t kNumDfts = 2;
constexpr size_t kFrameSize = kNumDfts * kDftSize;

// One mask value per bin, from DC to Nyquist.
constexpr size_t kMaskSize = kDftSize / 2 + 1;

// Number of calls of each test signal. The first call primes the overlap-add
// history, so the checks of the mask values start at the second one.
constexpr size_t kNumCalls = 4;
constexpr size_t kFirstCheckedCall = 1;

// Seeds of two different white noise test signals.
constexpr uint64_t kFirstSeed = 1;
constexpr uint64_t kSecondSeed = 2;

// Tolerance of the mask against its exact or closed-form value: the
// overlap-add and the second analysis add float rounding errors, and the
// closed forms neglect leakage that changes the mask by less than 1e-6.
constexpr float kMaskTolerance = 1e-4f;

// One cosine of the frequency-selective test signal, at the center of DFT bin
// `bin`, and the gain of that cosine in the denoised signal. Bin 0 is a
// constant (DC) and bin `kDftSize / 2` alternates in sign (Nyquist).
struct Tone {
  size_t bin;
  float denoised_gain;
};

// The periodic Hann window spreads a bin-centered cosine over its own bin and
// its two neighbors only. With the bins at least 2 apart, and 2 away from DC
// and Nyquist, where the mirrored halves of the spectrum meet, no cosine leaks
// into the center bin of another one, so the mask of each center bin is
// exactly the gain of its cosine. The gains differ from each other and stay
// below the cap at 1, so a mask value read from the wrong bin shows up.
constexpr auto kTones = std::to_array<Tone>({
    {.bin = 0, .denoised_gain = 0.9f},
    {.bin = 2, .denoised_gain = 0.1f},
    {.bin = 41, .denoised_gain = 0.6f},
    {.bin = 100, .denoised_gain = 0.3f},
    {.bin = 158, .denoised_gain = 0.75f},
    {.bin = kDftSize / 2, .denoised_gain = 0.2f},
});

// Amplitude of each cosine of the frequency-selective test signal.
constexpr float kToneAmplitude = 0.1f;

// Phase of each cosine per bin, in radians, so that the interior bins have
// both real and imaginary parts.
constexpr float kTonePhasePerBin = 1.0f;

// Which signal MakeToneDfts() analyzes.
enum class ToneSignal {
  // The sum of the cosines of `kTones`.
  kInput,
  // The same sum with each cosine scaled by its `denoised_gain`.
  kDenoised,
};

// Returns the DFTs of `kNumCalls` consecutive calls for `signal`, aligned for
// SpeechMaskEstimator::Estimate(). Analyzing a waveform, rather than scaling
// the input DFTs bin by bin, keeps the denoised DFTs consistent, so that their
// overlap-add is exact.
std::vector<base::AlignedHeapArray<float>> MakeToneDfts(ToneSignal signal) {
  WindowedFft windowed_fft(/*fft_size=*/kDftSize);
  std::vector<float> audio(kDftSize);
  std::vector<base::AlignedHeapArray<float>> dfts;
  dfts.reserve(kNumCalls);
  for (size_t call = 0; call < kNumCalls; ++call) {
    // Each call adds `kDftSize` new samples.
    for (size_t i = 0; i < kDftSize; ++i) {
      const size_t sample_index = call * kDftSize + i;
      audio[i] = 0.0f;
      for (const Tone& tone : kTones) {
        const float gain =
            signal == ToneSignal::kDenoised ? tone.denoised_gain : 1.0f;
        const double phase = 2.0 * std::numbers::pi *
                                 static_cast<double>(tone.bin) *
                                 static_cast<double>(sample_index) / kDftSize +
                             kTonePhasePerBin * static_cast<double>(tone.bin);
        audio[i] += gain * kToneAmplitude * static_cast<float>(std::cos(phase));
      }
    }
    dfts.push_back(base::AlignedUninit<float>(
        kFrameSize, SpeechMaskEstimator::kDftAlignment));
    windowed_fft.ForwardTransform(audio, dfts.back());
  }
  return dfts;
}

// Feeds `estimator` the calls of `input_dfts`, with denoised DFTs equal to the
// input DFTs times `denoised_gain`, and returns the mask after each call.
std::vector<std::vector<float>> EstimateMasks(
    SpeechMaskEstimator& estimator,
    const std::vector<std::vector<float>>& input_dfts,
    float denoised_gain) {
  std::vector<std::vector<float>> masks;
  base::AlignedHeapArray<float> denoised_dfts = base::AlignedUninit<float>(
      kFrameSize, SpeechMaskEstimator::kDftAlignment);
  for (const std::vector<float>& call_dfts : input_dfts) {
    for (size_t i = 0; i < kFrameSize; ++i) {
      denoised_dfts[i] = denoised_gain * call_dfts[i];
    }
    estimator.Estimate(call_dfts, denoised_dfts);
    masks.push_back(base::ToVector(estimator.mask()));
  }
  return masks;
}

}  // namespace

TEST(SpeechMaskEstimatorTest, MaskIsOneBeforeFirstEstimate) {
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);

  EXPECT_EQ(estimator.mask().size(), kMaskSize);
  EXPECT_THAT(base::ToVector(estimator.mask()), Each(1.0f));
}

TEST(SpeechMaskEstimatorTest, IdentityGivesUnitMask) {
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);

  const std::vector<std::vector<float>> masks = EstimateMasks(
      estimator,
      MakeNoiseDfts(kFirstSeed, /*dft_size=*/kDftSize, /*num_calls=*/kNumCalls),
      /*denoised_gain=*/1.0f);

  for (size_t call = kFirstCheckedCall; call < kNumCalls; ++call) {
    EXPECT_THAT(masks[call], Each(FloatNear(1.0f, kMaskTolerance)))
        << "call " << call;
  }
}

TEST(SpeechMaskEstimatorTest, ScaledDenoisedGivesScaledMask) {
  constexpr float kDenoisedGain = 0.25f;
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);

  const std::vector<std::vector<float>> masks = EstimateMasks(
      estimator,
      MakeNoiseDfts(kFirstSeed, /*dft_size=*/kDftSize, /*num_calls=*/kNumCalls),
      kDenoisedGain);

  for (size_t call = kFirstCheckedCall; call < kNumCalls; ++call) {
    EXPECT_THAT(masks[call], Each(FloatNear(kDenoisedGain, kMaskTolerance)))
        << "call " << call;
  }
}

TEST(SpeechMaskEstimatorTest, MaskIsCappedAtOne) {
  constexpr float kDenoisedGain = 4.0f;
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);

  const std::vector<std::vector<float>> masks = EstimateMasks(
      estimator,
      MakeNoiseDfts(kFirstSeed, /*dft_size=*/kDftSize, /*num_calls=*/kNumCalls),
      kDenoisedGain);

  for (size_t call = kFirstCheckedCall; call < kNumCalls; ++call) {
    EXPECT_THAT(masks[call], Each(1.0f)) << "call " << call;
  }
}

TEST(SpeechMaskEstimatorTest, FrequencySelectiveGainsMapToTheirBins) {
  const std::vector<base::AlignedHeapArray<float>> input_dfts =
      MakeToneDfts(ToneSignal::kInput);
  const std::vector<base::AlignedHeapArray<float>> denoised_dfts =
      MakeToneDfts(ToneSignal::kDenoised);
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);

  for (size_t call = 0; call < kNumCalls; ++call) {
    estimator.Estimate(input_dfts[call], denoised_dfts[call]);

    // Check the center bin of every cosine once the overlap-add is primed.
    if (call >= kFirstCheckedCall) {
      for (const Tone& tone : kTones) {
        EXPECT_NEAR(estimator.mask()[tone.bin], tone.denoised_gain,
                    kMaskTolerance)
            << "call " << call << ", bin " << tone.bin;
      }
    }
  }
}

// A denoiser that keeps the first DFT of every call and zeroes the second one
// returns DFTs that the overlapping windows disagree on: a ratio of the raw
// first DFTs would report a mask of 1. Overlap-add multiplies each inverse DFT
// by w / (w^2 + w'^2), where w' is the Hann window `w` shifted by half a DFT.
// Each DFT is already windowed by `w`, so it contributes the share
// w^2 / (w^2 + w'^2) of every sample, and the shares of two overlapping DFTs
// add up to 1. Only the share of the first DFT is left, so the second analysis
// sees each cosine through the window w^3 / (w^2 + w'^2) instead of `w`, and
// its center bin keeps
// sum(w^3 / (w^2 + w'^2)) / sum(w) = (3 - sqrt(2)) / 2 of its magnitude.
TEST(SpeechMaskEstimatorTest, OverlapAddAttenuatesInconsistentNeighboringDfts) {
  constexpr float kExpectedMask = (3.0f - std::numbers::sqrt2_v<float>) / 2.0f;

  // The window w^3 / (w^2 + w'^2) is narrower than the Hann window, so its
  // spectrum is wider: the cosines near DC and Nyquist (bins 0, 2, 158 and
  // 160) leak into each other's center bins, and only the interior ones, far
  // from every other cosine, match the closed form. From 8 bins away from DC
  // and Nyquist on, the mirror image of a cosine changes its mask by less than
  // 1e-6.
  constexpr size_t kMinBinsFromDcAndNyquist = 8;

  const std::vector<base::AlignedHeapArray<float>> input_dfts =
      MakeToneDfts(ToneSignal::kInput);
  base::AlignedHeapArray<float> denoised_dfts = base::AlignedZeroed<float>(
      kFrameSize, SpeechMaskEstimator::kDftAlignment);
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);

  for (size_t call = 0; call < kNumCalls; ++call) {
    // Keep the first DFT. The second one stays zero.
    denoised_dfts.first(kDftSize).copy_from(input_dfts[call].first(kDftSize));
    estimator.Estimate(input_dfts[call], denoised_dfts);
    // The first DFT of call 0 spans half a DFT of silence before the cosines.
    if (call < kFirstCheckedCall) {
      continue;
    }

    for (const Tone& tone : kTones) {
      if (tone.bin < kMinBinsFromDcAndNyquist ||
          tone.bin + kMinBinsFromDcAndNyquist > kDftSize / 2) {
        continue;
      }
      EXPECT_NEAR(estimator.mask()[tone.bin], kExpectedMask, kMaskTolerance)
          << "call " << call << ", bin " << tone.bin;
    }
  }
}

TEST(SpeechMaskEstimatorTest, SilentInputGivesUnitMask) {
  constexpr float kDenoisedGain = 0.25f;
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);

  // Drive the mask away from its initial 1.0f values first so that the check
  // below verifies that silent bins reset the mask to 1.0f.
  EstimateMasks(
      estimator,
      MakeNoiseDfts(kFirstSeed, /*dft_size=*/kDftSize, /*num_calls=*/kNumCalls),
      kDenoisedGain);

  // Without the silence threshold, the ratio would be 0 / (0 + epsilon) = 0.
  const std::vector<std::vector<float>> silent_dfts(
      kNumCalls, std::vector<float>(kFrameSize, 0.0f));
  const std::vector<std::vector<float>> masks =
      EstimateMasks(estimator, silent_dfts, /*denoised_gain=*/1.0f);

  for (size_t call = 0; call < kNumCalls; ++call) {
    EXPECT_THAT(masks[call], Each(1.0f)) << "call " << call;
  }
}

TEST(SpeechMaskEstimatorTest, ClearMatchesFreshInstance) {
  // A quarter of the input keeps the mask away from the cap at 1, which could
  // hide differences.
  constexpr float kDenoisedGain = 0.25f;
  const std::vector<std::vector<float>> first_signal =
      MakeNoiseDfts(kFirstSeed, /*dft_size=*/kDftSize, /*num_calls=*/kNumCalls);
  const std::vector<std::vector<float>> second_signal = MakeNoiseDfts(
      kSecondSeed, /*dft_size=*/kDftSize, /*num_calls=*/kNumCalls);

  // Reference: a fresh instance fed the second signal.
  SpeechMaskEstimator fresh_estimator(/*dft_size=*/kDftSize);
  const std::vector<std::vector<float>> expected_masks =
      EstimateMasks(fresh_estimator, second_signal, kDenoisedGain);

  // Without Clear(), the overlap-add history of the first signal changes the
  // mask of the second one. Otherwise this test could not detect whether
  // Clear() resets anything.
  SpeechMaskEstimator uncleared_estimator(/*dft_size=*/kDftSize);
  EstimateMasks(uncleared_estimator, first_signal, kDenoisedGain);
  ASSERT_NE(EstimateMasks(uncleared_estimator, second_signal, kDenoisedGain),
            expected_masks);

  // Clear() resets the mask to all ones and the estimator behaves as a fresh
  // one.
  SpeechMaskEstimator cleared_estimator(/*dft_size=*/kDftSize);
  EstimateMasks(cleared_estimator, first_signal, kDenoisedGain);
  cleared_estimator.Clear();
  EXPECT_THAT(base::ToVector(cleared_estimator.mask()), Each(1.0f));
  EXPECT_EQ(EstimateMasks(cleared_estimator, second_signal, kDenoisedGain),
            expected_masks);
}

TEST(SpeechMaskEstimatorDeathTest, DiesOnWrongInputSize) {
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);
  const std::vector<float> single_dft(kDftSize);
  const base::AlignedHeapArray<float> dfts = base::AlignedZeroed<float>(
      kFrameSize, SpeechMaskEstimator::kDftAlignment);

  EXPECT_CHECK_DEATH(estimator.Estimate(single_dft, dfts));
}

TEST(SpeechMaskEstimatorDeathTest, DiesOnWrongDenoisedSize) {
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);
  const std::vector<float> dfts(kFrameSize);
  const base::AlignedHeapArray<float> single_dft =
      base::AlignedZeroed<float>(kDftSize, SpeechMaskEstimator::kDftAlignment);

  EXPECT_CHECK_DEATH(estimator.Estimate(dfts, single_dft));
}

TEST(SpeechMaskEstimatorDeathTest, DiesOnUnalignedDenoisedDfts) {
  SpeechMaskEstimator estimator(/*dft_size=*/kDftSize);
  const std::vector<float> input_dfts(kFrameSize);
  const base::AlignedHeapArray<float> aligned_buffer =
      base::AlignedZeroed<float>(kFrameSize + 1,
                                 SpeechMaskEstimator::kDftAlignment);

  // Offset by one float (4 bytes) from a 16-byte aligned base.
  EXPECT_CHECK_DEATH(estimator.Estimate(input_dfts, aligned_buffer.subspan(1)));
}

}  // namespace media
