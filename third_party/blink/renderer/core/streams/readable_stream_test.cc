// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream.h"

#include <optional>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_get_reader_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_readablestreambyobreader_readablestreamdefaultreader.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/core/streams/test_utils.h"
#include "third_party/blink/renderer/core/streams/underlying_byte_source_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/string_resource.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Mock;
using ::testing::Return;

// Web platform tests test ReadableStream more thoroughly from scripts.
class ReadableStreamTest : public testing::Test {
 public:
  ReadableStreamTest() = default;

  std::optional<String> ReadAll(V8TestingScope& scope, ReadableStream* stream) {
    ScriptState* script_state = scope.GetScriptState();
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::Local<v8::Context> context = script_state->GetContext();
    v8::Local<v8::Value> v8_stream =
        ToV8Traits<ReadableStream>::ToV8(script_state, stream);
    v8::Local<v8::Object> global = context->Global();
    bool set_result = false;
    if (!global->Set(context, V8String(isolate, "stream"), v8_stream)
             .To(&set_result)) {
      ADD_FAILURE();
      return std::nullopt;
    }

    const char script[] =
        R"JS(;
result = undefined;
async function readAll(stream) {
  const reader = stream.getReader();
  let temp = "";
  while (true) {
    const v = await reader.read();
    if (v.done) {
      result = temp;
      return;
    }
    temp = temp + v.value;
  }
}
readAll(stream);
)JS";

    if (EvalWithPrintingError(&scope, script).IsEmpty()) {
      ADD_FAILURE();
      return std::nullopt;
    }

    while (true) {
      v8::Local<v8::Value> result;
      if (!global->Get(context, V8String(isolate, "result")).ToLocal(&result)) {
        ADD_FAILURE();
        return std::nullopt;
      }
      if (!result->IsUndefined()) {
        DCHECK(result->IsString());
        return ToCoreString(isolate, result.As<v8::String>());
      }

      // Need to run the event loop for the Serialize test to pass messages
      // through the MessagePort.
      test::RunPendingTasks();

      // Allow Promises to resolve.
      scope.PerformMicrotaskCheckpoint();
    }
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }
  test::TaskEnvironment task_environment_;
};

// This breaks expectations for general ReadableStreamTransferringOptimizer
// subclasses, but we don't care.
class TestTransferringOptimizer final
    : public ReadableStreamTransferringOptimizer {
  USING_FAST_MALLOC(TestTransferringOptimizer);

 public:
  TestTransferringOptimizer() = default;

  UnderlyingSourceBase* PerformInProcessOptimization(
      ScriptState* script_state) override {
    return MakeGarbageCollected<Source>(script_state);
  }

 private:
  class Source final : public UnderlyingSourceBase {
   public:
    explicit Source(ScriptState* script_state)
        : UnderlyingSourceBase(script_state) {}

    ScriptPromiseUntyped Start(ScriptState* script_state,
                               ExceptionState&) override {
      Controller()->Enqueue(V8String(script_state->GetIsolate(), "foo"));
      Controller()->Enqueue(V8String(script_state->GetIsolate(), ", bar"));
      Controller()->Close();
      return ToResolvedUndefinedPromise(script_state);
    }
  };
};

TEST_F(ReadableStreamTest, CreateWithoutArguments) {
  V8TestingScope scope;

  ReadableStream* stream =
      ReadableStream::Create(scope.GetScriptState(), scope.GetExceptionState());
  ASSERT_TRUE(stream);
  ASSERT_FALSE(scope.GetExceptionState().HadException());
}

TEST_F(ReadableStreamTest, CreateWithUnderlyingSourceOnly) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  EXPECT_FALSE(underlying_source->IsStartCalled());

  ReadableStream* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  ASSERT_TRUE(stream);
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(underlying_source->IsStartCalled());
}

TEST_F(ReadableStreamTest, CreateWithFullArguments) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  ScriptValue js_empty_strategy = EvalWithPrintingError(&scope, "{}");
  ASSERT_FALSE(js_empty_strategy.IsEmpty());
  ReadableStream* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  ASSERT_TRUE(stream);
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(underlying_source->IsStartCalled());
}

TEST_F(ReadableStreamTest, CreateWithPathologicalStrategy) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  ScriptValue js_pathological_strategy =
      EvalWithPrintingError(&scope, "({get size() { throw Error('e'); }})");
  ASSERT_FALSE(js_pathological_strategy.IsEmpty());

  ReadableStream* stream = ReadableStream::Create(
      scope.GetScriptState(), ScriptValue(isolate, v8::Undefined(isolate)),
      js_pathological_strategy, scope.GetExceptionState());
  ASSERT_FALSE(stream);
  ASSERT_TRUE(scope.GetExceptionState().HadException());
}

// Testing getReader, locked, IsLocked and IsDisturbed.
TEST_F(ReadableStreamTest, GetReader) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  ReadableStream* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(stream->locked());
  EXPECT_FALSE(stream->IsLocked());
  EXPECT_FALSE(stream->IsDisturbed());

  ReadableStreamDefaultReader* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream->locked());
  EXPECT_TRUE(stream->IsLocked());
  EXPECT_FALSE(stream->IsDisturbed());

  reader->read(script_state, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream->IsDisturbed());
}

// Regression test for https://crbug.com/1187774
TEST_F(ReadableStreamTest, TypeStringEquality) {
  V8TestingScope scope;
  ScriptValue byte_stream =
      EvalWithPrintingError(&scope, "new ReadableStream({type: 'b' + 'ytes'})");
  EXPECT_FALSE(byte_stream.IsEmpty());
}

// Testing getReader with mode BYOB.
TEST_F(ReadableStreamTest, GetBYOBReader) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  ScriptValue byte_stream =
      EvalWithPrintingError(&scope, "new ReadableStream({type: 'bytes'})");
  ReadableStream* stream{
      V8ReadableStream::ToWrappable(isolate, byte_stream.V8Value())};
  ASSERT_TRUE(stream);

  EXPECT_FALSE(stream->locked());
  EXPECT_FALSE(stream->IsLocked());
  EXPECT_FALSE(stream->IsDisturbed());

  auto* options = ReadableStreamGetReaderOptions::Create();
  options->setMode("byob");

  ReadableStreamBYOBReader* reader = nullptr;
  if (const auto* result =
          stream->getReader(script_state, options, ASSERT_NO_EXCEPTION)) {
    reader = result->GetAsReadableStreamBYOBReader();
  }
  ASSERT_TRUE(reader);

  EXPECT_TRUE(stream->locked());
  EXPECT_TRUE(stream->IsLocked());
  EXPECT_FALSE(stream->IsDisturbed());

  NotShared<DOMArrayBufferView> view =
      NotShared<DOMUint8Array>(DOMUint8Array::Create(1));
  reader->read(script_state, view, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream->IsDisturbed());
}

TEST_F(ReadableStreamTest, Cancel) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  ReadableStream* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(underlying_source->IsCancelled());
  EXPECT_FALSE(underlying_source->IsCancelledWithUndefined());
  EXPECT_FALSE(underlying_source->IsCancelledWithNull());

  stream->cancel(script_state, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(underlying_source->IsCancelled());
  EXPECT_TRUE(underlying_source->IsCancelledWithUndefined());
  EXPECT_FALSE(underlying_source->IsCancelledWithNull());
}

TEST_F(ReadableStreamTest, CancelWithNull) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  ReadableStream* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(underlying_source->IsCancelled());
  EXPECT_FALSE(underlying_source->IsCancelledWithUndefined());
  EXPECT_FALSE(underlying_source->IsCancelledWithNull());

  stream->cancel(script_state, ScriptValue(isolate, v8::Null(isolate)),
                 ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(underlying_source->IsCancelled());
  EXPECT_FALSE(underlying_source->IsCancelledWithUndefined());
  EXPECT_TRUE(underlying_source->IsCancelledWithNull());
}

// TODO(yhirano): Write tests for pipeThrough and pipeTo.

TEST_F(ReadableStreamTest, Tee) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  ReadableStream* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  ASSERT_TRUE(stream);

  underlying_source->Enqueue(ScriptValue(isolate, V8String(isolate, "hello")));
  underlying_source->Enqueue(ScriptValue(isolate, V8String(isolate, ", bye")));
  underlying_source->Close();

  ReadableStream* branch1 = nullptr;
  ReadableStream* branch2 = nullptr;
  stream->Tee(script_state, &branch1, &branch2, false, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream->IsLocked());
  EXPECT_FALSE(stream->IsDisturbed());

  ASSERT_TRUE(branch1);
  ASSERT_TRUE(branch2);

  EXPECT_FALSE(branch1->IsLocked());
  EXPECT_FALSE(branch1->IsDisturbed());
  EXPECT_FALSE(branch2->IsLocked());
  EXPECT_FALSE(branch2->IsDisturbed());

  auto result1 = ReadAll(scope, branch1);
  ASSERT_TRUE(result1);
  EXPECT_EQ(*result1, "hello, bye");

  EXPECT_TRUE(stream->IsDisturbed());

  auto result2 = ReadAll(scope, branch2);
  ASSERT_TRUE(result2);
  EXPECT_EQ(*result2, "hello, bye");
}

TEST_F(ReadableStreamTest, Close) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  ASSERT_TRUE(stream);

  EXPECT_TRUE(stream->IsReadable());
  EXPECT_FALSE(stream->IsClosed());
  EXPECT_FALSE(stream->IsErrored());

  underlying_source->Close();

  EXPECT_FALSE(stream->IsReadable());
  EXPECT_TRUE(stream->IsClosed());
  EXPECT_FALSE(stream->IsErrored());
}

TEST_F(ReadableStreamTest, CloseStream) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  ASSERT_TRUE(stream);

  EXPECT_TRUE(stream->IsReadable());
  EXPECT_FALSE(stream->IsClosed());
  EXPECT_FALSE(stream->IsErrored());

  stream->CloseStream(script_state, ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(stream->IsReadable());
  EXPECT_TRUE(stream->IsClosed());
  EXPECT_FALSE(stream->IsErrored());
}

TEST_F(ReadableStreamTest, Error) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  ASSERT_TRUE(stream);

  EXPECT_TRUE(stream->IsReadable());
  EXPECT_FALSE(stream->IsClosed());
  EXPECT_FALSE(stream->IsErrored());

  underlying_source->Error(ScriptValue(isolate, v8::Undefined(isolate)));

  EXPECT_FALSE(stream->IsReadable());
  EXPECT_FALSE(stream->IsClosed());
  EXPECT_TRUE(stream->IsErrored());
}

TEST_F(ReadableStreamTest, LockAndDisturb) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  ASSERT_TRUE(stream);

  EXPECT_FALSE(stream->IsLocked());
  EXPECT_FALSE(stream->IsDisturbed());

  stream->LockAndDisturb(script_state);

  EXPECT_TRUE(stream->IsLocked());
  EXPECT_TRUE(stream->IsDisturbed());
}

TEST_F(ReadableStreamTest, Serialize) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  auto* isolate = scope.GetIsolate();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  ASSERT_TRUE(stream);

  auto* channel =
      MakeGarbageCollected<MessageChannel>(scope.GetExecutionContext());

  stream->Serialize(script_state, channel->port1(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(stream->IsLocked());

  auto* transferred =
      ReadableStream::Deserialize(script_state, channel->port2(),
                                  /*optimizer=*/nullptr, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(transferred);

  underlying_source->Enqueue(ScriptValue(isolate, V8String(isolate, "hello")));
  underlying_source->Enqueue(ScriptValue(isolate, V8String(isolate, ", bye")));
  underlying_source->Close();

  EXPECT_EQ(ReadAll(scope, transferred),
            std::make_optional<String>("hello, bye"));
}

TEST_F(ReadableStreamTest, DeserializeWithNullOptimizer) {
  V8TestingScope scope;
  auto optimizer = std::make_unique<ReadableStreamTransferringOptimizer>();
  auto* script_state = scope.GetScriptState();
  auto* isolate = scope.GetIsolate();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  ASSERT_TRUE(stream);

  auto* channel =
      MakeGarbageCollected<MessageChannel>(scope.GetExecutionContext());

  stream->Serialize(script_state, channel->port1(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(stream->IsLocked());

  auto* transferred =
      ReadableStream::Deserialize(script_state, channel->port2(),
                                  std::move(optimizer), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(transferred);

  underlying_source->Enqueue(ScriptValue(isolate, V8String(isolate, "hello")));
  underlying_source->Enqueue(ScriptValue(isolate, V8String(isolate, ", bye")));
  underlying_source->Close();

  EXPECT_EQ(ReadAll(scope, transferred),
            std::make_optional<String>("hello, bye"));
}

TEST_F(ReadableStreamTest, DeserializeWithTestOptimizer) {
  V8TestingScope scope;
  auto optimizer = std::make_unique<TestTransferringOptimizer>();
  auto* script_state = scope.GetScriptState();
  auto* isolate = scope.GetIsolate();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  ASSERT_TRUE(stream);

  auto* channel =
      MakeGarbageCollected<MessageChannel>(scope.GetExecutionContext());

  stream->Serialize(script_state, channel->port1(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(stream->IsLocked());

  auto* transferred =
      ReadableStream::Deserialize(script_state, channel->port2(),
                                  std::move(optimizer), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(transferred);

  underlying_source->Enqueue(ScriptValue(isolate, V8String(isolate, "hello")));
  underlying_source->Enqueue(ScriptValue(isolate, V8String(isolate, ", bye")));
  underlying_source->Close();

  EXPECT_EQ(ReadAll(scope, transferred),
            std::make_optional<String>("hello, byefoo, bar"));
}

TEST_F(ReadableStreamTest, GarbageCollectJavaScriptUnderlyingSource) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();

  v8::Global<v8::Object> weak_underlying_source;

  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Object> underlying_source = v8::Object::New(isolate);
    ReadableStream::Create(scope.GetScriptState(),
                           ScriptValue(isolate, underlying_source),
                           ASSERT_NO_EXCEPTION);
    weak_underlying_source = v8::Global<v8::Object>(isolate, underlying_source);
    weak_underlying_source.SetWeak();
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_TRUE(weak_underlying_source.IsEmpty());
}

TEST_F(ReadableStreamTest, GarbageCollectCPlusPlusUnderlyingSource) {
  class NoopUnderlyingSource : public UnderlyingSourceBase {
   public:
    explicit NoopUnderlyingSource(ScriptState* script_state)
        : UnderlyingSourceBase(script_state) {}
  };

  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();

  WeakPersistent<NoopUnderlyingSource> weak_underlying_source;

  {
    v8::HandleScope handle_scope(isolate);
    auto* underlying_source =
        MakeGarbageCollected<NoopUnderlyingSource>(scope.GetScriptState());
    weak_underlying_source = underlying_source;
    ReadableStream::CreateWithCountQueueingStrategy(scope.GetScriptState(),
                                                    underlying_source, 0);
  }

  // Allow Promises to resolve.
  scope.PerformMicrotaskCheckpoint();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(weak_underlying_source);
}

class ReadableByteStreamTest : public testing::Test {
 public:
  ReadableByteStreamTest() = default;

  ReadableStream* Stream() const { return stream_; }

  void Init(ScriptState* script_state,
            UnderlyingByteSourceBase* underlying_byte_source) {
    stream_ =
        ReadableStream::CreateByteStream(script_state, underlying_byte_source);
  }

  // This takes the |stream| property of ReadableStream and copies it onto the
  // global object so it can be accessed by Eval().
  void CopyStreamToGlobal(const V8TestingScope& scope) {
    auto* script_state = scope.GetScriptState();
    ReadableStream* stream = Stream();
    v8::Local<v8::Object> global = script_state->GetContext()->Global();
    EXPECT_TRUE(
        global
            ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "stream"),
                  ToV8Traits<ReadableStream>::ToV8(script_state, stream))
            .IsJust());
  }

 private:
  test::TaskEnvironment task_environment_;
  Persistent<ReadableStream> stream_;
};

// A convenient base class to make tests shorter. Subclasses need not implement
// both Pull() and Cancel(), and can override the void versions to avoid
// the need to create a promise to return. Not appropriate for use in
// production.
class TestUnderlyingByteSource : public UnderlyingByteSourceBase {
 public:
  explicit TestUnderlyingByteSource(ScriptState* script_state)
      : script_state_(script_state) {}

  virtual void PullVoid(ReadableByteStreamController*, ExceptionState&) {}

  ScriptPromise<IDLUndefined> Pull(ReadableByteStreamController* controller,
                                   ExceptionState& exception_state) override {
    PullVoid(controller, exception_state);
    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  virtual void CancelVoid() {}

  ScriptPromise<IDLUndefined> Cancel() override {
    return Cancel(v8::Undefined(script_state_->GetIsolate()));
  }

  ScriptPromise<IDLUndefined> Cancel(v8::Local<v8::Value>) override {
    CancelVoid();
    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    UnderlyingByteSourceBase::Trace(visitor);
  }

 private:
  const Member<ScriptState> script_state_;
};

class MockUnderlyingByteSource : public UnderlyingByteSourceBase {
 public:
  explicit MockUnderlyingByteSource(ScriptState* script_state)
      : script_state_(script_state) {}

  MOCK_METHOD2(Pull,
               ScriptPromise<IDLUndefined>(ReadableByteStreamController*,
                                           ExceptionState&));
  MOCK_METHOD0(Cancel, ScriptPromise<IDLUndefined>());
  MOCK_METHOD1(Cancel,
               ScriptPromise<IDLUndefined>(v8::Local<v8::Value> reason));

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    UnderlyingByteSourceBase::Trace(visitor);
  }

 private:
  const Member<ScriptState> script_state_;
};

TEST_F(ReadableByteStreamTest, Construct) {
  V8TestingScope scope;
  Init(scope.GetScriptState(),
       MakeGarbageCollected<TestUnderlyingByteSource>(scope.GetScriptState()));
  EXPECT_TRUE(Stream());
}

TEST_F(ReadableByteStreamTest, PullIsCalled) {
  V8TestingScope scope;
  auto* mock =
      MakeGarbageCollected<MockUnderlyingByteSource>(scope.GetScriptState());
  Init(scope.GetScriptState(), mock);
  // Need to run microtasks so the startAlgorithm promise resolves.
  scope.PerformMicrotaskCheckpoint();
  CopyStreamToGlobal(scope);

  EXPECT_CALL(*mock, Pull(_, _))
      .WillOnce(
          Return(ByMove(ToResolvedUndefinedPromise(scope.GetScriptState()))));

  EvalWithPrintingError(
      &scope, "stream.getReader({ mode: 'byob' }).read(new Uint8Array(1));\n");

  Mock::VerifyAndClear(mock);
  Mock::AllowLeak(mock);
}

TEST_F(ReadableByteStreamTest, CancelIsCalled) {
  V8TestingScope scope;
  auto* mock =
      MakeGarbageCollected<MockUnderlyingByteSource>(scope.GetScriptState());
  Init(scope.GetScriptState(), mock);
  // Need to run microtasks so the startAlgorithm promise resolves.
  scope.PerformMicrotaskCheckpoint();
  CopyStreamToGlobal(scope);

  EXPECT_CALL(*mock, Cancel(_))
      .WillOnce(
          Return(ByMove(ToResolvedUndefinedPromise(scope.GetScriptState()))));

  EvalWithPrintingError(&scope,
                        "const reader = stream.getReader({ mode: 'byob' });\n"
                        "reader.cancel('a');\n");

  Mock::VerifyAndClear(mock);
  Mock::AllowLeak(mock);
}

bool IsTypeError(ScriptState* script_state,
                 ScriptValue value,
                 const String& message) {
  v8::Local<v8::Object> object;
  if (!value.V8Value()->ToObject(script_state->GetContext()).ToLocal(&object)) {
    return false;
  }
  if (!object->IsNativeError())
    return false;

  const auto& Has = [script_state, object](const String& key,
                                           const String& value) -> bool {
    v8::Local<v8::Value> actual;
    return object
               ->Get(script_state->GetContext(),
                     V8AtomicString(script_state->GetIsolate(), key))
               .ToLocal(&actual) &&
           ToCoreStringWithUndefinedOrNullCheck(script_state->GetIsolate(),
                                                actual) == value;
  };

  return Has("name", "TypeError") && Has("message", message);
}

TEST_F(ReadableByteStreamTest, ThrowFromPull) {
  static constexpr char kMessage[] = "errorInPull";
  class ThrowFromPullUnderlyingByteSource final
      : public TestUnderlyingByteSource {
   public:
    explicit ThrowFromPullUnderlyingByteSource(ScriptState* script_state)
        : TestUnderlyingByteSource(script_state) {}

    void PullVoid(ReadableByteStreamController*,
                  ExceptionState& exception_state) override {
      exception_state.ThrowTypeError(kMessage);
    }
  };

  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(script_state,
       MakeGarbageCollected<ThrowFromPullUnderlyingByteSource>(script_state));

  auto* reader =
      Stream()->GetBYOBReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  NotShared<DOMArrayBufferView> view =
      NotShared<DOMUint8Array>(DOMUint8Array::Create(1));
  ScriptPromiseTester read_tester(
      script_state, reader->read(script_state, view, ASSERT_NO_EXCEPTION));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, read_tester.Value(), kMessage));
}

TEST_F(ReadableByteStreamTest, ThrowFromCancel) {
  static constexpr char kMessage[] = "errorInCancel";
  class ThrowFromCancelUnderlyingByteSource final
      : public TestUnderlyingByteSource {
   public:
    explicit ThrowFromCancelUnderlyingByteSource(ScriptState* script_state)
        : TestUnderlyingByteSource(script_state) {}

    void CancelVoid() override {
      V8ThrowException::ThrowTypeError(GetScriptState()->GetIsolate(),
                                       kMessage);
    }
  };

  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(script_state,
       MakeGarbageCollected<ThrowFromCancelUnderlyingByteSource>(script_state));

  auto* reader =
      Stream()->GetBYOBReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(
      script_state, reader->cancel(script_state, ASSERT_NO_EXCEPTION));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, read_tester.Value(), kMessage));
}

TEST_F(ReadableByteStreamTest, CloseStream) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(script_state,
       MakeGarbageCollected<TestUnderlyingByteSource>(script_state));
  EXPECT_TRUE(Stream());

  auto* reader =
      Stream()->GetBYOBReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  NotShared<DOMArrayBufferView> view =
      NotShared<DOMUint8Array>(DOMUint8Array::Create(1));
  ScriptPromiseTester read_tester(
      script_state, reader->read(script_state, view, ASSERT_NO_EXCEPTION));
  // Close a byte stream with pending pull intos should fulfill read requests
  // with bytes filled is 0 and done is true.
  Stream()->CloseStream(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  EXPECT_FALSE(Stream()->IsReadable());
  EXPECT_TRUE(Stream()->IsClosed());
  EXPECT_FALSE(Stream()->IsErrored());
}

}  // namespace

}  // namespace blink
