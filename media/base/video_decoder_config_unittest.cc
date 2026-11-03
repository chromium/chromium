// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static constexpr gfx::Size kCodedSize(320, 240);
static constexpr gfx::Rect kVisibleRect(320, 240);
static constexpr gfx::Size kNaturalSize(320, 240);

TEST(VideoDecoderConfigTest, AlphaModeSetCorrectly) {
  VideoDecoderConfig config(VideoCodec::kVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoDecoderConfig::AlphaMode::kIsOpaque,
                            VideoColorSpace(), kNoTransformation, kCodedSize,
                            kVisibleRect, kNaturalSize, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted);
  EXPECT_TRUE(config.IsValidConfig());
  EXPECT_EQ(config.alpha_mode(), VideoDecoderConfig::AlphaMode::kIsOpaque);

  config.Initialize(VideoCodec::kVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                    VideoDecoderConfig::AlphaMode::kHasAlpha, VideoColorSpace(),
                    kNoTransformation, kCodedSize, kVisibleRect, kNaturalSize,
                    EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(config.alpha_mode(), VideoDecoderConfig::AlphaMode::kHasAlpha);
}

TEST(VideoDecoderConfigTest, SetProfile) {
  VideoDecoderConfig config(VideoCodec::kVP9, VP9PROFILE_PROFILE0,
                            VideoDecoderConfig::AlphaMode::kIsOpaque,
                            VideoColorSpace(), kNoTransformation, kCodedSize,
                            kVisibleRect, kNaturalSize, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted);
  config.set_profile(VP9PROFILE_PROFILE2);
  EXPECT_EQ(config.profile(), VP9PROFILE_PROFILE2);
}

TEST(VideoDecoderConfigTest, MatchesWithProjection) {
  VideoDecoderConfig config(VideoCodec::kVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoDecoderConfig::AlphaMode::kIsOpaque,
                            VideoColorSpace(), kNoTransformation, kCodedSize,
                            kVisibleRect, kNaturalSize, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted);
  EXPECT_EQ(config.spatial_format().projection_type,
            VideoProjectionType::kNone);
  EXPECT_EQ(config.spatial_format().stereo_mode, VideoStereoMode::kMono);

  VideoDecoderConfig config2 = config;
  EXPECT_TRUE(config.Matches(config2));

  config.set_spatial_format(VideoSpatialFormat{
      VideoProjectionType::kEquirect360,
      VideoStereoMode::kSideBySideLeftFirst,
  });
  EXPECT_FALSE(config.Matches(config2));

  config2.set_spatial_format(VideoSpatialFormat{
      VideoProjectionType::kEquirect360,
      VideoStereoMode::kSideBySideLeftFirst,
  });
  EXPECT_TRUE(config.Matches(config2));
}

}  // namespace media
