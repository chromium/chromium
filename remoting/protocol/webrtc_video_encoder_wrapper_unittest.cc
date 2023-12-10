// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_encoder_wrapper.h"

#include "base/test/task_environment.h"
#include "remoting/base/session_options.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/video_stream_event_router.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

using testing::_;
using testing::Field;
using testing::InSequence;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::StrictMock;
using webrtc::BasicDesktopFrame;
using webrtc::CodecSpecificInfo;
using webrtc::DesktopRect;
using webrtc::DesktopRegion;
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

namespace remoting::protocol {

namespace {

constexpr int kInputFrameWidth = 800;
constexpr int kInputFrameHeight = 600;
constexpr int kBitrateBps = 8000000;
constexpr int kTestScreenId = 16;

// Used for a +/- 5ms fudge factor when checking frame durations.
constexpr int kDurationMsFudgeFactor = 5;

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
  auto stats = std::make_unique<WebrtcVideoEncoder::FrameStats>();
  stats->screen_id = kTestScreenId;
  frame->mutable_updated_region()->SetRect(webrtc::DesktopRect::MakeSize(size));
  return WebrtcVideoFrameAdapter::CreateVideoFrame(std::move(frame),
                                                   std::move(stats));
}

VideoFrame MakeEmptyVideoFrame() {
  DesktopSize size(kInputFrameWidth, kInputFrameHeight);
  auto frame = std::make_unique<BasicDesktopFrame>(size);
  auto stats = std::make_unique<WebrtcVideoEncoder::FrameStats>();
  stats->screen_id = kTestScreenId;
  return WebrtcVideoFrameAdapter::CreateVideoFrame(std::move(frame),
                                                   std::move(stats));
}

// DesktopFrame arg, DesktopRect expected_update_rect
MATCHER_P(MatchesUpdateRect, expected_update_rect, "") {
  return arg.updated_region().Equals(DesktopRegion(expected_update_rect));
}

// Matcher which is true if |arg| (of type WebrtcVideoEncoder::FrameParams) is
// a key-frame request.
MATCHER(IsKeyFrame, "") {
  return arg.key_frame;
}

class MockVideoChannelStateObserver : public VideoChannelStateObserver {
 public:
  MockVideoChannelStateObserver() = default;
  ~MockVideoChannelStateObserver() override = default;

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

class MockVideoEncoder : public WebrtcVideoEncoder {
 public:
  MockVideoEncoder() = default;
  ~MockVideoEncoder() override = default;

  MOCK_METHOD(void, SetLosslessColor, (bool want_lossless), (override));
  MOCK_METHOD(void,
              Encode,
              (std::unique_ptr<webrtc::DesktopFrame> frame,
               const FrameParams& frame_params,
               EncodeCallback done),
              (override));
};

}  // namespace

class WebrtcVideoEncoderWrapperTest : public testing::Test {
 public:
  void SetUp() override {
    // Configure the mock encoder's default behavior to mimic a real encoder.
    mock_video_encoder_ = std::make_unique<NiceMock<MockVideoEncoder>>();
    ON_CALL(*mock_video_encoder_, Encode)
        .WillByDefault([&](std::unique_ptr<webrtc::DesktopFrame> frame,
                           const WebrtcVideoEncoder::FrameParams& frame_params,
                           WebrtcVideoEncoder::EncodeCallback done) {
          auto encoded_frame =
              std::make_unique<WebrtcVideoEncoder::EncodedFrame>();
          encoded_frame->dimensions = frame->size();
          encoded_frame->data = webrtc::EncodedImageBuffer::Create(
              frame->size().width() * frame->size().height());
          encoded_frame->key_frame = frame_params.key_frame;
          encoded_frame->quantizer = frame_params.vpx_min_quantizer;
          encoded_frame->codec = kVideoCodecVP9;

          auto expected_framerate = get_expected_framerate();
          if (expected_framerate.has_value()) {
            EXPECT_EQ(frame_params.fps, *expected_framerate);
            EXPECT_NEAR(frame_params.duration.InMilliseconds(),
                        base::Hertz(*expected_framerate).InMilliseconds(),
                        kDurationMsFudgeFactor);
          }
          std::move(done).Run(WebrtcVideoEncoder::EncodeResult::SUCCEEDED,
                              std::move(encoded_frame));
        });

    video_stream_event_router_.SetVideoChannelStateObserver(
        "screen_stream", observer_.GetWeakPtr());
  }

  std::unique_ptr<WebrtcVideoEncoderWrapper> InitEncoder(SdpVideoFormat sdp,
                                                         VideoCodec codec) {
    auto encoder = std::make_unique<WebrtcVideoEncoderWrapper>(
        sdp, SessionOptions(), task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
        video_stream_event_router_.GetWeakPtr());
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
  void set_expected_framerate(int framerate) {
    expected_framerate_ = framerate;
  }

  const std::optional<int>& get_expected_framerate() const {
    return expected_framerate_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::RunLoop run_loop_;

  std::optional<int> expected_framerate_;

  VideoStreamEventRouter video_stream_event_router_;
  NiceMock<MockVideoChannelStateObserver> observer_;
  MockEncodedImageCallback callback_;
  std::unique_ptr<NiceMock<MockVideoEncoder>> mock_video_encoder_;
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

TEST_F(WebrtcVideoEncoderWrapperTest, NotifiesFrameEncodedAndReturned) {
  EXPECT_CALL(callback_, OnEncodedImage(_, Field(&CodecSpecificInfo::codecType,
                                                 kVideoCodecVP9)))
      .WillOnce(Return(kResultOk));
  EXPECT_CALL(observer_,
              OnEncodedFrameSent(Field(&EncodedImageCallback::Result::error,
                                       EncodedImageCallback::Result::OK),
                                 _));

  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameKey};
  encoder->Encode(MakeVideoFrame(), &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest,
       NotifiesFrameEncodedAndReturnedInMultiStreamMode) {
  EXPECT_CALL(callback_, OnEncodedImage(_, Field(&CodecSpecificInfo::codecType,
                                                 kVideoCodecVP9)))
      .WillOnce(Return(kResultOk));
  EXPECT_CALL(observer_,
              OnEncodedFrameSent(Field(&EncodedImageCallback::Result::error,
                                       EncodedImageCallback::Result::OK),
                                 _));

  // Register a multi-stream observer for |kTestScreenId|.
  video_stream_event_router_.SetVideoChannelStateObserver(
      "screen_stream_16", observer_.GetWeakPtr());

  // Also set up a strict mock observer for another screen id to ensure the
  // proper observer is called.
  StrictMock<MockVideoChannelStateObserver> observer;
  video_stream_event_router_.SetVideoChannelStateObserver(
      "screen_stream_17", observer.GetWeakPtr());

  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameKey};
  encoder->Encode(MakeVideoFrame(), &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, FrameDroppedIfAsyncEncoderBusy) {
  EXPECT_CALL(callback_, OnEncodedImage(_, Field(&CodecSpecificInfo::codecType,
                                                 kVideoCodecVP9)))
      .Times(2)
      .WillRepeatedly(Return(kResultOk));
  auto frame1 = MakeVideoFrame();
  auto frame2 = MakeVideoFrame();
  auto frame3 = MakeVideoFrame();
  auto frame4 = MakeVideoFrame();
  auto frame5 = MakeVideoFrame();
  auto frame6 = MakeVideoFrame();
  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  encoder->SetEncoderForTest(std::move(mock_video_encoder_));
  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameKey};
  // Encode task will be posted immediately.
  encoder->Encode(frame1, &frame_types);
  // Frame2 contents will be stored in 'pending' frame while frame 1 is encoded.
  encoder->Encode(frame2, &frame_types);
  // Frame3 contents will be replace the frame2 contents in 'pending' and cause
  // frame2 to be dropped.
  encoder->Encode(frame3, &frame_types);
  // Replace frame3 contents with frame4.
  encoder->Encode(frame4, &frame_types);
  // Replace frame4 contents with frame5.
  encoder->Encode(frame5, &frame_types);
  // Replace frame5 contents with frame6.
  encoder->Encode(frame6, &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest,
       DroppedFrameUpdateRectCombinedWithNextFrame) {
  {
    InSequence s;

    // Encode frame1.
    EXPECT_CALL(*mock_video_encoder_, Encode);
    EXPECT_CALL(
        callback_,
        OnEncodedImage(_, Field(&CodecSpecificInfo::codecType, kVideoCodecVP9)))
        .WillOnce(Return(kResultOk));

    // Encode frame3. Its update-region should be the rectangle-union of frame2
    // and frame3.
    auto combined_rect = DesktopRect::MakeLTRB(100, 200, 310, 410);
    EXPECT_CALL(*mock_video_encoder_,
                Encode(Pointee(MatchesUpdateRect(combined_rect)), _, _));
    EXPECT_CALL(
        callback_,
        OnEncodedImage(_, Field(&CodecSpecificInfo::codecType, kVideoCodecVP9)))
        .WillOnce(Return(kResultOk));
  }

  auto frame1 = MakeVideoFrame();
  auto frame2 = MakeVideoFrame();
  frame2.set_update_rect(VideoFrame::UpdateRect{
      .offset_x = 100, .offset_y = 200, .width = 10, .height = 10});
  auto frame3 = MakeVideoFrame();
  frame3.set_update_rect(VideoFrame::UpdateRect{
      .offset_x = 300, .offset_y = 400, .width = 10, .height = 10});

  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  encoder->SetEncoderForTest(std::move(mock_video_encoder_));
  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameKey};

  // Frame2 will be dropped and replaced by frame3 since the encoder is busy.
  encoder->Encode(frame1, &frame_types);
  encoder->Encode(frame2, &frame_types);
  encoder->Encode(frame3, &frame_types);
  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, EmptyFrameDropped) {
  EXPECT_CALL(callback_, OnEncodedImage(_, _)).WillOnce(Return(kResultOk));

  auto frame1 = MakeVideoFrame();
  auto frame2 = MakeEmptyVideoFrame();
  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());

  // Delta is used here, since key-frame requests should not be dropped.
  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameDelta};
  encoder->Encode(frame1, &frame_types);

  // Need to fast-forward a little bit, so the frame is not dropped
  // because of the busy encoder.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  encoder->Encode(frame2, &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, EmptyFrameNotDroppedAfter2Seconds) {
  EXPECT_CALL(callback_, OnEncodedImage(_, _))
      .Times(2)
      .WillRepeatedly(Return(kResultOk));

  auto frame1 = MakeVideoFrame();
  auto frame2 = MakeEmptyVideoFrame();
  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  // Delta is used in this test, because key-frames should never be dropped
  // anyway.
  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameDelta};
  encoder->Encode(frame1, &frame_types);
  task_environment_.FastForwardBy(base::Milliseconds(2500));
  encoder->Encode(frame2, &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, EmptyFrameNotDroppedIfKeyFrame) {
  EXPECT_CALL(callback_, OnEncodedImage(_, _))
      .Times(2)
      .WillRepeatedly(Return(kResultOk));

  auto frame1 = MakeVideoFrame();
  auto frame2 = MakeEmptyVideoFrame();
  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameKey};
  encoder->Encode(frame1, &frame_types);

  // Fast-forward a little bit, so the frame is not dropped because of the
  // busy encoder.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  encoder->Encode(frame2, &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest,
       KeyFrameRequestRememberedIfAsyncEncoderBusy) {
  // Three frames are used for this test:
  // Frame 1 kicks off the encoder.
  // Frame 2 is a key-frame request, which is dropped because it is replaced by
  //     frame 3 while frame 1 is being encoded.
  // Frame 3 is a delta-frame request from WebRTC, but the encoder-wrapper
  //     should encode it as a key-frame because of the previous key-frame
  //     request from WebRTC.
  //
  // The end-result is that the encoder should see two key-frame requests (for
  // frames 1 and 3).
  EXPECT_CALL(*mock_video_encoder_, Encode(_, IsKeyFrame(), _)).Times(2);
  EXPECT_CALL(callback_, OnEncodedImage(_, _))
      .Times(2)
      .WillRepeatedly(Return(kResultOk));

  auto frame1 = MakeVideoFrame();
  auto frame2 = MakeVideoFrame();
  auto frame3 = MakeVideoFrame();
  std::vector<VideoFrameType> frame_types1{VideoFrameType::kVideoFrameKey};
  std::vector<VideoFrameType> frame_types2{VideoFrameType::kVideoFrameKey};
  std::vector<VideoFrameType> frame_types3{VideoFrameType::kVideoFrameDelta};
  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  encoder->SetEncoderForTest(std::move(mock_video_encoder_));

  encoder->Encode(frame1, &frame_types1);
  encoder->Encode(frame2, &frame_types2);
  encoder->Encode(frame3, &frame_types3);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest,
       NoRegisteredObserverIsHandledInMultiStreamMode) {
  EXPECT_CALL(callback_, OnEncodedImage(_, Field(&CodecSpecificInfo::codecType,
                                                 kVideoCodecVP9)))
      .WillOnce(Return(kResultOk));

  // Register a multi-stream observer for |kTestScreenId|.
  auto observer = std::make_unique<StrictMock<MockVideoChannelStateObserver>>();
  video_stream_event_router_.SetVideoChannelStateObserver(
      "screen_stream_16", observer->GetWeakPtr());

  observer.reset();

  auto sdp_format = GetVp9Format();
  sdp_format.parameters.emplace("max-fr", "42");
  auto encoder = InitEncoder(std::move(sdp_format), GetVp9Codec());
  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameKey};
  encoder->Encode(MakeVideoFrame(), &frame_types);

  PostQuitAndRun();
}

TEST_F(WebrtcVideoEncoderWrapperTest, FrameDurationAndFpsCalculated) {
  EXPECT_CALL(callback_, OnEncodedImage(_, _))
      .Times(3)
      .WillRepeatedly(Return(kResultOk));

  set_expected_framerate(kTargetFrameRate);

  auto encoder = InitEncoder(GetVp9Format(), GetVp9Codec());
  encoder->SetEncoderForTest(std::move(mock_video_encoder_));

  std::vector<VideoFrameType> frame_types{VideoFrameType::kVideoFrameKey};
  encoder->Encode(MakeVideoFrame(), &frame_types);
  task_environment_.FastForwardBy(base::Hertz(kTargetFrameRate));
  encoder->Encode(MakeVideoFrame(), &frame_types);
  task_environment_.FastForwardBy(base::Hertz(kTargetFrameRate));
  encoder->Encode(MakeVideoFrame(), &frame_types);

  PostQuitAndRun();
}

}  // namespace remoting::protocol
