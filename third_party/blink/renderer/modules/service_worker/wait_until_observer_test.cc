// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(WaitUntilObserverTest, WaitUntilSucceeds) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  ScriptState* script_state = scope.GetScriptState();

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      scope.GetExecutionContext(), WaitUntilObserver::kInstall, 0);
  observer->WillDispatchEvent();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  ExceptionState exception_state(isolate);
  bool result = observer->WaitUntil(script_state, promise, exception_state);

  EXPECT_TRUE(result);
  EXPECT_FALSE(exception_state.HadException());
}

// Regression test for https://crbug.com/477424489.
// WaitUntil() calls ScriptPromise::Then() which can fail if V8 execution
// is being terminated. Before the fix, WaitUntil() ignored the failure and
// returned true, leaving a pending V8 exception that caused the caller to
// crash when constructing a new ScriptPromiseResolver.
TEST(WaitUntilObserverTest, WaitUntilReturnsFalseWhenThenFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  ScriptState* script_state = scope.GetScriptState();

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      scope.GetExecutionContext(), WaitUntilObserver::kInstall, 0);
  observer->WillDispatchEvent();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  // Simulate worker termination: causes Then() inside WaitUntil() to fail.
  isolate->TerminateExecution();

  {
    ExceptionState exception_state(isolate);
    bool result = observer->WaitUntil(script_state, promise, exception_state);

    EXPECT_FALSE(result);
    EXPECT_TRUE(exception_state.HadException());

    // ExceptionState destructor DCHECKs that the isolate still has a pending
    // exception if had_exception_ is true, so it must destruct before we
    // cancel termination.
  }

  isolate->CancelTerminateExecution();
}

}  // namespace blink
