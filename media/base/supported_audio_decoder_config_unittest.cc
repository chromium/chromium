// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_audio_decoder_config.h"

#include "media/base/audio_codecs.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class SupportedAudioDecoderConfigTest : public ::testing::Test {
 public:
  SupportedAudioDecoderConfigTest() {
    supported_config_.codec = AudioCodec::kAAC;
    supported_config_.profile = AudioCodecProfile::kXHE_AAC;
  }

  SupportedAudioDecoderConfig supported_config_;
};

TEST_F(SupportedAudioDecoderConfigTest, ConstructionWithArgs) {
  SupportedAudioDecoderConfig config2(supported_config_.codec,
                                      supported_config_.profile);
  EXPECT_EQ(supported_config_, config2);
}

TEST_F(SupportedAudioDecoderConfigTest, CodecMismatches) {
  SupportedAudioDecoderConfig config2(AudioCodec::kUnknown,
                                      supported_config_.profile);
  EXPECT_NE(supported_config_, config2);
}

TEST_F(SupportedAudioDecoderConfigTest, ProfileMismatches) {
  SupportedAudioDecoderConfig config2(supported_config_.codec,
                                      AudioCodecProfile::kUnknown);
  EXPECT_NE(supported_config_, config2);
}

}  // namespace media
