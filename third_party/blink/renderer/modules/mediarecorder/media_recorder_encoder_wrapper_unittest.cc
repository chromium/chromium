// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_encoder_wrapper.h"

#include <memory>

#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

namespace blink {
namespace {
constexpr media::VideoCodecProfile kCodecProfile =
    media::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
constexpr media::VideoCodec kCodec = media::VideoCodec::kAV1;
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

MATCHER_P2(MatchEncoderOptions, bitrate, frame_size, "encoder option matcher") {
  return arg.bitrate.has_value() &&
         arg.bitrate->mode() == media::Bitrate::Mode::kVariable &&
         arg.bitrate->target_bps() == base::checked_cast<uint32_t>(bitrate) &&
         arg.frame_size == frame_size;
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
  media::MockVideoEncoder* const mock_encoder_;
  base::OnceClosure dtor_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class MediaRecorderEncoderWrapperTest : public ::testing::Test {
 public:
  MediaRecorderEncoderWrapperTest()
      : encoder_wrapper_(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            kCodecProfile,
            kDefaultBitrate,
            WTF::BindRepeating(
                &MediaRecorderEncoderWrapperTest::CreateMockVideoEncoder,
                base::Unretained(this)),
            WTF::BindRepeating(&MediaRecorderEncoderWrapperTest::OnEncodedVideo,
                               base::Unretained(this)),
            WTF::BindRepeating(&MediaRecorderEncoderWrapperTest::OnError,
                               base::Unretained(this))) {
    SetupSuccessful720pEncoderInitialization();
  }

  ~MediaRecorderEncoderWrapperTest() override {
    EXPECT_CALL(mock_encoder_, Dtor);
  }

 protected:
  MOCK_METHOD(void, CreateEncoder, (), ());
  MOCK_METHOD(void, OnError, (), ());
  MOCK_METHOD(void, MockVideoEncoderWrapperDtor, (), ());

  std::unique_ptr<media::VideoEncoder> CreateMockVideoEncoder() {
    CreateEncoder();
    return std::make_unique<MockVideoEncoderWrapper>(
        &mock_encoder_,
        base::BindOnce(
            &MediaRecorderEncoderWrapperTest::MockVideoEncoderWrapperDtor,
            base::Unretained(this)));
  }
  // EncodeFrame is a private function of MediaRecorderEncoderWrapper.
  // It can be called only in MediaRecorderEncoderWrapperTest.
  void EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                   base::TimeTicks capture_timestamp) {
    encoder_wrapper_.EncodeFrame(std::move(frame), capture_timestamp);
  }

  MOCK_METHOD(void,
              OnEncodedVideo,
              (const media::Muxer::VideoParameters& params,
               std::string encoded_data,
               std::string encoded_alpha,
               base::TimeTicks capture_timestamp,
               bool is_key_frame),
              ());

  void SetupSuccessful720pEncoderInitialization() {
    ON_CALL(mock_encoder_,
            Initialize(kCodecProfile,
                       MatchEncoderOptions(kDefaultBitrate, k720p), _, _, _))
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
  }

  media::VideoEncoder::OutputCB output_cb;

  media::MockVideoEncoder mock_encoder_;
  MediaRecorderEncoderWrapper encoder_wrapper_;
};

TEST_F(MediaRecorderEncoderWrapperTest, InitializesAndEncodesOneFrame) {
  InSequence s;
  EXPECT_CALL(*this, CreateEncoder);
  EXPECT_CALL(mock_encoder_, Initialize);
  EXPECT_CALL(mock_encoder_, Encode);
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, kCodec),
                             MatchStringSize(kChunkSize), MatchStringSize(0), _,
                             /*key_frame=*/true));
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest,
       EncodesTwoFramesWithoutRecreatingEncoder) {
  InSequence s;
  const auto capture_timestamp1 = base::TimeTicks::Now();
  // OnEncodedVideo to check capture_timestamp1.
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, kCodec),
                             MatchStringSize(kChunkSize), MatchStringSize(0),
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
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, kCodec),
                             MatchStringSize(kChunkSize), MatchStringSize(0),
                             capture_timestamp2, /*key_frame=*/false));

  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p), capture_timestamp2);
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest,
       EncodeTwoFramesAndDelayEncodeDoneAndOutputCB) {
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
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, kCodec),
                             MatchStringSize(kChunkSize), MatchStringSize(0),
                             capture_timestamp1, /*key_frame=*/true));
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k720p, kCodec),
                             MatchStringSize(kChunkSize), MatchStringSize(0),
                             capture_timestamp2, /*key_frame=*/false));
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p), capture_timestamp1);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p), capture_timestamp2);
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest, RecreatesEncoderOnNewResolution) {
  InSequence s;
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());

  EXPECT_CALL(mock_encoder_, Flush)
      .WillOnce(
          WithArgs<0>([](media::VideoEncoder::EncoderStatusCB flush_done_cb) {
            std::move(flush_done_cb).Run(media::EncoderStatus::Codes::kOk);
          }));
  EXPECT_CALL(*this, CreateEncoder);
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
  EXPECT_CALL(mock_encoder_,
              Initialize(kCodecProfile,
                         MatchEncoderOptions(kDefaultBitrate, k360p), _, _, _))
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
  EXPECT_CALL(*this,
              OnEncodedVideo(MatchVideoParams(k360p, kCodec),
                             MatchStringSize(kChunkSize), MatchStringSize(0), _,
                             /*key_frame=*/true));
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k360p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest, HandlesInitializeFailure) {
  InSequence s;
  EXPECT_CALL(mock_encoder_,
              Initialize(kCodecProfile,
                         MatchEncoderOptions(kDefaultBitrate, k720p), _, _, _))
      .WillOnce(WithArgs<4>(
          [](media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
            std::move(initialize_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderInitializationError);
          }));
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest, HandlesEncodeFailure) {
  InSequence s;
  EXPECT_CALL(mock_encoder_, Encode(_, MatchEncodeOption(false), _))
      .WillOnce(
          WithArgs<2>([](media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            std::move(encode_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderFailedEncode);
          }));
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest, HandlesFlushFailure) {
  InSequence s;
  EXPECT_CALL(mock_encoder_, Flush)
      .WillOnce(
          WithArgs<0>([](media::VideoEncoder::EncoderStatusCB flush_done_cb) {
            std::move(flush_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderFailedFlush);
          }));
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k360p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest, NotCallOnEncodedVideoCBIfEncodeFail) {
  InSequence s;
  EXPECT_CALL(mock_encoder_, Encode(_, MatchEncodeOption(false), _))
      .WillOnce(WithArgs<2>(
          [this](media::VideoEncoder::EncoderStatusCB encode_done_cb) {
            std::move(encode_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderFailedEncode);
            media::VideoEncoderOutput output = DefaultEncoderOutput();
            this->output_cb.Run(std::move(output), absl::nullopt);
          }));
  EXPECT_CALL(*this, OnError);
  EXPECT_CALL(*this, OnEncodedVideo).Times(0);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest,
       NotErrorCallbackTwiceByTwiceEncodeDoneFailure) {
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
  EXPECT_CALL(*this, OnError);
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EncodeFrame(media::VideoFrame::CreateBlackFrame(k720p),
              base::TimeTicks::Now());
  EXPECT_CALL(*this, MockVideoEncoderWrapperDtor);
}

TEST_F(MediaRecorderEncoderWrapperTest, IgnoresEncodeAfterFailure) {
  InSequence s;
  EXPECT_CALL(mock_encoder_,
              Initialize(kCodecProfile,
                         MatchEncoderOptions(kDefaultBitrate, k720p), _, _, _))
      .WillOnce(WithArgs<4>(
          [](media::VideoEncoder::EncoderStatusCB initialize_done_cb) {
            std::move(initialize_done_cb)
                .Run(media::EncoderStatus::Codes::kEncoderInitializationError);
          }));
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
}  // namespace blink
