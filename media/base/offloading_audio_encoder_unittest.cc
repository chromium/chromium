// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/offloading_audio_encoder.h"

#include <memory>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;

namespace media {

class OffloadingAudioEncoderTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_audio_encoder = std::make_unique<MockAudioEncoder>();
    mock_audio_encoder_ = mock_audio_encoder.get();
    work_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
    callback_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    EXPECT_CALL(*mock_audio_encoder_, DisablePostedCallbacks());
    offloading_encoder_ = std::make_unique<OffloadingAudioEncoder>(
        std::move(mock_audio_encoder), work_runner_, callback_runner_);
    EXPECT_CALL(*mock_audio_encoder_, OnDestruct())
        .WillOnce(Invoke([work_runner = work_runner_]() {
          EXPECT_TRUE(work_runner->RunsTasksInCurrentSequence());
        }));
  }

  void TearDown() override {
    // `mock_audio_encoder_` will be destroyed on `work_runner_` when tearing
    // down the test fixture so clear it now to avoid dangling pointer issues.
    mock_audio_encoder_ = nullptr;
  }

  void RunLoop() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> work_runner_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
  raw_ptr<MockAudioEncoder> mock_audio_encoder_;
  std::unique_ptr<OffloadingAudioEncoder> offloading_encoder_;
};

TEST_F(OffloadingAudioEncoderTest, Initialize) {
  bool called_done = false;
  bool called_output = false;
  AudioEncoder::Options options;
  AudioEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](EncodedAudioBuffer, std::optional<AudioEncoder::CodecDescription>) {
        EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
        called_output = true;
      });
  AudioEncoder::EncoderStatusCB done_cb =
      base::BindLambdaForTesting([&](EncoderStatus s) {
        EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
        called_done = true;
      });

  EXPECT_CALL(*mock_audio_encoder_, Initialize(_, _, _))
      .WillOnce(Invoke([this](const AudioEncoder::Options& options,
                              AudioEncoder::OutputCB output_cb,
                              AudioEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(work_runner_->RunsTasksInCurrentSequence());
        AudioParameters params;
        EncodedAudioBuffer buf(params, base::HeapArray<uint8_t>(),
                               base::TimeTicks());
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);

        // Usually |output_cb| is not called by Initialize() but for this
        // test it doesn't matter. We only care about a task runner used
        // for running |output_cb|, and not what triggers those callback.
        std::move(output_cb).Run(std::move(buf), {});
      }));

  offloading_encoder_->Initialize(options, std::move(output_cb),
                                  std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);
  EXPECT_TRUE(called_output);
}

TEST_F(OffloadingAudioEncoderTest, Encode) {
  bool called_done = false;
  AudioEncoder::EncoderStatusCB done_cb =
      base::BindLambdaForTesting([&](EncoderStatus s) {
        EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
        called_done = true;
      });

  EXPECT_CALL(*mock_audio_encoder_, Encode(_, _, _))
      .WillOnce(Invoke([this](std::unique_ptr<AudioBus> audio_bus,
                              base::TimeTicks capture_time,
                              AudioEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(work_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));

  base::TimeTicks ts;
  offloading_encoder_->Encode(nullptr, ts, std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);
}

TEST_F(OffloadingAudioEncoderTest, Flush) {
  bool called_done = false;
  AudioEncoder::EncoderStatusCB done_cb =
      base::BindLambdaForTesting([&](EncoderStatus s) {
        EXPECT_TRUE(callback_runner_->RunsTasksInCurrentSequence());
        called_done = true;
      });

  EXPECT_CALL(*mock_audio_encoder_, Flush(_))
      .WillOnce(Invoke([this](AudioEncoder::EncoderStatusCB done_cb) {
        EXPECT_TRUE(work_runner_->RunsTasksInCurrentSequence());
        std::move(done_cb).Run(EncoderStatus::Codes::kOk);
      }));

  offloading_encoder_->Flush(std::move(done_cb));
  RunLoop();
  EXPECT_TRUE(called_done);
}

}  // namespace media
