// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

TEST(WakeLockTest, RequestWakeLockGranted) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      V8WakeLockType::Enum::kScreen, mojom::blink::PermissionStatus::GRANTED);

  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto screen_promise = screen_resolver->Promise();

  auto* wake_lock = WakeLock::wakeLock(*context.DomWindow()->navigator());
  wake_lock->DoRequest(V8WakeLockType::Enum::kScreen, screen_resolver);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kScreen);
  MockPermissionService& permission_service = context.GetPermissionService();

  permission_service.WaitForPermissionRequest(V8WakeLockType::Enum::kScreen);
  screen_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(screen_promise);

  EXPECT_NE(nullptr,
            ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
                context.GetScriptState()->GetIsolate(), screen_promise));
  EXPECT_TRUE(screen_lock.is_acquired());
}

TEST(WakeLockTest, RequestWakeLockDenied) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      V8WakeLockType::Enum::kSystem, mojom::blink::PermissionStatus::DENIED);

  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto system_promise = system_resolver->Promise();

  auto* wake_lock = WakeLock::wakeLock(*context.DomWindow()->navigator());
  wake_lock->DoRequest(V8WakeLockType::Enum::kSystem, system_resolver);

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kSystem);
  MockPermissionService& permission_service = context.GetPermissionService();

  permission_service.WaitForPermissionRequest(V8WakeLockType::Enum::kSystem);
  context.WaitForPromiseRejection(system_promise);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(system_promise));
  EXPECT_FALSE(system_lock.is_acquired());

  // System locks are not allowed by default, so the promise should have been
  // rejected with a NotAllowedError DOMException.
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(
          context.GetScriptState()->GetIsolate(), system_promise);
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ("NotAllowedError", dom_exception->name());
}

// https://w3c.github.io/screen-wake-lock/#handling-document-loss-of-full-activity
TEST(WakeLockTest, LossOfDocumentActivity) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kScreen);
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kSystem);
  context.GetPermissionService().SetPermissionResponse(
      V8WakeLockType::Enum::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      V8WakeLockType::Enum::kSystem, mojom::blink::PermissionStatus::GRANTED);

  // First, acquire a handful of locks of different types.
  auto* screen_resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto* screen_resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto* system_resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());

  auto* wake_lock = WakeLock::wakeLock(*context.DomWindow()->navigator());
  wake_lock->DoRequest(V8WakeLockType::Enum::kScreen, screen_resolver1);
  wake_lock->DoRequest(V8WakeLockType::Enum::kScreen, screen_resolver2);
  screen_lock.WaitForRequest();
  wake_lock->DoRequest(V8WakeLockType::Enum::kSystem, system_resolver1);
  system_lock.WaitForRequest();

  // Now shut down our Document and make sure all [[ActiveLocks]] slots have
  // been cleared. We cannot check that the promises have been rejected because
  // ScriptPromiseResolverBase::Reject() will bail out if we no longer have a
  // valid execution context.
  context.Frame()->DomWindow()->FrameDestroyed();
  screen_lock.WaitForCancelation();
  system_lock.WaitForCancelation();

  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_FALSE(system_lock.is_acquired());
}

// https://w3c.github.io/screen-wake-lock/#handling-document-loss-of-visibility
TEST(WakeLockTest, PageVisibilityHidden) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      V8WakeLockType::Enum::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      V8WakeLockType::Enum::kSystem, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto screen_promise = screen_resolver->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kSystem);
  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto system_promise = system_resolver->Promise();

  auto* wake_lock = WakeLock::wakeLock(*context.DomWindow()->navigator());
  wake_lock->DoRequest(V8WakeLockType::Enum::kScreen, screen_resolver);
  screen_lock.WaitForRequest();
  wake_lock->DoRequest(V8WakeLockType::Enum::kSystem, system_resolver);
  system_lock.WaitForRequest();

  context.WaitForPromiseFulfillment(screen_promise);
  context.WaitForPromiseFulfillment(system_promise);

  context.Frame()->GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden, false);

  screen_lock.WaitForCancelation();

  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_TRUE(system_lock.is_acquired());

  context.Frame()->GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kVisible, false);

  auto* other_resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto other_promise = other_resolver->Promise();
  wake_lock->DoRequest(V8WakeLockType::Enum::kScreen, other_resolver);
  screen_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(other_promise);
  EXPECT_TRUE(screen_lock.is_acquired());
}

// https://w3c.github.io/screen-wake-lock/#handling-document-loss-of-visibility
TEST(WakeLockTest, PageVisibilityHiddenBeforeLockAcquisition) {
  test::TaskEnvironment task_environment;

  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  context.GetPermissionService().SetPermissionResponse(
      V8WakeLockType::Enum::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      V8WakeLockType::Enum::kSystem, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto screen_promise = screen_resolver->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kSystem);
  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto system_promise = system_resolver->Promise();

  auto* wake_lock = WakeLock::wakeLock(*context.DomWindow()->navigator());
  wake_lock->DoRequest(V8WakeLockType::Enum::kScreen, screen_resolver);
  wake_lock->DoRequest(V8WakeLockType::Enum::kSystem, system_resolver);
  context.Frame()->GetPage()->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHidden, false);

  context.WaitForPromiseRejection(screen_promise);
  system_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(system_promise);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(screen_promise));
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(
          context.GetScriptState()->GetIsolate(), screen_promise);
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ("NotAllowedError", dom_exception->name());

  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_TRUE(system_lock.is_acquired());
}

}  // namespace blink
