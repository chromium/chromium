// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "v8/include/v8.h"

namespace blink {

TEST(WakeLockTest, RequestWakeLockGranted) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);

  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  auto* wake_lock = MakeGarbageCollected<WakeLock>(*context.GetDocument());
  wake_lock->DoRequest(WakeLockType::kScreen, screen_resolver);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  MockPermissionService& permission_service = context.GetPermissionService();

  permission_service.WaitForPermissionRequest(WakeLockType::kScreen);
  screen_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(screen_promise);

  EXPECT_NE(nullptr, ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
                         screen_promise));
  EXPECT_TRUE(screen_lock.is_acquired());
}

TEST(WakeLockTest, RequestWakeLockDenied) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::DENIED);

  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  auto* wake_lock = MakeGarbageCollected<WakeLock>(*context.GetDocument());
  wake_lock->DoRequest(WakeLockType::kSystem, system_resolver);

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  MockPermissionService& permission_service = context.GetPermissionService();

  permission_service.WaitForPermissionRequest(WakeLockType::kSystem);
  context.WaitForPromiseRejection(system_promise);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(system_promise));
  EXPECT_FALSE(system_lock.is_acquired());

  // System locks are not allowed by default, so the promise should have been
  // rejected with a NotAllowedError DOMException.
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(system_promise);
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ("NotAllowedError", dom_exception->name());
}

// https://w3c.github.io/wake-lock/#handling-document-loss-of-full-activity
TEST(WakeLockTest, LossOfDocumentActivity) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::GRANTED);

  // First, acquire a handful of locks of different types.
  auto* screen_resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  screen_resolver1->Promise();
  auto* screen_resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  screen_resolver2->Promise();
  auto* system_resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  system_resolver1->Promise();

  auto* wake_lock = MakeGarbageCollected<WakeLock>(*context.GetDocument());
  wake_lock->DoRequest(WakeLockType::kScreen, screen_resolver1);
  wake_lock->DoRequest(WakeLockType::kScreen, screen_resolver2);
  screen_lock.WaitForRequest();
  wake_lock->DoRequest(WakeLockType::kSystem, system_resolver1);
  system_lock.WaitForRequest();

  // Now shut down our Document and make sure all [[ActiveLocks]] slots have
  // been cleared. We cannot check that the promises have been rejected because
  // ScriptPromiseResolver::Reject() will bail out if we no longer have a valid
  // execution context.
  context.GetDocument()->Shutdown();
  screen_lock.WaitForCancelation();
  system_lock.WaitForCancelation();

  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_FALSE(system_lock.is_acquired());
}

// https://w3c.github.io/wake-lock/#handling-document-loss-of-visibility
TEST(WakeLockTest, PageVisibilityHidden) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  auto* wake_lock = MakeGarbageCollected<WakeLock>(*context.GetDocument());
  wake_lock->DoRequest(WakeLockType::kScreen, screen_resolver);
  screen_lock.WaitForRequest();
  wake_lock->DoRequest(WakeLockType::kSystem, system_resolver);
  system_lock.WaitForRequest();

  context.WaitForPromiseFulfillment(screen_promise);
  context.WaitForPromiseFulfillment(system_promise);

  context.GetDocument()->GetPage()->SetVisibilityState(
      PageVisibilityState::kHidden, false);

  screen_lock.WaitForCancelation();

  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_TRUE(system_lock.is_acquired());

  context.GetDocument()->GetPage()->SetVisibilityState(
      PageVisibilityState::kVisible, false);

  auto* other_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise other_promise = other_resolver->Promise();
  wake_lock->DoRequest(WakeLockType::kScreen, other_resolver);
  screen_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(other_promise);
  EXPECT_TRUE(screen_lock.is_acquired());
}

// https://w3c.github.io/wake-lock/#handling-document-loss-of-visibility
TEST(WakeLockTest, PageVisibilityHiddenBeforeLockAcquisition) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  auto* wake_lock = MakeGarbageCollected<WakeLock>(*context.GetDocument());
  wake_lock->DoRequest(WakeLockType::kScreen, screen_resolver);
  wake_lock->DoRequest(WakeLockType::kSystem, system_resolver);
  context.GetDocument()->GetPage()->SetVisibilityState(
      PageVisibilityState::kHidden, false);

  context.WaitForPromiseRejection(screen_promise);
  system_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(system_promise);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(screen_promise));
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(screen_promise);
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ("NotAllowedError", dom_exception->name());

  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_TRUE(system_lock.is_acquired());
}

}  // namespace blink
