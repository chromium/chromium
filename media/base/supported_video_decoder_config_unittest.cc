// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_video_decoder_config.h"
#include "media/base/test_helpers.h"
#include "media/base/video_codecs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class SupportedVideoDecoderConfigTest : public ::testing::Test {
 public:
  SupportedVideoDecoderConfigTest()
      : decoder_config_(
            TestVideoConfig::NormalCodecProfile(VideoCodec::kH264,
                                                H264PROFILE_EXTENDED)) {
    supported_config_.profile_min = H264PROFILE_MIN;
    supported_config_.profile_max = H264PROFILE_MAX;
    supported_config_.coded_size_min = gfx::Size(10, 20);
    supported_config_.coded_size_max = gfx::Size(10000, 20000);
    supported_config_.allow_encrypted = true;
    supported_config_.require_encrypted = false;
  }

  SupportedVideoDecoderConfig supported_config_;

  // Decoder config that matches |supported_config_|.
  VideoDecoderConfig decoder_config_;
};

TEST_F(SupportedVideoDecoderConfigTest, ConstructionWithArgs) {
  SupportedVideoDecoderConfig config2(
      supported_config_.profile_min, supported_config_.profile_max,
      supported_config_.coded_size_min, supported_config_.coded_size_max,
      supported_config_.allow_encrypted, supported_config_.require_encrypted);
  EXPECT_EQ(supported_config_.profile_min, config2.profile_min);
  EXPECT_EQ(supported_config_.profile_max, config2.profile_max);
  EXPECT_EQ(supported_config_.coded_size_min, config2.coded_size_min);
  EXPECT_EQ(supported_config_.coded_size_max, config2.coded_size_max);
  EXPECT_EQ(supported_config_.allow_encrypted, config2.allow_encrypted);
  EXPECT_EQ(supported_config_.require_encrypted, config2.require_encrypted);
}

TEST_F(SupportedVideoDecoderConfigTest, MatchingConfigMatches) {
  EXPECT_TRUE(supported_config_.Matches(decoder_config_));

  // Since |supported_config_| allows encrypted, this should also succeed.
  decoder_config_.SetIsEncrypted(true);
  EXPECT_TRUE(supported_config_.Matches(decoder_config_));
}

TEST_F(SupportedVideoDecoderConfigTest, LowerProfileMismatches) {
  // Raise |profile_min| above |decoder_config_|.
  supported_config_.profile_min = H264PROFILE_HIGH;
  EXPECT_FALSE(supported_config_.Matches(decoder_config_));
}

TEST_F(SupportedVideoDecoderConfigTest, HigherProfileMismatches) {
  // Lower |profile_max| below |decoder_config_|.
  supported_config_.profile_max = H264PROFILE_MAIN;
  EXPECT_FALSE(supported_config_.Matches(decoder_config_));
}

TEST_F(SupportedVideoDecoderConfigTest, SmallerMinWidthMismatches) {
  supported_config_.coded_size_min =
      gfx::Size(decoder_config_.coded_size().width() + 1, 0);
  EXPECT_FALSE(supported_config_.Matches(decoder_config_));
}

TEST_F(SupportedVideoDecoderConfigTest, SmallerMinHeightMismatches) {
  supported_config_.coded_size_min =
      gfx::Size(0, decoder_config_.coded_size().height() + 1);
  EXPECT_FALSE(supported_config_.Matches(decoder_config_));
}

TEST_F(SupportedVideoDecoderConfigTest, LargerMaxWidthMismatches) {
  supported_config_.coded_size_max =
      gfx::Size(decoder_config_.coded_size().width() - 1, 10000);
  EXPECT_FALSE(supported_config_.Matches(decoder_config_));
}

TEST_F(SupportedVideoDecoderConfigTest, LargerMaxHeightMismatches) {
  supported_config_.coded_size_max =
      gfx::Size(10000, decoder_config_.coded_size().height() - 1);
  EXPECT_FALSE(supported_config_.Matches(decoder_config_));
}

TEST_F(SupportedVideoDecoderConfigTest, RequiredEncryptionMismatches) {
  supported_config_.require_encrypted = true;
  EXPECT_FALSE(supported_config_.Matches(decoder_config_));

  // The encrypted version should succeed.
  decoder_config_.SetIsEncrypted(true);
  EXPECT_TRUE(supported_config_.Matches(decoder_config_));
}

TEST_F(SupportedVideoDecoderConfigTest, AllowedEncryptionMismatches) {
  supported_config_.allow_encrypted = false;
  decoder_config_.SetIsEncrypted(true);
  EXPECT_FALSE(supported_config_.Matches(decoder_config_));
}

}  // namespace media
