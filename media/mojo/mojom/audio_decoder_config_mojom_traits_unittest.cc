// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/audio_decoder_config_mojom_traits.h"

#include <utility>

#include "base/cxx17_backports.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(AudioDecoderConfigStructTraitsTest, ConvertAudioDecoderConfig_Normal) {
  const uint8_t kExtraData[] = "input extra data";
  const std::vector<uint8_t> kExtraDataVector(
      &kExtraData[0], &kExtraData[0] + base::size(kExtraData));

  AudioDecoderConfig input;
  input.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                   kExtraDataVector, EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  std::vector<uint8_t> data =
      media::mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest,
     ConvertAudioDecoderConfig_EmptyExtraData) {
  AudioDecoderConfig input;
  input.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                   EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  std::vector<uint8_t> data =
      media::mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest, ConvertAudioDecoderConfig_Encrypted) {
  AudioDecoderConfig input;
  input.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                   EmptyExtraData(), EncryptionScheme::kCenc, base::TimeDelta(),
                   0);
  std::vector<uint8_t> data =
      media::mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest,
     ConvertAudioDecoderConfig_WithProfile) {
  AudioDecoderConfig input;
  input.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                   EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  input.set_profile(AudioCodecProfile::kXHE_AAC);
  std::vector<uint8_t> data =
      media::mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(AudioDecoderConfigStructTraitsTest,
     ConvertAudioDecoderConfig_DisableDiscardDecoderDelay) {
  AudioDecoderConfig input;
  input.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                   EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  input.disable_discard_decoder_delay();
  std::vector<uint8_t> data =
      media::mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
  EXPECT_FALSE(output.should_discard_decoder_delay());
}

TEST(AudioDecoderConfigStructTraitsTest,
     ConvertAudioDecoderConfig_TargetOutputChannelLayout) {
  AudioDecoderConfig input;
  input.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                   EmptyExtraData(), EncryptionScheme::kUnencrypted,
                   base::TimeDelta(), 0);
  input.set_target_output_channel_layout(CHANNEL_LAYOUT_5_1);
  std::vector<uint8_t> data =
      media::mojom::AudioDecoderConfig::Serialize(&input);
  AudioDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::AudioDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
  EXPECT_EQ(output.target_output_channel_layout(), CHANNEL_LAYOUT_5_1);
}

}  // namespace media
