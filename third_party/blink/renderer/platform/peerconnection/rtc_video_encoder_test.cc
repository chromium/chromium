// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Values;
using ::testing::WithArgs;

namespace blink {

namespace {

const int kInputFrameFillY = 12;
const int kInputFrameFillU = 23;
const int kInputFrameFillV = 34;
const uint16_t kInputFrameHeight = 234;
const uint16_t kInputFrameWidth = 345;
const uint16_t kStartBitrate = 100;

const webrtc::VideoEncoder::Capabilities kVideoEncoderCapabilities(
    /* loss_notification= */ false);
const webrtc::VideoEncoder::Settings
    kVideoEncoderSettings(kVideoEncoderCapabilities, 1, 12345);

class EncodedImageCallbackWrapper : public webrtc::EncodedImageCallback {
 public:
  using EncodedCallback = base::OnceCallback<void(
      const webrtc::EncodedImage& encoded_image,
      const webrtc::CodecSpecificInfo* codec_specific_info)>;

  EncodedImageCallbackWrapper(EncodedCallback encoded_callback)
      : encoded_callback_(std::move(encoded_callback)) {}

  Result OnEncodedImage(
      const webrtc::EncodedImage& encoded_image,
      const webrtc::CodecSpecificInfo* codec_specific_info) override {
    std::move(encoded_callback_).Run(encoded_image, codec_specific_info);
    return Result(Result::OK);
  }

 private:
  EncodedCallback encoded_callback_;
};
}  // anonymous namespace

class RTCVideoEncoderTest
    : public ::testing::TestWithParam<webrtc::VideoCodecType> {
 public:
  RTCVideoEncoderTest()
      : encoder_thread_("vea_thread"),
        mock_gpu_factories_(
            new media::MockGpuVideoAcceleratorFactories(nullptr)),
        idle_waiter_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  media::MockVideoEncodeAccelerator* ExpectCreateInitAndDestroyVEA() {
    // The VEA will be owned by the RTCVideoEncoder once
    // factory.CreateVideoEncodeAccelerator() is called.
    media::MockVideoEncodeAccelerator* mock_vea =
        new media::MockVideoEncodeAccelerator();

    EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
        .WillRepeatedly(Return(mock_vea));
    EXPECT_CALL(*mock_vea, Initialize(_, _))
        .WillOnce(Invoke(this, &RTCVideoEncoderTest::Initialize));
    EXPECT_CALL(*mock_vea, UseOutputBitstreamBuffer(_)).Times(AtLeast(3));
    EXPECT_CALL(*mock_vea, Destroy()).Times(1);
    return mock_vea;
  }

  void SetUp() override {
    DVLOG(3) << __func__;
    ASSERT_TRUE(encoder_thread_.Start());

    EXPECT_CALL(*mock_gpu_factories_.get(), GetTaskRunner())
        .WillRepeatedly(Return(encoder_thread_.task_runner()));
    mock_vea_ = ExpectCreateInitAndDestroyVEA();
  }

  void TearDown() override {
    DVLOG(3) << __func__;
    EXPECT_TRUE(encoder_thread_.IsRunning());
    RunUntilIdle();
    rtc_encoder_->Release();
    RunUntilIdle();
    encoder_thread_.Stop();
  }

  void RunUntilIdle() {
    DVLOG(3) << __func__;
    encoder_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                  base::Unretained(&idle_waiter_)));
    idle_waiter_.Wait();
  }

  void CreateEncoder(webrtc::VideoCodecType codec_type) {
    DVLOG(3) << __func__;
    media::VideoCodecProfile media_profile;
    switch (codec_type) {
      case webrtc::kVideoCodecVP8:
        media_profile = media::VP8PROFILE_ANY;
        break;
      case webrtc::kVideoCodecH264:
        media_profile = media::H264PROFILE_BASELINE;
        break;
      case webrtc::kVideoCodecVP9:
        media_profile = media::VP9PROFILE_PROFILE0;
        break;
      default:
        ADD_FAILURE() << "Unexpected codec type: " << codec_type;
        media_profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
    }
    rtc_encoder_ = std::make_unique<RTCVideoEncoder>(media_profile, false,
                                                     mock_gpu_factories_.get());
  }

  // media::VideoEncodeAccelerator implementation.
  bool Initialize(const media::VideoEncodeAccelerator::Config& config,
                  media::VideoEncodeAccelerator::Client* client) {
    DVLOG(3) << __func__;
    client_ = client;
    client_->RequireBitstreamBuffers(0, config.input_visible_size,
                                     config.input_visible_size.GetArea());
    return true;
  }

  void RegisterEncodeCompleteCallback(
      EncodedImageCallbackWrapper::EncodedCallback callback) {
    callback_wrapper_ =
        std::make_unique<EncodedImageCallbackWrapper>(std::move(callback));
    rtc_encoder_->RegisterEncodeCompleteCallback(callback_wrapper_.get());
  }

  webrtc::VideoCodec GetDefaultCodec() {
    webrtc::VideoCodec codec = {};
    memset(&codec, 0, sizeof(codec));
    codec.width = kInputFrameWidth;
    codec.height = kInputFrameHeight;
    codec.codecType = webrtc::kVideoCodecVP8;
    codec.startBitrate = kStartBitrate;
    return codec;
  }

  webrtc::VideoCodec GetDefaultTemporalLayerCodec() {
    const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
    webrtc::VideoCodec codec{};
    codec.codecType = codec_type;
    codec.width = kInputFrameWidth;
    codec.height = kInputFrameHeight;
    codec.startBitrate = kStartBitrate;
    codec.maxBitrate = codec.startBitrate * 2;
    codec.minBitrate = codec.startBitrate / 2;
    codec.maxFramerate = 24;
    codec.active = true;
    codec.qpMax = 30;
    codec.numberOfSimulcastStreams = 1;
    codec.mode = webrtc::VideoCodecMode::kRealtimeVideo;
    webrtc::VideoCodecVP9& vp9 = *codec.VP9();
    vp9.numberOfTemporalLayers = 3;
    vp9.numberOfSpatialLayers = 1;
    webrtc::SpatialLayer& sl = codec.spatialLayers[0];
    sl.width = kInputFrameWidth;
    sl.height = kInputFrameHeight;
    sl.maxFramerate = 24;
    sl.numberOfTemporalLayers = vp9.numberOfTemporalLayers;
    sl.targetBitrate = kStartBitrate;
    sl.maxBitrate = sl.targetBitrate;
    sl.minBitrate = sl.targetBitrate;
    sl.qpMax = 30;
    sl.active = true;
    return codec;
  }

  void FillFrameBuffer(rtc::scoped_refptr<webrtc::I420Buffer> frame) {
    CHECK(libyuv::I420Rect(frame->MutableDataY(), frame->StrideY(),
                           frame->MutableDataU(), frame->StrideU(),
                           frame->MutableDataV(), frame->StrideV(), 0, 0,
                           frame->width(), frame->height(), kInputFrameFillY,
                           kInputFrameFillU, kInputFrameFillV) == 0);
  }

  void VerifyEncodedFrame(scoped_refptr<media::VideoFrame> frame,
                          bool force_keyframe) {
    DVLOG(3) << __func__;
    EXPECT_EQ(kInputFrameWidth, frame->visible_rect().width());
    EXPECT_EQ(kInputFrameHeight, frame->visible_rect().height());
    EXPECT_EQ(kInputFrameFillY,
              frame->visible_data(media::VideoFrame::kYPlane)[0]);
    EXPECT_EQ(kInputFrameFillU,
              frame->visible_data(media::VideoFrame::kUPlane)[0]);
    EXPECT_EQ(kInputFrameFillV,
              frame->visible_data(media::VideoFrame::kVPlane)[0]);
  }

  void ReturnFrameWithTimeStamp(scoped_refptr<media::VideoFrame> frame,
                                bool force_keyframe) {
    client_->BitstreamBufferReady(
        0,
        media::BitstreamBufferMetadata(0, force_keyframe, frame->timestamp()));
  }

  void ReturnTemporalLayerFrameWithVp9Metadata(
      scoped_refptr<media::VideoFrame> frame,
      bool force_keyframe) {
    int32_t bitstream_buffer_id = frame->timestamp().InMicroseconds();
    CHECK(0 <= bitstream_buffer_id && bitstream_buffer_id <= 4);
    media::BitstreamBufferMetadata metadata(100u /* payload_size_bytes */,
                                            force_keyframe, frame->timestamp());
    // Assume the number of TLs is three. TL structure is below.
    // TL2:      [#1]     /-[#3]
    // TL1:     /_____[#2]   /
    // TL0: [#0]-----------------[#4]
    media::Vp9Metadata vp9;
    vp9.has_reference = bitstream_buffer_id != 0 && !force_keyframe;
    vp9.temporal_up_switch = bitstream_buffer_id != 3;
    switch (bitstream_buffer_id) {
      case 0:
        vp9.temporal_idx = 0;
        break;
      case 1:
        vp9.temporal_idx = 2;
        vp9.p_diffs = {1};
        break;
      case 2:
        vp9.temporal_idx = 1;
        vp9.p_diffs = {2};
        break;
      case 3:
        vp9.temporal_idx = 2;
        vp9.p_diffs = {1, 3};
        break;
      case 4:
        vp9.temporal_idx = 0;
        vp9.p_diffs = {4};
        break;
    }
    metadata.vp9 = vp9;
    client_->BitstreamBufferReady(bitstream_buffer_id, metadata);
  }

  void VerifyTimestamp(uint32_t rtp_timestamp,
                       int64_t capture_time_ms,
                       const webrtc::EncodedImage& encoded_image,
                       const webrtc::CodecSpecificInfo* codec_specific_info) {
    DVLOG(3) << __func__;
    EXPECT_EQ(rtp_timestamp, encoded_image.Timestamp());
    EXPECT_EQ(capture_time_ms, encoded_image.capture_time_ms_);
  }

 protected:
  media::MockVideoEncodeAccelerator* mock_vea_;
  std::unique_ptr<RTCVideoEncoder> rtc_encoder_;
  media::VideoEncodeAccelerator::Client* client_;
  base::Thread encoder_thread_;

 private:
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  std::unique_ptr<EncodedImageCallbackWrapper> callback_wrapper_;
  base::WaitableEvent idle_waiter_;
};

TEST_P(RTCVideoEncoderTest, CreateAndInitSucceeds) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_P(RTCVideoEncoderTest, RepeatedInitSucceeds) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

INSTANTIATE_TEST_SUITE_P(CodecProfiles,
                         RTCVideoEncoderTest,
                         Values(webrtc::kVideoCodecVP8,
                                webrtc::kVideoCodecH264));

TEST_F(RTCVideoEncoderTest, CreateAndInitSucceedsForTemporalLayer) {
  webrtc::VideoCodec tl_codec = GetDefaultTemporalLayerCodec();
  CreateEncoder(tl_codec.codecType);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
}

// Checks that WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE is returned when there is
// platform error.
TEST_F(RTCVideoEncoderTest, SoftwareFallbackAfterError) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .WillOnce(Invoke([this](scoped_refptr<media::VideoFrame>, bool) {
        encoder_thread_.task_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &media::VideoEncodeAccelerator::Client::NotifyError,
                base::Unretained(client_),
                media::VideoEncodeAccelerator::kPlatformFailureError));
      }));

  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  RunUntilIdle();

  // Expect the next frame to return SW fallback.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}

TEST_F(RTCVideoEncoderTest, EncodeScaledFrame) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .Times(2)
      .WillRepeatedly(Invoke(this, &RTCVideoEncoderTest::VerifyEncodedFrame));

  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));

  const rtc::scoped_refptr<webrtc::I420Buffer> upscaled_buffer =
      webrtc::I420Buffer::Create(2 * kInputFrameWidth, 2 * kInputFrameHeight);
  FillFrameBuffer(upscaled_buffer);
  webrtc::VideoFrame rtc_frame = webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(upscaled_buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build();
  rtc_frame.set_ntp_time_ms(123456);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(rtc_frame, &frame_types));
}

TEST_F(RTCVideoEncoderTest, PreserveTimestamps) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  const uint32_t rtp_timestamp = 1234567;
  const uint32_t capture_time_ms = 3456789;
  RegisterEncodeCompleteCallback(
      base::BindOnce(&RTCVideoEncoderTest::VerifyTimestamp,
                     base::Unretained(this), rtp_timestamp, capture_time_ms));

  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::ReturnFrameWithTimeStamp));
  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types;
  webrtc::VideoFrame rtc_frame = webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(rtp_timestamp)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build();
  rtc_frame.set_timestamp_us(capture_time_ms * rtc::kNumMicrosecsPerMillisec);
  // We need to set ntp_time_ms because it will be used to derive
  // media::VideoFrame timestamp.
  rtc_frame.set_ntp_time_ms(4567891);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(rtc_frame, &frame_types));
}

TEST_F(RTCVideoEncoderTest, EncodeTemporalLayer) {
  webrtc::VideoCodec tl_codec = GetDefaultTemporalLayerCodec();
  CreateEncoder(tl_codec.codecType);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  size_t kNumEncodeFrames = 5u;
  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .Times(kNumEncodeFrames)
      .WillRepeatedly(Invoke(
          this, &RTCVideoEncoderTest::ReturnTemporalLayerFrameWithVp9Metadata));

  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_timestamp_rtp(0)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
  }
}
}  // namespace blink
