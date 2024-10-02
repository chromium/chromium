// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"

#include <memory>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob_registry.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using testing::InSequence;
using testing::Return;
using testing::_;
using testing::SaveArg;
using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;
using MockFetchDataLoaderClient =
    BytesConsumerTestUtil::MockFetchDataLoaderClient;

class BodyStreamBufferTest : public testing::Test {
 protected:
  using Command = ReplayingBytesConsumer::Command;
  ScriptValue Eval(ScriptState* script_state, const char* s) {
    v8::Local<v8::String> source;
    v8::Local<v8::Script> script;
    v8::MicrotasksScope microtasks(script_state->GetIsolate(),
                                   ToMicrotaskQueue(script_state),
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    if (!v8::String::NewFromUtf8(script_state->GetIsolate(), s,
                                 v8::NewStringType::kNormal)
             .ToLocal(&source)) {
      ADD_FAILURE();
      return ScriptValue();
    }
    if (!v8::Script::Compile(script_state->GetContext(), source)
             .ToLocal(&script)) {
      ADD_FAILURE() << "Compilation fails";
      return ScriptValue();
    }
    return ScriptValue(
        script_state->GetIsolate(),
        script->Run(script_state->GetContext()).ToLocalChecked());
  }
  ScriptValue EvalWithPrintingError(ScriptState* script_state, const char* s) {
    v8::TryCatch block(script_state->GetIsolate());
    ScriptValue r = Eval(script_state, s);
    if (block.HasCaught()) {
      ADD_FAILURE() << ToCoreString(script_state->GetIsolate(),
                                    block.Exception()
                                        ->ToString(script_state->GetContext())
                                        .ToLocalChecked())
                           .Utf8();
      block.ReThrow();
    }
    return r;
  }
  scoped_refptr<BlobDataHandle> CreateBlob(const String& body) {
    auto data = std::make_unique<BlobData>();
    data->AppendText(body, false);
    uint64_t length = data->length();
    return BlobDataHandle::Create(std::move(data), length);
  }

 private:
  test::TaskEnvironment task_environment;
};

class MockFetchDataLoader : public FetchDataLoader {
 public:
  // Cancel() gets called during garbage collection after the test is
  // finished. Since most tests don't care about this, use NiceMock so that the
  // calls to Cancel() are ignored.
  static testing::NiceMock<MockFetchDataLoader>* Create() {
    return MakeGarbageCollected<testing::NiceMock<MockFetchDataLoader>>();
  }

  MOCK_METHOD2(Start, void(BytesConsumer*, FetchDataLoader::Client*));
  MOCK_METHOD0(Cancel, void());

 protected:
  MockFetchDataLoader() = default;
};

TEST_F(BodyStreamBufferTest, Tee) {
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;
  Checkpoint checkpoint;
  auto* client1 = MakeGarbageCollected<MockFetchDataLoaderClient>();
  auto* client2 = MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(*client1, DidFetchDataLoadedString(String("hello, world")));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*client2, DidFetchDataLoadedString(String("hello, world")));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(checkpoint, Call(4));

  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");

  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kData, "hello, "));
  src->Add(Command(Command::kData, "world"));
  src->Add(Command(Command::kDone));
  BodyStreamBuffer* buffer =
      BodyStreamBuffer::Create(scope.GetScriptState(), src,
                               /*abort_signal=*/nullptr,
                               /*cached_metadata=*/nullptr, side_data_blob);
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());

  BodyStreamBuffer* new1;
  BodyStreamBuffer* new2;
  buffer->Tee(&new1, &new2, exception_state);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());

  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());
  EXPECT_EQ(side_data_blob, new1->GetSideDataBlobForTest());
  EXPECT_EQ(side_data_blob, new2->GetSideDataBlobForTest());

  checkpoint.Call(0);
  new1->StartLoading(FetchDataLoader::CreateLoaderAsString(
                         TextResourceDecoderOptions::CreateUTF8Decode()),
                     client1, exception_state);
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);

  new2->StartLoading(FetchDataLoader::CreateLoaderAsString(
                         TextResourceDecoderOptions::CreateUTF8Decode()),
                     client2, exception_state);
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
}

TEST_F(BodyStreamBufferTest, TeeFromHandleMadeFromStream) {
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(scope.GetScriptState());
  auto* chunk1 = DOMUint8Array::Create(2);
  chunk1->Data()[0] = 0x41;
  chunk1->Data()[1] = 0x42;
  auto* chunk2 = DOMUint8Array::Create(2);
  chunk2->Data()[0] = 0x55;
  chunk2->Data()[1] = 0x58;

  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      scope.GetScriptState(), underlying_source, 0);
  ASSERT_TRUE(stream);

  underlying_source->Enqueue(ScriptValue(
      scope.GetIsolate(),
      ToV8Traits<DOMUint8Array>::ToV8(scope.GetScriptState(), chunk1)));
  underlying_source->Enqueue(ScriptValue(
      scope.GetIsolate(),
      ToV8Traits<DOMUint8Array>::ToV8(scope.GetScriptState(), chunk2)));
  underlying_source->Close();

  Checkpoint checkpoint;
  auto* client1 = MakeGarbageCollected<MockFetchDataLoaderClient>();
  auto* client2 = MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client1, DidFetchDataLoadedString(String("ABUX")));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client2, DidFetchDataLoadedString(String("ABUX")));
  EXPECT_CALL(checkpoint, Call(4));

  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), stream, /*cached_metadta_handler=*/nullptr);

  BodyStreamBuffer* new1;
  BodyStreamBuffer* new2;
  buffer->Tee(&new1, &new2, exception_state);

  EXPECT_TRUE(buffer->IsStreamLocked());
  // Note that this behavior is slightly different from for the behavior of
  // a BodyStreamBuffer made from a BytesConsumer. See the above test. In this
  // test, the stream will get disturbed when the microtask is performed.
  // TODO(yhirano): A uniformed behavior is preferred.
  EXPECT_FALSE(buffer->IsStreamDisturbed());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());

  new1->StartLoading(FetchDataLoader::CreateLoaderAsString(
                         TextResourceDecoderOptions::CreateUTF8Decode()),
                     client1, exception_state);
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);

  new2->StartLoading(FetchDataLoader::CreateLoaderAsString(
                         TextResourceDecoderOptions::CreateUTF8Decode()),
                     client2, exception_state);
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandle) {
  V8TestingScope scope;
  scoped_refptr<BlobDataHandle> blob_data_handle = CreateBlob("hello");
  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(),
      MakeGarbageCollected<BlobBytesConsumer>(scope.GetExecutionContext(),
                                              blob_data_handle),
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());
  scoped_refptr<BlobDataHandle> output_blob_data_handle =
      buffer->DrainAsBlobDataHandle(
          BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize,
          ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());
  EXPECT_EQ(blob_data_handle, output_blob_data_handle);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandleReturnsNull) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src,
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());

  EXPECT_FALSE(buffer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize,
      ASSERT_NO_EXCEPTION));

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());
}

TEST_F(BodyStreamBufferTest,
       DrainAsBlobFromBufferMadeFromBufferMadeFromStream) {
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;
  auto* stream =
      ReadableStream::Create(scope.GetScriptState(), exception_state);
  ASSERT_TRUE(stream);
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), stream, /*cached_metadata_handler=*/nullptr);

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->IsStreamReadable());

  EXPECT_FALSE(buffer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize,
      ASSERT_NO_EXCEPTION));

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->IsStreamReadable());
}

TEST_F(BodyStreamBufferTest, DrainAsFormData) {
  V8TestingScope scope;
  auto* data = MakeGarbageCollected<FormData>(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  scoped_refptr<EncodedFormData> input_form_data =
      data->EncodeMultiPartFormData();
  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");

  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(),
      MakeGarbageCollected<FormDataBytesConsumer>(scope.GetExecutionContext(),
                                                  input_form_data),
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());
  scoped_refptr<EncodedFormData> output_form_data =
      buffer->DrainAsFormData(ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());
  EXPECT_EQ(output_form_data->FlattenToString(),
            input_form_data->FlattenToString());
}

TEST_F(BodyStreamBufferTest, DrainAsFormDataReturnsNull) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src,
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());

  EXPECT_FALSE(buffer->DrainAsFormData(ASSERT_NO_EXCEPTION));

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());
}

TEST_F(BodyStreamBufferTest,
       DrainAsFormDataFromBufferMadeFromBufferMadeFromStream) {
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;
  auto* stream =
      ReadableStream::Create(scope.GetScriptState(), exception_state);
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), stream, /*cached_metadata_handler=*/nullptr);

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->IsStreamReadable());

  EXPECT_FALSE(buffer->DrainAsFormData(ASSERT_NO_EXCEPTION));

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->IsStreamReadable());
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsArrayBuffer) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();
  DOMArrayBuffer* array_buffer = nullptr;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedArrayBufferMock(_))
      .WillOnce(SaveArg<0>(&array_buffer));
  EXPECT_CALL(checkpoint, Call(2));

  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "hello"));
  src->Add(Command(Command::kDone));
  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src,
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsArrayBuffer(), client,
                       ASSERT_NO_EXCEPTION);

  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());
  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());

  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  ASSERT_TRUE(array_buffer);
  EXPECT_EQ("hello", String(static_cast<const char*>(array_buffer->Data()),
                            array_buffer->ByteLength()));
}

class BodyStreamBufferBlobTest : public BodyStreamBufferTest {
 public:
  BodyStreamBufferBlobTest()
      : fake_task_runner_(base::MakeRefCounted<scheduler::FakeTaskRunner>()),
        blob_registry_receiver_(
            &fake_blob_registry_,
            blob_registry_remote_.BindNewPipeAndPassReceiver()) {
    BlobDataHandle::SetBlobRegistryForTesting(blob_registry_remote_.get());
  }

  ~BodyStreamBufferBlobTest() override {
    BlobDataHandle::SetBlobRegistryForTesting(nullptr);
  }

 protected:
  scoped_refptr<scheduler::FakeTaskRunner> fake_task_runner_;

 private:
  FakeBlobRegistry fake_blob_registry_;
  mojo::Remote<mojom::blink::BlobRegistry> blob_registry_remote_;
  mojo::Receiver<mojom::blink::BlobRegistry> blob_registry_receiver_;
};

TEST_F(BodyStreamBufferBlobTest, LoadBodyStreamBufferAsBlob) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();
  scoped_refptr<BlobDataHandle> blob_data_handle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blob_data_handle));
  EXPECT_CALL(checkpoint, Call(2));

  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "hello"));
  src->Add(Command(Command::kDone));
  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src,
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsBlobHandle(
                           "text/plain", fake_task_runner_),
                       client, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());
  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());

  checkpoint.Call(1);
  fake_task_runner_->RunUntilIdle();
  test::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_EQ(5u, blob_data_handle->size());
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsString) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedString(String("hello")));
  EXPECT_CALL(checkpoint, Call(2));

  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "hello"));
  src->Add(Command(Command::kDone));
  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src,
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);
  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(
                           TextResourceDecoderOptions::CreateUTF8Decode()),
                       client, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());
  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());

  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
}

TEST_F(BodyStreamBufferTest, LoadClosedHandle) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedString(String("")));
  EXPECT_CALL(checkpoint, Call(2));

  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), BytesConsumer::CreateClosed(),
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);

  EXPECT_TRUE(buffer->IsStreamClosed());

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());

  checkpoint.Call(1);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(
                           TextResourceDecoderOptions::CreateUTF8Decode()),
                       client, ASSERT_NO_EXCEPTION);
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
}

TEST_F(BodyStreamBufferTest, LoadErroredHandle) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(2));

  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(),
      BytesConsumer::CreateErrored(BytesConsumer::Error()),
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);

  EXPECT_TRUE(buffer->IsStreamErrored());

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());

  checkpoint.Call(1);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(
                           TextResourceDecoderOptions::CreateUTF8Decode()),
                       client, ASSERT_NO_EXCEPTION);
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
}

TEST_F(BodyStreamBufferTest, LoaderShouldBeKeptAliveByBodyStreamBuffer) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedString(String("hello")));
  EXPECT_CALL(checkpoint, Call(2));

  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "hello"));
  src->Add(Command(Command::kDone));
  Persistent<BodyStreamBuffer> buffer =
      BodyStreamBuffer::Create(scope.GetScriptState(), src, nullptr,
                               /*cached_metadata_handler=*/nullptr);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(
                           TextResourceDecoderOptions::CreateUTF8Decode()),
                       client, ASSERT_NO_EXCEPTION);

  ThreadState::Current()->CollectAllGarbageForTesting();
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
}

TEST_F(BodyStreamBufferTest, SourceShouldBeCanceledWhenCanceled) {
  V8TestingScope scope;
  ReplayingBytesConsumer* consumer =
      MakeGarbageCollected<ReplayingBytesConsumer>(
          scope.GetDocument().GetTaskRunner(TaskType::kNetworking));

  BodyStreamBuffer* buffer =
      BodyStreamBuffer::Create(scope.GetScriptState(), consumer, nullptr,
                               /*cached_metadata_handler=*/nullptr);
  ScriptValue reason(scope.GetIsolate(),
                     V8String(scope.GetIsolate(), "reason"));
  EXPECT_FALSE(consumer->IsCancelled());
  buffer->Cancel(reason.V8Value());
  EXPECT_TRUE(consumer->IsCancelled());
}

TEST_F(BodyStreamBufferTest, NestedPull) {
  V8TestingScope scope;
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "hello"));
  src->Add(Command(Command::kError));
  Persistent<BodyStreamBuffer> buffer =
      BodyStreamBuffer::Create(scope.GetScriptState(), src, nullptr,
                               /*cached_metadata_handler=*/nullptr);

  auto result =
      scope.GetScriptState()->GetContext()->Global()->CreateDataProperty(
          scope.GetScriptState()->GetContext(),
          V8String(scope.GetIsolate(), "stream"),
          ToV8Traits<ReadableStream>::ToV8(scope.GetScriptState(),
                                           buffer->Stream()));

  ASSERT_TRUE(result.IsJust());
  ASSERT_TRUE(result.FromJust());

  ScriptValue stream = EvalWithPrintingError(scope.GetScriptState(),
                                             "reader = stream.getReader();");
  ASSERT_FALSE(stream.IsEmpty());

  EvalWithPrintingError(scope.GetScriptState(), "reader.read();");
  EvalWithPrintingError(scope.GetScriptState(), "reader.read();");

  test::RunPendingTasks();
  scope.PerformMicrotaskCheckpoint();
}

TEST_F(BodyStreamBufferTest, NullAbortSignalIsNotAborted) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  BodyStreamBuffer* buffer =
      BodyStreamBuffer::Create(scope.GetScriptState(), src, nullptr,
                               /*cached_metadata_handler=*/nullptr);

  EXPECT_FALSE(buffer->IsAborted());
}

TEST_F(BodyStreamBufferTest, AbortSignalMakesAborted) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  auto* controller = AbortController::Create(scope.GetScriptState());
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src, controller->signal(),
      /*cached_metadata_handler=*/nullptr);

  EXPECT_FALSE(buffer->IsAborted());
  controller->abort(scope.GetScriptState());
  EXPECT_TRUE(buffer->IsAborted());
}

TEST_F(BodyStreamBufferTest,
       AbortBeforeStartLoadingCallsDataLoaderClientAbort) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoader* loader = MockFetchDataLoader::Create();
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();
  auto* src = MakeGarbageCollected<BytesConsumerTestUtil::MockBytesConsumer>();

  EXPECT_CALL(*loader, Start(_, _)).Times(0);

  InSequence s;
  EXPECT_CALL(*src, SetClient(_));
  EXPECT_CALL(*src, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*src, Cancel());

  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*client, Abort());

  EXPECT_CALL(checkpoint, Call(3));

  auto* controller = AbortController::Create(scope.GetScriptState());
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src, controller->signal(),
      /*cached_metadata_handler=*/nullptr);

  checkpoint.Call(1);
  controller->abort(scope.GetScriptState());

  checkpoint.Call(2);
  buffer->StartLoading(loader, client, ASSERT_NO_EXCEPTION);

  checkpoint.Call(3);
}

TEST_F(BodyStreamBufferTest, AbortAfterStartLoadingCallsDataLoaderClientAbort) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoader* loader = MockFetchDataLoader::Create();
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();
  auto* src = MakeGarbageCollected<BytesConsumerTestUtil::MockBytesConsumer>();

  InSequence s;
  EXPECT_CALL(*src, SetClient(_));
  EXPECT_CALL(*src, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*src, ClearClient());
  EXPECT_CALL(*loader, Start(_, _));

  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*client, Abort());

  EXPECT_CALL(checkpoint, Call(3));

  auto* controller = AbortController::Create(scope.GetScriptState());
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src, controller->signal(),
      /*cached_metadata_handler=*/nullptr);

  checkpoint.Call(1);
  buffer->StartLoading(loader, client, ASSERT_NO_EXCEPTION);

  checkpoint.Call(2);
  controller->abort(scope.GetScriptState());

  checkpoint.Call(3);
}

TEST_F(BodyStreamBufferTest,
       AsyncAbortAfterStartLoadingCallsDataLoaderClientAbort) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoader* loader = MockFetchDataLoader::Create();
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();
  auto* src = MakeGarbageCollected<BytesConsumerTestUtil::MockBytesConsumer>();

  InSequence s;
  EXPECT_CALL(*src, SetClient(_));
  EXPECT_CALL(*src, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*src, ClearClient());
  EXPECT_CALL(*loader, Start(_, _));

  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*client, Abort());

  EXPECT_CALL(checkpoint, Call(3));

  auto* controller = AbortController::Create(scope.GetScriptState());
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(), src, controller->signal(),
      /*cached_metadata_handler=*/nullptr);

  checkpoint.Call(1);
  buffer->StartLoading(loader, client, ASSERT_NO_EXCEPTION);
  test::RunPendingTasks();

  checkpoint.Call(2);
  controller->abort(scope.GetScriptState());

  checkpoint.Call(3);
}

TEST_F(BodyStreamBufferTest, CachedMetadataHandler) {
  V8TestingScope scope;
  Persistent<BodyStreamBuffer> buffer;
  WeakPersistent<ScriptCachedMetadataHandler> weak_handler;
  {
    BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
        scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
    auto* handler = MakeGarbageCollected<ScriptCachedMetadataHandler>(
        WTF::TextEncoding(), nullptr);
    weak_handler = handler;
    buffer = BodyStreamBuffer::Create(scope.GetScriptState(), src,
                                      /*abort_signal=*/nullptr, handler);

    EXPECT_EQ(handler, buffer->GetCachedMetadataHandler());
    EXPECT_NE(weak_handler.Get(), nullptr);

    buffer->CloseAndLockAndDisturb(ASSERT_NO_EXCEPTION);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_EQ(weak_handler.Get(), nullptr);
}

TEST_F(BodyStreamBufferTest, CachedMetadataHandlerAndTee) {
  V8TestingScope scope;
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  auto* handler = MakeGarbageCollected<ScriptCachedMetadataHandler>(
      WTF::TextEncoding(), nullptr);
  auto* buffer = BodyStreamBuffer::Create(scope.GetScriptState(), src,
                                          /*abort_signal=*/nullptr, handler);

  EXPECT_EQ(handler, buffer->GetCachedMetadataHandler());

  BodyStreamBuffer* dest1 = nullptr;
  BodyStreamBuffer* dest2 = nullptr;
  buffer->Tee(&dest1, &dest2, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(dest1->GetCachedMetadataHandler(), handler);
  EXPECT_EQ(dest2->GetCachedMetadataHandler(), handler);
}

TEST_F(BodyStreamBufferTest,
       CachedMetadataHandlerAndTeeForBufferMadeFromStream) {
  V8TestingScope scope;
  auto* handler = MakeGarbageCollected<ScriptCachedMetadataHandler>(
      WTF::TextEncoding(), nullptr);
  auto* stream =
      ReadableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  auto* buffer = MakeGarbageCollected<BodyStreamBuffer>(scope.GetScriptState(),
                                                        stream, handler);

  EXPECT_EQ(handler, buffer->GetCachedMetadataHandler());

  BodyStreamBuffer* dest1 = nullptr;
  BodyStreamBuffer* dest2 = nullptr;
  buffer->Tee(&dest1, &dest2, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(dest1->GetCachedMetadataHandler(), handler);
  EXPECT_EQ(dest2->GetCachedMetadataHandler(), handler);
}

TEST_F(BodyStreamBufferTest, TakeSideDataBlob) {
  V8TestingScope scope;
  scoped_refptr<BlobDataHandle> blob_data_handle = CreateBlob("hello");
  scoped_refptr<BlobDataHandle> side_data_blob = CreateBlob("side data");
  BodyStreamBuffer* buffer = BodyStreamBuffer::Create(
      scope.GetScriptState(),
      MakeGarbageCollected<BlobBytesConsumer>(scope.GetExecutionContext(),
                                              blob_data_handle),
      /*abort_signal=*/nullptr, /*cached_metadata_handler=*/nullptr,
      side_data_blob);

  EXPECT_EQ(side_data_blob, buffer->GetSideDataBlobForTest());
  EXPECT_EQ(side_data_blob, buffer->TakeSideDataBlob());
  EXPECT_EQ(nullptr, buffer->GetSideDataBlobForTest());
  EXPECT_EQ(nullptr, buffer->TakeSideDataBlob());
}

TEST_F(BodyStreamBufferTest, KeptAliveWhileLoading) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();

  WeakPersistent<BodyStreamBuffer> buffer;
  WeakPersistent<ReplayingBytesConsumer> src;
  {
    v8::HandleScope handle_scope(isolate);
    auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();

    src = MakeGarbageCollected<ReplayingBytesConsumer>(
        scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
    src->Add(Command(Command::kWait));
    src->Add(Command(Command::kData, "hello"));

    buffer = BodyStreamBuffer::Create(scope.GetScriptState(), src,
                                      /*signal=*/nullptr,
                                      /*cached_metadata_handler=*/nullptr);
    buffer->StartLoading(FetchDataLoader::CreateLoaderAsArrayBuffer(), client,
                         ASSERT_NO_EXCEPTION);
  }
  test::RunPendingTasks();

  // The BodyStreamBuffer is kept alive while loading due to a SelfKeepAlive.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_NE(nullptr, buffer);

  // Allow it to finish which clears the SelfKeepAlive and makes it collectable.
  src->Add(Command(Command::kDone));
  src->TriggerOnStateChange();
  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(nullptr, buffer);
}

}  // namespace

}  // namespace blink
