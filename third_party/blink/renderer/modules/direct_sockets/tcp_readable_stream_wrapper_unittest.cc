// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using ::testing::ElementsAre;

// The purpose of this class is to ensure that the data pipe is reset before the
// V8TestingScope is destroyed, so that the TCPReadableStreamWrapper object
// doesn't try to create a DOMException after the ScriptState has gone away.
class StreamCreator : public GarbageCollected<StreamCreator> {
 public:
  StreamCreator() = default;
  ~StreamCreator() = default;

  // The default value of |capacity| means some sensible value selected by mojo.
  TCPReadableStreamWrapper* Create(V8TestingScope& scope,
                                   uint32_t capacity = 0) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = capacity;

    mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
    MojoResult result =
        mojo::CreateDataPipe(&options, data_pipe_producer_, data_pipe_consumer);
    if (result != MOJO_RESULT_OK) {
      ADD_FAILURE() << "CreateDataPipe() returned " << result;
    }

    auto* script_state = scope.GetScriptState();
    stream_wrapper_ = MakeGarbageCollected<TCPReadableStreamWrapper>(
        script_state,
        WTF::BindOnce(&StreamCreator::Close, WrapWeakPersistent(this)),
        std::move(data_pipe_consumer));

    scope.PerformMicrotaskCheckpoint();
    test::RunPendingTasks();

    return stream_wrapper_.Get();
  }

  void ResetPipe() { data_pipe_producer_.reset(); }

  void WriteToPipe(Vector<uint8_t> data) {
    EXPECT_EQ(data_pipe_producer_->WriteAllData(data), MOJO_RESULT_OK);
  }

  // Copies the contents of a v8::Value containing a Uint8Array to a Vector.
  static Vector<uint8_t> ToVector(V8TestingScope& scope,
                                  v8::Local<v8::Value> v8value) {
    Vector<uint8_t> ret;

    NotShared<DOMUint8Array> value =
        NativeValueTraits<NotShared<DOMUint8Array>>::NativeValue(
            scope.GetIsolate(), v8value, scope.GetExceptionState());
    if (!value) {
      ADD_FAILURE() << "chunk is not an Uint8Array";
      return ret;
    }
    ret.Append(static_cast<uint8_t*>(value->Data()),
               static_cast<wtf_size_t>(value->byteLength()));
    return ret;
  }

  struct Iterator {
    bool done = false;
    Vector<uint8_t> value;
  };

  // Performs a single read from |reader|, converting the output to the
  // Iterator type. Assumes that the readable stream is not errored.
  // static Iterator Read(const V8TestingScope& scope,
  Iterator Read(V8TestingScope& scope, ReadableStreamDefaultReader* reader) {
    auto* script_state = scope.GetScriptState();
    ScriptPromiseUntyped read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION);
    ScriptPromiseTester tester(script_state, read_promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    return IteratorFromReadResult(scope, tester.Value().V8Value());
  }

  static Iterator IteratorFromReadResult(V8TestingScope& scope,
                                         v8::Local<v8::Value> result) {
    CHECK(result->IsObject());
    Iterator ret;
    v8::Local<v8::Value> v8value;
    if (!V8UnpackIterationResult(scope.GetScriptState(),
                                 result.As<v8::Object>(), &v8value,
                                 &ret.done)) {
      ADD_FAILURE() << "Couldn't unpack iterator";
      return {};
    }
    if (ret.done) {
      EXPECT_TRUE(v8value->IsUndefined());
      return ret;
    }

    ret.value = ToVector(scope, v8value);
    return ret;
  }

  bool CloseCalledWith(bool error) const { return close_called_with_ == error; }

  void Trace(Visitor* visitor) const { visitor->Trace(stream_wrapper_); }

  void Cleanup() { data_pipe_producer_.reset(); }

 private:
  void Close(ScriptValue exception) {
    close_called_with_ = !exception.IsEmpty();
  }

  std::optional<bool> close_called_with_;
  mojo::ScopedDataPipeProducerHandle data_pipe_producer_;
  Member<TCPReadableStreamWrapper> stream_wrapper_;
};

class ScopedStreamCreator {
 public:
  explicit ScopedStreamCreator(StreamCreator* stream_creator)
      : stream_creator_(stream_creator) {}

  ~ScopedStreamCreator() { stream_creator_->Cleanup(); }

  StreamCreator* operator->() const { return stream_creator_; }

 private:
  Persistent<StreamCreator> stream_creator_;
};

TEST(TCPReadableStreamWrapperTest, Create) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  EXPECT_TRUE(tcp_readable_stream_wrapper->Readable());
}

TEST(TCPReadableStreamWrapperTest, ReadArrayBuffer) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* reader =
      tcp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);
  stream_creator->WriteToPipe({'A'});

  StreamCreator::Iterator result = stream_creator->Read(scope, reader);
  EXPECT_FALSE(result.done);
  EXPECT_THAT(result.value, ElementsAre('A'));
}

TEST(TCPReadableStreamWrapperTest, WriteToPipeWithPendingRead) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* reader =
      tcp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped read_promise =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, read_promise);

  stream_creator->WriteToPipe({'A'});

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  StreamCreator::Iterator result =
      stream_creator->IteratorFromReadResult(scope, tester.Value().V8Value());
  EXPECT_FALSE(result.done);
  EXPECT_THAT(result.value, ElementsAre('A'));
}

class TCPReadableStreamWrapperCloseTest : public testing::TestWithParam<bool> {
};

INSTANTIATE_TEST_SUITE_P(/**/,
                         TCPReadableStreamWrapperCloseTest,
                         testing::Bool());

TEST_P(TCPReadableStreamWrapperCloseTest, TriggerClose) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* reader =
      tcp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped read_promise =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, read_promise);

  stream_creator->WriteToPipe({'A'});

  bool graceful = GetParam();

  // 1. OnReadError(...) is called.
  tcp_readable_stream_wrapper->ErrorStream(graceful ? net::OK
                                                    : net::ERR_UNEXPECTED);

  // 2. pipe reset event arrives.
  stream_creator->ResetPipe();
  test::RunPendingTasks();

  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsFulfilled());
  ASSERT_EQ(tcp_readable_stream_wrapper->GetState(),
            graceful ? StreamWrapper::State::kClosed
                     : StreamWrapper::State::kAborted);
}

TEST_P(TCPReadableStreamWrapperCloseTest, TriggerCloseInReverseOrder) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* reader =
      tcp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped read_promise =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, read_promise);

  stream_creator->WriteToPipe({'A'});

  bool graceful = GetParam();

  // 1. pipe reset event arrives.
  stream_creator->ResetPipe();
  test::RunPendingTasks();

  // 2. OnReadError(...) is called.
  tcp_readable_stream_wrapper->ErrorStream(graceful ? net::OK
                                                    : net::ERR_UNEXPECTED);
  tester.WaitUntilSettled();

  ASSERT_TRUE(stream_creator->CloseCalledWith(!graceful));

  ASSERT_TRUE(tester.IsFulfilled());
  ASSERT_EQ(tcp_readable_stream_wrapper->GetState(),
            graceful ? StreamWrapper::State::kClosed
                     : StreamWrapper::State::kAborted);
}

TEST_P(TCPReadableStreamWrapperCloseTest, ErrorCancelReset) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();

  bool graceful = GetParam();

  // 1. OnReadError(...) is called.
  tcp_readable_stream_wrapper->ErrorStream(graceful ? net::OK
                                                    : net::ERR_UNEXPECTED);

  // 2. readable.cancel() is called.
  auto tester = ScriptPromiseTester(
      script_state, tcp_readable_stream_wrapper->Readable()->cancel(
                        script_state, ASSERT_NO_EXCEPTION));
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  // 3. pipe reset event arrives.
  stream_creator->ResetPipe();
  test::RunPendingTasks();

  ASSERT_EQ(tcp_readable_stream_wrapper->GetState(),
            graceful ? StreamWrapper::State::kClosed
                     : StreamWrapper::State::kAborted);
}

TEST_P(TCPReadableStreamWrapperCloseTest, ResetCancelError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();

  bool graceful = GetParam();

  // 1. pipe reset event arrives.
  stream_creator->ResetPipe();
  test::RunPendingTasks();

  // 2. readable.cancel() is called.
  auto tester = ScriptPromiseTester(
      script_state, tcp_readable_stream_wrapper->Readable()->cancel(
                        script_state, ASSERT_NO_EXCEPTION));
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  // 3. OnReadError(...) is called.
  tcp_readable_stream_wrapper->ErrorStream(graceful ? net::OK
                                                    : net::ERR_UNEXPECTED);

  stream_creator->ResetPipe();

  // cancel() always has priority.
  ASSERT_EQ(tcp_readable_stream_wrapper->GetState(),
            StreamWrapper::State::kClosed);
}

}  // namespace

}  // namespace blink
