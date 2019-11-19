// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

using PublicState = BytesConsumer::PublicState;
using Result = BytesConsumer::Result;

class BlobBytesConsumerTestClient final
    : public GarbageCollected<BlobBytesConsumerTestClient>,
      public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(BlobBytesConsumerTestClient);

 public:
  void OnStateChange() override { ++num_on_state_change_called_; }
  String DebugName() const override { return "BlobBytesConsumerTestClient"; }
  int NumOnStateChangeCalled() const { return num_on_state_change_called_; }

 private:
  int num_on_state_change_called_ = 0;
};

class BlobBytesConsumerTest : public PageTestBase {
 public:
  void SetUp() override { PageTestBase::SetUp(IntSize(1, 1)); }
  scoped_refptr<BlobDataHandle> CreateBlob(const String& body) {
    mojo::PendingRemote<mojom::blink::Blob> mojo_blob;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FakeBlob>(kBlobUUID, body, &blob_state_),
        mojo_blob.InitWithNewPipeAndPassReceiver());
    return BlobDataHandle::Create(kBlobUUID, "", body.length(),
                                  std::move(mojo_blob));
  }

  bool DidStartLoading() {
    base::RunLoop().RunUntilIdle();
    return blob_state_.did_initiate_read_operation;
  }

 private:
  const String kBlobUUID = "blob-id";
  FakeBlob::State blob_state_;
};

TEST_F(BlobBytesConsumerTest, TwoPhaseRead) {
  String body = "hello, world";
  scoped_refptr<BlobDataHandle> blob_data_handle = CreateBlob(body);

  BlobBytesConsumer* consumer =
      MakeGarbageCollected<BlobBytesConsumer>(&GetDocument(), blob_data_handle);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());

  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(&buffer, &available));
  EXPECT_TRUE(DidStartLoading());
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize));
  EXPECT_FALSE(consumer->DrainAsFormData());
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());

  auto result =
      (MakeGarbageCollected<BytesConsumerTestReader>(consumer))->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ("hello, world",
            BytesConsumerTestUtil::CharVectorToString(result.second));
}

TEST_F(BlobBytesConsumerTest, CancelBeforeStarting) {
  scoped_refptr<BlobDataHandle> blob_data_handle = CreateBlob("foo bar");
  BlobBytesConsumer* consumer =
      MakeGarbageCollected<BlobBytesConsumer>(&GetDocument(), blob_data_handle);
  BlobBytesConsumerTestClient* client =
      MakeGarbageCollected<BlobBytesConsumerTestClient>();
  consumer->SetClient(client);

  consumer->Cancel();

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());
  EXPECT_EQ(0, client->NumOnStateChangeCalled());
}

TEST_F(BlobBytesConsumerTest, CancelAfterStarting) {
  scoped_refptr<BlobDataHandle> blob_data_handle = CreateBlob("foo bar");
  BlobBytesConsumer* consumer =
      MakeGarbageCollected<BlobBytesConsumer>(&GetDocument(), blob_data_handle);
  BlobBytesConsumerTestClient* client =
      MakeGarbageCollected<BlobBytesConsumerTestClient>();
  consumer->SetClient(client);

  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(0, client->NumOnStateChangeCalled());

  consumer->Cancel();
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(0, client->NumOnStateChangeCalled());
  EXPECT_TRUE(DidStartLoading());
}

TEST_F(BlobBytesConsumerTest, DrainAsBlobDataHandle) {
  String body = "hello, world";
  scoped_refptr<BlobDataHandle> blob_data_handle = CreateBlob(body);
  BlobBytesConsumer* consumer =
      MakeGarbageCollected<BlobBytesConsumer>(&GetDocument(), blob_data_handle);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());

  scoped_refptr<BlobDataHandle> result = consumer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize);
  ASSERT_TRUE(result);
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize));
  EXPECT_EQ(body.length(), result->size());
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());
}

TEST_F(BlobBytesConsumerTest, DrainAsBlobDataHandle_2) {
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create("uuid", "", std::numeric_limits<uint64_t>::max(),
                             CreateBlob("foo bar")->CloneBlobRemote());
  BlobBytesConsumer* consumer =
      MakeGarbageCollected<BlobBytesConsumer>(&GetDocument(), blob_data_handle);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());

  scoped_refptr<BlobDataHandle> result = consumer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize);
  ASSERT_TRUE(result);
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize));
  EXPECT_EQ(UINT64_MAX, result->size());
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());
}

TEST_F(BlobBytesConsumerTest, DrainAsBlobDataHandle_3) {
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create("uuid", "", std::numeric_limits<uint64_t>::max(),
                             CreateBlob("foo bar")->CloneBlobRemote());
  BlobBytesConsumer* consumer =
      MakeGarbageCollected<BlobBytesConsumer>(&GetDocument(), blob_data_handle);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());

  EXPECT_FALSE(consumer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize));
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());
}

TEST_F(BlobBytesConsumerTest, DrainAsFormData) {
  String body = "hello, world";
  scoped_refptr<BlobDataHandle> blob_data_handle = CreateBlob(body);
  BlobBytesConsumer* consumer =
      MakeGarbageCollected<BlobBytesConsumer>(&GetDocument(), blob_data_handle);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());

  scoped_refptr<EncodedFormData> result = consumer->DrainAsFormData();
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, result->Elements().size());
  ASSERT_EQ(FormDataElement::kEncodedBlob, result->Elements()[0].type_);
  ASSERT_TRUE(result->Elements()[0].optional_blob_data_handle_);
  EXPECT_EQ(body.length(),
            result->Elements()[0].optional_blob_data_handle_->size());
  EXPECT_EQ(blob_data_handle->Uuid(), result->Elements()[0].blob_uuid_);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_FALSE(DidStartLoading());
}

TEST_F(BlobBytesConsumerTest, ConstructedFromNullHandle) {
  BlobBytesConsumer* consumer =
      MakeGarbageCollected<BlobBytesConsumer>(&GetDocument(), nullptr);
  const char* buffer = nullptr;
  size_t available;
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
}

}  // namespace

}  // namespace blink
