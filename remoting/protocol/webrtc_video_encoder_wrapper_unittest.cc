// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_encoder_wrapper.h"

#include "base/test/task_environment.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

using testing::_;
using testing::Field;
using testing::NiceMock;
using testing::Return;
using webrtc::BasicDesktopFrame;
using webrtc::CodecSpecificInfo;
using webrtc::DesktopSize;
using webrtc::EncodedImage;
using webrtc::EncodedImageCallback;
using webrtc::kVideoCodecVP8;
using webrtc::kVideoCodecVP9;
using webrtc::SdpVideoFormat;
using webrtc::VideoCodec;
using webrtc::VideoEncoder;
using webrtc::VideoFrame;
using webrtc::VideoFrameType;

namespace remoting {
namespace protocol {

namespace {

const int kInputFrameWidth = 800;
const int kInputFrameHeight = 600;
const int kBitrateBps = 8000000;
const VideoEncoder::Capabilities kVideoEncoderCapabilities(
    /*loss_notification*/ false);
const VideoEncoder::Settings kVideoEncoderSettings(kVideoEncoderCapabilities,
                                                   /*number_of_cores*/ 1,
                                                   /*max_payload*/ 10000);
const EncodedImageCallback::Result kResultOk(EncodedImageCallback::Result::OK);

VideoEncoder::RateControlParameters DefaultRateControlParameters() {
  VideoEncoder::RateControlParameters params;
  params.bitrate.SetBitrate(0, 0, kBitrateBps);
  return params;
}

SdpVideoFormat GetVp8Format() {
  return SdpVideoFormat("VP8");
}

SdpVideoFormat GetVp9Format() {
  return SdpVideoFormat("VP9");
}

VideoCodec GetVp8Codec() {
  VideoCodec codec{};
  codec.width = kInputFrameWidth;
  codec.height = kInputFrameHeight;
  codec.codecType = kVideoCodecVP8;
  codec.numberOfSimulcastStreams = 1;
  return codec;
}

VideoCodec GetVp9Codec() {
  VideoCodec codec = GetVp8Codec();
  codec.codecType = kVideoCodecVP9;
  auto* vp9 = codec.VP9();
  vp9->numberOfSpatialLayers = 1;
  return codec;
}

VideoFrame MakeVideoFrame() {
  DesktopSize size(kInputFrameWidth, kInputFrameHeight);
  auto frame = std::make_unique<BasicDesktopFrame>(size);
  frame->mutable_updated_region()->SetRect(webrtc::DesktopRect::MakeSize(size));
  return WebrtcVideoFrameAdapter::CreateVideoFrame(std::move(frame));
}

class MockVideoChannelStateObserver : public VideoChannelStateObserver {
 public:
  MockVideoChannelStateObserver() = default;
  ~MockVideoChannelStateObserver() override = default;

  MOCK_METHOD(void, OnEncoderReady, (), (override));
  MOCK_METHOD(void, OnKeyFrameRequested, (), (override));
  MOCK_METHOD(void, OnTargetBitrateChanged, (int bitrate_kbps), (override));
  MOCK_METHOD(void, OnRttUpdate, (base::TimeDelta rtt), (override));
  MOCK_METHOD(void, OnTopOffActive, (bool active), (override));
  MOCK_METHOD(void,
              OnFrameEncoded,
              (WebrtcVideoEncoder::EncodeResult encode_result,
               WebrtcVideoEncoder::EncodedFrame* frame),
              (override));
  MOCK_METHOD(void,
              OnEncodedFrameSent,
              (EncodedImageCallback::Result result,
               const WebrtcVideoEncoder::EncodedFrame& frame),
              (override));

  base::WeakPtr<MockVideoChannelStateObserver> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  base::WeakPtrFactory<MockVideoChannelStateObserver> weak_factory_{this};
};

class MockEncodedImageCallback : public EncodedImageCallback {
 public:
  MockEncodedImageCallback() = default;
  ~MockEncodedImageCallback() override = default;

  MOCK_METHOD(Result,
              OnEncodedImage,
              (const EncodedImage& encoded_image,
               const CodecSpecificInfo* codec_specific_info),
              (override));
};

}  // namespace

class WebrtcVideoEncoderWrapperTest : public testing::Test {
 public:
  std::unique_ptr<WebrtcVideoEncoderWrapper> InitEncoder(SdpVideoFormat sdp,
                                                         VideoCodec codec) {
    auto encoder = std::make_unique<WebrtcVideoEncoderWrapper>(
        sdp, task_environment_.GetMainThreadTaskRunner(),
        observer_.GetWeakPtr());
    encoder->InitEncode(&codec, kVideoEncoderSettings);
    encoder->RegisterEncodeCompleteCallback(&callback_);
    encoder->SetRates(DefaultRateControlParameters());
    return encoder;
  }

  void PostQuitAndRun() {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop_.QuitWhenIdleClosure());
    run_loop_.Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{};

  base::RunLoop run_loop_;

  NiceMock<MockVideoChannelStateObserver> observer_;
  MockEncodedImageCallback callback_;
};

TEST_F(WebrtcVideoEncoderWrapperTest, ReturnsVP8EncodedFrames) {
  EXPECT_CALL(callback_, OnEncodedImage(_, Field(&CodecSpecificInfo::codecType,
                                                 kVideoCodecVP8)))
      .WillOnce(Return(kResultOk));

  auto encoder = InitEncoder(GetVp8Format(), GetVp8Codec());
  std::vector<VideoFrameType> frame_types;
  encoder->Encode(MakeVideoFrame(), &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, ReturnsVP9EncodedFrames) {
  EXPECT_CALL(callback_, OnEncodedImage(_, Field(&CodecSpecificInfo::codecType,
                                                 kVideoCodecVP9)))
      .WillOnce(Return(kResultOk));

  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  std::vector<VideoFrameType> frame_types;
  encoder->Encode(MakeVideoFrame(), &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, NotifiesOnBitrateChanged) {
  EXPECT_CALL(observer_, OnTargetBitrateChanged(kBitrateBps / 1000));

  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, NotifiesOnRttUpdate) {
  EXPECT_CALL(observer_, OnRttUpdate(base::TimeDelta::FromMilliseconds(123)));

  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  encoder->OnRttUpdate(123);
  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, NotifiesFrameEncodedAndReturned) {
  EXPECT_CALL(callback_, OnEncodedImage(_, Field(&CodecSpecificInfo::codecType,
                                                 kVideoCodecVP9)))
      .WillOnce(Return(kResultOk));
  EXPECT_CALL(observer_,
              OnFrameEncoded(WebrtcVideoEncoder::EncodeResult::SUCCEEDED, _));
  EXPECT_CALL(observer_,
              OnEncodedFrameSent(Field(&EncodedImageCallback::Result::error,
                                       EncodedImageCallback::Result::OK),
                                 _));

  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  std::vector<VideoFrameType> frame_types;
  frame_types.push_back(VideoFrameType::kVideoFrameKey);
  encoder->Encode(MakeVideoFrame(), &frame_types);

  PostQuitAndRun();
}

}  // namespace protocol
}  // namespace remoting
