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
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestHelperFunction : public ScriptFunction::Callable {
 public:
  explicit TestHelperFunction(String* value) : value_(value) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    DCHECK(!value.IsEmpty());
    *value_ = ToCoreString(
        value.V8Value()->ToString(script_state->GetContext()).ToLocalChecked());
    return value;
  }

 private:
  String* value_;
};

class ScriptPromiseResolverTest : public testing::Test {
 public:
  ScriptPromiseResolverTest()
      : page_holder_(std::make_unique<DummyPageHolder>()) {}

  ~ScriptPromiseResolverTest() override {
    // Execute all pending microtasks
    PerformMicrotaskCheckpoint();
  }

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

TEST_F(ScriptPromiseResolverTest, construct) {
  ASSERT_FALSE(GetExecutionContext()->IsContextDestroyed());
  ScriptState::Scope scope(GetScriptState());
  MakeGarbageCollected<ScriptPromiseResolver>(GetScriptState());
}

TEST_F(ScriptPromiseResolverTest, resolve) {
  ScriptPromiseResolver* resolver = nullptr;
  ScriptPromise promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver>(GetScriptState());
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
    EXPECT_TRUE(resolver->Promise().IsEmpty());
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

TEST_F(ScriptPromiseResolverTest, reject) {
  ScriptPromiseResolver* resolver = nullptr;
  ScriptPromise promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver>(GetScriptState());
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
    EXPECT_TRUE(resolver->Promise().IsEmpty());
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

TEST_F(ScriptPromiseResolverTest, stop) {
  ScriptPromiseResolver* resolver = nullptr;
  ScriptPromise promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver>(GetScriptState());
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
  {
    ScriptState::Scope scope(GetScriptState());
    EXPECT_TRUE(resolver->Promise().IsEmpty());
  }

  resolver->Resolve("hello");
  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);
}

class ScriptPromiseResolverKeepAlive : public ScriptPromiseResolver {
 public:
  explicit ScriptPromiseResolverKeepAlive(ScriptState* script_state)
      : ScriptPromiseResolver(script_state) {}
  ~ScriptPromiseResolverKeepAlive() override { destructor_calls_++; }

  static void Reset() { destructor_calls_ = 0; }
  static bool IsAlive() { return !destructor_calls_; }

  static int destructor_calls_;
};

int ScriptPromiseResolverKeepAlive::destructor_calls_ = 0;

TEST_F(ScriptPromiseResolverTest, keepAliveUntilResolved) {
  ScriptPromiseResolverKeepAlive::Reset();
  ScriptPromiseResolver* resolver = nullptr;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver =
        MakeGarbageCollected<ScriptPromiseResolverKeepAlive>(GetScriptState());
  }
  resolver->KeepAliveWhilePending();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  ASSERT_TRUE(ScriptPromiseResolverKeepAlive::IsAlive());

  resolver->Resolve("hello");
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_FALSE(ScriptPromiseResolverKeepAlive::IsAlive());
}

TEST_F(ScriptPromiseResolverTest, keepAliveUntilRejected) {
  ScriptPromiseResolverKeepAlive::Reset();
  ScriptPromiseResolver* resolver = nullptr;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver =
        MakeGarbageCollected<ScriptPromiseResolverKeepAlive>(GetScriptState());
  }
  resolver->KeepAliveWhilePending();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  ASSERT_TRUE(ScriptPromiseResolverKeepAlive::IsAlive());

  resolver->Reject("hello");
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_FALSE(ScriptPromiseResolverKeepAlive::IsAlive());
}

TEST_F(ScriptPromiseResolverTest, keepAliveWhileScriptForbidden) {
  ScriptPromiseResolverKeepAlive::Reset();
  ScriptPromiseResolver* resolver = nullptr;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver =
        MakeGarbageCollected<ScriptPromiseResolverKeepAlive>(GetScriptState());
  }

  {
    ScriptForbiddenScope forbidden;
    resolver->Resolve("hello");

    ThreadState::Current()->CollectAllGarbageForTesting(
        ThreadState::StackState::kNoHeapPointers);
    EXPECT_TRUE(ScriptPromiseResolverKeepAlive::IsAlive());
  }

  base::RunLoop().RunUntilIdle();

  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_FALSE(ScriptPromiseResolverKeepAlive::IsAlive());
}

TEST_F(ScriptPromiseResolverTest, keepAliveUntilStopped) {
  ScriptPromiseResolverKeepAlive::Reset();
  ScriptPromiseResolver* resolver = nullptr;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver =
        MakeGarbageCollected<ScriptPromiseResolverKeepAlive>(GetScriptState());
  }
  resolver->KeepAliveWhilePending();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_TRUE(ScriptPromiseResolverKeepAlive::IsAlive());

  GetExecutionContext()->NotifyContextDestroyed();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_FALSE(ScriptPromiseResolverKeepAlive::IsAlive());
}

TEST_F(ScriptPromiseResolverTest, suspend) {
  ScriptPromiseResolverKeepAlive::Reset();
  ScriptPromiseResolver* resolver = nullptr;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver =
        MakeGarbageCollected<ScriptPromiseResolverKeepAlive>(GetScriptState());
  }
  resolver->KeepAliveWhilePending();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  ASSERT_TRUE(ScriptPromiseResolverKeepAlive::IsAlive());

  page_holder_->GetPage().SetPaused(true);
  resolver->Resolve("hello");
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_TRUE(ScriptPromiseResolverKeepAlive::IsAlive());

  GetExecutionContext()->NotifyContextDestroyed();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_FALSE(ScriptPromiseResolverKeepAlive::IsAlive());
}

TEST_F(ScriptPromiseResolverTest, resolveVoid) {
  ScriptPromiseResolver* resolver = nullptr;
  ScriptPromise promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver>(GetScriptState());
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

TEST_F(ScriptPromiseResolverTest, rejectVoid) {
  ScriptPromiseResolver* resolver = nullptr;
  ScriptPromise promise;
  {
    ScriptState::Scope scope(GetScriptState());
    resolver = MakeGarbageCollected<ScriptPromiseResolver>(GetScriptState());
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

}  // namespace

}  // namespace blink
