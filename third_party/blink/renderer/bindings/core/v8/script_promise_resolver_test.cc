// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

#include <memory>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestHelperFunction : public ScriptFunction::Callable {
 public:
  explicit TestHelperFunction(String* value) : value_(value) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    DCHECK(!value.IsEmpty());
    *value_ = ToCoreString(
        script_state->GetIsolate(),
        value.V8Value()->ToString(script_state->GetContext()).ToLocalChecked());
    return value;
  }

 private:
  String* value_;
};

class ScriptPromiseResolverBaseTest : public testing::Test {
 public:
  ScriptPromiseResolverBaseTest()
      : page_holder_(std::make_unique<DummyPageHolder>()) {}

  ~ScriptPromiseResolverBaseTest() override {
    // Execute all pending microtasks
    PerformMicrotaskCheckpoint();
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(&page_holder_->GetFrame());
  }
  ExecutionContext* GetExecutionContext() const {
    return page_holder_->GetFrame().DomWindow();
  }
  v8::Isolate* GetIsolate() const { return GetScriptState()->GetIsolate(); }

  void PerformMicrotaskCheckpoint() {
    ScriptState::Scope scope(GetScriptState());
    GetScriptState()->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
        GetIsolate());
  }
};

TEST_F(ScriptPromiseResolverBaseTest, construct) {
  ASSERT_FALSE(GetExecutionContext()->IsContextDestroyed());
  ScriptState::Scope scope(GetScriptState());
  MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(GetScriptState());
}

TEST_F(ScriptPromiseResolverBaseTest, resolve) {
  ScriptPromiseResolver<IDLString>* resolver = nullptr;
  ScriptPromise<IDLString> promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
        GetScriptState());
    promise = resolver->Promise();
  }

  String on_fulfilled, on_rejected;
  ASSERT_FALSE(promise.IsEmpty());
  {
    ScriptState::Scope scope(GetScriptState());
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
                 MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_rejected)));
  }

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);

  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);

  resolver->Resolve("hello");

  {
    ScriptState::Scope scope(GetScriptState());
    EXPECT_FALSE(resolver->Promise().IsEmpty());
  }

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);

  PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", on_fulfilled);
  EXPECT_EQ(String(), on_rejected);

  resolver->Resolve("bye");
  resolver->Reject("bye");
  PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", on_fulfilled);
  EXPECT_EQ(String(), on_rejected);
}

TEST_F(ScriptPromiseResolverBaseTest, reject) {
  ScriptPromiseResolver<IDLString>* resolver = nullptr;
  ScriptPromise<IDLString> promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
        GetScriptState());
    promise = resolver->Promise();
  }

  String on_fulfilled, on_rejected;
  ASSERT_FALSE(promise.IsEmpty());
  {
    ScriptState::Scope scope(GetScriptState());
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
                 MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_rejected)));
  }

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);

  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);

  resolver->Reject("hello");

  {
    ScriptState::Scope scope(GetScriptState());
    EXPECT_FALSE(resolver->Promise().IsEmpty());
  }

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);

  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ("hello", on_rejected);

  resolver->Resolve("bye");
  resolver->Reject("bye");
  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ("hello", on_rejected);
}

TEST_F(ScriptPromiseResolverBaseTest, stop) {
  ScriptPromiseResolver<IDLString>* resolver = nullptr;
  ScriptPromise<IDLString> promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
        GetScriptState());
    promise = resolver->Promise();
  }

  String on_fulfilled, on_rejected;
  ASSERT_FALSE(promise.IsEmpty());
  {
    ScriptState::Scope scope(GetScriptState());
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
                 MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_rejected)));
  }

  GetExecutionContext()->NotifyContextDestroyed();

  resolver->Resolve("hello");
  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);
}

TEST_F(ScriptPromiseResolverBaseTest, resolveUndefined) {
  ScriptPromiseResolver<IDLUndefined>* resolver = nullptr;
  ScriptPromise<IDLUndefined> promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        GetScriptState());
    promise = resolver->Promise();
  }

  String on_fulfilled, on_rejected;
  ASSERT_FALSE(promise.IsEmpty());
  {
    ScriptState::Scope scope(GetScriptState());
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
                 MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_rejected)));
  }

  resolver->Resolve();
  PerformMicrotaskCheckpoint();

  EXPECT_EQ("undefined", on_fulfilled);
  EXPECT_EQ(String(), on_rejected);
}

TEST_F(ScriptPromiseResolverBaseTest, rejectUndefined) {
  ScriptPromiseResolver<IDLUndefined>* resolver = nullptr;
  ScriptPromise<IDLUndefined> promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        GetScriptState());
    promise = resolver->Promise();
  }

  String on_fulfilled, on_rejected;
  ASSERT_FALSE(promise.IsEmpty());
  {
    ScriptState::Scope scope(GetScriptState());
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
                 MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_rejected)));
  }

  resolver->Reject();
  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ("undefined", on_rejected);
}

TEST_F(ScriptPromiseResolverBaseTest, OverrideScriptStateToCurrentContext) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::string base_url = "http://www.test.com/";
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8("single_iframe.html"));
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8("visible_iframe.html"));
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url + "single_iframe.html");

  LocalFrame* main_frame = web_view_impl->MainFrameImpl()->GetFrame();
  LocalFrame* iframe = To<LocalFrame>(main_frame->Tree().FirstChild());
  ScriptState* main_script_state = ToScriptStateForMainWorld(main_frame);
  ScriptState* iframe_script_state = ToScriptStateForMainWorld(iframe);

  ScriptPromiseResolver<IDLString>* resolver = nullptr;
  ScriptPromise<IDLString> promise;
  {
    ScriptState::Scope scope(main_script_state);
    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
        main_script_state);
    promise = resolver->Promise();
  }

  String on_fulfilled, on_rejected;
  ASSERT_FALSE(promise.IsEmpty());
  {
    ScriptState::Scope scope(main_script_state);
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     main_script_state,
                     MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
                 MakeGarbageCollected<ScriptFunction>(
                     main_script_state,
                     MakeGarbageCollected<TestHelperFunction>(&on_rejected)));
  }

  {
    ScriptState::Scope scope(iframe_script_state);
    iframe->DomWindow()->NotifyContextDestroyed();
    resolver->ResolveOverridingToCurrentContext("hello");
  }
  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);
}

}  // namespace

}  // namespace blink
