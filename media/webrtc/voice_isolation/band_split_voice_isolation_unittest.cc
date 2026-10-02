// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/band_split_voice_isolation.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "media/webrtc/voice_isolation/mock_voice_isolation.h"
#include "media/webrtc/voice_isolation/stft_voice_isolation.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using testing::_;
using testing::Return;

namespace {

using BandSplit = BandSplitVoiceIsolation;

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

TEST(BandSplitVoiceIsolationTest, ProcessAudioLowBandPassthrough) {
  auto mock_passthrough_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_passthrough_inner, FrameSize())
      .WillRepeatedly(Return(kInnerFrameSize));
  EXPECT_CALL(*mock_passthrough_inner, FramesPerSecond())
      .WillRepeatedly(Return(kFramesPerSecond));

  // Capture the DFTs received by the inner 16 kHz component.
  std::vector<float> inner_input;
  EXPECT_CALL(*mock_passthrough_inner, ProcessAudio(_, _))
      .WillRepeatedly([&inner_input](base::span<const float> inner_in,
                                     base::span<float> inner_out) {
        inner_input.assign(inner_in.begin(), inner_in.end());
        inner_out.copy_from_nonoverlapping(inner_in);
      });

  BandSplitVoiceIsolation band_split(std::move(mock_passthrough_inner));
  const size_t frame_size = band_split.FrameSize();
  std::vector<float> input(frame_size, 0.0f);
  std::vector<float> output(frame_size, 0.0f);

  // Fill input with incrementing values to test coefficient routing.
  for (size_t i = 0; i < frame_size; ++i) {
    input[i] = static_cast<float>(i) * 0.1f;
  }

  band_split.ProcessAudio(input, output);

  // Verify that the inner component receives the low band of each 48kHz FFT
  // scaled by 1/3, with the 8kHz real part moved into the Nyquist slot. The
  // passthrough round trip below cannot detect a missing or inverted scaling,
  // since the forward (1/3) and inverse (3) scale factors cancel out.
  ASSERT_EQ(inner_input.size(), kInnerFrameSize);
  for (size_t fft = 0; fft < kNumDfts; ++fft) {
    const size_t inner_offset = fft * kLowBandSize;
    const size_t outer_offset = fft * kFullBandSize;
    for (size_t i = 0; i < kLowBandSize; ++i) {
      const size_t source_index = i == kNyquistIndex ? kLowBandSize : i;
      EXPECT_NEAR(inner_input[inner_offset + i],
                  input[outer_offset + source_index] * kForwardScale,
                  kTolerance)
          << "FFT " << fft << ", index " << i;
    }
  }

  // Verify that the first FFT's low band (DC and bins up to 8kHz, including the
  // 8kHz real part at index 320) is preserved, while the 24kHz Nyquist slot
  // (index 1), the 8kHz imaginary part (index 321), and upper bands (indices
  // 322..959) are zeroed.
  for (size_t i = 0; i < kFullBandSize; ++i) {
    if (i == kNyquistIndex || i > kLowBandSize) {
      EXPECT_NEAR(output[i], 0.0f, kTolerance) << "Index " << i;
    } else {
      EXPECT_NEAR(output[i], input[i], kTolerance) << "Index " << i;
    }
  }

  // Verify that the second FFT's low band (indices 960 and 962..1280) is
  // preserved, while its 24kHz Nyquist slot (index 961), its 8kHz imaginary
  // part (index 1281), and upper bands (indices 1282..1919) are zeroed.
  for (size_t i = kFullBandSize; i < frame_size; ++i) {
    const size_t offset = i - kFullBandSize;
    if (offset == kNyquistIndex || offset > kLowBandSize) {
      EXPECT_NEAR(output[i], 0.0f, kTolerance) << "Index " << i;
    } else {
      EXPECT_NEAR(output[i], input[i], kTolerance) << "Index " << i;
    }
  }
}

TEST(BandSplitVoiceIsolationTest, BandSplitSpectrumVerification) {
  constexpr int kFftSize = 960;
  constexpr float kSampleRate = 48000.0f;
  constexpr float kLowFreqHz = 1000.0f;
  constexpr float kHighFreqHz = 10000.0f;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

  WindowedFft windowed_fft(/*fft_size=*/kFftSize);

  std::vector<float> input(kFftSize, 0.0f);
  std::vector<float> dfts(2 * kFftSize, 0.0f);
  std::vector<float> filtered_dfts(2 * kFftSize, 0.0f);
  std::vector<float> output(kFftSize, 0.0f);

  // Create a composite signal with 1000Hz (low band) and 10000Hz (high band).
  for (int i = 0; i < kFftSize; ++i) {
    const float time_sec = static_cast<float>(i) / kSampleRate;
    input[i] = std::sin(kTwoPi * kLowFreqHz * time_sec) +
               std::sin(kTwoPi * kHighFreqHz * time_sec);
  }

  auto mock_passthrough_inner = std::make_unique<MockVoiceIsolationComponent>();
  EXPECT_CALL(*mock_passthrough_inner, FrameSize())
      .WillRepeatedly(Return(kInnerFrameSize));
  EXPECT_CALL(*mock_passthrough_inner, FramesPerSecond())
      .WillRepeatedly(Return(kFramesPerSecond));
  EXPECT_CALL(*mock_passthrough_inner, ProcessAudio(_, _))
      .WillRepeatedly(
          [](base::span<const float> inner_in, base::span<float> inner_out) {
            inner_out.copy_from_nonoverlapping(inner_in);
          });

  BandSplitVoiceIsolation band_split(std::move(mock_passthrough_inner));

  // Process two frames to prime the overlap-add delay buffer and reach steady
  // state.
  for (int frame = 0; frame < 2; ++frame) {
    windowed_fft.ForwardTransform(input, dfts);
    band_split.ProcessAudio(dfts, filtered_dfts);
    windowed_fft.InverseTransform(filtered_dfts, output);
  }

  // Verify that BandSplitVoiceIsolation retains the 1000Hz low-band signal and
  // removes the 10000Hz high-band signal.
  float low_band_correlation = 0.0f;
  float high_band_correlation = 0.0f;
  for (int i = 0; i < kFftSize; ++i) {
    const float time_sec = static_cast<float>(i) / kSampleRate;
    const float low_sine = std::sin(kTwoPi * kLowFreqHz * time_sec);
    const float high_sine = std::sin(kTwoPi * kHighFreqHz * time_sec);
    low_band_correlation += output[i] * low_sine;
    high_band_correlation += output[i] * high_sine;
  }

  EXPECT_GT(low_band_correlation, 100.0f);
  EXPECT_LT(std::abs(high_band_correlation), 1.0f);
}

}  // namespace media
