// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/place_holder_bytes_consumer.h"

#include <utility>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

namespace blink {
namespace {

class PlaceHolderBytesConsumerTest : public testing::Test {
 public:
  using Command = ReplayingBytesConsumer::Command;
  using PublicState = BytesConsumer::PublicState;
  using Result = BytesConsumer::Result;
  using BlobSizePolicy = BytesConsumer::BlobSizePolicy;
};

TEST_F(PlaceHolderBytesConsumerTest, Construct) {
  auto* consumer = MakeGarbageCollected<PlaceHolderBytesConsumer>();

  const char* buffer;
  size_t available;

  EXPECT_EQ(consumer->GetPublicState(), PublicState::kReadableOrWaiting);
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle(
      BlobSizePolicy::kAllowBlobWithInvalidSize));
  EXPECT_FALSE(consumer->DrainAsFormData());
  EXPECT_FALSE(consumer->DrainAsDataPipe());

  Result result = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kShouldWait);
  EXPECT_EQ(buffer, nullptr);
  EXPECT_EQ(available, 0u);
}

TEST_F(PlaceHolderBytesConsumerTest, Update) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<PlaceHolderBytesConsumer>();

  auto* actual_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  actual_bytes_consumer->Add(Command(Command::kDataAndDone, "hello"));

  const char* buffer;
  size_t available;

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  ASSERT_EQ(Result::kShouldWait, consumer->BeginRead(&buffer, &available));
  consumer->Update(actual_bytes_consumer);

  EXPECT_EQ(consumer->GetPublicState(), PublicState::kReadableOrWaiting);

  Result result = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kOk);
  ASSERT_EQ(available, 5u);
  EXPECT_EQ(String(buffer, available), "hello");

  result = consumer->EndRead(available);
  EXPECT_EQ(result, Result::kDone);

  EXPECT_EQ(consumer->GetPublicState(), PublicState::kClosed);
}

TEST_F(PlaceHolderBytesConsumerTest, DrainAsDataPipe) {
  mojo::ScopedDataPipeConsumerHandle consumer_end;
  mojo::ScopedDataPipeProducerHandle producer_end;
  auto result = mojo::CreateDataPipe(nullptr, producer_end, consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<PlaceHolderBytesConsumer>();
  auto* actual_bytes_consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);

  EXPECT_EQ(consumer->GetPublicState(), PublicState::kReadableOrWaiting);
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle(
      BlobSizePolicy::kAllowBlobWithInvalidSize));
  EXPECT_FALSE(consumer->DrainAsFormData());
  EXPECT_FALSE(consumer->DrainAsDataPipe());

  consumer->Update(actual_bytes_consumer);

  EXPECT_TRUE(consumer->DrainAsDataPipe());
}

TEST_F(PlaceHolderBytesConsumerTest, Cancel) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* actual_bytes_consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  actual_bytes_consumer->Add(Command(Command::kData, "hello"));
  auto* consumer = MakeGarbageCollected<PlaceHolderBytesConsumer>();

  const char* buffer;
  size_t available;

  EXPECT_EQ(consumer->GetPublicState(), PublicState::kReadableOrWaiting);

  consumer->Cancel();

  EXPECT_EQ(consumer->GetPublicState(), PublicState::kClosed);
  Result result = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kDone);
  EXPECT_EQ(buffer, nullptr);
  EXPECT_EQ(available, 0u);

  consumer->Update(actual_bytes_consumer);

  EXPECT_EQ(consumer->GetPublicState(), PublicState::kClosed);
  result = consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(result, Result::kDone);
  EXPECT_EQ(buffer, nullptr);
  EXPECT_EQ(available, 0u);
}

}  // namespace
}  // namespace blink
