// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/crossfader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>

#include "base/containers/span.h"
#include "base/test/gtest_util.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {
namespace {

constexpr int kMonoChannels = 1;
constexpr int kStereoChannels = 2;
constexpr int kFramesPerBuffer = 480;
constexpr float kFromLevel = 0.8f;
constexpr float kToLevel = -0.3f;
constexpr float kTolerance = 1e-6f;

// Reference window: w[n] = sin^2(pi * (n + 1) / (2 * num_frames)).
float ExpectedFadeIn(int index, int num_frames) {
  const double sine =
      std::sin(std::numbers::pi * (index + 1) / (2.0 * num_frames));
  return static_cast<float>(sine * sine);
}

std::unique_ptr<media::AudioBus> CreateBusWithLevel(int channels,
                                                    int frames,
                                                    float level) {
  std::unique_ptr<media::AudioBus> bus =
      media::AudioBus::Create(channels, frames);
  for (int ch = 0; ch < channels; ++ch) {
    std::ranges::fill(bus->channel(ch), level);
  }
  return bus;
}

// Checks that `bus` holds the crossfade of constant `from_level` and
// `to_level` on every channel.
void ExpectCrossfadeOfLevels(const media::AudioBus& bus,
                             float from_level,
                             float to_level) {
  for (int ch = 0; ch < bus.channels(); ++ch) {
    base::span<const float> channel = bus.channel(ch);
    for (int i = 0; i < bus.frames(); ++i) {
      const float fade_in = ExpectedFadeIn(i, bus.frames());
      EXPECT_NEAR(channel[i],
                  (1.0f - fade_in) * from_level + fade_in * to_level,
                  kTolerance)
          << "channel " << ch << ", frame " << i;
    }
  }
}

TEST(CrossfaderTest, CrossfadeFromSilenceToDcIsTheWindow) {
  const Crossfader crossfader(kFramesPerBuffer);
  std::unique_ptr<media::AudioBus> silence =
      CreateBusWithLevel(kMonoChannels, kFramesPerBuffer, /*level=*/0.0f);
  std::unique_ptr<media::AudioBus> dc =
      CreateBusWithLevel(kMonoChannels, kFramesPerBuffer, /*level=*/1.0f);
  std::unique_ptr<media::AudioBus> destination =
      media::AudioBus::Create(kMonoChannels, kFramesPerBuffer);

  crossfader.Crossfade(/*from=*/*silence, /*to=*/*dc, *destination);

  // The window matches the reference formula and never decreases.
  base::span<const float> window = destination->channel(0);
  for (int i = 0; i < kFramesPerBuffer; ++i) {
    EXPECT_NEAR(window[i], ExpectedFadeIn(i, kFramesPerBuffer), kTolerance)
        << "frame " << i;
    if (i > 0) {
      EXPECT_GE(window[i], window[i - 1]) << "frame " << i;
    }
  }

  // Edges: starts near silence, reaches half gain at the midpoint and ends at
  // unity gain so that the following buffer continues without a step.
  EXPECT_GT(window.front(), 0.0f);
  EXPECT_NEAR(window[kFramesPerBuffer / 2 - 1], 0.5f, kTolerance);
  EXPECT_FLOAT_EQ(window.back(), 1.0f);
}

TEST(CrossfaderTest, CrossfadeGainsAreComplementary) {
  const Crossfader crossfader(kFramesPerBuffer);
  std::unique_ptr<media::AudioBus> from =
      CreateBusWithLevel(kMonoChannels, kFramesPerBuffer, kFromLevel);
  std::unique_ptr<media::AudioBus> to =
      CreateBusWithLevel(kMonoChannels, kFramesPerBuffer, kToLevel);
  std::unique_ptr<media::AudioBus> destination =
      media::AudioBus::Create(kMonoChannels, kFramesPerBuffer);

  crossfader.Crossfade(*from, *to, *destination);

  ExpectCrossfadeOfLevels(*destination, kFromLevel, kToLevel);
  EXPECT_FLOAT_EQ(destination->channel(0).back(), kToLevel);
}

TEST(CrossfaderTest, CrossfadeAliasingFrom) {
  const Crossfader crossfader(kFramesPerBuffer);
  std::unique_ptr<media::AudioBus> from =
      CreateBusWithLevel(kMonoChannels, kFramesPerBuffer, kFromLevel);
  std::unique_ptr<media::AudioBus> to =
      CreateBusWithLevel(kMonoChannels, kFramesPerBuffer, kToLevel);

  crossfader.Crossfade(*from, *to, /*destination=*/*from);

  ExpectCrossfadeOfLevels(*from, kFromLevel, kToLevel);
}

TEST(CrossfaderTest, CrossfadeAliasingTo) {
  const Crossfader crossfader(kFramesPerBuffer);
  std::unique_ptr<media::AudioBus> from =
      CreateBusWithLevel(kMonoChannels, kFramesPerBuffer, kFromLevel);
  std::unique_ptr<media::AudioBus> to =
      CreateBusWithLevel(kMonoChannels, kFramesPerBuffer, kToLevel);

  crossfader.Crossfade(*from, *to, /*destination=*/*to);

  ExpectCrossfadeOfLevels(*to, kFromLevel, kToLevel);
}

TEST(CrossfaderTest, EveryChannelIsProcessed) {
  constexpr std::array<float, kStereoChannels> kFromLevels = {0.25f, -0.5f};
  constexpr std::array<float, kStereoChannels> kToLevels = {-0.75f, 1.0f};
  const Crossfader crossfader(kFramesPerBuffer);
  std::unique_ptr<media::AudioBus> from =
      media::AudioBus::Create(kStereoChannels, kFramesPerBuffer);
  std::unique_ptr<media::AudioBus> to =
      media::AudioBus::Create(kStereoChannels, kFramesPerBuffer);
  std::unique_ptr<media::AudioBus> crossfaded =
      media::AudioBus::Create(kStereoChannels, kFramesPerBuffer);

  // Use a different level per channel to catch channel mix-ups.
  for (int ch = 0; ch < kStereoChannels; ++ch) {
    std::ranges::fill(from->channel(ch), kFromLevels[ch]);
    std::ranges::fill(to->channel(ch), kToLevels[ch]);
  }

  crossfader.Crossfade(*from, *to, *crossfaded);

  for (int ch = 0; ch < kStereoChannels; ++ch) {
    base::span<const float> crossfaded_channel = crossfaded->channel(ch);
    for (int i = 0; i < kFramesPerBuffer; ++i) {
      const float fade_in = ExpectedFadeIn(i, kFramesPerBuffer);
      EXPECT_NEAR(crossfaded_channel[i],
                  (1.0f - fade_in) * kFromLevels[ch] + fade_in * kToLevels[ch],
                  kTolerance)
          << "channel " << ch << ", frame " << i;
    }
  }
}

TEST(CrossfaderTest, SingleFrameBuffer) {
  constexpr int kSingleFrame = 1;
  const Crossfader crossfader(kSingleFrame);
  std::unique_ptr<media::AudioBus> from =
      CreateBusWithLevel(kMonoChannels, kSingleFrame, kFromLevel);
  std::unique_ptr<media::AudioBus> to =
      CreateBusWithLevel(kMonoChannels, kSingleFrame, kToLevel);
  std::unique_ptr<media::AudioBus> destination =
      media::AudioBus::Create(kMonoChannels, kSingleFrame);

  // With a single frame the window is {1.0}: the output is `to` right away.
  crossfader.Crossfade(*from, *to, *destination);

  EXPECT_FLOAT_EQ(destination->channel(0).front(), kToLevel);
}

TEST(CrossfaderDeathTest, ZeroFramesCrashes) {
  EXPECT_CHECK_DEATH(Crossfader(/*frames_per_buffer=*/0));
}

}  // namespace
}  // namespace audio
