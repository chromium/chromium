// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_decoding_info_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/webrtc/api/audio_codecs/audio_decoder_factory.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_decoder.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

class MockVideoDecoderFactory : public webrtc::VideoDecoderFactory {
 public:
  // webrtc::VideoDecoderFactory implementation:
  MOCK_METHOD(std::unique_ptr<webrtc::VideoDecoder>,
              CreateVideoDecoder,
              (const webrtc::SdpVideoFormat& format),
              (override));
  MOCK_METHOD(std::vector<webrtc::SdpVideoFormat>,
              GetSupportedFormats,
              (),
              (const));
  MOCK_METHOD(webrtc::VideoDecoderFactory::CodecSupport,
              QueryCodecSupport,
              (const webrtc::SdpVideoFormat& format, bool reference_scaling),
              (const, override));
};

class MediaCapabilitiesDecodingInfoCallback {
 public:
  void OnWebrtcDecodingInfoSupport(bool is_supported, bool is_power_efficient) {
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

typedef webrtc::VideoDecoderFactory::CodecSupport CodecSupport;

class WebrtcDecodingInfoHandlerTests : public ::testing::Test {
 public:
  WebrtcDecodingInfoHandlerTests()
      : mock_video_decoder_factory_(new MockVideoDecoderFactory()),
        video_decoder_factory_(mock_video_decoder_factory_),
        audio_decoder_factory_(blink::CreateWebrtcAudioDecoderFactory()) {}

  void SetUp() override {}

  void VerifyDecodingInfo(
      const absl::optional<String> audio_mime_type,
      const absl::optional<String> video_mime_type,
      const absl::optional<String> video_scalability_mode,
      const absl::optional<webrtc::SdpVideoFormat> expected_format,
      const bool expected_reference_scaling,
      const CodecSupport support) {
    if (expected_format) {
      ON_CALL(*mock_video_decoder_factory_, QueryCodecSupport)
          .WillByDefault(
              testing::Invoke([expected_format, expected_reference_scaling,
                               support](const webrtc::SdpVideoFormat& format,
                                        bool reference_scaling) {
                EXPECT_TRUE(format.IsSameCodec(*expected_format));
                EXPECT_EQ(reference_scaling, expected_reference_scaling);
                return support;
              }));
      EXPECT_CALL(*mock_video_decoder_factory_, QueryCodecSupport)
          .Times(::testing::AtMost(1));
    }
    WebrtcDecodingInfoHandler decoding_info_handler(
        std::move(video_decoder_factory_), audio_decoder_factory_);
    MediaCapabilitiesDecodingInfoCallback decoding_info_callback;

    decoding_info_handler.DecodingInfo(
        audio_mime_type, video_mime_type, video_scalability_mode,
        base::BindOnce(
            &MediaCapabilitiesDecodingInfoCallback::OnWebrtcDecodingInfoSupport,
            base::Unretained(&decoding_info_callback)));

    EXPECT_TRUE(decoding_info_callback.IsCalled());
    EXPECT_TRUE(decoding_info_callback.IsSuccess());
    EXPECT_EQ(decoding_info_callback.IsSupported(), support.is_supported);
    EXPECT_EQ(decoding_info_callback.IsPowerEfficient(),
              support.is_power_efficient);
  }

 protected:
  std::vector<webrtc::AudioCodecSpec> kSupportedAudioCodecs;
  MockVideoDecoderFactory* mock_video_decoder_factory_;
  std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory_;
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory_;
};

TEST_F(WebrtcDecodingInfoHandlerTests, BasicAudio) {
  VerifyDecodingInfo(
      "audio/opus", /*video_mime_type=*/absl::nullopt,
      /*video_scalability_mode=*/absl::nullopt,
      /*expected_format=*/absl::nullopt, /*expected_reference_scaling=*/false,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/true});
}

TEST_F(WebrtcDecodingInfoHandlerTests, UnsupportedAudio) {
  VerifyDecodingInfo(
      "audio/foo", /*video_mime_type=*/absl::nullopt,
      /*video_scalability_mode=*/absl::nullopt,
      /*expected_format=*/absl::nullopt, /*expected_reference_scaling=*/false,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

// These tests verify that the video MIME type is correctly parsed into
// SdpVideoFormat and that the return value from
// VideoDecoderFactory::QueryCodecSupport is correctly returned through the
// callback.
TEST_F(WebrtcDecodingInfoHandlerTests, BasicVideo) {
  const webrtc::SdpVideoFormat kExpectedFormat("VP9");
  VerifyDecodingInfo(
      /*audio_mime_type=*/absl::nullopt, "video/VP9",
      /*video_scalability_mode=*/absl::nullopt, kExpectedFormat,
      /*expected_reference_scaling=*/false,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcDecodingInfoHandlerTests, BasicVideoPowerEfficient) {
  const webrtc::SdpVideoFormat kExpectedFormat("VP9");
  VerifyDecodingInfo(
      /*audio_mime_type=*/absl::nullopt, "video/VP9",
      /*video_scalability_mode=*/absl::nullopt, kExpectedFormat,
      /*expected_reference_scaling=*/false,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/true});
}

TEST_F(WebrtcDecodingInfoHandlerTests, UnsupportedVideo) {
  const webrtc::SdpVideoFormat kExpectedFormat(
      "VP9", webrtc::SdpVideoFormat::Parameters{{"profile-level", "5"}});
  VerifyDecodingInfo(
      /*audio_mime_type=*/absl::nullopt, "video/VP9; profile-level=5",
      /*video_scalability_mode=*/absl::nullopt, kExpectedFormat,
      /*expected_reference_scaling=*/false,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcDecodingInfoHandlerTests, VideoWithReferenceScaling) {
  const webrtc::SdpVideoFormat kExpectedFormat("VP9");
  VerifyDecodingInfo(
      /*audio_mime_type=*/absl::nullopt, "video/VP9", "L3T3", kExpectedFormat,
      /*expected_reference_scaling=*/true,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcDecodingInfoHandlerTests, SupportedAudioUnsupportedVideo) {
  const webrtc::SdpVideoFormat kExpectedFormat("foo");
  VerifyDecodingInfo(
      "audio/opus", "video/foo", /*video_scalability_mode=*/absl::nullopt,
      kExpectedFormat, /*expected_reference_scaling=*/false,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

TEST_F(WebrtcDecodingInfoHandlerTests, SupportedVideoUnsupportedAudio) {
  const webrtc::SdpVideoFormat kExpectedFormat("VP9");
  VerifyDecodingInfo(
      "audio/foo", "video/VP9", /*video_scalability_mode=*/absl::nullopt,
      kExpectedFormat, /*expected_reference_scaling=*/false,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

}  // namespace blink
