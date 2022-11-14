// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encoder_fallback.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::Invoke;

namespace media {

const gfx::Size kFrameSize(320, 200);
const int kFrameCount = 10;

class VideoEncoderFallbackTest : public testing::Test {
 protected:
  void SetUp() override {
    auto main_video_encoder = std::make_unique<MockVideoEncoder>();
    main_video_encoder_ = main_video_encoder.get();
    secondary_video_encoder_holder_ = std::make_unique<MockVideoEncoder>();
    secondary_video_encoder_ = secondary_video_encoder_holder_.get();
    callback_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    fallback_encoder_ = std::make_unique<VideoEncoderFallback>(
        std::move(main_video_encoder),
        base::BindOnce(&VideoEncoderFallbackTest::CreateSecondaryEncoder,
                       base::Unretained(this)));
    EXPECT_CALL(*main_video_encoder_, Dtor());
    EXPECT_CALL(*secondary_video_encoder_, Dtor());
  }

  void RunLoop() { task_environment_.RunUntilIdle(); }

  std::unique_ptr<VideoEncoder> CreateSecondaryEncoder() {
    return std::unique_ptr<VideoEncoder>(
        secondary_video_encoder_holder_.release());
  }

  bool FallbackHappened() { return !secondary_video_encoder_holder_; }

  void RunStatusCallbackAync(
      VideoEncoder::EncoderStatusCB callback,
      EncoderStatus::Codes code = EncoderStatus::Codes::kOk) {
    base::BindPostTask(callback_runner_, std::move(callback)).Run(code);
  }

  VideoEncoder::EncoderStatusCB ValidatingStatusCB(
      EncoderStatus::Codes code = EncoderStatus::Codes::kOk,
      base::Location loc = FROM_HERE) {
    struct CallEnforcer {
      bool called = false;
      std::string location;
      ~CallEnforcer() {
        EXPECT_TRUE(called) << "Callback created: " << location;
      }
    };
    auto enforcer = std::make_unique<CallEnforcer>();
    enforcer->location = loc.ToString();
    return BindToCurrentLoop(base::BindLambdaForTesting(
        [code, this, enforcer{std::move(enforcer)}](EncoderStatus s) {
          EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
          EXPECT_EQ(s.code(), code)
              << " Callback created: " << enforcer->location;
          enforcer->called = true;
        }));
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
  raw_ptr<MockVideoEncoder> main_video_encoder_;
  raw_ptr<MockVideoEncoder> secondary_video_encoder_;
  std::unique_ptr<MockVideoEncoder> secondary_video_encoder_holder_;
  std::unique_ptr<VideoEncoderFallback> fallback_encoder_;
};

TEST_F(VideoEncoderFallbackTest, NoFallbackEncoding) {
  int outputs = 0;
  VideoEncoder::Options options;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoEncoder::OutputCB output_cb =
      BindToCurrentLoop(base::BindLambdaForTesting(
          [&](VideoEncoderOutput,
              absl::optional<VideoEncoder::CodecDescription>) { outputs++; }));
  VideoEncoder::OutputCB saved_output_cb;

  EXPECT_CALL(*main_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        saved_output_cb = std::move(output_cb);
        RunStatusCallbackAync(std::move(done_cb));
      }));

  EXPECT_CALL(*main_video_encoder_, Encode(_, _, _))
      .WillRepeatedly(
          Invoke([&, this](scoped_refptr<VideoFrame> frame, bool key_frame,
                           VideoEncoder::EncoderStatusCB done_cb) {
            VideoEncoderOutput output;
            output.timestamp = frame->timestamp();
            saved_output_cb.Run(std::move(output), {});
            RunStatusCallbackAync(std::move(done_cb));
          }));

  fallback_encoder_->Initialize(profile, options, std::move(output_cb),
                                ValidatingStatusCB());

  RunLoop();
  for (int i = 0; i < kFrameCount; i++) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kFrameSize,
                                         gfx::Rect(kFrameSize), kFrameSize,
                                         base::Seconds(i));
    fallback_encoder_->Encode(frame, true, ValidatingStatusCB());
  }
  RunLoop();
  EXPECT_FALSE(FallbackHappened());
  EXPECT_EQ(outputs, kFrameCount);
}

// Test that VideoEncoderFallback can switch to a secondary encoder if
// the main encoder fails to initialize.
TEST_F(VideoEncoderFallbackTest, FallbackOnInitialize) {
  int outputs = 0;
  VideoEncoder::Options options;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoEncoder::OutputCB output_cb =
      BindToCurrentLoop(base::BindLambdaForTesting(
          [&](VideoEncoderOutput,
              absl::optional<VideoEncoder::CodecDescription>) { outputs++; }));
  VideoEncoder::OutputCB saved_output_cb;

  // Initialize() on the main encoder should fail
  EXPECT_CALL(*main_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        RunStatusCallbackAync(
            std::move(done_cb),
            EncoderStatus::Codes::kEncoderInitializationError);
      }));

  // Initialize() on the second encoder should succeed
  EXPECT_CALL(*secondary_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        saved_output_cb = std::move(output_cb);
        RunStatusCallbackAync(std::move(done_cb));
      }));

  // All encodes should come to the secondary encoder.
  EXPECT_CALL(*secondary_video_encoder_, Encode(_, _, _))
      .WillRepeatedly(
          Invoke([&, this](scoped_refptr<VideoFrame> frame, bool key_frame,
                           VideoEncoder::EncoderStatusCB done_cb) {
            VideoEncoderOutput output;
            output.timestamp = frame->timestamp();
            saved_output_cb.Run(std::move(output), {});
            RunStatusCallbackAync(std::move(done_cb));
          }));

  fallback_encoder_->Initialize(profile, options, std::move(output_cb),
                                ValidatingStatusCB());

  RunLoop();
  for (int i = 0; i < kFrameCount; i++) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kFrameSize,
                                         gfx::Rect(kFrameSize), kFrameSize,
                                         base::Seconds(i));
    fallback_encoder_->Encode(frame, true, ValidatingStatusCB());
  }
  RunLoop();
  EXPECT_TRUE(FallbackHappened());
  EXPECT_EQ(outputs, kFrameCount);
}

// Test that VideoEncoderFallback can switch to a secondary encoder if
// the main encoder fails on encode.
TEST_F(VideoEncoderFallbackTest, FallbackOnEncode) {
  int outputs = 0;
  VideoEncoder::Options options;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoEncoder::OutputCB output_cb =
      BindToCurrentLoop(base::BindLambdaForTesting(
          [&](VideoEncoderOutput,
              absl::optional<VideoEncoder::CodecDescription>) { outputs++; }));
  VideoEncoder::OutputCB primary_output_cb;
  VideoEncoder::OutputCB secondary_output_cb;

  // Initialize() on the main encoder should succeed
  EXPECT_CALL(*main_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        primary_output_cb = std::move(output_cb);
        RunStatusCallbackAync(std::move(done_cb));
      }));

  // Initialize() on the second encoder should succeed as well
  EXPECT_CALL(*secondary_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        secondary_output_cb = std::move(output_cb);
        RunStatusCallbackAync(std::move(done_cb));
      }));

  auto encoder_switch_time = base::Seconds(kFrameCount / 2);

  // Start failing encodes after half of the frames.
  EXPECT_CALL(*main_video_encoder_, Encode(_, _, _))
      .WillRepeatedly(Invoke([&, this](scoped_refptr<VideoFrame> frame,
                                       bool key_frame,
                                       VideoEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(frame);
        EXPECT_TRUE(done_cb);
        if (frame->timestamp() > encoder_switch_time) {
          std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
          return;
        }

        VideoEncoderOutput output;
        output.timestamp = frame->timestamp();
        primary_output_cb.Run(std::move(output), {});
        RunStatusCallbackAync(std::move(done_cb));
      }));

  // All encodes should come to the secondary encoder.
  EXPECT_CALL(*secondary_video_encoder_, Encode(_, _, _))
      .WillRepeatedly(
          Invoke([&, this](scoped_refptr<VideoFrame> frame, bool key_frame,
                           VideoEncoder::EncoderStatusCB done_cb) {
            EXPECT_TRUE(frame);
            EXPECT_TRUE(done_cb);
            EXPECT_GT(frame->timestamp(), encoder_switch_time);
            VideoEncoderOutput output;
            output.timestamp = frame->timestamp();
            secondary_output_cb.Run(std::move(output), {});
            RunStatusCallbackAync(std::move(done_cb));
          }));

  fallback_encoder_->Initialize(profile, options, std::move(output_cb),
                                ValidatingStatusCB());

  RunLoop();
  for (int i = 0; i < kFrameCount; i++) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kFrameSize,
                                         gfx::Rect(kFrameSize), kFrameSize,
                                         base::Seconds(i));
    fallback_encoder_->Encode(frame, true, ValidatingStatusCB());
  }
  RunLoop();
  EXPECT_TRUE(FallbackHappened());
  EXPECT_EQ(outputs, kFrameCount);
}

// Test how VideoEncoderFallback reports errors in initialization of the
// secondary encoder.
TEST_F(VideoEncoderFallbackTest, SecondaryFailureOnInitialize) {
  VideoEncoder::Options options;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  auto output_cb = BindToCurrentLoop(base::BindLambdaForTesting(
      [&](VideoEncoderOutput, absl::optional<VideoEncoder::CodecDescription>) {
      }));

  // Initialize() on the main encoder should fail
  EXPECT_CALL(*main_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        RunStatusCallbackAync(std::move(done_cb),
                              EncoderStatus::Codes::kEncoderUnsupportedProfile);
      }));

  // Initialize() on the second encoder should also fail
  EXPECT_CALL(*secondary_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        RunStatusCallbackAync(std::move(done_cb),
                              EncoderStatus::Codes::kEncoderUnsupportedCodec);
      }));

  fallback_encoder_->Initialize(
      profile, options, std::move(output_cb),
      ValidatingStatusCB(EncoderStatus::Codes::kEncoderUnsupportedCodec));
  RunLoop();

  EXPECT_CALL(*secondary_video_encoder_, Encode(_, _, _))
      .Times(kFrameCount)
      .WillRepeatedly(Invoke([&, this](scoped_refptr<VideoFrame> frame,
                                       bool key_frame,
                                       VideoEncoder::EncoderStatusCB done_cb) {
        RunStatusCallbackAync(std::move(done_cb),
                              EncoderStatus::Codes::kEncoderUnsupportedCodec);
      }));

  for (int i = 0; i < kFrameCount; i++) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kFrameSize,
                                         gfx::Rect(kFrameSize), kFrameSize,
                                         base::Seconds(i));
    auto done_callback = base::BindLambdaForTesting([this](EncoderStatus s) {
      EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
      EXPECT_EQ(s.code(), EncoderStatus::Codes::kEncoderUnsupportedCodec);
      callback_runner_->DeleteSoon(FROM_HERE, std::move(fallback_encoder_));
    });
    fallback_encoder_->Encode(frame, true, std::move(done_callback));
  }

  RunLoop();
  EXPECT_TRUE(FallbackHappened());
}

// Test how VideoEncoderFallback reports errors when encoding with the secondary
// encoder.
TEST_F(VideoEncoderFallbackTest, SecondaryFailureOnEncode) {
  int outputs = 0;
  VideoEncoder::Options options;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoEncoder::OutputCB output_cb =
      BindToCurrentLoop(base::BindLambdaForTesting(
          [&](VideoEncoderOutput,
              absl::optional<VideoEncoder::CodecDescription>) { outputs++; }));
  VideoEncoder::OutputCB primary_output_cb;
  VideoEncoder::OutputCB secondary_output_cb;

  // Initialize() on the main encoder should succeed
  EXPECT_CALL(*main_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        primary_output_cb = std::move(output_cb);
        RunStatusCallbackAync(std::move(done_cb));
      }));

  // Initialize() on the second encoder should succeed as well
  EXPECT_CALL(*secondary_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([&, this](VideoCodecProfile profile,
                                 const VideoEncoder::Options& options,
                                 VideoEncoder::OutputCB output_cb,
                                 VideoEncoder::EncoderStatusCB done_cb) {
        secondary_output_cb = std::move(output_cb);
        RunStatusCallbackAync(std::move(done_cb));
      }));

  // Start failing encodes after half of the frames.
  auto encoder_switch_time = base::Seconds(kFrameCount / 2);
  EXPECT_CALL(*main_video_encoder_, Encode(_, _, _))
      .WillRepeatedly(Invoke([&, this](scoped_refptr<VideoFrame> frame,
                                       bool key_frame,
                                       VideoEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(frame);
        EXPECT_TRUE(done_cb);
        if (frame->timestamp() > encoder_switch_time) {
          std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
          return;
        }

        VideoEncoderOutput output;
        output.timestamp = frame->timestamp();
        primary_output_cb.Run(std::move(output), {});
        RunStatusCallbackAync(std::move(done_cb));
      }));

  // All encodes should come to the secondary encoder. Again fail encoding
  // once we reach 3/4 the total frame count.
  auto second_encoder_fail_time = base::Seconds(3 * kFrameCount / 4);
  LOG(ERROR) << second_encoder_fail_time << "!!!!";
  EXPECT_CALL(*secondary_video_encoder_, Encode(_, _, _))
      .WillRepeatedly(Invoke([&, this](scoped_refptr<VideoFrame> frame,
                                       bool key_frame,
                                       VideoEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(frame);
        EXPECT_TRUE(done_cb);
        EXPECT_GT(frame->timestamp(), encoder_switch_time);
        if (frame->timestamp() > second_encoder_fail_time) {
          std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
          return;
        }
        VideoEncoderOutput output;
        output.timestamp = frame->timestamp();
        secondary_output_cb.Run(std::move(output), {});
        RunStatusCallbackAync(std::move(done_cb));
      }));

  fallback_encoder_->Initialize(profile, options, std::move(output_cb),
                                ValidatingStatusCB());
  RunLoop();

  for (int i = 1; i <= kFrameCount; i++) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kFrameSize,
                                         gfx::Rect(kFrameSize), kFrameSize,
                                         base::Seconds(i));
    auto done_cb =
        ValidatingStatusCB((frame->timestamp() <= second_encoder_fail_time)
                               ? EncoderStatus::Codes::kOk
                               : EncoderStatus::Codes::kEncoderFailedEncode);
    fallback_encoder_->Encode(frame, true, std::move(done_cb));
  }
  RunLoop();
  EXPECT_TRUE(FallbackHappened());
  EXPECT_EQ(outputs, 3 * kFrameCount / 4);
}

}  // namespace media
