// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"

#include <tuple>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_lock_options.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

// Counts how many times javascript event `event_name` is dispatched.
class EventCountingListener final : public NativeEventListener {
 public:
  EventCountingListener(Document& document, const AtomicString& event_name) {
    document.addEventListener(event_name, this);
  }

  void Invoke(ExecutionContext*, Event*) override { ++count_; }
  int count() const { return count_; }

 private:
  int count_ = 0;
};

}  // namespace

class PointerLockControllerTest : public SimTest {
 public:
  PointerLockControllerTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    SimTest::SetUp();
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(
        "<html><body><div id='target'>Target</div></body></html>");
  }

  void TearDown() override {
    PointerLockController().recent_lock_request_timestamps_.clear();
    SimTest::TearDown();
  }

  blink::PointerLockController& PointerLockController() {
    return GetDocument().GetPage()->GetPointerLockController();
  }

  size_t GetMaxLocksInWindow() {
    return PointerLockController::kMaxLocksInWindow;
  }

  base::TimeDelta GetLockRateLimitWindow() {
    return PointerLockController::kLockRateLimitWindow;
  }

  Element* GetTargetElement() {
    return GetDocument().getElementById(AtomicString("target"));
  }

  // Simulates receiving a response from the browser process to a previous
  // pointer lock request.
  void SimulateLockRequestResult(mojom::blink::PointerLockResult result) {
    PointerLockController().ProcessResult(base::DoNothing(), false, result);
  }

  // Drives the controller into the "locked" steady state on `target` without
  // tearing the lock back down. This function should be called when conditions
  // would allow the controller to send the lock request to the browser process.
  // Note: `mouse_lock_context_` is not bound by this helper because the real
  // mojo plumbing is bypassed, so `IsPointerLocked()` (which checks the remote
  // binding) will return false. Callers should assert via `GetElement()` /
  // `PointerLockElement()` instead.
  void SimulateAcquirePointerLock(ScriptState* script_state, Element* target) {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    auto promise = resolver->Promise();
    PointerLockController().RequestPointerLock(resolver, target, nullptr);
    // The promise should be pending (waiting for browser response) after a
    // successful request.
    EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());
    SimulateLockRequestResult(mojom::blink::PointerLockResult::kSuccess);
    PointerLockController().DidAcquirePointerLock();
  }

  void SimulateAcquireAndReleasePointerLock(ScriptState* script_state,
                                            Element* target) {
    SimulateAcquirePointerLock(script_state, target);
    PointerLockController().ExitPointerLock();
  }

  wtf_size_t GetRecentLockCount() {
    return PointerLockController().recent_lock_request_timestamps_.size();
  }

  // Loads a page containing a sandboxed iframe with the given `sandbox_tokens`
  // and returns the child document, target element, and script state.
  std::tuple<Document*, Element*, ScriptState*> SetUpSandboxedIframe(
      const String& sandbox_tokens) {
    SimRequest main_resource("https://sandbox.example.com/", "text/html");
    LoadURL("https://sandbox.example.com/");
    main_resource.Complete("<iframe id='sandboxed' sandbox='" + sandbox_tokens +
                           "' srcdoc='<div id=target></div>'></iframe>");
    test::RunPendingTasks();

    auto* iframe = To<HTMLIFrameElement>(
        GetDocument().QuerySelector(AtomicString("#sandboxed")));
    Document* child_doc = iframe->contentDocument();
    Element* target = child_doc->getElementById(AtomicString("target"));
    ScriptState* script_state =
        ToScriptStateForMainWorld(child_doc->GetFrame());
    return std::make_tuple(child_doc, target, script_state);
  }

  // Requests pointer lock on `target` and asserts the promise is rejected
  // with a DOMException whose name matches `expected_exception_name`.
  void RequestPointerLockAndExpectRejection(
      ScriptState* script_state,
      Element* target,
      const String& expected_exception_name) {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    auto promise = resolver->Promise();
    PointerLockController().RequestPointerLock(resolver, target, nullptr);

    EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());
    DOMException* dom_exception = V8DOMException::ToWrappable(
        script_state->GetIsolate(), promise.V8Promise()->Result());
    EXPECT_THAT(dom_exception, ::testing::NotNull());
    EXPECT_EQ(expected_exception_name, dom_exception->name());
  }

 private:
  ScopedRateLimitPointerLockRequestsForTest enable_rate_limiting_{true};
};

TEST_F(PointerLockControllerTest, SuccessfulLockAddsToRecentTimestamps) {
  EXPECT_EQ(0, GetRecentLockCount());

  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);
  SimulateAcquireAndReleasePointerLock(script_state, target);
  EXPECT_EQ(1, GetRecentLockCount());

  SimulateAcquireAndReleasePointerLock(script_state, target);
  EXPECT_EQ(2, GetRecentLockCount());
}

// Simulate enough locks to exceed the threshold and assert that the pointer
// lock request is rejected due to the rate limit.
TEST_F(PointerLockControllerTest, RequestRejectedAfterThresholdExceeded) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  // Simulate locks to reach the threshold.
  for (size_t i = 0; i < GetMaxLocksInWindow(); ++i) {
    SimulateAcquireAndReleasePointerLock(script_state, target);
  }
  EXPECT_EQ(static_cast<int>(GetMaxLocksInWindow()), GetRecentLockCount());

  // Request pointer lock. It should be rejected due to rate limiting.
  RequestPointerLockAndExpectRejection(script_state, target, "NotAllowedError");
}

// Simulate enough locks to exceed the threshold, then advance time past the
// rate limit window and verify that a new pointer lock request is accepted.
TEST_F(PointerLockControllerTest, RequestAcceptedAfterRateLimitWindowPasses) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  // Simulate locks to reach the threshold.
  for (size_t i = 0; i < GetMaxLocksInWindow(); ++i) {
    SimulateAcquireAndReleasePointerLock(script_state, target);
  }
  EXPECT_EQ(static_cast<int>(GetMaxLocksInWindow()), GetRecentLockCount());

  // Request pointer lock. It should be rejected due to rate limiting since
  // we're still within the rate limit window.
  {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    PointerLockController().RequestPointerLock(resolver, target, nullptr);
    EXPECT_EQ(v8::Promise::kRejected, resolver->Promise().V8Promise()->State());
  }

  // Advance time halfway through the window. Request should still be
  // rejected.
  task_environment().FastForwardBy(GetLockRateLimitWindow() / 2);
  {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    PointerLockController().RequestPointerLock(resolver, target, nullptr);
    EXPECT_EQ(v8::Promise::kRejected, resolver->Promise().V8Promise()->State());
    // Counter is not reset yet since we're still within the window.
    EXPECT_LE(static_cast<int>(GetMaxLocksInWindow()), GetRecentLockCount());
  }

  // Advance time past the rate limit window. The queue should be flushed and
  // the request should be accepted (pending browser response).
  task_environment().FastForwardBy(GetLockRateLimitWindow() / 2 +
                                   base::Milliseconds(10));
  {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    auto promise = resolver->Promise();
    PointerLockController().RequestPointerLock(resolver, target, nullptr);
    // The promise should be pending (waiting for browser response) after the
    // counter was reset.
    EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());
    // Counter resets when the window passes. The only timestamp is the one
    // from this request.
    EXPECT_EQ(1, GetRecentLockCount());
  }
}

// Verify that a steady cadence of requests that stays below the threshold
// does not cause the rate limit blocking to trigger.
TEST_F(PointerLockControllerTest, SteadyCadenceDoesNotBlockRequests) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  // Request `GetMaxLocksInWindow()`+1 locks in exactly
  // `GetLockRateLimitWindow()`. When the last request arrives, the first
  // timestamp should expire and the request should be successful.
  const base::TimeDelta time_interval =
      GetLockRateLimitWindow() / (GetMaxLocksInWindow() - 1);
  for (size_t i = 0; i <= GetMaxLocksInWindow(); ++i) {
    SimulateAcquireAndReleasePointerLock(script_state, target);
    task_environment().FastForwardBy(time_interval);
    if (i < GetMaxLocksInWindow() - 1) {
      EXPECT_EQ(i + 1, GetRecentLockCount());
    }
  }
  // First timestamp expired, and the last one made it into the queue.
  EXPECT_EQ(static_cast<int>(GetMaxLocksInWindow()), GetRecentLockCount());
  // Now, repeat requests at the exact interval and ensure that this cycle of
  // expiring the oldest stamp and accepting the newest repeats.
  // This emulates the scenario where a user may be working on a page that
  // requires short, fine and repeated motions during a period of time longer
  // than `kLockRateLimitWindow`.
  for (size_t i = 0; i < 5; ++i) {
    SimulateAcquireAndReleasePointerLock(script_state, target);
    task_environment().FastForwardBy(time_interval);
    EXPECT_EQ(static_cast<int>(GetMaxLocksInWindow()), GetRecentLockCount());
  }
}

// Verify that `ExitPointerLock()` releases the lock and fires a
// `pointerLockChanged` event.
TEST_F(PointerLockControllerTest,
       ExitPointerLockReleasesLockAndFiresChangeEvent) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  SimulateAcquirePointerLock(script_state, target);
  ASSERT_EQ(target, PointerLockController().GetElement());
  ASSERT_FALSE(PointerLockController().LockPending());
  ASSERT_EQ(target, GetDocument().PointerLockElement());

  // Register a listener after the initial `pointerlockchange` from acquiring
  // the lock has already been queued, so we only count the unlock event.
  test::RunPendingTasks();
  auto* change_listener = MakeGarbageCollected<EventCountingListener>(
      GetDocument(), event_type_names::kPointerlockchange);

  PointerLockController().ExitPointerLock();
  test::RunPendingTasks();

  EXPECT_EQ(nullptr, PointerLockController().GetElement());
  EXPECT_FALSE(PointerLockController().LockPending());
  EXPECT_EQ(nullptr, GetDocument().PointerLockElement());
  EXPECT_EQ(1, change_listener->count());
}

// Verifies that the pointer lock is released if the locked element is removed
// from its document.
TEST_F(PointerLockControllerTest, ElementRemovedFromDomReleasesLock) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  SimulateAcquirePointerLock(script_state, target);
  ASSERT_EQ(target, PointerLockController().GetElement());

  test::RunPendingTasks();
  auto* change_listener = MakeGarbageCollected<EventCountingListener>(
      GetDocument(), event_type_names::kPointerlockchange);

  target->remove();
  test::RunPendingTasks();

  EXPECT_EQ(nullptr, PointerLockController().GetElement());
  EXPECT_FALSE(PointerLockController().LockPending());
  EXPECT_EQ(nullptr, GetDocument().PointerLockElement());
  EXPECT_EQ(1, change_listener->count());
}

// Verifies that the pointer lock gets released when the document that
// contains the element targeted by the lock is detached.
TEST_F(PointerLockControllerTest, DocumentDetachedReleasesLock) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id='child' srcdoc='<div id=target></div>'></iframe>
  )HTML");
  test::RunPendingTasks();

  auto* iframe = To<HTMLIFrameElement>(
      GetDocument().QuerySelector(AtomicString("#child")));
  ASSERT_TRUE(iframe);
  Document* child_doc = iframe->contentDocument();
  ASSERT_TRUE(child_doc);
  Element* target = child_doc->getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  ScriptState* script_state = ToScriptStateForMainWorld(child_doc->GetFrame());
  ScriptState::Scope scope(script_state);
  SimulateAcquirePointerLock(script_state, target);
  ASSERT_EQ(target, PointerLockController().GetElement());

  // Removing the iframe detaches its document. The locked element is *not*
  // removed from its own tree (so `ElementRemoved` does not fire on it); the
  // unlock must go through `DocumentDetached`.
  iframe->remove();
  test::RunPendingTasks();

  EXPECT_EQ(nullptr, PointerLockController().GetElement());
  EXPECT_FALSE(PointerLockController().LockPending());
}

// Verifies that when the page calls `requestPointerLock()` again on the
// already-locked element with the same `unadjustedMovement` setting, the
// controller resolves the promise synchronously without sending a new IPC to
// the browser and no event is fired.
TEST_F(PointerLockControllerTest,
       SecondRequestForSameTargetSameOptionsResolvesSynchronously) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  SimulateAcquirePointerLock(script_state, target);
  ASSERT_EQ(target, PointerLockController().GetElement());

  // Request again on the same element with the same (default) options.
  auto* change_listener = MakeGarbageCollected<EventCountingListener>(
      GetDocument(), event_type_names::kPointerlockchange);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  PointerLockController().RequestPointerLock(resolver, target, nullptr);

  // No browser round-trip should be needed; the promise must already be
  // fulfilled by the time control returns.
  EXPECT_EQ(v8::Promise::kFulfilled, promise.V8Promise()->State());
  EXPECT_EQ(target, PointerLockController().GetElement());
  EXPECT_EQ(0, change_listener->count());
}

// Sandboxed browsing contexts without the `allow-pointer-lock` token must
// have their requests rejected.
TEST_F(PointerLockControllerTest,
       RequestInSandboxedFrameWithoutPermissionRejectsWithSecurityError) {
  auto [child_doc, target, script_state] =
      SetUpSandboxedIframe("allow-scripts");
  ScriptState::Scope scope(script_state);

  auto* error_listener = MakeGarbageCollected<EventCountingListener>(
      *child_doc, event_type_names::kPointerlockerror);

  RequestPointerLockAndExpectRejection(script_state, target, "SecurityError");

  EXPECT_EQ(nullptr, PointerLockController().GetElement());
  EXPECT_FALSE(PointerLockController().LockPending());

  // The sandbox check runs before the rate limiter, so the counter stays at 0.
  EXPECT_EQ(0, GetRecentLockCount());

  // The spec requires firing `pointerlockerror` on the target's document.
  test::RunPendingTasks();
  EXPECT_EQ(1, error_listener->count());
}

// Verifies that a sandboxed iframe that *does* carry the `allow-pointer-lock`
// token can request a pointer lock like normal.
TEST_F(PointerLockControllerTest,
       RequestInSandboxedFrameWithPointerLockPermissionPasses) {
  auto [child_doc, target, script_state] =
      SetUpSandboxedIframe("allow-scripts allow-pointer-lock");
  ScriptState::Scope scope(script_state);

  auto* error_listener = MakeGarbageCollected<EventCountingListener>(
      *child_doc, event_type_names::kPointerlockerror);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  PointerLockController().RequestPointerLock(resolver, target, nullptr);

  // The request is in flight to the browser, so the promise is pending and
  // the controller is in the "lock pending" state.
  EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());
  EXPECT_EQ(target, PointerLockController().GetElement());
  EXPECT_TRUE(PointerLockController().LockPending());
  // The sandbox check did not short-circuit, so the rate limiter saw the
  // attempt.
  EXPECT_EQ(1, GetRecentLockCount());

  // No `pointerlockerror` is queued. Flush pending tasks to confirm.
  test::RunPendingTasks();
  EXPECT_EQ(0, error_listener->count());
}

// Verifies that a request on a detached element gets rejected without needing
// to go to the browser process.
TEST_F(PointerLockControllerTest,
       RequestOnDisconnectedTargetRejectsWithWrongDocumentError) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  auto* error_listener = MakeGarbageCollected<EventCountingListener>(
      GetDocument(), event_type_names::kPointerlockerror);

  // Detach the target before requesting the lock.
  target->remove();
  ASSERT_FALSE(target->isConnected());

  RequestPointerLockAndExpectRejection(script_state, target,
                                       "WrongDocumentError");

  // Spec requires firing `pointerlockerror` on the target's document.
  test::RunPendingTasks();
  EXPECT_EQ(1, error_listener->count());

  // The early rejection must run before the rate limiter, so a page can't
  // drain the rate-limit window by repeatedly requesting on detached nodes.
  EXPECT_EQ(nullptr, PointerLockController().GetElement());
  EXPECT_FALSE(PointerLockController().LockPending());
  EXPECT_EQ(0, GetRecentLockCount());
}

}  // namespace blink
