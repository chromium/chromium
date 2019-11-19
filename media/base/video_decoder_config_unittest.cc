// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

TEST(VideoDecoderConfigTest, AlphaModeSetCorrectly) {
  VideoDecoderConfig config(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoDecoderConfig::AlphaMode::kIsOpaque,
                            VideoColorSpace(), kNoTransformation, kCodedSize,
                            kVisibleRect, kNaturalSize, EmptyExtraData(),
                            EncryptionScheme::kUnencrypted);
  EXPECT_TRUE(config.IsValidConfig());
  EXPECT_EQ(config.alpha_mode(), VideoDecoderConfig::AlphaMode::kIsOpaque);

  config.Initialize(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                    VideoDecoderConfig::AlphaMode::kHasAlpha, VideoColorSpace(),
                    kNoTransformation, kCodedSize, kVisibleRect, kNaturalSize,
                    EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_EQ(config.alpha_mode(), VideoDecoderConfig::AlphaMode::kHasAlpha);
}

TEST(VideoDecoderConfigTest, Invalid_AspectRatioNumeratorZero) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 0, 1);
  VideoDecoderConfig config(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      natural_size, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(config.IsValidConfig());
}

TEST(VideoDecoderConfigTest, Invalid_AspectRatioDenominatorZero) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, 0);
  VideoDecoderConfig config(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      natural_size, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(config.IsValidConfig());
}

TEST(VideoDecoderConfigTest, Invalid_AspectRatioNumeratorNegative) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), -1, 1);
  VideoDecoderConfig config(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      natural_size, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(config.IsValidConfig());
}

TEST(VideoDecoderConfigTest, Invalid_AspectRatioDenominatorNegative) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, -1);
  VideoDecoderConfig config(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      natural_size, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(config.IsValidConfig());
}

TEST(VideoDecoderConfigTest, Invalid_AspectRatioNumeratorTooLarge) {
  int width = kVisibleRect.size().width();
  int num = ceil(static_cast<double>(limits::kMaxDimension + 1) / width);
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), num, 1);
  VideoDecoderConfig config(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      natural_size, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(config.IsValidConfig());
}

TEST(VideoDecoderConfigTest, Invalid_AspectRatioDenominatorVeryLarge) {
  // This test makes sure that highly skewed pixel ratios arent counted as valid
  // configurations.
  int den = 2 * kVisibleRect.size().width() + 1;
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, den);
  EXPECT_EQ(320, natural_size.width());
  EXPECT_EQ(240 * 641, natural_size.height());
  VideoDecoderConfig config(
      kCodecVP8, VP8PROFILE_ANY, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(), kNoTransformation, kCodedSize, kVisibleRect,
      natural_size, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(config.IsValidConfig());
}

}  // namespace media
