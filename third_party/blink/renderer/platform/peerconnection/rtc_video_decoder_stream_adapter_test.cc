// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/check.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/decode_status.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_stream_adapter.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"
#include "third_party/webrtc/media/base/vp9_profile.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace blink {

namespace {

class MockVideoDecoder : public media::VideoDecoder {
 public:
  media::VideoDecoderType GetDecoderType() const override {
    return media::VideoDecoderType::kTesting;
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
  bool IsOptimizedForRTC() const override { return true; }
};

class MockDecoderFactory : public media::DecoderFactory {
 public:
  ~MockDecoderFactory() override = default;

  MOCK_METHOD3(
      CreateAudioDecoders,
      void(scoped_refptr<base::SequencedTaskRunner> task_runner,
           media::MediaLog* media_log,
           std::vector<std::unique_ptr<media::AudioDecoder>>* audio_decoders));

  void CreateVideoDecoders(scoped_refptr<base::SequencedTaskRunner> task_runner,
                           media::GpuVideoAcceleratorFactories* gpu_factories,
                           media::MediaLog* media_log,
                           media::RequestOverlayInfoCB request_overlay_info_cb,
                           const gfx::ColorSpace& target_color_space,
                           std::vector<std::unique_ptr<media::VideoDecoder>>*
                               video_decoders) override {
    video_decoders->push_back(std::move(decoder_));
  }

  MockVideoDecoder* decoder() const { return decoder_.get(); }

 private:
  // `decoder_` will be null once we're asked to create video decoders.
  std::unique_ptr<MockVideoDecoder> decoder_ =
      std::make_unique<MockVideoDecoder>();
};

// Wraps a callback as a webrtc::DecodedImageCallback.
class DecodedImageCallback : public webrtc::DecodedImageCallback {
 public:
  explicit DecodedImageCallback(
      base::RepeatingCallback<void(const webrtc::VideoFrame&)> callback)
      : callback_(callback) {}

  int32_t Decoded(webrtc::VideoFrame& decodedImage) override {
    callback_.Run(decodedImage);
    // TODO(sandersd): Does the return value matter? RTCVideoDecoder
    // ignores it.
    return 0;
  }

 private:
  base::RepeatingCallback<void(const webrtc::VideoFrame&)> callback_;

  DISALLOW_COPY_AND_ASSIGN(DecodedImageCallback);
};

}  // namespace

class RTCVideoDecoderStreamAdapterTest : public ::testing::TestWithParam<bool> {
 public:
  RTCVideoDecoderStreamAdapterTest()
      : task_environment_(
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        media_thread_task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({})),
        use_hw_decoders_(GetParam()),
        decoded_image_callback_(decoded_cb_.Get()),
        sdp_format_(webrtc::SdpVideoFormat(
            webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecVP9))) {
    decoder_factory_ = std::make_unique<MockDecoderFactory>();
  }

  ~RTCVideoDecoderStreamAdapterTest() override {
    if (!adapter_)
      return;

    media_thread_task_runner_->DeleteSoon(FROM_HERE, std::move(adapter_));
    task_environment_.RunUntilIdle();
  }

 protected:
  bool BasicSetup() {
    if (!CreateAndInitialize())
      return false;
    if (InitDecode() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    if (RegisterDecodeCompleteCallback() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    task_environment_.RunUntilIdle();
    return true;
  }

  bool BasicTeardown() {
    // Flush the media thread, to finish any in-flight decodes.  Otherwise, they
    // will be cancelled by the call to Release().
    task_environment_.RunUntilIdle();

    if (Release() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    return true;
  }

  bool CreateAndInitialize(bool init_cb_result = true) {
    auto* decoder = decoder_factory_->decoder();
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _, _))
        .WillOnce(DoAll(
            SaveArg<0>(&vda_config_), SaveArg<4>(&output_cb_),
            base::test::RunOnceCallback<3>(
                init_cb_result
                    ? media::OkStatus()
                    : media::Status(media::StatusCode::kCodeOnlyForTesting))));
    adapter_ = RTCVideoDecoderStreamAdapter::Create(
        use_hw_decoders_ ? &gpu_factories_ : nullptr, decoder_factory_.get(),
        media_thread_task_runner_, gfx::ColorSpace{}, sdp_format_);
    return !!adapter_;
  }

  int32_t InitDecode() {
    webrtc::VideoCodec codec_settings;
    codec_settings.codecType = webrtc::kVideoCodecVP9;
    return adapter_->InitDecode(&codec_settings, 1);
  }

  int32_t RegisterDecodeCompleteCallback() {
    return adapter_->RegisterDecodeCompleteCallback(&decoded_image_callback_);
  }

  int32_t Decode(uint32_t timestamp, bool missing_frames = false) {
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    input_image._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    input_image.SetTimestamp(timestamp);
    return adapter_->Decode(input_image, missing_frames, 0);
  }

  void FinishDecode(uint32_t timestamp) {
    media_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCVideoDecoderStreamAdapterTest::FinishDecodeOnMediaThread,
            base::Unretained(this), timestamp));
  }

  void FinishDecodeOnMediaThread(uint32_t timestamp) {
    DCHECK(media_thread_task_runner_->RunsTasksInCurrentSequence());
    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
    mailbox_holders[0].mailbox = gpu::Mailbox::Generate();
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapNativeTextures(
            media::PIXEL_FORMAT_ARGB, mailbox_holders,
            media::VideoFrame::ReleaseMailboxCB(), gfx::Size(640, 360),
            gfx::Rect(640, 360), gfx::Size(640, 360),
            base::TimeDelta::FromMicroseconds(timestamp));
    output_cb_.Run(std::move(frame));
  }

  int32_t Release() { return adapter_->Release(); }

  webrtc::EncodedImage GetEncodedImageWithColorSpace(uint32_t timestamp) {
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    input_image._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    input_image.SetTimestamp(timestamp);
    webrtc::ColorSpace webrtc_color_space;
    webrtc_color_space.set_primaries_from_uint8(1);
    webrtc_color_space.set_transfer_from_uint8(1);
    webrtc_color_space.set_matrix_from_uint8(1);
    webrtc_color_space.set_range_from_uint8(1);
    input_image.SetColorSpace(webrtc_color_space);
    return input_image;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> media_thread_task_runner_;

  const bool use_hw_decoders_;
  StrictMock<media::MockGpuVideoAcceleratorFactories> gpu_factories_{
      nullptr /* SharedImageInterface* */};
  std::unique_ptr<MockDecoderFactory> decoder_factory_;
  StrictMock<base::MockCallback<
      base::RepeatingCallback<void(const webrtc::VideoFrame&)>>>
      decoded_cb_;
  DecodedImageCallback decoded_image_callback_;
  std::unique_ptr<RTCVideoDecoderStreamAdapter> adapter_;
  media::VideoDecoder::OutputCB output_cb_;

  const gfx::ColorSpace rendering_colorspace_{gfx::ColorSpace::CreateSRGB()};

 private:
  webrtc::SdpVideoFormat sdp_format_;

  media::VideoDecoderConfig vda_config_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderStreamAdapterTest);
};

TEST_P(RTCVideoDecoderStreamAdapterTest, Create_UnknownFormat) {
  auto adapter = RTCVideoDecoderAdapter::Create(
      use_hw_decoders_ ? &gpu_factories_ : nullptr,
      webrtc::SdpVideoFormat(
          webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecGeneric)));
  ASSERT_FALSE(adapter);
}

TEST_P(RTCVideoDecoderStreamAdapterTest, FailInit_InitDecodeFails) {
  // If initialization fails before InitDecode runs, then InitDecode should too.
  EXPECT_TRUE(CreateAndInitialize(false));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(InitDecode(), WEBRTC_VIDEO_CODEC_UNINITIALIZED);
  EXPECT_FALSE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, FailInit_DecodeFails) {
  // If initialization fails after InitDecode runs, then the first Decode should
  // fail instead.
  EXPECT_TRUE(CreateAndInitialize(false));
  EXPECT_EQ(InitDecode(), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
  EXPECT_FALSE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, MissingFramesRequestsKeyframe) {
  EXPECT_TRUE(BasicSetup());
  EXPECT_EQ(Decode(0, true), WEBRTC_VIDEO_CODEC_ERROR);
}

TEST_P(RTCVideoDecoderStreamAdapterTest, DecodeOneFrame) {
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(BasicSetup());
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(media::DecodeStatus::OK));
  EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(0);
  EXPECT_TRUE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, SlowDecodingCausesReset) {
  // If we send enough(tm) decodes without returning any decoded frames, then
  // the decoder should try a flush + reset.
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(BasicSetup());

  // All Decodes succeed immediately.  The backup will come from the fact that
  // we won't run the media thread while sending decode requests in.
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillRepeatedly(base::test::RunOnceCallback<1>(media::DecodeStatus::OK));
  // At some point, `adapter_` should trigger a reset.
  EXPECT_CALL(*decoder, Reset_(_)).WillOnce(base::test::RunOnceCallback<0>());

  // Add decodes without calling FinishDecode.
  int limit = -1;
  for (int i = 0; i < 100 && limit < 0; i++) {
    switch (auto result = Decode(i)) {
      case WEBRTC_VIDEO_CODEC_OK:
        // Keep going -- it's still happy.
        break;
      case WEBRTC_VIDEO_CODEC_ERROR:
        // Yay -- it now believes that it's hopelessly behind, and has requested
        // a keyframe.
        limit = i;
        break;
      default:
        EXPECT_TRUE(false) << result;
    }
  }

  // We should have found a limit at some point.
  EXPECT_GT(limit, -1);

  // Let the decodes / reset complete.
  task_environment_.RunUntilIdle();

  // Everything should be reset, so one more decode should succeed.  Push a
  // frame through to make sure.
  // Should we consider pushing some non-keyframes as well, which should be
  // rejected since it reset?
  EXPECT_EQ(Decode(1000), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(1000);
  EXPECT_TRUE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, ReallySlowDecodingCausesFallback) {
  // If we send really enough(tm) decodes without returning any decoded frames,
  // then the decoder should fall back to software.
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(BasicSetup());

  // All Decodes succeed immediately.  The backup will come from the fact that
  // we won't run the media thread while sending decode requests in.
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillRepeatedly(base::test::RunOnceCallback<1>(media::DecodeStatus::OK));
  // At some point, `adapter_` should trigger a reset, before it falls back.  It
  // should not do so more than once, since we won't complete the reset.
  EXPECT_CALL(*decoder, Reset_(_)).WillOnce(base::test::RunOnceCallback<0>());

  // Add decodes without calling FinishDecode.
  int limit = -1;
  for (int i = 0; i < 100 && limit < 0; i++) {
    switch (auto result = Decode(i)) {
      case WEBRTC_VIDEO_CODEC_OK:
      case WEBRTC_VIDEO_CODEC_ERROR:
        // Keep going -- it's still happy, or not sufficiretrvg.
        break;
      case WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE:
        // Yay -- it now believes that it's hopelessly behind, and has requested
        // a keyframe.
        limit = i;
        break;
      default:
        EXPECT_TRUE(false) << result;
    }
  }

  // We should have found a limit at some point.
  EXPECT_GT(limit, -1);

  // Let the decodes / reset complete.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, WontForwardFramesAfterRelease) {
  // Deliver frames after calling Release, and verify that it doesn't (a)
  // crash or (b) deliver the frames.
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(BasicSetup());
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(media::DecodeStatus::OK));
  EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();
  // Should not be called.
  EXPECT_CALL(decoded_cb_, Run(_)).Times(0);
  FinishDecode(0);
  // Note that the decode is queued at this point, but hasn't run yet on the
  // media thread.  Release on the decoder thread, so that it should not try to
  // forward the frame when the media thread finally runs.
  adapter_->Release();
  EXPECT_TRUE(BasicTeardown());
}

INSTANTIATE_TEST_SUITE_P(UseHwDecoding,
                         RTCVideoDecoderStreamAdapterTest,
                         ::testing::Values(false, true));

}  // namespace blink
