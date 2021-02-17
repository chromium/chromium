// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/offloading_video_encoder.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;

namespace media {

class OffloadingVideoEncoderTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_video_encoder = std::make_unique<MockVideoEncoder>();
    mock_video_encoder_ = mock_video_encoder.get();
    work_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
    callback_runner_ = base::SequencedTaskRunnerHandle::Get();
    offloading_encoder_ = std::make_unique<OffloadingVideoEncoder>(
        std::move(mock_video_encoder), work_runner_, callback_runner_);
    EXPECT_CALL(*mock_video_encoder_, Dtor()).WillOnce(Invoke([this]() {
      EXPECT_TRUE(work_runner_->RunsTasksInCurrentSequence());
    }));
  }

  void RunLoop() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> work_runner_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
  MockVideoEncoder* mock_video_encoder_;
  std::unique_ptr<OffloadingVideoEncoder> offloading_encoder_;
};

TEST_F(OffloadingVideoEncoderTest, Initialize) {
  bool called_done = false;
  bool called_output = false;
  VideoEncoder::Options options;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
        EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
        called_output = true;
      });
  VideoEncoder::StatusCB done_cb = base::BindLambdaForTesting([&](Status s) {
    EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
    called_done = true;
  });

  EXPECT_CALL(*mock_video_encoder_, Initialize(_, _, _, _))
      .WillOnce(Invoke([this](VideoCodecProfile profile,
                              const VideoEncoder::Options& options,
                              VideoEncoder::OutputCB output_cb,
                              VideoEncoder::StatusCB done_cb) {
        EXPECT_TRUE(work_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(Status());
        std::move(output_cb).Run(VideoEncoderOutput(), {});
      }));

  offloading_encoder_->Initialize(profile, options, std::move(output_cb),
                                  std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);
  EXPECT_TRUE(called_output);
}

TEST_F(OffloadingVideoEncoderTest, Encode) {
  bool called_done = false;
  VideoEncoder::StatusCB done_cb = base::BindLambdaForTesting([&](Status s) {
    EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
    called_done = true;
  });

  EXPECT_CALL(*mock_video_encoder_, Encode(_, _, _))
      .WillOnce(Invoke([this](scoped_refptr<VideoFrame> frame, bool key_frame,
                              VideoEncoder::StatusCB done_cb) {
        EXPECT_TRUE(work_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(Status());
      }));

  offloading_encoder_->Encode(nullptr, false, std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);
}

TEST_F(OffloadingVideoEncoderTest, ChangeOptions) {
  bool called_done = false;
  VideoEncoder::Options options;
  VideoEncoder::StatusCB done_cb = base::BindLambdaForTesting([&](Status s) {
    EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
    called_done = true;
  });

  VideoEncoder::OutputCB output_cb = base::BindRepeating(
      [](VideoEncoderOutput, base::Optional<VideoEncoder::CodecDescription>) {
      });

  EXPECT_CALL(*mock_video_encoder_, ChangeOptions(_, _, _))
      .WillOnce(Invoke([this](const VideoEncoder::Options& options,
                              VideoEncoder::OutputCB output_cb,
                              VideoEncoder::StatusCB done_cb) {
        EXPECT_TRUE(work_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(Status());
      }));

  offloading_encoder_->ChangeOptions(options, std::move(output_cb),
                                     std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);
}

TEST_F(OffloadingVideoEncoderTest, Flush) {
  bool called_done = false;
  VideoEncoder::StatusCB done_cb = base::BindLambdaForTesting([&](Status s) {
    EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
    called_done = true;
  });

  EXPECT_CALL(*mock_video_encoder_, Flush(_))
      .WillOnce(Invoke([this](VideoEncoder::StatusCB done_cb) {
        EXPECT_TRUE(work_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(Status());
      }));

  offloading_encoder_->Flush(std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);
}

}  // namespace media
