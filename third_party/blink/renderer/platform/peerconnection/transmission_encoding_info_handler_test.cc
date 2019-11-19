// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/transmission_encoding_info_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "media/base/video_codecs.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_audio_configuration.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_configuration.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_video_configuration.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

class FakeVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  FakeVideoEncoderFactory() = default;
  ~FakeVideoEncoderFactory() override = default;

  void AddSupportedFormat(const webrtc::SdpVideoFormat& video_format,
                          bool is_hardware_accelerated = false,
                          bool has_internal_source = false) {
    supported_video_formats_.push_back(video_format);
    codec_info_.push_back({.is_hardware_accelerated = is_hardware_accelerated,
                           .has_internal_source = has_internal_source});
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    return supported_video_formats_;
  }

  CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override {
    DCHECK_EQ(supported_video_formats_.size(), codec_info_.size());
    for (size_t i = 0; i < supported_video_formats_.size(); ++i) {
      if (supported_video_formats_[i] == format)
        return codec_info_[i];
    }
    NOTREACHED() << "QueryVideoEncoder() assumes |format| is supported.";
    return CodecInfo();
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override {
    return std::unique_ptr<webrtc::VideoEncoder>();
  }

 private:
  std::vector<webrtc::SdpVideoFormat> supported_video_formats_;
  std::vector<CodecInfo> codec_info_;
};

}  // namespace

// Stores WebMediaCapabilitiesEncodingInfoCallbacks' result for verify.
class EncodingInfoObserver {
 public:
  EncodingInfoObserver() = default;
  ~EncodingInfoObserver() = default;

  void OnSuccess(std::unique_ptr<blink::WebMediaCapabilitiesInfo> info) {
    info_.swap(info);
    is_success_ = true;
  }
  void OnError() { is_error_ = true; }

  const blink::WebMediaCapabilitiesInfo* info() const { return info_.get(); }
  bool IsCalled() const { return is_success_ || is_error_; }
  bool is_success() const { return is_success_; }
  bool is_error() const { return is_error_; }

 private:
  std::unique_ptr<blink::WebMediaCapabilitiesInfo> info_;
  bool is_success_;
  bool is_error_;
};

// It places callback's result to EncodingInfoObserver for testing code
// to verify. Because blink::WebMediaCapabilitiesEncodingInfoCallbacks instance
// is handed to TransmissionEncodingInfoHandler, we cannot directly inspect
// OnSuccess() received argument. So it moves OnSuccess()'s received argument,
// WebMediaCapabilitiesInfo instance, to EncodingInfoObserver instance for
// inspection.
class WebMediaCapabilitiesEncodingInfoCallbacksForTest {
 public:
  WebMediaCapabilitiesEncodingInfoCallbacksForTest(
      EncodingInfoObserver* observer)
      : observer_(observer) {
    DCHECK(observer_);
  }
  virtual ~WebMediaCapabilitiesEncodingInfoCallbacksForTest() = default;

  void OnSuccess(std::unique_ptr<blink::WebMediaCapabilitiesInfo> info) {
    observer_->OnSuccess(std::move(info));
  }

  void OnError() { observer_->OnError(); }

 private:
  EncodingInfoObserver* observer_;
};

class TransmissionEncodingInfoHandlerTest : public testing::Test {
 protected:
  blink::WebVideoConfiguration ComposeVideoConfiguration(
      const std::string& mime_type,
      const std::string& codec,
      unsigned int width = 1920,
      unsigned int height = 1080,
      double framerate = 30.0) {
    constexpr int kBitrate = 2661034;
    return blink::WebVideoConfiguration{blink::WebString::FromASCII(mime_type),
                                        blink::WebString::FromASCII(codec),
                                        width,
                                        height,
                                        kBitrate,
                                        framerate};
  }

  blink::WebAudioConfiguration ComposeAudioConfiguration(
      const std::string& mime_type,
      const std::string& codec) {
    return blink::WebAudioConfiguration{blink::WebString::FromASCII(mime_type),
                                        blink::WebString::FromASCII(codec),
                                        blink::WebString(), base::nullopt,
                                        base::nullopt};
  }

  blink::WebMediaConfiguration ComposeWebMediaConfigurationForVideo(
      const std::string& mime_type,
      const std::string& codec) {
    return blink::WebMediaConfiguration(
        blink::MediaConfigurationType::kTransmission, base::nullopt,
        ComposeVideoConfiguration(mime_type, codec));
  }

  blink::WebMediaConfiguration ComposeWebMediaConfigurationForAudio(
      const std::string& mime_type,
      const std::string& codec) {
    return blink::WebMediaConfiguration(
        blink::MediaConfigurationType::kTransmission,
        ComposeAudioConfiguration(mime_type, codec), base::nullopt);
  }

  void VerifyEncodingInfo(const TransmissionEncodingInfoHandler& handler,
                          const blink::WebMediaConfiguration& configuration,
                          bool expect_supported,
                          bool expect_smooth,
                          bool expect_power_efficient) {
    EncodingInfoObserver observer;
    auto callbacks =
        std::make_unique<WebMediaCapabilitiesEncodingInfoCallbacksForTest>(
            &observer);
    handler.EncodingInfo(
        configuration,
        base::BindOnce(
            &WebMediaCapabilitiesEncodingInfoCallbacksForTest::OnSuccess,
            base::Unretained(callbacks.get())));

    EXPECT_TRUE(observer.IsCalled());
    EXPECT_TRUE(observer.is_success());
    const blink::WebMediaCapabilitiesInfo* encoding_info = observer.info();
    ASSERT_TRUE(encoding_info);
    EXPECT_EQ(expect_supported, encoding_info->supported);
    EXPECT_EQ(expect_smooth, encoding_info->smooth);
    EXPECT_EQ(expect_power_efficient, encoding_info->power_efficient);
  }
};

TEST_F(TransmissionEncodingInfoHandlerTest, SupportedVideoCodec) {
  auto video_encoder_factory = std::make_unique<FakeVideoEncoderFactory>();
  video_encoder_factory->AddSupportedFormat(webrtc::SdpVideoFormat("vp8"),
                                            false);
  TransmissionEncodingInfoHandler handler(std::move(video_encoder_factory),
                                          false);
  VerifyEncodingInfo(handler,
                     ComposeWebMediaConfigurationForVideo("video/vp8", ""),
                     true, false, false);
  // Temporarily unsupported: "video/vp9" and "video/h264".
  // TODO(crbug.com/941320): "video/vp9" and "video/h264" should be supported
  // once their MIME type parser are implemented.
  VerifyEncodingInfo(handler,
                     ComposeWebMediaConfigurationForVideo("video/vp9", ""),
                     false, false, false);
  VerifyEncodingInfo(handler,
                     ComposeWebMediaConfigurationForVideo("video/h264", ""),
                     false, false, false);
  // "video/webm" is not a "transmission" MIME type.
  VerifyEncodingInfo(handler,
                     ComposeWebMediaConfigurationForVideo("video/webm", "vp8"),
                     false, false, false);
}

TEST_F(TransmissionEncodingInfoHandlerTest, SupportedAudioCodec) {
  TransmissionEncodingInfoHandler handler;
  for (const char* mime_type :
       {"audio/g722", "audio/isac", "audio/opus", "audio/pcma", "audio/pcmu"}) {
    // For audio codec, if it is supported, it is smooth.
    VerifyEncodingInfo(handler,
                       ComposeWebMediaConfigurationForAudio(mime_type, ""),
                       true, true, true);
  }
}

TEST_F(TransmissionEncodingInfoHandlerTest, HardwareAcceleratedVideoCodec) {
  auto video_encoder_factory = std::make_unique<FakeVideoEncoderFactory>();
  video_encoder_factory->AddSupportedFormat(webrtc::SdpVideoFormat("vp8"),
                                            true);
  TransmissionEncodingInfoHandler handler(std::move(video_encoder_factory),
                                          false);
  VerifyEncodingInfo(handler,
                     ComposeWebMediaConfigurationForVideo("video/vp8", ""),
                     true, true, true);
}

TEST_F(TransmissionEncodingInfoHandlerTest, SmoothVideoCodecPowerfulCpu) {
  // Assume no HW vp8 encoder.
  auto video_encoder_factory = std::make_unique<FakeVideoEncoderFactory>();
  video_encoder_factory->AddSupportedFormat(webrtc::SdpVideoFormat("vp8"),
                                            false);
  // Assume powerful CPU.
  TransmissionEncodingInfoHandler handler(std::move(video_encoder_factory),
                                          true);
  VerifyEncodingInfo(handler,
                     ComposeWebMediaConfigurationForVideo("video/vp8", ""),
                     true, true, false);
}

TEST_F(TransmissionEncodingInfoHandlerTest, SmoothVideoCodecVgaResolution) {
  // Assume no HW vp8 encoder.
  auto video_encoder_factory = std::make_unique<FakeVideoEncoderFactory>();
  video_encoder_factory->AddSupportedFormat(webrtc::SdpVideoFormat("vp8"),
                                            false);
  // Assume no powerful CPU.
  TransmissionEncodingInfoHandler handler(std::move(video_encoder_factory),
                                          false);

  // VP8 encoding for 640x480 video.
  blink::WebMediaConfiguration config(
      blink::MediaConfigurationType::kTransmission, base::nullopt,
      ComposeVideoConfiguration("video/vp8", "", 640, 480));

  VerifyEncodingInfo(handler, config, true, true, false);
}

TEST_F(TransmissionEncodingInfoHandlerTest, SmoothVideoCodecBelowHdResolution) {
  // Assume no HW vp8 encoder.
  auto video_encoder_factory = std::make_unique<FakeVideoEncoderFactory>();
  video_encoder_factory->AddSupportedFormat(webrtc::SdpVideoFormat("vp8"),
                                            false);
  // Assume no powerful CPU.
  TransmissionEncodingInfoHandler handler(std::move(video_encoder_factory),
                                          false);

  // VP8 encoding for 1024x768 video. Note its area size is below 1280x720).
  blink::WebMediaConfiguration config(
      blink::MediaConfigurationType::kTransmission, base::nullopt,
      ComposeVideoConfiguration("video/vp8", "", 1024, 768));

  VerifyEncodingInfo(handler, config, true, true, false);
}

TEST_F(TransmissionEncodingInfoHandlerTest, AudioAndVideoCodec) {
  // Both video/vp8 and audio/opus are given.
  blink::WebMediaConfiguration config(
      blink::MediaConfigurationType::kTransmission,
      ComposeAudioConfiguration("audio/opus", ""),
      ComposeVideoConfiguration("video/vp8", ""));

  auto video_encoder_factory = std::make_unique<FakeVideoEncoderFactory>();
  video_encoder_factory->AddSupportedFormat(webrtc::SdpVideoFormat("vp8"),
                                            false);
  TransmissionEncodingInfoHandler handler(std::move(video_encoder_factory),
                                          false);
  VerifyEncodingInfo(handler, config, true, false, false);
}

TEST_F(TransmissionEncodingInfoHandlerTest,
       AudioAndVideoCodecWithVideoHardwareEncoder) {
  // Both video/vp8 and audio/opus are given.
  blink::WebMediaConfiguration config(
      blink::MediaConfigurationType::kTransmission,
      ComposeAudioConfiguration("audio/opus", ""),
      ComposeVideoConfiguration("video/vp8", ""));

  auto video_encoder_factory = std::make_unique<FakeVideoEncoderFactory>();
  video_encoder_factory->AddSupportedFormat(webrtc::SdpVideoFormat("vp8"),
                                            true);
  TransmissionEncodingInfoHandler handler(std::move(video_encoder_factory),
                                          false);
  VerifyEncodingInfo(handler, config, true, true, true);
}

TEST_F(TransmissionEncodingInfoHandlerTest, AudioAndVideoCodecWithPowerfulCpu) {
  // Both video/vp8 and audio/opus are given.
  blink::WebMediaConfiguration config(
      blink::MediaConfigurationType::kTransmission,
      ComposeAudioConfiguration("audio/opus", ""),
      ComposeVideoConfiguration("video/vp8", ""));

  // Assume no HW vp8 encoder.
  auto video_encoder_factory = std::make_unique<FakeVideoEncoderFactory>();
  video_encoder_factory->AddSupportedFormat(webrtc::SdpVideoFormat("vp8"),
                                            false);
  // Assume powerful CPU.
  TransmissionEncodingInfoHandler handler(std::move(video_encoder_factory),
                                          true);
  VerifyEncodingInfo(handler, config, true, true, false);
}

}  // namespace blink
