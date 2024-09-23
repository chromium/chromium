// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ndk_media_codec_wrapper.h"

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
constexpr char kMimeType[] = "video/avc";
}

class REQUIRES_ANDROID_API(NDK_MEDIA_CODEC_MIN_API) NdkMediaCodecWrapperTest
    : public ::testing::Test,
      public NdkMediaCodecWrapper::Client {
 public:
  NdkMediaCodecWrapperTest() = default;
  ~NdkMediaCodecWrapperTest() override = default;

  void SetUp() override {
    if (__builtin_available(android NDK_MEDIA_CODEC_MIN_API, *)) {
      // Negation results in compiler warning.
    } else {
      GTEST_SKIP() << "Not supported Android version";
    }

    codec_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});

    // Test error if this fails, as we wouldn't catch threading issues.
    ASSERT_FALSE(codec_task_runner_->RunsTasksInCurrentSequence());
  }

 protected:
  void ClearExpectations() { testing::Mock::VerifyAndClearExpectations(this); }

  void FlushMainThread() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void CreateMediaCodecWrapper() {
    wrapper_ = NdkMediaCodecWrapper::CreateByMimeType(
        kMimeType, this, base::SequencedTaskRunner::GetCurrentDefault());
  }

  void SimulateAsyncCodecMessage(base::OnceClosure closure) {
    ASSERT_TRUE(wrapper_);
    base::RunLoop run_loop;
    codec_task_runner_->PostTask(
        FROM_HERE, std::move(closure).Then(run_loop.QuitClosure()));

    run_loop.Run();
  }

  void SimulateInputAvailable(NdkMediaCodecWrapper::BufferIndex index) {
    ASSERT_TRUE(wrapper_);

    SimulateAsyncCodecMessage(base::BindLambdaForTesting([&]() {
      NdkMediaCodecWrapper::OnAsyncInputAvailable(nullptr, wrapper_.get(),
                                                  index);
    }));
  }

  void SimulateOutputAvailable(NdkMediaCodecWrapper::BufferIndex index) {
    ASSERT_TRUE(wrapper_);

    SimulateAsyncCodecMessage(base::BindLambdaForTesting([&]() {
      AMediaCodecBufferInfo buffer_info;
      NdkMediaCodecWrapper::OnAsyncOutputAvailable(nullptr, wrapper_.get(),
                                                   index, &buffer_info);
    }));
  }

  void SimulateError(media_status_t error) {
    ASSERT_TRUE(wrapper_);

    SimulateAsyncCodecMessage(base::BindLambdaForTesting([&]() {
      NdkMediaCodecWrapper::OnAsyncError(nullptr, wrapper_.get(), error, 0,
                                         "Fake Error");
    }));
  }

  MOCK_METHOD(void, OnInputAvailable, ());
  MOCK_METHOD(void, OnOutputAvailable, ());
  MOCK_METHOD(void, OnError, (media_status_t));

  // Declared first so it is created first and destroyed last.
  base::test::TaskEnvironment task_environment_;

  // Task runner used to simulate AMediaCodec async calls off the main thread.
  scoped_refptr<base::SequencedTaskRunner> codec_task_runner_;

  std::unique_ptr<NdkMediaCodecWrapper> wrapper_;
};

#pragma clang attribute push DEFAULT_REQUIRES_ANDROID_API( \
    NDK_MEDIA_CODEC_MIN_API)
TEST_F(NdkMediaCodecWrapperTest, Create) {
  auto wrapper = NdkMediaCodecWrapper::CreateByMimeType(
      kMimeType, this, base::SequencedTaskRunner::GetCurrentDefault());

  EXPECT_TRUE(wrapper);
}

TEST_F(NdkMediaCodecWrapperTest, Inputs_SingleInput) {
  CreateMediaCodecWrapper();
  ClearExpectations();
  EXPECT_CALL(*this, OnInputAvailable()).Times(1);

  // Inputs should initially be empty.
  EXPECT_FALSE(wrapper_->HasInput());

  constexpr NdkMediaCodecWrapper::BufferIndex kIndex = 3;

  SimulateInputAvailable(kIndex);
  FlushMainThread();

  // There should be one matching input buffer index.
  EXPECT_TRUE(wrapper_->HasInput());
  EXPECT_EQ(wrapper_->TakeInput(), kIndex);

  // Taking the index should clear the queue.
  EXPECT_FALSE(wrapper_->HasInput());
}

TEST_F(NdkMediaCodecWrapperTest, Inputs_MultipleInputs) {
  CreateMediaCodecWrapper();
  ClearExpectations();

  EXPECT_CALL(*this, OnInputAvailable()).Times(3);

  const NdkMediaCodecWrapper::BufferIndex kIndices[] = {3, 7, 5};

  for (auto index : kIndices) {
    SimulateInputAvailable(index);
  }
  FlushMainThread();

  for (auto index : kIndices) {
    EXPECT_TRUE(wrapper_->HasInput());

    // We should receive indices in FIFO order.
    EXPECT_EQ(wrapper_->TakeInput(), index);
  }

  // The queue should be empty after taking all buffers.
  EXPECT_FALSE(wrapper_->HasInput());
}

TEST_F(NdkMediaCodecWrapperTest, Outputs_SingleInput) {
  CreateMediaCodecWrapper();
  ClearExpectations();
  EXPECT_CALL(*this, OnOutputAvailable()).Times(1);

  // Inputs should initially be empty.
  EXPECT_FALSE(wrapper_->HasOutput());

  constexpr NdkMediaCodecWrapper::BufferIndex kIndex = 3;

  SimulateOutputAvailable(kIndex);
  FlushMainThread();

  // There should be one matching output buffer index.
  EXPECT_TRUE(wrapper_->HasOutput());

  // Peeking shouldn't drain the queue.
  EXPECT_EQ(wrapper_->PeekOutput().buffer_index, kIndex);
  EXPECT_TRUE(wrapper_->HasOutput());

  // Taking the index should clear the queue.
  EXPECT_EQ(wrapper_->TakeOutput().buffer_index, kIndex);
  EXPECT_FALSE(wrapper_->HasOutput());
}

TEST_F(NdkMediaCodecWrapperTest, Outputs_MultipleOutputs) {
  CreateMediaCodecWrapper();
  ClearExpectations();

  EXPECT_CALL(*this, OnOutputAvailable()).Times(3);

  const NdkMediaCodecWrapper::BufferIndex kIndices[] = {3, 7, 5};

  for (auto index : kIndices) {
    SimulateOutputAvailable(index);
  }
  FlushMainThread();

  for (auto index : kIndices) {
    // Peeking shouldn't drain the queue.
    EXPECT_EQ(wrapper_->PeekOutput().buffer_index, index);
    EXPECT_TRUE(wrapper_->HasOutput());

    // We should receive indices in FIFO order.
    EXPECT_EQ(wrapper_->TakeOutput().buffer_index, index);
  }

  // The queue should be empty after taking all buffers.
  EXPECT_FALSE(wrapper_->HasOutput());
}

TEST_F(NdkMediaCodecWrapperTest, Errors) {
  CreateMediaCodecWrapper();
  ClearExpectations();

  constexpr media_status_t kError = AMEDIA_ERROR_IO;

  EXPECT_CALL(*this, OnError(kError));

  SimulateError(kError);
  FlushMainThread();
}
#pragma clang attribute pop

}  // namespace media
