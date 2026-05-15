// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_decoder_config_mojom_traits.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
mojom::AudioDecoderConfigPtr CreateValidAudioDecoderConfig() {
  mojom::AudioDecoderConfigPtr input = mojom::AudioDecoderConfig::New();
  input->codec = AudioCodec::kAAC;
  input->sample_format = kSampleFormatU8;
  input->channel_layout = CHANNEL_LAYOUT_STEREO;
  input->channels = 2;
  input->extra_data = EmptyExtraData();
  input->samples_per_second = 48000;
  input->encryption_scheme = EncryptionScheme::kUnencrypted;
  input->seek_preroll = base::TimeDelta();
  input->profile = AudioCodecProfile::kUnknown;
  input->codec_delay = 0;
  input->should_discard_decoder_delay = true;
  input->target_output_channel_layout = ChannelLayoutConfig::Stereo();
  input->target_output_sample_format = kSampleFormatU8;

  return input;
}

bool CanBeCreated(mojom::AudioDecoderConfigPtr& input) {
  AudioDecoderConfig output;
  return mojo::test::SerializeAndDeserialize<mojom::AudioDecoderConfig>(input,
                                                                        output);
}
}  // namespace

const ChannelLayoutConfig kSurroundChannelLayout =
    ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_SURROUND>();

TEST(AudioDecoderConfigStructTraitsTest, Normal) {
  const std::vector<uint8_t> kExtraData =
      base::ToVector(base::as_byte_span("input extra data"));

  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, kSurroundChannelLayout,
                   48000, kExtraData, EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest, EmptyExtraData) {
  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, kSurroundChannelLayout,
                   48000, EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest, Encrypted) {
  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, kSurroundChannelLayout,
                   48000, EmptyExtraData(), EncryptionScheme::kCenc,
                   base::TimeDelta(), 0);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest, WithProfile) {
  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, kSurroundChannelLayout,
                   48000, EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  input.set_profile(AudioCodecProfile::kXHE_AAC);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest, DisableDiscardDecoderDelay) {
  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, kSurroundChannelLayout,
                   48000, EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  input.disable_discard_decoder_delay();
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
  EXPECT_FALSE(output.should_discard_decoder_delay());
}

TEST(AudioDecoderConfigStructTraitsTest, TargetOutputChannelLayout) {
  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, kSurroundChannelLayout,
                   48000, EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  input.set_target_output_channel_layout(
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>());
  input.set_target_output_sample_format(kSampleFormatDts);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
  EXPECT_EQ(output.target_output_channel_layout(),
            ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>());
  EXPECT_EQ(output.target_output_sample_format(), kSampleFormatDts);
}

TEST(AudioDecoderConfigStructTraitsTest, ChannelLayout) {
  mojom::AudioDecoderConfigPtr input = CreateValidAudioDecoderConfig();

  // The default config is STEREO and 2 channels.
  EXPECT_TRUE(CanBeCreated(input));

  // STEREO should be 2 channels.
  input->channel_layout = CHANNEL_LAYOUT_STEREO;
  input->channels = 1;
  EXPECT_FALSE(CanBeCreated(input));

  // DISCRETE can handle multiple channel counts, but not zero.
  input->channel_layout = CHANNEL_LAYOUT_DISCRETE;
  input->channels = 9;
  EXPECT_TRUE(CanBeCreated(input));

  input->channel_layout = CHANNEL_LAYOUT_DISCRETE;
  input->channels = 0;
  EXPECT_FALSE(CanBeCreated(input));

  // BITSTREAM means passthrough, it should always be zero channels.
  input->channel_layout = CHANNEL_LAYOUT_BITSTREAM;
  input->channels = 0;
  EXPECT_TRUE(CanBeCreated(input));

  input->channel_layout = CHANNEL_LAYOUT_BITSTREAM;
  input->channels = 1;
  EXPECT_FALSE(CanBeCreated(input));

  // Negative channel counts are invalid.
  input->channel_layout = CHANNEL_LAYOUT_DISCRETE;
  input->channels = -1;
  EXPECT_FALSE(CanBeCreated(input));

  // NONE must be zero.
  input->channel_layout = CHANNEL_LAYOUT_NONE;
  input->channels = 0;
  EXPECT_TRUE(CanBeCreated(input));

  input->channel_layout = CHANNEL_LAYOUT_NONE;
  input->channels = 3;
  EXPECT_FALSE(CanBeCreated(input));

  // UNSUPPORTED is considered invalid.
  input->channel_layout = CHANNEL_LAYOUT_UNSUPPORTED;
  input->channels = 0;
  EXPECT_FALSE(CanBeCreated(input));

  input->channel_layout = CHANNEL_LAYOUT_UNSUPPORTED;
  input->channels = 1;
  EXPECT_FALSE(CanBeCreated(input));

  input->channel_layout = CHANNEL_LAYOUT_UNSUPPORTED;
  input->channels = 6;
  EXPECT_FALSE(CanBeCreated(input));

  input->channel_layout = CHANNEL_LAYOUT_UNSUPPORTED;
  input->channels = 9;
  EXPECT_FALSE(CanBeCreated(input));
}

}  // namespace media
