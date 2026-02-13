// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder_config.h"

#include "base/test/gtest_util.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioDecoderConfigTest : public testing::Test {
 public:
  AudioDecoderConfigTest() = default;
};

TEST_F(AudioDecoderConfigTest, InitializeWithChannelLayoutConfig_Discrete) {
  AudioDecoderConfig config;
  ChannelLayoutConfig layout_config(CHANNEL_LAYOUT_DISCRETE, 2);
  config.Initialize(AudioCodec::kOpus, kSampleFormatF32, layout_config, 48000,
                    {}, EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);
  EXPECT_TRUE(config.IsValidConfig());
  EXPECT_EQ(config.channel_layout(), CHANNEL_LAYOUT_DISCRETE);
  EXPECT_EQ(config.channels(), 2);
  EXPECT_EQ(config.bytes_per_frame(), 8);
}

TEST_F(AudioDecoderConfigTest, InitializeWithChannelLayoutConfig_Stereo) {
  AudioDecoderConfig config;
  ChannelLayoutConfig layout_config(CHANNEL_LAYOUT_STEREO, 2);
  config.Initialize(AudioCodec::kOpus, kSampleFormatF32, layout_config, 48000,
                    {}, EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);
  EXPECT_TRUE(config.IsValidConfig());
  EXPECT_EQ(config.channel_layout(), CHANNEL_LAYOUT_STEREO);
  EXPECT_EQ(config.channels(), 2);
  EXPECT_EQ(config.bytes_per_frame(), 8);
}

TEST_F(AudioDecoderConfigTest, InitializeWithChannelLayoutConfig_Bitstream) {
  AudioDecoderConfig config;
  ChannelLayoutConfig layout_config(CHANNEL_LAYOUT_BITSTREAM, 0);
  config.Initialize(AudioCodec::kOpus, kSampleFormatF32, layout_config, 48000,
                    {}, EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);
  EXPECT_TRUE(config.IsValidConfig());
  EXPECT_EQ(config.channel_layout(), CHANNEL_LAYOUT_BITSTREAM);
  // This information is in the bitstream, so we expect zero here.
  EXPECT_EQ(config.channels(), 0);
  EXPECT_EQ(config.bytes_per_frame(), 0);
}

TEST_F(AudioDecoderConfigTest, IsValidConfig_InvalidSampleFormat) {
  AudioDecoderConfig config;
  ChannelLayoutConfig layout_config(CHANNEL_LAYOUT_STEREO, 2);
  config.Initialize(AudioCodec::kOpus, kUnknownSampleFormat, layout_config,
                    48000, {}, EncryptionScheme::kUnencrypted,
                    base::TimeDelta(), 0);
  EXPECT_FALSE(config.IsValidConfig());
}

}  // namespace media
