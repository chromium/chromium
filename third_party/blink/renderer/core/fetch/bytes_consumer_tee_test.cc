// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/bytes_consumer_tee.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using Result = BytesConsumer::Result;

class BytesConsumerTestClient final
    : public GarbageCollected<BytesConsumerTestClient>,
      public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(BytesConsumerTestClient);

 public:
  void OnStateChange() override { ++num_on_state_change_called_; }
  String DebugName() const override { return "BytesConsumerTestClient"; }
  int NumOnStateChangeCalled() const { return num_on_state_change_called_; }

 private:
  int num_on_state_change_called_ = 0;
};

class BytesConsumerTeeTest : public PageTestBase {
 public:
  using Command = ReplayingBytesConsumer::Command;
  void SetUp() override { PageTestBase::SetUp(IntSize()); }
};

class FakeBlobBytesConsumer : public BytesConsumer {
 public:
  explicit FakeBlobBytesConsumer(scoped_refptr<BlobDataHandle> handle)
      : blob_handle_(std::move(handle)) {}
  ~FakeBlobBytesConsumer() override {}

  Result BeginRead(const char** buffer, size_t* available) override {
    if (state_ == PublicState::kClosed)
      return Result::kDone;
    blob_handle_ = nullptr;
    state_ = PublicState::kErrored;
    return Result::kError;
  }
  Result EndRead(size_t read_size) override {
    if (state_ == PublicState::kClosed)
      return Result::kError;
    blob_handle_ = nullptr;
    state_ = PublicState::kErrored;
    return Result::kError;
  }
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    if (state_ != PublicState::kReadableOrWaiting)
      return nullptr;
    DCHECK(blob_handle_);
    if (policy == BlobSizePolicy::kDisallowBlobWithInvalidSize &&
        blob_handle_->size() == UINT64_MAX)
      return nullptr;
    state_ = PublicState::kClosed;
    return std::move(blob_handle_);
  }

  void SetClient(Client*) override {}
  void ClearClient() override {}
  void Cancel() override {}
  PublicState GetPublicState() const override { return state_; }
  Error GetError() const override { return Error(); }
  String DebugName() const override { return "FakeBlobBytesConsumer"; }

 private:
  PublicState state_ = PublicState::kReadableOrWaiting;
  scoped_refptr<BlobDataHandle> blob_handle_;
};

class FakeFormDataBytesConsumer : public BytesConsumer {
 public:
  explicit FakeFormDataBytesConsumer(scoped_refptr<EncodedFormData> form_data)
      : form_data_(std::move(form_data)) {}
  ~FakeFormDataBytesConsumer() override {}

  Result BeginRead(const char** buffer, size_t* available) override {
    if (state_ == PublicState::kClosed)
      return Result::kDone;
    form_data_ = nullptr;
    state_ = PublicState::kErrored;
    return Result::kError;
  }
  Result EndRead(size_t read_size) override {
    if (state_ == PublicState::kClosed)
      return Result::kError;
    form_data_ = nullptr;
    state_ = PublicState::kErrored;
    return Result::kError;
  }
  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    if (state_ != PublicState::kReadableOrWaiting)
      return nullptr;
    DCHECK(form_data_);
    return std::move(form_data_);
  }

  void SetClient(Client*) override {}
  void ClearClient() override {}
  void Cancel() override {}
  PublicState GetPublicState() const override { return state_; }
  Error GetError() const override { return Error(); }
  String DebugName() const override { return "FakeFormDataBytesConsumer"; }

 private:
  PublicState state_ = PublicState::kReadableOrWaiting;
  scoped_refptr<EncodedFormData> form_data_;
};

TEST_F(BytesConsumerTeeTest, CreateDone) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kDone));
  EXPECT_FALSE(src->IsCancelled());

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  auto result1 = (MakeGarbageCollected<BytesConsumerTestReader>(dest1))->Run();
  auto result2 = (MakeGarbageCollected<BytesConsumerTestReader>(dest2))->Run();

  EXPECT_EQ(Result::kDone, result1.first);
  EXPECT_TRUE(result1.second.IsEmpty());
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(Result::kDone, result2.first);
  EXPECT_TRUE(result2.second.IsEmpty());
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());

  // Cancelling does nothing when closed.
  dest1->Cancel();
  dest2->Cancel();
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());
}

TEST_F(BytesConsumerTeeTest, TwoPhaseRead) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));

  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "hello, "));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "world"));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kDone));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest2->GetPublicState());

  auto result1 = (MakeGarbageCollected<BytesConsumerTestReader>(dest1))->Run();
  auto result2 = (MakeGarbageCollected<BytesConsumerTestReader>(dest2))->Run();

  EXPECT_EQ(Result::kDone, result1.first);
  EXPECT_EQ("hello, world",
            BytesConsumerTestUtil::CharVectorToString(result1.second));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(Result::kDone, result2.first);
  EXPECT_EQ("hello, world",
            BytesConsumerTestUtil::CharVectorToString(result2.second));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());
}

TEST_F(BytesConsumerTeeTest, TwoPhaseReadWithDataAndDone) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));

  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "hello, "));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kDataAndDone, "world"));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest2->GetPublicState());

  auto result1 = (MakeGarbageCollected<BytesConsumerTestReader>(dest1))->Run();
  auto result2 = (MakeGarbageCollected<BytesConsumerTestReader>(dest2))->Run();

  EXPECT_EQ(Result::kDone, result1.first);
  EXPECT_EQ("hello, world",
            BytesConsumerTestUtil::CharVectorToString(result1.second));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(Result::kDone, result2.first);
  EXPECT_EQ("hello, world",
            BytesConsumerTestUtil::CharVectorToString(result2.second));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());
}

TEST_F(BytesConsumerTeeTest, Error) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));

  src->Add(Command(Command::kData, "hello, "));
  src->Add(Command(Command::kData, "world"));
  src->Add(Command(Command::kError));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::kErrored, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, dest2->GetPublicState());

  auto result1 = (MakeGarbageCollected<BytesConsumerTestReader>(dest1))->Run();
  auto result2 = (MakeGarbageCollected<BytesConsumerTestReader>(dest2))->Run();

  EXPECT_EQ(Result::kError, result1.first);
  EXPECT_TRUE(result1.second.IsEmpty());
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, dest1->GetPublicState());
  EXPECT_EQ(Result::kError, result2.first);
  EXPECT_TRUE(result2.second.IsEmpty());
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());

  // Cancelling does nothing when errored.
  dest1->Cancel();
  dest2->Cancel();
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());
}

TEST_F(BytesConsumerTeeTest, Cancel) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));

  src->Add(Command(Command::kData, "hello, "));
  src->Add(Command(Command::kWait));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest2->GetPublicState());

  EXPECT_FALSE(src->IsCancelled());
  dest1->Cancel();
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());
  dest2->Cancel();
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest2->GetPublicState());
  EXPECT_TRUE(src->IsCancelled());
}

TEST_F(BytesConsumerTeeTest, CancelShouldNotAffectTheOtherDestination) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));

  src->Add(Command(Command::kData, "hello, "));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "world"));
  src->Add(Command(Command::kDone));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest2->GetPublicState());

  EXPECT_FALSE(src->IsCancelled());
  dest1->Cancel();
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());

  auto result2 = (MakeGarbageCollected<BytesConsumerTestReader>(dest2))->Run();

  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest2->GetPublicState());
  EXPECT_EQ(Result::kDone, result2.first);
  EXPECT_EQ("hello, world",
            BytesConsumerTestUtil::CharVectorToString(result2.second));
  EXPECT_FALSE(src->IsCancelled());
}

TEST_F(BytesConsumerTeeTest, CancelShouldNotAffectTheOtherDestination2) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));

  src->Add(Command(Command::kData, "hello, "));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "world"));
  src->Add(Command(Command::kError));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest2->GetPublicState());

  EXPECT_FALSE(src->IsCancelled());
  dest1->Cancel();
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest2->GetPublicState());
  EXPECT_FALSE(src->IsCancelled());

  auto result2 = (MakeGarbageCollected<BytesConsumerTestReader>(dest2))->Run();

  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, dest2->GetPublicState());
  EXPECT_EQ(Result::kError, result2.first);
  EXPECT_FALSE(src->IsCancelled());
}

TEST_F(BytesConsumerTeeTest, BlobHandle) {
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::make_unique<BlobData>(), 12345);
  BytesConsumer* src =
      MakeGarbageCollected<FakeBlobBytesConsumer>(blob_data_handle);

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  scoped_refptr<BlobDataHandle> dest_blob_data_handle1 =
      dest1->DrainAsBlobDataHandle(
          BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize);
  scoped_refptr<BlobDataHandle> dest_blob_data_handle2 =
      dest2->DrainAsBlobDataHandle(
          BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize);
  ASSERT_TRUE(dest_blob_data_handle1);
  ASSERT_TRUE(dest_blob_data_handle2);
  EXPECT_EQ(12345u, dest_blob_data_handle1->size());
  EXPECT_EQ(12345u, dest_blob_data_handle2->size());
}

TEST_F(BytesConsumerTeeTest, BlobHandleWithInvalidSize) {
  scoped_refptr<BlobDataHandle> blob_data_handle = BlobDataHandle::Create(
      std::make_unique<BlobData>(), std::numeric_limits<uint64_t>::max());
  BytesConsumer* src =
      MakeGarbageCollected<FakeBlobBytesConsumer>(blob_data_handle);

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  scoped_refptr<BlobDataHandle> dest_blob_data_handle1 =
      dest1->DrainAsBlobDataHandle(
          BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize);
  scoped_refptr<BlobDataHandle> dest_blob_data_handle2 =
      dest2->DrainAsBlobDataHandle(
          BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize);
  ASSERT_TRUE(dest_blob_data_handle1);
  ASSERT_FALSE(dest_blob_data_handle2);
  EXPECT_EQ(UINT64_MAX, dest_blob_data_handle1->size());
}

TEST_F(BytesConsumerTeeTest, FormData) {
  auto form_data = EncodedFormData::Create();

  auto* src = MakeGarbageCollected<FakeFormDataBytesConsumer>(form_data);

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  scoped_refptr<EncodedFormData> dest_form_data1 = dest1->DrainAsFormData();
  scoped_refptr<EncodedFormData> dest_form_data2 = dest2->DrainAsFormData();
  EXPECT_EQ(form_data, dest_form_data1);
  EXPECT_EQ(form_data, dest_form_data2);
}

TEST_F(BytesConsumerTeeTest, ConsumerCanBeErroredInTwoPhaseRead) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kData, "a"));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kError));

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);
  BytesConsumerTestClient* client =
      MakeGarbageCollected<BytesConsumerTestClient>();
  dest1->SetClient(client);

  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kOk, dest1->BeginRead(&buffer, &available));
  ASSERT_EQ(1u, available);

  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest1->GetPublicState());
  int num_on_state_change_called = client->NumOnStateChangeCalled();
  EXPECT_EQ(
      Result::kError,
      (MakeGarbageCollected<BytesConsumerTestReader>(dest2))->Run().first);
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, dest1->GetPublicState());
  EXPECT_EQ(num_on_state_change_called + 1, client->NumOnStateChangeCalled());
  EXPECT_EQ('a', buffer[0]);
  EXPECT_EQ(Result::kOk, dest1->EndRead(available));
}

TEST_F(BytesConsumerTeeTest,
       AsyncNotificationShouldBeDispatchedWhenAllDataIsConsumed) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kData, "a"));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kDone));
  BytesConsumerTestClient* client =
      MakeGarbageCollected<BytesConsumerTestClient>();

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  dest1->SetClient(client);

  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kOk, dest1->BeginRead(&buffer, &available));
  ASSERT_EQ(1u, available);
  EXPECT_EQ('a', buffer[0]);

  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            src->GetPublicState());
  test::RunPendingTasks();
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, src->GetPublicState());
  // Just for checking UAF.
  EXPECT_EQ('a', buffer[0]);
  ASSERT_EQ(Result::kOk, dest1->EndRead(1));

  EXPECT_EQ(0, client->NumOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest1->GetPublicState());
  test::RunPendingTasks();
  EXPECT_EQ(1, client->NumOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
}

TEST_F(BytesConsumerTeeTest,
       AsyncCloseNotificationShouldBeCancelledBySubsequentReadCall) {
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kData, "a"));
  src->Add(Command(Command::kDone));
  BytesConsumerTestClient* client =
      MakeGarbageCollected<BytesConsumerTestClient>();

  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumerTee(&GetDocument(), src, &dest1, &dest2);

  dest1->SetClient(client);

  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kOk, dest1->BeginRead(&buffer, &available));
  ASSERT_EQ(1u, available);
  EXPECT_EQ('a', buffer[0]);

  test::RunPendingTasks();
  // Just for checking UAF.
  EXPECT_EQ('a', buffer[0]);
  ASSERT_EQ(Result::kOk, dest1->EndRead(1));
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            dest1->GetPublicState());

  EXPECT_EQ(Result::kDone, dest1->BeginRead(&buffer, &available));
  EXPECT_EQ(0, client->NumOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
  test::RunPendingTasks();
  EXPECT_EQ(0, client->NumOnStateChangeCalled());
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, dest1->GetPublicState());
}

TEST(BytesConusmerTest, ClosedBytesConsumer) {
  BytesConsumer* consumer = BytesConsumer::CreateClosed();

  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST(BytesConusmerTest, ErroredBytesConsumer) {
  BytesConsumer::Error error("hello");
  BytesConsumer* consumer = BytesConsumer::CreateErrored(error);

  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kError, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(error.Message(), consumer->GetError().Message());

  consumer->Cancel();
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, consumer->GetPublicState());
}

}  // namespace

}  // namespace blink
