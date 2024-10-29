// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/promise_all.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

String ToString(v8::Local<v8::Context> context, const ScriptValue& value) {
  return ToCoreString(context->GetIsolate(),
                      value.V8Value()->ToString(context).ToLocalChecked());
}

struct ResolveUndefined final
    : public ThenCallable<IDLUndefined, ResolveUndefined> {
 public:
  void React(ScriptState*) { react_called = true; }
  bool react_called = false;
};

struct ResolveStrings final
    : public ThenCallable<IDLSequence<IDLString>, ResolveStrings> {
 public:
  void React(ScriptState*, Vector<String> strings) {
    react_called = true;
    resolve_strings = strings;
  }
  bool react_called = false;
  Vector<String> resolve_strings;
};

struct ResolveDocuments final
    : public ThenCallable<IDLSequence<Document>, ResolveDocuments> {
 public:
  void React(ScriptState*, HeapVector<Member<Document>>) {
    react_called = true;
  }
  bool react_called = false;
};

struct Reject final : public ThenCallable<IDLAny, Reject> {
 public:
  void React(ScriptState*, ScriptValue value) {
    react_called = true;
    rejected_value = value;
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(rejected_value);
    ThenCallable<IDLAny, Reject>::Trace(visitor);
  }
  bool react_called = false;
  ScriptValue rejected_value;
};

TEST(PromiseAllTest, ResolveUndefined) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  HeapVector<ScriptPromise<IDLUndefined>> promises;
  promises.push_back(ToResolvedUndefinedPromise(script_state));
  promises.push_back(ToResolvedUndefinedPromise(script_state));

  auto promise = PromiseAll<IDLUndefined>::Create(script_state, promises);
  ASSERT_FALSE(promise.IsEmpty());

  auto* resolve = MakeGarbageCollected<ResolveUndefined>();
  auto* reject = MakeGarbageCollected<Reject>();
  promise.React(script_state, resolve, reject);

  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);
}

TEST(PromiseAllTest, ResolveStrings) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  HeapVector<ScriptPromise<IDLString>> promises;
  promises.push_back(ToResolvedPromise<IDLString>(script_state, "first"));
  promises.push_back(ToResolvedPromise<IDLString>(script_state, "second"));
  promises.push_back(ToResolvedPromise<IDLString>(script_state, "third"));

  auto promise = PromiseAll<IDLString>::Create(script_state, promises);
  ASSERT_FALSE(promise.IsEmpty());

  auto* resolve = MakeGarbageCollected<ResolveStrings>();
  auto* reject = MakeGarbageCollected<Reject>();
  promise.React(script_state, resolve, reject);

  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_TRUE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);
  EXPECT_EQ(resolve->resolve_strings[0], "first");
  EXPECT_EQ(resolve->resolve_strings[1], "second");
  EXPECT_EQ(resolve->resolve_strings[2], "third");
}

TEST(PromiseAllTest, Reject) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  HeapVector<ScriptPromise<IDLUndefined>> promises;
  promises.push_back(ToResolvedUndefinedPromise(script_state));
  promises.push_back(ScriptPromise<IDLUndefined>::Reject(
      script_state, V8String(scope.GetIsolate(), "world")));

  auto promise = PromiseAll<IDLUndefined>::Create(script_state, promises);
  ASSERT_FALSE(promise.IsEmpty());

  auto* resolve = MakeGarbageCollected<ResolveUndefined>();
  auto* reject = MakeGarbageCollected<Reject>();
  promise.React(script_state, resolve, reject);

  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve->react_called);
  EXPECT_TRUE(reject->react_called);

  EXPECT_FALSE(reject->rejected_value.IsEmpty());
  EXPECT_EQ("world", ToString(scope.GetContext(), reject->rejected_value));
}

TEST(PromiseAllTest, RejectTypeMismatch) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  HeapVector<ScriptPromise<Document>> promises;
  promises.push_back(ScriptPromise<Document>::FromV8Value(
      script_state,
      ToV8Traits<LocalDOMWindow>::ToV8(script_state, &scope.GetWindow())));

  auto promise = PromiseAll<Document>::Create(script_state, promises);
  ASSERT_FALSE(promise.IsEmpty());

  auto* resolve = MakeGarbageCollected<ResolveDocuments>();
  auto* reject = MakeGarbageCollected<Reject>();
  promise.React(script_state, resolve, reject);

  EXPECT_FALSE(resolve->react_called);
  EXPECT_FALSE(reject->react_called);

  scope.PerformMicrotaskCheckpoint();

  EXPECT_FALSE(resolve->react_called);
  EXPECT_TRUE(reject->react_called);

  EXPECT_FALSE(reject->rejected_value.IsEmpty());
  EXPECT_EQ("TypeError: Failed to convert value to 'Document'.",
            ToString(scope.GetContext(), reject->rejected_value));
}

}  // namespace

}  // namespace blink
