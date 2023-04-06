// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/stable_video_decoder_types_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidNonEOSDecoderBuffer) {
  stable::mojom::DecoderBufferPtr mojom_decoder_buffer =
      stable::mojom::DecoderBuffer::New();
  mojom_decoder_buffer->timestamp = base::Milliseconds(32);
  mojom_decoder_buffer->duration = base::Milliseconds(16);
  mojom_decoder_buffer->is_end_of_stream = false;
  mojom_decoder_buffer->data_size = 100;
  mojom_decoder_buffer->is_key_frame = true;

  std::vector<uint8_t> serialized_decoder_buffer =
      stable::mojom::DecoderBuffer::Serialize(&mojom_decoder_buffer);

  scoped_refptr<DecoderBuffer> deserialized_decoder_buffer;
  ASSERT_TRUE(stable::mojom::DecoderBuffer::Deserialize(
      serialized_decoder_buffer, &deserialized_decoder_buffer));
  ASSERT_TRUE(deserialized_decoder_buffer);

  ASSERT_FALSE(deserialized_decoder_buffer->end_of_stream());
  EXPECT_EQ(deserialized_decoder_buffer->timestamp(),
            mojom_decoder_buffer->timestamp);
  EXPECT_EQ(deserialized_decoder_buffer->duration(),
            mojom_decoder_buffer->duration);
  EXPECT_EQ(deserialized_decoder_buffer->data_size(),
            base::strict_cast<size_t>(mojom_decoder_buffer->data_size));
  EXPECT_EQ(deserialized_decoder_buffer->is_key_frame(),
            mojom_decoder_buffer->is_key_frame);
}

TEST(StableVideoDecoderTypesMojomTraitsTest, InfiniteDecoderBufferDuration) {
  stable::mojom::DecoderBufferPtr mojom_decoder_buffer =
      stable::mojom::DecoderBuffer::New();
  mojom_decoder_buffer->timestamp = base::Milliseconds(32);
  mojom_decoder_buffer->duration = media::kInfiniteDuration;
  mojom_decoder_buffer->is_end_of_stream = false;
  mojom_decoder_buffer->data_size = 100;
  mojom_decoder_buffer->is_key_frame = true;

  std::vector<uint8_t> serialized_decoder_buffer =
      stable::mojom::DecoderBuffer::Serialize(&mojom_decoder_buffer);

  scoped_refptr<DecoderBuffer> deserialized_decoder_buffer;
  ASSERT_FALSE(stable::mojom::DecoderBuffer::Deserialize(
      serialized_decoder_buffer, &deserialized_decoder_buffer));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, NegativeDecoderBufferDuration) {
  stable::mojom::DecoderBufferPtr mojom_decoder_buffer =
      stable::mojom::DecoderBuffer::New();
  mojom_decoder_buffer->timestamp = base::Milliseconds(32);
  mojom_decoder_buffer->duration = base::TimeDelta() - base::Milliseconds(16);
  mojom_decoder_buffer->is_end_of_stream = false;
  mojom_decoder_buffer->data_size = 100;
  mojom_decoder_buffer->is_key_frame = true;

  std::vector<uint8_t> serialized_decoder_buffer =
      stable::mojom::DecoderBuffer::Serialize(&mojom_decoder_buffer);

  scoped_refptr<DecoderBuffer> deserialized_decoder_buffer;
  ASSERT_FALSE(stable::mojom::DecoderBuffer::Deserialize(
      serialized_decoder_buffer, &deserialized_decoder_buffer));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidSupportedVideoDecoderConfig) {
  stable::mojom::SupportedVideoDecoderConfigPtr
      mojom_supported_video_decoder_config =
          stable::mojom::SupportedVideoDecoderConfig::New();
  mojom_supported_video_decoder_config->profile_min =
      VideoCodecProfile::AV1PROFILE_MIN;
  mojom_supported_video_decoder_config->profile_max =
      VideoCodecProfile::AV1PROFILE_MAX;
  mojom_supported_video_decoder_config->coded_size_min = gfx::Size(16, 32);
  mojom_supported_video_decoder_config->coded_size_max = gfx::Size(1280, 720);
  mojom_supported_video_decoder_config->allow_encrypted = true;
  mojom_supported_video_decoder_config->require_encrypted = false;

  std::vector<uint8_t> serialized_supported_video_decoder_config =
      stable::mojom::SupportedVideoDecoderConfig::Serialize(
          &mojom_supported_video_decoder_config);

  SupportedVideoDecoderConfig deserialized_supported_video_decoder_config;
  ASSERT_TRUE(stable::mojom::SupportedVideoDecoderConfig::Deserialize(
      serialized_supported_video_decoder_config,
      &deserialized_supported_video_decoder_config));

  EXPECT_EQ(deserialized_supported_video_decoder_config.profile_min,
            mojom_supported_video_decoder_config->profile_min);
  EXPECT_EQ(deserialized_supported_video_decoder_config.profile_max,
            mojom_supported_video_decoder_config->profile_max);
  EXPECT_EQ(deserialized_supported_video_decoder_config.coded_size_min,
            mojom_supported_video_decoder_config->coded_size_min);
  EXPECT_EQ(deserialized_supported_video_decoder_config.coded_size_max,
            mojom_supported_video_decoder_config->coded_size_max);
  EXPECT_EQ(deserialized_supported_video_decoder_config.allow_encrypted,
            mojom_supported_video_decoder_config->allow_encrypted);
  EXPECT_EQ(deserialized_supported_video_decoder_config.require_encrypted,
            mojom_supported_video_decoder_config->require_encrypted);
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     SupportedVideoConfigWithUnknownMinProfile) {
  stable::mojom::SupportedVideoDecoderConfigPtr
      mojom_supported_video_decoder_config =
          stable::mojom::SupportedVideoDecoderConfig::New();
  mojom_supported_video_decoder_config->profile_min =
      VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
  mojom_supported_video_decoder_config->profile_max =
      VideoCodecProfile::AV1PROFILE_MAX;
  mojom_supported_video_decoder_config->coded_size_min = gfx::Size(16, 32);
  mojom_supported_video_decoder_config->coded_size_max = gfx::Size(1280, 720);
  mojom_supported_video_decoder_config->allow_encrypted = true;
  mojom_supported_video_decoder_config->require_encrypted = false;

  std::vector<uint8_t> serialized_supported_video_decoder_config =
      stable::mojom::SupportedVideoDecoderConfig::Serialize(
          &mojom_supported_video_decoder_config);

  SupportedVideoDecoderConfig deserialized_supported_video_decoder_config;
  ASSERT_FALSE(stable::mojom::SupportedVideoDecoderConfig::Deserialize(
      serialized_supported_video_decoder_config,
      &deserialized_supported_video_decoder_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     SupportedVideoConfigWithUnknownMaxProfile) {
  stable::mojom::SupportedVideoDecoderConfigPtr
      mojom_supported_video_decoder_config =
          stable::mojom::SupportedVideoDecoderConfig::New();
  mojom_supported_video_decoder_config->profile_min =
      VideoCodecProfile::AV1PROFILE_MIN;
  mojom_supported_video_decoder_config->profile_max =
      VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
  mojom_supported_video_decoder_config->coded_size_min = gfx::Size(16, 32);
  mojom_supported_video_decoder_config->coded_size_max = gfx::Size(1280, 720);
  mojom_supported_video_decoder_config->allow_encrypted = true;
  mojom_supported_video_decoder_config->require_encrypted = false;

  std::vector<uint8_t> serialized_supported_video_decoder_config =
      stable::mojom::SupportedVideoDecoderConfig::Serialize(
          &mojom_supported_video_decoder_config);

  SupportedVideoDecoderConfig deserialized_supported_video_decoder_config;
  ASSERT_FALSE(stable::mojom::SupportedVideoDecoderConfig::Deserialize(
      serialized_supported_video_decoder_config,
      &deserialized_supported_video_decoder_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     SupportedVideoConfigWithMaxProfileLessThanMinProfile) {
  stable::mojom::SupportedVideoDecoderConfigPtr
      mojom_supported_video_decoder_config =
          stable::mojom::SupportedVideoDecoderConfig::New();
  mojom_supported_video_decoder_config->profile_min =
      VideoCodecProfile::AV1PROFILE_MAX;
  mojom_supported_video_decoder_config->profile_max =
      VideoCodecProfile::AV1PROFILE_MIN;
  mojom_supported_video_decoder_config->coded_size_min = gfx::Size(16, 32);
  mojom_supported_video_decoder_config->coded_size_max = gfx::Size(1280, 720);
  mojom_supported_video_decoder_config->allow_encrypted = true;
  mojom_supported_video_decoder_config->require_encrypted = false;

  std::vector<uint8_t> serialized_supported_video_decoder_config =
      stable::mojom::SupportedVideoDecoderConfig::Serialize(
          &mojom_supported_video_decoder_config);

  SupportedVideoDecoderConfig deserialized_supported_video_decoder_config;
  ASSERT_FALSE(stable::mojom::SupportedVideoDecoderConfig::Deserialize(
      serialized_supported_video_decoder_config,
      &deserialized_supported_video_decoder_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     SupportedVideoConfigWithMaxCodedSizeLessThanMinCodedSize) {
  stable::mojom::SupportedVideoDecoderConfigPtr
      mojom_supported_video_decoder_config =
          stable::mojom::SupportedVideoDecoderConfig::New();
  mojom_supported_video_decoder_config->profile_min =
      VideoCodecProfile::AV1PROFILE_MIN;
  mojom_supported_video_decoder_config->profile_max =
      VideoCodecProfile::AV1PROFILE_MAX;
  mojom_supported_video_decoder_config->coded_size_min = gfx::Size(1280, 720);
  mojom_supported_video_decoder_config->coded_size_max = gfx::Size(16, 32);
  mojom_supported_video_decoder_config->allow_encrypted = true;
  mojom_supported_video_decoder_config->require_encrypted = false;

  std::vector<uint8_t> serialized_supported_video_decoder_config =
      stable::mojom::SupportedVideoDecoderConfig::Serialize(
          &mojom_supported_video_decoder_config);

  SupportedVideoDecoderConfig deserialized_supported_video_decoder_config;
  ASSERT_FALSE(stable::mojom::SupportedVideoDecoderConfig::Deserialize(
      serialized_supported_video_decoder_config,
      &deserialized_supported_video_decoder_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     SupportedVideoDecoderConfigWithInconsistentEncryptionFields) {
  stable::mojom::SupportedVideoDecoderConfigPtr
      mojom_supported_video_decoder_config =
          stable::mojom::SupportedVideoDecoderConfig::New();
  mojom_supported_video_decoder_config->profile_min =
      VideoCodecProfile::AV1PROFILE_MIN;
  mojom_supported_video_decoder_config->profile_max =
      VideoCodecProfile::AV1PROFILE_MAX;
  mojom_supported_video_decoder_config->coded_size_min = gfx::Size(16, 32);
  mojom_supported_video_decoder_config->coded_size_max = gfx::Size(1280, 720);
  mojom_supported_video_decoder_config->allow_encrypted = false;
  mojom_supported_video_decoder_config->require_encrypted = true;

  std::vector<uint8_t> serialized_supported_video_decoder_config =
      stable::mojom::SupportedVideoDecoderConfig::Serialize(
          &mojom_supported_video_decoder_config);

  SupportedVideoDecoderConfig deserialized_supported_video_decoder_config;
  ASSERT_FALSE(stable::mojom::SupportedVideoDecoderConfig::Deserialize(
      serialized_supported_video_decoder_config,
      &deserialized_supported_video_decoder_config));
}

}  // namespace media
