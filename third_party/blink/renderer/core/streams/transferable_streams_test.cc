// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transferable_streams.h"

#include "base/types/strong_alias.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_default_reader.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

enum class SourceType { kPush, kPull };

class TestUnderlyingSource final : public UnderlyingSourceBase {
 public:
  TestUnderlyingSource(SourceType source_type,
                       ScriptState* script_state,
                       Vector<int> sequence,
                       ScriptPromiseUntyped start_promise)
      : UnderlyingSourceBase(script_state),
        type_(source_type),
        sequence_(std::move(sequence)),
        start_promise_(start_promise) {}
  TestUnderlyingSource(SourceType source_type,
                       ScriptState* script_state,
                       Vector<int> sequence)
      : TestUnderlyingSource(source_type,
                             script_state,
                             std::move(sequence),
                             ToResolvedUndefinedPromise(script_state)) {}
  ~TestUnderlyingSource() override = default;

  ScriptPromiseUntyped Start(ScriptState* script_state,
                             ExceptionState&) override {
    started_ = true;
    if (type_ == SourceType::kPush) {
      for (int element : sequence_) {
        EnqueueOrError(script_state, element);
      }
      index_ = sequence_.size();
      Controller()->Close();
    }
    return start_promise_;
  }
  ScriptPromiseUntyped Pull(ScriptState* script_state,
                            ExceptionState&) override {
    if (type_ == SourceType::kPush) {
      return ToResolvedUndefinedPromise(script_state);
    }
    if (index_ == sequence_.size()) {
      Controller()->Close();
      return ToResolvedUndefinedPromise(script_state);
    }
    EnqueueOrError(script_state, sequence_[index_]);
    ++index_;
    return ToResolvedUndefinedPromise(script_state);
  }
  ScriptPromiseUntyped Cancel(ScriptState* script_state,
                              ScriptValue reason,
                              ExceptionState&) override {
    cancelled_ = true;
    cancel_reason_ = reason;
    return ToResolvedUndefinedPromise(script_state);
  }

  bool IsStarted() const { return started_; }
  bool IsCancelled() const { return cancelled_; }
  ScriptValue CancelReason() const { return cancel_reason_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(start_promise_);
    visitor->Trace(cancel_reason_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  void EnqueueOrError(ScriptState* script_state, int num) {
    if (num < 0) {
      Controller()->Error(V8ThrowException::CreateRangeError(
          script_state->GetIsolate(), "foo"));
      return;
    }
    Controller()->Enqueue(v8::Integer::New(script_state->GetIsolate(), num));
  }

  const SourceType type_;
  const Vector<int> sequence_;
  wtf_size_t index_ = 0;

  const ScriptPromiseUntyped start_promise_;
  bool started_ = false;
  bool cancelled_ = false;
  ScriptValue cancel_reason_;
};

void ExpectValue(int line,
                 ScriptState* script_state,
                 v8::Local<v8::Value> result,
                 int32_t expectation) {
  SCOPED_TRACE(testing::Message() << "__LINE__ = " << line);
  if (!result->IsObject()) {
    ADD_FAILURE() << "The result is not an Object.";
    return;
  }
  v8::Local<v8::Value> value;
  bool done = false;
  if (!V8UnpackIterationResult(script_state, result.As<v8::Object>(), &value,
                               &done)) {
    ADD_FAILURE() << "Failed to unpack the iterator result.";
    return;
  }
  EXPECT_FALSE(done);
  if (!value->IsInt32()) {
    ADD_FAILURE() << "The value is not an int32.";
    return;
  }
  EXPECT_EQ(value.As<v8::Number>()->Value(), expectation);
}

void ExpectDone(int line,
                ScriptState* script_state,
                v8::Local<v8::Value> result) {
  SCOPED_TRACE(testing::Message() << "__LINE__ = " << line);
  v8::Local<v8::Value> value;
  bool done = false;
  if (!V8UnpackIterationResult(script_state, result.As<v8::Object>(), &value,
                               &done)) {
    ADD_FAILURE() << "Failed to unpack the iterator result.";
    return;
  }
  EXPECT_TRUE(done);
}

// We only do minimal testing here. The functionality of transferable streams is
// tested in the layout tests.
TEST(TransferableStreamsTest, SmokeTest) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto* channel =
      MakeGarbageCollected<MessageChannel>(scope.GetExecutionContext());
  auto* script_state = scope.GetScriptState();
  auto* writable = CreateCrossRealmTransformWritable(
      script_state, channel->port1(), AllowPerChunkTransferring(false),
      /*optimizer=*/nullptr, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(writable);
  auto* readable = CreateCrossRealmTransformReadable(
      script_state, channel->port2(), /*optimizer=*/nullptr,
      ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(readable);

  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  writer->write(script_state, ScriptValue::CreateNull(scope.GetIsolate()),
                ASSERT_NO_EXCEPTION);

  class ExpectNullResponse : public ScriptFunction::Callable {
   public:
    explicit ExpectNullResponse(bool* got_response)
        : got_response_(got_response) {}

    ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
      *got_response_ = true;
      if (!value.IsObject()) {
        ADD_FAILURE() << "iterator must be an object";
        return ScriptValue();
      }
      v8::Local<v8::Value> chunk;
      bool done = false;
      if (!V8UnpackIterationResult(script_state,
                                   value.V8Value()
                                       ->ToObject(script_state->GetContext())
                                       .ToLocalChecked(),
                                   &chunk, &done)) {
        ADD_FAILURE() << "V8UnpackIterationResult failed";
        return ScriptValue();
      }
      EXPECT_FALSE(done);
      EXPECT_TRUE(chunk->IsNull());
      return ScriptValue();
    }

    bool* got_response_;
  };

  // TODO(ricea): This is copy-and-pasted from transform_stream_test.cc. Put it
  // in a shared location.
  class ExpectNotReached : public ScriptFunction::Callable {
   public:
    ExpectNotReached() = default;

    ScriptValue Call(ScriptState*, ScriptValue) override {
      ADD_FAILURE() << "ExpectNotReached was reached";
      return ScriptValue();
    }
  };

  bool got_response = false;
  reader->read(script_state, ASSERT_NO_EXCEPTION)
      .Then(MakeGarbageCollected<ScriptFunction>(
                script_state,
                MakeGarbageCollected<ExpectNullResponse>(&got_response)),
            MakeGarbageCollected<ScriptFunction>(
                script_state, MakeGarbageCollected<ExpectNotReached>()));

  // Need to run the event loop to pass messages through the MessagePort.
  test::RunPendingTasks();

  // Resolve promises.
  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(got_response);
}

TEST(ConcatenatedReadableStreamTest, Empty) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({}));
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({}));

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectDone(__LINE__, script_state, read_promise->Result());
  }
  EXPECT_TRUE(source1->IsStarted());
  EXPECT_TRUE(source2->IsStarted());
  EXPECT_FALSE(source1->IsCancelled());
  EXPECT_FALSE(source2->IsCancelled());
}

TEST(ConcatenatedReadableStreamTest, SuccessfulRead) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({1}));
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({5, 6}));

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 1);
    EXPECT_TRUE(source1->IsStarted());
    EXPECT_FALSE(source2->IsStarted());
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 5);
    EXPECT_TRUE(source2->IsStarted());
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 6);
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectDone(__LINE__, script_state, read_promise->Result());
  }
  EXPECT_FALSE(source1->IsCancelled());
  EXPECT_FALSE(source2->IsCancelled());
}

TEST(ConcatenatedReadableStreamTest, SuccessfulReadForPushSources) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPush, script_state, Vector<int>({1}));
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPush, script_state, Vector<int>({5, 6}));

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 1);
    EXPECT_TRUE(source1->IsStarted());
    EXPECT_FALSE(source2->IsStarted());
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 5);
    EXPECT_TRUE(source2->IsStarted());
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 6);
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectDone(__LINE__, script_state, read_promise->Result());
  }
  EXPECT_FALSE(source1->IsCancelled());
  EXPECT_FALSE(source2->IsCancelled());
}

TEST(ConcatenatedReadableStreamTest, ErrorInSource1) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({1, -2}));
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({5, 6}));

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 1);
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kRejected);
  }
  EXPECT_TRUE(source1->IsStarted());
  EXPECT_FALSE(source1->IsCancelled());
  EXPECT_TRUE(source2->IsStarted());
  EXPECT_TRUE(source2->IsCancelled());
}

TEST(ConcatenatedReadableStreamTest, ErrorInSource2) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({1}));
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({-2}));

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 1);
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kRejected);
  }
  EXPECT_TRUE(source1->IsStarted());
  EXPECT_FALSE(source1->IsCancelled());
  EXPECT_TRUE(source2->IsStarted());
  EXPECT_FALSE(source2->IsCancelled());
}

TEST(ConcatenatedReadableStreamTest, Cancel1) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({1, 2}));
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({5, 6}));

  ScriptValue reason(script_state->GetIsolate(),
                     V8String(script_state->GetIsolate(), "hello"));

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 1);
  }
  EXPECT_TRUE(source1->IsStarted());
  EXPECT_FALSE(source1->IsCancelled());
  EXPECT_FALSE(source2->IsStarted());
  EXPECT_FALSE(source2->IsCancelled());
  {
    reader->cancel(script_state, reason, ASSERT_NO_EXCEPTION);
    scope.PerformMicrotaskCheckpoint();
  }
  EXPECT_TRUE(source1->IsStarted());
  EXPECT_TRUE(source1->IsCancelled());
  EXPECT_EQ(reason, source1->CancelReason());
  EXPECT_TRUE(source2->IsStarted());
  EXPECT_TRUE(source2->IsCancelled());
  EXPECT_EQ(reason, source2->CancelReason());
}

TEST(ConcatenatedReadableStreamTest, Cancel2) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({}));
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({5}));

  ScriptValue reason(script_state->GetIsolate(),
                     V8String(script_state->GetIsolate(), "hello"));

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 5);
  }
  {
    reader->cancel(script_state, reason, ASSERT_NO_EXCEPTION);
    scope.PerformMicrotaskCheckpoint();
  }
  EXPECT_TRUE(source1->IsStarted());
  EXPECT_FALSE(source1->IsCancelled());
  EXPECT_TRUE(source2->IsStarted());
  EXPECT_TRUE(source2->IsCancelled());
  EXPECT_EQ(reason, source2->CancelReason());
}

TEST(ConcatenatedReadableStreamTest, PendingStart1) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({1, 2}),
      resolver->Promise());
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({5, 6}));

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kPending);

    resolver->Resolve();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 1);
  }
  EXPECT_TRUE(source1->IsStarted());
  EXPECT_FALSE(source2->IsStarted());
}

TEST(ConcatenatedReadableStreamTest, PendingStart2) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  TestUnderlyingSource* source1 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({1}));
  TestUnderlyingSource* source2 = MakeGarbageCollected<TestUnderlyingSource>(
      SourceType::kPull, script_state, Vector<int>({5, 6}),
      resolver->Promise());

  ReadableStream* stream =
      CreateConcatenatedReadableStream(script_state, source1, source2);
  ASSERT_TRUE(stream);

  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(reader);

  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 1);
  }
  {
    v8::Local<v8::Promise> read_promise =
        reader->read(script_state, ASSERT_NO_EXCEPTION).V8Promise();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kPending);

    resolver->Resolve();
    scope.PerformMicrotaskCheckpoint();
    ASSERT_EQ(read_promise->State(), v8::Promise::kFulfilled);
    ExpectValue(__LINE__, script_state, read_promise->Result(), 5);
  }
  EXPECT_TRUE(source1->IsStarted());
  EXPECT_TRUE(source2->IsStarted());
}

}  // namespace

}  // namespace blink
