// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/band_split_voice_isolation.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "media/webrtc/voice_isolation/mock_voice_isolation.h"
#include "media/webrtc/voice_isolation/stft_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using testing::_;
using testing::Each;
using testing::FloatNear;
using testing::NiceMock;
using testing::Pointwise;
using testing::Return;

namespace {

using BandSplit = BandSplitVoiceIsolation;
using NiceMockComponent = NiceMock<MockVoiceIsolationComponent>;

constexpr size_t kInnerFrameSize = BandSplit::kInnerFrameSize;
constexpr size_t kFramesPerSecond = BandSplit::kFramesPerSecond;
constexpr size_t kNumDfts = BandSplit::kNumDfts;
constexpr size_t kLowBandSize = kInnerFrameSize / kNumDfts;
constexpr size_t kBandRatio = BandSplit::kBandRatio;
constexpr size_t kFullBandSize = BandSplit::kFrameSize / kNumDfts;
// Absolute value, not derived from `BandSplit::kFrameSize`, so that `Creation`
// checks the real layout: two concatenated 960-float 48 kHz DFTs.
constexpr size_t kOuterFrameSize = 1920;
constexpr size_t kNyquistIndex = 1;
constexpr float kForwardScale = 1.0f / static_cast<float>(kBandRatio);
constexpr float kTolerance = 1e-4f;

// The internal component reports one speech mask value per complex bin of a
// 16 kHz DFT, from DC to Nyquist.
constexpr size_t kSpeechMaskSize = kLowBandSize / 2 + 1;

// The bins of both the 48 kHz and the 16 kHz DFTs are 50 Hz apart. The high
// band gain is the mean of the speech mask over bins [120, 152), from 6000 to
// 7600 Hz, weighted by the magnitude of the model input.
constexpr size_t kGainStartBin = 120;
constexpr size_t kGainEndBin = 152;

// The model output fades into the scaled input along a falling half-Hann ramp
// of kRampLengthBins bins that starts at bin kRampStartBin (7500 Hz): bin
// kRampStartBin + k keeps a share
// sin^2(pi * (kRampLengthBins - k) / (2 * kRampLengthBins)) of the model
// output. The first ramp bin is a plain copy of the model output, so the
// blending starts at the real part of the next bin and runs up to 8 kHz, where
// the ramp reaches 0 and the scaled input takes over.
constexpr size_t kRampStartBin = 150;
constexpr size_t kRampLengthBins = 10;
constexpr size_t kCrossfadeStartIndex = 2 * (kRampStartBin + 1);

// Step between consecutive floats of MakeRampInput().
constexpr float kRampStep = 0.01f;

// Correlation of a unit-amplitude tone with itself over a 48 kHz DFT window
// that holds a whole number of its periods: half the window length.
constexpr float kUnchangedToneCorrelation = kFullBandSize / 2.0f;
constexpr float kCorrelationTolerance = 1.0f;

// Returns the share of the model output in float `index` of a 48 kHz DFT, for
// an index in the crossfade range [kCrossfadeStartIndex, kLowBandSize).
float CrossfadeWeight(size_t index) {
  const size_t ramp_bin = index / 2 - kRampStartBin;
  const double sine = std::sin(std::numbers::pi *
                               static_cast<double>(kRampLengthBins - ramp_bin) /
                               static_cast<double>(2 * kRampLengthBins));
  return static_cast<float>(sine * sine);
}

// Returns the factor that the band split applies to float `index` of a 48 kHz
// DFT when the model multiplies its input by `model_gain` and the high band
// gain of the DFT is `high_band_gain`. DC and the bins up to 7500 Hz come from
// the model, the 24 kHz Nyquist slot and the bins from 8 kHz up come from the
// input scaled by the high band gain, and the bins in between blend both.
float ExpectedFactor(size_t index, float model_gain, float high_band_gain) {
  if (index == kNyquistIndex || index >= kLowBandSize) {
    return high_band_gain;
  }
  if (index < kCrossfadeStartIndex) {
    return model_gain;
  }
  const float weight = CrossfadeWeight(index);
  return weight * model_gain + (1.0f - weight) * high_band_gain;
}

// Expects every float of `output` to be the matching float of `input` times
// ExpectedFactor(), with the high band gain of its DFT taken from
// `high_band_gains`.
void ExpectOutput(base::span<const float> input,
                  base::span<const float> output,
                  float model_gain,
                  const std::array<float, kNumDfts>& high_band_gains) {
  ASSERT_EQ(output.size(), input.size());
  for (size_t i = 0; i < output.size(); ++i) {
    const float factor = ExpectedFactor(/*index=*/i % kFullBandSize, model_gain,
                                        high_band_gains[i / kFullBandSize]);
    EXPECT_NEAR(output[i], input[i] * factor, kTolerance) << "Index " << i;
  }
}

// Returns two 48 kHz DFTs whose floats fall in steps of kRampStep, so that
// every routing error shows up as a wrong value. The high band of the second
// DFT is quieter than that of the first one, so both DFTs get the same high
// band gain.
std::vector<float> MakeRampInput() {
  std::vector<float> input(BandSplit::kFrameSize);
  for (size_t i = 0; i < input.size(); ++i) {
    input[i] = static_cast<float>(input.size() - i) * kRampStep;
  }
  return input;
}

// Returns two 48 kHz DFTs whose floats are all 1, except for the high band of
// the second DFT (from 8 kHz up), which is `second_high_band_level`.
std::vector<float> MakeLevelInput(float second_high_band_level) {
  std::vector<float> input(BandSplit::kFrameSize, 1.0f);
  for (size_t i = kFullBandSize + kLowBandSize; i < input.size(); ++i) {
    input[i] = second_high_band_level;
  }
  return input;
}

// Returns a 16 kHz internal component that multiplies its input by
// `model_gain` and reports `speech_mask`, which must outlive it.
std::unique_ptr<NiceMockComponent> CreateScalingModel(
    base::span<const float> speech_mask,
    float model_gain) {
  std::unique_ptr<NiceMockComponent> model =
      std::make_unique<NiceMockComponent>();
  ON_CALL(*model, FrameSize()).WillByDefault(Return(kInnerFrameSize));
  ON_CALL(*model, FramesPerSecond()).WillByDefault(Return(kFramesPerSecond));
  ON_CALL(*model, SpeechMask()).WillByDefault(Return(speech_mask));
  ON_CALL(*model, ProcessAudio(_, _))
      .WillByDefault([model_gain](base::span<const float> input,
                                  base::span<float> output) {
        for (size_t i = 0; i < output.size(); ++i) {
          output[i] = input[i] * model_gain;
        }
      });
  return model;
}

// Correlations of an output frame with the low band and the high band tones.
struct ToneCorrelations {
  float low_band = 0.0f;
  float high_band = 0.0f;
};

// Runs a 1 kHz, a 7 kHz and a 10 kHz tone through a 48 kHz STFT, the band
// split around a passthrough model that reports `speech_mask`, and the inverse
// STFT. Returns the correlations of the last output frame with the 1 kHz and
// the 10 kHz tones. The 7 kHz tone gives the bins that set the high band gain
// a magnitude. The three tones are orthogonal over a frame, so each
// correlation only measures its own tone.
ToneCorrelations ProcessThreeTones(base::span<const float> speech_mask) {
  constexpr int kFftSize = static_cast<int>(kFullBandSize);
  constexpr float kSampleRateHz = 48000.0f;
  constexpr float kLowToneHz = 1000.0f;
  constexpr float kGainToneHz = 7000.0f;
  constexpr float kHighToneHz = 10000.0f;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

  // Build one frame of each reference tone and of the sum of all three tones.
  std::vector<float> low_tone(kFullBandSize);
  std::vector<float> high_tone(kFullBandSize);
  std::vector<float> input(kFullBandSize);
  for (size_t i = 0; i < kFullBandSize; ++i) {
    const float time_s = static_cast<float>(i) / kSampleRateHz;
    low_tone[i] = std::sin(kTwoPi * kLowToneHz * time_s);
    high_tone[i] = std::sin(kTwoPi * kHighToneHz * time_s);
    input[i] =
        low_tone[i] + std::sin(kTwoPi * kGainToneHz * time_s) + high_tone[i];
  }

  BandSplitVoiceIsolation band_split(
      CreateScalingModel(speech_mask, /*model_gain=*/1.0f));
  WindowedFft windowed_fft(/*fft_size=*/kFftSize);
  constexpr size_t kPffftAlignment = 16;
  std::vector<float> dfts(BandSplit::kFrameSize);
  base::AlignedHeapArray<float> processed_dfts =
      base::AlignedUninit<float>(BandSplit::kFrameSize, kPffftAlignment);
  std::vector<float> output(kFullBandSize);

  // The first frame fills the overlap-add history of the inverse STFT. Every
  // tone repeats each frame, so the later frames reach steady state.
  constexpr int kNumFrames = 3;
  for (int frame = 0; frame < kNumFrames; ++frame) {
    windowed_fft.ForwardTransform(input, dfts);
    band_split.ProcessAudio(dfts, processed_dfts);
    windowed_fft.InverseTransform(processed_dfts, output);
  }

  // Correlate the last output frame with each reference tone.
  ToneCorrelations correlations;
  for (size_t i = 0; i < kFullBandSize; ++i) {
    correlations.low_band += output[i] * low_tone[i];
    correlations.high_band += output[i] * high_tone[i];
  }
  return correlations;
}

}  // namespace

// Verifies that a BandSplitVoiceIsolation can be constructed around an inner
// 16 kHz component and that its frame size has the expected absolute value:
// two concatenated 960-coefficient 48 kHz FFTs, for a total of 1920 floats.
TEST(BandSplitVoiceIsolationTest, Creation) {
  auto mock_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_inner, FrameSize()).WillRepeatedly(Return(kInnerFrameSize));
  EXPECT_CALL(*mock_inner, FramesPerSecond())
      .WillRepeatedly(Return(kFramesPerSecond));

  BandSplitVoiceIsolation band_split(std::move(mock_inner));
  EXPECT_EQ(band_split.FrameSize(), kOuterFrameSize);
}

// In contrast to `Creation`, which checks the absolute outer frame size, this
// test verifies how the outer timing parameters are derived from the inner
// component: the frame size is scaled by the 48 kHz / 16 kHz ratio (three
// times the inner frame size), while the frame rate is the same as the inner
// one, since both components process the same 20 ms frames. The band split
// keeps no history, so its algorithmic delay is the inner one.
TEST(BandSplitVoiceIsolationTest, FrameSizeAndDelay) {
  constexpr base::TimeDelta kInnerDelay = base::Milliseconds(5);
  auto mock_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_inner, FrameSize()).WillRepeatedly(Return(kInnerFrameSize));
  EXPECT_CALL(*mock_inner, FramesPerSecond())
      .WillRepeatedly(Return(kFramesPerSecond));
  EXPECT_CALL(*mock_inner, AlgorithmicDelay())
      .WillRepeatedly(Return(kInnerDelay));

  BandSplitVoiceIsolation band_split(std::move(mock_inner));
  EXPECT_EQ(band_split.FrameSize(), kBandRatio * kInnerFrameSize);
  EXPECT_EQ(band_split.FramesPerSecond(), kFramesPerSecond);
  EXPECT_EQ(band_split.AlgorithmicDelay(), kInnerDelay);
}

// The internal component must be the 16 kHz model.
TEST(BandSplitVoiceIsolationDeathTest, DiesOnWrongInternalFrameSize) {
  auto mock_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_inner, FrameSize())
      .WillRepeatedly(Return(kInnerFrameSize / kNumDfts));
  EXPECT_CALL(*mock_inner, FramesPerSecond())
      .WillRepeatedly(Return(kFramesPerSecond));

  EXPECT_CHECK_DEATH(BandSplitVoiceIsolation(std::move(mock_inner)));
}

// The internal component must run at the 16 kHz model frame rate.
TEST(BandSplitVoiceIsolationDeathTest, DiesOnWrongInternalFramesPerSecond) {
  auto mock_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_inner, FrameSize()).WillRepeatedly(Return(kInnerFrameSize));
  EXPECT_CALL(*mock_inner, FramesPerSecond())
      .WillRepeatedly(Return(kNumDfts * kFramesPerSecond));

  EXPECT_CHECK_DEATH(BandSplitVoiceIsolation(std::move(mock_inner)));
}

TEST(BandSplitVoiceIsolationTest, ClearBuffersPropagatesToInternalComponent) {
  auto mock_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_inner, FrameSize()).WillRepeatedly(Return(kInnerFrameSize));
  EXPECT_CALL(*mock_inner, FramesPerSecond())
      .WillRepeatedly(Return(kFramesPerSecond));
  EXPECT_CALL(*mock_inner, ClearBuffers()).Times(1);

  BandSplitVoiceIsolation band_split(std::move(mock_inner));
  band_split.ClearBuffers();
}

// The speech mask must have one value per complex bin of a 16 kHz DFT.
TEST(BandSplitVoiceIsolationDeathTest, DiesOnWrongSpeechMaskSize) {
  std::array<float, kSpeechMaskSize - 1> speech_mask;
  speech_mask.fill(1.0f);
  BandSplitVoiceIsolation band_split(
      CreateScalingModel(speech_mask, /*model_gain=*/1.0f));
  const std::vector<float> input = MakeRampInput();
  std::vector<float> output(BandSplit::kFrameSize);

  EXPECT_CHECK_DEATH(band_split.ProcessAudio(input, output));
}

// With a passthrough model and a unit speech mask, the band split returns its
// input, and the model receives the low band of each 48 kHz DFT.
TEST(BandSplitVoiceIsolationTest,
     ProcessAudioPassthroughWithUnitMaskIsIdentity) {
  std::array<float, kSpeechMaskSize> speech_mask;
  speech_mask.fill(1.0f);
  std::unique_ptr<NiceMockComponent> model =
      CreateScalingModel(speech_mask, /*model_gain=*/1.0f);

  // Capture the DFTs that the model receives.
  std::vector<float> model_input;
  ON_CALL(*model, ProcessAudio(_, _))
      .WillByDefault([&model_input](base::span<const float> input,
                                    base::span<float> output) {
        model_input.assign(input.begin(), input.end());
        output.copy_from_nonoverlapping(input);
      });

  BandSplitVoiceIsolation band_split(std::move(model));
  const std::vector<float> input = MakeRampInput();
  std::vector<float> output(BandSplit::kFrameSize);
  band_split.ProcessAudio(input, output);

  // The model must receive the low band of each 48 kHz DFT scaled by 1/3, with
  // the real part of the 8 kHz bin in the Nyquist slot. The identity check
  // below cannot detect a missing or inverted scaling, since the forward (1/3)
  // and inverse (3) scale factors cancel out.
  ASSERT_EQ(model_input.size(), kInnerFrameSize);
  for (size_t dft = 0; dft < kNumDfts; ++dft) {
    const size_t inner_offset = dft * kLowBandSize;
    const size_t outer_offset = dft * kFullBandSize;
    for (size_t i = 0; i < kLowBandSize; ++i) {
      const size_t source_index = i == kNyquistIndex ? kLowBandSize : i;
      EXPECT_NEAR(model_input[inner_offset + i],
                  input[outer_offset + source_index] * kForwardScale,
                  kTolerance)
          << "DFT " << dft << ", index " << i;
    }
  }

  // Both DFTs get a high band gain of 1, so the crossfade blends two equal
  // values and every float of the output equals the input.
  EXPECT_THAT(output, Pointwise(FloatNear(kTolerance), input));
}

// A uniform speech mask of 0.5 halves the high band of both DFTs and blends
// the model output with the halved input across the crossfade.
TEST(BandSplitVoiceIsolationTest, ProcessAudioWithHalfMaskScalesHighBand) {
  std::array<float, kSpeechMaskSize> speech_mask;
  speech_mask.fill(0.5f);
  BandSplitVoiceIsolation band_split(
      CreateScalingModel(speech_mask, /*model_gain=*/1.0f));
  const std::vector<float> input = MakeRampInput();
  std::vector<float> output(BandSplit::kFrameSize);
  band_split.ProcessAudio(input, output);

  // A uniform mask is its own weighted mean, and the high band of the second
  // DFT is quieter than that of the first one, so both DFTs get that gain.
  ExpectOutput(input, output, /*model_gain=*/1.0f, {0.5f, 0.5f});
}

// The high band gain weights the speech mask by the magnitude of the model
// input, so loud bins count more than quiet ones.
TEST(BandSplitVoiceIsolationTest, HighBandGainIsMagnitudeWeightedMeanOfMask) {
  // Pass the lower half of the averaged bins and block the upper half.
  constexpr size_t kGainMiddleBin = (kGainStartBin + kGainEndBin) / 2;
  std::array<float, kSpeechMaskSize> speech_mask = {};
  for (size_t bin = kGainStartBin; bin < kGainMiddleBin; ++bin) {
    speech_mask[bin] = 1.0f;
  }

  // Give the passed bins of the first DFT (re, im) = (1, 0) (magnitude 1) and
  // the blocked bins (re, im) = (3, 4) (magnitude 5). Both halves have the
  // same number of bins, so the weighted mean is 1 / (1 + 5) = 1/6, while a
  // plain mean would give 0.5.
  constexpr float kLoudReal = 3.0f;
  constexpr float kLoudImag = 4.0f;
  constexpr float kLoudMagnitude = 5.0f;
  std::vector<float> input(BandSplit::kFrameSize, 1.0f);
  for (size_t bin = kGainStartBin; bin < kGainMiddleBin; ++bin) {
    input[2 * bin + 1] = 0.0f;
  }
  for (size_t bin = kGainMiddleBin; bin < kGainEndBin; ++bin) {
    input[2 * bin] = kLoudReal;
    input[2 * bin + 1] = kLoudImag;
  }

  BandSplitVoiceIsolation band_split(
      CreateScalingModel(speech_mask, /*model_gain=*/1.0f));
  std::vector<float> output(BandSplit::kFrameSize);
  band_split.ProcessAudio(input, output);

  // Both DFTs have the same high band, so the second one keeps the gain of the
  // first one.
  constexpr float kExpectedGain = 1.0f / (1.0f + kLoudMagnitude);
  ExpectOutput(input, output, /*model_gain=*/1.0f,
               {kExpectedGain, kExpectedGain});
}

// The speech mask belongs to the first DFT. A louder high band in the second
// DFT, which may be a noise burst, lowers the gain of that DFT by the ratio of
// the RMS levels.
TEST(BandSplitVoiceIsolationTest, LouderSecondDftLowersItsHighBandGain) {
  std::array<float, kSpeechMaskSize> speech_mask;
  speech_mask.fill(0.5f);
  BandSplitVoiceIsolation band_split(
      CreateScalingModel(speech_mask, /*model_gain=*/1.0f));
  constexpr float kLouderLevel = 4.0f;
  const std::vector<float> input =
      MakeLevelInput(/*second_high_band_level=*/kLouderLevel);
  std::vector<float> output(BandSplit::kFrameSize);
  band_split.ProcessAudio(input, output);

  ExpectOutput(input, output, /*model_gain=*/1.0f, {0.5f, 0.5f / kLouderLevel});
}

// A quieter high band in the second DFT keeps the gain of the first DFT.
TEST(BandSplitVoiceIsolationTest, QuieterSecondDftKeepsFirstGain) {
  std::array<float, kSpeechMaskSize> speech_mask;
  speech_mask.fill(0.5f);
  BandSplitVoiceIsolation band_split(
      CreateScalingModel(speech_mask, /*model_gain=*/1.0f));
  constexpr float kQuieterLevel = 0.5f;
  const std::vector<float> input =
      MakeLevelInput(/*second_high_band_level=*/kQuieterLevel);
  std::vector<float> output(BandSplit::kFrameSize);
  band_split.ProcessAudio(input, output);

  ExpectOutput(input, output, /*model_gain=*/1.0f, {0.5f, 0.5f});
}

// With a silent model, only the scaled input remains: none of it below
// 7550 Hz, a growing share across the crossfade, and all of it from 8 kHz up.
TEST(BandSplitVoiceIsolationTest, CrossfadesToScaledInputWhenModelIsSilent) {
  std::array<float, kSpeechMaskSize> speech_mask;
  speech_mask.fill(0.5f);
  BandSplitVoiceIsolation band_split(
      CreateScalingModel(speech_mask, /*model_gain=*/0.0f));
  const std::vector<float> input = MakeRampInput();
  std::vector<float> output(BandSplit::kFrameSize);
  band_split.ProcessAudio(input, output);

  ExpectOutput(input, output, /*model_gain=*/0.0f, {0.5f, 0.5f});
}

// When the low band is silent, the averaged bins carry no energy, so the high
// band gain is 0 instead of the 0 / 0 of a plain ratio, and the high band is
// removed.
TEST(BandSplitVoiceIsolationTest, SilentLowBandRemovesHighBand) {
  std::array<float, kSpeechMaskSize> speech_mask;
  speech_mask.fill(1.0f);
  BandSplitVoiceIsolation band_split(
      CreateScalingModel(speech_mask, /*model_gain=*/1.0f));

  // Silence DC and every bin below 8 kHz of both DFTs. Keep the 24 kHz Nyquist
  // slot, which belongs to the high band.
  std::vector<float> input = MakeRampInput();
  for (size_t dft = 0; dft < kNumDfts; ++dft) {
    for (size_t i = 0; i < kLowBandSize; ++i) {
      if (i == kNyquistIndex) {
        continue;
      }
      input[dft * kFullBandSize + i] = 0.0f;
    }
  }
  std::vector<float> output(BandSplit::kFrameSize);
  band_split.ProcessAudio(input, output);

  EXPECT_THAT(output, Each(0.0f));
}

// With a unit speech mask, a passthrough model keeps both the 1 kHz tone of the
// low band and the 10 kHz tone of the high band.
TEST(BandSplitVoiceIsolationTest, SpectrumKeepsHighBandWithUnitMask) {
  std::array<float, kSpeechMaskSize> speech_mask;
  speech_mask.fill(1.0f);
  const ToneCorrelations correlations = ProcessThreeTones(speech_mask);

  EXPECT_NEAR(correlations.low_band, kUnchangedToneCorrelation,
              kCorrelationTolerance);
  EXPECT_NEAR(correlations.high_band, kUnchangedToneCorrelation,
              kCorrelationTolerance);
}

// With a zero speech mask, the 1 kHz tone of the low band stays and the 10 kHz
// tone of the high band is removed.
TEST(BandSplitVoiceIsolationTest, SpectrumRemovesHighBandWithZeroMask) {
  const std::array<float, kSpeechMaskSize> speech_mask = {};
  const ToneCorrelations correlations = ProcessThreeTones(speech_mask);

  EXPECT_NEAR(correlations.low_band, kUnchangedToneCorrelation,
              kCorrelationTolerance);
  EXPECT_NEAR(correlations.high_band, 0.0f, kCorrelationTolerance);
}

}  // namespace media
