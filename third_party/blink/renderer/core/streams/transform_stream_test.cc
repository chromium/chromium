// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transform_stream.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/test_utils.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Mock;
using ::testing::Return;

class TransformStreamTest : public ::testing::Test {
 public:
  TransformStreamTest() = default;

  TransformStream* Stream() const { return stream_; }

  void Init(TransformStreamTransformer* transformer,
            ScriptState* script_state,
            ExceptionState& exception_state) {
    stream_ =
        TransformStream::Create(script_state, transformer, exception_state);
  }

  // This takes the |readable| and |writable| properties of the TransformStream
  // and copies them onto the global object so they can be accessed by Eval().
  void CopyReadableAndWritableToGlobal(const V8TestingScope& scope) {
    auto* script_state = scope.GetScriptState();
    ReadableStream* readable = Stream()->Readable();
    WritableStream* writable = Stream()->Writable();
    v8::Local<v8::Object> global = script_state->GetContext()->Global();
    EXPECT_TRUE(
        global
            ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "readable"),
                  ToV8Traits<ReadableStream>::ToV8(script_state, readable))
            .IsJust());
    EXPECT_TRUE(
        global
            ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "writable"),
                  ToV8Traits<WritableStream>::ToV8(script_state, writable))
            .IsJust());
  }

 private:
  test::TaskEnvironment task_environment_;
  Persistent<TransformStream> stream_;
};

// A convenient base class to make tests shorter. Subclasses need not implement
// both Transform() and Flush(), and can override the void versions to avoid the
// need to create a promise to return. Not appropriate for use in production.
class TestTransformer : public TransformStreamTransformer {
 public:
  explicit TestTransformer(ScriptState* script_state)
      : script_state_(script_state) {}

  virtual void TransformVoid(v8::Local<v8::Value>,
                             TransformStreamDefaultController*,
                             ExceptionState&) {}

  ScriptPromise<IDLUndefined> Transform(
      v8::Local<v8::Value> chunk,
      TransformStreamDefaultController* controller,
      ExceptionState& exception_state) override {
    TransformVoid(chunk, controller, exception_state);
    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  virtual void FlushVoid(TransformStreamDefaultController*, ExceptionState&) {}

  ScriptPromise<IDLUndefined> Flush(
      TransformStreamDefaultController* controller,
      ExceptionState& exception_state) override {
    FlushVoid(controller, exception_state);
    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    TransformStreamTransformer::Trace(visitor);
  }

 private:
  const Member<ScriptState> script_state_;
};

class IdentityTransformer final : public TestTransformer {
 public:
  explicit IdentityTransformer(ScriptState* script_state)
      : TestTransformer(script_state) {}

  void TransformVoid(v8::Local<v8::Value> chunk,
                     TransformStreamDefaultController* controller,
                     ExceptionState& exception_state) override {
    controller->enqueue(GetScriptState(),
                        ScriptValue(GetScriptState()->GetIsolate(), chunk),
                        exception_state);
  }
};

class MockTransformStreamTransformer : public TransformStreamTransformer {
 public:
  explicit MockTransformStreamTransformer(ScriptState* script_state)
      : script_state_(script_state) {}

  MOCK_METHOD3(Transform,
               ScriptPromise<IDLUndefined>(v8::Local<v8::Value> chunk,
                                           TransformStreamDefaultController*,
                                           ExceptionState&));
  MOCK_METHOD2(Flush,
               ScriptPromise<IDLUndefined>(TransformStreamDefaultController*,
                                           ExceptionState&));

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    TransformStreamTransformer::Trace(visitor);
  }

 private:
  const Member<ScriptState> script_state_;
};

// If this doesn't work then nothing else will.
TEST_F(TransformStreamTest, Construct) {
  V8TestingScope scope;
  Init(MakeGarbageCollected<IdentityTransformer>(scope.GetScriptState()),
       scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(Stream());
}

TEST_F(TransformStreamTest, Accessors) {
  V8TestingScope scope;
  Init(MakeGarbageCollected<IdentityTransformer>(scope.GetScriptState()),
       scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ReadableStream* readable = Stream()->Readable();
  WritableStream* writable = Stream()->Writable();
  EXPECT_TRUE(readable);
  EXPECT_TRUE(writable);
}

TEST_F(TransformStreamTest, TransformIsCalled) {
  V8TestingScope scope;
  auto* mock = MakeGarbageCollected<MockTransformStreamTransformer>(
      scope.GetScriptState());
  Init(mock, scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // Need to run microtasks so the startAlgorithm promise resolves.
  scope.PerformMicrotaskCheckpoint();
  CopyReadableAndWritableToGlobal(scope);

  EXPECT_CALL(*mock, Transform(_, _, _))
      .WillOnce(
          Return(ByMove(ToResolvedUndefinedPromise(scope.GetScriptState()))));

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
  auto* mock = MakeGarbageCollected<MockTransformStreamTransformer>(
      scope.GetScriptState());
  Init(mock, scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  // Need to run microtasks so the startAlgorithm promise resolves.
  scope.PerformMicrotaskCheckpoint();
  CopyReadableAndWritableToGlobal(scope);

  EXPECT_CALL(*mock, Flush(_, _))
      .WillOnce(
          Return(ByMove(ToResolvedUndefinedPromise(scope.GetScriptState()))));

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.close();\n");

  Mock::VerifyAndClear(mock);
  Mock::AllowLeak(mock);
}

bool IsIteratorForStringMatching(ScriptState* script_state,
                                 ScriptValue value,
                                 const String& expected) {
  if (!value.IsObject()) {
    return false;
  }
  v8::Local<v8::Value> chunk;
  bool done = false;
  if (!V8UnpackIterationResult(script_state,
                               value.V8Value()
                                   ->ToObject(script_state->GetContext())
                                   .ToLocalChecked(),
                               &chunk, &done)) {
    return false;
  }
  if (done)
    return false;
  return ToCoreStringWithUndefinedOrNullCheck(script_state->GetIsolate(),
                                              chunk) == expected;
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

TEST_F(TransformStreamTest, EnqueueFromTransform) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(MakeGarbageCollected<IdentityTransformer>(scope.GetScriptState()),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.write('a');\n");

  ReadableStream* readable = Stream()->Readable();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state,
                             reader->read(script_state, ASSERT_NO_EXCEPTION));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(IsIteratorForStringMatching(script_state, tester.Value(), "a"));
}

TEST_F(TransformStreamTest, EnqueueFromFlush) {
  class EnqueueFromFlushTransformer final : public TestTransformer {
   public:
    explicit EnqueueFromFlushTransformer(ScriptState* script_state)
        : TestTransformer(script_state) {}

    void FlushVoid(TransformStreamDefaultController* controller,
                   ExceptionState& exception_state) override {
      controller->enqueue(
          GetScriptState(),
          ScriptValue(GetScriptState()->GetIsolate(),
                      V8String(GetScriptState()->GetIsolate(), "a")),
          exception_state);
    }
  };

  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(MakeGarbageCollected<EnqueueFromFlushTransformer>(script_state),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  EvalWithPrintingError(&scope,
                        "const writer = writable.getWriter();\n"
                        "writer.close();\n");

  ReadableStream* readable = Stream()->Readable();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state,
                             reader->read(script_state, ASSERT_NO_EXCEPTION));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(IsIteratorForStringMatching(script_state, tester.Value(), "a"));
}

TEST_F(TransformStreamTest, ThrowFromTransform) {
  static constexpr char kMessage[] = "errorInTransform";
  class ThrowFromTransformTransformer final : public TestTransformer {
   public:
    explicit ThrowFromTransformTransformer(ScriptState* script_state)
        : TestTransformer(script_state) {}

    void TransformVoid(v8::Local<v8::Value>,
                       TransformStreamDefaultController*,
                       ExceptionState& exception_state) override {
      exception_state.ThrowTypeError(kMessage);
    }
  };

  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(MakeGarbageCollected<ThrowFromTransformTransformer>(
           scope.GetScriptState()),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  ScriptValue promise =
      EvalWithPrintingError(&scope,
                            "const writer = writable.getWriter();\n"
                            "writer.write('a');\n");

  ReadableStream* readable = Stream()->Readable();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(
      script_state, reader->read(script_state, ASSERT_NO_EXCEPTION));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, read_tester.Value(), kMessage));
  ScriptPromiseTester write_tester(
      script_state, ToResolvedPromise<IDLAny>(script_state, promise));
  write_tester.WaitUntilSettled();
  EXPECT_TRUE(write_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, write_tester.Value(), kMessage));
}

TEST_F(TransformStreamTest, ThrowFromFlush) {
  static constexpr char kMessage[] = "errorInFlush";
  class ThrowFromFlushTransformer final : public TestTransformer {
   public:
    explicit ThrowFromFlushTransformer(ScriptState* script_state)
        : TestTransformer(script_state) {}

    void FlushVoid(TransformStreamDefaultController*,
                   ExceptionState& exception_state) override {
      exception_state.ThrowTypeError(kMessage);
    }
  };
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  Init(MakeGarbageCollected<ThrowFromFlushTransformer>(scope.GetScriptState()),
       script_state, ASSERT_NO_EXCEPTION);

  CopyReadableAndWritableToGlobal(scope);

  ScriptValue promise =
      EvalWithPrintingError(&scope,
                            "const writer = writable.getWriter();\n"
                            "writer.close();\n");

  ReadableStream* readable = Stream()->Readable();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(
      script_state, reader->read(script_state, ASSERT_NO_EXCEPTION));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, read_tester.Value(), kMessage));
  ScriptPromiseTester write_tester(
      script_state, ToResolvedPromise<IDLAny>(script_state, promise));
  write_tester.WaitUntilSettled();
  EXPECT_TRUE(write_tester.IsRejected());
  EXPECT_TRUE(IsTypeError(script_state, write_tester.Value(), kMessage));
}

TEST_F(TransformStreamTest, CreateFromReadableWritablePair) {
  V8TestingScope scope;
  ReadableStream* readable =
      ReadableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  WritableStream* writable =
      WritableStream::Create(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  TransformStream* transform =
      MakeGarbageCollected<TransformStream>(readable, writable);
  EXPECT_EQ(readable, transform->Readable());
  EXPECT_EQ(writable, transform->Writable());
}

TEST_F(TransformStreamTest, WaitInTransform) {
  class WaitInTransformTransformer final : public TestTransformer {
   public:
    explicit WaitInTransformTransformer(ScriptState* script_state)
        : TestTransformer(script_state),
          transform_promise_resolver_(
              MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
                  script_state)) {}

    ScriptPromise<IDLUndefined> Transform(v8::Local<v8::Value>,
                                          TransformStreamDefaultController*,
                                          ExceptionState&) override {
      return transform_promise_resolver_->Promise();
    }

    void FlushVoid(TransformStreamDefaultController*,
                   ExceptionState&) override {
      flush_called_ = true;
    }

    void ResolvePromise() { transform_promise_resolver_->Resolve(); }
    bool FlushCalled() const { return flush_called_; }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(transform_promise_resolver_);
      TestTransformer::Trace(visitor);
    }

   private:
    const Member<ScriptPromiseResolver<IDLUndefined>>
        transform_promise_resolver_;
    bool flush_called_ = false;
  };

  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  auto* transformer =
      MakeGarbageCollected<WaitInTransformTransformer>(script_state);
  Init(transformer, script_state, ASSERT_NO_EXCEPTION);
  CopyReadableAndWritableToGlobal(scope);

  ScriptValue promise =
      EvalWithPrintingError(&scope,
                            "const writer = writable.getWriter();\n"
                            "const promise = writer.write('a');\n"
                            "writer.close();\n"
                            "promise;\n");
  // Need to read to relieve backpressure.
  Stream()
      ->Readable()
      ->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION)
      ->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester write_tester(
      script_state, ToResolvedPromise<IDLAny>(script_state, promise));

  // Give Transform() the opportunity to be called.
  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(write_tester.IsFulfilled());
  EXPECT_FALSE(transformer->FlushCalled());

  transformer->ResolvePromise();

  write_tester.WaitUntilSettled();
  EXPECT_TRUE(write_tester.IsFulfilled());
  EXPECT_TRUE(transformer->FlushCalled());
}

TEST_F(TransformStreamTest, WaitInFlush) {
  class WaitInFlushTransformer final : public TestTransformer {
   public:
    explicit WaitInFlushTransformer(ScriptState* script_state)
        : TestTransformer(script_state),
          flush_promise_resolver_(
              MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
                  script_state)) {}

    ScriptPromise<IDLUndefined> Flush(TransformStreamDefaultController*,
                                      ExceptionState&) override {
      return flush_promise_resolver_->Promise();
    }

    void ResolvePromise() { flush_promise_resolver_->Resolve(); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(flush_promise_resolver_);
      TestTransformer::Trace(visitor);
    }

   private:
    const Member<ScriptPromiseResolver<IDLUndefined>> flush_promise_resolver_;
  };

  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  auto* transformer =
      MakeGarbageCollected<WaitInFlushTransformer>(script_state);
  Init(transformer, script_state, ASSERT_NO_EXCEPTION);
  CopyReadableAndWritableToGlobal(scope);

  ScriptValue promise =
      EvalWithPrintingError(&scope,
                            "const writer = writable.getWriter();\n"
                            "writer.close();\n");

  // Need to read to relieve backpressure.
  Stream()
      ->Readable()
      ->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION)
      ->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester close_tester(
      script_state, ToResolvedPromise<IDLAny>(script_state, promise));

  // Give Flush() the opportunity to be called.
  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(close_tester.IsFulfilled());
  transformer->ResolvePromise();
  close_tester.WaitUntilSettled();
  EXPECT_TRUE(close_tester.IsFulfilled());
}

}  // namespace
}  // namespace blink
