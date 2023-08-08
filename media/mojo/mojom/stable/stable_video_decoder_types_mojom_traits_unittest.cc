// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/stable_video_decoder_types_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_status.h"
#elif BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_status.h"
#endif

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

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidErrorStatusData) {
  const std::string message("Could not decode");
  internal::StatusData status_data(
      DecoderStatusTraits::Group(),
      static_cast<StatusCodeType>(DecoderStatus::Codes::kPlatformDecodeFailure),
      message,
      /*root_cause=*/0u);

  status_data.AddLocation(base::Location::Current());

  // TODO(b/217970098): allow building the VaapiStatus and V4L2Status without
  // USE_VAAPI/USE_V4L2_CODEC so these guards can be removed.
  const std::string cause_message("Because of this");
#if BUILDFLAG(USE_VAAPI)
  status_data.cause = std::make_unique<internal::StatusData>(
      VaapiStatusTraits::Group(),
      static_cast<StatusCodeType>(VaapiStatus::Codes::kNoImage), cause_message,
      /*root_cause=*/0u);
#elif BUILDFLAG(USE_V4L2_CODEC)
  status_data.cause = std::make_unique<internal::StatusData>(
      V4L2StatusTraits::Group(),
      static_cast<StatusCodeType>(V4L2Status::Codes::kNoProfile), cause_message,
      /*root_cause=*/0u);
#endif

  std::vector<uint8_t> serialized_status_data =
      stable::mojom::StatusData::Serialize(&status_data);

  internal::StatusData deserialized_status_data;
  ASSERT_TRUE(stable::mojom::StatusData::Deserialize(
      serialized_status_data, &deserialized_status_data));
  EXPECT_EQ(deserialized_status_data.group, DecoderStatusTraits::Group());
  // Any status code other than DecoderStatus::Codes::kOk and
  // DecoderStatus::Codes::kAborted should get serialized as
  // stable::mojom::StatusCode::kError which should then get deserialized as
  // DecoderStatus::Codes::kFailed.
  EXPECT_EQ(deserialized_status_data.code,
            static_cast<StatusCodeType>(DecoderStatus::Codes::kFailed));
  EXPECT_EQ(deserialized_status_data.message, message);
  EXPECT_EQ(deserialized_status_data.frames.size(), 1u);

#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
  ASSERT_TRUE(deserialized_status_data.cause);
  // All cause status codes should get serialized as
  // stable::mojom::StatusCode::kError which should then get deserialized as
  // DecoderStatus::Codes::kFailed.
  EXPECT_EQ(deserialized_status_data.cause->group,
            DecoderStatusTraits::Group());
  EXPECT_EQ(deserialized_status_data.cause->code,
            static_cast<StatusCodeType>(DecoderStatus::Codes::kFailed));
  EXPECT_EQ(deserialized_status_data.cause->message, cause_message);
#endif
}

TEST(StableVideoDecoderTypesMojomTraitsTest, StatusDataWithOkCode) {
  stable::mojom::StatusDataPtr mojom_status_data =
      stable::mojom::StatusData::New();
  mojom_status_data->group = std::string(DecoderStatusTraits::Group());
  mojom_status_data->code = stable::mojom::StatusCode::kOk_DEPRECATED;

  std::vector<uint8_t> serialized_status_data =
      stable::mojom::StatusData::Serialize(&mojom_status_data);

  internal::StatusData deserialized_status_data;
  ASSERT_FALSE(stable::mojom::StatusData::Deserialize(
      serialized_status_data, &deserialized_status_data));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, StatusDataWithBadFrame) {
  stable::mojom::StatusDataPtr mojom_status_data =
      stable::mojom::StatusData::New();
  mojom_status_data->group = std::string(DecoderStatusTraits::Group());
  mojom_status_data->code = stable::mojom::StatusCode::kError;

  base::Value::Dict badly_serialized_frame;
  badly_serialized_frame.Set("a", "b");
  badly_serialized_frame.Set("c", 1);
  mojom_status_data->frames.emplace_back(std::move(badly_serialized_frame));

  std::vector<uint8_t> serialized_status_data =
      stable::mojom::StatusData::Serialize(&mojom_status_data);

  internal::StatusData deserialized_status_data;
  ASSERT_FALSE(stable::mojom::StatusData::Deserialize(
      serialized_status_data, &deserialized_status_data));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, StatusDataWithAbortedCause) {
  stable::mojom::StatusDataPtr mojom_status_data =
      stable::mojom::StatusData::New();
  mojom_status_data->group = std::string(DecoderStatusTraits::Group());
  mojom_status_data->code = stable::mojom::StatusCode::kError;
  mojom_status_data->cause = absl::make_optional<internal::StatusData>(
      DecoderStatusTraits::Group(),
      static_cast<StatusCodeType>(DecoderStatus::Codes::kAborted),
      /*message=*/"",
      /*root_cause=*/0u);

  std::vector<uint8_t> serialized_status_data =
      stable::mojom::StatusData::Serialize(&mojom_status_data);

  internal::StatusData deserialized_status_data;
  ASSERT_FALSE(stable::mojom::StatusData::Deserialize(
      serialized_status_data, &deserialized_status_data));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidOkStatus) {
  DecoderStatus status(OkStatus());

  std::vector<uint8_t> serialized_status =
      stable::mojom::Status::Serialize(&status);

  DecoderStatus deserialized_status;
  ASSERT_TRUE(stable::mojom::Status::Deserialize(serialized_status,
                                                 &deserialized_status));
  EXPECT_TRUE(deserialized_status.is_ok());
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidCENCDecryptConfig) {
  stable::mojom::DecryptConfigPtr mojom_decrypt_config =
      stable::mojom::DecryptConfig::New();
  mojom_decrypt_config->encryption_scheme = EncryptionScheme::kCenc;
  mojom_decrypt_config->key_id = "ABC";
  mojom_decrypt_config->iv = "0123456789ABCDEF";
  mojom_decrypt_config->subsamples = {
      SubsampleEntry(/*clear_bytes=*/4u, /*cypher_bytes=*/10u),
      SubsampleEntry(/*clear_bytes=*/90u, /*cypher_bytes=*/2u)};
  mojom_decrypt_config->encryption_pattern = absl::nullopt;

  std::vector<uint8_t> serialized_decrypt_config =
      stable::mojom::DecryptConfig::Serialize(&mojom_decrypt_config);

  std::unique_ptr<DecryptConfig> deserialized_decrypt_config;
  ASSERT_TRUE(stable::mojom::DecryptConfig::Deserialize(
      serialized_decrypt_config, &deserialized_decrypt_config));
  ASSERT_TRUE(deserialized_decrypt_config);

  EXPECT_EQ(deserialized_decrypt_config->encryption_scheme(),
            mojom_decrypt_config->encryption_scheme);
  EXPECT_EQ(deserialized_decrypt_config->key_id(),
            mojom_decrypt_config->key_id);
  EXPECT_EQ(deserialized_decrypt_config->subsamples(),
            mojom_decrypt_config->subsamples);
  EXPECT_EQ(deserialized_decrypt_config->encryption_pattern(),
            mojom_decrypt_config->encryption_pattern);
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidCBCSDecryptConfig) {
  stable::mojom::DecryptConfigPtr mojom_decrypt_config =
      stable::mojom::DecryptConfig::New();
  mojom_decrypt_config->encryption_scheme = EncryptionScheme::kCbcs;
  mojom_decrypt_config->key_id = "ABC";
  mojom_decrypt_config->iv = "0123456789ABCDEF";
  mojom_decrypt_config->subsamples = {
      SubsampleEntry(/*clear_bytes=*/4u, /*cypher_bytes=*/10u),
      SubsampleEntry(/*clear_bytes=*/90u, /*cypher_bytes=*/2u)};
  mojom_decrypt_config->encryption_pattern =
      EncryptionPattern(/*crypt_byte_block=*/2u, /*skip_byte_block=*/5u);

  std::vector<uint8_t> serialized_decrypt_config =
      stable::mojom::DecryptConfig::Serialize(&mojom_decrypt_config);

  std::unique_ptr<DecryptConfig> deserialized_decrypt_config;
  ASSERT_TRUE(stable::mojom::DecryptConfig::Deserialize(
      serialized_decrypt_config, &deserialized_decrypt_config));
  ASSERT_TRUE(deserialized_decrypt_config);

  EXPECT_EQ(deserialized_decrypt_config->encryption_scheme(),
            mojom_decrypt_config->encryption_scheme);
  EXPECT_EQ(deserialized_decrypt_config->key_id(),
            mojom_decrypt_config->key_id);
  EXPECT_EQ(deserialized_decrypt_config->subsamples(),
            mojom_decrypt_config->subsamples);
  EXPECT_EQ(deserialized_decrypt_config->encryption_pattern(),
            mojom_decrypt_config->encryption_pattern);
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     DecryptConfigWithUnencryptedScheme) {
  stable::mojom::DecryptConfigPtr mojom_decrypt_config =
      stable::mojom::DecryptConfig::New();
  mojom_decrypt_config->encryption_scheme = EncryptionScheme::kUnencrypted;
  mojom_decrypt_config->key_id = "ABC";
  mojom_decrypt_config->iv = "0123456789ABCDEF";
  mojom_decrypt_config->subsamples = {
      SubsampleEntry(/*clear_bytes=*/4u, /*cypher_bytes=*/10u),
      SubsampleEntry(/*clear_bytes=*/90u, /*cypher_bytes=*/2u)};
  mojom_decrypt_config->encryption_pattern =
      EncryptionPattern(/*crypt_byte_block=*/2u, /*skip_byte_block=*/5u);

  std::vector<uint8_t> serialized_decrypt_config =
      stable::mojom::DecryptConfig::Serialize(&mojom_decrypt_config);

  std::unique_ptr<DecryptConfig> deserialized_decrypt_config;
  ASSERT_FALSE(stable::mojom::DecryptConfig::Deserialize(
      serialized_decrypt_config, &deserialized_decrypt_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, DecryptConfigWithEmptyKeyID) {
  stable::mojom::DecryptConfigPtr mojom_decrypt_config =
      stable::mojom::DecryptConfig::New();
  mojom_decrypt_config->encryption_scheme = EncryptionScheme::kCbcs;
  mojom_decrypt_config->key_id = "";
  mojom_decrypt_config->iv = "0123456789ABCDEF";
  mojom_decrypt_config->subsamples = {
      SubsampleEntry(/*clear_bytes=*/4u, /*cypher_bytes=*/10u),
      SubsampleEntry(/*clear_bytes=*/90u, /*cypher_bytes=*/2u)};
  mojom_decrypt_config->encryption_pattern =
      EncryptionPattern(/*crypt_byte_block=*/2u, /*skip_byte_block=*/5u);

  std::vector<uint8_t> serialized_decrypt_config =
      stable::mojom::DecryptConfig::Serialize(&mojom_decrypt_config);

  std::unique_ptr<DecryptConfig> deserialized_decrypt_config;
  ASSERT_FALSE(stable::mojom::DecryptConfig::Deserialize(
      serialized_decrypt_config, &deserialized_decrypt_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     DecryptConfigWithIVOfIncorrectSize) {
  stable::mojom::DecryptConfigPtr mojom_decrypt_config =
      stable::mojom::DecryptConfig::New();
  mojom_decrypt_config->encryption_scheme = EncryptionScheme::kCbcs;
  mojom_decrypt_config->key_id = "ABC";
  mojom_decrypt_config->iv = "0123456789ABCDEFG";
  mojom_decrypt_config->subsamples = {
      SubsampleEntry(/*clear_bytes=*/4u, /*cypher_bytes=*/10u),
      SubsampleEntry(/*clear_bytes=*/90u, /*cypher_bytes=*/2u)};
  mojom_decrypt_config->encryption_pattern =
      EncryptionPattern(/*crypt_byte_block=*/2u, /*skip_byte_block=*/5u);

  std::vector<uint8_t> serialized_decrypt_config =
      stable::mojom::DecryptConfig::Serialize(&mojom_decrypt_config);

  std::unique_ptr<DecryptConfig> deserialized_decrypt_config;
  ASSERT_FALSE(stable::mojom::DecryptConfig::Deserialize(
      serialized_decrypt_config, &deserialized_decrypt_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, CENCDecryptConfigWithPattern) {
  stable::mojom::DecryptConfigPtr mojom_decrypt_config =
      stable::mojom::DecryptConfig::New();
  mojom_decrypt_config->encryption_scheme = EncryptionScheme::kCenc;
  mojom_decrypt_config->key_id = "ABC";
  mojom_decrypt_config->iv = "0123456789ABCDEF";
  mojom_decrypt_config->subsamples = {
      SubsampleEntry(/*clear_bytes=*/4u, /*cypher_bytes=*/10u),
      SubsampleEntry(/*clear_bytes=*/90u, /*cypher_bytes=*/2u)};
  mojom_decrypt_config->encryption_pattern =
      EncryptionPattern(/*crypt_byte_block=*/2u, /*skip_byte_block=*/5u);

  std::vector<uint8_t> serialized_decrypt_config =
      stable::mojom::DecryptConfig::Serialize(&mojom_decrypt_config);

  std::unique_ptr<DecryptConfig> deserialized_decrypt_config;
  ASSERT_FALSE(stable::mojom::DecryptConfig::Deserialize(
      serialized_decrypt_config, &deserialized_decrypt_config));
}
}  // namespace media
