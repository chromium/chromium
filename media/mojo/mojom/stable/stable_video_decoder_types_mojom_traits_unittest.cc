// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/mojom/stable/stable_video_decoder_types_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_status.h"
#elif BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_status.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "media/mojo/mojom/stable/mojom_traits_test_util.h"

#include <linux/kcmp.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "base/process/process.h"
#include "base/test/gtest_util.h"  // nogncheck
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

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
  EXPECT_EQ(deserialized_decoder_buffer->size(),
            base::strict_cast<size_t>(mojom_decoder_buffer->data_size));
  EXPECT_EQ(deserialized_decoder_buffer->is_key_frame(),
            mojom_decoder_buffer->is_key_frame);
  EXPECT_FALSE(deserialized_decoder_buffer->has_side_data());
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

TEST(StableVideoDecoderTypesMojomTraitsTest, RawSideDataToValidSpatialLayers) {
  stable::mojom::DecoderBufferPtr mojom_decoder_buffer =
      stable::mojom::DecoderBuffer::New();
  mojom_decoder_buffer->is_end_of_stream = false;
  constexpr uint32_t kValidSpatialLayers[] = {1, 2, 3};
  constexpr size_t kLayersSize = std::size(kValidSpatialLayers);
  mojom_decoder_buffer->raw_side_data.assign(
      reinterpret_cast<const uint8_t*>(kValidSpatialLayers),
      reinterpret_cast<const uint8_t*>(kValidSpatialLayers + kLayersSize));

  std::vector<uint8_t> serialized_decoder_buffer =
      stable::mojom::DecoderBuffer::Serialize(&mojom_decoder_buffer);

  scoped_refptr<DecoderBuffer> deserialized_decoder_buffer;
  ASSERT_TRUE(stable::mojom::DecoderBuffer::Deserialize(
      serialized_decoder_buffer, &deserialized_decoder_buffer));
  ASSERT_TRUE(deserialized_decoder_buffer);

  ASSERT_FALSE(deserialized_decoder_buffer->end_of_stream());
  ASSERT_TRUE(deserialized_decoder_buffer->has_side_data());
  EXPECT_EQ(deserialized_decoder_buffer->side_data()->spatial_layers,
            std::vector<uint32_t>(kValidSpatialLayers,
                                  kValidSpatialLayers + kLayersSize));
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     RawSideDataToInvalidSpatialLayers) {
  stable::mojom::DecoderBufferPtr mojom_decoder_buffer =
      stable::mojom::DecoderBuffer::New();
  mojom_decoder_buffer->is_end_of_stream = false;
  // The max number of spatial layers is 3.
  constexpr uint32_t kInvalidSpatialLayers[] = {1, 2, 3, 4};
  constexpr size_t kLayersSize = std::size(kInvalidSpatialLayers);
  mojom_decoder_buffer->raw_side_data.assign(
      reinterpret_cast<const uint8_t*>(kInvalidSpatialLayers),
      reinterpret_cast<const uint8_t*>(kInvalidSpatialLayers + kLayersSize));

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
  mojom_status_data->cause = std::make_optional<internal::StatusData>(
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
  mojom_decrypt_config->encryption_pattern = std::nullopt;

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

TEST(StableVideoDecoderTypesMojomTraitsTest, EmptyVideoFrameMetadata) {
  stable::mojom::VideoFrameMetadataPtr mojom_video_frame_metadata =
      stable::mojom::VideoFrameMetadata::New();

  std::vector<uint8_t> serialized_video_frame_metadata =
      stable::mojom::VideoFrameMetadata::Serialize(&mojom_video_frame_metadata);

  VideoFrameMetadata deserialized_video_frame_metadata;
  ASSERT_TRUE(stable::mojom::VideoFrameMetadata::Deserialize(
      serialized_video_frame_metadata, &deserialized_video_frame_metadata));

  EXPECT_FALSE(deserialized_video_frame_metadata.capture_counter.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.capture_update_rect.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.transformation.has_value());
  EXPECT_TRUE(deserialized_video_frame_metadata.allow_overlay);
  EXPECT_FALSE(deserialized_video_frame_metadata.copy_required);
  EXPECT_FALSE(deserialized_video_frame_metadata.end_of_stream);
  EXPECT_FALSE(deserialized_video_frame_metadata.texture_owner);
  EXPECT_FALSE(deserialized_video_frame_metadata.wants_promotion_hint);
  EXPECT_FALSE(deserialized_video_frame_metadata.protected_video);
  EXPECT_FALSE(deserialized_video_frame_metadata.hw_protected);
  EXPECT_FALSE(deserialized_video_frame_metadata.is_webgpu_compatible);
  EXPECT_TRUE(deserialized_video_frame_metadata.power_efficient);
  EXPECT_TRUE(deserialized_video_frame_metadata.read_lock_fences_enabled);
  EXPECT_FALSE(deserialized_video_frame_metadata.interactive_content);
  EXPECT_FALSE(deserialized_video_frame_metadata.overlay_plane_id.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.device_scale_factor.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.page_scale_factor.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.root_scroll_offset_x.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.root_scroll_offset_y.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.top_controls_visible_height
                   .has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.frame_rate.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.rtp_timestamp.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.receive_time.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.capture_begin_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.capture_end_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.decode_begin_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.decode_end_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.reference_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.processing_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.frame_duration.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.wallclock_frame_duration.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.source_size.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.region_capture_rect.has_value());
  EXPECT_EQ(0u, deserialized_video_frame_metadata.sub_capture_target_version);
  EXPECT_FALSE(deserialized_video_frame_metadata.dcomp_surface);
#if BUILDFLAG(USE_VAAPI)
  EXPECT_FALSE(
      deserialized_video_frame_metadata.hw_va_protected_session_id.has_value());
#endif
  EXPECT_TRUE(deserialized_video_frame_metadata.texture_origin_is_top_left);
  EXPECT_FALSE(deserialized_video_frame_metadata
                   .maximum_composition_delay_in_frames.has_value());
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidVideoFrameMetadata) {
  stable::mojom::VideoFrameMetadataPtr mojom_video_frame_metadata =
      stable::mojom::VideoFrameMetadata::New();

  mojom_video_frame_metadata->protected_video = true;
  mojom_video_frame_metadata->hw_protected = true;

  std::vector<uint8_t> serialized_video_frame_metadata =
      stable::mojom::VideoFrameMetadata::Serialize(&mojom_video_frame_metadata);

  VideoFrameMetadata deserialized_video_frame_metadata;
  ASSERT_TRUE(stable::mojom::VideoFrameMetadata::Deserialize(
      serialized_video_frame_metadata, &deserialized_video_frame_metadata));

  EXPECT_EQ(mojom_video_frame_metadata->protected_video,
            deserialized_video_frame_metadata.protected_video);
  EXPECT_EQ(mojom_video_frame_metadata->hw_protected,
            deserialized_video_frame_metadata.hw_protected);

  EXPECT_FALSE(deserialized_video_frame_metadata.capture_counter.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.capture_update_rect.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.transformation.has_value());
  EXPECT_TRUE(deserialized_video_frame_metadata.allow_overlay);
  EXPECT_FALSE(deserialized_video_frame_metadata.copy_required);
  EXPECT_FALSE(deserialized_video_frame_metadata.end_of_stream);
  EXPECT_FALSE(deserialized_video_frame_metadata.texture_owner);
  EXPECT_FALSE(deserialized_video_frame_metadata.wants_promotion_hint);
  EXPECT_FALSE(deserialized_video_frame_metadata.is_webgpu_compatible);
  EXPECT_TRUE(deserialized_video_frame_metadata.power_efficient);
  EXPECT_TRUE(deserialized_video_frame_metadata.read_lock_fences_enabled);
  EXPECT_FALSE(deserialized_video_frame_metadata.interactive_content);
  EXPECT_FALSE(deserialized_video_frame_metadata.overlay_plane_id.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.device_scale_factor.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.page_scale_factor.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.root_scroll_offset_x.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.root_scroll_offset_y.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.top_controls_visible_height
                   .has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.frame_rate.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.rtp_timestamp.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.receive_time.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.capture_begin_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.capture_end_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.decode_begin_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.decode_end_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.reference_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.processing_time.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.frame_duration.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.wallclock_frame_duration.has_value());
  EXPECT_FALSE(deserialized_video_frame_metadata.source_size.has_value());
  EXPECT_FALSE(
      deserialized_video_frame_metadata.region_capture_rect.has_value());
  EXPECT_EQ(0u, deserialized_video_frame_metadata.sub_capture_target_version);
  EXPECT_FALSE(deserialized_video_frame_metadata.dcomp_surface);
#if BUILDFLAG(USE_VAAPI)
  EXPECT_FALSE(
      deserialized_video_frame_metadata.hw_va_protected_session_id.has_value());
#endif
  EXPECT_TRUE(deserialized_video_frame_metadata.texture_origin_is_top_left);
  EXPECT_FALSE(deserialized_video_frame_metadata
                   .maximum_composition_delay_in_frames.has_value());
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     VideoFrameMetadataWithInconsistentProtectedContentFields) {
  stable::mojom::VideoFrameMetadataPtr mojom_video_frame_metadata =
      stable::mojom::VideoFrameMetadata::New();

  mojom_video_frame_metadata->protected_video = false;
  mojom_video_frame_metadata->hw_protected = true;

  std::vector<uint8_t> serialized_video_frame_metadata =
      stable::mojom::VideoFrameMetadata::Serialize(&mojom_video_frame_metadata);

  VideoFrameMetadata deserialized_video_frame_metadata;
  ASSERT_FALSE(stable::mojom::VideoFrameMetadata::Deserialize(
      serialized_video_frame_metadata, &deserialized_video_frame_metadata));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidVideoDecoderConfig) {
  stable::mojom::VideoDecoderConfigPtr mojom_video_decoder_config =
      stable::mojom::VideoDecoderConfig::New();
  mojom_video_decoder_config->codec = VideoCodec::kVP9;
  mojom_video_decoder_config->profile = VP9PROFILE_PROFILE0;
  mojom_video_decoder_config->level = kNoVideoCodecLevel;
  mojom_video_decoder_config->has_alpha = true;
  mojom_video_decoder_config->coded_size = gfx::Size(320, 240);
  mojom_video_decoder_config->visible_rect = gfx::Rect(4, 4, 310, 230);
  mojom_video_decoder_config->natural_size = gfx::Size(310, 234);
  mojom_video_decoder_config->extra_data.push_back(2u);
  mojom_video_decoder_config->extra_data.push_back(1u);
  mojom_video_decoder_config->extra_data.push_back(3u);
  mojom_video_decoder_config->extra_data.push_back(5u);
  mojom_video_decoder_config->extra_data.push_back(9u);
  mojom_video_decoder_config->extra_data.push_back(4u);
  mojom_video_decoder_config->encryption_scheme =
      EncryptionScheme::kUnencrypted;
  mojom_video_decoder_config->color_space_info =
      gfx::ColorSpace::CreateREC601();
  mojom_video_decoder_config->hdr_metadata =
      gfx::HDRMetadata(gfx::HdrMetadataSmpteSt2086(
                           {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f},
                           /*luminance_max=*/1000,
                           /*luminance_min=*/0),
                       gfx::HdrMetadataCta861_3(123, 456));

  std::vector<uint8_t> serialized_video_decoder_config =
      stable::mojom::VideoDecoderConfig::Serialize(&mojom_video_decoder_config);

  VideoDecoderConfig deserialized_video_decoder_config;
  ASSERT_TRUE(stable::mojom::VideoDecoderConfig::Deserialize(
      serialized_video_decoder_config, &deserialized_video_decoder_config));

  EXPECT_EQ(deserialized_video_decoder_config.codec(),
            mojom_video_decoder_config->codec);
  EXPECT_EQ(deserialized_video_decoder_config.profile(),
            mojom_video_decoder_config->profile);
  EXPECT_EQ(deserialized_video_decoder_config.level(),
            mojom_video_decoder_config->level);
  EXPECT_EQ(deserialized_video_decoder_config.alpha_mode() ==
                VideoDecoderConfig::AlphaMode::kHasAlpha,
            mojom_video_decoder_config->has_alpha);
  EXPECT_EQ(deserialized_video_decoder_config.coded_size(),
            mojom_video_decoder_config->coded_size);
  EXPECT_EQ(deserialized_video_decoder_config.visible_rect(),
            mojom_video_decoder_config->visible_rect);
  EXPECT_EQ(deserialized_video_decoder_config.natural_size(),
            mojom_video_decoder_config->natural_size);
  EXPECT_EQ(deserialized_video_decoder_config.extra_data(),
            mojom_video_decoder_config->extra_data);
  EXPECT_EQ(deserialized_video_decoder_config.encryption_scheme(),
            mojom_video_decoder_config->encryption_scheme);
  EXPECT_EQ(
      deserialized_video_decoder_config.color_space_info().ToGfxColorSpace(),
      mojom_video_decoder_config->color_space_info);
  EXPECT_FALSE(deserialized_video_decoder_config.is_encrypted());
  EXPECT_EQ(deserialized_video_decoder_config.video_transformation(),
            kNoTransformation);
  EXPECT_EQ(deserialized_video_decoder_config.aspect_ratio().GetNaturalSize(
                mojom_video_decoder_config->visible_rect),
            mojom_video_decoder_config->natural_size);
  EXPECT_EQ(deserialized_video_decoder_config.hdr_metadata(),
            mojom_video_decoder_config->hdr_metadata);
  EXPECT_TRUE(deserialized_video_decoder_config.IsValidConfig());
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     VideoDecoderConfigWithUnknownCodec) {
  stable::mojom::VideoDecoderConfigPtr mojom_video_decoder_config =
      stable::mojom::VideoDecoderConfig::New();
  mojom_video_decoder_config->codec = VideoCodec::kUnknown;
  mojom_video_decoder_config->profile = VP9PROFILE_PROFILE0;
  mojom_video_decoder_config->level = kNoVideoCodecLevel;
  mojom_video_decoder_config->has_alpha = true;
  mojom_video_decoder_config->coded_size = gfx::Size(320, 240);
  mojom_video_decoder_config->visible_rect = gfx::Rect(4, 4, 310, 230);
  mojom_video_decoder_config->natural_size = gfx::Size(310, 234);
  mojom_video_decoder_config->extra_data.push_back(2u);
  mojom_video_decoder_config->extra_data.push_back(1u);
  mojom_video_decoder_config->extra_data.push_back(3u);
  mojom_video_decoder_config->extra_data.push_back(5u);
  mojom_video_decoder_config->extra_data.push_back(9u);
  mojom_video_decoder_config->extra_data.push_back(4u);
  mojom_video_decoder_config->encryption_scheme =
      EncryptionScheme::kUnencrypted;
  mojom_video_decoder_config->color_space_info =
      gfx::ColorSpace::CreateREC601();
  mojom_video_decoder_config->hdr_metadata =
      gfx::HDRMetadata(gfx::HdrMetadataSmpteSt2086(
                           {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f},
                           /*luminance_max=*/1000,
                           /*luminance_min=*/0),
                       gfx::HdrMetadataCta861_3(123, 456));

  std::vector<uint8_t> serialized_video_decoder_config =
      stable::mojom::VideoDecoderConfig::Serialize(&mojom_video_decoder_config);

  VideoDecoderConfig deserialized_video_decoder_config;
  ASSERT_FALSE(stable::mojom::VideoDecoderConfig::Deserialize(
      serialized_video_decoder_config, &deserialized_video_decoder_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     VideoDecoderConfigWithTooLargeVisibleRect) {
  stable::mojom::VideoDecoderConfigPtr mojom_video_decoder_config =
      stable::mojom::VideoDecoderConfig::New();
  mojom_video_decoder_config->codec = VideoCodec::kVP9;
  mojom_video_decoder_config->profile = VP9PROFILE_PROFILE0;
  mojom_video_decoder_config->level = kNoVideoCodecLevel;
  mojom_video_decoder_config->has_alpha = true;
  mojom_video_decoder_config->coded_size = gfx::Size(320, 240);
  mojom_video_decoder_config->visible_rect = gfx::Rect(4, 4, 320, 240);
  mojom_video_decoder_config->natural_size = gfx::Size(310, 234);
  mojom_video_decoder_config->extra_data.push_back(2u);
  mojom_video_decoder_config->extra_data.push_back(1u);
  mojom_video_decoder_config->extra_data.push_back(3u);
  mojom_video_decoder_config->extra_data.push_back(5u);
  mojom_video_decoder_config->extra_data.push_back(9u);
  mojom_video_decoder_config->extra_data.push_back(4u);
  mojom_video_decoder_config->encryption_scheme =
      EncryptionScheme::kUnencrypted;
  mojom_video_decoder_config->color_space_info =
      gfx::ColorSpace::CreateREC601();
  mojom_video_decoder_config->hdr_metadata =
      gfx::HDRMetadata(gfx::HdrMetadataSmpteSt2086(
                           {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f},
                           /*luminance_max=*/1000,
                           /*luminance_min=*/0),
                       gfx::HdrMetadataCta861_3(123, 456));

  std::vector<uint8_t> serialized_video_decoder_config =
      stable::mojom::VideoDecoderConfig::Serialize(&mojom_video_decoder_config);

  VideoDecoderConfig deserialized_video_decoder_config;
  ASSERT_FALSE(stable::mojom::VideoDecoderConfig::Deserialize(
      serialized_video_decoder_config, &deserialized_video_decoder_config));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidSubsampleEntry) {
  auto subsample_entry = SubsampleEntry(22, 42);

  std::vector<uint8_t> serialized_subsample_entry =
      stable::mojom::SubsampleEntry::Serialize(&subsample_entry);

  SubsampleEntry deserialized_subsample_entry;
  ASSERT_TRUE(stable::mojom::SubsampleEntry::Deserialize(
      serialized_subsample_entry, &deserialized_subsample_entry));

  EXPECT_EQ(subsample_entry.clear_bytes,
            deserialized_subsample_entry.clear_bytes);
  EXPECT_EQ(subsample_entry.cypher_bytes,
            deserialized_subsample_entry.cypher_bytes);
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidMediaLogRecord) {
  MediaLogRecord media_log_record;
  media_log_record.id = 2;
  media_log_record.type = MediaLogRecord::Type::kMediaStatus;
  media_log_record.params.Set("Test", "Value");
  media_log_record.time = base::TimeTicks::Now();

  std::vector<uint8_t> serialized_media_log_record =
      stable::mojom::MediaLogRecord::Serialize(&media_log_record);

  MediaLogRecord deserialized_media_log_record;
  ASSERT_TRUE(stable::mojom::MediaLogRecord::Deserialize(
      serialized_media_log_record, &deserialized_media_log_record));

  EXPECT_EQ(media_log_record.id, deserialized_media_log_record.id);
  EXPECT_EQ(media_log_record.type, deserialized_media_log_record.type);
  EXPECT_EQ(media_log_record.params, deserialized_media_log_record.params);
  EXPECT_EQ(media_log_record.time, deserialized_media_log_record.time);
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidDecoderBufferSideData) {
  auto decoder_buffer_side_data = DecoderBufferSideData();
  decoder_buffer_side_data.spatial_layers = {1, 2, 3};
  decoder_buffer_side_data.alpha_data = {0, 1, 2};
  decoder_buffer_side_data.secure_handle = 14;

  std::vector<uint8_t> serialized_decoder_buffer_side_data =
      stable::mojom::DecoderBufferSideData::Serialize(
          &decoder_buffer_side_data);

  DecoderBufferSideData deserialized_decoder_buffer_side_data;
  ASSERT_TRUE(stable::mojom::DecoderBufferSideData::Deserialize(
      serialized_decoder_buffer_side_data,
      &deserialized_decoder_buffer_side_data));

  EXPECT_EQ(decoder_buffer_side_data.spatial_layers,
            deserialized_decoder_buffer_side_data.spatial_layers);
  EXPECT_EQ(decoder_buffer_side_data.alpha_data,
            deserialized_decoder_buffer_side_data.alpha_data);
  EXPECT_EQ(decoder_buffer_side_data.secure_handle,
            deserialized_decoder_buffer_side_data.secure_handle);
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     DecoderBufferSideDataWithTooManySpatialLayers) {
  auto decoder_buffer_side_data = DecoderBufferSideData();
  decoder_buffer_side_data.spatial_layers = {1, 2, 3, 4};
  decoder_buffer_side_data.alpha_data = {0, 1, 2};
  decoder_buffer_side_data.secure_handle = 14;

  std::vector<uint8_t> serialized_decoder_buffer_side_data =
      stable::mojom::DecoderBufferSideData::Serialize(
          &decoder_buffer_side_data);

  DecoderBufferSideData deserialized_decoder_buffer_side_data;
  ASSERT_FALSE(stable::mojom::DecoderBufferSideData::Deserialize(
      serialized_decoder_buffer_side_data,
      &deserialized_decoder_buffer_side_data));
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidColorVolumeMetadata) {
  auto color_volume_metadata = gfx::HdrMetadataSmpteSt2086(
      {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f},
      /*luminance_max=*/1000,
      /*luminance_min=*/0);

  std::vector<uint8_t> serialized_color_volume_metadata =
      stable::mojom::ColorVolumeMetadata::Serialize(&color_volume_metadata);

  gfx::HdrMetadataSmpteSt2086 deserialized_color_volume_metadata;
  ASSERT_TRUE(stable::mojom::ColorVolumeMetadata::Deserialize(
      serialized_color_volume_metadata, &deserialized_color_volume_metadata));

  EXPECT_EQ(color_volume_metadata.primaries,
            deserialized_color_volume_metadata.primaries);
  EXPECT_EQ(color_volume_metadata.luminance_min,
            deserialized_color_volume_metadata.luminance_min);
  EXPECT_EQ(color_volume_metadata.luminance_max,
            deserialized_color_volume_metadata.luminance_max);
  EXPECT_EQ(color_volume_metadata, deserialized_color_volume_metadata);
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidHDRMetadata) {
  auto hdr_metadata =
      gfx::HDRMetadata(gfx::HdrMetadataSmpteSt2086(
                           {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f},
                           /*luminance_max=*/1000,
                           /*luminance_min=*/0),
                       gfx::HdrMetadataCta861_3(123, 456));

  std::vector<uint8_t> serialized_hdr_metadata =
      stable::mojom::HDRMetadata::Serialize(&hdr_metadata);

  gfx::HDRMetadata deserialized_hdr_metadata;
  ASSERT_TRUE(stable::mojom::HDRMetadata::Deserialize(
      serialized_hdr_metadata, &deserialized_hdr_metadata));

  EXPECT_EQ(hdr_metadata.smpte_st_2086,
            deserialized_hdr_metadata.smpte_st_2086);
  EXPECT_EQ(hdr_metadata.cta_861_3, deserialized_hdr_metadata.cta_861_3);
  EXPECT_EQ(hdr_metadata, deserialized_hdr_metadata);
}

TEST(StableVideoDecoderTypesMojomTraitsTest, ValidColorSpace) {
  skcms_Matrix3x3 primary_matrix = {{
      {0.205276f, 0.625671f, 0.060867f},
      {0.149185f, 0.063217f, 0.744553f},
      {0.609741f, 0.311111f, 0.019470f},
  }};
  skcms_TransferFunction transfer_fn = {2.1f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  auto color_space = gfx::ColorSpace::CreateCustom(primary_matrix, transfer_fn);

  std::vector<uint8_t> serialized_color_space =
      stable::mojom::ColorSpace::Serialize(&color_space);

  gfx::ColorSpace deserialized_color_space;
  ASSERT_TRUE(stable::mojom::ColorSpace::Deserialize(
      serialized_color_space, &deserialized_color_space));

  EXPECT_EQ(color_space.GetPrimaryID(),
            deserialized_color_space.GetPrimaryID());
  EXPECT_EQ(color_space.GetTransferID(),
            deserialized_color_space.GetTransferID());
  EXPECT_EQ(color_space.GetMatrixID(), deserialized_color_space.GetMatrixID());
  EXPECT_EQ(color_space.GetRangeID(), deserialized_color_space.GetRangeID());
  EXPECT_EQ(color_space.GetPrimaries(),
            deserialized_color_space.GetPrimaries());
  EXPECT_EQ(color_space, deserialized_color_space);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST(StableVideoDecoderTypesMojomTraitsTest, ValidNativeGpuMemoryBufferHandle) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.id = gfx::GpuMemoryBufferId(10);
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  uint32_t stride = 50;
  uint64_t offset = 0;
  uint64_t size = 2500;
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      stride, offset, size, CreateValidLookingBufferHandle(size + offset));
  stride = 25;
  offset = 2500;
  size = 625;
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      stride, offset, size, CreateValidLookingBufferHandle(size + offset));
  stride = 25;
  offset = 3125;
  size = 625;
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      stride, offset, size, CreateValidLookingBufferHandle(size + offset));
  gmb_handle.native_pixmap_handle.modifier = 123u;
  gmb_handle.native_pixmap_handle.supports_zero_copy_webgpu_import = true;

  // Mojo serialization can be destructive, so we clone the handle before
  // serialization in order to use it later to compare it against the handle
  // in the deserialized message.
  gfx::GpuMemoryBufferHandle gmb_handle_clone = gmb_handle.Clone();
  ASSERT_FALSE(gmb_handle_clone.native_pixmap_handle.planes.empty());

  auto message = stable::mojom::NativeGpuMemoryBufferHandle::SerializeAsMessage(
      &gmb_handle);
  ASSERT_TRUE(!message.IsNull());

  // Required to pass base deserialize checks.
  mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
  ASSERT_TRUE(handle.is_valid());
  auto received_message = mojo::Message::CreateFromMessageHandle(&handle);
  ASSERT_TRUE(!received_message.IsNull());

  gfx::GpuMemoryBufferHandle deserialized_gpu_native_pixmap_handle;
  ASSERT_TRUE(
      stable::mojom::NativeGpuMemoryBufferHandle::DeserializeFromMessage(
          std::move(received_message), &deserialized_gpu_native_pixmap_handle));

  EXPECT_EQ(gmb_handle_clone.id, deserialized_gpu_native_pixmap_handle.id);
  EXPECT_EQ(gfx::NATIVE_PIXMAP, deserialized_gpu_native_pixmap_handle.type);

  ASSERT_EQ(
      gmb_handle_clone.native_pixmap_handle.planes.size(),
      deserialized_gpu_native_pixmap_handle.native_pixmap_handle.planes.size());
  const auto pid = base::Process::Current().Pid();
  for (size_t i = 0; i < gmb_handle_clone.native_pixmap_handle.planes.size();
       i++) {
    EXPECT_EQ(
        gmb_handle_clone.native_pixmap_handle.planes[i].stride,
        deserialized_gpu_native_pixmap_handle.native_pixmap_handle.planes[i]
            .stride);
    EXPECT_EQ(
        gmb_handle_clone.native_pixmap_handle.planes[i].offset,
        deserialized_gpu_native_pixmap_handle.native_pixmap_handle.planes[i]
            .offset);
    EXPECT_EQ(
        gmb_handle_clone.native_pixmap_handle.planes[i].size,
        deserialized_gpu_native_pixmap_handle.native_pixmap_handle.planes[i]
            .size);
    EXPECT_EQ(syscall(SYS_kcmp, pid, pid, KCMP_FILE,
                      gmb_handle_clone.native_pixmap_handle.planes[i].fd.get(),
                      deserialized_gpu_native_pixmap_handle.native_pixmap_handle
                          .planes[i]
                          .fd.get()),
              0);
  }
  EXPECT_EQ(
      gmb_handle_clone.native_pixmap_handle.modifier,
      deserialized_gpu_native_pixmap_handle.native_pixmap_handle.modifier);
  // The |supports_zero_copy_webgpu_import| field is not intended to cross
  // process boundaries. It will not be serialized and it is set to false by
  // default.
  EXPECT_FALSE(deserialized_gpu_native_pixmap_handle.native_pixmap_handle
                   .supports_zero_copy_webgpu_import);
}

TEST(StableVideoDecoderTypesMojomTraitsTest,
     NativeGpuMemoryBufferHandleWithInvalidType) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.id = gfx::GpuMemoryBufferId(10);
  gmb_handle.type = gfx::SHARED_MEMORY_BUFFER;
  gmb_handle.region = base::UnsafeSharedMemoryRegion::Create(100);
  gmb_handle.offset = 2;
  gmb_handle.stride = 10;

  ASSERT_CHECK_DEATH(
      stable::mojom::NativeGpuMemoryBufferHandle::SerializeAsMessage(
          &gmb_handle));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}  // namespace media
