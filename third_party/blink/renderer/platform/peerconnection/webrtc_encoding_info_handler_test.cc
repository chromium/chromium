// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_encoding_info_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/webrtc/api/audio_codecs/audio_encoder_factory.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

class MockVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  // webrtc::VideoEncoderFactory implementation:
  MOCK_METHOD(std::unique_ptr<webrtc::VideoEncoder>,
              CreateVideoEncoder,
              (const webrtc::SdpVideoFormat& format),
              (override));
  MOCK_METHOD(std::vector<webrtc::SdpVideoFormat>,
              GetSupportedFormats,
              (),
              (const));
  MOCK_METHOD(webrtc::VideoEncoderFactory::CodecSupport,
              QueryCodecSupport,
              (const webrtc::SdpVideoFormat& format,
               absl::optional<std::string> scalability_mode),
              (const, override));
};

class MediaCapabilitiesEncodingInfoCallback {
 public:
  void OnWebrtcEncodingInfoSupport(bool is_supported, bool is_power_efficient) {
    is_success_ = true;
    is_supported_ = is_supported;
    is_power_efficient_ = is_power_efficient;
  }

  void OnError() { is_error_ = true; }

  bool IsCalled() const { return is_success_ || is_error_; }
  bool IsSuccess() const { return is_success_; }
  bool IsError() const { return is_error_; }
  bool IsSupported() const { return is_supported_; }
  bool IsPowerEfficient() const { return is_power_efficient_; }

 private:
  bool is_success_ = false;
  bool is_error_ = false;
  bool is_supported_ = false;
  bool is_power_efficient_ = false;
};

}  // namespace

typedef webrtc::VideoEncoderFactory::CodecSupport CodecSupport;

class WebrtcEncodingInfoHandlerTests : public ::testing::Test {
 public:
  WebrtcEncodingInfoHandlerTests()
      : mock_video_encoder_factory_(new MockVideoEncoderFactory()),
        video_encoder_factory_(mock_video_encoder_factory_),
        audio_encoder_factory_(blink::CreateWebrtcAudioEncoderFactory()) {}

  void SetUp() override {}

  void VerifyEncodingInfo(
      const absl::optional<String> audio_mime_type,
      const absl::optional<String> video_mime_type,
      const absl::optional<String> video_scalability_mode,
      const absl::optional<webrtc::SdpVideoFormat> expected_format,
      const CodecSupport support) {
    if (expected_format) {
      const absl::optional<std::string> expected_scalability_mode =
          video_scalability_mode
              ? absl::make_optional(video_scalability_mode->Utf8())
              : absl::nullopt;

      ON_CALL(*mock_video_encoder_factory_, QueryCodecSupport)
          .WillByDefault(testing::Invoke(
              [expected_format, expected_scalability_mode, support](
                  const webrtc::SdpVideoFormat& format,
                  absl::optional<std::string> scalability_mode) {
                EXPECT_TRUE(format.IsSameCodec(*expected_format));
                EXPECT_EQ(scalability_mode, expected_scalability_mode);
                return support;
              }));
      EXPECT_CALL(*mock_video_encoder_factory_, QueryCodecSupport)
          .Times(::testing::AtMost(1));
    }
    WebrtcEncodingInfoHandler encoding_info_handler(
        std::move(video_encoder_factory_), audio_encoder_factory_);
    MediaCapabilitiesEncodingInfoCallback encoding_info_callback;

    encoding_info_handler.EncodingInfo(
        audio_mime_type, video_mime_type, video_scalability_mode,
        base::BindOnce(
            &MediaCapabilitiesEncodingInfoCallback::OnWebrtcEncodingInfoSupport,
            base::Unretained(&encoding_info_callback)));

    EXPECT_TRUE(encoding_info_callback.IsCalled());
    EXPECT_TRUE(encoding_info_callback.IsSuccess());
    EXPECT_EQ(encoding_info_callback.IsSupported(), support.is_supported);
    EXPECT_EQ(encoding_info_callback.IsPowerEfficient(),
              support.is_power_efficient);
  }

 protected:
  std::vector<webrtc::AudioCodecSpec> kSupportedAudioCodecs;
  MockVideoEncoderFactory* mock_video_encoder_factory_;
  std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory_;
  rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory_;
};

TEST_F(WebrtcEncodingInfoHandlerTests, BasicAudio) {
  VerifyEncodingInfo(
      "audio/opus", /*video_mime_type=*/absl::nullopt,
      /*video_scalability_mode=*/absl::nullopt,
      /*expected_format=*/absl::nullopt,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/true});
}

TEST_F(WebrtcEncodingInfoHandlerTests, UnsupportedAudio) {
  VerifyEncodingInfo(
      "audio/foo", /*video_mime_type=*/absl::nullopt,
      /*video_scalability_mode=*/absl::nullopt,
      /*expected_format=*/absl::nullopt,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

// These tests verify that the video MIME type is correctly parsed into
// SdpVideoFormat and that the return value from
// VideoEncoderFactory::QueryCodecSupport is correctly returned through the
// callback.
TEST_F(WebrtcEncodingInfoHandlerTests, BasicVideo) {
  const webrtc::SdpVideoFormat kExpectedFormat("VP9");
  VerifyEncodingInfo(
      /*audio_mime_type=*/absl::nullopt, "video/VP9",
      /*video_scalability_mode=*/absl::nullopt, kExpectedFormat,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcEncodingInfoHandlerTests, BasicVideoPowerEfficient) {
  const webrtc::SdpVideoFormat kExpectedFormat("VP9");
  VerifyEncodingInfo(
      /*audio_mime_type=*/absl::nullopt, "video/VP9",
      /*video_scalability_mode=*/absl::nullopt, kExpectedFormat,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/true});
}

TEST_F(WebrtcEncodingInfoHandlerTests, UnsupportedVideo) {
  const webrtc::SdpVideoFormat kExpectedFormat(
      "VP9", webrtc::SdpVideoFormat::Parameters{{"profile-level", "5"}});
  VerifyEncodingInfo(
      /*audio_mime_type=*/absl::nullopt, "video/VP9; profile-level=5",
      /*video_scalability_mode=*/absl::nullopt, kExpectedFormat,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcEncodingInfoHandlerTests, VideoWithScalabilityMode) {
  const webrtc::SdpVideoFormat kExpectedFormat("VP9");
  VerifyEncodingInfo(
      /*audio_mime_type=*/absl::nullopt, "video/VP9", "L1T3", kExpectedFormat,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcEncodingInfoHandlerTests, SupportedAudioUnsupportedVideo) {
  const webrtc::SdpVideoFormat kExpectedFormat("foo");
  VerifyEncodingInfo(
      "audio/opus", "video/foo", /*video_scalability_mode=*/absl::nullopt,
      kExpectedFormat,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

TEST_F(WebrtcEncodingInfoHandlerTests, SupportedVideoUnsupportedAudio) {
  const webrtc::SdpVideoFormat kExpectedFormat("VP9");
  VerifyEncodingInfo(
      "audio/foo", "video/VP9", /*video_scalability_mode=*/absl::nullopt,
      kExpectedFormat,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

}  // namespace blink
