// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/openscreen/config_conversions.h"

#include "base/containers/contains.h"
#include "media/base/media_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/capture_configs.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media::cast {
namespace {

void ValidateAudioConfig(const media::AudioDecoderConfig& config,
                         const media::AudioDecoderConfig& expected) {
  EXPECT_EQ(config.codec(), expected.codec());
  EXPECT_EQ(config.sample_format(), media::SampleFormat::kSampleFormatF32);
  EXPECT_EQ(config.channel_layout(), expected.channel_layout());
  EXPECT_EQ(config.samples_per_second(), expected.samples_per_second());
  EXPECT_EQ(config.extra_data().size(), size_t{0});
  EXPECT_FALSE(config.is_encrypted());
}

void ValidateAudioConfig(const openscreen::cast::AudioCaptureConfig& config,
                         const openscreen::cast::AudioCaptureConfig& expected) {
  EXPECT_EQ(config.codec, expected.codec);
  EXPECT_EQ(config.channels, expected.channels);
  EXPECT_EQ(config.bit_rate, expected.bit_rate);
  EXPECT_EQ(config.sample_rate, expected.sample_rate);
  EXPECT_EQ(config.target_playout_delay, expected.target_playout_delay);
}

void ValidateVideoConfig(const media::VideoDecoderConfig& config,
                         const media::VideoDecoderConfig& expected) {
  EXPECT_EQ(config.codec(), expected.codec());
  EXPECT_EQ(config.profile(), expected.profile());
  EXPECT_EQ(config.alpha_mode(),
            media::VideoDecoderConfig::AlphaMode::kIsOpaque);
  EXPECT_EQ(config.extra_data().size(), size_t{0});
  EXPECT_FALSE(config.is_encrypted());

  EXPECT_EQ(config.coded_size().width(), expected.coded_size().width());
  EXPECT_EQ(config.coded_size().height(), expected.coded_size().height());

  EXPECT_EQ(config.visible_rect().width(), expected.visible_rect().width());
  EXPECT_EQ(config.visible_rect().height(), expected.visible_rect().height());

  EXPECT_EQ(config.natural_size().width(), expected.natural_size().width());
  EXPECT_EQ(config.natural_size().height(), expected.natural_size().height());
}

void ValidateVideoConfig(const openscreen::cast::VideoCaptureConfig& config,
                         const openscreen::cast::VideoCaptureConfig& expected) {
  EXPECT_EQ(config.codec, expected.codec);
  EXPECT_EQ(config.max_frame_rate, expected.max_frame_rate);
  EXPECT_EQ(config.max_bit_rate, expected.max_bit_rate);
  EXPECT_EQ(config.target_playout_delay, expected.target_playout_delay);
  ASSERT_EQ(config.resolutions.size(), expected.resolutions.size());
  for (const auto& resolution : config.resolutions) {
    EXPECT_TRUE(base::Contains(expected.resolutions, resolution));
  }
}

openscreen::cast::AudioCaptureConfig CreateAudioCaptureConfig() {
  openscreen::cast::AudioCaptureConfig config;
  config.codec = openscreen::cast::AudioCodec::kAac;
  config.channels = 2;
  config.sample_rate = 42;
  return config;
}

media::AudioDecoderConfig CreateAudioDecoderConfig(
    media::AudioCodec codec,
    media::ChannelLayout channel_layout,
    int samples_per_second) {
  return media::AudioDecoderConfig(codec, media::SampleFormat::kSampleFormatF32,
                                   channel_layout, samples_per_second,
                                   media::EmptyExtraData(),
                                   media::EncryptionScheme::kUnencrypted);
}

openscreen::cast::VideoCaptureConfig CreateVideoCaptureConfig() {
  openscreen::cast::VideoCaptureConfig config;
  config.codec = openscreen::cast::VideoCodec::kH264;
  config.resolutions.push_back({1080, 720});
  return config;
}

media::VideoDecoderConfig CreateVideoDecoderConfig(
    media::VideoCodec codec,
    media::VideoCodecProfile codec_profile,
    int width,
    int height) {
  gfx::Size video_size(width, height);
  gfx::Rect video_rect(width, height);
  return media::VideoDecoderConfig(
      codec, codec_profile, media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      media::VideoColorSpace(), media::VideoTransformation(), video_size,
      video_rect, video_size, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);
}

}  // namespace

TEST(ConfigConversionsTest, AudioConfigCodecConversion) {
  auto capture_config = CreateAudioCaptureConfig();
  auto decoder_config = CreateAudioDecoderConfig(
      media::AudioCodec::kAAC, media::ChannelLayout::CHANNEL_LAYOUT_STEREO, 42);
  ValidateAudioConfig(ToAudioDecoderConfig(capture_config), decoder_config);
  ValidateAudioConfig(ToAudioCaptureConfig(decoder_config), capture_config);

  capture_config.codec = openscreen::cast::AudioCodec::kOpus;
  decoder_config =
      CreateAudioDecoderConfig(media::AudioCodec::kOpus,
                               media::ChannelLayout::CHANNEL_LAYOUT_STEREO, 42);
  ValidateAudioConfig(ToAudioDecoderConfig(capture_config), decoder_config);
  ValidateAudioConfig(ToAudioCaptureConfig(decoder_config), capture_config);
}

TEST(ConfigConversionsTest, AudioConfigChannelsConversion) {
  auto capture_config = CreateAudioCaptureConfig();
  auto decoder_config = CreateAudioDecoderConfig(
      media::AudioCodec::kAAC, media::ChannelLayout::CHANNEL_LAYOUT_STEREO, 42);
  ValidateAudioConfig(ToAudioDecoderConfig(capture_config), decoder_config);
  ValidateAudioConfig(ToAudioCaptureConfig(decoder_config), capture_config);

  capture_config.channels = 1;
  decoder_config = CreateAudioDecoderConfig(
      media::AudioCodec::kAAC, media::ChannelLayout::CHANNEL_LAYOUT_MONO, 42);
  ValidateAudioConfig(ToAudioDecoderConfig(capture_config), decoder_config);
  ValidateAudioConfig(ToAudioCaptureConfig(decoder_config), capture_config);

  // Other configurations are not expected in practice.
}

TEST(ConfigConversionsTest, AudioConfigSampleRateConversion) {
  auto capture_config = CreateAudioCaptureConfig();
  auto decoder_config = CreateAudioDecoderConfig(
      media::AudioCodec::kAAC, media::ChannelLayout::CHANNEL_LAYOUT_STEREO, 42);
  ValidateAudioConfig(ToAudioDecoderConfig(capture_config), decoder_config);
  ValidateAudioConfig(ToAudioCaptureConfig(decoder_config), capture_config);

  capture_config.sample_rate = 1234;
  decoder_config = CreateAudioDecoderConfig(
      media::AudioCodec::kAAC, media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
      1234);
  ValidateAudioConfig(ToAudioDecoderConfig(capture_config), decoder_config);
  ValidateAudioConfig(ToAudioCaptureConfig(decoder_config), capture_config);

  capture_config.sample_rate = -1;
  decoder_config = CreateAudioDecoderConfig(
      media::AudioCodec::kAAC, media::ChannelLayout::CHANNEL_LAYOUT_STEREO, -1);
  ValidateAudioConfig(ToAudioDecoderConfig(capture_config), decoder_config);
  ValidateAudioConfig(ToAudioCaptureConfig(decoder_config), capture_config);

  capture_config.sample_rate = 0;
  decoder_config = CreateAudioDecoderConfig(
      media::AudioCodec::kAAC, media::ChannelLayout::CHANNEL_LAYOUT_STEREO, 0);
  ValidateAudioConfig(ToAudioDecoderConfig(capture_config), decoder_config);
  ValidateAudioConfig(ToAudioCaptureConfig(decoder_config), capture_config);
}

TEST(ConfigConversionsTest, VideoConfigCodecConversion) {
  const int width = 1080;
  const int height = 720;
  auto capture_config = CreateVideoCaptureConfig();
  auto decoder_config = CreateVideoDecoderConfig(
      media::VideoCodec::kH264, media::VideoCodecProfile::H264PROFILE_BASELINE,
      width, height);
  ValidateVideoConfig(ToVideoDecoderConfig(capture_config), decoder_config);
  ValidateVideoConfig(ToVideoCaptureConfig(decoder_config), capture_config);

  capture_config.codec = openscreen::cast::VideoCodec::kVp8;
  decoder_config = CreateVideoDecoderConfig(
      media::VideoCodec::kVP8, media::VideoCodecProfile::VP8PROFILE_MIN, width,
      height);
  ValidateVideoConfig(ToVideoDecoderConfig(capture_config), decoder_config);
  ValidateVideoConfig(ToVideoCaptureConfig(decoder_config), capture_config);

  capture_config.codec = openscreen::cast::VideoCodec::kHevc;
  decoder_config = CreateVideoDecoderConfig(
      media::VideoCodec::kHEVC, media::VideoCodecProfile::HEVCPROFILE_MAIN,
      width, height);
  ValidateVideoConfig(ToVideoDecoderConfig(capture_config), decoder_config);
  ValidateVideoConfig(ToVideoCaptureConfig(decoder_config), capture_config);

  capture_config.codec = openscreen::cast::VideoCodec::kVp9;
  decoder_config = CreateVideoDecoderConfig(
      media::VideoCodec::kVP9, media::VideoCodecProfile::VP9PROFILE_PROFILE0,
      width, height);
  ValidateVideoConfig(ToVideoDecoderConfig(capture_config), decoder_config);
  ValidateVideoConfig(ToVideoCaptureConfig(decoder_config), capture_config);
}

TEST(ConfigConversionsTest, VideoConfigResolutionConversion) {
  auto capture_config = CreateVideoCaptureConfig();
  auto decoder_config = CreateVideoDecoderConfig(
      media::VideoCodec::kH264, media::VideoCodecProfile::H264PROFILE_BASELINE,
      1080, 720);
  ValidateVideoConfig(ToVideoDecoderConfig(capture_config), decoder_config);
  ValidateVideoConfig(ToVideoCaptureConfig(decoder_config), capture_config);

  ASSERT_EQ(capture_config.resolutions.size(), size_t{1});

  capture_config.resolutions[0].width = 42;
  capture_config.resolutions[0].height = 16;
  decoder_config = CreateVideoDecoderConfig(
      media::VideoCodec::kH264, media::VideoCodecProfile::H264PROFILE_BASELINE,
      42, 16);
  ValidateVideoConfig(ToVideoDecoderConfig(capture_config), decoder_config);
  ValidateVideoConfig(ToVideoCaptureConfig(decoder_config), capture_config);

  capture_config.resolutions[0].width = 1;
  capture_config.resolutions[0].height = 2;
  decoder_config = CreateVideoDecoderConfig(
      media::VideoCodec::kH264, media::VideoCodecProfile::H264PROFILE_BASELINE,
      1, 2);
  ValidateVideoConfig(ToVideoDecoderConfig(capture_config), decoder_config);
  ValidateVideoConfig(ToVideoCaptureConfig(decoder_config), capture_config);

  capture_config.resolutions[0].width = 0;
  capture_config.resolutions[0].height = 0;
  decoder_config = CreateVideoDecoderConfig(
      media::VideoCodec::kH264, media::VideoCodecProfile::H264PROFILE_BASELINE,
      0, 0);
  ValidateVideoConfig(ToVideoDecoderConfig(capture_config), decoder_config);
  ValidateVideoConfig(ToVideoCaptureConfig(decoder_config), capture_config);
}

}  // namespace media::cast
