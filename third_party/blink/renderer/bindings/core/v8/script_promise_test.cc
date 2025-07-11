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
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

String ToString(v8::Local<v8::Context> context, const ScriptValue& value) {
  return ToCoreString(v8::Isolate::GetCurrent(),
                      value.V8Value()->ToString(context).ToLocalChecked());
}

struct ResolveString final : public ThenCallable<IDLString, ResolveString> {
 public:
  void React(ScriptState*, String value) {
    react_called = true;
    resolve_string = value;
  }
  bool react_called = false;
  String resolve_string;
};

struct AnyCallable final : public ThenCallable<IDLAny, AnyCallable> {
 public:
  void React(ScriptState*, ScriptValue value) {
    react_called = true;
    react_value = value;
  }
  void Trace(Visitor* visitor) const final {
    visitor->Trace(react_value);
    ThenCallable<IDLAny, AnyCallable>::Trace(visitor);
  }
  bool react_called = false;
  ScriptValue react_value;
};

struct AnyChainingCallable final
    : public ThenCallable<IDLAny, AnyChainingCallable, IDLAny> {
 public:
  ScriptValue React(ScriptState*, ScriptValue value) {
    react_called = true;
    react_value = value;
    return value;
  }
  void Trace(Visitor* visitor) const final {
    visitor->Trace(react_value);
    ThenCallable<IDLAny, AnyChainingCallable, IDLAny>::Trace(visitor);
  }
  bool react_called = false;
  ScriptValue react_value;
};

struct ThrowingCallable final
    : public ThenCallable<IDLAny, ThrowingCallable, IDLAny> {
 public:
  ScriptValue React(ScriptState* script_state, ScriptValue value) {
    v8::Isolate* isolate = script_state->GetIsolate();
    isolate->ThrowException(v8::Undefined(isolate));
    return value;
  }
};

struct ResolveDocument final : public ThenCallable<Document, ResolveDocument> {
 public:
  void React(ScriptState*, Document*) { react_called = true; }
  bool react_called = false;
};

struct ConvertAnyToStringCallable
    : public ThenCallable<IDLAny, ConvertAnyToStringCallable, IDLString> {
 public:
  String React(ScriptState* script_state, ScriptValue value) {
    react_called = true;
    return ToString(script_state->GetContext(), value);
  }
  bool react_called = false;
};

TEST(ScriptPromiseTest, ThenResolve) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  auto* resolve = MakeGarbageCollected<ResolveString>();
  auto* reject = MakeGarbageCollected<AnyCallable>();
  promise.Then(scope.GetScriptState(), resolve, reject);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();
  resolver->Resolve("hello");

  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);
  EXPECT_EQ("hello", resolve->resolve_string);
}

TEST(ScriptPromiseTest, ThenOnAlreadyResolvedPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto promise = ToResolvedPromise<IDLString>(scope.GetScriptState(), "hello");
  auto* resolve = MakeGarbageCollected<ResolveString>();
  auto* reject = MakeGarbageCollected<AnyCallable>();
  promise.Then(scope.GetScriptState(), resolve, reject);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);
  EXPECT_EQ("hello", resolve->resolve_string);
}

TEST(ScriptPromiseTest, ThenReject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();
  auto* resolve = MakeGarbageCollected<ResolveString>();
  auto* reject = MakeGarbageCollected<AnyCallable>();
  promise.Then(scope.GetScriptState(), resolve, reject);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();
  resolver->Reject(V8String(scope.GetIsolate(), "hello"));

  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve->react_called);
  EXPECT_TRUE(reject->react_called);
  EXPECT_EQ("hello", ToString(scope.GetContext(), reject->react_value));
}

TEST(ScriptPromiseTest, ThrowingOnFulfilled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();

  auto* throwing = MakeGarbageCollected<ThrowingCallable>();
  auto* resolve2 = MakeGarbageCollected<AnyCallable>();
  auto* reject1 = MakeGarbageCollected<AnyChainingCallable>();
  auto* reject2 = MakeGarbageCollected<AnyCallable>();
  auto promise2 = promise.Then(scope.GetScriptState(), throwing, reject1);
  promise2.Then(scope.GetScriptState(), resolve2, reject2);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve2->react_called);
  EXPECT_FALSE(reject1->react_called);
  EXPECT_FALSE(reject2->react_called);

  scope.PerformMicrotaskCheckpoint();
  resolver->Resolve(v8::Null(scope.GetIsolate()));

  EXPECT_FALSE(resolve2->react_called);
  EXPECT_FALSE(reject1->react_called);
  EXPECT_FALSE(reject2->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve2->react_called);
  EXPECT_FALSE(reject1->react_called);
  EXPECT_TRUE(reject2->react_called);
}

TEST(ScriptPromiseTest, ThrowingOnRejected) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();

  auto* throwing = MakeGarbageCollected<ThrowingCallable>();
  auto* resolve1 = MakeGarbageCollected<AnyChainingCallable>();
  auto* resolve2 = MakeGarbageCollected<AnyCallable>();
  auto* reject2 = MakeGarbageCollected<AnyCallable>();
  auto promise2 = promise.Then(scope.GetScriptState(), resolve1, throwing);
  promise2.Then(scope.GetScriptState(), resolve2, reject2);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve1->react_called);
  EXPECT_FALSE(resolve2->react_called);
  EXPECT_FALSE(reject2->react_called);

  scope.PerformMicrotaskCheckpoint();
  resolver->Reject(V8String(scope.GetIsolate(), "hello"));

  EXPECT_FALSE(resolve1->react_called);
  EXPECT_FALSE(resolve2->react_called);
  EXPECT_FALSE(reject2->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve1->react_called);
  EXPECT_FALSE(resolve2->react_called);
  EXPECT_TRUE(reject2->react_called);
}

TEST(ScriptPromiseTest, ThenOnAlreadyRejectedPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto promise = ScriptPromise<IDLString>::Reject(
      scope.GetScriptState(), V8String(scope.GetIsolate(), "hello"));
  auto* resolve = MakeGarbageCollected<ResolveString>();
  auto* reject = MakeGarbageCollected<AnyCallable>();
  promise.Then(scope.GetScriptState(), resolve, reject);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve->react_called);
  EXPECT_TRUE(reject->react_called);
  EXPECT_EQ("hello", ToString(scope.GetContext(), reject->react_value));
}

TEST(ScriptPromiseTest, CastPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto resolver = v8::Promise::Resolver::New(scope.GetContext());
  v8::Local<v8::Promise> promise = resolver.ToLocalChecked()->GetPromise();
  auto new_promise =
      ScriptPromise<IDLAny>::FromV8Promise(scope.GetIsolate(), promise);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_EQ(promise, new_promise.V8Promise());
}

TEST(ScriptPromiseTest, CastNonPromise) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScriptValue value =
      ScriptValue(scope.GetIsolate(), V8String(scope.GetIsolate(), "hello"));
  ScriptPromise<IDLAny> promise1 =
      ToResolvedPromise<IDLAny>(scope.GetScriptState(), value);
  ScriptPromise<IDLAny> promise2 =
      ToResolvedPromise<IDLAny>(scope.GetScriptState(), value);
  auto* resolve1 = MakeGarbageCollected<AnyChainingCallable>();
  auto* reject1 = MakeGarbageCollected<AnyChainingCallable>();
  promise1.Then(scope.GetScriptState(), resolve1, reject1);
  auto* resolve2 = MakeGarbageCollected<AnyCallable>();
  auto* reject2 = MakeGarbageCollected<AnyCallable>();
  promise2.Then(scope.GetScriptState(), resolve2, reject2);

  ASSERT_FALSE(promise1.IsEmpty());
  ASSERT_FALSE(promise2.IsEmpty());
  EXPECT_NE(promise1.V8Promise(), promise2.V8Promise());

  EXPECT_FALSE(resolve1->react_called);
  EXPECT_FALSE(reject1->react_called);
  EXPECT_FALSE(resolve2->react_called);
  EXPECT_FALSE(reject2->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(resolve1->react_called);
  EXPECT_FALSE(reject1->react_called);
  EXPECT_TRUE(resolve2->react_called);
  EXPECT_FALSE(reject2->react_called);
  EXPECT_EQ("hello", ToString(scope.GetContext(), resolve1->react_value));
  EXPECT_EQ("hello", ToString(scope.GetContext(), resolve2->react_value));
}

TEST(ScriptPromiseTest, Reject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScriptValue value =
      ScriptValue(scope.GetIsolate(), V8String(scope.GetIsolate(), "hello"));
  auto promise = ScriptPromise<IDLString>::Reject(scope.GetScriptState(),
                                                  ScriptValue(value));
  auto* resolve = MakeGarbageCollected<ResolveString>();
  auto* reject = MakeGarbageCollected<AnyCallable>();
  promise.Then(scope.GetScriptState(), resolve, reject);

  ASSERT_FALSE(promise.IsEmpty());

  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve->react_called);
  EXPECT_TRUE(reject->react_called);
  EXPECT_EQ("hello", ToString(scope.GetContext(), reject->react_value));
}

TEST(ScriptPromiseTest, RejectWithDOMException) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto promise = ScriptPromise<IDLString>::RejectWithDOMException(
      scope.GetScriptState(),
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kSyntaxError,
                                         "some syntax error"));
  auto* resolve = MakeGarbageCollected<ResolveString>();
  auto* reject = MakeGarbageCollected<AnyCallable>();
  promise.Then(scope.GetScriptState(), resolve, reject);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve->react_called);
  EXPECT_TRUE(reject->react_called);
  EXPECT_EQ("SyntaxError: some syntax error",
            ToString(scope.GetContext(), reject->react_value));
}

TEST(ScriptPromiseTest, RejectTypeMismatch) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto promise = ScriptPromise<Document>::FromV8Value(
      script_state,
      ToV8Traits<LocalDOMWindow>::ToV8(script_state, &scope.GetWindow()));

  auto* resolve = MakeGarbageCollected<ResolveDocument>();
  auto* reject = MakeGarbageCollected<AnyCallable>();
  promise.Then(scope.GetScriptState(), resolve, reject);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve->react_called);
  EXPECT_TRUE(reject->react_called);

  EXPECT_FALSE(reject->react_value.IsEmpty());
  EXPECT_EQ("TypeError: Failed to convert value to 'Document'.",
            ToString(scope.GetContext(), reject->react_value));
}

TEST(ScriptPromiseTest, ChainPromisesWithDifferentResolveTypes) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      scope.GetScriptState());
  auto promise = resolver->Promise();

  auto* resolve1 = MakeGarbageCollected<ConvertAnyToStringCallable>();
  auto* reject1 = MakeGarbageCollected<AnyChainingCallable>();
  auto promise2 =
      promise.Then<IDLString>(scope.GetScriptState(), resolve1, reject1);

  auto* resolve2 = MakeGarbageCollected<ResolveString>();
  auto* reject2 = MakeGarbageCollected<AnyCallable>();
  promise2.Then(scope.GetScriptState(), resolve2, reject2);

  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_FALSE(resolve1->react_called);
  EXPECT_FALSE(reject1->react_called);
  EXPECT_FALSE(resolve2->react_called);
  EXPECT_FALSE(reject2->react_called);

  scope.PerformMicrotaskCheckpoint();
  resolver->Resolve(V8String(scope.GetIsolate(), "hello"));

  EXPECT_FALSE(resolve1->react_called);
  EXPECT_FALSE(reject1->react_called);
  EXPECT_FALSE(resolve2->react_called);
  EXPECT_FALSE(reject2->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(resolve1->react_called);
  EXPECT_FALSE(reject1->react_called);
  EXPECT_TRUE(resolve2->react_called);
  EXPECT_FALSE(reject2->react_called);
  EXPECT_EQ("hello", resolve2->resolve_string);
}

}  // namespace

}  // namespace blink
