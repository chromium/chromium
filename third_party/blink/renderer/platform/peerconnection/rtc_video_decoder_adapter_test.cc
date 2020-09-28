// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/decode_status.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"
#include "third_party/webrtc/media/base/vp9_profile.h"
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

class MockVideoDecoder : public media::VideoDecoder {
 public:
  std::string GetDisplayName() const override { return "MockVideoDecoder"; }
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
};

// Wraps a callback as a webrtc::DecodedImageCallback.
class DecodedImageCallback : public webrtc::DecodedImageCallback {
 public:
  DecodedImageCallback(
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

class RTCVideoDecoderAdapterTest : public ::testing::Test {
 public:
  RTCVideoDecoderAdapterTest()
      : media_thread_("Media Thread"),
        gpu_factories_(nullptr),
        sdp_format_(webrtc::SdpVideoFormat(
            webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecVP9))),
        decoded_image_callback_(decoded_cb_.Get()) {
    media_thread_.Start();

    owned_video_decoder_ = std::make_unique<StrictMock<MockVideoDecoder>>();
    video_decoder_ = owned_video_decoder_.get();

    ON_CALL(gpu_factories_, GetTaskRunner())
        .WillByDefault(Return(media_thread_.task_runner()));
    EXPECT_CALL(gpu_factories_, GetTaskRunner()).Times(AtLeast(0));

    ON_CALL(gpu_factories_, IsDecoderConfigSupported(_, _))
        .WillByDefault(
            Return(media::GpuVideoAcceleratorFactories::Supported::kTrue));
    EXPECT_CALL(gpu_factories_, IsDecoderConfigSupported(_, _))
        .Times(AtLeast(0));

    ON_CALL(gpu_factories_, CreateVideoDecoder(_, _, _))
        .WillByDefault(
            [this](media::MediaLog* media_log,
                   media::VideoDecoderImplementation impl,
                   const media::RequestOverlayInfoCB& request_overlay_info_cb) {
              DCHECK(this->owned_video_decoder_);
              return std::move(this->owned_video_decoder_);
            });
    EXPECT_CALL(gpu_factories_, CreateVideoDecoder(_, _, _)).Times(AtLeast(0));
  }

  ~RTCVideoDecoderAdapterTest() {
    if (!rtc_video_decoder_adapter_)
      return;

    media_thread_.task_runner()->DeleteSoon(
        FROM_HERE, std::move(rtc_video_decoder_adapter_));
    media_thread_.FlushForTesting();
  }

 protected:
  bool BasicSetup() {
    if (!CreateAndInitialize())
      return false;
    if (InitDecode() != WEBRTC_VIDEO_CODEC_OK)
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

  bool CreateAndInitialize(bool init_cb_result = true) {
    EXPECT_CALL(*video_decoder_, Initialize_(_, _, _, _, _, _))
        .WillOnce(DoAll(
            SaveArg<0>(&vda_config_), SaveArg<4>(&output_cb_),
            base::test::RunOnceCallback<3>(
                init_cb_result
                    ? media::OkStatus()
                    : media::Status(media::StatusCode::kCodeOnlyForTesting))));
    rtc_video_decoder_adapter_ =
        RTCVideoDecoderAdapter::Create(&gpu_factories_, sdp_format_);
    return !!rtc_video_decoder_adapter_;
  }

  int32_t InitDecode() {
    webrtc::VideoCodec codec_settings;
    codec_settings.codecType = webrtc::kVideoCodecVP9;
    return rtc_video_decoder_adapter_->InitDecode(&codec_settings, 1);
  }

  int32_t RegisterDecodeCompleteCallback() {
    return rtc_video_decoder_adapter_->RegisterDecodeCompleteCallback(
        &decoded_image_callback_);
  }

  int32_t Decode(uint32_t timestamp) {
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    input_image._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    input_image._completeFrame = true;
    input_image.SetTimestamp(timestamp);
    return rtc_video_decoder_adapter_->Decode(input_image, false, 0);
  }

  void FinishDecode(uint32_t timestamp) {
    media_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RTCVideoDecoderAdapterTest::FinishDecodeOnMediaThread,
                       base::Unretained(this), timestamp));
  }

  void FinishDecodeOnMediaThread(uint32_t timestamp) {
    DCHECK(media_thread_.task_runner()->BelongsToCurrentThread());
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

  int32_t Release() { return rtc_video_decoder_adapter_->Release(); }

  webrtc::EncodedImage GetEncodedImageWithColorSpace(uint32_t timestamp) {
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    input_image._completeFrame = true;
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

  void SetSdpFormat(const webrtc::SdpVideoFormat& sdp_format) {
    sdp_format_ = sdp_format;
  }

  base::test::TaskEnvironment task_environment_;
  base::Thread media_thread_;

  // Owned by |rtc_video_decoder_adapter_|.
  StrictMock<MockVideoDecoder>* video_decoder_ = nullptr;

  StrictMock<base::MockCallback<
      base::RepeatingCallback<void(const webrtc::VideoFrame&)>>>
      decoded_cb_;

  StrictMock<media::MockGpuVideoAcceleratorFactories> gpu_factories_;
  media::VideoDecoderConfig vda_config_;
  std::unique_ptr<RTCVideoDecoderAdapter> rtc_video_decoder_adapter_;

 private:
  webrtc::SdpVideoFormat sdp_format_;
  std::unique_ptr<StrictMock<MockVideoDecoder>> owned_video_decoder_;
  DecodedImageCallback decoded_image_callback_;
  media::VideoDecoder::OutputCB output_cb_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderAdapterTest);
};

TEST_F(RTCVideoDecoderAdapterTest, Create_UnknownFormat) {
  rtc_video_decoder_adapter_ = RTCVideoDecoderAdapter::Create(
      &gpu_factories_, webrtc::SdpVideoFormat(webrtc::CodecTypeToPayloadString(
                           webrtc::kVideoCodecGeneric)));
  ASSERT_FALSE(rtc_video_decoder_adapter_);
}

TEST_F(RTCVideoDecoderAdapterTest, Create_UnsupportedFormat) {
  EXPECT_CALL(gpu_factories_, IsDecoderConfigSupported(_, _))
      .WillOnce(Return(media::GpuVideoAcceleratorFactories::Supported::kFalse));
  rtc_video_decoder_adapter_ = RTCVideoDecoderAdapter::Create(
      &gpu_factories_, webrtc::SdpVideoFormat(webrtc::CodecTypeToPayloadString(
                           webrtc::kVideoCodecVP9)));
  ASSERT_FALSE(rtc_video_decoder_adapter_);
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
      .WillOnce(base::test::RunOnceCallback<1>(media::DecodeStatus::OK));

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);

  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(0);
  media_thread_.FlushForTesting();
}

TEST_F(RTCVideoDecoderAdapterTest, Decode_Error) {
  ASSERT_TRUE(BasicSetup());

  EXPECT_CALL(*video_decoder_, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecodeStatus::DECODE_ERROR));

  ASSERT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  media_thread_.FlushForTesting();

  ASSERT_EQ(Decode(1), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

TEST_F(RTCVideoDecoderAdapterTest, Decode_Hang_Short) {
  ASSERT_TRUE(BasicSetup());

  // Ignore Decode() calls.
  EXPECT_CALL(*video_decoder_, Decode_(_, _)).Times(AtLeast(1));

  for (int counter = 0; counter < 10; counter++) {
    int32_t result = Decode(counter);
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
      .WillRepeatedly(base::test::RunOnceCallback<1>(media::DecodeStatus::OK));
  EXPECT_CALL(decoded_cb_, Run(_)).Times(2);

  // First Decode() should cause a reinitialize as new color space is given.
  EXPECT_CALL(*video_decoder_, Initialize_(_, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<0>(&vda_config_),
                      base::test::RunOnceCallback<3>(media::OkStatus())));
  webrtc::EncodedImage first_input_image = GetEncodedImageWithColorSpace(0);
  ASSERT_EQ(rtc_video_decoder_adapter_->Decode(first_input_image, false, 0),
            WEBRTC_VIDEO_CODEC_OK);
  media_thread_.FlushForTesting();
  EXPECT_TRUE(vda_config_.color_space_info().IsSpecified());
  FinishDecode(0);
  media_thread_.FlushForTesting();

  // Second Decode() with same params should happen normally.
  webrtc::EncodedImage second_input_image = GetEncodedImageWithColorSpace(1);
  ASSERT_EQ(rtc_video_decoder_adapter_->Decode(second_input_image, false, 0),
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
      .WillOnce(base::test::RunOnceCallback<1>(media::DecodeStatus::OK));

  // Set Initialize() to fail.
  EXPECT_CALL(*video_decoder_, Initialize_(_, _, _, _, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          media::Status(media::StatusCode::kCodeOnlyForTesting)));
  ASSERT_EQ(rtc_video_decoder_adapter_->Decode(input_image, false, 0),
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
      .WillOnce(base::test::RunOnceCallback<1>(media::DecodeStatus::ABORTED));
  ASSERT_EQ(rtc_video_decoder_adapter_->Decode(input_image, false, 0),
            WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
}

}  // namespace blink
