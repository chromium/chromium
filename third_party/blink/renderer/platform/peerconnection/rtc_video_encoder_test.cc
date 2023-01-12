// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Values;
using ::testing::WithArgs;

using SpatialLayer = media::VideoEncodeAccelerator::Config::SpatialLayer;

namespace blink {

namespace {

const int kInputFrameFillY = 12;
const int kInputFrameFillU = 23;
const int kInputFrameFillV = 34;
const uint16_t kInputFrameHeight = 234;
const uint16_t kInputFrameWidth = 456;
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

class RTCVideoEncoderWrapper : public webrtc::VideoEncoder {
 public:
  static std::unique_ptr<RTCVideoEncoderWrapper> Create(
      media::VideoCodecProfile profile,
      bool is_constrained_h264,
      media::GpuVideoAcceleratorFactories* gpu_factories) {
    auto wrapper = base::WrapUnique(new RTCVideoEncoderWrapper);
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    wrapper->task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<RTCVideoEncoder>* rtc_video_encoder,
               media::VideoCodecProfile profile, bool is_constrained_h264,
               media::GpuVideoAcceleratorFactories* gpu_factories,
               base::WaitableEvent* waiter) {
              *rtc_video_encoder = std::make_unique<RTCVideoEncoder>(
                  profile, is_constrained_h264, gpu_factories);
              waiter->Signal();
            },
            &wrapper->rtc_video_encoder_, profile, is_constrained_h264,
            gpu_factories, &waiter));
    waiter.Wait();
    return wrapper;
  }

  int InitEncode(const webrtc::VideoCodec* codec_settings,
                 const webrtc::VideoEncoder::Settings& settings) override {
    int result = 0;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](RTCVideoEncoder* rtc_video_encoder,
               const webrtc::VideoCodec* codec_settings,
               const webrtc::VideoEncoder::Settings& settings,
               base::WaitableEvent* waiter, int* result) {
              *result = rtc_video_encoder->InitEncode(codec_settings, settings);
              waiter->Signal();
            },
            rtc_video_encoder_.get(), codec_settings, settings, &waiter,
            &result));
    waiter.Wait();
    return result;
  }
  int32_t Encode(
      const webrtc::VideoFrame& input_image,
      const std::vector<webrtc::VideoFrameType>* frame_types) override {
    int32_t result = 0;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](RTCVideoEncoder* rtc_video_encoder,
               const webrtc::VideoFrame& input_image,
               const std::vector<webrtc::VideoFrameType>* frame_types,
               base::WaitableEvent* waiter, int32_t* result) {
              *result = rtc_video_encoder->Encode(input_image, frame_types);
              waiter->Signal();
            },
            rtc_video_encoder_.get(), input_image, frame_types, &waiter,
            &result));
    waiter.Wait();
    return result;
  }
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override {
    int32_t result = 0;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](RTCVideoEncoder* rtc_video_encoder,
               webrtc::EncodedImageCallback* callback,
               base::WaitableEvent* waiter, int32_t* result) {
              *result =
                  rtc_video_encoder->RegisterEncodeCompleteCallback(callback);
              waiter->Signal();
            },
            rtc_video_encoder_.get(), callback, &waiter, &result));
    waiter.Wait();
    return result;
  }
  int32_t Release() override {
    int32_t result = 0;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](RTCVideoEncoder* rtc_video_encoder,
                          base::WaitableEvent* waiter, int32_t* result) {
                         *result = rtc_video_encoder->Release();
                         waiter->Signal();
                       },
                       rtc_video_encoder_.get(), &waiter, &result));
    waiter.Wait();
    return result;
  }
  void SetErrorWaiter(base::WaitableEvent* error_waiter) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](RTCVideoEncoder* rtc_video_encoder,
               base::WaitableEvent* waiter) {
              rtc_video_encoder->SetErrorCallbackForTesting(CrossThreadBindOnce(
                  [](base::WaitableEvent* waiter) { waiter->Signal(); },
                  CrossThreadUnretained(waiter)));
            },
            rtc_video_encoder_.get(), error_waiter));
    return;
  }

  void SetRates(
      const webrtc::VideoEncoder::RateControlParameters& parameters) override {
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](RTCVideoEncoder* rtc_video_encoder,
               const webrtc::VideoEncoder::RateControlParameters& parameters,
               base::WaitableEvent* waiter) {
              rtc_video_encoder->SetRates(parameters);
              waiter->Signal();
            },
            rtc_video_encoder_.get(), parameters, &waiter));
    waiter.Wait();
  }
  EncoderInfo GetEncoderInfo() const override {
    NOTIMPLEMENTED();
    return EncoderInfo();
  }

  ~RTCVideoEncoderWrapper() override {
    if (task_runner_) {
      task_runner_->DeleteSoon(FROM_HERE, std::move(rtc_video_encoder_));
    }
    webrtc_encoder_thread_.FlushForTesting();
  }

 private:
  RTCVideoEncoderWrapper() : webrtc_encoder_thread_("WebRTC encoder thread") {
    webrtc_encoder_thread_.Start();
    task_runner_ = webrtc_encoder_thread_.task_runner();
  }

  base::Thread webrtc_encoder_thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // |webrtc_encoder_thread_| members.
  std::unique_ptr<RTCVideoEncoder> rtc_video_encoder_;
};
}  // anonymous namespace

class RTCVideoEncoderTest {
 public:
  RTCVideoEncoderTest()
      : encoder_thread_("vea_thread"),
        mock_gpu_factories_(
            new media::MockGpuVideoAcceleratorFactories(nullptr)) {}

  media::MockVideoEncodeAccelerator* ExpectCreateInitAndDestroyVEA(
      bool vea_used) {
    // The VEA will be owned by the RTCVideoEncoder once
    // factory.CreateVideoEncodeAccelerator() is called.
    media::MockVideoEncodeAccelerator* mock_vea =
        new media::MockVideoEncodeAccelerator();

    EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
        .WillRepeatedly(Return(mock_vea));
    if (vea_used) {
      EXPECT_CALL(*mock_vea, Initialize)
          .WillOnce(Invoke(this, &RTCVideoEncoderTest::Initialize));
      EXPECT_CALL(*mock_vea, UseOutputBitstreamBuffer).Times(AtLeast(3));
    }
    EXPECT_CALL(*mock_vea, Destroy()).Times(1);
    return mock_vea;
  }

  void SetUp() {
    DVLOG(3) << __func__;
    ASSERT_TRUE(encoder_thread_.Start());

    EXPECT_CALL(*mock_gpu_factories_.get(), GetTaskRunner())
        .WillRepeatedly(Return(encoder_thread_.task_runner()));
  }

  void TearDown() {
    DVLOG(3) << __func__;
    EXPECT_TRUE(encoder_thread_.IsRunning());
    RunUntilIdle();
    if (rtc_encoder_)
      rtc_encoder_->Release();
    RunUntilIdle();
    encoder_thread_.Stop();
  }

  void RunUntilIdle() {
    DVLOG(3) << __func__;
    encoder_thread_.FlushForTesting();
  }

  void CreateEncoder(webrtc::VideoCodecType codec_type, bool vea_used = true) {
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

    mock_vea_ = ExpectCreateInitAndDestroyVEA(vea_used);
    rtc_encoder_ = RTCVideoEncoderWrapper::Create(media_profile, false,
                                                  mock_gpu_factories_.get());
  }

  void CreateEncoderWithoutVea(webrtc::VideoCodecType codec_type) {
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

    rtc_encoder_ = RTCVideoEncoderWrapper::Create(media_profile, false,
                                                  mock_gpu_factories_.get());
  }

  // media::VideoEncodeAccelerator implementation.
  bool Initialize(const media::VideoEncodeAccelerator::Config& config,
                  media::VideoEncodeAccelerator::Client* client,
                  std::unique_ptr<media::MediaLog> media_log) {
    DVLOG(3) << __func__;
    config_ = config;
    client_ = client;

    constexpr size_t kNumInputBuffers = 3;
    client_->RequireBitstreamBuffers(kNumInputBuffers,
                                     config.input_visible_size,
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

  webrtc::VideoCodec GetSVCLayerCodec(webrtc::VideoCodecType codec_type,
                                      size_t num_spatial_layers) {
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
    switch (codec_type) {
      case webrtc::kVideoCodecVP9: {
        webrtc::VideoCodecVP9& vp9 = *codec.VP9();
        vp9.numberOfTemporalLayers = 3;
        vp9.numberOfSpatialLayers = num_spatial_layers;
        num_spatial_layers_ = num_spatial_layers;
        constexpr int kDenom[] = {4, 2, 1};
        for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
          webrtc::SpatialLayer& sl = codec.spatialLayers[sid];
          const int denom = kDenom[sid];
          sl.width = kInputFrameWidth / denom;
          sl.height = kInputFrameHeight / denom;
          sl.maxFramerate = 24;
          sl.numberOfTemporalLayers = vp9.numberOfTemporalLayers;
          sl.targetBitrate = kStartBitrate / denom;
          sl.maxBitrate = sl.targetBitrate / denom;
          sl.minBitrate = sl.targetBitrate / denom;
          sl.qpMax = 30;
          sl.active = true;
        }
      } break;
      default:
        NOTREACHED();
    }
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

  void ReturnSVCLayerFrameWithVp9Metadata(
      scoped_refptr<media::VideoFrame> frame,
      bool force_keyframe) {
    int32_t frame_num = frame->timestamp().InMicroseconds();
    CHECK(0 <= frame_num && frame_num <= 4);
    media::BitstreamBufferMetadata metadata(100u /* payload_size_bytes */,
                                            force_keyframe, frame->timestamp());

    for (size_t sid = 0; sid < num_spatial_layers_; ++sid) {
      // Assume the number of TLs is three. TL structure is below.
      // TL2:      [#1]     /-[#3]
      // TL1:     /_____[#2]
      // TL0: [#0]-----------------[#4]
      media::Vp9Metadata vp9;
      vp9.inter_pic_predicted = frame_num != 0 && !force_keyframe;
      constexpr int kNumTemporalLayers = 3;
      vp9.temporal_up_switch = frame_num != kNumTemporalLayers;
      switch (frame_num) {
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
          vp9.p_diffs = {1};
          break;
        case 4:
          vp9.temporal_idx = 0;
          vp9.p_diffs = {4};
          break;
      }
      // Assume k-SVC encoding.
      metadata.key_frame = frame_num == 0 && sid == 0;
      vp9.spatial_idx = sid;
      vp9.reference_lower_spatial_layers = frame_num == 0 && sid != 0;
      vp9.referenced_by_upper_spatial_layers =
          frame_num == 0 && (sid + 1 != num_spatial_layers_);
      vp9.end_of_picture = (sid + 1 == num_spatial_layers_);
      if (metadata.key_frame) {
        constexpr int kDenom[] = {4, 2, 1};
        for (size_t i = 0; i < num_spatial_layers_; ++i) {
          const int denom = kDenom[i];
          vp9.spatial_layer_resolutions.emplace_back(
              gfx::Size(frame->coded_size().width() / denom,
                        frame->coded_size().height() / denom));
        }
      }
      metadata.vp9 = vp9;
      client_->BitstreamBufferReady(sid, metadata);
    }
  }

  void VerifyTimestamp(uint32_t rtp_timestamp,
                       int64_t capture_time_ms,
                       const webrtc::EncodedImage& encoded_image,
                       const webrtc::CodecSpecificInfo* codec_specific_info) {
    DVLOG(3) << __func__;
    EXPECT_EQ(rtp_timestamp, encoded_image.Timestamp());
    EXPECT_EQ(capture_time_ms, encoded_image.capture_time_ms_);
  }

  std::vector<gfx::Size> ToResolutionList(const webrtc::VideoCodec& codec) {
    std::vector<gfx::Size> resolutions;
    switch (codec.codecType) {
      case webrtc::VideoCodecType::kVideoCodecVP8:
      case webrtc::VideoCodecType::kVideoCodecH264: {
        for (int i = 0; i < codec.numberOfSimulcastStreams; ++i) {
          if (!codec.simulcastStream[i].active) {
            break;
          }
          resolutions.emplace_back(codec.simulcastStream[i].width,
                                   codec.simulcastStream[i].height);
        }
        break;
      }
      case webrtc::VideoCodecType::kVideoCodecVP9: {
        for (int i = 0; i < codec.VP9().numberOfSpatialLayers; ++i) {
          if (!codec.spatialLayers[i].active) {
            break;
          }
          resolutions.emplace_back(codec.spatialLayers[i].width,
                                   codec.spatialLayers[i].height);
        }
        break;
      }
      default: {
        return {};
      }
    }

    return resolutions;
  }

 protected:
  media::MockVideoEncodeAccelerator* mock_vea_;
  std::unique_ptr<RTCVideoEncoderWrapper> rtc_encoder_;
  absl::optional<media::VideoEncodeAccelerator::Config> config_;
  media::VideoEncodeAccelerator::Client* client_;
  base::Thread encoder_thread_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  std::unique_ptr<EncodedImageCallbackWrapper> callback_wrapper_;
  size_t num_spatial_layers_;
};

class RTCVideoEncoderInitTest
    : public RTCVideoEncoderTest,
      public ::testing::TestWithParam<webrtc::VideoCodecType> {
 public:
  RTCVideoEncoderInitTest() = default;
  ~RTCVideoEncoderInitTest() override = default;
  void SetUp() override { RTCVideoEncoderTest::SetUp(); }
  void TearDown() override { RTCVideoEncoderTest::TearDown(); }
};

TEST_P(RTCVideoEncoderInitTest, CreateAndInitSucceeds) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_P(RTCVideoEncoderInitTest, RepeatedInitSucceeds) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
  ExpectCreateInitAndDestroyVEA(true);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_P(RTCVideoEncoderInitTest, CreateAndInitSucceedsForTemporalLayer) {
  const webrtc::VideoCodecType codec_type = GetParam();
  if (codec_type == webrtc::kVideoCodecVP8)
    GTEST_SKIP() << "VP8 temporal layer encoding is not supported";
  if (codec_type == webrtc::kVideoCodecH264)
    GTEST_SKIP() << "H264 temporal layer encoding is not supported";

  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(codec_type,
                                                 /*num_spatial_layers=*/1);
  CreateEncoder(tl_codec.codecType);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
}

INSTANTIATE_TEST_SUITE_P(CodecProfiles,
                         RTCVideoEncoderInitTest,
                         Values(webrtc::kVideoCodecH264,
                                webrtc::kVideoCodecVP8,
                                webrtc::kVideoCodecVP9));

class RTCVideoEncoderEncodeTest : public RTCVideoEncoderTest,
                                  public ::testing::TestWithParam<bool> {
 public:
  RTCVideoEncoderEncodeTest() {
    if (GetParam())
      feature_list_.InitAndEnableFeature(features::kWebRtcEncoderAsyncEncode);
  }

  ~RTCVideoEncoderEncodeTest() override = default;
  void SetUp() override { RTCVideoEncoderTest::SetUp(); }
  void TearDown() override { RTCVideoEncoderTest::TearDown(); }

 protected:
  bool AsyncEncodingIsEnabled() const {
    return base::FeatureList::IsEnabled(features::kWebRtcEncoderAsyncEncode);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(RTCVideoEncoderEncodeTest, H264SoftwareFallbackForOddSize) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecH264;
  CreateEncoder(codec_type, false);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  codec.width = kInputFrameWidth - 1;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  if (mock_vea_)
    mock_vea_->Destroy();
}

TEST_P(RTCVideoEncoderEncodeTest, VP8CreateAndInitSucceedsForOddSize) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  codec.width = kInputFrameWidth - 1;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_P(RTCVideoEncoderEncodeTest, VP9CreateAndInitSucceedsForOddSize) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  codec.width = kInputFrameWidth - 1;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

// Checks that WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE is returned when there is
// platform error.
TEST_P(RTCVideoEncoderEncodeTest, SoftwareFallbackAfterError) {
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
  base::WaitableEvent error_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc_encoder_->SetErrorWaiter(&error_waiter);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  error_waiter.Wait();
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

TEST_P(RTCVideoEncoderEncodeTest, SoftwareFallbackOnBadEncodeInput) {
  // Make RTCVideoEncoder expect native input.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kVideoCaptureUseGpuMemoryBuffer);

  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  ASSERT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  auto frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(kInputFrameWidth, kInputFrameHeight));
  frame->set_timestamp(base::Milliseconds(1));
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame, std::vector<scoped_refptr<media::VideoFrame>>(),
          new WebRtcVideoFrameAdapter::SharedResources(nullptr)));
  std::vector<webrtc::VideoFrameType> frame_types;

  // Expect SW fallback because the frame isn't a GpuMemoryBuffer-based frame.
  if (!AsyncEncodingIsEnabled()) {
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(frame_adapter)
                                       .set_timestamp_rtp(1000)
                                       .set_timestamp_us(2000)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    return;
  }

  // The frame type check is done in media thread asynchronously. The error is
  // reported in the second Encode callback.
  base::WaitableEvent error_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc_encoder_->SetErrorWaiter(&error_waiter);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_timestamp_rtp(1000)
                                     .set_timestamp_us(2000)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  error_waiter.Wait();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_timestamp_rtp(2000)
                                     .set_timestamp_us(3000)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}

TEST_P(RTCVideoEncoderEncodeTest, EncodeScaledFrame) {
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
                                     .set_timestamp_us(123456)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(rtc_frame, &frame_types));
}

TEST_P(RTCVideoEncoderEncodeTest, PreserveTimestamps) {
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
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(rtc_frame, &frame_types));
}

TEST_P(RTCVideoEncoderEncodeTest, AcceptsRepeatedWrappedMediaVideoFrame) {
  // Ensure encoder is accepting subsequent frames with the same timestamp in
  // the wrapped media::VideoFrame.
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings);

  auto frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(kInputFrameWidth, kInputFrameHeight));
  frame->set_timestamp(base::Milliseconds(1));
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame, std::vector<scoped_refptr<media::VideoFrame>>(),
          new WebRtcVideoFrameAdapter::SharedResources(nullptr)));
  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_timestamp_rtp(1000)
                                     .set_timestamp_us(2000)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_timestamp_rtp(2000)
                                     .set_timestamp_us(3000)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}

TEST_P(RTCVideoEncoderEncodeTest, EncodeVP9TemporalLayer) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/1);
  CreateEncoder(tl_codec.codecType);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  size_t kNumEncodeFrames = 5u;
  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .Times(kNumEncodeFrames)
      .WillRepeatedly(Invoke(
          this, &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata));

  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0)
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
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

TEST_P(RTCVideoEncoderEncodeTest, InitializeWithTooHighBitrateFails) {
  // We expect initialization to fail. We do not want a mock video encoder, as
  // it will not be successfully attached to the rtc_encoder_. So we do not call
  // CreateEncoder, but instead CreateEncoderWithoutVea.
  constexpr webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoderWithoutVea(codec_type);

  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  codec.startBitrate = std::numeric_limits<uint32_t>::max() / 100;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_PARAMETER,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)
// Currently we only test spatial SVC encoding on CrOS since only CrOS platform
// support spatial SVC encoding.

// http://crbug.com/1226875
TEST_P(RTCVideoEncoderEncodeTest, EncodeSpatialLayer) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  webrtc::VideoCodec sl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&sl_codec, kVideoEncoderSettings));

  constexpr size_t kNumEncodeFrames = 5u;
  class CodecSpecificVerifier : public webrtc::EncodedImageCallback {
   public:
    explicit CodecSpecificVerifier(const webrtc::VideoCodec& codec)
        : codec_(codec) {}
    webrtc::EncodedImageCallback::Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info) override {
      if (encoded_image._frameType == webrtc::VideoFrameType::kVideoFrameKey) {
        EXPECT_TRUE(codec_specific_info->codecSpecific.VP9.ss_data_available);
        const size_t num_spatial_layers = codec_.VP9().numberOfSpatialLayers;
        const auto& vp9_specific = codec_specific_info->codecSpecific.VP9;
        EXPECT_EQ(vp9_specific.num_spatial_layers, num_spatial_layers);
        for (size_t i = 0; i < num_spatial_layers; ++i) {
          EXPECT_EQ(vp9_specific.width[i], codec_.spatialLayers[i].width);
          EXPECT_EQ(vp9_specific.height[i], codec_.spatialLayers[i].height);
        }
      }

      if (encoded_image.Timestamp() == kNumEncodeFrames - 1 &&
          codec_specific_info->end_of_picture) {
        waiter_.Signal();
      }
      return Result(Result::OK);
    }

    void Wait() { waiter_.Wait(); }

   private:
    const webrtc::VideoCodec& codec_;
    base::WaitableEvent waiter_;
  };
  CodecSpecificVerifier sl_verifier(sl_codec);
  rtc_encoder_->RegisterEncodeCompleteCallback(&sl_verifier);

  EXPECT_CALL(*mock_vea_, Encode)
      .Times(kNumEncodeFrames)
      .WillRepeatedly(Invoke(
          this, &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata));

  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0)
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_timestamp_rtp(i)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
  }
  sl_verifier.Wait();
  RunUntilIdle();
}

TEST_P(RTCVideoEncoderEncodeTest, CreateAndInitVP9ThreeLayerSvc) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  CreateEncoder(tl_codec.codecType);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  EXPECT_THAT(
      *config_,
      Field(&media::VideoEncodeAccelerator::Config::spatial_layers,
            ElementsAre(
                AllOf(Field(&SpatialLayer::width, kInputFrameWidth / 4),
                      Field(&SpatialLayer::height, kInputFrameHeight / 4)),
                AllOf(Field(&SpatialLayer::width, kInputFrameWidth / 2),
                      Field(&SpatialLayer::height, kInputFrameHeight / 2)),
                AllOf(Field(&SpatialLayer::width, kInputFrameWidth),
                      Field(&SpatialLayer::height, kInputFrameHeight)))));
}

TEST_P(RTCVideoEncoderEncodeTest, CreateAndInitVP9SvcSinglecast) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  tl_codec.spatialLayers[1].active = false;
  tl_codec.spatialLayers[2].active = false;
  CreateEncoder(tl_codec.codecType);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  EXPECT_THAT(*config_,
              Field(&media::VideoEncodeAccelerator::Config::spatial_layers,
                    ElementsAre(AllOf(
                        Field(&SpatialLayer::width, kInputFrameWidth / 4),
                        Field(&SpatialLayer::height, kInputFrameHeight / 4)))));
}

TEST_P(RTCVideoEncoderEncodeTest,
       CreateAndInitVP9SvcSinglecastWithoutTemporalLayers) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  tl_codec.spatialLayers[1].active = false;
  tl_codec.spatialLayers[2].active = false;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  EXPECT_THAT(config_->spatial_layers, IsEmpty());
}

TEST_P(RTCVideoEncoderEncodeTest, RaiseErrorOnMissingEndOfPicture) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/2);
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[1].numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/true, /*timestamp=*/base::Milliseconds(0));
    metadata.key_frame = true;
    metadata.vp9.emplace();
    metadata.vp9->spatial_layer_resolutions = ToResolutionList(tl_codec);
    ASSERT_EQ(metadata.vp9->spatial_layer_resolutions.size(), 2u);
    metadata.vp9->end_of_picture = false;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);

    metadata.key_frame = false;
    metadata.vp9.emplace();
    metadata.vp9->reference_lower_spatial_layers = true;
    // Incorrectly mark last spatial layer with eop = false.
    metadata.vp9->end_of_picture = false;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);
  });
  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types{
      webrtc::VideoFrameType::kVideoFrameKey};
  if (!AsyncEncodingIsEnabled()) {
    EXPECT_NE(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_timestamp_rtp(0)
                                       .set_timestamp_us(0)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types),
              WEBRTC_VIDEO_CODEC_OK);
    return;
  }

  // BitstreamBufferReady() is called after the first Encode() returns.
  // The error is reported on the second call.
  base::WaitableEvent error_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc_encoder_->SetErrorWaiter(&error_waiter);
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  error_waiter.Wait();
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_P(RTCVideoEncoderEncodeTest, RaiseErrorOnMismatchingResolutions) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/2);
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[1].numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/true, /*timestamp=*/base::Milliseconds(0));
    metadata.key_frame = true;
    metadata.vp9.emplace();
    metadata.vp9->spatial_layer_resolutions = {gfx::Size(
        tl_codec.spatialLayers[0].width, tl_codec.spatialLayers[0].height)};
    metadata.vp9->end_of_picture = true;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);
  });

  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types{
      webrtc::VideoFrameType::kVideoFrameKey};

  if (!AsyncEncodingIsEnabled()) {
    EXPECT_NE(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_timestamp_rtp(0)
                                       .set_timestamp_us(0)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types),
              WEBRTC_VIDEO_CODEC_OK);
    return;
  }

  // BitstreamBufferReady() is called after the first Encode() returns.
  // The error is reported on the second call.
  base::WaitableEvent error_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc_encoder_->SetErrorWaiter(&error_waiter);
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  error_waiter.Wait();
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_P(RTCVideoEncoderEncodeTest, SpatialLayerTurnedOffAndOnAgain) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/2);
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[1].numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  // Start with two active spatial layers.
  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/true, /*timestamp=*/base::Milliseconds(0));
    metadata.key_frame = true;
    metadata.vp9.emplace();
    metadata.vp9->spatial_layer_resolutions = ToResolutionList(tl_codec);
    ASSERT_EQ(metadata.vp9->spatial_layer_resolutions.size(), 2u);
    metadata.vp9->end_of_picture = false;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);

    metadata.key_frame = false;
    metadata.vp9.emplace();
    metadata.vp9->reference_lower_spatial_layers = true;
    metadata.vp9->end_of_picture = true;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);
  });
  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types{
      webrtc::VideoFrameType::kVideoFrameKey};
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  RunUntilIdle();

  // Sind bitrate allocation disabling the second spatial layer.
  webrtc::VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 100000);
  EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange);
  rtc_encoder_->SetRates(webrtc::VideoEncoder::RateControlParameters(
      bitrate_allocation, tl_codec.maxFramerate));
  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/false, /*timestamp=*/base::Microseconds(1));
    metadata.vp9.emplace();
    metadata.vp9->inter_pic_predicted = true;
    metadata.vp9->end_of_picture = true;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);
  });
  frame_types[0] = webrtc::VideoFrameType::kVideoFrameDelta;
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(1)
                                     .set_timestamp_us(1)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  RunUntilIdle();

  // Re-enable the top layer.
  bitrate_allocation.SetBitrate(1, 0, 500000);
  EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange);
  rtc_encoder_->SetRates(webrtc::VideoEncoder::RateControlParameters(
      bitrate_allocation, tl_codec.maxFramerate));
  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/false, /*timestamp=*/base::Microseconds(2));
    metadata.vp9.emplace();
    metadata.vp9->inter_pic_predicted = true;
    metadata.vp9->end_of_picture = false;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);

    metadata.vp9.emplace();
    metadata.vp9->inter_pic_predicted = true;
    metadata.vp9->end_of_picture = true;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);
  });
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_timestamp_rtp(2)
                                     .set_timestamp_us(2)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  RunUntilIdle();
}

#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)

TEST_P(RTCVideoEncoderEncodeTest, EncodeFrameWithAdapter) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .Times(2)
      .WillRepeatedly(Invoke(
          [](scoped_refptr<media::VideoFrame> frame, bool force_keyframe) {
            EXPECT_EQ(kInputFrameWidth, frame->visible_rect().width());
            EXPECT_EQ(kInputFrameHeight, frame->visible_rect().height());
          }));

  // Encode first frame: full size. This will pass through to the encoder.
  auto frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(kInputFrameWidth, kInputFrameHeight));
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame, std::vector<scoped_refptr<media::VideoFrame>>(),
          new WebRtcVideoFrameAdapter::SharedResources(nullptr)));
  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));

  // Encode second frame: double size. This will trigger downscale prior to
  // encoder.
  frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(kInputFrameWidth * 2, kInputFrameHeight * 2));
  frame_adapter = new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
      frame, std::vector<scoped_refptr<media::VideoFrame>>(),
      new WebRtcVideoFrameAdapter::SharedResources(nullptr));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_timestamp_rtp(0)
                                     .set_timestamp_us(123456)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}

INSTANTIATE_TEST_SUITE_P(SyncAndAsynEncoding,
                         RTCVideoEncoderEncodeTest,
                         Values(false, true));

}  // namespace blink
