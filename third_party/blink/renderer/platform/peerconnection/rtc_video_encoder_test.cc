// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
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
#include "media/base/mock_filters.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/capture/capture_switches.h"
#include "media/mojo/clients/mock_mojo_video_encoder_metrics_provider_factory.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/blink/renderer/platform/webrtc/testing/mock_webrtc_video_frame_adapter_shared_resources.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/common_video/include/video_frame_buffer.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "third_party/webrtc/rtc_base/time_utils.h"
#if BUILDFLAG(RTC_USE_H265)
#include "third_party/blink/renderer/platform/peerconnection/h265_parameter_sets_tracker.h"
#endif

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::WithArgs;

using SpatialLayer = media::VideoEncodeAccelerator::Config::SpatialLayer;
using Type = webrtc::VideoFrameBuffer::Type;

namespace blink {

namespace {

const int kInputFrameFillY = 12;
const int kInputFrameFillU = 23;
const int kInputFrameFillV = 34;
// 360p is a valid HW resolution (unless `kForcingSoftwareIncludes360` is
// enabled).
const uint16_t kInputFrameWidth = 480;
const uint16_t kInputFrameHeight = 360;
const uint16_t kStartBitrate = 100;

#if !BUILDFLAG(IS_ANDROID)
// Less than 360p should result in SW fallback.
const uint16_t kSoftwareFallbackInputFrameWidth = 479;
const uint16_t kSoftwareFallbackInputFrameHeight = 359;
#endif

constexpr size_t kDefaultEncodedPayloadSize = 100;

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

class FakeNativeBufferI420 : public blink::WebRtcVideoFrameAdapter {
 public:
  FakeNativeBufferI420(int width, int height, bool allow_to_i420)
      : blink::WebRtcVideoFrameAdapter(
            media::VideoFrame::CreateBlackFrame(gfx::Size(480, 360))),
        width_(width),
        height_(height),
        allow_to_i420_(allow_to_i420) {}

  Type type() const override { return Type::kNative; }
  int width() const override { return width_; }
  int height() const override { return height_; }

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    if (allow_to_i420_) {
      return webrtc::I420Buffer::Create(width_, height_);
    } else {
      return nullptr;
    }
  }

  scoped_refptr<media::VideoFrame> getMediaVideoFrame() const override {
    const gfx::Size kSize360p(480, 360);
    const gfx::Rect kRect360p(0, 0, 480, 360);

    // The strictness of the mock ensures zero copy.
    auto resources =
        base::MakeRefCounted<testing::StrictMock<MockSharedResources>>();

    return CreateTestFrame(kSize360p, kRect360p, kSize360p,
                           media::VideoFrame::STORAGE_OWNED_MEMORY,
                           media::VideoPixelFormat::PIXEL_FORMAT_NV12,
                           base::TimeDelta());
  }

 private:
  const int width_;
  const int height_;
  const bool allow_to_i420_;
};

class RTCVideoEncoderWrapper : public webrtc::VideoEncoder {
 public:
  static std::unique_ptr<RTCVideoEncoderWrapper> Create(
      media::VideoCodecProfile profile,
      bool is_constrained_h264,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
          encoder_metrics_provider_factory) {
    auto wrapper = base::WrapUnique(new RTCVideoEncoderWrapper);
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    wrapper->task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<RTCVideoEncoder>* rtc_video_encoder,
               media::VideoCodecProfile profile, bool is_constrained_h264,
               media::GpuVideoAcceleratorFactories* gpu_factories,
               scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
                   encoder_metrics_provider_factory,
               base::WaitableEvent* waiter) {
              *rtc_video_encoder = std::make_unique<RTCVideoEncoder>(
                  profile, is_constrained_h264, gpu_factories,
                  std::move(encoder_metrics_provider_factory));
              waiter->Signal();
            },
            &wrapper->rtc_video_encoder_, profile, is_constrained_h264,
            gpu_factories, std::move(encoder_metrics_provider_factory),
            &waiter));
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

#if BUILDFLAG(RTC_USE_H265)
  void SetH265ParameterSetsTracker(
      std::unique_ptr<H265ParameterSetsTracker> tracker) {
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](RTCVideoEncoder* rtc_video_encoder,
               std::unique_ptr<H265ParameterSetsTracker> tracker,
               base::WaitableEvent* waiter) {
              rtc_video_encoder->SetH265ParameterSetsTrackerForTesting(
                  std::move(tracker));
              waiter->Signal();
            },
            rtc_video_encoder_.get(), std::move(tracker), &waiter));
    waiter.Wait();
  }
#endif

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

MATCHER_P3(CheckConfig,
           pixel_format,
           storage_type,
           drop_frame,
           "Check pixel format, storage type and drop frame in VEAConfig") {
  return arg.input_format == pixel_format && arg.storage_type == storage_type &&
         (arg.drop_frame_thresh_percentage > 0) == drop_frame;
}

MATCHER_P(CheckStatusCode, code, "Check the code of media::EncoderStatusCode") {
  return arg.code() == code;
}

class RTCVideoEncoderTest {
 public:
  RTCVideoEncoderTest()
      : encoder_thread_("vea_thread"),
        mock_gpu_factories_(
            new media::MockGpuVideoAcceleratorFactories(nullptr)),
        mock_encoder_metrics_provider_factory_(
            base::MakeRefCounted<
                media::MockMojoVideoEncoderMetricsProviderFactory>(
                media::mojom::VideoEncoderUseCase::kWebRTC)) {
    ON_CALL(*mock_encoder_metrics_provider_factory_,
            CreateVideoEncoderMetricsProvider())
        .WillByDefault(Return(::testing::ByMove(
            std::make_unique<media::MockVideoEncoderMetricsProvider>())));
  }

  void ExpectCreateInitAndDestroyVEA(
      media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420,
      media::VideoEncodeAccelerator::Config::StorageType storage_type =
          media::VideoEncodeAccelerator::Config::StorageType::kShmem,
      bool drop_frame = false) {
    // The VEA will be owned by the RTCVideoEncoder once
    // factory.CreateVideoEncodeAccelerator() is called.
    mock_vea_ = new media::MockVideoEncodeAccelerator();

    EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
        .WillRepeatedly(Return(mock_vea_.get()));
    EXPECT_CALL(
        *mock_vea_,
        Initialize(CheckConfig(pixel_format, storage_type, drop_frame), _, _))
        .WillOnce(Invoke(this, &RTCVideoEncoderTest::Initialize));
    EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer).Times(AtLeast(3));
    EXPECT_CALL(*mock_vea_, Destroy()).Times(1);
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
    rtc_encoder_.reset();
    encoder_thread_.task_runner()->ReleaseSoon(
        FROM_HERE, std::move(mock_encoder_metrics_provider_factory_));
    RunUntilIdle();
    encoder_thread_.Stop();
  }

  void RunUntilIdle() {
    DVLOG(3) << __func__;
    encoder_thread_.FlushForTesting();
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
#if BUILDFLAG(RTC_USE_H265)
      case webrtc::kVideoCodecH265:
        media_profile = media::HEVCPROFILE_MAIN;
        break;
#endif
      default:
        ADD_FAILURE() << "Unexpected codec type: " << codec_type;
        media_profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
    }

    rtc_encoder_ = RTCVideoEncoderWrapper::Create(
        media_profile, false, mock_gpu_factories_.get(),
        mock_encoder_metrics_provider_factory_);
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
        for (size_t sid = 0; sid < num_spatial_layers; ++sid) {
          const int denom = 1 << (num_spatial_layers_ - (sid + 1));
          webrtc::SpatialLayer& sl = codec.spatialLayers[sid];
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
        NOTREACHED_IN_MIGRATION();
    }
    return codec;
  }

  webrtc::VideoCodec GetSimulcastCodec(webrtc::VideoCodecType codec_type,
                                       size_t num_simulcast_streams) {
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
    codec.numberOfSimulcastStreams = num_simulcast_streams;
    codec.mode = webrtc::VideoCodecMode::kRealtimeVideo;
    switch (codec_type) {
      case webrtc::kVideoCodecVP9: {
        webrtc::VideoCodecVP9& vp9 = *codec.VP9();
        vp9.numberOfTemporalLayers = 3;
        vp9.numberOfSpatialLayers = 1;
        num_spatial_layers_ = 3;
        for (size_t sid = 0; sid < num_simulcast_streams; ++sid) {
          const int denom = 1 << (num_simulcast_streams - (sid + 1));
          webrtc::SimulcastStream& sl = codec.simulcastStream[sid];
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
        NOTREACHED_IN_MIGRATION();
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
              frame->visible_data(media::VideoFrame::Plane::kY)[0]);
    EXPECT_EQ(kInputFrameFillU,
              frame->visible_data(media::VideoFrame::Plane::kU)[0]);
    EXPECT_EQ(kInputFrameFillV,
              frame->visible_data(media::VideoFrame::Plane::kV)[0]);
  }

  void DropFrame(scoped_refptr<media::VideoFrame> frame, bool force_keyframe) {
    CHECK(!force_keyframe);
    client_->BitstreamBufferReady(
        0,
        media::BitstreamBufferMetadata::CreateForDropFrame(frame->timestamp()));
  }
  void ReturnSvcFramesThatShouldBeDropped(
      scoped_refptr<media::VideoFrame> frame,
      bool force_keyframe) {
    CHECK(!force_keyframe);
    for (size_t sid = 0; sid < num_spatial_layers_; ++sid) {
      const bool end_of_picture = sid + 1 == num_spatial_layers_;
      client_->BitstreamBufferReady(
          sid, media::BitstreamBufferMetadata::CreateForDropFrame(
                   frame->timestamp(), sid, end_of_picture));
    }
  }
  void ReturnFrameWithTimeStamp(scoped_refptr<media::VideoFrame> frame,
                                bool force_keyframe) {
    client_->BitstreamBufferReady(
        0, media::BitstreamBufferMetadata(kDefaultEncodedPayloadSize,
                                          force_keyframe, frame->timestamp()));
  }

  void FlushComplete(media::VideoEncodeAccelerator::FlushCallback callback) {
    std::move(callback).Run(true);
  }

  void FlushFailure(media::VideoEncodeAccelerator::FlushCallback callback) {
    std::move(callback).Run(false);
  }

  void ReturnSVCLayerFrameWithVp9Metadata(
      scoped_refptr<media::VideoFrame> frame,
      bool force_keyframe) {
    const size_t frame_num = return_svc_layer_frame_times_;
    CHECK(0 <= frame_num && frame_num <= 4);
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

      const bool end_of_picture = sid + 1 == num_spatial_layers_;
      media::BitstreamBufferMetadata metadata(
          100u /* payload_size_bytes */, force_keyframe, frame->timestamp());

      // Assume k-SVC encoding.
      metadata.key_frame = frame_num == 0 && sid == 0;
      vp9.end_of_picture = end_of_picture;
      vp9.spatial_idx = sid;
      vp9.reference_lower_spatial_layers = frame_num == 0 && sid != 0;
      vp9.referenced_by_upper_spatial_layers =
          frame_num == 0 && (sid + 1 != num_spatial_layers_);
      if (metadata.key_frame) {
        for (size_t i = 0; i < num_spatial_layers_; ++i) {
          const int denom = 1 << (num_spatial_layers_ - (i + 1));
          vp9.spatial_layer_resolutions.emplace_back(
              gfx::Size(frame->coded_size().width() / denom,
                        frame->coded_size().height() / denom));
        }
        vp9.begin_active_spatial_layer_index = 0;
        vp9.end_active_spatial_layer_index = num_spatial_layers_;
      }
      metadata.vp9 = vp9;
      client_->BitstreamBufferReady(sid, metadata);
    }

    return_svc_layer_frame_times_ += 1;
  }

  void ResetSVCLayerFrameTimes() { return_svc_layer_frame_times_ = 0; }

  void VerifyTimestamp(uint32_t rtp_timestamp,
                       int64_t capture_time_ms,
                       const webrtc::EncodedImage& encoded_image,
                       const webrtc::CodecSpecificInfo* codec_specific_info) {
    DVLOG(3) << __func__;
    EXPECT_EQ(rtp_timestamp, encoded_image.RtpTimestamp());
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
  raw_ptr<media::MockVideoEncodeAccelerator, DanglingUntriaged> mock_vea_;
  std::unique_ptr<RTCVideoEncoderWrapper> rtc_encoder_;
  std::optional<media::VideoEncodeAccelerator::Config> config_;
  raw_ptr<media::VideoEncodeAccelerator::Client, DanglingUntriaged> client_;
  base::Thread encoder_thread_;

  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  scoped_refptr<media::MockMojoVideoEncoderMetricsProviderFactory>
      mock_encoder_metrics_provider_factory_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<EncodedImageCallbackWrapper> callback_wrapper_;
  size_t num_spatial_layers_;
  size_t return_svc_layer_frame_times_ = 0;
};

class RTCVideoEncoderInitTest
    : public RTCVideoEncoderTest,
      public ::testing::TestWithParam<webrtc::VideoCodecType> {
 public:
  RTCVideoEncoderInitTest() {
    std::vector<base::test::FeatureRef> enabled_features;
  }
  ~RTCVideoEncoderInitTest() override = default;
  void SetUp() override { RTCVideoEncoderTest::SetUp(); }
  void TearDown() override { RTCVideoEncoderTest::TearDown(); }
};

TEST_P(RTCVideoEncoderInitTest, CreateAndInitSucceeds) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_P(RTCVideoEncoderInitTest, RepeatedInitSucceeds) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
              CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(::testing::ByMove(
          std::make_unique<media::MockVideoEncoderMetricsProvider>())));
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
  EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
              CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(::testing::ByMove(
          std::make_unique<media::MockVideoEncoderMetricsProvider>())));
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

// Software fallback for low resolution is not applicable on Android.
#if !BUILDFLAG(IS_ANDROID)

TEST_P(RTCVideoEncoderInitTest, SoftwareFallbackForLowResolution) {
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.width = kSoftwareFallbackInputFrameWidth;
  codec.height = kSoftwareFallbackInputFrameHeight;
  codec.codecType = codec_type;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_P(RTCVideoEncoderInitTest, SoftwareFallbackForLowResolutionIncludes360p) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kForcingSoftwareIncludes360);
  const webrtc::VideoCodecType codec_type = GetParam();
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.width = kInputFrameWidth;
  codec.height = kInputFrameHeight;
  codec.codecType = codec_type;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

#endif

TEST_P(RTCVideoEncoderInitTest, CreateAndInitSucceedsForTemporalLayer) {
  const webrtc::VideoCodecType codec_type = GetParam();
  if (codec_type == webrtc::kVideoCodecVP8)
    GTEST_SKIP() << "VP8 temporal layer encoding is not supported";
  if (codec_type == webrtc::kVideoCodecH264)
    GTEST_SKIP() << "H264 temporal layer encoding is not supported";

  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(codec_type,
                                                 /*num_spatial_layers=*/1);
  CreateEncoder(tl_codec.codecType);
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
}

const webrtc::VideoCodecType kInitTestCases[] = {
    webrtc::kVideoCodecH264,
    webrtc::kVideoCodecVP9,
    webrtc::kVideoCodecVP8,
};

INSTANTIATE_TEST_SUITE_P(InitTimingAndCodecProfiles,
                         RTCVideoEncoderInitTest,
                         ValuesIn(kInitTestCases));

class RTCVideoEncoderEncodeTest : public RTCVideoEncoderTest,
                                  public ::testing::Test {
 public:
  RTCVideoEncoderEncodeTest() : RTCVideoEncoderEncodeTest(false) {}

  explicit RTCVideoEncoderEncodeTest(
      bool enable_keep_encoder_instance_on_release) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kZeroCopyTabCapture,
    };
    if (enable_keep_encoder_instance_on_release) {
      enabled_features.push_back(features::kKeepEncoderInstanceOnRelease);
    }
    enabled_features.push_back(media::kWebRTCHardwareVideoEncoderFrameDrop);
    feature_list_.InitWithFeatures(enabled_features,
                                   /*disabled_features=*/{});
  }

  ~RTCVideoEncoderEncodeTest() override = default;
  void SetUp() override { RTCVideoEncoderTest::SetUp(); }
  void TearDown() override { RTCVideoEncoderTest::TearDown(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class RTCVideoEncoderFrameSizeChangeTest : public RTCVideoEncoderEncodeTest {
 public:
  RTCVideoEncoderFrameSizeChangeTest() : RTCVideoEncoderEncodeTest(true) {}

  void ExpectFrameSizeChange(const gfx::Size& expected_size) {
    // potentially validate bitrate and framerate
    EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange(
                                _, _, std::optional<gfx::Size>(expected_size)))
        .WillOnce(Invoke([this, expected_size](
                             const media::Bitrate& bitrate, uint32_t framerate,
                             const std::optional<gfx::Size>& size) {
          EXPECT_EQ(size, expected_size);
          client_->RequireBitstreamBuffers(3, expected_size,
                                           expected_size.GetArea());
        }));
    EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer).Times(AtLeast(3));
  }

  void SetUpEncodingWithFrameSizeChangeSupport(
      const webrtc::VideoCodec& codec) {
    ExpectCreateInitAndDestroyVEA();
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

    EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange(
                                _, _, std::optional<gfx::Size>()));
    webrtc::VideoBitrateAllocation bitrate_allocation;
    bitrate_allocation.SetBitrate(1, 0, 500000);
    rtc_encoder_->SetRates(webrtc::VideoEncoder::RateControlParameters(
        bitrate_allocation, codec.maxFramerate));

      // report frame size change support
      media::VideoEncoderInfo info;
      info.supports_frame_size_change = true;
      encoder_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &media::VideoEncodeAccelerator::Client::NotifyEncoderInfoChange,
              base::Unretained(client_), info));

      for (int i = 0; i < kFramesToEncodeBeforeFrameSizeChange; i++) {
        const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
            webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
        FillFrameBuffer(buffer);
        std::vector<webrtc::VideoFrameType> frame_types;
        frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);

        webrtc::VideoFrame rtc_frame =
            webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(buffer)
                .set_rtp_timestamp(i)
                .set_timestamp_us(i)
                .set_rotation(webrtc::kVideoRotation_0)
                .build();
        base::WaitableEvent event;
        EXPECT_CALL(*mock_vea_, Encode)
            .WillOnce(
                Invoke([this, &event](scoped_refptr<media::VideoFrame> frame,
                                      bool force_keyframe) {
                  client_->BitstreamBufferReady(
                      0, media::BitstreamBufferMetadata(0, force_keyframe,
                                                        frame->timestamp()));
                  event.Signal();
                }));

        EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                  rtc_encoder_->Encode(rtc_frame, &frame_types));
        event.Wait();
      }
  }

  ~RTCVideoEncoderFrameSizeChangeTest() override = default;

 protected:
  const int kFramesToEncodeBeforeFrameSizeChange = 3;
};

TEST_F(RTCVideoEncoderEncodeTest, H264SoftwareFallbackForOddSize) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecH264;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  codec.width = kInputFrameWidth - 1;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_F(RTCVideoEncoderEncodeTest, VP8CreateAndInitSucceedsForOddSize) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  codec.width = kInputFrameWidth - 1;
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_F(RTCVideoEncoderEncodeTest, VP9CreateAndInitSucceedsForOddSize) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  codec.width = kInputFrameWidth - 1;
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_F(RTCVideoEncoderEncodeTest, VP9SoftwareFallbackForVEANotSupport) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/1);
  CreateEncoder(tl_codec.codecType);
  media::VideoEncodeAccelerator::SupportedProfiles profiles = {
      {media::VP9PROFILE_PROFILE0,
       /*max_resolution*/ gfx::Size(1920, 1088),
       /*max_framerate_numerator*/ 30,
       /*max_framerate_denominator*/ 1,
       media::VideoEncodeAccelerator::kConstantMode,
       {media::SVCScalabilityMode::kL1T1}}};
  EXPECT_CALL(*mock_gpu_factories_.get(),
              GetVideoEncodeAcceleratorSupportedProfiles())
      .WillOnce(Return(profiles));
  // The mock gpu factories return |profiles| as VEA supported profiles, which
  // only support VP9 single layer acceleration. When requesting VP9 SVC
  // encoding, InitEncode() will fail in scalability mode check and return
  // WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
}

TEST_F(RTCVideoEncoderEncodeTest, ClearSetErrorRequestWhenInitNewEncoder) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;

  mock_vea_ = new media::MockVideoEncodeAccelerator();
  EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
      .WillOnce(Return(mock_vea_.get()));
  media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
  media::VideoEncodeAccelerator::Config::StorageType storage_type =
      media::VideoEncodeAccelerator::Config::StorageType::kShmem;
  bool drop_frame = false;
  EXPECT_CALL(
      *mock_vea_,
      Initialize(CheckConfig(pixel_format, storage_type, drop_frame), _, _))
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::Initialize));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  // Notify error status to rtc video encoder.
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&media::VideoEncodeAccelerator::Client::NotifyErrorStatus,
                     base::Unretained(client_),
                     media::EncoderStatus::Codes::kEncoderFailedEncode));

  auto* mock_vea_new = new media::MockVideoEncodeAccelerator();
  EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
      .WillOnce(Return(mock_vea_new));
  EXPECT_CALL(
      *mock_vea_new,
      Initialize(CheckConfig(pixel_format, storage_type, drop_frame), _, _))
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::Initialize));
  auto encoder_metrics_provider =
      std::make_unique<media::MockVideoEncoderMetricsProvider>();
  EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
              CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(::testing::ByMove(std::move(encoder_metrics_provider))));
  // When InitEncode() is called again, RTCVideoEncoder will release current
  // impl_ and create a new instance, the set error request from a released
  // impl_ is regarded as invalid and should be rejected.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
  // If the invalid set error request is rejected as expected, Encode() will
  // return with WEBRTC_VIDEO_CODEC_OK.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  rtc_encoder_->Release();
}

// Checks that WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE is returned when there is
// platform error.
TEST_F(RTCVideoEncoderEncodeTest, SoftwareFallbackAfterError) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .WillOnce(Invoke([this](scoped_refptr<media::VideoFrame>, bool) {
        encoder_thread_.task_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &media::VideoEncodeAccelerator::Client::NotifyErrorStatus,
                base::Unretained(client_),
                media::EncoderStatus::Codes::kEncoderFailedEncode));
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
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  error_waiter.Wait();
  // Expect the next frame to return SW fallback.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}

// On Windows we allow native input that is mappable.
#if BUILDFLAG(IS_WIN)
TEST_F(RTCVideoEncoderEncodeTest, NoSoftwareFallbackOnMappableNativeInput) {
  // Make RTCVideoEncoder expect native input.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kVideoCaptureUseGpuMemoryBuffer);

  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecH264;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  ExpectCreateInitAndDestroyVEA(
      media::PIXEL_FORMAT_NV12,
      media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> mapped_buffer(
      rtc::make_ref_counted<FakeNativeBufferI420>(480, 360,
                                                  /*allow_to_i420=*/false));

  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(mapped_buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(RTCVideoEncoderEncodeTest, SoftwareFallbackOnBadEncodeInput) {
  // Make RTCVideoEncoder expect native input.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kVideoCaptureUseGpuMemoryBuffer);

  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  ExpectCreateInitAndDestroyVEA(
      media::PIXEL_FORMAT_NV12,
      media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer);
  ASSERT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  auto frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(kInputFrameWidth, kInputFrameHeight));
  frame->set_timestamp(base::Milliseconds(1));
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame, base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(
                     nullptr)));
  std::vector<webrtc::VideoFrameType> frame_types;

  // The frame type check is done in media thread asynchronously. The error is
  // reported in the second Encode callback.
  base::WaitableEvent error_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc_encoder_->SetErrorWaiter(&error_waiter);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_rtp_timestamp(1000)
                                     .set_timestamp_us(2000)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  error_waiter.Wait();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_rtp_timestamp(2000)
                                     .set_timestamp_us(3000)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}

TEST_F(RTCVideoEncoderEncodeTest, EncodeScaledFrame) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  ExpectCreateInitAndDestroyVEA();
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
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));

  const rtc::scoped_refptr<webrtc::I420Buffer> upscaled_buffer =
      webrtc::I420Buffer::Create(2 * kInputFrameWidth, 2 * kInputFrameHeight);
  FillFrameBuffer(upscaled_buffer);
  webrtc::VideoFrame rtc_frame = webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(upscaled_buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(123456)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(rtc_frame, &frame_types));
}

TEST_F(RTCVideoEncoderEncodeTest, PreserveTimestamps) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  ExpectCreateInitAndDestroyVEA();
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
                                     .set_rtp_timestamp(rtp_timestamp)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build();
  rtc_frame.set_timestamp_us(capture_time_ms * rtc::kNumMicrosecsPerMillisec);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(rtc_frame, &frame_types));
}

TEST_F(RTCVideoEncoderEncodeTest, AcceptsRepeatedWrappedMediaVideoFrame) {
  // Ensure encoder is accepting subsequent frames with the same timestamp in
  // the wrapped media::VideoFrame.
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  ExpectCreateInitAndDestroyVEA();
  rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings);

  auto frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(kInputFrameWidth, kInputFrameHeight));
  frame->set_timestamp(base::Milliseconds(1));
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame, base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(
                     nullptr)));
  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_rtp_timestamp(1000)
                                     .set_timestamp_us(2000)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_rtp_timestamp(2000)
                                     .set_timestamp_us(3000)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}

TEST_F(RTCVideoEncoderEncodeTest, EncodeVP9TemporalLayer) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/1);
  CreateEncoder(tl_codec.codecType);
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  size_t kNumEncodeFrames = 5u;
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    base::WaitableEvent event;
    if (i > 0) {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_)).Times(1);
    }
    EXPECT_CALL(*mock_vea_, Encode(_, _))
        .WillOnce(DoAll(
            Invoke(this,
                   &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata),
            [&event]() { event.Signal(); }));
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
}

TEST_F(RTCVideoEncoderEncodeTest, EncodeWithDropFrame) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.SetFrameDropEnabled(/*enabled=*/true);
  ExpectCreateInitAndDestroyVEA(
      media::PIXEL_FORMAT_I420,
      media::VideoEncodeAccelerator::Config::StorageType::kShmem, true);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  constexpr static size_t kNumEncodeFrames = 10u;
  constexpr static size_t kDropIndices[] = {3, 4, 7};
  class DropFrameVerifier : public webrtc::EncodedImageCallback {
   public:
    DropFrameVerifier() = default;
    ~DropFrameVerifier() override = default;

    void OnDroppedFrame(DropReason reason) override {
      AddResult(EncodeResult::kDropped);
    }

    webrtc::EncodedImageCallback::Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info) override {
      if (codec_specific_info->end_of_picture) {
        AddResult(EncodeResult::kEncoded);
      }
      return Result(Result::OK);
    }

    void Verify() {
      base::AutoLock auto_lock(lock_);
      ASSERT_EQ(encode_results_.size(), kNumEncodeFrames);
      for (size_t i = 0; i < kNumEncodeFrames; ++i) {
        EncodeResult expected = EncodeResult::kEncoded;
        if (base::Contains(kDropIndices, i)) {
          expected = EncodeResult::kDropped;
        }
        EXPECT_EQ(encode_results_[i], expected);
      }
    }

   private:
    enum class EncodeResult {
      kEncoded,
      kDropped,
    };

    void AddResult(EncodeResult result) {
      base::AutoLock auto_lock(lock_);
      encode_results_.push_back(result);
    }

    base::Lock lock_;
    std::vector<EncodeResult> encode_results_ GUARDED_BY(lock_);
  };

  DropFrameVerifier dropframe_verifier;
  rtc_encoder_->RegisterEncodeCompleteCallback(&dropframe_verifier);
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    base::WaitableEvent event;
    if (i > 0) {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_)).Times(1);
    }
    if (base::Contains(kDropIndices, i)) {
      EXPECT_CALL(*mock_vea_, Encode)
          .WillOnce(DoAll(Invoke(this, &RTCVideoEncoderTest::DropFrame),
                          [&event]() { event.Signal(); }));
    } else {
      EXPECT_CALL(*mock_vea_, Encode)
          .WillOnce(DoAll(
              Invoke(this, &RTCVideoEncoderTest::ReturnFrameWithTimeStamp),
              [&event]() { event.Signal(); }));
    }

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
  RunUntilIdle();
  dropframe_verifier.Verify();
  rtc_encoder_.reset();
}

TEST_F(RTCVideoEncoderEncodeTest, InitializeWithTooHighBitrateFails) {
  // We expect initialization to fail. We do not want a mock video encoder, as
  // it will not be successfully attached to the rtc_encoder_. So we do not call
  // CreateEncoder, but instead CreateEncoderWithoutVea.
  constexpr webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);

  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  codec.startBitrate = std::numeric_limits<uint32_t>::max() / 100;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_PARAMETER,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)
//  Currently we only test spatial SVC encoding on CrOS since only CrOS platform
//  support spatial SVC encoding.

// http://crbug.com/1226875
TEST_F(RTCVideoEncoderEncodeTest, EncodeSpatialLayer) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  constexpr size_t kNumSpatialLayers = 3;
  webrtc::VideoCodec sl_codec =
      GetSVCLayerCodec(webrtc::kVideoCodecVP9, kNumSpatialLayers);
  ExpectCreateInitAndDestroyVEA();
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
        const size_t num_spatial_layers = codec_->VP9().numberOfSpatialLayers;
        const auto& vp9_specific = codec_specific_info->codecSpecific.VP9;
        EXPECT_EQ(vp9_specific.num_spatial_layers, num_spatial_layers);
        for (size_t i = 0; i < num_spatial_layers; ++i) {
          EXPECT_EQ(vp9_specific.width[i], codec_->spatialLayers[i].width);
          EXPECT_EQ(vp9_specific.height[i], codec_->spatialLayers[i].height);
        }
      }

      if (encoded_image.RtpTimestamp() == kNumEncodeFrames - 1 &&
          codec_specific_info->end_of_picture) {
        waiter_.Signal();
      }

      if (encoded_image.TemporalIndex().has_value()) {
        EXPECT_EQ(encoded_image.TemporalIndex(),
                  codec_specific_info->codecSpecific.VP9.temporal_idx);
      }

      return Result(Result::OK);
    }

    void Wait() { waiter_.Wait(); }

   private:
    const raw_ref<const webrtc::VideoCodec> codec_;
    base::WaitableEvent waiter_;
  };
  CodecSpecificVerifier sl_verifier(sl_codec);
  rtc_encoder_->RegisterEncodeCompleteCallback(&sl_verifier);
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0)
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    base::WaitableEvent event;
    if (i > 0) {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_))
          .Times(kNumSpatialLayers);
    }
    EXPECT_CALL(*mock_vea_, Encode)
        .WillOnce(DoAll(
            Invoke(this,
                   &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata),
            [&event]() { event.Signal(); }));
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(i)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
  sl_verifier.Wait();
  RunUntilIdle();
}

TEST_F(RTCVideoEncoderEncodeTest, EncodeSpatialLayerWithDropFrame) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  constexpr size_t kNumSpatialLayers = 3;
  webrtc::VideoCodec sl_codec =
      GetSVCLayerCodec(webrtc::kVideoCodecVP9, kNumSpatialLayers);
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&sl_codec, kVideoEncoderSettings));

  constexpr static size_t kNumEncodeFrames = 5u;
  constexpr static size_t kDropIndices[] = {1, 3};
  class DropFrameVerifier : public webrtc::EncodedImageCallback {
   public:
    DropFrameVerifier() = default;
    ~DropFrameVerifier() override = default;

    void OnDroppedFrame(DropReason reason) override {
      AddResult(EncodeResult::kDropped);
    }

    webrtc::EncodedImageCallback::Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info) override {
      if (codec_specific_info->end_of_picture) {
        AddResult(EncodeResult::kEncoded);
      }
      return Result(Result::OK);
    }

    void Verify() {
      base::AutoLock auto_lock(lock_);
      ASSERT_EQ(encode_results_.size(), kNumEncodeFrames);
      for (size_t i = 0; i < kNumEncodeFrames; ++i) {
        EncodeResult expected = EncodeResult::kEncoded;
        if (base::Contains(kDropIndices, i)) {
          expected = EncodeResult::kDropped;
        }
        EXPECT_EQ(encode_results_[i], expected);
      }
    }

   private:
    enum class EncodeResult {
      kEncoded,
      kDropped,
    };

    void AddResult(EncodeResult result) {
      base::AutoLock auto_lock(lock_);
      encode_results_.push_back(result);
    }

    base::Lock lock_;
    std::vector<EncodeResult> encode_results_ GUARDED_BY(lock_);
  };
  DropFrameVerifier dropframe_verifier;
  rtc_encoder_->RegisterEncodeCompleteCallback(&dropframe_verifier);
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    base::WaitableEvent event;
    if (i > 0) {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_))
          .Times(kNumSpatialLayers);
    }
    if (base::Contains(kDropIndices, i)) {
      EXPECT_CALL(*mock_vea_, Encode)
          .WillOnce(DoAll(
              Invoke(this,
                     &RTCVideoEncoderTest::ReturnSvcFramesThatShouldBeDropped),
              [&event]() { event.Signal(); }));
    } else {
      EXPECT_CALL(*mock_vea_, Encode)
          .WillOnce(DoAll(
              Invoke(this,
                     &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata),
              [&event]() { event.Signal(); }));
    }
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(i)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
  RunUntilIdle();
  dropframe_verifier.Verify();
  rtc_encoder_.reset();
}

TEST_F(RTCVideoEncoderEncodeTest, CreateAndInitVP9ThreeLayerSvc) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  CreateEncoder(tl_codec.codecType);

    ExpectCreateInitAndDestroyVEA();
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

TEST_F(RTCVideoEncoderEncodeTest, CreateAndInitVP9SvcSinglecast) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  tl_codec.spatialLayers[1].active = false;
  tl_codec.spatialLayers[2].active = false;
  CreateEncoder(tl_codec.codecType);

  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  EXPECT_THAT(*config_,
              Field(&media::VideoEncodeAccelerator::Config::spatial_layers,
                    ElementsAre(AllOf(
                        Field(&SpatialLayer::width, kInputFrameWidth / 4),
                        Field(&SpatialLayer::height, kInputFrameHeight / 4)))));
}

TEST_F(RTCVideoEncoderEncodeTest,
       CreateAndInitVP9SvcSinglecastWithoutTemporalLayers) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  tl_codec.spatialLayers[1].active = false;
  tl_codec.spatialLayers[2].active = false;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);

    ExpectCreateInitAndDestroyVEA();
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
    EXPECT_THAT(config_->spatial_layers, IsEmpty());
}

TEST_F(RTCVideoEncoderEncodeTest,
       CreateAndInitVP9ThreeLayerSvcWithTopLayerInactive) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  tl_codec.spatialLayers[2].active = false;
  CreateEncoder(tl_codec.codecType);

    ExpectCreateInitAndDestroyVEA();
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
    EXPECT_THAT(
        *config_,
        Field(&media::VideoEncodeAccelerator::Config::spatial_layers,
              ElementsAre(
                  AllOf(Field(&SpatialLayer::width, kInputFrameWidth / 4),
                        Field(&SpatialLayer::height, kInputFrameHeight / 4)),
                  AllOf(Field(&SpatialLayer::width, kInputFrameWidth / 2),
                        Field(&SpatialLayer::height, kInputFrameHeight / 2)))));
    EXPECT_THAT(
        *config_,
        Field(&media::VideoEncodeAccelerator::Config::input_visible_size,
              AllOf(Property(&gfx::Size::width, kInputFrameWidth / 2),
                    Property(&gfx::Size::height, kInputFrameHeight / 2))));
}

TEST_F(RTCVideoEncoderEncodeTest, RaiseErrorOnMissingEndOfPicture) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/2);
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[1].numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/true,
        /*timestamp=*/base::Milliseconds(0));
    metadata.key_frame = true;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = false;
    metadata.vp9->spatial_idx = 0;
    metadata.vp9->spatial_layer_resolutions = ToResolutionList(tl_codec);
    ASSERT_EQ(metadata.vp9->spatial_layer_resolutions.size(), 2u);
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);

    metadata.key_frame = false;

    metadata.vp9.emplace();
    // Incorrectly mark last spatial layer with eop = false.
    metadata.vp9->end_of_picture = false;
    metadata.vp9->spatial_idx = 1;
    metadata.vp9->reference_lower_spatial_layers = true;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);
  });
  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types{
      webrtc::VideoFrameType::kVideoFrameKey};

  // BitstreamBufferReady() is called after the first Encode() returns.
  // The error is reported on the second call.
  base::WaitableEvent error_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc_encoder_->SetErrorWaiter(&error_waiter);

  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  error_waiter.Wait();
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_F(RTCVideoEncoderEncodeTest, RaiseErrorOnMismatchingResolutions) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/2);
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[1].numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/true,
        /*timestamp=*/base::Milliseconds(0));
    metadata.key_frame = true;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = false;
    metadata.vp9->spatial_layer_resolutions = {gfx::Size(
        tl_codec.spatialLayers[0].width, tl_codec.spatialLayers[0].height)};
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);
  });

  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types{
      webrtc::VideoFrameType::kVideoFrameKey};

  // BitstreamBufferReady() is called after the first Encode() returns.
  // The error is reported on the second call.
  base::WaitableEvent error_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc_encoder_->SetErrorWaiter(&error_waiter);
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  error_waiter.Wait();
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_F(RTCVideoEncoderEncodeTest, SpatialLayerTurnedOffAndOnAgain) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/2);
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[1].numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  // Start with two active spatial layers.
  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/true,
        /*timestamp=*/base::Milliseconds(0));
    metadata.key_frame = true;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = false;
    metadata.vp9->spatial_idx = 0;
    metadata.vp9->spatial_layer_resolutions = ToResolutionList(tl_codec);
    ASSERT_EQ(metadata.vp9->spatial_layer_resolutions.size(), 2u);
    metadata.vp9->begin_active_spatial_layer_index = 0;
    metadata.vp9->end_active_spatial_layer_index = 2;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);

    metadata.key_frame = false;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = true;
    metadata.vp9->spatial_idx = 1;
    metadata.vp9->reference_lower_spatial_layers = true;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);
  });
  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types{
      webrtc::VideoFrameType::kVideoFrameKey};
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  RunUntilIdle();

  // Sind bitrate allocation disabling the top spatial layer.
  webrtc::VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 100000);
  EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange);
  rtc_encoder_->SetRates(webrtc::VideoEncoder::RateControlParameters(
      bitrate_allocation, tl_codec.maxFramerate));
  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        100u /* payload_size_bytes */,
        /*keyframe=*/true,
        /*timestamp=*/base::Microseconds(1));
    metadata.vp9.emplace();
    metadata.vp9->spatial_idx = 0;
    metadata.vp9->inter_pic_predicted = true;
    metadata.vp9->spatial_layer_resolutions = {
        gfx::Size(tl_codec.spatialLayers[0].width,
                  tl_codec.spatialLayers[0].height),
    };
    metadata.vp9->begin_active_spatial_layer_index = 0;
    metadata.vp9->end_active_spatial_layer_index = 1;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);
  });
  frame_types[0] = webrtc::VideoFrameType::kVideoFrameDelta;
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(1)
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
        /*keyframe=*/true,
        /*timestamp=*/base::Microseconds(2));
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = false;
    metadata.vp9->spatial_idx = 0;
    metadata.vp9->inter_pic_predicted = true;
    metadata.vp9->spatial_layer_resolutions = {
        gfx::Size(tl_codec.spatialLayers[0].width,
                  tl_codec.spatialLayers[0].height),
        gfx::Size(tl_codec.spatialLayers[1].width,
                  tl_codec.spatialLayers[1].height),
    };
    metadata.vp9->begin_active_spatial_layer_index = 0;
    metadata.vp9->end_active_spatial_layer_index = 2;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);

    metadata.key_frame = false;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = true;
    metadata.vp9->spatial_idx = 1;
    metadata.vp9->inter_pic_predicted = true;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);
  });
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(2)
                                     .set_timestamp_us(2)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  RunUntilIdle();
}

TEST_F(RTCVideoEncoderEncodeTest, LowerSpatialLayerTurnedOffAndOnAgain) {
  // This test generates 6 layer frames with following dependencies:
  // disable S0 and S2 layers
  //       |
  //       V
  // S2  O
  //     |
  // S1  O---O---O
  //     |       |
  // S0  O       O
  //           ^
  //           |
  // re-enable S0 layer

  class Vp9CodecSpecificInfoContainer : public webrtc::EncodedImageCallback {
   public:
    webrtc::EncodedImageCallback::Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info) override {
      EXPECT_THAT(codec_specific_info, NotNull());
      if (codec_specific_info != nullptr) {
        EXPECT_EQ(codec_specific_info->codecType, webrtc::kVideoCodecVP9);
        infos_.push_back(codec_specific_info->codecSpecific.VP9);
      }
      if (encoded_image.TemporalIndex().has_value()) {
        EXPECT_EQ(encoded_image.TemporalIndex(),
                  codec_specific_info->codecSpecific.VP9.temporal_idx);
      }

      return Result(Result::OK);
    }

    const std::vector<webrtc::CodecSpecificInfoVP9>& infos() { return infos_; }

   private:
    std::vector<webrtc::CodecSpecificInfoVP9> infos_;
  };
  Vp9CodecSpecificInfoContainer encoded_callback;

  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  tl_codec.VP9()->numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[0].numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[1].numberOfTemporalLayers = 1;
  tl_codec.spatialLayers[2].numberOfTemporalLayers = 1;
  CreateEncoder(tl_codec.codecType);
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings),
            WEBRTC_VIDEO_CODEC_OK);

  rtc_encoder_->RegisterEncodeCompleteCallback(&encoded_callback);

  // Start with all three active spatial layers.
  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        /*payload_size_bytes=*/100u,
        /*keyframe=*/true, /*timestamp=*/base::Milliseconds(0));
    metadata.key_frame = true;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = false;
    metadata.vp9->spatial_idx = 0;
    metadata.vp9->spatial_layer_resolutions = ToResolutionList(tl_codec);
    ASSERT_THAT(metadata.vp9->spatial_layer_resolutions, SizeIs(3));
    metadata.vp9->begin_active_spatial_layer_index = 0;
    metadata.vp9->end_active_spatial_layer_index = 3;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);

    metadata.key_frame = false;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = false;
    metadata.vp9->spatial_idx = 1;
    metadata.vp9->reference_lower_spatial_layers = true;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);

    metadata.key_frame = false;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = true;
    metadata.vp9->spatial_idx = 2;
    metadata.vp9->reference_lower_spatial_layers = true;
    client_->BitstreamBufferReady(/*buffer_id=*/2, metadata);
  });
  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types{
      webrtc::VideoFrameType::kVideoFrameKey};
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  RunUntilIdle();
  ASSERT_THAT(encoded_callback.infos(), SizeIs(3));
  EXPECT_EQ(encoded_callback.infos()[0].first_active_layer, 0u);
  EXPECT_EQ(encoded_callback.infos()[0].num_spatial_layers, 3u);
  EXPECT_EQ(encoded_callback.infos()[1].first_active_layer, 0u);
  EXPECT_EQ(encoded_callback.infos()[1].num_spatial_layers, 3u);
  EXPECT_EQ(encoded_callback.infos()[2].first_active_layer, 0u);
  EXPECT_EQ(encoded_callback.infos()[2].num_spatial_layers, 3u);

  // Send bitrate allocation disabling the first and the last spatial layers.
  webrtc::VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(/*spatial_index=*/1, 0, 500'000);
  EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange);
  rtc_encoder_->SetRates(webrtc::VideoEncoder::RateControlParameters(
      bitrate_allocation, tl_codec.maxFramerate));
  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        /*payload_size_bytes=*/100u,
        /*keyframe=*/true, /*timestamp=*/base::Microseconds(1));
    metadata.vp9.emplace();
    metadata.vp9->spatial_idx = 0;
    metadata.vp9->reference_lower_spatial_layers = false;
    metadata.vp9->inter_pic_predicted = true;
    metadata.vp9->spatial_layer_resolutions = {
        gfx::Size(tl_codec.spatialLayers[1].width,
                  tl_codec.spatialLayers[1].height),
    };
    metadata.vp9->begin_active_spatial_layer_index = 1;
    metadata.vp9->end_active_spatial_layer_index = 2;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);
  });
  frame_types[0] = webrtc::VideoFrameType::kVideoFrameDelta;
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(1)
                                     .set_timestamp_us(1)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  RunUntilIdle();
  ASSERT_THAT(encoded_callback.infos(), SizeIs(4));
  EXPECT_EQ(encoded_callback.infos()[3].first_active_layer, 1u);
  EXPECT_EQ(encoded_callback.infos()[3].num_spatial_layers, 2u);

  // Re-enable the bottom layer.
  bitrate_allocation.SetBitrate(0, 0, 100'000);
  EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange);
  rtc_encoder_->SetRates(webrtc::VideoEncoder::RateControlParameters(
      bitrate_allocation, tl_codec.maxFramerate));
  EXPECT_CALL(*mock_vea_, Encode).WillOnce([&] {
    media::BitstreamBufferMetadata metadata(
        /*payload_size_bytes=*/100u,
        /*keyframe=*/true, /*timestamp=*/base::Microseconds(2));
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = false;
    metadata.vp9->spatial_idx = 0;
    metadata.vp9->inter_pic_predicted = false;
    metadata.vp9->spatial_layer_resolutions = {
        gfx::Size(tl_codec.spatialLayers[0].width,
                  tl_codec.spatialLayers[0].height),
        gfx::Size(tl_codec.spatialLayers[1].width,
                  tl_codec.spatialLayers[1].height),
    };
    metadata.vp9->begin_active_spatial_layer_index = 0;
    metadata.vp9->end_active_spatial_layer_index = 2;
    client_->BitstreamBufferReady(/*buffer_id=*/0, metadata);

    metadata.key_frame = false;
    metadata.vp9.emplace();
    metadata.vp9->end_of_picture = true;
    metadata.vp9->spatial_idx = 1;
    metadata.vp9->inter_pic_predicted = true;
    metadata.vp9->reference_lower_spatial_layers = true;
    client_->BitstreamBufferReady(/*buffer_id=*/1, metadata);
  });
  EXPECT_EQ(rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(2)
                                     .set_timestamp_us(2)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types),
            WEBRTC_VIDEO_CODEC_OK);
  RunUntilIdle();
  ASSERT_THAT(encoded_callback.infos(), SizeIs(6));
  EXPECT_EQ(encoded_callback.infos()[4].first_active_layer, 0u);
  EXPECT_EQ(encoded_callback.infos()[4].num_spatial_layers, 2u);
  EXPECT_EQ(encoded_callback.infos()[5].first_active_layer, 0u);
  EXPECT_EQ(encoded_callback.infos()[5].num_spatial_layers, 2u);
}

TEST_F(RTCVideoEncoderFrameSizeChangeTest,
       FrameSizeChangeSupportedVP9SpatialLayer) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/3);
  CreateEncoder(tl_codec.codecType);
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  // report frame size change support
  media::VideoEncoderInfo info;
  info.supports_frame_size_change = true;
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &media::VideoEncodeAccelerator::Client::NotifyEncoderInfoChange,
          base::Unretained(client_), info));

  size_t kNumEncodeFrames = 3u;
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    base::WaitableEvent event;
    EXPECT_CALL(*mock_vea_, Encode(_, _))
        .WillOnce(DoAll(
            Invoke(this,
                   &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata),
            [&event]() { event.Signal(); }));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());

  // Change frame size.
  tl_codec.width *= 2;
  tl_codec.height *= 2;
  for (auto& sl : tl_codec.spatialLayers) {
    sl.width *= 2;
    sl.height *= 2;
  }
  EXPECT_CALL(*mock_vea_, Flush)
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::FlushComplete));
  ExpectFrameSizeChange(gfx::Size(tl_codec.width, tl_codec.height));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  ResetSVCLayerFrameTimes();
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(tl_codec.width, tl_codec.height);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    base::WaitableEvent event;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    EXPECT_CALL(*mock_vea_, Encode(_, _))
        .WillOnce(DoAll(
            Invoke(this,
                   &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata),
            [&event]() { event.Signal(); }));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i + 3)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
}

// Simulcast requires SVC support
TEST_F(RTCVideoEncoderEncodeTest, CreateAndInitVP9Simulcast) {
  webrtc::VideoCodec tl_codec = GetSimulcastCodec(webrtc::kVideoCodecVP9,
                                                  /*num_simulcast_streams=*/3);
  CreateEncoder(tl_codec.codecType);

  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  // Simulcast is implemented via SVC, so expect spatial layers configuration.
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

// Simulcast requires SVC support
TEST_F(RTCVideoEncoderEncodeTest, CreateAndInitVP9SimulcastOneStream) {
  webrtc::VideoCodec tl_codec = GetSimulcastCodec(webrtc::kVideoCodecVP9,
                                                  /*num_simulcast_streams=*/3);
  tl_codec.simulcastStream[1].active = false;
  tl_codec.simulcastStream[2].active = false;
  CreateEncoder(tl_codec.codecType);

  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  // Simulcast is implemented via SVC, so expect spatial layers configuration.
  EXPECT_THAT(*config_,
              Field(&media::VideoEncodeAccelerator::Config::spatial_layers,
                    ElementsAre(AllOf(
                        Field(&SpatialLayer::width, kInputFrameWidth / 4),
                        Field(&SpatialLayer::height, kInputFrameHeight / 4)))));
}

// Simulcast requires SVC support
TEST_F(RTCVideoEncoderEncodeTest,
       FallbacksOnInconsistentSimulcastConfiguration) {
  webrtc::VideoCodec tl_codec = GetSimulcastCodec(webrtc::kVideoCodecVP9,
                                                  /*num_simulcast_streams=*/3);
  tl_codec.simulcastStream[1].numberOfTemporalLayers = 1;
  tl_codec.simulcastStream[2].numberOfTemporalLayers = 3;
  CreateEncoder(tl_codec.codecType);

  // Inconsistent parameters should be reported as parameters error, not as a
  // software fallback request.
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
}

#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(RTCVideoEncoderEncodeTest, MetricsProviderSetErrorIsCalledOnError) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  const auto pixel_format = media::PIXEL_FORMAT_I420;
  const auto storage_type =
      media::VideoEncodeAccelerator::Config::StorageType::kShmem;

  auto encoder_metrics_provider =
      std::make_unique<media::MockVideoEncoderMetricsProvider>();
  media::MockVideoEncoderMetricsProvider* mock_encoder_metrics_provider =
      encoder_metrics_provider.get();

  // The VEA will be owned by the RTCVideoEncoder once
  // factory.CreateVideoEncodeAccelerator() is called.
  mock_vea_ = new media::MockVideoEncodeAccelerator();
  EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
      .WillRepeatedly(Return(mock_vea_.get()));
  EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
              CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(::testing::ByMove(std::move(encoder_metrics_provider))));
  EXPECT_CALL(*mock_encoder_metrics_provider,
              MockInitialize(media::VP9PROFILE_PROFILE0,
                             gfx::Size(kInputFrameWidth, kInputFrameHeight),
                             /*is_hardware_encoder=*/true,
                             media::SVCScalabilityMode::kL1T1));
  EXPECT_CALL(*mock_vea_,
              Initialize(CheckConfig(pixel_format, storage_type, false), _, _))
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::Initialize));
  EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer).Times(AtLeast(3));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .WillOnce(Invoke([this](scoped_refptr<media::VideoFrame>, bool) {
        encoder_thread_.task_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &media::VideoEncodeAccelerator::Client::NotifyErrorStatus,
                base::Unretained(client_),
                media::EncoderStatus::Codes::kEncoderFailedEncode));
      }));

  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types;
  base::WaitableEvent error_waiter(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  EXPECT_CALL(*mock_encoder_metrics_provider,
              MockSetError(CheckStatusCode(
                  media::EncoderStatus::Codes::kEncoderFailedEncode)));
  rtc_encoder_->SetErrorWaiter(&error_waiter);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  error_waiter.Wait();
}

TEST_F(RTCVideoEncoderEncodeTest, EncodeVp9FrameWithMetricsProvider) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  const auto pixel_format = media::PIXEL_FORMAT_I420;
  const auto storage_type =
      media::VideoEncodeAccelerator::Config::StorageType::kShmem;

  auto encoder_metrics_provider =
      std::make_unique<media::MockVideoEncoderMetricsProvider>();
  media::MockVideoEncoderMetricsProvider* mock_encoder_metrics_provider =
      encoder_metrics_provider.get();

  // The VEA will be owned by the RTCVideoEncoder once
  // factory.CreateVideoEncodeAccelerator() is called.
  mock_vea_ = new media::MockVideoEncodeAccelerator();
  EXPECT_CALL(*mock_gpu_factories_.get(), DoCreateVideoEncodeAccelerator())
      .WillRepeatedly(Return(mock_vea_.get()));
  EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
              CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(::testing::ByMove(std::move(encoder_metrics_provider))));
  EXPECT_CALL(*mock_encoder_metrics_provider,
              MockInitialize(media::VP9PROFILE_PROFILE0,
                             gfx::Size(kInputFrameWidth, kInputFrameHeight),
                             /*is_hardware_encoder=*/true,
                             media::SVCScalabilityMode::kL1T1));
  EXPECT_CALL(*mock_vea_,
              Initialize(CheckConfig(pixel_format, storage_type, false), _, _))
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::Initialize));
  EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer).Times(AtLeast(3));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  size_t kNumEncodeFrames = 5u;
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    if (i > 0) {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_)).Times(1);
    }
    base::WaitableEvent event;
    EXPECT_CALL(*mock_vea_, Encode(_, _))
        .WillOnce(
            DoAll(Invoke(this, &RTCVideoEncoderTest::ReturnFrameWithTimeStamp),
                  [&event]() { event.Signal(); }));
    // This is executed in BitstreamBufferReady(). Therefore, it must be called
    // after ReturnFrameWithTimeStamp() completes.
    EXPECT_CALL(*mock_encoder_metrics_provider,
                MockIncrementEncodedFrameCount());
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
}

TEST_F(RTCVideoEncoderEncodeTest, EncodeFrameWithAdapter) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  ExpectCreateInitAndDestroyVEA();
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
          frame, base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(
                     nullptr)));
  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(0)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));

  // Encode second frame: double size. This will trigger downscale prior to
  // encoder.
  frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(kInputFrameWidth * 2, kInputFrameHeight * 2));
  frame_adapter = new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
      frame,
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(frame_adapter)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(123456)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
}

TEST_F(RTCVideoEncoderEncodeTest, EncodedBufferLifetimeExceedsEncoderLifetime) {
  webrtc::VideoCodec codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                              /*num_spatial_layers=*/1);
  CreateEncoder(codec.codecType);

  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  constexpr size_t kNumEncodeFrames = 3u;
  class EnodedBufferLifetimeVerifier : public webrtc::EncodedImageCallback {
   public:
    explicit EnodedBufferLifetimeVerifier() = default;
    ~EnodedBufferLifetimeVerifier() override {
      last_encoded_image_->data()[0] = 0;
    }

    webrtc::EncodedImageCallback::Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info) override {
      last_encoded_image_ = encoded_image.GetEncodedData();
      if (encoded_image.RtpTimestamp() == kNumEncodeFrames - 1 &&
          codec_specific_info->end_of_picture) {
        waiter_.Signal();
      }
      return Result(Result::OK);
    }

    void Wait() { waiter_.Wait(); }

   private:
    base::WaitableEvent waiter_;
    rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> last_encoded_image_;
  };

  EnodedBufferLifetimeVerifier lifetime_verifier;
  rtc_encoder_->RegisterEncodeCompleteCallback(&lifetime_verifier);
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    base::WaitableEvent event;
    if (i > 0) {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_))
          .Times((i > 1) ? 1 : 0);
    }
    EXPECT_CALL(*mock_vea_, Encode)
        .WillOnce(DoAll(
            Invoke(this,
                   &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata),
            [&event]() { event.Signal(); }));
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(i)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
  lifetime_verifier.Wait();
  RunUntilIdle();
  rtc_encoder_.reset();
}

TEST_F(RTCVideoEncoderEncodeTest, EncodeAndDropWhenTooManyFramesInEncoder) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP8;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();

  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  class DropFrameVerifier : public webrtc::EncodedImageCallback {
   public:
    DropFrameVerifier() = default;
    ~DropFrameVerifier() override = default;

    void OnDroppedFrame(DropReason reason) override {
      EXPECT_EQ(reason, DropReason::kDroppedByEncoder);
      num_dropped_frames_++;
      CHECK(event_);
      event_->Signal();
    }

    webrtc::EncodedImageCallback::Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info) override {
      if (codec_specific_info->end_of_picture) {
        num_encoded_frames_++;
        CHECK(event_);
        event_->Signal();
      }
      return Result(Result::OK);
    }

    void Verify(int num_dropped_frames, int num_encoded_frames) {
      EXPECT_EQ(num_dropped_frames_, num_dropped_frames);
      EXPECT_EQ(num_encoded_frames_, num_encoded_frames);
    }

    void SetEvent(base::WaitableEvent* event) { event_ = event; }

    void WaitEvent() { event_->Wait(); }

   private:
    raw_ptr<base::WaitableEvent> event_;
    int num_dropped_frames_{0};
    int num_encoded_frames_{0};
  };

  DropFrameVerifier dropframe_verifier;
  rtc_encoder_->RegisterEncodeCompleteCallback(&dropframe_verifier);

  constexpr static size_t kMaxFramesInEncoder = 15u;

  // Start by "loading the encoder" by building up frames sent to the VEA
  // without receiving any BitStreamBufferReady callbacks. Should lead to zero
  // dropped frames and zero encoded frames.
  base::WaitableEvent event;
  for (size_t i = 0; i < kMaxFramesInEncoder; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    EXPECT_CALL(*mock_vea_, Encode).WillOnce([&event]() { event.Signal(); });
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
    RunUntilIdle();
  }
  dropframe_verifier.Verify(0, 0);

  // At this stage the encoder holds `kMaxFramesInEncoder` frames and the next
  // frame sent to the encoder should not be encoded but dropped instead.
  // OnDroppedFrame(DropReason::kDroppedByMediaOptimizations) should be called
  // as a result and this.
  event.Reset();
  dropframe_verifier.SetEvent(&event);
  EXPECT_CALL(*mock_vea_, Encode).Times(0);
  const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
  FillFrameBuffer(buffer);
  std::vector<webrtc::VideoFrameType> frame_types;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(kMaxFramesInEncoder)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  dropframe_verifier.WaitEvent();
  RunUntilIdle();
  dropframe_verifier.Verify(1, 0);

  // Emulate that the first frame is now reported as encoded. This action should
  // decrement `frames_in_encoder_count_` to `kMaxFramesInEncoder` - 1 and also
  // result in the first OnEncodedImage callback.
  event.Reset();
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &media::VideoEncodeAccelerator::Client::BitstreamBufferReady,
          base::Unretained(client_), 0,
          media::BitstreamBufferMetadata(100, true, base::Microseconds(0))));
  dropframe_verifier.WaitEvent();
  RunUntilIdle();
  dropframe_verifier.Verify(1, 1);

  // Perform one more successful encode operation leading to a second
  // OnEncodedImage callback.
  event.Reset();
  EXPECT_CALL(*mock_vea_, Encode).WillOnce(Invoke([this] {
    client_->BitstreamBufferReady(
        0, media::BitstreamBufferMetadata(100, false, base::Microseconds(1)));
  }));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                     .set_video_frame_buffer(buffer)
                                     .set_rtp_timestamp(0)
                                     .set_timestamp_us(kMaxFramesInEncoder + 1)
                                     .set_rotation(webrtc::kVideoRotation_0)
                                     .build(),
                                 &frame_types));
  dropframe_verifier.WaitEvent();
  RunUntilIdle();
  dropframe_verifier.Verify(1, 2);
}

#if BUILDFLAG(RTC_USE_H265)
class FakeH265ParameterSetsTracker : public H265ParameterSetsTracker {
 public:
  FakeH265ParameterSetsTracker() = delete;
  explicit FakeH265ParameterSetsTracker(
      H265ParameterSetsTracker::PacketAction action)
      : action_(action) {}
  explicit FakeH265ParameterSetsTracker(rtc::ArrayView<const uint8_t> prefix)
      : action_(H265ParameterSetsTracker::PacketAction::kInsert),
        prefix_(prefix) {
    EXPECT_GT(prefix.size(), 0u);
  }

  FixedBitstream MaybeFixBitstream(
      rtc::ArrayView<const uint8_t> bitstream) override {
    FixedBitstream fixed;
    fixed.action = action_;
    if (prefix_.size() > 0) {
      fixed.bitstream =
          webrtc::EncodedImageBuffer::Create(bitstream.size() + prefix_.size());
      memcpy(fixed.bitstream->data(), prefix_.data(), prefix_.size());
      memcpy(fixed.bitstream->data() + prefix_.size(), bitstream.data(),
             bitstream.size());
    }
    return fixed;
  }

 private:
  H265ParameterSetsTracker::PacketAction action_;
  rtc::ArrayView<const uint8_t> prefix_;
};

TEST_F(RTCVideoEncoderEncodeTest, EncodeH265WithBitstreamFix) {
  class FixedBitstreamVerifier : public webrtc::EncodedImageCallback {
   public:
    explicit FixedBitstreamVerifier(rtc::ArrayView<const uint8_t> prefix,
                                    size_t encoded_image_size)
        : prefix_(prefix), encoded_image_size_(encoded_image_size) {}

    webrtc::EncodedImageCallback::Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info) override {
      EXPECT_EQ(encoded_image.size(), encoded_image_size_ + prefix_.size());
      EXPECT_THAT(
          rtc::ArrayView<const uint8_t>(encoded_image.data(), prefix_.size()),
          ::testing::ElementsAreArray(prefix_));
      waiter_.Signal();
      return Result(Result::OK);
    }

    void Wait() { waiter_.Wait(); }

   private:
    base::WaitableEvent waiter_;
    rtc::ArrayView<const uint8_t> prefix_;
    size_t encoded_image_size_;
  };

  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecH265;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  ExpectCreateInitAndDestroyVEA();

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  uint8_t prefix[] = {0x90, 0x91, 0x92, 0x93};
  rtc::ArrayView<uint8_t> prefix_view =
      rtc::ArrayView<uint8_t>(prefix, sizeof(prefix));
  rtc_encoder_->SetH265ParameterSetsTracker(
      std::make_unique<FakeH265ParameterSetsTracker>(prefix_view));
  FixedBitstreamVerifier bitstream_verifier(prefix_view,
                                            kDefaultEncodedPayloadSize);
  rtc_encoder_->RegisterEncodeCompleteCallback(&bitstream_verifier);

  EXPECT_CALL(*mock_vea_, Encode(_, _))
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::ReturnFrameWithTimeStamp));

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
}
#endif

TEST_F(RTCVideoEncoderFrameSizeChangeTest,
       FrameSizeChangeSupportedReCreateEncoder) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
              CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(::testing::ByMove(
          std::make_unique<media::MockVideoEncoderMetricsProvider>())));
  SetUpEncodingWithFrameSizeChangeSupport(codec);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());

  // Change codec type.
  codec.codecType = webrtc::kVideoCodecH264;
  EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
              CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(::testing::ByMove(
          std::make_unique<media::MockVideoEncoderMetricsProvider>())));
  ExpectCreateInitAndDestroyVEA();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
}

TEST_F(RTCVideoEncoderFrameSizeChangeTest, FrameSizeChangeSupportedVP9) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecVP9;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  SetUpEncodingWithFrameSizeChangeSupport(codec);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());

  // Change frame size.
  codec.width *= 2;
  codec.height *= 2;
  EXPECT_CALL(*mock_vea_, Flush)
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::FlushComplete));
  ExpectFrameSizeChange(gfx::Size(codec.width, codec.height));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  size_t kNumEncodeFrames = 3u;
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(codec.width, codec.height);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    base::WaitableEvent event;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    } else {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_)).Times(1);
    }
    EXPECT_CALL(*mock_vea_, Encode(_, _))
        .WillOnce(Invoke([this, &event](scoped_refptr<media::VideoFrame> frame,
                                        bool force_keyframe) {
          client_->BitstreamBufferReady(
              0, media::BitstreamBufferMetadata(0, force_keyframe,
                                                frame->timestamp()));
          event.Signal();
        }));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i + 3)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
}

TEST_F(RTCVideoEncoderFrameSizeChangeTest,
       FrameSizeChangeSupportedVP9TemporalLayer) {
  webrtc::VideoCodec tl_codec = GetSVCLayerCodec(webrtc::kVideoCodecVP9,
                                                 /*num_spatial_layers=*/1);
  CreateEncoder(tl_codec.codecType);
  ExpectCreateInitAndDestroyVEA();

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));
  // report frame size change support
  media::VideoEncoderInfo info;
  info.supports_frame_size_change = true;
  encoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &media::VideoEncodeAccelerator::Client::NotifyEncoderInfoChange,
          base::Unretained(client_), info));

  size_t kNumEncodeFrames = 3u;
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(kInputFrameWidth, kInputFrameHeight);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }
    base::WaitableEvent event;
    if (i > 0) {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_)).Times(1);
    }
    EXPECT_CALL(*mock_vea_, Encode(_, _))
        .WillOnce(DoAll(
            Invoke(this,
                   &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata),
            [&event]() { event.Signal(); }));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());

  // Change frame size.
  tl_codec.width *= 2;
  tl_codec.height *= 2;
  tl_codec.spatialLayers[0].width = tl_codec.width;
  tl_codec.spatialLayers[0].height = tl_codec.height;

  EXPECT_CALL(*mock_vea_, Flush)
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::FlushComplete));
  ExpectFrameSizeChange(gfx::Size(tl_codec.width, tl_codec.height));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&tl_codec, kVideoEncoderSettings));

  ResetSVCLayerFrameTimes();
  for (size_t i = 0; i < kNumEncodeFrames; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(tl_codec.width, tl_codec.height);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    base::WaitableEvent event;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    } else {
      EXPECT_CALL(*mock_vea_, UseOutputBitstreamBuffer(_)).Times(1);
    }
    EXPECT_CALL(*mock_vea_, Encode(_, _))
        .WillOnce(DoAll(
            Invoke(this,
                   &RTCVideoEncoderTest::ReturnSVCLayerFrameWithVp9Metadata),
            [&event]() { event.Signal(); }));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(0)
                                       .set_timestamp_us(i + 3)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build(),
                                   &frame_types));
    event.Wait();
  }
}

TEST_F(RTCVideoEncoderFrameSizeChangeTest, FrameSizeChangeSupported) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecH264;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  SetUpEncodingWithFrameSizeChangeSupport(codec);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());

  codec.width *= 2;
  codec.height *= 2;

  ExpectFrameSizeChange(gfx::Size(codec.width, codec.height));

  EXPECT_CALL(*mock_vea_, Flush)
      .WillOnce(Invoke(this, &RTCVideoEncoderTest::FlushComplete));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange(
                              _, _, std::optional<gfx::Size>()));

  webrtc::VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(1, 0, 500000);
  rtc_encoder_->SetRates(webrtc::VideoEncoder::RateControlParameters(
      bitrate_allocation, codec.maxFramerate));

  EXPECT_CALL(*mock_vea_, Encode)
      .WillRepeatedly(Invoke(
          [this](scoped_refptr<media::VideoFrame> frame, bool force_keyframe) {
            client_->BitstreamBufferReady(
                0, media::BitstreamBufferMetadata(0, force_keyframe,
                                                  frame->timestamp()));
          }));

  for (int i = 0; i < 2; i++) {
    const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(codec.width, codec.height);
    FillFrameBuffer(buffer);
    std::vector<webrtc::VideoFrameType> frame_types;
    if (i == 0) {
      frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
    }

    webrtc::VideoFrame rtc_frame = webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_rtp_timestamp(i + 3)
                                       .set_timestamp_us(i + 3)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build();

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->Encode(rtc_frame, &frame_types));
  }
  RunUntilIdle();
}

TEST_F(RTCVideoEncoderFrameSizeChangeTest,
       FrameSizeChangeSameSizeAfterSoftwareFallback) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecH264;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  SetUpEncodingWithFrameSizeChangeSupport(codec);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());

  codec.width -= 1;

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
            rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

  codec.width += 1;

    EXPECT_CALL(*mock_vea_, Flush)
        .WillOnce(Invoke(this, &RTCVideoEncoderTest::FlushComplete));
    EXPECT_CALL(*mock_vea_, RequestEncodingParametersChange(
                                _, _, std::optional<gfx::Size>()))
        .Times(AtLeast(1));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));

    webrtc::VideoBitrateAllocation bitrate_allocation;
    bitrate_allocation.SetBitrate(1, 0, 500000);
    rtc_encoder_->SetRates(webrtc::VideoEncoder::RateControlParameters(
        bitrate_allocation, codec.maxFramerate));

    EXPECT_CALL(*mock_vea_, Encode)
        .WillRepeatedly(Invoke([this](scoped_refptr<media::VideoFrame> frame,
                                      bool force_keyframe) {
          client_->BitstreamBufferReady(
              0, media::BitstreamBufferMetadata(0, force_keyframe,
                                                frame->timestamp()));
        }));

    for (int i = 0; i < 2; i++) {
      const rtc::scoped_refptr<webrtc::I420Buffer> buffer =
          webrtc::I420Buffer::Create(codec.width, codec.height);
      FillFrameBuffer(buffer);
      std::vector<webrtc::VideoFrameType> frame_types;
      if (i == 0) {
        frame_types.emplace_back(webrtc::VideoFrameType::kVideoFrameKey);
      }

      webrtc::VideoFrame rtc_frame =
          webrtc::VideoFrame::Builder()
              .set_video_frame_buffer(buffer)
              .set_rtp_timestamp(i + kFramesToEncodeBeforeFrameSizeChange)
              .set_timestamp_us(i + kFramesToEncodeBeforeFrameSizeChange)
              .set_rotation(webrtc::kVideoRotation_0)
              .build();

      EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
                rtc_encoder_->Encode(rtc_frame, &frame_types));
    }

  RunUntilIdle();
}

TEST_F(RTCVideoEncoderFrameSizeChangeTest, FrameSizeChangeFlushFailure) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecH264;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  SetUpEncodingWithFrameSizeChangeSupport(codec);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());

    std::vector<webrtc::VideoFrameType> frame_types;
    base::WaitableEvent error_waiter(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    rtc_encoder_->SetErrorWaiter(&error_waiter);

    EXPECT_CALL(*mock_vea_, Flush)
        .WillOnce(Invoke(this, &RTCVideoEncoderTest::FlushFailure));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
              rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
    error_waiter.Wait();

    auto encoder_metrics_provider =
        std::make_unique<media::MockVideoEncoderMetricsProvider>();
    EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
                CreateVideoEncoderMetricsProvider())
        .WillOnce(Return(ByMove(std::move(encoder_metrics_provider))));
    SetUpEncodingWithFrameSizeChangeSupport(codec);

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());
    RunUntilIdle();
}

TEST_F(RTCVideoEncoderFrameSizeChangeTest, FrameSizeChangeFailure) {
  const webrtc::VideoCodecType codec_type = webrtc::kVideoCodecH264;
  CreateEncoder(codec_type);
  webrtc::VideoCodec codec = GetDefaultCodec();
  codec.codecType = codec_type;
  SetUpEncodingWithFrameSizeChangeSupport(codec);

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());

  codec.width *= 2;
  codec.height *= 2;

    std::vector<webrtc::VideoFrameType> frame_types;
    base::WaitableEvent error_waiter(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    rtc_encoder_->SetErrorWaiter(&error_waiter);

    EXPECT_CALL(*mock_vea_, Flush)
        .WillOnce(Invoke(this, &RTCVideoEncoderTest::FlushComplete));
    EXPECT_CALL(
        *mock_vea_,
        RequestEncodingParametersChange(
            _, _,
            std::optional<gfx::Size>(gfx::Size(codec.width, codec.height))))
        .WillOnce(Invoke([this](const media::Bitrate&, uint32_t,
                                const std::optional<gfx::Size>&) {
          encoder_thread_.task_runner()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &media::VideoEncodeAccelerator::Client::NotifyErrorStatus,
                  base::Unretained(client_),
                  media::EncoderStatus::Codes::kSystemAPICallError));
        }));

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE,
              rtc_encoder_->InitEncode(&codec, kVideoEncoderSettings));
    error_waiter.Wait();

    auto encoder_metrics_provider =
        std::make_unique<media::MockVideoEncoderMetricsProvider>();
    EXPECT_CALL(*mock_encoder_metrics_provider_factory_,
                CreateVideoEncoderMetricsProvider())
        .WillOnce(Return(ByMove(std::move(encoder_metrics_provider))));
    SetUpEncodingWithFrameSizeChangeSupport(codec);

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, rtc_encoder_->Release());
}

}  // namespace blink
