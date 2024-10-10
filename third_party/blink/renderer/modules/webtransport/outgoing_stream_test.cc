// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"

#include <utility>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::testing::StrictMock;

class MockClient : public GarbageCollected<MockClient>,
                   public OutgoingStream::Client {
 public:
  MOCK_METHOD0(SendFin, void());
  MOCK_METHOD0(ForgetStream, void());
  MOCK_METHOD1(Reset, void(uint8_t));
};

// The purpose of this class is to ensure that the data pipe is reset before the
// V8TestingScope is destroyed, so that the OutgoingStream object doesn't try to
// create a DOMException after the ScriptState has gone away.
class StreamCreator {
  STACK_ALLOCATED();

 public:
  StreamCreator() = default;
  ~StreamCreator() {
    Reset();

    // Let the OutgoingStream object respond to the closure if it needs to.
    test::RunPendingTasks();
  }

  // The default value of |capacity| means some sensible value selected by mojo.
  OutgoingStream* Create(const V8TestingScope& scope, uint32_t capacity = 0) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = capacity;

    mojo::ScopedDataPipeProducerHandle data_pipe_producer;
    MojoResult result =
        mojo::CreateDataPipe(&options, data_pipe_producer, data_pipe_consumer_);
    if (result != MOJO_RESULT_OK) {
      ADD_FAILURE() << "CreateDataPipe() returned " << result;
    }

    auto* script_state = scope.GetScriptState();
    mock_client_ = MakeGarbageCollected<StrictMock<MockClient>>();
    auto* outgoing_stream = MakeGarbageCollected<OutgoingStream>(
        script_state, mock_client_, std::move(data_pipe_producer));
    ExceptionState exception_state(scope.GetIsolate(),
                                   v8::ExceptionContext::kConstructor,
                                   "OutgoingStream");
    outgoing_stream->Init(exception_state);
    CHECK(!exception_state.HadException());
    return outgoing_stream;
  }

  // Closes the pipe.
  void Reset() { data_pipe_consumer_.reset(); }

  // This is for use in EXPECT_CALL(), which is why it returns a reference.
  MockClient& GetMockClient() { return *mock_client_; }

  // Reads everything from |data_pipe_consumer_| and returns it in a vector.
  Vector<uint8_t> ReadAllPendingData() {
    Vector<uint8_t> data;
    base::span<const uint8_t> buffer;
    MojoResult result = data_pipe_consumer_->BeginReadData(
        MOJO_BEGIN_READ_DATA_FLAG_NONE, buffer);

    switch (result) {
      case MOJO_RESULT_OK:
        break;

      case MOJO_RESULT_SHOULD_WAIT:  // No more data yet.
        return data;

      default:
        ADD_FAILURE() << "BeginReadData() failed: " << result;
        return data;
    }

    data.AppendRange(buffer.begin(), buffer.end());
    data_pipe_consumer_->EndReadData(buffer.size());
    return data;
  }

  Persistent<StrictMock<MockClient>> mock_client_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer_;
};

TEST(OutgoingStreamTest, Create) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;
  auto* outgoing_stream = stream_creator.Create(scope);
  EXPECT_TRUE(outgoing_stream->Writable());

  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());
}

TEST(OutgoingStreamTest, WriteArrayBuffer) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;
  auto* outgoing_stream = stream_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMArrayBuffer::Create(base::byte_span_from_cstring("A"));
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(stream_creator.ReadAllPendingData(), ElementsAre('A'));

  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());
}

TEST(OutgoingStreamTest, WriteArrayBufferView) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;
  auto* outgoing_stream = stream_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* buffer = DOMArrayBuffer::Create(base::byte_span_from_cstring("*B"));
  // Create a view into the buffer with offset 1, ie. "B".
  auto* chunk = DOMUint8Array::Create(buffer, 1, 1);
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(stream_creator.ReadAllPendingData(), ElementsAre('B'));

  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());
}

bool IsAllNulls(base::span<const uint8_t> data) {
  return base::ranges::all_of(data, [](uint8_t c) { return !c; });
}

TEST(OutgoingStreamTest, AsyncWrite) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;
  // Set a large pipe capacity, so any platform-specific excess is dwarfed in
  // size.
  constexpr uint32_t kPipeCapacity = 512u * 1024u;
  auto* outgoing_stream = stream_creator.Create(scope, kPipeCapacity);

  auto* script_state = scope.GetScriptState();
  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);

  // Write a chunk that definitely will not fit in the pipe.
  const size_t kChunkSize = kPipeCapacity * 3;
  auto* chunk = DOMArrayBuffer::Create(kChunkSize, 1);
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), result);

  // Let the first pipe write complete.
  test::RunPendingTasks();

  // Let microtasks run just in case write() returns prematurely.
  scope.PerformMicrotaskCheckpoint();
  EXPECT_FALSE(tester.IsFulfilled());

  // Read the first part of the data.
  auto data1 = stream_creator.ReadAllPendingData();
  EXPECT_LT(data1.size(), kChunkSize);

  // Verify the data wasn't corrupted.
  EXPECT_TRUE(IsAllNulls(data1));

  // Allow the asynchronous pipe write to happen.
  test::RunPendingTasks();

  // Read the second part of the data.
  auto data2 = stream_creator.ReadAllPendingData();
  EXPECT_TRUE(IsAllNulls(data2));

  test::RunPendingTasks();

  // Read the final part of the data.
  auto data3 = stream_creator.ReadAllPendingData();
  EXPECT_TRUE(IsAllNulls(data3));
  EXPECT_EQ(data1.size() + data2.size() + data3.size(), kChunkSize);

  // Now the write() should settle.
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  // Nothing should be left to read.
  EXPECT_THAT(stream_creator.ReadAllPendingData(), ElementsAre());

  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());
}

// Writing immediately followed by closing should not lose data.
TEST(OutgoingStreamTest, WriteThenClose) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;

  auto* outgoing_stream = stream_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMArrayBuffer::Create(base::byte_span_from_cstring("D"));
  ScriptPromiseUntyped write_promise =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);

  EXPECT_CALL(stream_creator.GetMockClient(), SendFin()).WillOnce([&]() {
    // This needs to happen asynchronously.
    scope.GetExecutionContext()
        ->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&OutgoingStream::OnOutgoingStreamClosed,
                                 WrapWeakPersistent(outgoing_stream)));
  });

  ScriptPromiseUntyped close_promise =
      writer->close(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(scope.GetScriptState(), write_promise);
  ScriptPromiseTester close_tester(scope.GetScriptState(), close_promise);

  // Make sure that write() and close() both run before the event loop is
  // serviced.
  scope.PerformMicrotaskCheckpoint();

  write_tester.WaitUntilSettled();
  EXPECT_TRUE(write_tester.IsFulfilled());
  close_tester.WaitUntilSettled();
  EXPECT_TRUE(close_tester.IsFulfilled());

  EXPECT_THAT(stream_creator.ReadAllPendingData(), ElementsAre('D'));
}

TEST(OutgoingStreamTest, DataPipeClosed) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;

  auto* outgoing_stream = stream_creator.Create(scope);
  auto* script_state = scope.GetScriptState();

  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped closed = writer->closed(script_state);
  ScriptPromiseTester closed_tester(script_state, closed);

  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());

  // Close the other end of the pipe.
  stream_creator.Reset();

  closed_tester.WaitUntilSettled();
  EXPECT_TRUE(closed_tester.IsRejected());

  DOMException* closed_exception = V8DOMException::ToWrappable(
      scope.GetIsolate(), closed_tester.Value().V8Value());
  ASSERT_TRUE(closed_exception);
  EXPECT_EQ(closed_exception->name(), "NetworkError");
  EXPECT_EQ(closed_exception->message(),
            "The stream was aborted by the remote server");

  auto* chunk = DOMArrayBuffer::Create('C', 1);
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(script_state, result);
  write_tester.WaitUntilSettled();

  EXPECT_TRUE(write_tester.IsRejected());

  DOMException* write_exception = V8DOMException::ToWrappable(
      scope.GetIsolate(), write_tester.Value().V8Value());
  ASSERT_TRUE(write_exception);
  EXPECT_EQ(write_exception->name(), "NetworkError");
  EXPECT_EQ(write_exception->message(),
            "The stream was aborted by the remote server");
}

TEST(OutgoingStreamTest, DataPipeClosedDuringAsyncWrite) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;

  constexpr uint32_t kPipeCapacity = 512 * 1024;
  auto* outgoing_stream = stream_creator.Create(scope, kPipeCapacity);

  auto* script_state = scope.GetScriptState();

  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);

  const size_t kChunkSize = kPipeCapacity * 2;
  auto* chunk = DOMArrayBuffer::Create(kChunkSize, 1);
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(script_state, result);

  ScriptPromiseUntyped closed = writer->closed(script_state);
  ScriptPromiseTester closed_tester(script_state, closed);

  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());

  // Close the other end of the pipe.
  stream_creator.Reset();

  write_tester.WaitUntilSettled();

  EXPECT_TRUE(write_tester.IsRejected());

  DOMException* write_exception = V8DOMException::ToWrappable(
      scope.GetIsolate(), write_tester.Value().V8Value());
  ASSERT_TRUE(write_exception);
  EXPECT_EQ(write_exception->name(), "NetworkError");
  EXPECT_EQ(write_exception->message(),
            "The stream was aborted by the remote server");

  closed_tester.WaitUntilSettled();

  EXPECT_TRUE(closed_tester.IsRejected());

  DOMException* closed_exception = V8DOMException::ToWrappable(
      scope.GetIsolate(), write_tester.Value().V8Value());
  ASSERT_TRUE(closed_exception);
  EXPECT_EQ(closed_exception->name(), "NetworkError");
  EXPECT_EQ(closed_exception->message(),
            "The stream was aborted by the remote server");
}

TEST(OutgoingStreamTest, Abort) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  auto* outgoing_stream = stream_creator.Create(scope);

  testing::InSequence s;
  EXPECT_CALL(stream_creator.GetMockClient(), Reset(0u));
  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());

  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);
  writer->abort(script_state, ScriptValue(isolate, v8::Undefined(isolate)),
                ASSERT_NO_EXCEPTION);
}

TEST(OutgoingStreamTest, AbortWithWebTransportError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  auto* outgoing_stream = stream_creator.Create(scope);

  testing::InSequence s;
  EXPECT_CALL(stream_creator.GetMockClient(), Reset(0));
  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());

  v8::Local<v8::Value> error =
      WebTransportError::Create(isolate,
                                /*stream_error_code=*/std::nullopt, "foobar",
                                V8WebTransportErrorSource::Enum::kStream);

  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);
  writer->abort(script_state, ScriptValue(isolate, error), ASSERT_NO_EXCEPTION);
}

TEST(OutgoingStreamTest, AbortWithWebTransportErrorWithCode) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  auto* outgoing_stream = stream_creator.Create(scope);

  testing::InSequence s;
  EXPECT_CALL(stream_creator.GetMockClient(), Reset(8));
  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());

  v8::Local<v8::Value> error =
      WebTransportError::Create(isolate,
                                /*stream_error_code=*/8, "foobar",
                                V8WebTransportErrorSource::Enum::kStream);

  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);
  writer->abort(script_state, ScriptValue(isolate, error), ASSERT_NO_EXCEPTION);
}

TEST(OutgoingStreamTest, CloseAndConnectionError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  StreamCreator stream_creator;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  auto* outgoing_stream = stream_creator.Create(scope);

  testing::InSequence s;
  EXPECT_CALL(stream_creator.GetMockClient(), SendFin());
  EXPECT_CALL(stream_creator.GetMockClient(), ForgetStream());

  auto* writer =
      outgoing_stream->Writable()->getWriter(script_state, ASSERT_NO_EXCEPTION);

  // Run microtasks to ensure that the underlying sink's close function is
  // called immediately.
  scope.PerformMicrotaskCheckpoint();

  writer->close(script_state, ASSERT_NO_EXCEPTION);
  outgoing_stream->Error(ScriptValue(isolate, v8::Undefined(isolate)));
}

}  // namespace

}  // namespace blink
