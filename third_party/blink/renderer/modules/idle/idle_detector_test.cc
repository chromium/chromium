// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/idle/idle_detector.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idle_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_screen_idle_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_user_idle_state.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/modules/idle/idle_manager.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

using ::testing::Invoke;
using ::testing::WithoutArgs;

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext*, Event*));
};

class FakeIdleService final : public mojom::blink::IdleManager {
 public:
  FakeIdleService() {
    SetState(/*idle_time=*/std::nullopt, /*screen_locked=*/false);
  }

  mojo::PendingRemote<mojom::blink::IdleManager> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void SetState(std::optional<base::TimeDelta> idle_time,
                bool screen_locked,
                bool override = false) {
    state_ = mojom::blink::IdleState::New();
    state_->idle_time = idle_time;
    state_->screen_locked = screen_locked;

    if (monitor_)
      monitor_->Update(state_.Clone(), override);
  }

  // mojom::IdleManager
  void AddMonitor(mojo::PendingRemote<mojom::blink::IdleMonitor> monitor,
                  AddMonitorCallback callback) override {
    monitor_.Bind(std::move(monitor));
    std::move(callback).Run(mojom::blink::IdleManagerError::kSuccess,
                            state_.Clone());
  }

 private:
  mojo::Receiver<mojom::blink::IdleManager> receiver_{this};
  mojo::Remote<mojom::blink::IdleMonitor> monitor_;
  mojom::blink::IdleStatePtr state_;
};

}  // namespace

TEST(IdleDetectorTest, Start) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  auto* detector = IdleDetector::Create(scope.GetScriptState());

  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);
  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kActive, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kUnlocked, detector->screenState());
  })));

  auto* options = IdleOptions::Create();
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());
}

TEST(IdleDetectorTest, StartIdleWithLongThreshold) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  // Initial state is idle but the event should be delayed due to the long
  // threshold.
  idle_service.SetState(/*idle_time=*/base::Seconds(0),
                        /*screen_locked=*/false);

  auto* detector = IdleDetector::Create(scope.GetScriptState());
  detector->SetTaskRunnerForTesting(task_runner,
                                    task_runner->GetMockTickClock());

  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);
  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kActive, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kUnlocked, detector->screenState());
  })));

  auto* options = IdleOptions::Create();
  options->setThreshold(90000);
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());
  testing::Mock::VerifyAndClearExpectations(listener);

  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kIdle, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kUnlocked, detector->screenState());
  })));
  task_runner->FastForwardBy(base::Seconds(30));
  testing::Mock::VerifyAndClearExpectations(listener);
  EXPECT_FALSE(task_runner->HasPendingTask());
}

TEST(IdleDetectorTest, LockScreen) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  auto* detector = IdleDetector::Create(scope.GetScriptState());
  auto* options = IdleOptions::Create();
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());

  base::RunLoop loop;
  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);
  EXPECT_CALL(*listener, Invoke)
      .WillOnce(WithoutArgs(Invoke([detector, &loop]() {
        EXPECT_EQ(V8UserIdleState::Enum::kActive, detector->userState());
        EXPECT_EQ(V8ScreenIdleState::Enum::kLocked, detector->screenState());
        loop.Quit();
      })));
  idle_service.SetState(/*idle_time=*/std::nullopt, /*screen_locked=*/true);
  loop.Run();
}

TEST(IdleDetectorTest, BecomeIdle) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  auto* detector = IdleDetector::Create(scope.GetScriptState());
  auto* options = IdleOptions::Create();
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());

  base::RunLoop loop;
  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);
  EXPECT_CALL(*listener, Invoke)
      .WillOnce(WithoutArgs(Invoke([detector, &loop]() {
        EXPECT_EQ(V8UserIdleState::Enum::kIdle, detector->userState());
        EXPECT_EQ(V8ScreenIdleState::Enum::kUnlocked, detector->screenState());
        loop.Quit();
      })));
  idle_service.SetState(/*idle_time=*/base::Seconds(0),
                        /*screen_locked=*/false);
  loop.Run();
}

TEST(IdleDetectorTest, BecomeIdleAndLockScreen) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  auto* detector = IdleDetector::Create(scope.GetScriptState());
  auto* options = IdleOptions::Create();
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());

  base::RunLoop loop;
  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);
  EXPECT_CALL(*listener, Invoke)
      .WillOnce(WithoutArgs(Invoke([detector, &loop]() {
        EXPECT_EQ(V8UserIdleState::Enum::kIdle, detector->userState());
        EXPECT_EQ(V8ScreenIdleState::Enum::kLocked, detector->screenState());
        loop.Quit();
      })));
  idle_service.SetState(/*idle_time=*/base::Seconds(0), /*screen_locked=*/true);
  loop.Run();
}

TEST(IdleDetectorTest, BecomeIdleAndLockScreenWithLongThreshold) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  auto* detector = IdleDetector::Create(scope.GetScriptState());
  detector->SetTaskRunnerForTesting(task_runner,
                                    task_runner->GetMockTickClock());

  auto* options = IdleOptions::Create();
  options->setThreshold(90000);
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());

  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);

  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kActive, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kLocked, detector->screenState());
  })));
  idle_service.SetState(/*idle_time=*/base::Seconds(0), /*screen_locked=*/true);
  task_runner->FastForwardBy(base::Seconds(0));
  testing::Mock::VerifyAndClearExpectations(listener);

  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kIdle, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kLocked, detector->screenState());
  })));
  task_runner->FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(task_runner->HasPendingTask());

  testing::Mock::VerifyAndClearExpectations(listener);
}

TEST(IdleDetectorTest, BecomeIdleAndLockAfterWithLongThreshold) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  auto* detector = IdleDetector::Create(scope.GetScriptState());
  detector->SetTaskRunnerForTesting(task_runner,
                                    task_runner->GetMockTickClock());

  auto* options = IdleOptions::Create();
  options->setThreshold(90000);
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());

  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);

  // No initial event since the state hasn't change and the threshold hasn't
  // been reached.
  idle_service.SetState(/*idle_time=*/base::Seconds(0),
                        /*screen_locked=*/false);
  task_runner->FastForwardBy(base::Seconds(0));

  // Screen lock event fires immediately but still waiting for idle threshold
  // to be reached.
  task_runner->FastForwardBy(base::Seconds(15));
  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kActive, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kLocked, detector->screenState());
  })));
  idle_service.SetState(/*idle_time=*/base::Seconds(15),
                        /*screen_locked=*/true);
  task_runner->FastForwardBy(base::Seconds(0));
  testing::Mock::VerifyAndClearExpectations(listener);

  // Finally the idle threshold has been reached.
  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kIdle, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kLocked, detector->screenState());
  })));
  task_runner->FastForwardBy(base::Seconds(15));

  // There shouldn't be any remaining tasks.
  EXPECT_FALSE(task_runner->HasPendingTask());

  testing::Mock::VerifyAndClearExpectations(listener);
}

TEST(IdleDetectorTest, BecomeIdleThenActiveBeforeThreshold) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  auto* detector = IdleDetector::Create(scope.GetScriptState());
  detector->SetTaskRunnerForTesting(task_runner,
                                    task_runner->GetMockTickClock());

  auto* options = IdleOptions::Create();
  options->setThreshold(90000);
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());

  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);

  // No update on the initial event because the user has only been idle for 60s.
  EXPECT_CALL(*listener, Invoke).Times(0);
  idle_service.SetState(/*idle_time=*/base::Seconds(0),
                        /*screen_locked=*/false);

  // 15s later the user becomes active again.
  task_runner->FastForwardBy(base::Seconds(15));
  idle_service.SetState(/*idle_time=*/std::nullopt, /*screen_locked=*/false);

  // 15s later we would have fired an event but shouldn't because the user
  // became active.
  task_runner->FastForwardBy(base::Seconds(15));
  EXPECT_FALSE(task_runner->HasPendingTask());

  testing::Mock::VerifyAndClearExpectations(listener);
}

TEST(IdleDetectorTest, SetAndClearOverrides) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FakeIdleService idle_service;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  auto* idle_manager = IdleManager::From(scope.GetExecutionContext());
  idle_manager->InitForTesting(idle_service.BindNewPipeAndPassRemote());

  auto* detector = IdleDetector::Create(scope.GetScriptState());
  detector->SetTaskRunnerForTesting(task_runner,
                                    task_runner->GetMockTickClock());

  auto* options = IdleOptions::Create();
  options->setThreshold(90000);
  ScriptPromiseUntyped start_promise = detector->start(
      scope.GetScriptState(), options, scope.GetExceptionState());

  ScriptPromiseTester start_tester(scope.GetScriptState(), start_promise);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());

  auto* listener = MakeGarbageCollected<MockEventListener>();
  detector->addEventListener(event_type_names::kChange, listener);

  // Simulate DevTools specifying an override. Even though the threshold is
  // 90 seconds the state should be updated immediately.
  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kIdle, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kLocked, detector->screenState());
  })));
  idle_service.SetState(/*idle_time=*/base::Seconds(0),
                        /*screen_locked=*/true, /*override=*/true);
  task_runner->FastForwardBy(base::Seconds(0));
  testing::Mock::VerifyAndClearExpectations(listener);

  // Simulate DevTools clearing the override. By this point the user has
  // actually been idle for 15 seconds but the threshold hasn't been reached.
  // Only the lock state updates immediately.
  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kActive, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kUnlocked, detector->screenState());
  })));
  idle_service.SetState(/*idle_time=*/base::Seconds(15),
                        /*screen_locked=*/false, /*override=*/false);
  task_runner->FastForwardBy(base::Seconds(0));
  testing::Mock::VerifyAndClearExpectations(listener);

  // After the threshold has been reached the idle state updates as well.
  EXPECT_CALL(*listener, Invoke).WillOnce(WithoutArgs(Invoke([detector]() {
    EXPECT_EQ(V8UserIdleState::Enum::kIdle, detector->userState());
    EXPECT_EQ(V8ScreenIdleState::Enum::kUnlocked, detector->screenState());
  })));
  task_runner->FastForwardBy(base::Seconds(15));

  // There shouldn't be any remaining tasks.
  EXPECT_FALSE(task_runner->HasPendingTask());
  testing::Mock::VerifyAndClearExpectations(listener);
}

}  // namespace blink
