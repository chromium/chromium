// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_video_client.h"

#include "media/base/media_util.h"
#include "media/base/mock_media_log.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/webm/webm_constants.h"

namespace media {

namespace {
const gfx::Size kCodedSize(321, 243);
}

static const struct CodecTestParams {
  VideoCodecProfile profile;
  const std::vector<uint8_t> codec_private;
} kCodecTestParams[] = {
    {VP9PROFILE_PROFILE0, {}},
    {VP9PROFILE_PROFILE2,
     // Valid VP9 Profile 2 example, extracted out of a sample file at
     // https://www.webmproject.org/vp9/levels/#test-bitstreams
     {0x01, 0x01, 0x02, 0x02, 0x01, 0x0a, 0x3, 0x1, 0xa, 0x4, 0x1, 0x1}},
    // Invalid VP9 CodecPrivate: too short.
    {VP9PROFILE_PROFILE0, {0x01, 0x01}},
    // Invalid VP9 CodecPrivate: wrong field id.
    {VP9PROFILE_PROFILE0, {0x77, 0x01, 0x02}},
    // Invalid VP9 CodecPrivate: wrong field length.
    {VP9PROFILE_PROFILE0, {0x01, 0x75, 0x02}},
    // Invalid VP9 CodecPrivate: wrong Profile (can't be > 3).
    {VP9PROFILE_PROFILE0, {0x01, 0x01, 0x34}}};

class WebMVideoClientTest : public testing::TestWithParam<CodecTestParams> {
 public:
  WebMVideoClientTest() : webm_video_client_(&media_log_) {
    // Simulate configuring width and height in the |webm_video_client_|.
    webm_video_client_.OnUInt(kWebMIdPixelWidth, kCodedSize.width());
    webm_video_client_.OnUInt(kWebMIdPixelHeight, kCodedSize.height());
  }

  WebMParserClient* OnListStart(int id) {
    return webm_video_client_.OnListStart(id);
  }

  void OnListEnd(int id) { webm_video_client_.OnListEnd(id); }

  testing::StrictMock<MockMediaLog> media_log_;
  WebMVideoClient webm_video_client_;

  DISALLOW_COPY_AND_ASSIGN(WebMVideoClientTest);
};

TEST_P(WebMVideoClientTest, AutodetectVp9Profile2NoDetection) {
  const bool has_valid_codec_private = GetParam().codec_private.size() > 3;

  auto* parser = OnListStart(kWebMIdColour);
  // Set 8bit and SDR fields.
  parser->OnUInt(kWebMIdBitsPerChannel, 8);
  parser->OnUInt(kWebMIdTransferCharacteristics,
                 static_cast<int64_t>(VideoColorSpace::TransferID::BT709));
  OnListEnd(kWebMIdColour);

  VideoDecoderConfig config;
  EXPECT_TRUE(webm_video_client_.InitializeConfig(
      "V_VP9", GetParam().codec_private, EncryptionScheme(), &config));

  if (!has_valid_codec_private)
    EXPECT_EQ(config.profile(), VP9PROFILE_PROFILE0);
  else
    EXPECT_EQ(config.profile(), GetParam().profile);
}

TEST_P(WebMVideoClientTest, AutodetectVp9Profile2BitsPerChannel) {
  const bool has_valid_codec_private = GetParam().codec_private.size() > 3;

  auto* parser = OnListStart(kWebMIdColour);
  parser->OnUInt(kWebMIdBitsPerChannel, 10);
  OnListEnd(kWebMIdColour);

  VideoDecoderConfig config;
  EXPECT_TRUE(webm_video_client_.InitializeConfig(
      "V_VP9", GetParam().codec_private, EncryptionScheme(), &config));

  if (!has_valid_codec_private)
    EXPECT_EQ(config.profile(), VP9PROFILE_PROFILE2);
  else
    EXPECT_EQ(config.profile(), GetParam().profile);
}

TEST_P(WebMVideoClientTest, AutodetectVp9Profile2HDRMetaData) {
  const bool has_valid_codec_private = GetParam().codec_private.size() > 3;

  auto* color_parser = OnListStart(kWebMIdColour);
  auto* metadata_parser = color_parser->OnListStart(kWebMIdMasteringMetadata);
  metadata_parser->OnFloat(kWebMIdPrimaryRChromaticityX, 1.0);
  color_parser->OnListEnd(kWebMIdMasteringMetadata);
  OnListEnd(kWebMIdColour);

  VideoDecoderConfig config;
  EXPECT_TRUE(webm_video_client_.InitializeConfig(
      "V_VP9", GetParam().codec_private, EncryptionScheme(), &config));

  if (!has_valid_codec_private)
    EXPECT_EQ(config.profile(), VP9PROFILE_PROFILE2);
  else
    EXPECT_EQ(config.profile(), GetParam().profile);
}

TEST_P(WebMVideoClientTest, AutodetectVp9Profile2HDRColorSpace) {
  const bool has_valid_codec_private = GetParam().codec_private.size() > 3;

  auto* parser = OnListStart(kWebMIdColour);
  parser->OnUInt(
      kWebMIdTransferCharacteristics,
      static_cast<int64_t>(VideoColorSpace::TransferID::SMPTEST2084));
  OnListEnd(kWebMIdColour);

  VideoDecoderConfig config;
  EXPECT_TRUE(webm_video_client_.InitializeConfig(
      "V_VP9", GetParam().codec_private, EncryptionScheme(), &config));

  if (!has_valid_codec_private)
    EXPECT_EQ(config.profile(), VP9PROFILE_PROFILE2);
  else
    EXPECT_EQ(config.profile(), GetParam().profile);
}

TEST_P(WebMVideoClientTest, InitializeConfigVP9Profiles) {
  const std::string kCodecId = "V_VP9";
  const VideoCodecProfile profile = GetParam().profile;
  const std::vector<uint8_t> codec_private = GetParam().codec_private;

  VideoDecoderConfig config;
  EXPECT_TRUE(webm_video_client_.InitializeConfig(kCodecId, codec_private,
                                                  EncryptionScheme(), &config));

  VideoDecoderConfig expected_config(
      kCodecVP9, profile, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace::REC709(), kNoTransformation, kCodedSize,
      gfx::Rect(kCodedSize), kCodedSize, codec_private,
      EncryptionScheme::kUnencrypted);

  EXPECT_TRUE(config.Matches(expected_config))
      << "Config (" << config.AsHumanReadableString()
      << ") does not match expected ("
      << expected_config.AsHumanReadableString() << ")";
}

INSTANTIATE_TEST_SUITE_P(/* No prefix. */,
                         WebMVideoClientTest,
                         ::testing::ValuesIn(kCodecTestParams));

}  // namespace media
