// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"

#include "base/test/mock_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::testing::StrictMock;

// The purpose of this class is to ensure that the data pipe is reset before the
// V8TestingScope is destroyed, so that the TCPReadableStreamWrapper object
// doesn't try to create a DOMException after the ScriptState has gone away.
class StreamCreator : public GarbageCollected<StreamCreator> {
 public:
  StreamCreator() = default;
  ~StreamCreator() {
    // Let the TCPReadableStreamWrapper object respond to the closure if it
    // needs to.
    test::RunPendingTasks();
  }

  // The default value of |capacity| means some sensible value selected by mojo.
  TCPReadableStreamWrapper* Create(const V8TestingScope& scope,
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
        base::BindOnce(&StreamCreator::Close, base::Unretained(this)),
        std::move(data_pipe_consumer));
    return stream_wrapper_;
  }

  void WriteToPipe(Vector<uint8_t> data) {
    uint32_t num_bytes = data.size();
    EXPECT_EQ(data_pipe_producer_->WriteData(data.data(), &num_bytes,
                                             MOJO_WRITE_DATA_FLAG_ALL_OR_NONE),
              MOJO_RESULT_OK);
    EXPECT_EQ(num_bytes, data.size());
  }

  void ClosePipe() { data_pipe_producer_.reset(); }

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
    ScriptPromise read_promise =
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
    if (!V8UnpackIteratorResult(scope.GetScriptState(), result.As<v8::Object>(),
                                &ret.done)
             .ToLocal(&v8value)) {
      ADD_FAILURE() << "Couldn't unpack iterator";
      return ret;
    }
    if (ret.done) {
      EXPECT_TRUE(v8value->IsUndefined());
      return ret;
    }

    ret.value = ToVector(scope, v8value);
    return ret;
  }

  void Close(bool error) { stream_wrapper_->CloseStream(error); }

  void Trace(Visitor* visitor) const { visitor->Trace(stream_wrapper_); }

 private:
  mojo::ScopedDataPipeProducerHandle data_pipe_producer_;
  Member<TCPReadableStreamWrapper> stream_wrapper_;
};

TEST(TCPReadableStreamWrapperTest, Create) {
  V8TestingScope scope;

  auto* stream_creator = MakeGarbageCollected<StreamCreator>();
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  EXPECT_TRUE(tcp_readable_stream_wrapper->Readable());
}

TEST(TCPReadableStreamWrapperTest, ReadArrayBuffer) {
  V8TestingScope scope;

  auto* stream_creator = MakeGarbageCollected<StreamCreator>();
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
  V8TestingScope scope;

  auto* stream_creator = MakeGarbageCollected<StreamCreator>();
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* reader =
      tcp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, read_promise);

  test::RunPendingTasks();

  stream_creator->WriteToPipe({'A'});

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  StreamCreator::Iterator result =
      stream_creator->IteratorFromReadResult(scope, tester.Value().V8Value());
  EXPECT_FALSE(result.done);
  EXPECT_THAT(result.value, ElementsAre('A'));
}

TEST(TCPReadableStreamWrapperTest, TriggerOnAborted) {
  V8TestingScope scope;

  auto* stream_creator = MakeGarbageCollected<StreamCreator>();
  auto* tcp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* reader =
      tcp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, read_promise);

  test::RunPendingTasks();
  stream_creator->WriteToPipe({'A'});
  // Trigger OnAborted() on purpose.
  stream_creator->ClosePipe();
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsFulfilled());
  ASSERT_EQ(tcp_readable_stream_wrapper->GetState(),
            StreamWrapper::State::kAborted);
}

}  // namespace

}  // namespace blink
