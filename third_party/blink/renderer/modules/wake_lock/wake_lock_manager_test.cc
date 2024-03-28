// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

WakeLockManager* MakeManager(WakeLockTestingContext& context,
                             V8WakeLockType::Enum type) {
  return MakeGarbageCollected<WakeLockManager>(context.DomWindow(), type);
}

}  // namespace

TEST(WakeLockManagerTest, AcquireWakeLock) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* manager = MakeManager(context, V8WakeLockType::Enum::kScreen);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kScreen);
  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_FALSE(manager->wake_lock_.is_bound());

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise2 = resolver2->Promise();

  manager->AcquireWakeLock(resolver1);
  manager->AcquireWakeLock(resolver2);
  screen_lock.WaitForRequest();

  context.WaitForPromiseFulfillment(promise1);
  context.WaitForPromiseFulfillment(promise2);

  auto* sentinel1 = ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
      context.GetScriptState()->GetIsolate(), promise1);
  auto* sentinel2 = ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
      context.GetScriptState()->GetIsolate(), promise2);

  EXPECT_TRUE(manager->wake_lock_sentinels_.Contains(sentinel1));
  EXPECT_TRUE(manager->wake_lock_sentinels_.Contains(sentinel2));
  EXPECT_EQ(2U, manager->wake_lock_sentinels_.size());
  EXPECT_TRUE(screen_lock.is_acquired());
  EXPECT_TRUE(manager->wake_lock_.is_bound());
}

TEST(WakeLockManagerTest, ReleaseAllWakeLocks) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* manager = MakeManager(context, V8WakeLockType::Enum::kScreen);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kScreen);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise = resolver->Promise();

  manager->AcquireWakeLock(resolver);
  screen_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(promise);

  EXPECT_EQ(1U, manager->wake_lock_sentinels_.size());
  EXPECT_TRUE(screen_lock.is_acquired());

  auto* sentinel = ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
      context.GetScriptState()->GetIsolate(), promise);

  manager->UnregisterSentinel(sentinel);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(0U, manager->wake_lock_sentinels_.size());
  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_FALSE(manager->wake_lock_.is_bound());
}

TEST(WakeLockManagerTest, ReleaseOneWakeLock) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* manager = MakeManager(context, V8WakeLockType::Enum::kScreen);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kScreen);

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise2 = resolver2->Promise();

  manager->AcquireWakeLock(resolver1);
  manager->AcquireWakeLock(resolver2);
  screen_lock.WaitForRequest();

  context.WaitForPromiseFulfillment(promise1);
  context.WaitForPromiseFulfillment(promise2);

  EXPECT_TRUE(screen_lock.is_acquired());
  EXPECT_EQ(2U, manager->wake_lock_sentinels_.size());

  auto* sentinel1 = ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
      context.GetScriptState()->GetIsolate(), promise1);
  EXPECT_TRUE(manager->wake_lock_sentinels_.Contains(sentinel1));

  manager->UnregisterSentinel(sentinel1);
  EXPECT_FALSE(manager->wake_lock_sentinels_.Contains(sentinel1));
  EXPECT_TRUE(manager->wake_lock_.is_bound());
  EXPECT_EQ(1U, manager->wake_lock_sentinels_.size());
  EXPECT_TRUE(screen_lock.is_acquired());
}

TEST(WakeLockManagerTest, ClearEmptyWakeLockSentinelList) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* manager = MakeManager(context, V8WakeLockType::Enum::kSystem);

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kSystem);
  EXPECT_FALSE(system_lock.is_acquired());

  manager->ClearWakeLocks();
  test::RunPendingTasks();

  EXPECT_FALSE(system_lock.is_acquired());
}

TEST(WakeLockManagerTest, ClearWakeLocks) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* manager = MakeManager(context, V8WakeLockType::Enum::kSystem);

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise2 = resolver2->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kSystem);

  manager->AcquireWakeLock(resolver1);
  manager->AcquireWakeLock(resolver2);
  system_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(promise1);
  context.WaitForPromiseFulfillment(promise2);

  EXPECT_EQ(2U, manager->wake_lock_sentinels_.size());

  manager->ClearWakeLocks();
  system_lock.WaitForCancelation();

  EXPECT_EQ(0U, manager->wake_lock_sentinels_.size());
  EXPECT_FALSE(system_lock.is_acquired());
}

TEST(WakeLockManagerTest, WakeLockConnectionError) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* manager = MakeManager(context, V8WakeLockType::Enum::kSystem);

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise2 = resolver2->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(V8WakeLockType::Enum::kSystem);

  manager->AcquireWakeLock(resolver1);
  manager->AcquireWakeLock(resolver2);
  system_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(promise1);
  context.WaitForPromiseFulfillment(promise2);

  EXPECT_TRUE(manager->wake_lock_.is_bound());
  EXPECT_EQ(2U, manager->wake_lock_sentinels_.size());

  // Unbind and wait for the disconnection to reach |wake_lock_|'s
  // disconnection handler.
  system_lock.Unbind();
  manager->wake_lock_.FlushForTesting();

  EXPECT_EQ(0U, manager->wake_lock_sentinels_.size());
  EXPECT_FALSE(manager->wake_lock_.is_bound());
  EXPECT_FALSE(system_lock.is_acquired());
}

}  // namespace blink
