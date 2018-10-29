// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/data_pipe_bytes_consumer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
class DataPipeBytesConsumerTest : public PageTestBase {
 public:
  using PublicState = BytesConsumer::PublicState;
  using Result = BytesConsumer::Result;
  void SetUp() override { PageTestBase::SetUp(IntSize()); }
};

TEST_F(DataPipeBytesConsumerTest, TwoPhaseRead) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  const std::string kData = "Such hospitality. I'm underwhelmed.";
  uint32_t write_size = kData.size();

  MojoResult rv = pipe.producer_handle->WriteData(kData.c_str(), &write_size,
                                                  MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  ASSERT_EQ(kData.size(), write_size);

  // Close the producer so the consumer will reach the kDone state after
  // completion is signaled below.
  pipe.producer_handle.reset();

  DataPipeBytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));
  consumer->SignalComplete();
  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(consumer))->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ(
      kData,
      BytesConsumerTestUtil::CharVectorToString(result.second).Utf8().data());
}

TEST_F(DataPipeBytesConsumerTest, TwoPhaseRead_SignalError) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  const std::string kData = "Such hospitality. I'm underwhelmed.";
  uint32_t write_size = kData.size();

  MojoResult rv = pipe.producer_handle->WriteData(kData.c_str(), &write_size,
                                                  MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  ASSERT_EQ(kData.size(), write_size);

  pipe.producer_handle.reset();

  DataPipeBytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));

  // Then explicitly signal an error.  This should override the pipe completion
  // and result in kError.
  consumer->SignalError();

  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(consumer))->Run();
  EXPECT_EQ(Result::kError, result.first);
  EXPECT_TRUE(result.second.IsEmpty());
}

// Verify that both the DataPipe must close and SignalComplete()
// must be called for the DataPipeBytesConsumer to reach the closed
// state.
TEST_F(DataPipeBytesConsumerTest, EndOfPipeBeforeComplete) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  DataPipeBytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  pipe.producer_handle.reset();
  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  consumer->SignalComplete();
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kDone, rv);
}

TEST_F(DataPipeBytesConsumerTest, CompleteBeforeEndOfPipe) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  DataPipeBytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  consumer->SignalComplete();
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  pipe.producer_handle.reset();
  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kDone, rv);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
}

// Verify that SignalError moves the DataPipeBytesConsumer to the
// errored state immediately without waiting for the end of the
// DataPipe.
TEST_F(DataPipeBytesConsumerTest, EndOfPipeBeforeError) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  DataPipeBytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  pipe.producer_handle.reset();
  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  consumer->SignalError();
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kError, rv);
}

TEST_F(DataPipeBytesConsumerTest, ErrorBeforeEndOfPipe) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  DataPipeBytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  consumer->SignalError();
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kError, rv);

  pipe.producer_handle.reset();
  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kError, rv);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
}

// Verify that draining the DataPipe and SignalComplete() will
// close the DataPipeBytesConsumer.
TEST_F(DataPipeBytesConsumerTest, DrainPipeBeforeComplete) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  DataPipeBytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));

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

  consumer->SignalComplete();
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());

  rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kDone, rv);
}

TEST_F(DataPipeBytesConsumerTest, CompleteBeforeDrainPipe) {
  mojo::DataPipe pipe;
  ASSERT_TRUE(pipe.producer_handle.is_valid());

  DataPipeBytesConsumer* consumer = new DataPipeBytesConsumer(
      &GetDocument(), std::move(pipe.consumer_handle));

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  const char* buffer = nullptr;
  size_t available = 0;

  Result rv = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(Result::kShouldWait, rv);

  consumer->SignalComplete();
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
