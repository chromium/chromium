// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"

#include <memory>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
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
    return ScriptValue(script_state->GetIsolate(),
                       script->Run(script_state->GetContext()));
  }
  ScriptValue EvalWithPrintingError(ScriptState* script_state, const char* s) {
    v8::TryCatch block(script_state->GetIsolate());
    ScriptValue r = Eval(script_state, s);
    if (block.HasCaught()) {
      ADD_FAILURE() << ToCoreString(block.Exception()
                                        ->ToString(script_state->GetContext())
                                        .ToLocalChecked())
                           .Utf8();
      block.ReThrow();
    }
    return r;
  }
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

  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kData, "hello, "));
  src->Add(Command(Command::kData, "world"));
  src->Add(Command(Command::kDone));
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);

  BodyStreamBuffer* new1;
  BodyStreamBuffer* new2;
  buffer->Tee(&new1, &new2, exception_state);

  EXPECT_TRUE(buffer->IsStreamLocked(exception_state).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(exception_state).value_or(false));
  EXPECT_FALSE(buffer->HasPendingActivity());

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

  underlying_source->Enqueue(
      ScriptValue(scope.GetIsolate(), ToV8(chunk1, scope.GetScriptState())));
  underlying_source->Enqueue(
      ScriptValue(scope.GetIsolate(), ToV8(chunk2, scope.GetScriptState())));
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

  BodyStreamBuffer* buffer =
      MakeGarbageCollected<BodyStreamBuffer>(scope.GetScriptState(), stream);

  BodyStreamBuffer* new1;
  BodyStreamBuffer* new2;
  buffer->Tee(&new1, &new2, exception_state);

  EXPECT_TRUE(buffer->IsStreamLocked(exception_state).value_or(false));
  // Note that this behavior is slightly different from for the behavior of
  // a BodyStreamBuffer made from a BytesConsumer. See the above test. In this
  // test, the stream will get disturbed when the microtask is performed.
  // TODO(yhirano): A uniformed behavior is preferred.
  EXPECT_FALSE(buffer->IsStreamDisturbed(exception_state).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());

  EXPECT_TRUE(buffer->IsStreamLocked(exception_state).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(exception_state).value_or(false));
  EXPECT_FALSE(buffer->HasPendingActivity());

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
  auto data = std::make_unique<BlobData>();
  data->AppendText("hello", false);
  auto size = data->length();
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::move(data), size);
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(),
      MakeGarbageCollected<BlobBytesConsumer>(scope.GetExecutionContext(),
                                              blob_data_handle),
      nullptr);

  EXPECT_FALSE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());
  scoped_refptr<BlobDataHandle> output_blob_data_handle =
      buffer->DrainAsBlobDataHandle(
          BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize,
          ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_EQ(blob_data_handle, output_blob_data_handle);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandleReturnsNull) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);

  EXPECT_FALSE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());

  EXPECT_FALSE(buffer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize,
      ASSERT_NO_EXCEPTION));

  EXPECT_FALSE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest,
       DrainAsBlobFromBufferMadeFromBufferMadeFromStream) {
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;
  auto* stream =
      ReadableStream::Create(scope.GetScriptState(), exception_state);
  ASSERT_TRUE(stream);
  BodyStreamBuffer* buffer =
      MakeGarbageCollected<BodyStreamBuffer>(scope.GetScriptState(), stream);

  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_FALSE(buffer->IsStreamLocked(exception_state).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(exception_state).value_or(true));
  EXPECT_TRUE(buffer->IsStreamReadable(exception_state).value_or(false));

  EXPECT_FALSE(buffer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize,
      exception_state));

  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_FALSE(buffer->IsStreamLocked(exception_state).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(exception_state).value_or(true));
  EXPECT_TRUE(buffer->IsStreamReadable(exception_state).value_or(false));
}

TEST_F(BodyStreamBufferTest, DrainAsFormData) {
  V8TestingScope scope;
  auto* data = MakeGarbageCollected<FormData>(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  scoped_refptr<EncodedFormData> input_form_data =
      data->EncodeMultiPartFormData();

  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(),
      MakeGarbageCollected<FormDataBytesConsumer>(scope.GetExecutionContext(),
                                                  input_form_data),
      nullptr);

  EXPECT_FALSE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());
  scoped_refptr<EncodedFormData> output_form_data =
      buffer->DrainAsFormData(ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_EQ(output_form_data->FlattenToString(),
            input_form_data->FlattenToString());
}

TEST_F(BodyStreamBufferTest, DrainAsFormDataReturnsNull) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);

  EXPECT_FALSE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());

  EXPECT_FALSE(buffer->DrainAsFormData(ASSERT_NO_EXCEPTION));

  EXPECT_FALSE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest,
       DrainAsFormDataFromBufferMadeFromBufferMadeFromStream) {
  V8TestingScope scope;
  NonThrowableExceptionState exception_state;
  auto* stream =
      ReadableStream::Create(scope.GetScriptState(), exception_state);
  BodyStreamBuffer* buffer =
      MakeGarbageCollected<BodyStreamBuffer>(scope.GetScriptState(), stream);

  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_FALSE(buffer->IsStreamLocked(exception_state).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(exception_state).value_or(true));
  EXPECT_TRUE(buffer->IsStreamReadable(exception_state).value_or(false));

  EXPECT_FALSE(buffer->DrainAsFormData(exception_state));

  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_FALSE(buffer->IsStreamLocked(exception_state).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(exception_state).value_or(true));
  EXPECT_TRUE(buffer->IsStreamReadable(exception_state).value_or(false));
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
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsArrayBuffer(), client,
                       ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());
  ASSERT_TRUE(array_buffer);
  EXPECT_EQ("hello", String(static_cast<const char*>(array_buffer->Data()),
                            array_buffer->ByteLengthAsSizeT()));
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsBlob) {
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
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsBlobHandle("text/plain"),
                       client, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_FALSE(buffer->HasPendingActivity());
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
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(
                           TextResourceDecoderOptions::CreateUTF8Decode()),
                       client, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoadClosedHandle) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedString(String("")));
  EXPECT_CALL(checkpoint, Call(2));

  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), BytesConsumer::CreateClosed(), nullptr);

  EXPECT_TRUE(buffer->IsStreamClosed(ASSERT_NO_EXCEPTION).value_or(false));

  EXPECT_FALSE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(
                           TextResourceDecoderOptions::CreateUTF8Decode()),
                       client, ASSERT_NO_EXCEPTION);
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoadErroredHandle) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  auto* client = MakeGarbageCollected<MockFetchDataLoaderClient>();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(2));

  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(),
      BytesConsumer::CreateErrored(BytesConsumer::Error()), nullptr);

  EXPECT_TRUE(buffer->IsStreamErrored(ASSERT_NO_EXCEPTION).value_or(false));

  EXPECT_FALSE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(true));
  EXPECT_FALSE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(
                           TextResourceDecoderOptions::CreateUTF8Decode()),
                       client, ASSERT_NO_EXCEPTION);
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_TRUE(buffer->IsStreamDisturbed(ASSERT_NO_EXCEPTION).value_or(false));
  EXPECT_FALSE(buffer->HasPendingActivity());
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
  Persistent<BodyStreamBuffer> buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);
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

  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), consumer, nullptr);
  ScriptValue reason(scope.GetIsolate(),
                     V8String(scope.GetIsolate(), "reason"));
  EXPECT_FALSE(consumer->IsCancelled());
  buffer->Cancel(scope.GetScriptState(), reason);
  EXPECT_TRUE(consumer->IsCancelled());
}

TEST_F(BodyStreamBufferTest, NestedPull) {
  V8TestingScope scope;
  ReplayingBytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kWait));
  src->Add(Command(Command::kData, "hello"));
  src->Add(Command(Command::kError));
  Persistent<BodyStreamBuffer> buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);

  auto result =
      scope.GetScriptState()->GetContext()->Global()->CreateDataProperty(
          scope.GetScriptState()->GetContext(),
          V8String(scope.GetIsolate(), "stream"),
          ToV8(buffer->Stream(), scope.GetScriptState()));

  ASSERT_TRUE(result.IsJust());
  ASSERT_TRUE(result.FromJust());

  ScriptValue stream = EvalWithPrintingError(scope.GetScriptState(),
                                             "reader = stream.getReader();");
  ASSERT_FALSE(stream.IsEmpty());

  EvalWithPrintingError(scope.GetScriptState(), "reader.read();");
  EvalWithPrintingError(scope.GetScriptState(), "reader.read();");

  test::RunPendingTasks();
  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
}

TEST_F(BodyStreamBufferTest, NullAbortSignalIsNotAborted) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, nullptr);

  EXPECT_FALSE(buffer->IsAborted());
}

TEST_F(BodyStreamBufferTest, AbortSignalMakesAborted) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      scope.GetDocument().GetTaskRunner(TaskType::kNetworking));
  auto* signal = MakeGarbageCollected<AbortSignal>(scope.GetExecutionContext());
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, signal);

  EXPECT_FALSE(buffer->IsAborted());
  signal->SignalAbort();
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

  auto* signal = MakeGarbageCollected<AbortSignal>(scope.GetExecutionContext());
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, signal);

  checkpoint.Call(1);
  signal->SignalAbort();

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

  auto* signal = MakeGarbageCollected<AbortSignal>(scope.GetExecutionContext());
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, signal);

  checkpoint.Call(1);
  buffer->StartLoading(loader, client, ASSERT_NO_EXCEPTION);

  checkpoint.Call(2);
  signal->SignalAbort();

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

  auto* signal = MakeGarbageCollected<AbortSignal>(scope.GetExecutionContext());
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(), src, signal);

  checkpoint.Call(1);
  buffer->StartLoading(loader, client, ASSERT_NO_EXCEPTION);
  test::RunPendingTasks();

  checkpoint.Call(2);
  signal->SignalAbort();

  checkpoint.Call(3);
}

}  // namespace

}  // namespace blink
