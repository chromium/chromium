// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_level_calculator.h"

#include <algorithm>

#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

constexpr int kDefaultChannels = 2;
constexpr int kDefaultFrames = 20;
constexpr int kUpdateFrequency = 10;

void FillAudioBus(media::AudioBus* bus, float value) {
  for (media::AudioBus::Channel channel_span : bus->AllChannels()) {
    std::ranges::fill(channel_span, value);
  }
}

std::unique_ptr<media::AudioBus> CreateAndFillAudioBus(float value) {
  auto audio_bus = media::AudioBus::Create(kDefaultChannels, kDefaultFrames);
  FillAudioBus(audio_bus.get(), value);
  return audio_bus;
}

class MediaStreamAudioLevelCalculatorTest : public ::testing::Test {
 protected:
  MediaStreamAudioLevelCalculator calculator_;
};

// Validates that the audio level is correctly calculated for silent audio,
// both with and without the `assume_nonzero_energy` flag.
TEST_F(MediaStreamAudioLevelCalculatorTest, Silence) {
  auto audio_bus = CreateAndFillAudioBus(0.0f);

  // Test with silence and assume_nonzero_energy = false.
  {
    MediaStreamAudioLevelCalculator calculator;
    for (int i = 0; i < kUpdateFrequency; ++i) {
      calculator.Calculate(*audio_bus, false /* assume_nonzero_energy */);
    }
    EXPECT_FLOAT_EQ(calculator.level()->GetCurrent(), 0.0f);
  }

  // Test with silence and assume_nonzero_energy = true.
  // The level should be small but non-zero.
  {
    MediaStreamAudioLevelCalculator calculator;
    for (int i = 0; i < kUpdateFrequency; ++i) {
      calculator.Calculate(*audio_bus, true /* assume_nonzero_energy */);
    }
    const float current_level = calculator.level()->GetCurrent();
    EXPECT_GT(current_level, 0.0f);
    EXPECT_LT(current_level, 0.0001f);
  }
}

// Validates that the audio level is correctly calculated for a constant signal.
TEST_F(MediaStreamAudioLevelCalculatorTest, ConstantSignal) {
  auto audio_bus = CreateAndFillAudioBus(0.5f);
  for (int i = 0; i < kUpdateFrequency; ++i) {
    calculator_.Calculate(*audio_bus, false);
  }
  EXPECT_FLOAT_EQ(calculator_.level()->GetCurrent(), 0.5f);
}

// Validates that signals at or outside the [-1.0, 1.0] range are clipped
// to 1.0.
TEST_F(MediaStreamAudioLevelCalculatorTest, SignalClipping) {
  constexpr float kMaxAmplitude = 1.0f;

  auto run_clipping_test = [](float input_value) {
    MediaStreamAudioLevelCalculator calculator;
    auto audio_bus = CreateAndFillAudioBus(input_value);
    for (int i = 0; i < kUpdateFrequency; ++i) {
      calculator.Calculate(*audio_bus, false);
    }
    EXPECT_FLOAT_EQ(calculator.level()->GetCurrent(), kMaxAmplitude);
  };

  run_clipping_test(kMaxAmplitude);
  run_clipping_test(-kMaxAmplitude);
  run_clipping_test(2 * kMaxAmplitude);
  run_clipping_test(-2 * kMaxAmplitude);
}

// Validates that the audio level is correctly calculated for an alternating
// positive and negative signal, ensuring it uses the absolute maximum.
TEST_F(MediaStreamAudioLevelCalculatorTest, AlternatingSignal) {
  auto audio_bus = media::AudioBus::Create(kDefaultChannels, kDefaultFrames);
  for (auto channel_span : audio_bus->AllChannels()) {
    for (int j = 0; j < kDefaultFrames; ++j) {
      channel_span[j] = (j % 2 == 0) ? 0.7f : -0.9f;
    }
  }

  for (int i = 0; i < kUpdateFrequency; ++i) {
    calculator_.Calculate(*audio_bus, false);
  }
  EXPECT_FLOAT_EQ(calculator_.level()->GetCurrent(), 0.9f);
}

// Validates that the audio level is not updated on every call to Calculate(),
// but rather at a specific frequency (every 10 calls).
TEST_F(MediaStreamAudioLevelCalculatorTest, LevelDoesNotUpdateImmediately) {
  auto audio_bus = CreateAndFillAudioBus(0.9f);

  // Initial level is 0.
  EXPECT_FLOAT_EQ(calculator_.level()->GetCurrent(), 0.0f);

  // Call Calculate() one fewer times than needed for an update. The level
  // should not be updated yet.
  for (int i = 0; i < kUpdateFrequency - 1; ++i) {
    calculator_.Calculate(*audio_bus, false);
    EXPECT_FLOAT_EQ(calculator_.level()->GetCurrent(), 0.0f);
  }

  // The next call should trigger an update.
  calculator_.Calculate(*audio_bus, false);
  EXPECT_FLOAT_EQ(calculator_.level()->GetCurrent(), 0.9f);
}

// Validates that the internal `max_amplitude_` decays over time when no new
// loud signals are received.
TEST_F(MediaStreamAudioLevelCalculatorTest, AmplitudeDecay) {
  auto audio_bus = CreateAndFillAudioBus(0.8f);
  for (int i = 0; i < kUpdateFrequency; ++i) {
    calculator_.Calculate(*audio_bus, false);
  }
  float current_level = calculator_.level()->GetCurrent();
  float last_level = current_level;
  EXPECT_FLOAT_EQ(current_level, 0.8f);

  // Now, send silence and check for decay.
  audio_bus = CreateAndFillAudioBus(0.0f);

  for (int loops = 0; loops < 3; ++loops) {
    for (int i = 0; i < kUpdateFrequency; ++i) {
      calculator_.Calculate(*audio_bus, false);
    }

    current_level = calculator_.level()->GetCurrent();
    // We use a decay factor of 1/4 per update. This is an implementation detail
    // which is too specific for a unit test, but we want to make sure we decay
    // by a factor, rather than a constant amount.
    EXPECT_NEAR(last_level / current_level, 4.0f, 0.01f);
    last_level = current_level;
  }
}

// Validates that the calculator correctly identifies the maximum amplitude
// across multiple audio channels.
TEST_F(MediaStreamAudioLevelCalculatorTest, MultiChannelMaxAmplitude) {
  auto audio_bus = media::AudioBus::Create(kDefaultChannels, kDefaultFrames);
  // Fill channel 0 with 0.5 and channel 1 with 0.8.
  std::ranges::fill(audio_bus->channel(0), 0.5f);
  std::ranges::fill(audio_bus->channel(1), 0.8f);

  for (int i = 0; i < kUpdateFrequency; ++i) {
    calculator_.Calculate(*audio_bus, false);
  }
  // The level should be based on the max of the two channels.
  EXPECT_FLOAT_EQ(calculator_.level()->GetCurrent(), 0.8f);
}

// Validates that the destructor of MediaStreamAudioLevelCalculator sets the
// audio level to 0.0f.
TEST_F(MediaStreamAudioLevelCalculatorTest, DestructorSetsLevelToZero) {
  scoped_refptr<MediaStreamAudioLevelCalculator::Level> level;
  {
    MediaStreamAudioLevelCalculator calculator;
    level = calculator.level();
    auto audio_bus = CreateAndFillAudioBus(0.7f);
    for (int i = 0; i < kUpdateFrequency; ++i) {
      calculator.Calculate(*audio_bus, false);
    }
    EXPECT_NE(level->GetCurrent(), 0.0f);
  }  // calculator goes out of scope here.

  EXPECT_FLOAT_EQ(level->GetCurrent(), 0.0f);
}

}  // namespace
}  // namespace blink
