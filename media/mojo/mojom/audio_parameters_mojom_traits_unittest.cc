// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_parameters_mojom_traits.h"

#include <vector>

#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_parameters.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(AudioParametersStructTraitsTest, NormalRoundTrip) {
  AudioParameters input(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        ChannelLayoutConfig::Stereo(),
                        /*sample_rate=*/48000, /*frames_per_buffer=*/480);
  input.set_effects(AudioParameters::ECHO_CANCELLER | AudioParameters::DUCKING);
  input.set_mic_positions({Point(/*x=*/1.0f, /*y=*/2.0f, /*z=*/3.0f),
                           Point(/*x=*/4.0f, /*y=*/5.0f, /*z=*/6.0f)});
  input.set_latency_tag(AudioLatency::Type::kRtc);

  AudioParameters::HardwareCapabilities hardware_capabilities(
      /*min_frames_per_buffer=*/128, /*max_frames_per_buffer=*/1024,
      /*default_frames_per_buffer=*/512, /*require_offload=*/false);
  hardware_capabilities.require_encapsulation = true;
  input.set_hardware_capabilities(hardware_capabilities);

  AudioParameters output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioParameters>(
      input, output));

  EXPECT_EQ(output.format(), input.format());
  EXPECT_EQ(output.channel_layout_config(), input.channel_layout_config());
  EXPECT_EQ(output.sample_rate(), input.sample_rate());
  EXPECT_EQ(output.frames_per_buffer(), input.frames_per_buffer());
  EXPECT_EQ(output.effects(), input.effects());
  EXPECT_EQ(output.mic_positions(), input.mic_positions());
  EXPECT_EQ(output.latency_tag(), input.latency_tag());
  EXPECT_EQ(output.hardware_capabilities(), input.hardware_capabilities());
}

TEST(AudioParametersStructTraitsTest, DiscreteRoundTrip) {
  const AudioParameters input(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                              ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, 5),
                              /*sample_rate=*/44100, /*frames_per_buffer=*/256);
  AudioParameters output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioParameters>(
      input, output));

  EXPECT_EQ(output.format(), input.format());
  EXPECT_EQ(output.channel_layout_config(), input.channel_layout_config());
  EXPECT_EQ(output.sample_rate(), input.sample_rate());
  EXPECT_EQ(output.frames_per_buffer(), input.frames_per_buffer());
}

mojom::AudioParametersPtr CreateValidMojomAudioParameters() {
  auto input = mojom::AudioParameters::New();
  input->format = AudioParameters::AUDIO_PCM_LOW_LATENCY;
  input->channel_layout_config = ChannelLayoutConfig::Stereo();
  input->sample_rate = 48000;
  input->frames_per_buffer = 480;
  return input;
}

TEST(AudioParametersStructTraitsTest, InvalidSampleRate) {
  auto input = CreateValidMojomAudioParameters();
  input->sample_rate = -1000;

  AudioParameters output;
  EXPECT_FALSE(mojom::AudioParameters::Deserialize(
      mojom::AudioParameters::Serialize(&input), &output));
}

TEST(AudioParametersStructTraitsTest, InvalidFramesPerBuffer) {
  auto input = CreateValidMojomAudioParameters();
  input->frames_per_buffer = -5;

  AudioParameters output;
  EXPECT_FALSE(mojom::AudioParameters::Deserialize(
      mojom::AudioParameters::Serialize(&input), &output));
}

TEST(AudioParametersStructTraitsTest, InvalidHardwareCapabilities) {
  auto input = CreateValidMojomAudioParameters();

  // Set invalid min_frames_per_buffer.
  input->hardware_capabilities = media::AudioParameters::HardwareCapabilities(
      /*min_frames_per_buffer=*/-10, /*max_frames_per_buffer=*/512);

  AudioParameters output;
  EXPECT_FALSE(mojom::AudioParameters::Deserialize(
      mojom::AudioParameters::Serialize(&input), &output));
}

TEST(AudioParametersStructTraitsTest, InvalidChannelLayoutDiscreteTooLarge) {
  auto input = CreateValidMojomAudioParameters();
  input->channel_layout_config =
      media::ChannelLayoutConfig(media::CHANNEL_LAYOUT_DISCRETE, 10000);

  AudioParameters output;
  EXPECT_FALSE(mojom::AudioParameters::Deserialize(
      mojom::AudioParameters::Serialize(&input), &output));
}

}  // namespace media
