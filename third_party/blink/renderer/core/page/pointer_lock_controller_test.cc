// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_lock_options.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class PointerLockControllerRateLimitTest : public SimTest {
 public:
  PointerLockControllerRateLimitTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    SimTest::SetUp();
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(
        "<html><body><div id='target'>Target</div></body></html>");
  }

  void TearDown() override {
    PointerLockController().last_successful_lock_timestamp_ = base::TimeTicks();
    PointerLockController().recent_lock_attempts_ = 0;
    SimTest::TearDown();
  }

  blink::PointerLockController& PointerLockController() {
    return GetDocument().GetPage()->GetPointerLockController();
  }

  Element* GetTargetElement() {
    return GetDocument().getElementById(AtomicString("target"));
  }

  // Simulates receiving a response from the browser process to a previous
  // pointer lock request.
  void SimulateLockRequestResult(mojom::blink::PointerLockResult result) {
    PointerLockController().ProcessResult(base::DoNothing(), false, result);
  }

  // This function expects to be able to send the request to the browser
  // process. Therefore it shouldn't be called when `recent_lock_attempts_` >=
  // `kMaxLocksInWindow`.
  void SimulateAcquireAndReleasePointerLock(ScriptState* script_state,
                                            Element* target) {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    auto promise = resolver->Promise();
    PointerLockController().RequestPointerLock(resolver, target, nullptr);
    // The promise should be pending (waiting for browser response) after the
    // counter was reset.
    EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());
    SimulateLockRequestResult(mojom::blink::PointerLockResult::kSuccess);
    PointerLockController().DidAcquirePointerLock();
    PointerLockController().ExitPointerLock();
  }

  uint8_t GetRecentLockCount() {
    return PointerLockController().recent_lock_attempts_;
  }

 private:
  ScopedRateLimitPointerLockRequestsForTest enable_rate_limiting_{true};
};

TEST_F(PointerLockControllerRateLimitTest,
       SuccessfulLockAddsToRecentTimestamps) {
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
TEST_F(PointerLockControllerRateLimitTest,
       RequestRejectedAfterThresholdExceeded) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  // Simulate locks to reach the threshold.
  for (size_t i = 0; i < PointerLockController::kMaxLocksInWindow; ++i) {
    SimulateAcquireAndReleasePointerLock(script_state, target);
  }
  EXPECT_EQ(static_cast<int>(PointerLockController::kMaxLocksInWindow),
            GetRecentLockCount());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  // Request pointer lock. It should be rejected due to rate limiting.
  PointerLockController().RequestPointerLock(resolver, target, nullptr);

  // Verify rejected with DOMExceptionCode::kNotAllowedError.
  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());
  DOMException* dom_exception = V8DOMException::ToWrappable(
      script_state->GetIsolate(), promise.V8Promise()->Result());
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ("NotAllowedError", dom_exception->name());
}

// Simulate enough locks to exceed the threshold, then advance time past the
// rate limit window and verify that a new pointer lock request is accepted.
TEST_F(PointerLockControllerRateLimitTest,
       RequestAcceptedAfterRateLimitWindowPasses) {
  Element* target = GetTargetElement();
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope scope(script_state);

  // Simulate locks to reach the threshold.
  for (size_t i = 0; i < PointerLockController::kMaxLocksInWindow; ++i) {
    SimulateAcquireAndReleasePointerLock(script_state, target);
  }
  EXPECT_EQ(static_cast<int>(PointerLockController::kMaxLocksInWindow),
            GetRecentLockCount());

  // Request pointer lock. It should be rejected due to rate limiting since
  // we're still within the rate limit window.
  {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    PointerLockController().RequestPointerLock(resolver, target, nullptr);
    EXPECT_EQ(v8::Promise::kRejected, resolver->Promise().V8Promise()->State());
  }

  // Advance time halfway through the window. Request should still be rejected.
  task_environment().FastForwardBy(PointerLockController::kLockRateLimitWindow /
                                   2);
  {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    PointerLockController().RequestPointerLock(resolver, target, nullptr);
    EXPECT_EQ(v8::Promise::kRejected, resolver->Promise().V8Promise()->State());
    // Counter is not reset yet since we're still within the window.
    EXPECT_LT(static_cast<int>(PointerLockController::kMaxLocksInWindow),
              GetRecentLockCount());
  }

  // Advance time past the rate limit window. Counter should reset and request
  // should be accepted (pending browser response).
  task_environment().FastForwardBy(
      PointerLockController::kLockRateLimitWindow / 2 + base::Seconds(1));
  {
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    auto promise = resolver->Promise();
    PointerLockController().RequestPointerLock(resolver, target, nullptr);
    // The promise should be pending (waiting for browser response) after the
    // counter was reset.
    EXPECT_EQ(v8::Promise::kPending, promise.V8Promise()->State());
    // Counter resets when the window passes.
    EXPECT_EQ(0, GetRecentLockCount());
  }
}

}  // namespace blink
