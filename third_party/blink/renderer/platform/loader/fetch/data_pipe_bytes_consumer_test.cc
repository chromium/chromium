// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

namespace blink {
class DataPipeBytesConsumerTest : public testing::Test {
 public:
  using PublicState = BytesConsumer::PublicState;
  using Result = BytesConsumer::Result;

  DataPipeBytesConsumerTest()
      : task_runner_(base::MakeRefCounted<scheduler::FakeTaskRunner>()) {}

  const scoped_refptr<scheduler::FakeTaskRunner> task_runner_;
};

TEST_F(DataPipeBytesConsumerTest, TwoPhaseRead) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  const std::string_view kData = "Such hospitality. I'm underwhelmed.";
  size_t actually_written_bytes = 0;
  MojoResult rv = producer_handle->WriteData(base::as_byte_span(kData),
                                             MOJO_WRITE_DATA_FLAG_NONE,
                                             actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  ASSERT_EQ(kData.size(), actually_written_bytes);

  // Close the producer so the consumer will reach the kDone state after
  // completion is signaled below.
  producer_handle.reset();

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(consumer_handle), &notifier);
  notifier->SignalComplete();
  auto result = MakeGarbageCollected<BytesConsumerTestReader>(consumer)->Run(
      task_runner_.get());
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ(kData, String(result.second.data(), result.second.size()).Utf8());
}

TEST_F(DataPipeBytesConsumerTest, TwoPhaseRead_SignalError) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  const std::string_view kData = "Such hospitality. I'm underwhelmed.";
  size_t actually_written_bytes = 0;
  MojoResult rv = producer_handle->WriteData(base::as_byte_span(kData),
                                             MOJO_WRITE_DATA_FLAG_NONE,
                                             actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  ASSERT_EQ(kData.size(), actually_written_bytes);

  producer_handle.reset();

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(consumer_handle), &notifier);

  // Then explicitly signal an error.  This should override the pipe completion
  // and result in kError.
  notifier->SignalError(BytesConsumer::Error());

  auto result = MakeGarbageCollected<BytesConsumerTestReader>(consumer)->Run(
      task_runner_.get());
  EXPECT_EQ(Result::kError, result.first);
  EXPECT_TRUE(result.second.empty());
}

// Verify that both the DataPipe must close and SignalComplete()
// must be called for the DataPipeBytesConsumer to reach the closed
// state.
TEST_F(DataPipeBytesConsumerTest, EndOfPipeBeforeComplete) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(consumer_handle), &notifier);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  producer_handle.reset();
  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  notifier->SignalComplete();
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kDone, rv);
}

TEST_F(DataPipeBytesConsumerTest, CompleteBeforeEndOfPipe) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(consumer_handle), &notifier);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  notifier->SignalComplete();
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  producer_handle.reset();
  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kDone, rv);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
}

// Verify that SignalError moves the DataPipeBytesConsumer to the
// errored state immediately without waiting for the end of the
// DataPipe.
TEST_F(DataPipeBytesConsumerTest, EndOfPipeBeforeError) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(consumer_handle), &notifier);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  producer_handle.reset();
  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  notifier->SignalError(BytesConsumer::Error());
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kError, rv);
}

TEST_F(DataPipeBytesConsumerTest, SignalSizeBeforeRead) {
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::ScopedDataPipeProducerHandle writable;
  const MojoCreateDataPipeOptions options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 0};
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, writable, readable));
  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(readable), &notifier);

  const std::string_view kData = "hello";
  size_t actually_written_bytes = 0;
  MojoResult write_result =
      writable->WriteData(base::as_byte_span(kData), MOJO_WRITE_DATA_FLAG_NONE,
                          actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, write_result);
  ASSERT_EQ(5u, actually_written_bytes);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  notifier->SignalSize(5);

  Result rv = consumer->BeginRead(&buffer, &available);
  ASSERT_EQ(Result::kOk, rv);
  EXPECT_EQ(available, 5u);

  rv = consumer->EndRead(2);
  ASSERT_EQ(Result::kOk, rv);

  rv = consumer->BeginRead(&buffer, &available);
  ASSERT_EQ(Result::kOk, rv);
  EXPECT_EQ(available, 3u);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  rv = consumer->EndRead(3);
  ASSERT_EQ(Result::kDone, rv);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(DataPipeBytesConsumerTest, SignalExcessSizeBeforeEndOfData) {
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::ScopedDataPipeProducerHandle writable;
  const MojoCreateDataPipeOptions options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 0};
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, writable, readable));
  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(readable), &notifier);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  notifier->SignalSize(1);

  const char* buffer = nullptr;
  size_t available = 0;
  Result rv = consumer->BeginRead(&buffer, &available);
  ASSERT_EQ(Result::kShouldWait, rv);

  writable.reset();

  rv = consumer->BeginRead(&buffer, &available);
  ASSERT_EQ(Result::kError, rv);

  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
}

TEST_F(DataPipeBytesConsumerTest, SignalExcessSizeAfterEndOfData) {
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::ScopedDataPipeProducerHandle writable;
  const MojoCreateDataPipeOptions options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 0};
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, writable, readable));
  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(readable), &notifier);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  writable.reset();

  const char* buffer = nullptr;
  size_t available = 0;
  Result rv = consumer->BeginRead(&buffer, &available);
  ASSERT_EQ(Result::kShouldWait, rv);

  notifier->SignalSize(1);

  rv = consumer->BeginRead(&buffer, &available);
  ASSERT_EQ(Result::kError, rv);

  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
}

TEST_F(DataPipeBytesConsumerTest, SignalSizeAfterRead) {
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::ScopedDataPipeProducerHandle writable;
  const MojoCreateDataPipeOptions options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 0};
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, writable, readable));

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(readable), &notifier);

  const std::string_view kData = "hello";
  size_t actually_written_bytes = 0;
  MojoResult write_result =
      writable->WriteData(base::as_byte_span(kData), MOJO_WRITE_DATA_FLAG_NONE,
                          actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, write_result);
  ASSERT_EQ(5u, actually_written_bytes);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  ASSERT_EQ(Result::kOk, rv);
  EXPECT_EQ(available, 5u);

  rv = consumer->EndRead(5);
  ASSERT_EQ(Result::kOk, rv);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  notifier->SignalSize(5);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(DataPipeBytesConsumerTest, ErrorBeforeEndOfPipe) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(consumer_handle), &notifier);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  notifier->SignalError(BytesConsumer::Error());
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kError, rv);

  producer_handle.reset();
  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kError, rv);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
}

// Verify that draining the DataPipe and SignalComplete() will
// close the DataPipeBytesConsumer.
TEST_F(DataPipeBytesConsumerTest, DrainPipeBeforeComplete) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(consumer_handle), &notifier);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  mojo::ScopedDataPipeConsumerHandle drained = consumer->DrainAsDataPipe();
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  notifier->SignalComplete();
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kDone, rv);
}

TEST_F(DataPipeBytesConsumerTest, CompleteBeforeDrainPipe) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* notifier = nullptr;
  DataPipeBytesConsumer* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner_, std::move(consumer_handle), &notifier);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  notifier->SignalComplete();
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  mojo::ScopedDataPipeConsumerHandle drained = consumer->DrainAsDataPipe();
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kDone, rv);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
}

}  // namespace blink
