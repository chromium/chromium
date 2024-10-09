// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob_registry.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::SetArgReferee;
using testing::StrictMock;
using Checkpoint = StrictMock<testing::MockFunction<void(int)>>;
using MockFetchDataLoaderClient =
    BytesConsumerTestUtil::MockFetchDataLoaderClient;
using MockBytesConsumer = BytesConsumerTestUtil::MockBytesConsumer;
using Result = BytesConsumer::Result;

constexpr char kQuickBrownFox[] = "Quick brown fox";
constexpr size_t kQuickBrownFoxLength = 15;
constexpr size_t kQuickBrownFoxLengthWithTerminatingNull = 16;
constexpr char kQuickBrownFoxFormData[] =
    "--boundary\r\n"
    "Content-Disposition: form-data; name=blob; filename=blob\r\n"
    "Content-Type: text/plain; charset=iso-8859-1\r\n"
    "\r\n"
    "Quick brown fox\r\n"
    "--boundary\r\n"
    "Content-Disposition: form-data; name=\"blob\xC2\xA0without\xC2\xA0type\"; "
    "filename=\"blob\xC2\xA0without\xC2\xA0type.txt\"\r\n"
    "\r\n"
    "Quick brown fox\r\n"
    "--boundary\r\n"
    "Content-Disposition: form-data; name=string\r\n"
    "\r\n"
    "Quick brown fox\r\n"
    "--boundary\r\n"
    "Content-Disposition: form-data; name=string-with-type\r\n"
    "Content-Type: text/plain; charset=invalid\r\n"
    "\r\n"
    "Quick brown fox\r\n"
    "--boundary--\r\n";
constexpr size_t kQuickBrownFoxFormDataLength =
    std::size(kQuickBrownFoxFormData) - 1u;

class FetchDataLoaderTest : public testing::Test {
 protected:
  struct PipingClient : public GarbageCollected<PipingClient>,
                        public FetchDataLoader::Client {
   public:
    explicit PipingClient(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner)
        : task_runner_(std::move(task_runner)) {}

    void DidFetchDataStartedDataPipe(
        mojo::ScopedDataPipeConsumerHandle handle) override {
      DataPipeBytesConsumer::CompletionNotifier* notifier;
      destination_ = MakeGarbageCollected<DataPipeBytesConsumer>(
          task_runner_, std::move(handle), &notifier);
      completion_notifier_ = notifier;
    }
    void DidFetchDataLoadedDataPipe() override {
      completion_notifier_->SignalComplete();
    }
    void DidFetchDataLoadFailed() override {
      completion_notifier_->SignalError(BytesConsumer::Error());
    }
    void Abort() override {
      completion_notifier_->SignalError(BytesConsumer::Error());
    }

    BytesConsumer* GetDestination() { return destination_.Get(); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(destination_);
      visitor->Trace(completion_notifier_);
      FetchDataLoader::Client::Trace(visitor);
    }

   private:
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    Member<BytesConsumer> destination_;
    Member<DataPipeBytesConsumer::CompletionNotifier> completion_notifier_;
  };
  test::TaskEnvironment task_environment_;
};

class FetchDataLoaderBlobTest : public FetchDataLoaderTest {
 public:
  FetchDataLoaderBlobTest()
      : fake_task_runner_(base::MakeRefCounted<scheduler::FakeTaskRunner>()),
        blob_registry_receiver_(
            &fake_blob_registry_,
            blob_registry_remote_.BindNewPipeAndPassReceiver()) {
    BlobDataHandle::SetBlobRegistryForTesting(blob_registry_remote_.get());
  }

  ~FetchDataLoaderBlobTest() override {
    BlobDataHandle::SetBlobRegistryForTesting(nullptr);
  }

 protected:
  scoped_refptr<scheduler::FakeTaskRunner> fake_task_runner_;

 private:
  FakeBlobRegistry fake_blob_registry_;
  mojo::Remote<mojom::blink::BlobRegistry> blob_registry_remote_;
  mojo::Receiver<mojom::blink::BlobRegistry> blob_registry_receiver_;
};

ACTION_P(QUITLOOP, loop) {
  loop->Quit();
}

TEST_F(FetchDataLoaderBlobTest, LoadAsBlob) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test", fake_task_runner_);
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();
  scoped_refptr<BlobDataHandle> blob_data_handle;

  base::RunLoop run_loop;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, DrainAsDataPipe());
  EXPECT_CALL(*consumer, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(
          SetArgReferee<0>(base::span_with_nul_from_cstring(kQuickBrownFox)),
          Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedBlobHandleMock(_))
      .WillOnce(DoAll(SaveArg<0>(&blob_data_handle), QUITLOOP(&run_loop)));
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  // Pump the |task_runner| to process the data pipe's task indicating its
  // writable.
  fake_task_runner_->RunUntilIdle();
  checkpoint.Call(3);
  client->OnStateChange();
  run_loop.Run();
  checkpoint.Call(4);

  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blob_data_handle->size());
  EXPECT_EQ(String("text/test"), blob_data_handle->GetType());
}

TEST_F(FetchDataLoaderBlobTest, LoadAsBlobFailed) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test", fake_task_runner_);
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, DrainAsDataPipe());
  EXPECT_CALL(*consumer, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(
          SetArgReferee<0>(base::span_with_nul_from_cstring(kQuickBrownFox)),
          Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kError));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  // Pump the |task_runner| to process the data pipe's task indicating its
  // writable.
  fake_task_runner_->RunUntilIdle();
  checkpoint.Call(3);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(4);
}

TEST_F(FetchDataLoaderBlobTest, LoadAsBlobCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test", fake_task_runner_);
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, DrainAsDataPipe());
  EXPECT_CALL(*consumer, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  // Pump the |task_runner| to process the data pipe's task indicating its
  // writable.
  fake_task_runner_->RunUntilIdle();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST_F(FetchDataLoaderBlobTest, LoadAsBlobNoClientCallbacksAfterCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test", fake_task_runner_);
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();
  scoped_refptr<BlobDataHandle> blob_data_handle;

  base::RunLoop run_loop;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, DrainAsDataPipe());
  EXPECT_CALL(*consumer, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(
          SetArgReferee<0>(base::span_with_nul_from_cstring(kQuickBrownFox)),
          Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(4));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(5));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*consumer, Cancel());
  // This should never happen due to explicit FetchDataLoader::Cancel call.
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedBlobHandleMock(_))
      .Times(0);
  EXPECT_CALL(checkpoint, Call(6));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fake_task_runner_->RunUntilIdle();
  checkpoint.Call(3);
  client->OnStateChange();
  run_loop.RunUntilIdle();
  checkpoint.Call(4);
  // Cancel the load to verify no FetchDataLoader::Client calls happen
  // afterwards.
  fetch_data_loader->Cancel();
  checkpoint.Call(5);
  client->OnStateChange();
  run_loop.RunUntilIdle();
  checkpoint.Call(6);
}

TEST_F(FetchDataLoaderBlobTest,
       LoadAsBlobViaDrainAsBlobDataHandleWithSameContentType) {
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendBytes(
      base::as_bytes(base::span_with_nul_from_cstring(kQuickBrownFox)));
  blob_data->SetContentType("text/test");
  scoped_refptr<BlobDataHandle> input_blob_data_handle = BlobDataHandle::Create(
      std::move(blob_data), kQuickBrownFoxLengthWithTerminatingNull);

  Checkpoint checkpoint;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test", fake_task_runner_);
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();
  scoped_refptr<BlobDataHandle> blob_data_handle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(input_blob_data_handle)));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blob_data_handle));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);

  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(input_blob_data_handle, blob_data_handle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blob_data_handle->size());
  EXPECT_EQ(String("text/test"), blob_data_handle->GetType());
}

TEST_F(FetchDataLoaderBlobTest,
       LoadAsBlobViaDrainAsBlobDataHandleWithDifferentContentType) {
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendBytes(
      base::as_bytes(base::span_with_nul_from_cstring(kQuickBrownFox)));
  blob_data->SetContentType("text/different");
  scoped_refptr<BlobDataHandle> input_blob_data_handle = BlobDataHandle::Create(
      std::move(blob_data), kQuickBrownFoxLengthWithTerminatingNull);

  Checkpoint checkpoint;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test", fake_task_runner_);
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();
  scoped_refptr<BlobDataHandle> blob_data_handle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(input_blob_data_handle)));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blob_data_handle));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);

  ASSERT_TRUE(blob_data_handle);
  EXPECT_NE(input_blob_data_handle, blob_data_handle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blob_data_handle->size());
  EXPECT_EQ(String("text/test"), blob_data_handle->GetType());
}

TEST_F(FetchDataLoaderTest, LoadAsArrayBuffer) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsArrayBuffer();
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();
  DOMArrayBuffer* array_buffer = nullptr;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(
          SetArgReferee<0>(base::span_with_nul_from_cstring(kQuickBrownFox)),
          Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedArrayBufferMock(_))
      .WillOnce(SaveArg<0>(&array_buffer));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);

  ASSERT_TRUE(array_buffer);
  ASSERT_EQ(kQuickBrownFoxLengthWithTerminatingNull,
            array_buffer->ByteLength());
  EXPECT_STREQ(kQuickBrownFox, static_cast<const char*>(array_buffer->Data()));
}

TEST_F(FetchDataLoaderTest, LoadAsArrayBufferFailed) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsArrayBuffer();
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(
          SetArgReferee<0>(base::span_with_nul_from_cstring(kQuickBrownFox)),
          Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kError));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST_F(FetchDataLoaderTest, LoadAsArrayBufferCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsArrayBuffer();
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);
}

TEST_F(FetchDataLoaderTest, LoadAsFormData) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsFormData("boundary");
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();
  FormData* form_data = nullptr;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(
          SetArgReferee<0>(base::span_from_cstring(kQuickBrownFoxFormData)),
          Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxFormDataLength))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedFormDataMock(_))
      .WillOnce(SaveArg<0>(&form_data));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);

  ASSERT_TRUE(form_data);
  ASSERT_EQ(4u, form_data->Entries().size());

  EXPECT_EQ("blob", form_data->Entries()[0]->name());
  EXPECT_EQ("blob", form_data->Entries()[0]->Filename());
  ASSERT_TRUE(form_data->Entries()[0]->isFile());
  EXPECT_EQ(kQuickBrownFoxLength, form_data->Entries()[0]->GetBlob()->size());
  EXPECT_EQ("text/plain; charset=iso-8859-1",
            form_data->Entries()[0]->GetBlob()->type());

  EXPECT_EQ("blob\xC2\xA0without\xC2\xA0type",
            form_data->Entries()[1]->name().Utf8());
  EXPECT_EQ("blob\xC2\xA0without\xC2\xA0type.txt",
            form_data->Entries()[1]->Filename().Utf8());
  ASSERT_TRUE(form_data->Entries()[1]->isFile());
  EXPECT_EQ(kQuickBrownFoxLength, form_data->Entries()[1]->GetBlob()->size());
  EXPECT_EQ("text/plain", form_data->Entries()[1]->GetBlob()->type());

  EXPECT_EQ("string", form_data->Entries()[2]->name());
  EXPECT_TRUE(form_data->Entries()[2]->Filename().IsNull());
  ASSERT_TRUE(form_data->Entries()[2]->IsString());
  EXPECT_EQ(kQuickBrownFox, form_data->Entries()[2]->Value());

  EXPECT_EQ("string-with-type", form_data->Entries()[3]->name());
  EXPECT_TRUE(form_data->Entries()[3]->Filename().IsNull());
  ASSERT_TRUE(form_data->Entries()[3]->IsString());
  EXPECT_EQ(kQuickBrownFox, form_data->Entries()[3]->Value());
}

TEST_F(FetchDataLoaderTest, LoadAsFormDataPartialInput) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsFormData("boundary");
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(
          DoAll(SetArgReferee<0>(base::span_from_cstring(kQuickBrownFoxFormData)
                                     .first(kQuickBrownFoxFormDataLength - 3u)),
                Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxFormDataLength - 3u))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST_F(FetchDataLoaderTest, LoadAsFormDataFailed) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsFormData("boundary");
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(
          SetArgReferee<0>(base::span_from_cstring(kQuickBrownFoxFormData)),
          Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxFormDataLength))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kError));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST_F(FetchDataLoaderTest, LoadAsFormDataCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsFormData("boundary");
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);
}

TEST_F(FetchDataLoaderTest, LoadAsString) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader = FetchDataLoader::CreateLoaderAsString(
      TextResourceDecoderOptions::CreateUTF8Decode());
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span_from_cstring(kQuickBrownFox)),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLength))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client,
              DidFetchDataLoadedString(String(kQuickBrownFox)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST_F(FetchDataLoaderTest, LoadAsStringWithNullBytes) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader = FetchDataLoader::CreateLoaderAsString(
      TextResourceDecoderOptions::CreateUTF8Decode());
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  constexpr char kPattern[] = "Quick\0brown\0fox";
  constexpr size_t kLength = sizeof(kPattern);

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(
          DoAll(SetArgReferee<0>(base::span_with_nul_from_cstring(kPattern)),
                Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(16)).WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client,
              DidFetchDataLoadedString(String(kPattern, kLength)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST_F(FetchDataLoaderTest, LoadAsStringError) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader = FetchDataLoader::CreateLoaderAsString(
      TextResourceDecoderOptions::CreateUTF8Decode());
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span_from_cstring(kQuickBrownFox)),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLength))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_)).WillOnce(Return(Result::kError));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST_F(FetchDataLoaderTest, LoadAsStringCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  auto* consumer = MakeGarbageCollected<MockBytesConsumer>();

  FetchDataLoader* fetch_data_loader = FetchDataLoader::CreateLoaderAsString(
      TextResourceDecoderOptions::CreateUTF8Decode());
  auto* fetch_data_loader_client =
      MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_))
      .WillOnce(DoAll(SetArgReferee<0>(base::span<const char>{}),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);
}

TEST_F(FetchDataLoaderTest, LoadAsDataPipeWithCopy) {
  using Command = ReplayingBytesConsumer::Command;
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* src = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  src->Add(Command(Command::Name::kData, "hello, "));
  src->Add(Command(Command::Name::kDataAndDone, "world"));

  auto* loader = FetchDataLoader::CreateLoaderAsDataPipe(task_runner);
  auto* client = MakeGarbageCollected<PipingClient>(task_runner);
  loader->Start(src, client);

  BytesConsumer* dest = client->GetDestination();
  ASSERT_TRUE(dest);

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(dest);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(result.first, BytesConsumer::Result::kDone);
  EXPECT_EQ(String(result.second.data(), result.second.size()), "hello, world");
}

TEST_F(FetchDataLoaderTest, LoadAsDataPipeWithCopyFailure) {
  using Command = ReplayingBytesConsumer::Command;
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* src = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  src->Add(Command(Command::Name::kData, "hello, "));
  src->Add(Command(Command::Name::kError));

  auto* loader = FetchDataLoader::CreateLoaderAsDataPipe(task_runner);
  auto* client = MakeGarbageCollected<PipingClient>(task_runner);
  loader->Start(src, client);

  BytesConsumer* dest = client->GetDestination();
  ASSERT_TRUE(dest);

  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(dest);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(result.first, BytesConsumer::Result::kError);
}

TEST_F(FetchDataLoaderTest, LoadAsDataPipeFromDataPipe) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::ScopedDataPipeProducerHandle writable;
  MojoResult rv = mojo::CreateDataPipe(nullptr, writable, readable);
  ASSERT_EQ(rv, MOJO_RESULT_OK);

  ASSERT_TRUE(mojo::BlockingCopyFromString("hello", writable));

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;
  auto* src = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(readable), &completion_notifier);

  auto* loader = FetchDataLoader::CreateLoaderAsDataPipe(task_runner);
  auto* client = MakeGarbageCollected<PipingClient>(task_runner);
  loader->Start(src, client);

  BytesConsumer* dest = client->GetDestination();
  ASSERT_TRUE(dest);

  base::span<const char> buffer;
  auto result = dest->BeginRead(buffer);
  ASSERT_EQ(result, BytesConsumer::Result::kOk);
  EXPECT_EQ(buffer.size(), 5u);
  EXPECT_EQ(std::string(buffer.data(), buffer.size()), "hello");
  result = dest->EndRead(buffer.size());
  ASSERT_EQ(result, BytesConsumer::Result::kOk);

  result = dest->BeginRead(buffer);
  ASSERT_EQ(result, BytesConsumer::Result::kShouldWait);

  writable.reset();
  result = dest->BeginRead(buffer);
  ASSERT_EQ(result, BytesConsumer::Result::kShouldWait);

  completion_notifier->SignalComplete();
  result = dest->BeginRead(buffer);
  ASSERT_EQ(result, BytesConsumer::Result::kDone);
}

TEST_F(FetchDataLoaderTest, LoadAsDataPipeFromDataPipeFailure) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::ScopedDataPipeProducerHandle writable;
  MojoResult rv = mojo::CreateDataPipe(nullptr, writable, readable);
  ASSERT_EQ(rv, MOJO_RESULT_OK);

  ASSERT_TRUE(mojo::BlockingCopyFromString("hello", writable));

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;
  auto* src = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(readable), &completion_notifier);

  auto* loader = FetchDataLoader::CreateLoaderAsDataPipe(task_runner);
  auto* client = MakeGarbageCollected<PipingClient>(task_runner);
  loader->Start(src, client);

  BytesConsumer* dest = client->GetDestination();
  ASSERT_TRUE(dest);

  completion_notifier->SignalError(BytesConsumer::Error());
  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(dest);
  auto result = reader->Run(task_runner.get());

  EXPECT_EQ(result.first, BytesConsumer::Result::kError);
}

}  // namespace

}  // namespace blink
