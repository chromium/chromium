// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/instrumented_video_encoder_wrapper.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/video_encoder_state_observer.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Return;

namespace blink {
namespace {
class MockVideoEncoderStateObserver : public VideoEncoderStateObserver {
 public:
  MockVideoEncoderStateObserver() = default;
  ~MockVideoEncoderStateObserver() override = default;

  MOCK_METHOD(void,
              OnEncoderCreated,
              (int, const webrtc::VideoCodec&),
              (override));
  MOCK_METHOD(void, OnEncoderDestroyed, (int), (override));
  MOCK_METHOD(void, OnRatesUpdated, (int, const Vector<bool>&), (override));
  MOCK_METHOD(void, OnEncode, (int, uint32_t rtp_timestamp), (override));
  MOCK_METHOD(void, OnEncodedImage, (int, const EncodeResult&), (override));
};

class MockEncodedImageCallback : public webrtc::EncodedImageCallback {
 public:
  MOCK_METHOD(webrtc::EncodedImageCallback::Result,
              OnEncodedImage,
              (const webrtc::EncodedImage&, const webrtc::CodecSpecificInfo*),
              (override));
  MOCK_METHOD(void,
              OnDroppedFrame,
              (webrtc::EncodedImageCallback::DropReason),
              (override));
};

class FakeVideoEncoder : public webrtc::VideoEncoder {
 public:
  FakeVideoEncoder() = default;
  ~FakeVideoEncoder() override = default;

  // webrtc::VideoEncoder implementations.
  void SetFecControllerOverride(
      webrtc::FecControllerOverride* fec_controller_override) override {}
  int InitEncode(const webrtc::VideoCodec* codec_settings,
                 const webrtc::VideoEncoder::Settings& settings) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }
  int32_t Encode(
      const webrtc::VideoFrame& frame,
      const std::vector<webrtc::VideoFrameType>* frame_types) override {
    if (callback_) {
      webrtc::EncodedImage encoded_image;
      encoded_image._encodedWidth = frame.width();
      encoded_image._encodedHeight = frame.height();
      encoded_image.SetRtpTimestamp(frame.rtp_timestamp());
      encoded_image._frameType = frame_types->at(0);
      callback_->OnEncodedImage(encoded_image,
                                /*codec_specific_info=*/nullptr);
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }
  void SetRates(const RateControlParameters& parameters) override {}
  void OnPacketLossRateUpdate(float packet_loss_rate) override {}
  void OnRttUpdate(int64_t rtt_ms) override {}
  void OnLossNotification(const LossNotification& loss_notification) override {}
  webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override {
    webrtc::VideoEncoder::EncoderInfo info;
    info.is_hardware_accelerated = false;
    return info;
  }

 private:
  raw_ptr<webrtc::EncodedImageCallback> callback_ = nullptr;
};

constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr uint64_t kTimestamp = 1000000;

webrtc::VideoFrame CreateFrame(int width = kWidth,
                               int height = kHeight,
                               uint64_t timestamp_us = kTimestamp) {
  auto frame = media::VideoFrame::CreateBlackFrame(gfx::Size(width, height));
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame, base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(
                     nullptr)));
  return webrtc::VideoFrame::Builder()
      .set_video_frame_buffer(std::move(frame_adapter))
      .set_rtp_timestamp(kTimestamp + 10)
      .set_timestamp_us(kTimestamp)
      .set_rotation(webrtc::kVideoRotation_0)
      .build();
}

webrtc::VideoCodec CreateVideoCodec(
    int width = kWidth,
    int height = kHeight,
    webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8) {
  webrtc::VideoCodec video_codec;
  video_codec.width = width;
  video_codec.height = height;
  video_codec.codecType = codec_type;
  video_codec.startBitrate = 12345;
  video_codec.maxFramerate = 30;
  video_codec.numberOfSimulcastStreams = 1;
  return video_codec;
}
}  // namespace

constexpr int kEncoderId = 10;
const webrtc::VideoCodec kVideoCodec = CreateVideoCodec();
const webrtc::VideoEncoder::Settings kEncoderSettings(
    webrtc::VideoEncoder::Capabilities(/*loss_notification=*/false),
    /*number_of_cores=*/1,
    /*max_payload_size=*/12345);

class InstrumentedVideoEncoderWrapperTest : public ::testing::Test {
 public:
  InstrumentedVideoEncoderWrapperTest() = default;
  ~InstrumentedVideoEncoderWrapperTest() override = default;
  void SetUp() override {
    mock_state_observer_ = std::make_unique<MockVideoEncoderStateObserver>();

    auto fake_encoder = std::make_unique<FakeVideoEncoder>();
    fake_encoder_ = fake_encoder.get();
    wrapper_ = std::make_unique<InstrumentedVideoEncoderWrapper>(
        /*id=*/kEncoderId, std::move(fake_encoder),
        static_cast<VideoEncoderStateObserver*>(mock_state_observer_.get()));
  }
  void TearDown() override {
    fake_encoder_ = nullptr;
    wrapper_.reset();
    mock_state_observer_.reset();
  }

 protected:
  using EncodeResult = VideoEncoderStateObserver::EncodeResult;

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<MockVideoEncoderStateObserver> mock_state_observer_;

  std::unique_ptr<InstrumentedVideoEncoderWrapper> wrapper_;
  raw_ptr<FakeVideoEncoder> fake_encoder_;
};

TEST_F(InstrumentedVideoEncoderWrapperTest, InitEncodeAndRelease) {
  EXPECT_CALL(*mock_state_observer_,
              OnEncoderCreated(kEncoderId, Field(&webrtc::VideoCodec::width,
                                                 kVideoCodec.width)));
  EXPECT_EQ(wrapper_->InitEncode(&kVideoCodec, kEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  EXPECT_CALL(*mock_state_observer_, OnEncoderDestroyed(kEncoderId));
  EXPECT_EQ(wrapper_->Release(), WEBRTC_VIDEO_CODEC_OK);
}

TEST_F(InstrumentedVideoEncoderWrapperTest, Encode) {
  EXPECT_EQ(wrapper_->InitEncode(&kVideoCodec, kEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);
  MockEncodedImageCallback encoded_image_callback;
  EXPECT_EQ(wrapper_->RegisterEncodeCompleteCallback(&encoded_image_callback),
            WEBRTC_VIDEO_CODEC_OK);
  auto frame = CreateFrame();
  std::vector<webrtc::VideoFrameType> frame_types{
      webrtc::VideoFrameType::kVideoFrameKey};
  EXPECT_CALL(*mock_state_observer_,
              OnEncode(kEncoderId, frame.rtp_timestamp()));
  EXPECT_CALL(
      *mock_state_observer_,
      OnEncodedImage(
          kEncoderId,
          AllOf(Field(&EncodeResult::width, kWidth),
                Field(&EncodeResult::height, kHeight),
                Field(&EncodeResult::keyframe, true),
                Field(&EncodeResult::spatial_index, std::nullopt),
                Field(&EncodeResult::rtp_timestamp, frame.rtp_timestamp()),
                Field(&EncodeResult::is_hardware_accelerated, false))));
  EXPECT_CALL(encoded_image_callback, OnEncodedImage(_, _))
      .WillOnce(Return(webrtc::EncodedImageCallback::Result(
          webrtc::EncodedImageCallback::Result::OK)));
  wrapper_->Encode(frame, &frame_types);
}

TEST_F(InstrumentedVideoEncoderWrapperTest, SetRates) {
  EXPECT_EQ(wrapper_->InitEncode(&kVideoCodec, kEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  constexpr double kNewFrameRate = 10;
  Vector<bool> kExpectedActiveLayers{true};
  webrtc::VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 12346);

  EXPECT_CALL(*mock_state_observer_,
              OnRatesUpdated(kEncoderId, kExpectedActiveLayers));
  wrapper_->SetRates(webrtc::VideoEncoder::RateControlParameters(
      bitrate_allocation, kNewFrameRate));
}
}  // namespace blink
