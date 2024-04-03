// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_decoding_info_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/webrtc/api/audio_codecs/audio_decoder_factory.h"
#include "third_party/webrtc/api/audio_codecs/audio_format.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_decoder.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {
const webrtc::SdpVideoFormat kVideoFormatVp9{"VP9"};
const webrtc::SdpVideoFormat kVideoFormatFoo{"Foo"};

const webrtc::SdpAudioFormat kAudioFormatOpus{"opus", /*clockrate_hz=*/8000,
                                              /*num_channels=*/1};
const webrtc::SdpAudioFormat kAudioFormatFoo{"Foo", /*clockrate_hz=*/8000,
                                             /*num_channels=*/1};

class MockVideoDecoderFactory : public webrtc::VideoDecoderFactory {
 public:
  // webrtc::VideoDecoderFactory implementation:
  MOCK_METHOD(std::unique_ptr<webrtc::VideoDecoder>,
              Create,
              (const webrtc::Environment&,
               const webrtc::SdpVideoFormat& format),
              (override));
  MOCK_METHOD(std::vector<webrtc::SdpVideoFormat>,
              GetSupportedFormats,
              (),
              (const));
  MOCK_METHOD(webrtc::VideoDecoderFactory::CodecSupport,
              QueryCodecSupport,
              (const webrtc::SdpVideoFormat& format, bool spatial_scalability),
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

using CodecSupport = webrtc::VideoDecoderFactory::CodecSupport;

class WebrtcDecodingInfoHandlerTests : public ::testing::Test {
 public:
  void VerifyDecodingInfo(
      const std::optional<webrtc::SdpAudioFormat> sdp_audio_format,
      const std::optional<webrtc::SdpVideoFormat> sdp_video_format,
      const bool video_spatial_scalability,
      const CodecSupport support) {
    auto video_decoder_factory = std::make_unique<MockVideoDecoderFactory>();
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory =
        blink::CreateWebrtcAudioDecoderFactory();
    if (sdp_video_format) {
      ON_CALL(*video_decoder_factory, QueryCodecSupport)
          .WillByDefault(
              testing::Invoke([sdp_video_format, video_spatial_scalability,
                               support](const webrtc::SdpVideoFormat& format,
                                        bool spatial_scalability) {
                EXPECT_TRUE(format.IsSameCodec(*sdp_video_format));
                EXPECT_EQ(spatial_scalability, video_spatial_scalability);
                return support;
              }));
      EXPECT_CALL(*video_decoder_factory, QueryCodecSupport)
          .Times(::testing::AtMost(1));
    }
    WebrtcDecodingInfoHandler decoding_info_handler(
        std::move(video_decoder_factory), audio_decoder_factory);
    MediaCapabilitiesDecodingInfoCallback decoding_info_callback;

    decoding_info_handler.DecodingInfo(
        sdp_audio_format, sdp_video_format, video_spatial_scalability,
        base::BindOnce(
            &MediaCapabilitiesDecodingInfoCallback::OnWebrtcDecodingInfoSupport,
            base::Unretained(&decoding_info_callback)));

    EXPECT_TRUE(decoding_info_callback.IsCalled());
    EXPECT_TRUE(decoding_info_callback.IsSuccess());
    EXPECT_EQ(decoding_info_callback.IsSupported(), support.is_supported);
    EXPECT_EQ(decoding_info_callback.IsPowerEfficient(),
              support.is_power_efficient);
  }
};

TEST_F(WebrtcDecodingInfoHandlerTests, BasicAudio) {
  VerifyDecodingInfo(
      kAudioFormatOpus, /*sdp_video_format=*/std::nullopt,
      /*video_spatial_scalability=*/false,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/true});
}

TEST_F(WebrtcDecodingInfoHandlerTests, UnsupportedAudio) {
  VerifyDecodingInfo(
      kAudioFormatFoo, /*sdp_video_format=*/std::nullopt,
      /*video_spatial_scalability=*/false,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

// These tests verify that the video MIME type is correctly parsed into
// SdpVideoFormat and that the return value from
// VideoDecoderFactory::QueryCodecSupport is correctly returned through the
// callback.
TEST_F(WebrtcDecodingInfoHandlerTests, BasicVideo) {
  VerifyDecodingInfo(
      /*sdp _audio_format=*/std::nullopt, kVideoFormatVp9,
      /*video_spatial_scalability=*/false,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcDecodingInfoHandlerTests, BasicVideoPowerEfficient) {
  VerifyDecodingInfo(
      /*sdp _audio_format=*/std::nullopt, kVideoFormatVp9,
      /*video_spatial_scalability=*/false,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/true});
}

TEST_F(WebrtcDecodingInfoHandlerTests, UnsupportedVideo) {
  VerifyDecodingInfo(
      /*sdp _audio_format=*/std::nullopt, kVideoFormatFoo,
      /*video_spatial_scalability=*/false,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcDecodingInfoHandlerTests, VideoWithReferenceScaling) {
  VerifyDecodingInfo(
      /*sdp _audio_format=*/std::nullopt, kVideoFormatVp9,
      /*video_spatial_scalability=*/true,
      CodecSupport{/*is_supported=*/true, /*is_power_efficient=*/false});
}

TEST_F(WebrtcDecodingInfoHandlerTests, SupportedAudioUnsupportedVideo) {
  VerifyDecodingInfo(
      kAudioFormatOpus, kVideoFormatFoo,
      /*video_spatial_scalability=*/false,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

TEST_F(WebrtcDecodingInfoHandlerTests, SupportedVideoUnsupportedAudio) {
  VerifyDecodingInfo(
      kAudioFormatFoo, kVideoFormatVp9,
      /*video_spatial_scalability=*/false,
      CodecSupport{/*is_supported=*/false, /*is_power_efficient=*/false});
}

}  // namespace blink
