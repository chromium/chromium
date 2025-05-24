// Copyright 2017 The Chromium Authors
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

MATCHER(UnexpectedStereoMode, "") {
  return CONTAINS_STRING(arg, "Unexpected value for StereoMode: 0x");
}

MATCHER(UnexpectedMultipleValues, "") {
  return CONTAINS_STRING(arg, "Multiple values for id");
}
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

  WebMVideoClientTest(const WebMVideoClientTest&) = delete;
  WebMVideoClientTest& operator=(const WebMVideoClientTest&) = delete;

  WebMParserClient* OnListStart(int id) {
    return webm_video_client_.OnListStart(id);
  }

  void OnListEnd(int id) { webm_video_client_.OnListEnd(id); }

  bool OnUInt(int id, int64_t val) {
    return webm_video_client_.OnUInt(id, val);
  }

  testing::StrictMock<MockMediaLog> media_log_;
  WebMVideoClient webm_video_client_;
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
  auto* metadata_parser = color_parser->OnListStart(kWebMIdColorVolumeMetadata);
  metadata_parser->OnFloat(kWebMIdPrimaryRChromaticityX, 1.0);
  color_parser->OnListEnd(kWebMIdColorVolumeMetadata);
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
      VideoCodec::kVP9, profile, VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace::REC709(), kNoTransformation, kCodedSize,
      gfx::Rect(kCodedSize), kCodedSize, codec_private,
      EncryptionScheme::kUnencrypted);

  EXPECT_TRUE(config.Matches(expected_config))
      << "Config (" << config.AsHumanReadableString()
      << ") does not match expected ("
      << expected_config.AsHumanReadableString() << ")";
}

#if BUILDFLAG(ENABLE_AV1_DECODER)
TEST_F(WebMVideoClientTest, InitializeConfigAV1Profile) {
  const std::string codec_id = "V_AV1";
  const auto expected_profile = AV1PROFILE_PROFILE_HIGH;
  const std::vector<uint8_t> codec_private{0x81, 0x20, 0x00, 0x00, 0x0a, 0x0a,
                                           0x20, 0x00, 0x00, 0x03, 0xbf, 0x7f,
                                           0x7b, 0xff, 0xf3, 0x04};

  VideoDecoderConfig config;
  EXPECT_TRUE(webm_video_client_.InitializeConfig(codec_id, codec_private,
                                                  EncryptionScheme(), &config));

  VideoDecoderConfig expected_config(
      VideoCodec::kAV1, expected_profile,
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace::REC709(),
      kNoTransformation, kCodedSize, gfx::Rect(kCodedSize), kCodedSize,
      codec_private, EncryptionScheme::kUnencrypted);

  EXPECT_TRUE(config.Matches(expected_config))
      << "Config (" << config.AsHumanReadableString()
      << ") does not match expected ("
      << expected_config.AsHumanReadableString() << ")";
}
#endif

TEST_F(WebMVideoClientTest, InvalidStereoMode) {
  EXPECT_MEDIA_LOG(UnexpectedStereoMode());
  OnUInt(kWebMIdStereoMode, 15);
}

TEST_F(WebMVideoClientTest, MultipleStereoMode) {
  OnUInt(kWebMIdStereoMode, 1);
  EXPECT_MEDIA_LOG(UnexpectedMultipleValues());
  OnUInt(kWebMIdStereoMode, 1);
}

TEST_F(WebMVideoClientTest, VerifyTransformationFromProjection) {
  const auto perform_projection_test =
      [&](double roll, double yaw,
          media::VideoTransformation expected_transformation) {
        SCOPED_TRACE(
            testing::Message()
            << "roll: " << roll << ", yaw: " << yaw
            << ", expected_rotation: " << expected_transformation.rotation
            << ", expected_mirrored: " << expected_transformation.mirrored);

        webm_video_client_.Reset();
        OnUInt(kWebMIdPixelWidth, kCodedSize.width());
        OnUInt(kWebMIdPixelHeight, kCodedSize.height());

        WebMParserClient* projection_parser_client =
            OnListStart(kWebMIdProjection);
        ASSERT_NE(projection_parser_client, nullptr);
        ASSERT_TRUE(projection_parser_client->OnUInt(kWebMIdProjectionType,
                                                     0));  // 0 for rectangular

        ASSERT_TRUE(
            projection_parser_client->OnFloat(kWebMIdProjectionPoseYaw, yaw));
        ASSERT_TRUE(
            projection_parser_client->OnFloat(kWebMIdProjectionPosePitch, 0.0));
        ASSERT_TRUE(
            projection_parser_client->OnFloat(kWebMIdProjectionPoseRoll, roll));
        OnListEnd(kWebMIdProjection);

        VideoDecoderConfig config;
        EXPECT_TRUE(webm_video_client_.InitializeConfig(
            "V_VP9", {}, EncryptionScheme::kUnencrypted, &config));

        EXPECT_EQ(config.video_transformation().rotation,
                  expected_transformation.rotation);
        EXPECT_EQ(config.video_transformation().mirrored,
                  expected_transformation.mirrored);
      };

  const auto verify_roll = [&](double roll_degrees,
                               VideoRotation expected_rotation_enum) {
    perform_projection_test(roll_degrees, /*yaw=*/0.0,
                            media::VideoTransformation(expected_rotation_enum));
  };

  // Test cases for roll values, checking snapping to 0, 90, 180, 270 degrees.
  // VIDEO_ROTATION_0
  verify_roll(0.0, VIDEO_ROTATION_0);
  verify_roll(44.9, VIDEO_ROTATION_0);
  verify_roll(-44.9, VIDEO_ROTATION_0);

  // VIDEO_ROTATION_90
  verify_roll(90.0, VIDEO_ROTATION_90);
  verify_roll(45.0, VIDEO_ROTATION_90);
  verify_roll(134.9, VIDEO_ROTATION_90);

  // VIDEO_ROTATION_180
  verify_roll(180.0, VIDEO_ROTATION_180);
  verify_roll(135.0, VIDEO_ROTATION_180);
  verify_roll(-180.0, VIDEO_ROTATION_180);

  // VIDEO_ROTATION_270
  verify_roll(-90.0, VIDEO_ROTATION_270);
  verify_roll(-45.1, VIDEO_ROTATION_270);

  const auto verify_yaw = [&](double yaw_degrees, bool expected_mirrored) {
    perform_projection_test(
        0.0, yaw_degrees,
        media::VideoTransformation(VIDEO_ROTATION_0, expected_mirrored));
  };

  // Yaw mirror threshold is 1.0. Mirrored if abs(abs(yaw) - 180) < 1.0.
  constexpr double kYawMirrorThreshold = 1.0;

  // Test cases for yaw values (mirroring)
  constexpr bool kMirrored = true;
  verify_yaw(180.0, kMirrored);
  verify_yaw(-180.0, kMirrored);
  verify_yaw(0.0, !kMirrored);
  verify_yaw(1.0, !kMirrored);
  verify_yaw(-1.0, !kMirrored);
  verify_yaw(180.0 - kYawMirrorThreshold, !kMirrored);
  verify_yaw(-180.0 + kYawMirrorThreshold, !kMirrored);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebMVideoClientTest,
                         ::testing::ValuesIn(kCodecTestParams));

}  // namespace media
