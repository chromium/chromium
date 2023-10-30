// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_encoder_wrapper.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

namespace blink {
namespace {
constexpr uint32_t kDefaultBitrate = 1280 * 720;
constexpr gfx::Size k720p{1280, 720};
constexpr gfx::Size k360p{640, 360};
constexpr size_t kChunkSize = 1234;
media::VideoEncoderOutput DefaultEncoderOutput() {
  media::VideoEncoderOutput output;
  output.data = std::make_unique<uint8_t[]>(kChunkSize);
  output.size = kChunkSize;
  output.key_frame = true;
  return output;
}

MATCHER_P3(MatchEncoderOptions,
           bitrate,
           frame_size,
           content_hint,
           "encoder option matcher") {
  return arg.bitrate.has_value() &&
         arg.bitrate->mode() == media::Bitrate::Mode::kVariable &&
         arg.bitrate->target_bps() == base::checked_cast<uint32_t>(bitrate) &&
         *arg.content_hint == content_hint && arg.frame_size == frame_size;
}

MATCHER_P(MatchEncodeOption, key_frame, "encode option matcher") {
  return arg.key_frame == key_frame && !arg.quantizer.has_value();
}

MATCHER_P(MatchStringSize, data_size, "encode data size matcher") {
  return arg.size() == static_cast<size_t>(data_size);
}

MATCHER_P2(MatchVideoParams,
           visible_rect_size,
           video_codec,
           "video_params matcher") {
  return arg.visible_rect_size == visible_rect_size && arg.codec == video_codec;
}

MATCHER_P(MatchErrorCode, code, "error code matcher") {
  return arg.code() == code;
}
}  // namespace
// Wraps MockVideoEncoder to not delete the pointer of MockVideoEncoder by
// the std::unique_ptr.
class MockVideoEncoderWrapper : public media::VideoEncoder {
 public:
  explicit MockVideoEncoderWrapper(media::MockVideoEncoder* const mock_encoder,
                                   base::OnceClosure dtor_cb)
      : mock_encoder_(mock_encoder), dtor_cb_(std::move(dtor_cb)) {
    CHECK(mock_encoder_);
  }

  ~MockVideoEncoderWrapper() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(dtor_cb_).Run();
  }
  void Initialize(media::VideoCodecProfile profile,
                  const Options& options,
                  EncoderInfoCB info_cb,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mock_encoder_->Initialize(profile, options, info_cb, output_cb,
                                     std::move(done_cb));
  }
  void Encode(scoped_refptr<media::VideoFrame> frame,
              const EncodeOptions& options,
              EncoderStatusCB done_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mock_encoder_->Encode(std::move(frame), options, std::move(done_cb));
  }
  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     EncoderStatusCB done_cb) override {
    NOTREACHED();
  }
  void Flush(EncoderStatusCB done_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mock_encoder_->Flush(std::move(done_cb));
  }

 private:
  const raw_ptr<media::MockVideoEncoder, ExperimentalRenderer> mock_encoder_;
  base::OnceClosure dtor_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class MediaRecorderEncoderWrapperTest
    : public ::testing::TestWithParam<media::VideoCodecProfile> {
 public:
  MediaRecorderEncoderWrapperTest()
      : profile_(GetParam()),
        codec_(media::VideoCodecProfileToVideoCodec(profile_)) {}

  ~MediaRecorderEncoderWrapperTest() override {
    EXPECT_CALL(mock_encoder_, Dtor);
  }

 protected:
  MOCK_METHOD(void, CreateEncoder, (), ());
  MOCK_METHOD(void, OnError, (), ());
  MOCK_METHOD(void, MockVideoEncoderWrapperDtor, (), ());

  std::unique_ptr<media::VideoEncoder> CreateMockVideoEncoder(
      media::GpuVideoAcceleratorFactories* /*gpu_factories*/) {
    CreateEncoder();
    return std::make_unique<MockVideoEncoderWrapper>(
        &mock_encoder_,
        base::BindOnce(
            &MediaRecorderEncoderWrapperTest::MockVideoEncoderWrapperDtor,
            base::Unretained(this)));
  }

  void CreateEncoderWrapper(bool is_screencast) {
    encoder_wrapper_ = std::make_unique<MediaRecorderEncoderWrapper>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), profile_,
        kDefaultBitrate, is_screencast,
        /*gpu_factories=*/nullptr,
        WTF::BindRepeating(
            &MediaRecorderEncoderWrapperTest::CreateMockVideoEncoder,
            base::Unretained(this)),
        WTF::BindRepeating(&MediaRecorderEncoderWrapperTest::OnEncodedVideo,
                           base::Unretained(this)),
        WTF::BindRepeating(&MediaRecorderEncoderWrapperTest::OnError,
                           base::Unretained(this)));
    EXPECT_EQ(is_screencast,
              encoder_wrapper_->IsScreenContentEncodingForTesting());
    auto metrics_provider =
        std::make_unique<media::MockVideoEncoderMetricsProvider>();
    mock_metrics_provider_ = metrics_provider.get();
    encoder_wrapper_->metrics_provider_ = std::move(metrics_provider);

    SetupSuccessful720pEncoderInitialization();
  }

  // EncodeFrame is a private function of MediaRecorderEncoderWrapper.
  // It can be called only in MediaRecorderEncoderWrapperTest.
  void EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                   base::TimeTicks capture_timestamp) {
    encoder_wrapper_->EncodeFrame(std::move(frame), capture_timestamp, false);
  }

  MOCK_METHOD(
      void,
      OnEncodedVideo,
      (const media::Muxer::VideoParameters& params,
       std::string encoded_data,
       std::string encoded_alpha,
       absl::optional<media::VideoEncoder::CodecDescription> codec_description,
       base::TimeTicks capture_timestamp,
       bool is_key_frame),
      ());

  void SetupSuccessful720pEncoderInitialization() {
    ON_CALL(mock_encoder_,
            Initialize(
                profile_,
                MatchEncoderOptions(kDefaultBitrate, k720p,
                                    media::VideoEncoder::ContentHint::Camera),
                _, _, _))
        .WillByDefault(WithArgs<3, 4>(
            [this](media::VideoEncoder::OutputCB output_callback,
                   media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
              this->output_cb = output_callback;
              std::move(initialize_done_cb)
                  .Run(media::EncoderStatus::Codes::kOk);
            }));
    ON_CALL(mock_encoder_, Encode)
        .WillByDefault(WithArgs<2>(
            [this](media::VideoEncoder::EncoderStatusCB encode_done_cb) {
              std::move(encode_done_cb).Run(media::EncoderStatus::Codes::kOk);
              media::VideoEncoderOutput output = DefaultEncoderOutput();
              this->output_cb.Run(std::move(output), absl::nullopt);
            }));
    ON_CALL(*mock_metrics_provider_,
            MockInitialize(profile_, k720p, false,
                           media::SVCScalabilityMode::kL1T1))
        .WillByDefault(Return());
  }

  const media::VideoCodecProfile profile_;
  const media::VideoCodec codec_;

  media::VideoEncoder::OutputCB output_cb;

  media::MockVideoEncoder mock_encoder_;
  raw_ptr<media::MockVideoEncoderMetricsProvider, DanglingUntriaged>
      mock_metrics_provider_;
  std::unique_ptr<MediaRecorderEncoderWrapper> encoder_wrapper_;
};

TEST_P(MediaRecorderEncoderWrapperTest, InitializesAndEncodesOneFrame) {
  CreateEncoderWrapper(false);
  InSequence s;
  EXPECT_CALL(*this, CreateEncoder);
  EXPECT_CALL(*mock_metrics_provider_, MockInitialize);
  EXPECT_CALL(mock_encoder_, Initialize);
  EXPECT_CALL(mock_encoder_, Encode);

  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EXPECT_CALL(*this, OnEncodedVideo(MatchVideoParams(k720p, codec_),
                                    MatchStringSize(kChunkSize),
                                    MatchStringSize(0), _, _,
                                    /*key_frame=*/true));
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest,
       InitializesWithScreenCastAndEncodesOneFrame) {
  CreateEncoderWrapper(true);
  InSequence s;
  EXPECT_CALL(*this, CreateEncoder);
  EXPECT_CALL(*mock_metrics_provider_, MockInitialize);
  ON_CALL(
      mock_encoder_,
      Initialize(profile_,
                 MatchEncoderOptions(kDefaultBitrate, k720p,
                                     media::VideoEncoder::ContentHint::Screen),
                 _, _, _))
      .WillByDefault(WithArgs<3, 4>(
          [this](media::VideoEncoder::OutputCB output_callback,
                 media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
            this->output_cb = output_callback;
            std::move(initialize_done_cb).Run(media::EncoderStatus::Codes::kOk);
          }));
  EXPECT_CALL(mock_encoder_, Encode);

  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EXPECT_CALL(*this, OnEncodedVideo(MatchVideoParams(k720p, codec_),
                                    MatchStringSize(kChunkSize),
                                    MatchStringSize(0), _, _,
                                    /*key_frame=*/true));
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest,
       EncodesTwoFramesWithoutRecreatingEncoder) {
  CreateEncoderWrapper(false);
  InSequence s;
  const auto capture_timestamp1 = base::TimeTicks::Now();
  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  // OnEncodedVideo to check capture_timestamp1.
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, codec_),
                             MatchStringSize(kChunkSize), MatchStringSize(0), _,
                             capture_timestamp1, /*key_frame=*/true));
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p), capture_timestamp1);

  const base::TimeTicks capture_timestamp2 =
      capture_timestamp1 + base::Microseconds(1);
  // Encode to check key_frame=false, and OnEncodedVideo to check
  // key_frame=false and capture_timestamp2.
  EXPECT_CALL(mock_encoder_, Encode)
      .WillOnce(WithArgs<2>(
          [this](media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            std::move(encode_done_cb).Run(media::EncoderStatus::Codes::kOk);
            media::VideoEncoderOutput output = DefaultEncoderOutput();
            output.key_frame = false;
            this->output_cb.Run(std::move(output), absl::nullopt);
          }));
  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, codec_),
                             MatchStringSize(kChunkSize), MatchStringSize(0), _,
                             capture_timestamp2, /*key_frame=*/false));

  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p), capture_timestamp2);
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest,
       EncodeTwoFramesAndDelayEncodeDoneAndOutputCB) {
  CreateEncoderWrapper(false);
  InSequence s;
  media::VideoEncoder::EncoderStatusCB encode_done_cb1;
  const auto capture_timestamp1 = base::TimeTicks::Now();
  const base::TimeTicks capture_timestamp2 =
      capture_timestamp1 + base::Microseconds(1);
  EXPECT_CALL(mock_encoder_, Encode)
      .WillOnce(
          WithArgs<2>([&encode_done_cb1](
                          media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            encode_done_cb1 = std::move(encode_done_cb);
          }));
  EXPECT_CALL(mock_encoder_, Encode)
      .WillOnce(WithArgs<2>(
          [this, encode_done_cb1_ptr = &encode_done_cb1](
              media::VideoEncoder::EncoderStatusCB encode_done_cb2) {
            std::move(*encode_done_cb1_ptr)
                .Run(media::EncoderStatus::Codes::kOk);
            std::move(encode_done_cb2).Run(media::EncoderStatus::Codes::kOk);
            media::VideoEncoderOutput output1 = DefaultEncoderOutput();
            media::VideoEncoderOutput output2 = DefaultEncoderOutput();
            output2.key_frame = false;
            this->output_cb.Run(std::move(output1), absl::nullopt);
            this->output_cb.Run(std::move(output2), absl::nullopt);
          }));
  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, codec_),
                             MatchStringSize(kChunkSize), MatchStringSize(0), _,
                             capture_timestamp1, /*key_frame=*/true));
  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, codec_),
                             MatchStringSize(kChunkSize), MatchStringSize(0), _,
                             capture_timestamp2, /*key_frame=*/false));
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p), capture_timestamp1);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p), capture_timestamp2);
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest, RecreatesEncoderOnNewResolution) {
  CreateEncoderWrapper(false);
  InSequence s;
  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());

  EXPECT_CALL(mock_encoder_, Flush)
      .WillOnce(
          WithArgs<0>([](media::VideoEncoder::EncoderStatusCB flush_done_cb) {
            std::move(flush_done_cb).Run(media::EncoderStatus::Codes::kOk);
          }));
  EXPECT_CALL(*this, CreateEncoder);
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
  EXPECT_CALL(
      *mock_metrics_provider_,
      MockInitialize(profile_, k360p, false, media::SVCScalabilityMode::kL1T1));
  EXPECT_CALL(
      mock_encoder_,
      Initialize(profile_,
                 MatchEncoderOptions(kDefaultBitrate, k360p,
                                     media::VideoEncoder::ContentHint::Camera),
                 _, _, _))
      .WillOnce(WithArgs<3, 4>(
          [this](media::VideoEncoder::OutputCB output_cb,
                 media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
            this->output_cb = output_cb;
            std::move(initialize_done_cb).Run(media::EncoderStatus::Codes::kOk);
          }));
  EXPECT_CALL(mock_encoder_, Encode)
      .WillOnce(WithArgs<2>(
          [this](media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            std::move(encode_done_cb).Run(media::EncoderStatus::Codes::kOk);
            media::VideoEncoderOutput output = DefaultEncoderOutput();
            this->output_cb.Run(std::move(output), absl::nullopt);
          }));
  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EXPECT_CALL(*this, OnEncodedVideo(MatchVideoParams(k360p, codec_),
                                    MatchStringSize(kChunkSize),
                                    MatchStringSize(0), _, _,
                                    /*key_frame=*/true));
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k360p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest, HandlesInitializeFailure) {
  CreateEncoderWrapper(false);
  InSequence s;
  EXPECT_CALL(
      mock_encoder_,
      Initialize(profile_,
                 MatchEncoderOptions(kDefaultBitrate, k720p,
                                     media::VideoEncoder::ContentHint::Camera),
                 _, _, _))
      .WillOnce(WithArgs<4>(
          [](media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
            std::move(initialize_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderInitializationError);
          }));
  EXPECT_CALL(*mock_metrics_provider_,
              MockSetError(MatchErrorCode(
                  media::EncoderStatus::Codes::kEncoderInitializationError)));
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest, HandlesEncodeFailure) {
  CreateEncoderWrapper(false);
  InSequence s;
  EXPECT_CALL(mock_encoder_, Encode(_, MatchEncodeOption(false), _))
      .WillOnce(
          WithArgs<2>([](media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            std::move(encode_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderFailedEncode);
          }));
  EXPECT_CALL(*mock_metrics_provider_,
              MockSetError(MatchErrorCode(
                  media::EncoderStatus::Codes::kEncoderFailedEncode)));
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest, HandlesFlushFailure) {
  CreateEncoderWrapper(false);
  InSequence s;
  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EXPECT_CALL(mock_encoder_, Flush)
      .WillOnce(
          WithArgs<0>([](media::VideoEncoder::EncoderStatusCB flush_done_cb) {
            std::move(flush_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderFailedFlush);
          }));
  EXPECT_CALL(*mock_metrics_provider_,
              MockSetError(MatchErrorCode(
                  media::EncoderStatus::Codes::kEncoderFailedFlush)));
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k360p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest, NotCallOnEncodedVideoCBIfEncodeFail) {
  CreateEncoderWrapper(false);
  InSequence s;
  EXPECT_CALL(mock_encoder_, Encode(_, MatchEncodeOption(false), _))
      .WillOnce(WithArgs<2>(
          [this](media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            std::move(encode_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderFailedEncode);
            media::VideoEncoderOutput output = DefaultEncoderOutput();
            this->output_cb.Run(std::move(output), absl::nullopt);
          }));
  EXPECT_CALL(*mock_metrics_provider_,
              MockSetError(MatchErrorCode(
                  media::EncoderStatus::Codes::kEncoderFailedEncode)));
  EXPECT_CALL(*this, OnError);
  EXPECT_CALL(*this, OnEncodedVideo).Times(0);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest,
       NotErrorCallbackTwiceByTwiceEncodeDoneFailure) {
  CreateEncoderWrapper(false);
  InSequence s;
  media::VideoEncoder::EncoderStatusCB encode_done_cb1;
  EXPECT_CALL(mock_encoder_, Encode)
      .WillOnce(
          WithArgs<2>([&encode_done_cb1](
                          media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            encode_done_cb1 = std::move(encode_done_cb);
          }));
  EXPECT_CALL(mock_encoder_, Encode)
      .WillOnce(WithArgs<2>(
          [encode_done_cb1_ptr = &encode_done_cb1](
              media::VideoEncoder::EncoderStatusCB encode_done_cb2) {
            std::move(*encode_done_cb1_ptr)
                .Run(media::EncoderStatus::Codes::kEncoderFailedEncode);
            std::move(encode_done_cb2)
                .Run(media::EncoderStatus::Codes::kEncoderFailedEncode);
          }));
  EXPECT_CALL(*mock_metrics_provider_,
              MockSetError(MatchErrorCode(
                  media::EncoderStatus::Codes::kEncoderFailedEncode)));
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest, IgnoresEncodeAfterFailure) {
  CreateEncoderWrapper(false);
  InSequence s;
  EXPECT_CALL(
      mock_encoder_,
      Initialize(profile_,
                 MatchEncoderOptions(kDefaultBitrate, k720p,
                                     media::VideoEncoder::ContentHint::Camera),
                 _, _, _))
      .WillOnce(WithArgs<4>(
          [](media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
            std::move(initialize_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderInitializationError);
          }));
  EXPECT_CALL(*mock_metrics_provider_,
              MockSetError(MatchErrorCode(
                  media::EncoderStatus::Codes::kEncoderInitializationError)));
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k360p),
              base::TimeTicks::Now());
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_P(MediaRecorderEncoderWrapperTest, InitializesAndEncodesOneAlphaFrame) {
  InSequence s;
  if (codec_ != media::VideoCodec::kVP8 && codec_ != media::VideoCodec::kVP9) {
    GTEST_SKIP() << "no alpha encoding is supported in"
                 << media::GetCodecName(codec_);
  }
  CreateEncoderWrapper(false);
  constexpr size_t kAlphaChunkSize = 2345;
  EXPECT_CALL(*this, CreateEncoder).Times(2);
  EXPECT_CALL(*mock_metrics_provider_, MockInitialize);
  media::VideoEncoder::OutputCB yuv_output_cb;
  media::VideoEncoder::OutputCB alpha_output_cb;
  EXPECT_CALL(mock_encoder_, Initialize(profile_, _, _, _, _))
      .WillOnce(WithArgs<3, 4>(
          [&](media::VideoEncoder::OutputCB output_callback,
              media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
            yuv_output_cb = output_callback;
            std::move(initialize_done_cb).Run(media::EncoderStatus::Codes::kOk);
          }));
  EXPECT_CALL(mock_encoder_, Initialize(profile_, _, _, _, _))
      .WillOnce(WithArgs<3, 4>(
          [&](media::VideoEncoder::OutputCB output_callback,
              media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
            alpha_output_cb = output_callback;
            std::move(initialize_done_cb).Run(media::EncoderStatus::Codes::kOk);
          }));

  EXPECT_CALL(mock_encoder_, Encode)
      .WillOnce(
          WithArgs<2>([yuv_output_cb_ptr = &yuv_output_cb](
                          media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            std::move(encode_done_cb).Run(media::EncoderStatus::Codes::kOk);
            media::VideoEncoderOutput output;
            output.data = std::make_unique<uint8_t[]>(kChunkSize);
            output.size = kChunkSize;
            output.key_frame = true;
            yuv_output_cb_ptr->Run(std::move(output), absl::nullopt);
          }));
  EXPECT_CALL(mock_encoder_, Encode)
      .WillOnce(
          WithArgs<2>([alpha_output_cb_ptr = &alpha_output_cb](
                          media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            std::move(encode_done_cb).Run(media::EncoderStatus::Codes::kOk);
            media::VideoEncoderOutput output;
            output.data = std::make_unique<uint8_t[]>(kAlphaChunkSize);
            output.size = kAlphaChunkSize;
            output.key_frame = true;
            alpha_output_cb_ptr->Run(std::move(output), absl::nullopt);
          }));

  EXPECT_CALL(*mock_metrics_provider_, MockIncrementEncodedFrameCount);
  EXPECT_CALL(*this, OnEncodedVideo(MatchVideoParams(k720p, codec_),
                                    MatchStringSize(kChunkSize),
                                    MatchStringSize(kAlphaChunkSize), _, _,
                                    /*key_frame=*/true));

  EncodeFrame(media::VideoFrame::CreateZeroInitializedFrame(
                  media::VideoPixelFormat::PIXEL_FORMAT_I420A, k720p,
                  gfx::Rect(k720p), k720p, base::TimeDelta()),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor).Times(2);
}

INSTANTIATE_TEST_SUITE_P(CodecProfile,
                         MediaRecorderEncoderWrapperTest,
                         ::testing::Values(media::H264PROFILE_MIN,
                                           media::VP8PROFILE_MIN,
                                           media::VP9PROFILE_MIN,
                                           media::AV1PROFILE_MIN));

}  // namespace blink
