// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class SyncEventListener final : public NativeEventListener {
 public:
  explicit SyncEventListener(base::OnceClosure invocation_callback)
      : invocation_callback_(std::move(invocation_callback)) {}
  void Invoke(ExecutionContext*, Event*) override {
    DCHECK(invocation_callback_);
    std::move(invocation_callback_).Run();
  }

 private:
  base::OnceClosure invocation_callback_;
};

}  // namespace

TEST(WakeLockSentinelTest, SentinelType) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  auto* sentinel = MakeGarbageCollected<WakeLockSentinel>(
      context.GetScriptState(), V8WakeLockType::Enum::kScreen,
      /*manager=*/nullptr);
  EXPECT_EQ("screen", sentinel->type().AsString());

  sentinel = MakeGarbageCollected<WakeLockSentinel>(
      context.GetScriptState(), V8WakeLockType::Enum::kSystem,
      /*manager=*/nullptr);
  EXPECT_EQ("system", sentinel->type().AsString());
}

TEST(WakeLockSentinelTest, SentinelReleased) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  auto* manager = MakeGarbageCollected<WakeLockManager>(
      context.DomWindow(), V8WakeLockType::Enum::kScreen);
  auto* sentinel = MakeGarbageCollected<WakeLockSentinel>(
      context.GetScriptState(), V8WakeLockType::Enum::kScreen, manager);
  EXPECT_FALSE(sentinel->released());

  manager = MakeGarbageCollected<WakeLockManager>(
      context.DomWindow(), V8WakeLockType::Enum::kSystem);
  sentinel = MakeGarbageCollected<WakeLockSentinel>(
      context.GetScriptState(), V8WakeLockType::Enum::kSystem, manager);
  EXPECT_FALSE(sentinel->released());
}

TEST(WakeLockSentinelTest, MultipleReleaseCalls) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  auto* manager = MakeGarbageCollected<WakeLockManager>(
      context.DomWindow(), V8WakeLockType::Enum::kScreen);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise = resolver->Promise();
  manager->AcquireWakeLock(resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* sentinel = ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
      context.GetScriptState()->GetIsolate(), promise);
  ASSERT_NE(nullptr, sentinel);
  EXPECT_FALSE(sentinel->released());

  base::RunLoop run_loop;
  auto* event_listener =
      MakeGarbageCollected<SyncEventListener>(run_loop.QuitClosure());
  sentinel->addEventListener(event_type_names::kRelease, event_listener,
                             /*use_capture=*/false);
  sentinel->release(context.GetScriptState());
  run_loop.Run();
  sentinel->removeEventListener(event_type_names::kRelease, event_listener,
                                /*use_capture=*/false);

  EXPECT_EQ(nullptr, sentinel->manager_);
  EXPECT_TRUE(sentinel->released());

  event_listener = MakeGarbageCollected<SyncEventListener>(WTF::BindOnce([]() {
    EXPECT_TRUE(false) << "This event handler should not be reached.";
  }));
  sentinel->addEventListener(event_type_names::kRelease, event_listener);
  sentinel->release(context.GetScriptState());
  EXPECT_TRUE(sentinel->released());
}

TEST(WakeLockSentinelTest, ContextDestruction) {
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

  WakeLockManager* manager =
      wake_lock->managers_[static_cast<size_t>(V8WakeLockType::Enum::kScreen)];
  ASSERT_TRUE(manager);

  context.WaitForPromiseFulfillment(screen_promise);
  auto* sentinel = ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
      context.GetScriptState()->GetIsolate(), screen_promise);
  ASSERT_TRUE(sentinel);

  auto* event_listener =
      MakeGarbageCollected<SyncEventListener>(WTF::BindOnce([]() {
        EXPECT_TRUE(false) << "This event handler should not be reached.";
      }));
  sentinel->addEventListener(event_type_names::kRelease, event_listener);
  EXPECT_TRUE(sentinel->HasPendingActivity());

  context.DomWindow()->FrameDestroyed();

  // If the method returns false the object can be GC'ed.
  EXPECT_FALSE(sentinel->HasPendingActivity());
}

TEST(WakeLockSentinelTest, HasPendingActivityConditions) {
  test::TaskEnvironment task_environment;
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);

  auto* manager = MakeGarbageCollected<WakeLockManager>(
      context.DomWindow(), V8WakeLockType::Enum::kScreen);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WakeLockSentinel>>(
          context.GetScriptState());
  auto promise = resolver->Promise();
  manager->AcquireWakeLock(resolver);
  context.WaitForPromiseFulfillment(promise);
  auto* sentinel = ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
      context.GetScriptState()->GetIsolate(), promise);
  ASSERT_TRUE(sentinel);

  // A new WakeLockSentinel was created and it can be GC'ed.
  EXPECT_FALSE(sentinel->HasPendingActivity());

  base::RunLoop run_loop;
  auto* event_listener =
      MakeGarbageCollected<SyncEventListener>(run_loop.QuitClosure());
  sentinel->addEventListener(event_type_names::kRelease, event_listener);

  // The sentinel cannot be GC'ed, it has an event listener and it has not been
  // released.
  EXPECT_TRUE(sentinel->HasPendingActivity());

  // An event such as a page visibility change will eventually call this method.
  manager->ClearWakeLocks();
  run_loop.Run();

  // The sentinel can be GC'ed even though it still has an event listener, as
  // it has already been released.
  EXPECT_FALSE(sentinel->HasPendingActivity());
}

}  // namespace blink
