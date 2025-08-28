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
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(AudioDecoderConfigStructTraitsTest, Normal) {
  const std::vector<uint8_t> kExtraData =
      base::ToVector(base::as_byte_span("input extra data"));

  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND,
                   48000, kExtraData, EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest, EmptyExtraData) {
  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND,
                   48000, EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest, Encrypted) {
  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND,
                   48000, EmptyExtraData(), EncryptionScheme::kCenc,
                   base::TimeDelta(), 0);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest, WithProfile) {
  AudioDecoderConfig input;
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND,
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
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND,
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
  input.Initialize(AudioCodec::kAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND,
                   48000, EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  input.set_target_output_channel_layout(CHANNEL_LAYOUT_5_1);
  input.set_target_output_sample_format(kSampleFormatDts);
  std::vector<uint8_t> data = mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
  EXPECT_EQ(output.target_output_channel_layout(), CHANNEL_LAYOUT_5_1);
  EXPECT_EQ(output.target_output_sample_format(), kSampleFormatDts);
}

}  // namespace media
