// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transform_stream.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_extras_test_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/garbage_collected_script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::Mock;

class TransformStreamTest : public ::testing::Test {
 public:
  void TearDown() override {
    if (holder_)
      ClearHolder();
  }

  void Init(TransformStreamTransformer* transformer,
            ScriptState* script_state,
            ExceptionState& exception_state) {
    holder_ = new Holder(script_state);
    holder_->Stream()->Init(transformer, script_state, exception_state);
  }

  TransformStream* Stream() const { return holder_->Stream(); }

  // This takes the |readable| and |writable| properties of the TransformStream
  // and copies them onto the global object so they can be accessed by Eval().
  void CopyReadableAndWritableToGlobal(const V8TestingScope& scope) {
    auto* script_state = scope.GetScriptState();
    ScriptValue readable =
        Stream()->Readable(script_state, ASSERT_NO_EXCEPTION);
    ScriptValue writable =
        Stream()->Writable(script_state, ASSERT_NO_EXCEPTION);
    v8::Local<v8::Object> global = script_state->GetContext()->Global();
    EXPECT_TRUE(global
                    ->Set(scope.GetContext(),
                          V8String(scope.GetIsolate(), "readable"),
                          readable.V8Value())
                    .IsJust());
    EXPECT_TRUE(global
                    ->Set(scope.GetContext(),
                          V8String(scope.GetIsolate(), "writable"),
                          writable.V8Value())
                    .IsJust());
  }

  void ClearHolder() {
    holder_->Destroy();
    holder_ = nullptr;
  }

 private:
  // In normal use, TransformStream will be referenced by a ScriptWrappable and
  // so will be visible to the V8 garbage collector via wrapper tracing. For
  // testing purposes we need a dummy ScriptWrappable object to do the same
  // thing.
  // TODO(ricea): Remove this once unified GC has replaced wrapper tracing.
  class Holder : public GarbageCollectedScriptWrappable {
   public:
    explicit Holder(ScriptState* script_state)
        : GarbageCollectedScriptWrappable("Holder"),
          this_as_v8_value_(
              ScriptValue(script_state, ToV8(this, script_state))),
          stream_(new TransformStream()) {}

    // Destroy() must be called to break the reference cycle.
    void Destroy() {
      this_as_v8_value_.Clear();
      stream_ = nullptr;
    }

    TransformStream* Stream() const { return stream_.Get(); }

    void Trace(Visitor* visitor) override {
      visitor->Trace(stream_);
      GarbageCollectedScriptWrappable::Trace(visitor);
    }

   private:
    // Self-reference to keep this object referenced from V8.
    ScriptValue this_as_v8_value_;
    TraceWrapperMember<TransformStream> stream_;
  };

  Persistent<Holder> holder_;
};

class IdentityTransformer final : public TransformStreamTransformer {
 public:
  void Transform(v8::Local<v8::Value> chunk,
                 TransformStreamDefaultController* controller,
                 ExceptionState& exception_state) override {
    controller->Enqueue(chunk, exception_state);
  }

  void Flush(TransformStreamDefaultController* controller,
             ExceptionState& exception_state) override {}
};

class MockTransformStreamTransformer : public TransformStreamTransformer {
 public:
  MOCK_METHOD3(Transform,
               void(v8::Local<v8::Value> chunk,
                    TransformStreamDefaultController*,
                    ExceptionState&));
  MOCK_METHOD2(Flush, void(TransformStreamDefaultController*, ExceptionState&));
};

// If this doesn't work then nothing else will.
TEST_F(TransformStreamTest, Construct) {
  V8TestingScope scope;
  Init(new IdentityTransformer(), scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(Stream());
}

TEST_F(TransformStreamTest, Accessors) {
  V8TestingScope scope;
  Init(new IdentityTransformer(), scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptValue readable =
      Stream()->Readable(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptValue writable =
      Stream()->Writable(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(readable.IsObject());
  EXPECT_TRUE(writable.IsObject());
  EXPECT_TRUE(ReadableStreamOperations::IsReadableStream(
                  scope.GetScriptState(), readable, ASSERT_NO_EXCEPTION)
                  .value_or(false));
  // TODO(ricea): Check writable too once we have a wrapper for
  // IsWritableStream().
}

TEST_F(TransformStreamTest, TransformIsCalled) {
  V8TestingScope scope;
  auto* mock = new ::testing::StrictMock<MockTransformStreamTransformer>();
  Init(mock, scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // Need to run microtasks so the startAlgorithm promise resolves.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  CopyReadableAndWritableToGlobal(scope);

  EXPECT_CALL(*mock, Transform(_, _, _));

  // The initial read is needed to relieve backpressure.
  EvalWithPrintingError(&scope,
                        "readable.getReader().read();\n"
                        "const writer = writable.getWriter();\n"
                        "writer.write('a');\n");

  Mock::VerifyAndClear(mock);
  Mock::AllowLeak(mock);
}

TEST_F(TransformStreamTest, FlushIsCalled) {
  V8TestingScope scope;
  auto* mock = new ::testing::StrictMock<MockTransformStreamTransformer>();
  Init(mock, scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // Need to run microtasks so the startAlgorithm promise resolves.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  CopyReadableAndWritableToGlobal(scope);

  EXPECT_CALL(*mock, Flush(_, _));

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.close();\n");

  Mock::VerifyAndClear(mock);
  Mock::AllowLeak(mock);
}

class ExpectNotReached : public ScriptFunction {
 public:
  static v8::Local<v8::Function> Create(ScriptState* script_state) {
    auto* self = new ExpectNotReached(script_state);
    return self->BindToV8Function();
  }

 private:
  explicit ExpectNotReached(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  ScriptValue Call(ScriptValue) override {
    ADD_FAILURE() << "ExpectNotReached was reached";
    return ScriptValue();
  }
};

// Fails the test if the iterator passed to the function does not have a value
// of exactly |expected|.
class ExpectChunkIsString : public ScriptFunction {
 public:
  static v8::Local<v8::Function> Create(ScriptState* script_state,
                                        const String& expected,
                                        bool* called) {
    auto* self = new ExpectChunkIsString(script_state, expected, called);
    return self->BindToV8Function();
  }

 private:
  ExpectChunkIsString(ScriptState* script_state,
                      const String& expected,
                      bool* called)
      : ScriptFunction(script_state), expected_(expected), called_(called) {}

  ScriptValue Call(ScriptValue value) override {
    *called_ = true;
    if (!value.IsObject()) {
      ADD_FAILURE() << "iterator must be an object";
      return ScriptValue();
    }
    bool done = false;
    auto* script_state = GetScriptState();
    auto chunk = V8UnpackIteratorResult(
        script_state,
        value.V8Value()->ToObject(script_state->GetContext()).ToLocalChecked(),
        &done);
    EXPECT_FALSE(done);
    EXPECT_FALSE(chunk.IsEmpty());
    EXPECT_EQ(ToCoreStringWithUndefinedOrNullCheck(chunk.ToLocalChecked()),
              expected_);
    return ScriptValue();
  }

  String expected_;
  bool* called_;
};

class ExpectTypeError : public ScriptFunction {
 public:
  static v8::Local<v8::Function> Create(ScriptState* script_state,
                                        const String& message,
                                        bool* called) {
    auto* self = new ExpectTypeError(script_state, message, called);
    return self->BindToV8Function();
  }

 private:
  ExpectTypeError(ScriptState* script_state,
                  const String& message,
                  bool* called)
      : ScriptFunction(script_state), message_(message), called_(called) {}

  ScriptValue Call(ScriptValue value) override {
    *called_ = true;
    EXPECT_TRUE(IsTypeError(GetScriptState(), value, message_));
    return ScriptValue();
  }

  static bool IsTypeError(ScriptState* script_state,
                          ScriptValue value,
                          const String& message) {
    v8::Local<v8::Object> object;
    if (!value.V8Value()
             ->ToObject(script_state->GetContext())
             .ToLocal(&object)) {
      return false;
    }
    if (!object->IsNativeError())
      return false;
    return Has(script_state, object, "name", "TypeError") &&
           Has(script_state, object, "message", message);
  }

  static bool Has(ScriptState* script_state,
                  v8::Local<v8::Object> object,
                  const String& key,
                  const String& value) {
    auto context = script_state->GetContext();
    auto* isolate = script_state->GetIsolate();
    v8::Local<v8::Value> actual;
    return object->Get(context, V8AtomicString(isolate, key))
               .ToLocal(&actual) &&
           ToCoreStringWithUndefinedOrNullCheck(actual) == value;
  }

  String message_;
  bool* called_;
};

TEST_F(TransformStreamTest, EnqueueFromTransform) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(new IdentityTransformer(), script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.write('a');\n");

  ScriptValue readable = Stream()->Readable(script_state, ASSERT_NO_EXCEPTION);
  ScriptValue reader = ReadableStreamOperations::GetReader(
      script_state, readable, ASSERT_NO_EXCEPTION);
  bool chunk_seen = false;
  ReadableStreamOperations::DefaultReaderRead(script_state, reader)
      .Then(ExpectChunkIsString::Create(script_state, "a", &chunk_seen),
            ExpectNotReached::Create(script_state));
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(chunk_seen);
}

TEST_F(TransformStreamTest, EnqueueFromFlush) {
  class EnqueueFromFlushTransformer : public TransformStreamTransformer {
   public:
    EnqueueFromFlushTransformer(v8::Local<v8::Object> global,
                                v8::Isolate* isolate)
        : global_(global), isolate_(isolate) {}

    void Transform(v8::Local<v8::Value>,
                   TransformStreamDefaultController*,
                   ExceptionState&) override {}
    void Flush(TransformStreamDefaultController* controller,
               ExceptionState& exception_state) override {
      controller->Enqueue(ToV8("a", global_, isolate_), exception_state);
    }

   private:
    v8::Local<v8::Object> global_;
    v8::Isolate* isolate_;
  };
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(new EnqueueFromFlushTransformer(scope.GetContext()->Global(),
                                       scope.GetIsolate()),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.close();\n");

  ScriptValue readable = Stream()->Readable(script_state, ASSERT_NO_EXCEPTION);
  ScriptValue reader = ReadableStreamOperations::GetReader(
      script_state, readable, ASSERT_NO_EXCEPTION);
  bool chunkSeen = false;
  ReadableStreamOperations::DefaultReaderRead(script_state, reader)
      .Then(ExpectChunkIsString::Create(script_state, "a", &chunkSeen),
            ExpectNotReached::Create(script_state));
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(chunkSeen);
}

TEST_F(TransformStreamTest, ThrowFromTransform) {
  static constexpr char kMessage[] = "errorInTransform";
  class ThrowFromTransformTransformer : public TransformStreamTransformer {
   public:
    void Transform(v8::Local<v8::Value>,
                   TransformStreamDefaultController*,
                   ExceptionState& exception_state) override {
      exception_state.ThrowTypeError(kMessage);
    }
    void Flush(TransformStreamDefaultController*, ExceptionState&) override {}
  };
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(new ThrowFromTransformTransformer(), script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  ScriptValue promise =
      EvalWithPrintingError(&scope,
                            "const writer = writable.getWriter();\n"
                            "writer.write('a');\n");

  ScriptValue readable = Stream()->Readable(script_state, ASSERT_NO_EXCEPTION);
  ScriptValue reader = ReadableStreamOperations::GetReader(
      script_state, readable, ASSERT_NO_EXCEPTION);
  bool readableTypeErrorThrown = false;
  bool writableTypeErrorThrown = false;
  ReadableStreamOperations::DefaultReaderRead(script_state, reader)
      .Then(ExpectNotReached::Create(script_state),
            ExpectTypeError::Create(script_state, kMessage,
                                    &readableTypeErrorThrown));
  ScriptPromise::Cast(script_state, promise)
      .Then(ExpectNotReached::Create(script_state),
            ExpectTypeError::Create(script_state, kMessage,
                                    &writableTypeErrorThrown));
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(readableTypeErrorThrown);
  EXPECT_TRUE(writableTypeErrorThrown);
}

TEST_F(TransformStreamTest, ThrowFromFlush) {
  static constexpr char kMessage[] = "errorInFlush";
  class ThrowFromFlushTransformer : public TransformStreamTransformer {
   public:
    void Transform(v8::Local<v8::Value>,
                   TransformStreamDefaultController*,
                   ExceptionState&) override {}
    void Flush(TransformStreamDefaultController*,
               ExceptionState& exception_state) override {
      exception_state.ThrowTypeError(kMessage);
    }
  };
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(new ThrowFromFlushTransformer(), script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  ScriptValue promise =
      EvalWithPrintingError(&scope,
                            "const writer = writable.getWriter();\n"
                            "writer.close();\n");

  ScriptValue readable = Stream()->Readable(script_state, ASSERT_NO_EXCEPTION);
  ScriptValue reader = ReadableStreamOperations::GetReader(
      script_state, readable, ASSERT_NO_EXCEPTION);
  bool readableTypeErrorThrown = false;
  bool writableTypeErrorThrown = false;
  ReadableStreamOperations::DefaultReaderRead(script_state, reader)
      .Then(ExpectNotReached::Create(script_state),
            ExpectTypeError::Create(script_state, kMessage,
                                    &readableTypeErrorThrown));
  ScriptPromise::Cast(script_state, promise)
      .Then(ExpectNotReached::Create(script_state),
            ExpectTypeError::Create(script_state, kMessage,
                                    &writableTypeErrorThrown));
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(readableTypeErrorThrown);
  EXPECT_TRUE(writableTypeErrorThrown);
}

// Verify that the JavaScript TransformStream object is kept alive by the C++
// TransformStream object.
TEST_F(TransformStreamTest, SurvivesGarbageCollectionWhenTraced) {
  auto page_holder = DummyPageHolder::Create();
  Persistent<ScriptState> script_state =
      ToScriptStateForMainWorld(page_holder->GetDocument().GetFrame());
  {
    ScriptState::Scope scope(script_state);
    Init(new IdentityTransformer(), script_state, ASSERT_NO_EXCEPTION);
  }
  Microtask::PerformCheckpoint(script_state->GetIsolate());
  script_state->GetIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  ScriptState::Scope scope(script_state);
  ScriptValue readable = Stream()->Readable(script_state, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(readable.IsObject());
  EXPECT_TRUE(ReadableStreamOperations::IsReadableStream(script_state, readable,
                                                         ASSERT_NO_EXCEPTION)
                  .value_or(false));
}

}  // namespace

}  // namespace blink
