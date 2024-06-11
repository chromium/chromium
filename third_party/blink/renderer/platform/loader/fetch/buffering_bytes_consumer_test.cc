// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/buffering_bytes_consumer.h"

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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
             mojo::CreateDataPipe(&data_pipe_options, producer_handle,
                                  consumer_handle));
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

  task_environment_.FastForwardBy(base::Milliseconds(51));
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
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* data_pipe_consumer =
      MakeGarbageCollected<DataPipeBytesConsumer>(task_runner, MakeDataPipe(),
                                                  &notifier);

  auto* bytes_consumer = BufferingBytesConsumer::CreateWithDelay(
      data_pipe_consumer, scheduler::GetSingleThreadTaskRunnerForTesting());

  task_environment_.FastForwardBy(base::Milliseconds(51));

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  auto drained_consumer_handle = bytes_consumer->DrainAsDataPipe();
  EXPECT_FALSE(drained_consumer_handle.is_valid());
}

constexpr size_t kMaxBufferSize = BufferingBytesConsumer::kMaxBufferSize;

struct MaxBytesParams {
  size_t chunk_size;
  size_t total_size;
};

class BufferingBytesConsumerMaxBytesTest
    : public BufferingBytesConsumerTest,
      public ::testing::WithParamInterface<MaxBytesParams> {
 protected:
  BufferingBytesConsumerMaxBytesTest()
      : task_runner_(base::MakeRefCounted<scheduler::FakeTaskRunner>()),
        replaying_bytes_consumer_(
            MakeGarbageCollected<ReplayingBytesConsumer>(task_runner_)) {}

  size_t ChunkSize() const { return GetParam().chunk_size; }

  size_t TotalSize() const { return GetParam().total_size; }

  // Adds `TotalSize()` / `ChunkSize()` chunks to `consumer` of size
  // `ChunkSize()`.
  void FillReplayingBytesConsumer() {
    CHECK_EQ(TotalSize() % ChunkSize(), 0u);
    Vector<char> chunk(ChunkSize(), 'a');
    for (size_t size = 0; size < TotalSize(); size += ChunkSize()) {
      replaying_bytes_consumer_->Add(Command(Command::kData, chunk));
    }
  }

  std::pair<Result, Vector<char>> Read(BufferingBytesConsumer* consumer) {
    auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(consumer);
    reader->set_max_chunk_size(ChunkSize());
    return reader->Run(task_runner_.get());
  }

  scoped_refptr<scheduler::FakeTaskRunner> task_runner_;
  ScopedBufferedBytesConsumerLimitSizeForTest feature_{true};
  Persistent<ReplayingBytesConsumer> replaying_bytes_consumer_;
};

TEST_P(BufferingBytesConsumerMaxBytesTest, ReadLargeResourceSuccessfully) {
  FillReplayingBytesConsumer();

  replaying_bytes_consumer_->Add(Command(Command::kDone));

  auto* bytes_consumer =
      BufferingBytesConsumer::Create(replaying_bytes_consumer_);

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer_->GetPublicState());

  task_runner_->RunUntilIdle();

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer_->GetPublicState());

  auto [result, data] = Read(bytes_consumer);

  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  ASSERT_EQ(result, Result::kDone);
  ASSERT_EQ(data.size(), TotalSize());
}

TEST_P(BufferingBytesConsumerMaxBytesTest, ReadLargeResourceWithError) {
  FillReplayingBytesConsumer();

  replaying_bytes_consumer_->Add(Command(Command::kError));

  auto* bytes_consumer =
      BufferingBytesConsumer::Create(replaying_bytes_consumer_);

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer_->GetPublicState());

  task_runner_->RunUntilIdle();

  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());
  EXPECT_EQ(PublicState::kReadableOrWaiting,
            replaying_bytes_consumer_->GetPublicState());

  auto [result, data] = Read(bytes_consumer);

  EXPECT_EQ(PublicState::kErrored, bytes_consumer->GetPublicState());
  ASSERT_EQ(result, Result::kError);
}

std::string PrintToString(const MaxBytesParams& params) {
  auto& [chunk_size, total_size] = params;
  return base::StringPrintf("%zu_%zu", chunk_size, total_size);
}

constexpr size_t kSixDigitPrime = 665557;
constexpr size_t kNextMultipleOfSixDigitPrimeAfterMaxBufferSize =
    ((kMaxBufferSize + kSixDigitPrime) / kSixDigitPrime) * kSixDigitPrime;

INSTANTIATE_TEST_SUITE_P(
    BufferingBytesConsumerMaxBytesTest,
    BufferingBytesConsumerMaxBytesTest,
    ::testing::Values(MaxBytesParams{1024 * 1024, kMaxBufferSize + 1024 * 1024},
                      MaxBytesParams{
                          kSixDigitPrime,
                          kNextMultipleOfSixDigitPrimeAfterMaxBufferSize},
                      MaxBytesParams{1024 * 1024, kMaxBufferSize},
                      MaxBytesParams{kMaxBufferSize, kMaxBufferSize}),
    ::testing::PrintToStringParamName());

}  // namespace
}  // namespace blink
