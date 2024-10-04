// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_observer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_record.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_state.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_test_utils.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_record.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "v8/include/v8.h"

using device::mojom::blink::PressureState;

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

// Helper class expected to be used as a PressureObserver callback (i.e.
// PressureUpdateCallback). When invoked, it takes the |changes| array passed as
// a first argument to the callback and stores its elements, which can later be
// retrieved by the pressure_records() method.
//
// This is similar to the following in JS:
//
// let pressure_records = [];
// const PressureRecordAccumulator = (changes) => {
//   pressure_records = pressure_records.concat(changes);
// }
// /* Later on */
// const observer = new PressureObserver(PressureRecordAccumulator);
class PressureRecordAccumulator final : public ScriptFunction::Callable {
 public:
  ScriptValue Call(ScriptState*, ScriptValue script_value) override {
    {
      NonThrowableExceptionState exception_state;
      const auto& updates =
          NativeValueTraits<IDLSequence<PressureRecord>>::NativeValue(
              script_value.GetIsolate(), script_value.V8Value(),
              exception_state);
      pressure_records_.AppendVector(updates);
    }
    return ScriptValue();
  }

  const auto& pressure_records() const { return pressure_records_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(pressure_records_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  HeapVector<Member<PressureRecord>> pressure_records_;
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

TEST(PressureObserverTest, RateObfuscationMitigation) {
  test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  FakePressureService pressure_service;
  ComputePressureTestingContext scope(&pressure_service);

  auto* pressure_record_accumulator =
      MakeGarbageCollected<PressureRecordAccumulator>();
  auto* callback = V8PressureUpdateCallback::Create(
      MakeGarbageCollected<ScriptFunction>(scope.GetScriptState(),
                                           pressure_record_accumulator)
          ->V8Function());

  constexpr size_t kNumPressureStates =
      static_cast<size_t>(PressureState::kMaxValue) + 1U;
  constexpr std::array<device::mojom::blink::PressureState, kNumPressureStates>
      kPressureStates = {
          PressureState::kNominal,
          PressureState::kFair,
          PressureState::kSerious,
          PressureState::kCritical,
      };
  constexpr base::TimeDelta kSamplingInterval = base::Milliseconds(200);
  constexpr base::TimeDelta kSmallInterval = base::Milliseconds(100);

  auto* observer = PressureObserver::Create(callback);
  // Add 1 to kNumPressureStats because we want to initially send kNumPressure
  // updates without triggering the rate obfuscation mitigations.
  observer->change_rate_monitor_for_testing()
      .set_change_count_threshold_for_testing(kNumPressureStates + 1U);
  observer->change_rate_monitor_for_testing().set_penalty_duration_for_testing(
      kPenaltyDuration);

  auto* options = PressureObserverOptions::Create();
  options->setSampleInterval(kSamplingInterval.InMilliseconds());
  auto promise = observer->observe(
      scope.GetScriptState(), V8PressureSource(V8PressureSource::Enum::kCpu),
      options, scope.GetExceptionState());
  WaitForPromiseFulfillment(scope.GetScriptState(), promise);

  // Fast-forward by any positive amount of time just so that any
  // base::TimeTicks::Now() invocation differs from the original value recorded
  // by ChangeRateMonitor when it was created by PressureObserver.
  task_environment.FastForwardBy(kSmallInterval);

  const auto& pressure_records =
      pressure_record_accumulator->pressure_records();

  // First test sending updates without triggering the rate obfuscation
  // mitigation. We send kNumPressureStates updates and verify that they were
  // sent without delay.
  {
    for (wtf_size_t i = 0; i < kPressureStates.size(); ++i) {
      // None of these updates trigger the rate obfuscation mitigations because
      // of the value we passed to set_change_count_threshold_for_testing(), so
      // they are all dispatched immediately even if we do not advance time.
      EXPECT_EQ(pressure_records.size(), i);
      const auto& state = kPressureStates[i];
      pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
          device::mojom::blink::PressureSource::kCpu, state,
          base::TimeTicks::Now()));
      task_environment.FastForwardBy(base::Milliseconds(0));
      EXPECT_EQ(pressure_records.size(), i + 1);

      // Advance time nonetheless so that the next update is sent with a more
      // recent timestamp.
      task_environment.FastForwardBy(kSamplingInterval);
    }
    ASSERT_EQ(pressure_records.size(), kNumPressureStates);

    // While here, check that PressureRecord.time is recorded properly for each
    // update. The difference between each timestamp should be
    // kSamplingInterval, which is how much we fast-forwarded by in the loop
    // above.
    for (wtf_size_t i = 0; i < (pressure_records.size() - 1U); ++i) {
      EXPECT_EQ(pressure_records[i + 1]->time() - pressure_records[i]->time(),
                kSamplingInterval.InMilliseconds());
    }
  }

  // Test the rate obfuscation mitigation. At this point, we have sent
  // kNumPressureStates updates and therefore activated the rate obfuscation
  // mitigation for future updates.
  {
    const wtf_size_t original_callback_count = pressure_records.size();

    // This update will not be sent immediately and will be queued by the rate
    // obfuscation code instead.
    pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
        device::mojom::blink::PressureSource::kCpu, PressureState::kNominal,
        base::TimeTicks::Now()));
    task_environment.FastForwardBy(base::Milliseconds(0));
    EXPECT_EQ(pressure_records.size(), original_callback_count);

    // Advancing by a delta smaller than kPenaltyDuration (4000ms) also does
    // not send any updates.
    task_environment.FastForwardBy(kSamplingInterval);
    EXPECT_EQ(pressure_records.size(), original_callback_count);

    // Test the what happens when an update is sent while we are already under
    // penalty: the new update must replace the previously queued one.
    pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
        device::mojom::blink::PressureSource::kCpu, PressureState::kFair,
        base::TimeTicks::Now()));
    // If we advance another 200ms, we are still 3600s short of the penalty
    // duration, after which we will finally send an update.
    task_environment.FastForwardBy(kSamplingInterval);
    EXPECT_EQ(pressure_records.size(), original_callback_count);
    // Advance the remaining 3600s to get out of the penalty. This is
    // kPenaltyDuration minus the two FastForwardBy(kSamplingInterval) calls we
    // have made.
    task_environment.FastForwardBy(kPenaltyDuration - kSamplingInterval * 2);
    const wtf_size_t new_callback_count = original_callback_count + 1U;
    ASSERT_EQ(pressure_records.size(), new_callback_count);

    // Verify that the update sent is the second one, not the first.
    // We compare strings to make the output easier to compare in case of error.
    EXPECT_EQ(pressure_records.back()->state().AsString(),
              V8PressureState(V8PressureState::Enum::kFair).AsString());
    // This update was sent after fast-forwarding by 100ms once and by 200ms 5
    // times.
    EXPECT_EQ(pressure_records.back()->time(),
              (kSmallInterval + (5 * kSamplingInterval)).InMilliseconds());
  }

  // Check that the rate obfuscation mitigation has been reset and not in place
  // anymore.
  {
    const wtf_size_t original_callback_count = pressure_records.size();

    // Send an update and verify it has been delivered with no delay again.
    pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
        device::mojom::blink::PressureSource::kCpu, PressureState::kSerious,
        base::TimeTicks::Now()));
    task_environment.FastForwardBy(base::Milliseconds(0));
    EXPECT_EQ(pressure_records.size(), original_callback_count + 1U);
    EXPECT_EQ(pressure_records.back()->state().AsString(),
              V8PressureState(V8PressureState::Enum::kSerious).AsString());
  }

  observer->disconnect();
}

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
      device::mojom::blink::PressureSource::kCpu, PressureState::kCritical,
      base::TimeTicks::Now()));

  callback_run_loop.Run();

  // Second update triggering the penalty.
  task_environment.FastForwardBy(kDelayTime);
  pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
      device::mojom::blink::PressureSource::kCpu, PressureState::kNominal,
      base::TimeTicks::Now()));
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
      device::mojom::blink::PressureSource::kCpu, PressureState::kNominal,
      base::TimeTicks::Now()));

  callback_run_loop.Run();

  // Second update triggering the penalty.
  task_environment.FastForwardBy(kDelayTime);
  pressure_service.SendUpdate(device::mojom::blink::PressureUpdate::New(
      device::mojom::blink::PressureSource::kCpu, PressureState::kCritical,
      base::TimeTicks::Now()));
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
