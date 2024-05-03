// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_observer_options.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_test_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Constants to modify ChangeRateMonitor settings for deterministic test.
constexpr uint64_t kChangeCount = 2;
constexpr base::TimeDelta kDelayTime = base::Seconds(1);
constexpr base::TimeDelta kPenaltyDuration = base::Seconds(4);

// Helper class for WaitForPromiseFulfillment(). It provides a
// function that invokes |callback| when a ScriptPromiseUntyped is resolved.
class ClosureRunnerCallable final : public ScriptFunction::Callable {
 public:
  explicit ClosureRunnerCallable(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  ScriptValue Call(ScriptState*, ScriptValue) override {
    if (callback_) {
      std::move(callback_).Run();
    }
    return ScriptValue();
  }

 private:
  base::OnceClosure callback_;
};

void WaitForPromiseFulfillment(ScriptState* script_state,
                               ScriptPromiseUntyped promise) {
  base::RunLoop run_loop;
  promise.Then(MakeGarbageCollected<ScriptFunction>(
      script_state,
      MakeGarbageCollected<ClosureRunnerCallable>(run_loop.QuitClosure())));
  // Execute pending microtasks, otherwise it can take a few seconds for the
  // promise to resolve.
  script_state->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
      script_state->GetIsolate());
  run_loop.Run();
}

}  // namespace

TEST(PressureObserverTest, PressureObserverDisconnectBeforePenaltyEnd) {
  test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  FakePressureService pressure_service;
  ComputePressureTestingContext scope(&pressure_service);

  base::RunLoop callback_run_loop;

  auto* callback_function = MakeGarbageCollected<ScriptFunction>(
      scope.GetScriptState(), MakeGarbageCollected<ClosureRunnerCallable>(
                                  callback_run_loop.QuitClosure()));
  auto* callback =
      V8PressureUpdateCallback::Create(callback_function->V8Function());

  V8PressureSource source(V8PressureSource::Enum::kCpu);
  auto* options = PressureObserverOptions::Create();
  auto* observer = PressureObserver::Create(callback);
  auto promise = observer->observe(scope.GetScriptState(), source, options,
                                   scope.GetExceptionState());

  WaitForPromiseFulfillment(scope.GetScriptState(), promise);

  observer->change_rate_monitor_for_testing().set_change_count_threshold_for_testing(
      kChangeCount);
  observer->change_rate_monitor_for_testing().set_penalty_duration_for_testing(
      kPenaltyDuration);

  // First update.
  task_environment.FastForwardBy(kDelayTime);
  pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
      device::mojom::blink::PressureSource::kCpu,
      device::mojom::blink::PressureState::kCritical, base::TimeTicks::Now()));

  callback_run_loop.Run();

  // Second update triggering the penalty.
  task_environment.FastForwardBy(kDelayTime);
  pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
      device::mojom::blink::PressureSource::kCpu,
      device::mojom::blink::PressureState::kNominal, base::TimeTicks::Now()));
  // The number of seconds here should not exceed the penalty time, we just
  // want to run some code like OnUpdate() but not the pending delayed task
  // that it should have created.
  task_environment.FastForwardBy(kDelayTime);

  observer->disconnect();
  // This should not crash.
  // The number of seconds here together with the previous FastForwardBy() call
  // needs to exceed the chosen penalty time.
  task_environment.FastForwardBy(kPenaltyDuration);
}

TEST(PressureObserverTest, PressureObserverUnobserveBeforePenaltyEnd) {
  test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  FakePressureService pressure_service;
  ComputePressureTestingContext scope(&pressure_service);

  base::RunLoop callback_run_loop;

  auto* callback_function = MakeGarbageCollected<ScriptFunction>(
      scope.GetScriptState(), MakeGarbageCollected<ClosureRunnerCallable>(
                                  callback_run_loop.QuitClosure()));
  auto* callback =
      V8PressureUpdateCallback::Create(callback_function->V8Function());

  V8PressureSource source(V8PressureSource::Enum::kCpu);
  auto* options = PressureObserverOptions::Create();
  auto* observer = PressureObserver::Create(callback);
  auto promise = observer->observe(scope.GetScriptState(), source, options,
                                   scope.GetExceptionState());

  WaitForPromiseFulfillment(scope.GetScriptState(), promise);

  observer->change_rate_monitor_for_testing().set_change_count_threshold_for_testing(
      kChangeCount);
  observer->change_rate_monitor_for_testing().set_penalty_duration_for_testing(
      kPenaltyDuration);

  // First update.
  task_environment.FastForwardBy(kDelayTime);
  pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
      device::mojom::blink::PressureSource::kCpu,
      device::mojom::blink::PressureState::kNominal, base::TimeTicks::Now()));

  callback_run_loop.Run();

  // Second update triggering the penalty.
  task_environment.FastForwardBy(kDelayTime);
  pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
      device::mojom::blink::PressureSource::kCpu,
      device::mojom::blink::PressureState::kCritical, base::TimeTicks::Now()));
  // The number of seconds here should not exceed the penalty time, we just
  // want to run some code like OnUpdate() but not the pending delayed task
  // that it should have created.
  task_environment.FastForwardBy(kDelayTime);

  observer->unobserve(source);
  // This should not crash.
  // The number of seconds here together with the previous FastForwardBy() call
  // needs to exceed the chosen penalty time.
  task_environment.FastForwardBy(kPenaltyDuration);
}

}  // namespace blink
