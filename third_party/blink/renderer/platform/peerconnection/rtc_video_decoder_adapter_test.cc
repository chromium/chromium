// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/decoder_status.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/peerconnection/resolution_monitor.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace blink {

namespace {

class FakeResolutionMonitor : public ResolutionMonitor {
 public:
  explicit FakeResolutionMonitor(bool pass_resolution_monitor,
                                 const webrtc::SdpVideoFormat& format)
      : pass_resolution_monitor_(pass_resolution_monitor),
        codec_(WebRtcToMediaVideoCodec(
            webrtc::PayloadStringToCodecType(format.name))) {}
  ~FakeResolutionMonitor() override = default;

  std::optional<gfx::Size> GetResolution(
      const media::DecoderBuffer& buffer) override {
    if (pass_resolution_monitor_) {
      return gfx::Size(1280, 720);
    } else {
      return gfx::Size(1, 1);
    }
  }
  media::VideoCodec codec() const override { return codec_; }

 private:
  const bool pass_resolution_monitor_;
  const media::VideoCodec codec_;
};

class MockVideoDecoder : public media::VideoDecoder {
 public:
  MockVideoDecoder()
      : current_decoder_type_(media::VideoDecoderType::kTesting) {}

  media::VideoDecoderType GetDecoderType() const override {
    return current_decoder_type_;
  }
  void Initialize(const media::VideoDecoderConfig& config,
                  bool low_delay,
                  media::CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const media::WaitingCB& waiting_cb) override {
    Initialize_(config, low_delay, cdm_context, init_cb, output_cb, waiting_cb);
  }
  MOCK_METHOD6(Initialize_,
               void(const media::VideoDecoderConfig& config,
                    bool low_delay,
                    media::CdmContext* cdm_context,
                    InitCB& init_cb,
                    const OutputCB& output_cb,
                    const media::WaitingCB& waiting_cb));
  void Decode(scoped_refptr<media::DecoderBuffer> buffer,
              DecodeCB cb) override {
    Decode_(std::move(buffer), cb);
  }
  MOCK_METHOD2(Decode_,
               void(scoped_refptr<media::DecoderBuffer> buffer, DecodeCB&));
  void Reset(base::OnceClosure cb) override { Reset_(cb); }
  MOCK_METHOD1(Reset_, void(base::OnceClosure&));
  bool NeedsBitstreamConversion() const override { return false; }
  bool CanReadWithoutStalling() const override { return true; }
  int GetMaxDecodeRequests() const override { return 1; }
  // We can set the type of decoder we want, the default value is kTesting.
  void SetDecoderType(media::VideoDecoderType expected_decoder_type) {
    current_decoder_type_ = expected_decoder_type;
  }

 private:
  media::VideoDecoderType current_decoder_type_;
};

// Wraps a callback as a webrtc::DecodedImageCallback.
class DecodedImageCallback : public webrtc::DecodedImageCallback {
 public:
  DecodedImageCallback(
      base::RepeatingCallback<void(const webrtc::VideoFrame&)> callback)
      : callback_(callback) {}
  DecodedImageCallback(const DecodedImageCallback&) = delete;
  DecodedImageCallback& operator=(const DecodedImageCallback&) = delete;

  int32_t Decoded(webrtc::VideoFrame& decodedImage) override {
    callback_.Run(decodedImage);
    // TODO(sandersd): Does the return value matter? RTCVideoDecoder
    // ignores it.
    return 0;
  }

 private:
  base::RepeatingCallback<void(const webrtc::VideoFrame&)> callback_;
};

class RTCVideoDecoderAdapterWrapper : public webrtc::VideoDecoder {
 public:
  static std::unique_ptr<RTCVideoDecoderAdapterWrapper> Create(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      const webrtc::SdpVideoFormat& format,
      bool pass_resolution_monitor) {
    auto wrapper = base::WrapUnique(new RTCVideoDecoderAdapterWrapper);
    bool result = false;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    wrapper->task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<RTCVideoDecoderAdapter>*
                              rtc_video_decoder_adapter,
                          media::GpuVideoAcceleratorFactories* gpu_factories,
                          const webrtc::SdpVideoFormat& format,
                          bool pass_resolution_monitor,
                          base::WaitableEvent* waiter, bool* result) {
                         *rtc_video_decoder_adapter =
                             RTCVideoDecoderAdapter::Create(
                                 gpu_factories, format,
                                 std::make_unique<FakeResolutionMonitor>(
                                     pass_resolution_monitor, format));
                         *result = !!(*rtc_video_decoder_adapter);
                         waiter->Signal();
                       },
                       &wrapper->rtc_video_decoder_adapter_, gpu_factories,
                       format, pass_resolution_monitor, &waiter, &result));
    waiter.Wait();
    return result ? std::move(wrapper) : nullptr;
  }

  bool Configure(const webrtc::VideoDecoder::Settings& settings) override {
    int32_t result = false;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](RTCVideoDecoderAdapter* rtc_video_decoder_adapter,
               webrtc::VideoDecoder::Settings settings,
               base::WaitableEvent* waiter, int32_t* result) {
              *result = rtc_video_decoder_adapter->Configure(settings);
              waiter->Signal();
            },
            rtc_video_decoder_adapter_.get(), settings, &waiter, &result));
    waiter.Wait();
    return result;
  }

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    int32_t result = false;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](RTCVideoDecoderAdapter* rtc_video_decoder_adapter,
               webrtc::DecodedImageCallback* callback,
               base::WaitableEvent* waiter, int32_t* result) {
              *result =
                  rtc_video_decoder_adapter->RegisterDecodeCompleteCallback(
                      callback);
              waiter->Signal();
            },
            rtc_video_decoder_adapter_.get(), callback, &waiter, &result));
    waiter.Wait();
    return result;
  }
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    int32_t result = false;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](RTCVideoDecoderAdapter* rtc_video_decoder_adapter,
                          const webrtc::EncodedImage& input_image,
                          bool missing_frames, int64_t render_time_ms,
                          base::WaitableEvent* waiter, int32_t* result) {
                         *result = rtc_video_decoder_adapter->Decode(
                             input_image, missing_frames, render_time_ms);
                         waiter->Signal();
                       },
                       rtc_video_decoder_adapter_.get(), input_image,
                       missing_frames, render_time_ms, &waiter, &result));
    waiter.Wait();
    return result;
  }

  int32_t Release() override {
    int32_t result = false;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](RTCVideoDecoderAdapter* rtc_video_decoder_adapter,
                          base::WaitableEvent* waiter, int32_t* result) {
                         *result = rtc_video_decoder_adapter->Release();
                         waiter->Signal();
                       },
                       rtc_video_decoder_adapter_.get(), &waiter, &result));
    waiter.Wait();
    return result;
  }

  ~RTCVideoDecoderAdapterWrapper() override {
    if (task_runner_) {
      task_runner_->DeleteSoon(FROM_HERE,
                               std::move(rtc_video_decoder_adapter_));
    }
    webrtc_decoder_thread_.FlushForTesting();
  }

 private:
  RTCVideoDecoderAdapterWrapper()
      : webrtc_decoder_thread_("WebRTC decoder thread") {
    webrtc_decoder_thread_.Start();
    task_runner_ = webrtc_decoder_thread_.task_runner();
  }

  base::Thread webrtc_decoder_thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // webrtc_decoder_thread_ members.
  std::unique_ptr<RTCVideoDecoderAdapter> rtc_video_decoder_adapter_;
};

}  // namespace

class RTCVideoDecoderAdapterTest : public ::testing::Test {
 public:
  RTCVideoDecoderAdapterTest(const RTCVideoDecoderAdapterTest&) = delete;
  RTCVideoDecoderAdapterTest& operator=(const RTCVideoDecoderAdapterTest&) =
      delete;
  RTCVideoDecoderAdapterTest()
      : media_thread_("Media Thread"),
        gpu_factories_(nullptr),
        sdp_format_(webrtc::SdpVideoFormat(
            webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecVP9))),
        decoded_image_callback_(decoded_cb_.Get()),
        spatial_index_(0) {
    media_thread_.Start();

    owned_video_decoder_ = std::make_unique<StrictMock<MockVideoDecoder>>();
    video_decoder_ = owned_video_decoder_.get();

    ON_CALL(gpu_factories_, GetTaskRunner())
        .WillByDefault(Return(media_thread_.task_runner()));
    EXPECT_CALL(gpu_factories_, GetTaskRunner()).Times(AtLeast(0));

    ON_CALL(gpu_factories_, IsDecoderConfigSupported(_))
        .WillByDefault(
            Return(media::GpuVideoAcceleratorFactories::Supported::kTrue));
    EXPECT_CALL(gpu_factories_, IsDecoderConfigSupported(_)).Times(AtLeast(0));

    ON_CALL(gpu_factories_, CreateVideoDecoder(_, _))
        .WillByDefault(
            [this](media::MediaLog* media_log,
                   const media::RequestOverlayInfoCB& request_overlay_info_cb) {
              // If gpu factories tries to get a second video decoder, for
              // testing purposes we will just return null.
              // RTCVideoDecodeAdapter already handles the case where the
              // decoder is null.
              return std::move(owned_video_decoder_);
            });
    EXPECT_CALL(gpu_factories_, CreateVideoDecoder(_, _)).Times(AtLeast(0));
    std::vector<base::test::FeatureRef> enable_features;
#if BUILDFLAG(IS_WIN)
    enable_features.emplace_back(::media::kD3D11Vp9kSVCHWDecoding);
#endif
    if (!enable_features.empty())
      feature_list_.InitWithFeatures(enable_features, {});
  }

  ~RTCVideoDecoderAdapterTest() override {
    adapter_wrapper_.reset();
    media_thread_.FlushForTesting();
  }

 protected:
  bool BasicSetup() {
    if (!CreateAndInitialize())
      return false;
    if (!InitDecode())
      return false;
    if (RegisterDecodeCompleteCallback() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    return true;
  }

  bool BasicTeardown() {
    if (Release() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    return true;
  }

  bool CreateAndInitialize(bool init_cb_result = true,
                           bool pass_resolution_monitor = true) {
    EXPECT_CALL(*video_decoder_, Initialize_(_, _, _, _, _, _))
        .WillOnce(
            DoAll(SaveArg<0>(&vda_config_), SaveArg<4>(&output_cb_),
                  base::test::RunOnceCallback<3>(
                      init_cb_result ? media::DecoderStatus::Codes::kOk
                                     : media::DecoderStatus::Codes::kFailed)));

    adapter_wrapper_ = RTCVideoDecoderAdapterWrapper::Create(
        &gpu_factories_, sdp_format_, pass_resolution_monitor);
    return !!adapter_wrapper_;
  }

  bool InitDecode() {
    webrtc::VideoDecoder::Settings settings;
    settings.set_codec_type(webrtc::kVideoCodecVP9);
    return adapter_wrapper_->Configure(settings);
  }

  int32_t RegisterDecodeCompleteCallback() {
    return adapter_wrapper_->RegisterDecodeCompleteCallback(
        &decoded_image_callback_);
  }

  int32_t Decode(uint32_t timestamp, bool keyframe = true) {
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetSpatialIndex(spatial_index_);
    for (int i = 0; i <= spatial_index_; i++)
      input_image.SetSpatialLayerFrameSize(i, 4);
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    if (timestamp == 0 || keyframe) {
      input_image._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    } else {
      input_image._frameType = webrtc::VideoFrameType::kVideoFrameDelta;
    }
    input_image.SetRtpTimestamp(timestamp);
    return adapter_wrapper_->Decode(input_image, false, 0);
  }

  void FinishDecode(uint32_t timestamp) {
    media_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RTCVideoDecoderAdapterTest::FinishDecodeOnMediaThread,
                       base::Unretained(this), timestamp));
  }

  void FinishDecodeOnMediaThread(uint32_t timestamp) {
    DCHECK(media_thread_.task_runner()->BelongsToCurrentThread());
    scoped_refptr<gpu::ClientSharedImage> shared_image =
        gpu::ClientSharedImage::CreateForTesting();
    scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapSharedImage(
        media::PIXEL_FORMAT_ARGB, shared_image, gpu::SyncToken(),
        media::VideoFrame::ReleaseMailboxCB(), gfx::Size(640, 360),
        gfx::Rect(640, 360), gfx::Size(640, 360),
        base::Microseconds(timestamp));
    output_cb_.Run(std::move(frame));
  }

  int32_t Release() { return adapter_wrapper_->Release(); }

  webrtc::EncodedImage GetEncodedImageWithColorSpace(uint32_t timestamp) {
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    input_image._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    input_image.SetRtpTimestamp(timestamp);
    webrtc::ColorSpace webrtc_color_space;
    webrtc_color_space.set_primaries_from_uint8(1);
    webrtc_color_space.set_transfer_from_uint8(1);
    webrtc_color_space.set_matrix_from_uint8(1);
    webrtc_color_space.set_range_from_uint8(1);
    input_image.SetColorSpace(webrtc_color_space);
    return input_image;
  }

  webrtc::EncodedImage GetEncodedImageWithSingleSpatialLayer(
      uint32_t timestamp) {
    constexpr int kSpatialIndex = 1;
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    input_image._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    input_image.SetRtpTimestamp(timestamp);
    // Input image only has 1 spatial layer, but non-zero spatial index.
    input_image.SetSpatialIndex(kSpatialIndex);
    input_image.SetSpatialLayerFrameSize(kSpatialIndex, sizeof(data));
    return input_image;
  }

  int GetCurrentDecoderCount() {
    int cnt = 0;
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    media_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WaitableEvent* waiter, int32_t* result) {
              *result =
                  RTCVideoDecoderAdapter::GetCurrentDecoderCountForTesting();
              waiter->Signal();
            },
            &waiter, &cnt));
    waiter.Wait();
    return cnt;
  }

  void IncrementCurrentDecoderCount() {
    media_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce([]() {
          RTCVideoDecoderAdapter::IncrementCurrentDecoderCountForTesting();
        }));
    media_thread_.FlushForTesting();
  }
  void DecrementCurrentDecoderCount() {
    media_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce([]() {
          RTCVideoDecoderAdapter::DecrementCurrentDecoderCountForTesting();
        }));
    media_thread_.FlushForTesting();
  }

  void SetSdpFormat(const webrtc::SdpVideoFormat& sdp_format) {
    sdp_format_ = sdp_format;
  }

  // We can set the spatial index we want, the default value is 0.
  void SetSpatialIndex(int spatial_index) { spatial_index_ = spatial_index; }

  base::test::TaskEnvironment task_environment_;
  base::Thread media_thread_;

  // Owned by |rtc_video_decoder_adapter_|.
  raw_ptr<StrictMock<MockVideoDecoder>, DanglingUntriaged> video_decoder_ =
      nullptr;

  StrictMock<base::MockCallback<
      base::RepeatingCallback<void(const webrtc::VideoFrame&)>>>
      decoded_cb_;

  StrictMock<media::MockGpuVideoAcceleratorFactories> gpu_factories_;
  media::VideoDecoderConfig vda_config_;
  std::unique_ptr<RTCVideoDecoderAdapterWrapper> adapter_wrapper_;

 private:
  webrtc::SdpVideoFormat sdp_format_;
  std::unique_ptr<StrictMock<MockVideoDecoder>> owned_video_decoder_;
  DecodedImageCallback decoded_image_callback_;
  media::VideoDecoder::OutputCB output_cb_;
  base::test::ScopedFeatureList feature_list_;
  int spatial_index_;
};

TEST_F(RTCVideoDecoderAdapterTest, Create_UnknownFormat) {
  ASSERT_FALSE(RTCVideoDecoderAdapterWrapper::Create(
      &gpu_factories_,
      webrtc::SdpVideoFormat(
          webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecGeneric)),
      /*pass_resolution_monitor=*/true));
}

TEST_F(RTCVideoDecoderAdapterTest, Create_UnsupportedFormat) {
  EXPECT_CALL(gpu_factories_, IsDecoderConfigSupported(_))
      .WillRepeatedly(
          Return(media::GpuVideoAcceleratorFactories::Supported::kFalse));
  ASSERT_FALSE(RTCVideoDecoderAdapterWrapper::Create(
      &gpu_factories_,
      webrtc::SdpVideoFormat(
          webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecVP9)),
      /*pass_resolution_monitor=*/true));
}

TEST_F(RTCVideoDecoderAdapterTest, Lifecycle) {
  ASSERT_TRUE(BasicSetup());
  ASSERT_TRUE(BasicTeardown());
}

TEST_F(RTCVideoDecoderAdapterTest, InitializationFailure) {
  ASSERT_FALSE(CreateAndInitialize(false));
}

TEST_F(RTCVideoDecoderAdapterTest, Decode) {
  ASSERT_TRUE(BasicSetup());

  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);

  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(0);
  media_thread_.FlushForTesting();
}

TEST_F(RTCVideoDecoderAdapterTest, Decode_Error) {
  ASSERT_TRUE(BasicSetup());

  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kFailed));

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  media_thread_.FlushForTesting();

  ASSERT_EQ(Decode(1), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_F(RTCVideoDecoderAdapterTest, Decode_Hang_Short) {
  ASSERT_TRUE(BasicSetup());

  // Ignore Decode() calls.
  EXPECT_CALL(*video_decoder_, Decode_(_, _)).Times(AtLeast(1));

  for (int counter = 0; counter < 11; counter++) {
    // At the ten-th frame, EnqueueBuffer() notifies kErrorRequestKeyFrame for
    // DecodeInternal(). It checks if the frame is keyframe on 11-th frame. If
    // the frame is the keyframe, Decode() doesn't return
    // WEBRTC_VIDEO_CODEC_ERROR. This sets |keyframe|=false so that Decode()
    // returns WEBRTC_VIDEO_CODEC_ERROR.
    int32_t result = Decode(counter, /*keyframe=*/false);
    if (result == WEBRTC_VIDEO_CODEC_ERROR) {
      ASSERT_GT(counter, 2);
      return;
    }
    media_thread_.FlushForTesting();
  }

  FAIL();
}

TEST_F(RTCVideoDecoderAdapterTest, Decode_Hang_Long) {
  ASSERT_TRUE(BasicSetup());

  // Ignore Decode() calls.
  EXPECT_CALL(*video_decoder_, Decode_(_, _)).Times(AtLeast(1));

  for (int counter = 0; counter < 100; counter++) {
    int32_t result = Decode(counter);
    if (result == WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE) {
      ASSERT_GT(counter, 10);
      return;
    }
    media_thread_.FlushForTesting();
  }

  FAIL();
}

TEST_F(RTCVideoDecoderAdapterTest, ReinitializesForHDRColorSpaceInitially) {
  SetSdpFormat(webrtc::SdpVideoFormat(
      "VP9", {{webrtc::kVP9FmtpProfileId,
               webrtc::VP9ProfileToString(webrtc::VP9Profile::kProfile2)}}));
  ASSERT_TRUE(BasicSetup());
  EXPECT_EQ(media::VP9PROFILE_PROFILE2, vda_config_.profile());
  EXPECT_FALSE(vda_config_.color_space_info().IsSpecified());

  // Decode() is expected to be called for EOS flush as well.
  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .Times(3)
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(
          media::DecoderStatus::Codes::kOk));
  EXPECT_CALL(decoded_cb_, Run(_)).Times(2);

  // First Decode() should cause a reinitialize as new color space is given.
  EXPECT_CALL(*video_decoder_, Initialize_(_, _, _, _, _, _))
      .WillOnce(DoAll(
          SaveArg<0>(&vda_config_),
          base::test::RunOnceCallback<3>(media::DecoderStatus::Codes::kOk)));
  webrtc::EncodedImage first_input_image = GetEncodedImageWithColorSpace(0);
  ASSERT_EQ(adapter_wrapper_->Decode(first_input_image, false, 0),
            WEBRTC_VIDEO_CODEC_OK);
  media_thread_.FlushForTesting();
  EXPECT_TRUE(vda_config_.color_space_info().IsSpecified());
  FinishDecode(0);
  media_thread_.FlushForTesting();

  // Second Decode() with same params should happen normally.
  webrtc::EncodedImage second_input_image = GetEncodedImageWithColorSpace(1);
  ASSERT_EQ(adapter_wrapper_->Decode(second_input_image, false, 0),
            WEBRTC_VIDEO_CODEC_OK);
  FinishDecode(1);
  media_thread_.FlushForTesting();
}

TEST_F(RTCVideoDecoderAdapterTest, HandlesReinitializeFailure) {
  SetSdpFormat(webrtc::SdpVideoFormat(
      "VP9", {{webrtc::kVP9FmtpProfileId,
               webrtc::VP9ProfileToString(webrtc::VP9Profile::kProfile2)}}));
  ASSERT_TRUE(BasicSetup());
  EXPECT_EQ(media::VP9PROFILE_PROFILE2, vda_config_.profile());
  EXPECT_FALSE(vda_config_.color_space_info().IsSpecified());
  webrtc::EncodedImage input_image = GetEncodedImageWithColorSpace(0);

  // Decode() is expected to be called for EOS flush as well.
  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));

  // Set Initialize() to fail.
  EXPECT_CALL(*video_decoder_, Initialize_(_, _, _, _, _, _))
      .WillOnce(
          base::test::RunOnceCallback<3>(media::DecoderStatus::Codes::kFailed));
  ASSERT_EQ(adapter_wrapper_->Decode(input_image, false, 0),
            WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_F(RTCVideoDecoderAdapterTest, HandlesFlushFailure) {
  SetSdpFormat(webrtc::SdpVideoFormat(
      "VP9", {{webrtc::kVP9FmtpProfileId,
               webrtc::VP9ProfileToString(webrtc::VP9Profile::kProfile2)}}));
  ASSERT_TRUE(BasicSetup());
  EXPECT_EQ(media::VP9PROFILE_PROFILE2, vda_config_.profile());
  EXPECT_FALSE(vda_config_.color_space_info().IsSpecified());
  webrtc::EncodedImage input_image = GetEncodedImageWithColorSpace(0);

  // Decode() is expected to be called for EOS flush, set to fail.
  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          media::DecoderStatus::Codes::kAborted));
  ASSERT_EQ(adapter_wrapper_->Decode(input_image, false, 0),
            WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_F(RTCVideoDecoderAdapterTest, DecoderCountIsIncrementedByDecode) {
  // If the count is nonzero, then fail immediately -- the test isn't sane.
  ASSERT_EQ(GetCurrentDecoderCount(), 0);

  // Creating a decoder should not increment the count, since we haven't sent
  // anything to decode.
  ASSERT_TRUE(CreateAndInitialize(true));
  EXPECT_EQ(GetCurrentDecoderCount(), 0);

  // The first decode should increment the count.
  EXPECT_CALL(*video_decoder_, Decode_)
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));
  EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  media_thread_.FlushForTesting();
  EXPECT_EQ(GetCurrentDecoderCount(), 1);

  // Make sure that it goes back to zero.
  EXPECT_EQ(GetCurrentDecoderCount(), 1);
  adapter_wrapper_.reset();
  media_thread_.FlushForTesting();
  EXPECT_EQ(GetCurrentDecoderCount(), 0);
}

TEST_F(RTCVideoDecoderAdapterTest, FallsBackForLowResolution) {
  // Make sure that low-resolution decoders fall back if there are too many.
  webrtc::VideoDecoder::Settings decoder_settings;
  decoder_settings.set_codec_type(webrtc::kVideoCodecVP9);

  // Pretend that we have many decoders already.
  for (int i = 0; i < RTCVideoDecoderAdapter::kMaxDecoderInstances; i++)
    IncrementCurrentDecoderCount();

  // Creating a decoder should not increment the count, since we haven't sent
  // anything to decode.
  ASSERT_TRUE(CreateAndInitialize(true, false));
  EXPECT_TRUE(adapter_wrapper_->Configure(decoder_settings));

  // The first decode should fail.  It shouldn't forward the decode call to the
  // underlying decoder.
  EXPECT_CALL(*video_decoder_, Decode_(_, _)).Times(0);
  // A fallback is caused when a number of concurrent instances are decoding
  // small resolutions.
  EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);

  // It should not increment the count, else more decoders might fall back.
  const auto max_decoder_instances =
      RTCVideoDecoderAdapter::kMaxDecoderInstances;
  EXPECT_EQ(GetCurrentDecoderCount(), max_decoder_instances);

  // Reset the count, since it's static.
  for (int i = 0; i < RTCVideoDecoderAdapter::kMaxDecoderInstances; i++)
    DecrementCurrentDecoderCount();

  // Deleting the decoder should not decrement the count.
  adapter_wrapper_.reset();
  media_thread_.FlushForTesting();
  EXPECT_EQ(GetCurrentDecoderCount(), 0);
}

#if BUILDFLAG(RTC_USE_H265)
TEST_F(RTCVideoDecoderAdapterTest, DoesNotFailForH256LowResolution) {
  // Make sure that low-resolution decode does not fail for H.265.
  SetSdpFormat(webrtc::SdpVideoFormat(
      webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecH265)));
  ASSERT_TRUE(CreateAndInitialize(true, false));
  webrtc::VideoDecoder::Settings settings;
  settings.set_codec_type(webrtc::kVideoCodecH265);
  ASSERT_TRUE(adapter_wrapper_->Configure(settings));
  ASSERT_EQ(RegisterDecodeCompleteCallback(), WEBRTC_VIDEO_CODEC_OK);

  EXPECT_CALL(*video_decoder_, Decode_(_, _)).Times(1);

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);

  media_thread_.FlushForTesting();
}
#endif

TEST_F(RTCVideoDecoderAdapterTest, DoesNotFallBackForHighResolution) {
  // Make sure that high-resolution decoders don't fall back.
  webrtc::VideoDecoder::Settings decoder_settings;
  decoder_settings.set_codec_type(webrtc::kVideoCodecVP9);

  // Pretend that we have many decoders already.
  for (int i = 0; i < RTCVideoDecoderAdapter::kMaxDecoderInstances; i++)
    IncrementCurrentDecoderCount();

  // Creating a decoder should not increment the count, since we haven't sent
  // anything to decode.
  ASSERT_TRUE(CreateAndInitialize(true, true));
  EXPECT_TRUE(adapter_wrapper_->Configure(decoder_settings));

  // The first decode should increment the count and succeed.
  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));
  EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  media_thread_.FlushForTesting();
  EXPECT_EQ(GetCurrentDecoderCount(),
            RTCVideoDecoderAdapter::kMaxDecoderInstances + 1);

  // Reset the count, since it's static.
  for (int i = 0; i < RTCVideoDecoderAdapter::kMaxDecoderInstances; i++)
    DecrementCurrentDecoderCount();
}

TEST_F(RTCVideoDecoderAdapterTest, DecodesImageWithSingleSpatialLayer) {
  ASSERT_TRUE(BasicSetup());
  webrtc::EncodedImage input_image = GetEncodedImageWithSingleSpatialLayer(0);
  scoped_refptr<media::DecoderBuffer> decoder_buffer;
  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .WillOnce(::testing::DoAll(
          ::testing::SaveArg<0>(&decoder_buffer),
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk)));
  EXPECT_EQ(adapter_wrapper_->Decode(input_image, false, 0),
            WEBRTC_VIDEO_CODEC_OK);

  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(0);
  media_thread_.FlushForTesting();

  // Check the side data was not set as there was only 1 spatial layer.
  ASSERT_TRUE(decoder_buffer);
  if (decoder_buffer->has_side_data()) {
    EXPECT_TRUE(decoder_buffer->side_data()->spatial_layers.empty());
  }
}

#if BUILDFLAG(IS_WIN)
TEST_F(RTCVideoDecoderAdapterTest, UseD3D11ToDecodeVP9kSVCStream) {
  video_decoder_->SetDecoderType(media::VideoDecoderType::kD3D11);
  ASSERT_TRUE(BasicSetup());
  SetSpatialIndex(2);
  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);

  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(0);
  media_thread_.FlushForTesting();
}
#elif !(defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS))
// ChromeOS has the ability to decode VP9 kSVC Stream. Other cases should
// fallback to sw decoder.
TEST_F(RTCVideoDecoderAdapterTest,
       FallbackToSWSinceDecodeVP9kSVCStreamWithoutD3D11) {
  ASSERT_TRUE(BasicSetup());
  SetSpatialIndex(2);
  // kTesting will represent hw decoders for other use cases mentioned above.
  EXPECT_CALL(*video_decoder_, Decode_(_, _)).Times(0);

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);

  media_thread_.FlushForTesting();
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(RTCVideoDecoderAdapterTest, FallbackToSWInAV1SVC) {
  SetSdpFormat(webrtc::SdpVideoFormat(
      webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecAV1)));
  ASSERT_TRUE(CreateAndInitialize());
  webrtc::VideoDecoder::Settings settings;
  settings.set_codec_type(webrtc::kVideoCodecAV1);
  ASSERT_TRUE(adapter_wrapper_->Configure(settings));
  ASSERT_EQ(RegisterDecodeCompleteCallback(), WEBRTC_VIDEO_CODEC_OK);

  SetSpatialIndex(2);
  // kTesting will represent hw decoders for other use cases mentioned above.
  EXPECT_CALL(*video_decoder_, Decode_(_, _)).Times(0);

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);

  media_thread_.FlushForTesting();
}

}  // namespace blink
