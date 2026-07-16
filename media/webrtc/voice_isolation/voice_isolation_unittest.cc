// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/voice_isolation.h"

#include <memory>

#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(VoiceIsolationTest, UnsupportedFormatsCrashes) {
  AudioParameters unsupported_params(AudioParameters::AUDIO_BITSTREAM_AC3,
                                     ChannelLayoutConfig::Stereo(), 48000, 480);
  EXPECT_DEATH_IF_SUPPORTED(
      VoiceIsolation::CreateForTesting(unsupported_params), "");
}

TEST(VoiceIsolationTest, ProcessAudioCopiesToAllChannels) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), 48000, 480);
  std::unique_ptr<VoiceIsolation> voice_isolation =
      VoiceIsolation::CreateForTesting(params);
  ASSERT_NE(voice_isolation, nullptr);

  // Use a 3-channel bus to ensure copying happens to all other channels.
  std::unique_ptr<AudioBus> input_bus = AudioBus::Create(3, 480);
  std::unique_ptr<AudioBus> output_bus = AudioBus::Create(3, 480);

  // Fill input bus with dummy data.
  for (int i = 0; i < 480; ++i) {
    input_bus->channel(0)[i] = i * 0.001f;
    input_bus->channel(1)[i] = -1.0f;  // Ignored by passthrough
    input_bus->channel(2)[i] = -2.0f;  // Ignored by passthrough
  }

  // Clear output bus to verify changes.
  output_bus->Zero();

  voice_isolation->ProcessAudio(*input_bus, *output_bus);

  // Expected results:
  // - Channel 0 is processed
  // - Channel 1 and 2 are copied from output channel 0.
  for (int i = 0; i < 480; ++i) {
    float expected = i * 0.001f;
    EXPECT_FLOAT_EQ(output_bus->channel(0)[i], expected);
    EXPECT_FLOAT_EQ(output_bus->channel(1)[i], expected);
    EXPECT_FLOAT_EQ(output_bus->channel(2)[i], expected);
  }
}

}  // namespace media
