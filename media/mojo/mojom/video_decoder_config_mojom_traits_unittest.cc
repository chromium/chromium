// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_decoder_config_mojom_traits.h"

#include <utility>

#include "base/stl_util.h"
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
      &kExtraData[0], &kExtraData[0] + base::size(kExtraData));
  VideoDecoderConfig input(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      kNaturalSize, kExtraDataVector, EncryptionScheme::kUnencrypted);
  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(VideoDecoderConfigStructTraitsTest,
     ConvertVideoDecoderConfig_EmptyExtraData) {
  VideoDecoderConfig input(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      kNaturalSize, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  std::vector<uint8_t> data =
      media::mojom::VideoDecoderConfig::Serialize(&input);
  VideoDecoderConfig output;
  EXPECT_TRUE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
  EXPECT_TRUE(output.Matches(input));
}

TEST(VideoDecoderConfigStructTraitsTest, ConvertVideoDecoderConfig_Encrypted) {
  VideoDecoderConfig input(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      kNaturalSize, EmptyExtraData(), EncryptionScheme::kCenc);
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
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
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
  VideoDecoderConfig input(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      kNaturalSize, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  HDRMetadata hdr_metadata;
  hdr_metadata.max_frame_average_light_level = 123;
  hdr_metadata.max_content_light_level = 456;
  hdr_metadata.mastering_metadata.primary_r.set_x(0.1f);
  hdr_metadata.mastering_metadata.primary_r.set_y(0.2f);
  hdr_metadata.mastering_metadata.primary_g.set_x(0.3f);
  hdr_metadata.mastering_metadata.primary_g.set_y(0.4f);
  hdr_metadata.mastering_metadata.primary_b.set_x(0.5f);
  hdr_metadata.mastering_metadata.primary_b.set_y(0.6f);
  hdr_metadata.mastering_metadata.white_point.set_x(0.7f);
  hdr_metadata.mastering_metadata.white_point.set_y(0.8f);
  hdr_metadata.mastering_metadata.luminance_max = 1000;
  hdr_metadata.mastering_metadata.luminance_min = 0;
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
  input.Initialize(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      kInvalidNaturalSize, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(input.IsValidConfig());

  // Deserialize should again fail due to invalid config.
  EXPECT_FALSE(
      media::mojom::VideoDecoderConfig::Deserialize(std::move(data), &output));
}

}  // namespace media
