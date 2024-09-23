/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

template <typename T, typename... Args>
ScriptFunction* CreateFunction(ScriptState* script_state, Args&&... args) {
  return MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<T>(std::forward<Args>(args)...));
}

class FunctionForScriptPromiseTest : public ScriptFunction::Callable {
 public:
  explicit FunctionForScriptPromiseTest(ScriptValue* output)
      : output_(output) {}

  ScriptValue Call(ScriptState*, ScriptValue value) override {
    DCHECK(!value.IsEmpty());
    *output_ = value;
    return value;
  }

 private:
  ScriptValue* output_;
};

class ThrowingCallable : public ScriptFunction::Callable {
 public:
 private:
  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    v8::Isolate* isolate = script_state->GetIsolate();
    isolate->ThrowException(v8::Undefined(isolate));
    return ScriptValue();
  }
};

class NotReached : public ScriptFunction::Callable {
  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    ADD_FAILURE() << "This function should not be called.";
    return ScriptValue();
  }
};

class ScriptValueHolder final : public GarbageCollected<ScriptValueHolder> {
 public:
  const ScriptValue& Value() const { return value_; }
  void SetValue(const ScriptValue& value) { value_ = value; }
  void Trace(Visitor* visitor) const { visitor->Trace(value_); }

 private:
  ScriptValue value_;
};

class CapturingCallable final : public ScriptFunction::Callable {
 public:
  explicit CapturingCallable(ScriptValueHolder* holder) : holder_(holder) {}
  ScriptValue Call(ScriptState*, ScriptValue value) override {
    holder_->SetValue(value);
    return value;
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(holder_);
    Callable::Trace(visitor);
  }

 private:
  Member<ScriptValueHolder> holder_;
};

String ToString(v8::Local<v8::Context> context, const ScriptValue& value) {
  return ToCoreString(context->GetIsolate(),
                      value.V8Value()->ToString(context).ToLocalChecked());
}

Vector<String> ToStringArray(v8::Isolate* isolate, const ScriptValue& value) {
  NonThrowableExceptionState exception_state;
  return NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
      isolate, value.V8Value(), exception_state);
}

TEST(ScriptPromiseTest, ThenResolve) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  ScriptValue on_fulfilled, on_rejected;
  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();
  resolver->Resolve("hello");

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled));
  EXPECT_TRUE(on_rejected.IsEmpty());
}

TEST(ScriptPromiseTest, ThenResolveScriptFunction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  auto* const on_fulfilled = MakeGarbageCollected<ScriptValueHolder>();
  promise.Then(
      CreateFunction<CapturingCallable>(scope.GetScriptState(), on_fulfilled),
      CreateFunction<NotReached>(scope.GetScriptState()));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();
  resolver->Resolve("hello");

  EXPECT_TRUE(on_fulfilled->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled->Value()));
}

TEST(ScriptPromiseTest, ResolveThen) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto promise = ToResolvedPromise<IDLString>(scope.GetScriptState(), "hello");
  ScriptValue on_fulfilled, on_rejected;
  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled));
  EXPECT_TRUE(on_rejected.IsEmpty());
}

TEST(ScriptPromiseTest, ResolveThenScriptFunction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto promise = ToResolvedPromise<IDLString>(scope.GetScriptState(), "hello");
  auto* const on_fulfilled = MakeGarbageCollected<ScriptValueHolder>();
  promise.Then(
      CreateFunction<CapturingCallable>(scope.GetScriptState(), on_fulfilled),
      CreateFunction<NotReached>(scope.GetScriptState()));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled->Value()));
}

TEST(ScriptPromiseTest, ThenReject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  ScriptValue on_fulfilled, on_rejected;
  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();
  resolver->Reject(V8String(scope.GetIsolate(), "hello"));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_EQ("hello", ToString(scope.GetContext(), on_rejected));
}

TEST(ScriptPromiseTest, ThenRejectScriptFunction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  auto* const on_rejected = MakeGarbageCollected<ScriptValueHolder>();
  promise.Then(
      CreateFunction<NotReached>(scope.GetScriptState()),
      CreateFunction<CapturingCallable>(scope.GetScriptState(), on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_rejected->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();
  resolver->Reject(V8String(scope.GetIsolate(), "hello"));

  EXPECT_TRUE(on_rejected->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_rejected->Value()));
}

TEST(ScriptPromiseTest, ThrowingOnFulfilled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  ScriptValue on_rejected, on_fulfilled2, on_rejected2;

  auto promise2 =
      promise.Then(CreateFunction<ThrowingCallable>(scope.GetScriptState()),
                   CreateFunction<FunctionForScriptPromiseTest>(
                       scope.GetScriptState(), &on_rejected));
  promise2.Then(CreateFunction<FunctionForScriptPromiseTest>(
                    scope.GetScriptState(), &on_fulfilled2),
                CreateFunction<FunctionForScriptPromiseTest>(
                    scope.GetScriptState(), &on_rejected2));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());
  EXPECT_TRUE(on_fulfilled2.IsEmpty());
  EXPECT_TRUE(on_rejected2.IsEmpty());

  scope.PerformMicrotaskCheckpoint();
  resolver->Resolve("hello");

  EXPECT_TRUE(on_rejected.IsEmpty());
  EXPECT_TRUE(on_fulfilled2.IsEmpty());
  EXPECT_TRUE(on_rejected2.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(on_rejected.IsEmpty());
  EXPECT_TRUE(on_fulfilled2.IsEmpty());
  EXPECT_FALSE(on_rejected2.IsEmpty());
}

TEST(ScriptPromiseTest, ThrowingOnFulfilledScriptFunction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  auto* const on_rejected = MakeGarbageCollected<ScriptValueHolder>();

  auto promise2 =
      promise.Then(CreateFunction<ThrowingCallable>(scope.GetScriptState()),
                   CreateFunction<NotReached>(scope.GetScriptState()));
  promise2.Then(
      CreateFunction<NotReached>(scope.GetScriptState()),
      CreateFunction<CapturingCallable>(scope.GetScriptState(), on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_rejected->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();
  resolver->Resolve("hello");

  EXPECT_TRUE(on_rejected->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(on_rejected->Value().IsEmpty());
}

TEST(ScriptPromiseTest, ThrowingOnRejected) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  ScriptValue on_fulfilled, on_fulfilled2, on_rejected2;

  auto promise2 =
      promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                       scope.GetScriptState(), &on_fulfilled2),
                   CreateFunction<ThrowingCallable>(scope.GetScriptState()));
  promise2.Then(CreateFunction<FunctionForScriptPromiseTest>(
                    scope.GetScriptState(), &on_fulfilled2),
                CreateFunction<FunctionForScriptPromiseTest>(
                    scope.GetScriptState(), &on_rejected2));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_fulfilled2.IsEmpty());
  EXPECT_TRUE(on_rejected2.IsEmpty());

  scope.PerformMicrotaskCheckpoint();
  resolver->Reject(V8String(scope.GetIsolate(), "hello"));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_fulfilled2.IsEmpty());
  EXPECT_TRUE(on_rejected2.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_fulfilled2.IsEmpty());
  EXPECT_FALSE(on_rejected2.IsEmpty());
}

TEST(ScriptPromiseTest, ThrowingOnRejectedScriptFunction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  auto* const on_rejected = MakeGarbageCollected<ScriptValueHolder>();

  auto promise2 =
      promise.Then(CreateFunction<NotReached>(scope.GetScriptState()),
                   CreateFunction<ThrowingCallable>(scope.GetScriptState()));
  promise2.Then(
      CreateFunction<NotReached>(scope.GetScriptState()),
      CreateFunction<CapturingCallable>(scope.GetScriptState(), on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_rejected->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();
  resolver->Reject(V8String(scope.GetIsolate(), "hello"));

  EXPECT_TRUE(on_rejected->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(on_rejected->Value().IsEmpty());
}

TEST(ScriptPromiseTest, RejectThen) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue on_fulfilled, on_rejected;
  auto promise = ScriptPromise<IDLUndefined>::Reject(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "hello"));
  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_EQ("hello", ToString(scope.GetContext(), on_rejected));
}

TEST(ScriptPromiseTest, RejectThenScriptFunction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* const on_rejected = MakeGarbageCollected<ScriptValueHolder>();
  auto promise = ScriptPromise<IDLUndefined>::Reject(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "hello"));
  promise.Then(
      CreateFunction<NotReached>(scope.GetScriptState()),
      CreateFunction<CapturingCallable>(scope.GetScriptState(), on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_rejected->Value().IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_rejected->Value()));
}

TEST(ScriptPromiseTest, CastPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto resolver = v8::Promise::Resolver::New(scope.GetContext());
  v8::Local<v8::Promise> promise = resolver.ToLocalChecked()->GetPromise();
  auto new_promise =
      ScriptPromise<IDLAny>::FromV8Promise(scope.GetIsolate(), promise);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_EQ(promise, new_promise.V8Value());
}

TEST(ScriptPromiseTest, CastNonPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue on_fulfilled1, on_fulfilled2, on_rejected1, on_rejected2;

  ScriptValue value =
      ScriptValue(scope.GetIsolate(), V8String(scope.GetIsolate(), "hello"));
  ScriptPromise<IDLAny> promise1 =
      ToResolvedPromise<IDLAny>(scope.GetScriptState(), value);
  ScriptPromise<IDLAny> promise2 =
      ToResolvedPromise<IDLAny>(scope.GetScriptState(), value);
  promise1.Then(CreateFunction<FunctionForScriptPromiseTest>(
                    scope.GetScriptState(), &on_fulfilled1),
                CreateFunction<FunctionForScriptPromiseTest>(
                    scope.GetScriptState(), &on_rejected1));
  promise2.Then(CreateFunction<FunctionForScriptPromiseTest>(
                    scope.GetScriptState(), &on_fulfilled2),
                CreateFunction<FunctionForScriptPromiseTest>(
                    scope.GetScriptState(), &on_rejected2));

  ASSERT_FALSE(promise1.IsEmpty());
  ASSERT_FALSE(promise2.IsEmpty());
  EXPECT_NE(promise1.V8Value(), promise2.V8Value());

  ASSERT_TRUE(promise1.V8Value()->IsPromise());
  ASSERT_TRUE(promise2.V8Value()->IsPromise());

  EXPECT_TRUE(on_fulfilled1.IsEmpty());
  EXPECT_TRUE(on_fulfilled2.IsEmpty());
  EXPECT_TRUE(on_rejected1.IsEmpty());
  EXPECT_TRUE(on_rejected2.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled1));
  EXPECT_EQ("hello", ToString(scope.GetContext(), on_fulfilled2));
  EXPECT_TRUE(on_rejected1.IsEmpty());
  EXPECT_TRUE(on_rejected2.IsEmpty());
}

TEST(ScriptPromiseTest, Reject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue on_fulfilled, on_rejected;

  ScriptValue value =
      ScriptValue(scope.GetIsolate(), V8String(scope.GetIsolate(), "hello"));
  auto promise = ScriptPromise<IDLUndefined>::Reject(scope.GetScriptState(),
                                                     ScriptValue(value));
  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  ASSERT_TRUE(promise.V8Value()->IsPromise());

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_EQ("hello", ToString(scope.GetContext(), on_rejected));
}

TEST(ScriptPromiseTest, RejectWithExceptionState) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue on_fulfilled, on_rejected;
  auto promise = ScriptPromise<IDLUndefined>::RejectWithDOMException(
      scope.GetScriptState(),
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                         "some syntax error"));
  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_EQ("SyntaxError: some syntax error",
            ToString(scope.GetContext(), on_rejected));
}

TEST(ScriptPromiseTest, AllWithEmptyPromises) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue on_fulfilled, on_rejected;

  ScriptPromiseUntyped promise = ScriptPromiseUntyped::All(
      scope.GetScriptState(), HeapVector<ScriptPromiseUntyped>());
  ASSERT_FALSE(promise.IsEmpty());

  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(ToStringArray(scope.GetIsolate(), on_fulfilled).empty());
  EXPECT_TRUE(on_rejected.IsEmpty());
}

TEST(ScriptPromiseTest, AllWithResolvedPromises) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue on_fulfilled, on_rejected;

  HeapVector<ScriptPromiseUntyped> promises;
  promises.push_back(ToResolvedPromise<IDLAny>(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "hello")));
  promises.push_back(ToResolvedPromise<IDLAny>(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "world")));

  ScriptPromiseUntyped promise =
      ScriptPromiseUntyped::All(scope.GetScriptState(), promises);
  ASSERT_FALSE(promise.IsEmpty());
  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(on_fulfilled.IsEmpty());
  Vector<String> values = ToStringArray(scope.GetIsolate(), on_fulfilled);
  EXPECT_EQ(2u, values.size());
  EXPECT_EQ("hello", values[0]);
  EXPECT_EQ("world", values[1]);
  EXPECT_TRUE(on_rejected.IsEmpty());
}

TEST(ScriptPromiseTest, AllWithRejectedPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptValue on_fulfilled, on_rejected;

  HeapVector<ScriptPromiseUntyped> promises;
  promises.push_back(ToResolvedPromise<IDLAny>(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "hello")));
  promises.push_back(ScriptPromise<IDLAny>::Reject(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "world")));

  ScriptPromiseUntyped promise =
      ScriptPromiseUntyped::All(scope.GetScriptState(), promises);
  ASSERT_FALSE(promise.IsEmpty());
  promise.Then(CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_fulfilled),
               CreateFunction<FunctionForScriptPromiseTest>(
                   scope.GetScriptState(), &on_rejected));

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_TRUE(on_rejected.IsEmpty());

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(on_fulfilled.IsEmpty());
  EXPECT_FALSE(on_rejected.IsEmpty());
  EXPECT_EQ("world", ToString(scope.GetContext(), on_rejected));
}

}  // namespace

}  // namespace blink
