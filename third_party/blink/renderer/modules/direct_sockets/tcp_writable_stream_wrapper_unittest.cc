// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_writable_stream_wrapper.h"

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/test/mock_callback.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::testing::StrictMock;

// The purpose of this class is to ensure that the data pipe is reset before the
// V8TestingScope is destroyed, so that the TCPWritableStreamWrapper object
// doesn't try to create a DOMException after the ScriptState has gone away.
class StreamCreator : public GarbageCollected<StreamCreator> {
 public:
  StreamCreator() = default;
  ~StreamCreator() = default;

  // The default value of |capacity| means some sensible value selected by mojo.
  TCPWritableStreamWrapper* Create(const V8TestingScope& scope,
                                   uint32_t capacity = 1) {
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
    stream_wrapper_ = MakeGarbageCollected<TCPWritableStreamWrapper>(
        script_state, base::DoNothing(), std::move(data_pipe_producer));
    return stream_wrapper_.Get();
  }

  void ResetPipe() { data_pipe_consumer_.reset(); }

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

  void Close(bool error) {}

  void Trace(Visitor* visitor) const { visitor->Trace(stream_wrapper_); }

 private:
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer_;
  Member<TCPWritableStreamWrapper> stream_wrapper_;
};

class ScopedStreamCreator {
 public:
  explicit ScopedStreamCreator(StreamCreator* stream_creator)
      : stream_creator_(stream_creator) {}

  ~ScopedStreamCreator() { stream_creator_->ResetPipe(); }

  StreamCreator* operator->() const { return stream_creator_; }

 private:
  Persistent<StreamCreator> stream_creator_;
};

TEST(TCPWritableStreamWrapperTest, Create) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_writable_stream_wrapper = stream_creator->Create(scope);
  EXPECT_TRUE(tcp_writable_stream_wrapper->Writable());
}

TEST(TCPWritableStreamWrapperTest, WriteArrayBuffer) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMArrayBuffer::Create(base::byte_span_from_cstring("A"));
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(stream_creator->ReadAllPendingData(), ElementsAre('A'));
}

TEST(TCPWritableStreamWrapperTest, WriteArrayBufferView) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* buffer = DOMArrayBuffer::Create(base::byte_span_from_cstring("*B"));
  // Create a view into the buffer with offset 1, ie. "B".
  auto* chunk = DOMUint8Array::Create(buffer, 1, 1);
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(stream_creator->ReadAllPendingData(), ElementsAre('B'));
}

bool IsAllNulls(base::span<const uint8_t> data) {
  return base::ranges::all_of(data, [](uint8_t c) { return !c; });
}

TEST(TCPWritableStreamWrapperTest, AsyncWrite) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  // Set a large pipe capacity, so any platform-specific excess is dwarfed in
  // size.
  constexpr uint32_t kPipeCapacity = 512u * 1024u;
  auto* tcp_writable_stream_wrapper =
      stream_creator->Create(scope, kPipeCapacity);

  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  // Write a chunk that definitely will not fit in the pipe.
  const size_t kChunkSize = kPipeCapacity * 3;
  auto* chunk = DOMArrayBuffer::Create(kChunkSize, 1);
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);

  // Let the first pipe write complete.
  test::RunPendingTasks();

  // Let microtasks run just in case write() returns prematurely.
  scope.PerformMicrotaskCheckpoint();
  ASSERT_FALSE(tester.IsFulfilled());

  // Read the first part of the data.
  auto data1 = stream_creator->ReadAllPendingData();
  EXPECT_LT(data1.size(), kChunkSize);

  // Verify the data wasn't corrupted.
  EXPECT_TRUE(IsAllNulls(data1));

  // Allow the asynchronous pipe write to happen.
  test::RunPendingTasks();

  // Read the second part of the data.
  auto data2 = stream_creator->ReadAllPendingData();
  EXPECT_TRUE(IsAllNulls(data2));

  test::RunPendingTasks();

  // Read the final part of the data.
  auto data3 = stream_creator->ReadAllPendingData();
  EXPECT_TRUE(IsAllNulls(data3));
  EXPECT_EQ(data1.size() + data2.size() + data3.size(), kChunkSize);

  // Now the write() should settle.
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  // Nothing should be left to read.
  EXPECT_THAT(stream_creator->ReadAllPendingData(), ElementsAre());
}

// Writing immediately followed by closing should not lose data.
TEST(TCPWritableStreamWrapperTest, WriteThenClose) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMArrayBuffer::Create(base::byte_span_from_cstring("D"));
  ScriptPromiseUntyped write_promise =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);

  ScriptPromiseUntyped close_promise =
      writer->close(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(script_state, write_promise);
  ScriptPromiseTester close_tester(script_state, close_promise);

  // Make sure that write() and close() both run before the event loop is
  // serviced.
  scope.PerformMicrotaskCheckpoint();

  write_tester.WaitUntilSettled();
  ASSERT_TRUE(write_tester.IsFulfilled());
  close_tester.WaitUntilSettled();
  ASSERT_TRUE(close_tester.IsFulfilled());

  EXPECT_THAT(stream_creator->ReadAllPendingData(), ElementsAre('D'));
}

TEST(TCPWritableStreamWrapperTest, DISABLED_TriggerHasAborted) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMArrayBuffer::Create(base::byte_span_from_cstring("D"));
  ScriptPromiseUntyped write_promise =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(script_state, write_promise);

  tcp_writable_stream_wrapper->ErrorStream(net::ERR_UNEXPECTED);
  write_tester.WaitUntilSettled();

  ASSERT_FALSE(write_tester.IsFulfilled());

  EXPECT_EQ(tcp_writable_stream_wrapper->GetState(),
            StreamWrapper::State::kAborted);
}

class TCPWritableStreamWrapperCloseTestWithMaybePendingWrite
    : public testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(/**/,
                         TCPWritableStreamWrapperCloseTestWithMaybePendingWrite,
                         testing::Bool());

TEST_P(TCPWritableStreamWrapperCloseTestWithMaybePendingWrite, TriggerClose) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_writable_stream_wrapper = stream_creator->Create(scope);

  bool pending_write = GetParam();
  std::optional<ScriptPromiseTester> tester;
  if (pending_write) {
    auto* script_state = scope.GetScriptState();
    auto* chunk =
        DOMArrayBuffer::Create(base::byte_span_with_nul_from_cstring("D"));
    ScriptPromiseUntyped write_promise =
        tcp_writable_stream_wrapper->Writable()
            ->getWriter(script_state, ASSERT_NO_EXCEPTION)
            ->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
    tester.emplace(script_state, write_promise);
    test::RunPendingTasks();
  }

  // 1. OnWriteError(...) is called.
  tcp_writable_stream_wrapper->ErrorStream(net::ERR_UNEXPECTED);

  // 2. pipe reset event arrives.
  stream_creator->ResetPipe();
  test::RunPendingTasks();

  if (pending_write) {
    tester->WaitUntilSettled();
    ASSERT_TRUE(tester->IsRejected());
  }

  ASSERT_EQ(tcp_writable_stream_wrapper->GetState(),
            StreamWrapper::State::kAborted);
}

TEST_P(TCPWritableStreamWrapperCloseTestWithMaybePendingWrite,
       TriggerCloseInReverseOrder) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_writable_stream_wrapper = stream_creator->Create(scope);

  bool pending_write = GetParam();
  std::optional<ScriptPromiseTester> tester;
  if (pending_write) {
    auto* script_state = scope.GetScriptState();
    auto* chunk =
        DOMArrayBuffer::Create(base::byte_span_with_nul_from_cstring("D"));
    ScriptPromiseUntyped write_promise =
        tcp_writable_stream_wrapper->Writable()
            ->getWriter(script_state, ASSERT_NO_EXCEPTION)
            ->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
    tester.emplace(script_state, write_promise);
    test::RunPendingTasks();
  }

  // 1. pipe reset event arrives.
  stream_creator->ResetPipe();
  test::RunPendingTasks();

  // 2. OnWriteError(...) is called.
  tcp_writable_stream_wrapper->ErrorStream(net::ERR_UNEXPECTED);

  if (pending_write) {
    tester->WaitUntilSettled();
    ASSERT_TRUE(tester->IsRejected());
  }

  ASSERT_EQ(tcp_writable_stream_wrapper->GetState(),
            StreamWrapper::State::kAborted);
}

}  // namespace

}  // namespace blink
