// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/mojom/video_decoder_config_mojom_traits.h"

#include <utility>

#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

}  // namespace

TEST(VideoDecoderConfigStructTraitsTest, ConvertVideoDecoderConfig_Normal) {
  const uint8_t kExtraData[] = "config extra data";
  const std::vector<uint8_t> kExtraDataVector(
      &kExtraData[0], &kExtraData[0] + std::size(kExtraData));
  VideoDecoderConfig input(VideoCodec::kVP8, VP8PROFILE_ANY,
                           VideoDecoderConfig::AlphaMode::kIsOpaque,
                           VideoColorSpace(), kNoTransformation, kCodedSize,
                           kVisibleRect, kNaturalSize, kExtraDataVector,
                           EncryptionScheme::kUnencrypted);
  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(VideoDecoderConfigStructTraitsTest,
     ConvertVideoDecoderConfig_EmptyExtraData) {
  VideoDecoderConfig input(VideoCodec::kVP8, VP8PROFILE_ANY,
                           VideoDecoderConfig::AlphaMode::kIsOpaque,
                           VideoColorSpace(), kNoTransformation, kCodedSize,
                           kVisibleRect, kNaturalSize, EmptyExtraData(),
                           EncryptionScheme::kUnencrypted);
  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(VideoDecoderConfigStructTraitsTest, ConvertVideoDecoderConfig_Encrypted) {
  VideoDecoderConfig input(VideoCodec::kVP8, VP8PROFILE_ANY,
                           VideoDecoderConfig::AlphaMode::kIsOpaque,
                           VideoColorSpace(), kNoTransformation, kCodedSize,
                           kVisibleRect, kNaturalSize, EmptyExtraData(),
                           EncryptionScheme::kCenc);
  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(VideoDecoderConfigStructTraitsTest,
     ConvertVideoDecoderConfig_AspectRatio) {
  VideoDecoderConfig input(
      VideoCodec::kVP8, VP8PROFILE_ANY,
      VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                      VideoColorSpace::TransferID::SMPTEST2084,
                      VideoColorSpace::MatrixID::BT2020_CL,
                      gfx::ColorSpace::RangeID::LIMITED),
      kNoTransformation, kCodedSize, kVisibleRect, kNaturalSize,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  input.set_aspect_ratio(VideoAspectRatio::DAR(3, 1));
  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(VideoDecoderConfigStructTraitsTest,
     ConvertVideoDecoderConfig_ColorSpaceInfo) {
  VideoDecoderConfig input(
      VideoCodec::kVP8, VP8PROFILE_ANY,
      VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                      VideoColorSpace::TransferID::SMPTEST2084,
                      VideoColorSpace::MatrixID::BT2020_CL,
                      gfx::ColorSpace::RangeID::LIMITED),
      kNoTransformation, kCodedSize, kVisibleRect, kNaturalSize,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(VideoDecoderConfigStructTraitsTest,
     ConvertVideoDecoderConfig_HDRMetadata) {
  VideoDecoderConfig input(VideoCodec::kVP8, VP8PROFILE_ANY,
                           VideoDecoderConfig::AlphaMode::kIsOpaque,
                           VideoColorSpace(), kNoTransformation, kCodedSize,
                           kVisibleRect, kNaturalSize, EmptyExtraData(),
                           EncryptionScheme::kUnencrypted);
  gfx::HDRMetadata hdr_metadata;
  hdr_metadata.cta_861_3 = gfx::HdrMetadataCta861_3(123, 456);
  hdr_metadata.smpte_st_2086 = gfx::HdrMetadataSmpteSt2086(
      {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f},
      /*luminance_max=*/1000,
      /*luminance_min=*/0);
  input.set_hdr_metadata(hdr_metadata);
  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(VideoDecoderConfigStructTraitsTest,
     ConvertVideoDecoderConfig_InvalidConfigs) {
  // Create an invalid empty config.
  VideoDecoderConfig input;
  EXPECT_FALSE(input.IsValidConfig());

  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;

  // Deserialize should only pass for valid configs.
  EXPECT_FALSE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));

  // Next try an non-empty invalid config. Natural size must not be zero.
  const gfx::Size kInvalidNaturalSize(0, 0);
  input.Initialize(VideoCodec::kVP8, VP8PROFILE_ANY,
                   VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
                   kNoTransformation, kCodedSize, kVisibleRect,
                   kInvalidNaturalSize, EmptyExtraData(),
                   EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(input.IsValidConfig());

  // Deserialize should again fail due to invalid config.
  EXPECT_FALSE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
}

}  // namespace media
