// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/buffering_bytes_consumer.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

namespace blink {
namespace {

class BufferingBytesConsumerTest : public testing::Test {
 public:
  BufferingBytesConsumerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  using Command = ReplayingBytesConsumer::Command;
  using Result = BytesConsumer::Result;
  using PublicState = BytesConsumer::PublicState;

 protected:
  mojo::ScopedDataPipeConsumerHandle MakeDataPipe() {
    MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        0};
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    mojo::ScopedDataPipeProducerHandle producer_handle;
    CHECK_EQ(MOJO_RESULT_OK,
             mojo::CreateDataPipe(&data_pipe_options, &producer_handle,
                                  &consumer_handle));
    return consumer_handle;
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(BufferingBytesConsumerTest, Read) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* replaying_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "1"));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "23"));
  replaying_bytes_consumer->Add(Command(Command::kData, "4"));
  replaying_bytes_consumer->Add(Command(Command::kData, "567"));
  replaying_bytes_consumer->Add(Command(Command::kData, "8"));
  replaying_bytes_consumer->Add(Command(Command::kDone));

  auto* bytes_consumer =
      BufferingBytesConsumer::Create(replaying_bytes_consumer);

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", String(result.second.data(), result.second.size()));
}

TEST_F(BufferingBytesConsumerTest, ReadWithDelay) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* replaying_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBufferingBytesConsumerDelay, {{"milliseconds", "10"}});

  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "1"));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "23"));
  replaying_bytes_consumer->Add(Command(Command::kData, "4"));
  replaying_bytes_consumer->Add(Command(Command::kData, "567"));
  replaying_bytes_consumer->Add(Command(Command::kData, "8"));
  replaying_bytes_consumer->Add(Command(Command::kDone));

  auto* bytes_consumer = BufferingBytesConsumer::CreateWithDelay(
      replaying_bytes_consumer,
      scheduler::GetSingleThreadTaskRunnerForTesting());

  task_runner->RunUntilIdle();

  // The underlying consumer should not have been read yet due to the delay.
  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto result = reader->Run(task_runner.get());

  // Reading before the delay expires should still work correctly.
  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", String(result.second.data(), result.second.size()));
}

TEST_F(BufferingBytesConsumerTest, Buffering) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* replaying_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "1"));
  replaying_bytes_consumer->Add(Command(Command::kData, "23"));
  replaying_bytes_consumer->Add(Command(Command::kData, "4"));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "567"));
  replaying_bytes_consumer->Add(Command(Command::kData, "8"));
  replaying_bytes_consumer->Add(Command(Command::kDone));

  auto* bytes_consumer =
      BufferingBytesConsumer::Create(replaying_bytes_consumer);

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  task_runner->RunUntilIdle();

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kClosed, replaying_bytes_consumer->GetPublicState());

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", String(result.second.data(), result.second.size()));
}

TEST_F(BufferingBytesConsumerTest, BufferingWithDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBufferingBytesConsumerDelay, {{"milliseconds", "10"}});

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* replaying_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "1"));
  replaying_bytes_consumer->Add(Command(Command::kData, "23"));
  replaying_bytes_consumer->Add(Command(Command::kData, "4"));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "567"));
  replaying_bytes_consumer->Add(Command(Command::kData, "8"));
  replaying_bytes_consumer->Add(Command(Command::kDone));

  auto* bytes_consumer = BufferingBytesConsumer::CreateWithDelay(
      replaying_bytes_consumer,
      scheduler::GetSingleThreadTaskRunnerForTesting());

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  task_runner->RunUntilIdle();

  // The underlying consumer should not have been read yet due to the delay.
  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(11));
  task_runner->RunUntilIdle();

  // After the delay expires the underlying consumer should be completely read.
  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kClosed, replaying_bytes_consumer->GetPublicState());

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", String(result.second.data(), result.second.size()));
}

TEST_F(BufferingBytesConsumerTest, StopBuffering) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* replaying_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);

  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "1"));
  replaying_bytes_consumer->Add(Command(Command::kData, "23"));
  replaying_bytes_consumer->Add(Command(Command::kData, "4"));
  replaying_bytes_consumer->Add(Command(Command::kWait));
  replaying_bytes_consumer->Add(Command(Command::kData, "567"));
  replaying_bytes_consumer->Add(Command(Command::kData, "8"));
  replaying_bytes_consumer->Add(Command(Command::kDone));

  auto* bytes_consumer =
      BufferingBytesConsumer::Create(replaying_bytes_consumer);
  bytes_consumer->StopBuffering();

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  task_runner->RunUntilIdle();

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer->GetPublicState());

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result.first, Result::kDone);
  EXPECT_EQ("12345678", String(result.second.data(), result.second.size()));
}

TEST_F(BufferingBytesConsumerTest, DrainAsDataPipeFailsWithoutDelay) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* data_pipe_consumer =
      MakeGarbageCollected<DataPipeBytesConsumer>(task_runner, MakeDataPipe(),
                                                  &notifier);

  auto* bytes_consumer = BufferingBytesConsumer::Create(data_pipe_consumer);

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  auto pipe = bytes_consumer->DrainAsDataPipe();
  EXPECT_FALSE(pipe.is_valid());
}

TEST_F(BufferingBytesConsumerTest, DrainAsDataPipeSucceedsWithDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBufferingBytesConsumerDelay, {{"milliseconds", "10"}});

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* data_pipe_consumer =
      MakeGarbageCollected<DataPipeBytesConsumer>(task_runner, MakeDataPipe(),
                                                  &notifier);

  auto* bytes_consumer = BufferingBytesConsumer::CreateWithDelay(
      data_pipe_consumer, scheduler::GetSingleThreadTaskRunnerForTesting());

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  auto drained_consumer_handle = bytes_consumer->DrainAsDataPipe();
  EXPECT_TRUE(drained_consumer_handle.is_valid());
}

TEST_F(BufferingBytesConsumerTest, DrainAsDataPipeFailsWithExpiredDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBufferingBytesConsumerDelay, {{"milliseconds", "10"}});

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* data_pipe_consumer =
      MakeGarbageCollected<DataPipeBytesConsumer>(task_runner, MakeDataPipe(),
                                                  &notifier);

  auto* bytes_consumer = BufferingBytesConsumer::CreateWithDelay(
      data_pipe_consumer, scheduler::GetSingleThreadTaskRunnerForTesting());

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(11));

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  auto drained_consumer_handle = bytes_consumer->DrainAsDataPipe();
  EXPECT_FALSE(drained_consumer_handle.is_valid());
}

}  // namespace
}  // namespace blink
