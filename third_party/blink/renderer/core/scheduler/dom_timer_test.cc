// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/dom_timer.h"

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/scheduler/scheduled_action.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"

using testing::DoubleNear;
using testing::ElementsAreArray;
using testing::Matcher;

namespace blink {

namespace {

// The resolution of performance.now is 5us, so the threshold for time
// comparison is 6us to account for rounding errors.
const double kThreshold = 0.006;

class DOMTimerTest : public RenderingTest {
 public:
  DOMTimerTest()
      : RenderingTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  // Expected time between each iterator for setInterval(..., 1) or nested
  // setTimeout(..., 1) are 1, 1, 1, 1, 1, 1, 4, 4, ... as a minimum clamp
  // of 4ms is applied from the 7th iteration onwards.
  const Vector<Matcher<double>> kExpectedTimings = {
      DoubleNear(1., kThreshold), DoubleNear(1., kThreshold),
      DoubleNear(1., kThreshold), DoubleNear(1., kThreshold),
      DoubleNear(1., kThreshold), DoubleNear(1., kThreshold),
      DoubleNear(4., kThreshold), DoubleNear(4., kThreshold),
  };

  void SetUp() override {
    AdvanceClock(base::Seconds(1));
    RenderingTest::SetUp();
    auto* window_performance =
        DOMWindowPerformance::performance(*GetDocument().domWindow());
    auto now_ticks = base::TimeTicks::Now();
    window_performance->ResetTimeOriginForTesting(now_ticks);
    window_performance->SetCrossOriginIsolatedCapabilityForTesting(true);
    GetDocument().GetSettings()->SetScriptEnabled(true);
    auto* loader = GetDocument().Loader();
    loader->GetTiming().SetNavigationStart(now_ticks);
  }

  v8::Local<v8::Value> EvalExpression(const char* expr) {
    return ClassicScript::CreateUnspecifiedScript(expr)
        ->RunScriptAndReturnValue(GetDocument().domWindow())
        .GetSuccessValueOrEmpty();
  }

  Vector<double> ToDoubleArray(v8::Local<v8::Value> value,
                               v8::HandleScope& scope) {
    ScriptState::Scope context_scope(ToScriptStateForMainWorld(&GetFrame()));
    NonThrowableExceptionState exception_state;
    return NativeValueTraits<IDLSequence<IDLDouble>>::NativeValue(
        scope.GetIsolate(), value, exception_state);
  }

  double ToDoubleValue(v8::Local<v8::Value> value, v8::HandleScope& scope) {
    NonThrowableExceptionState exceptionState;
    return ToDouble(scope.GetIsolate(), value, exceptionState);
  }

  void ExecuteScriptAndWaitUntilIdle(const char* script_text) {
    ClassicScript::CreateUnspecifiedScript(String(script_text))
        ->RunScript(GetDocument().domWindow());
    FastForwardUntilNoTasksRemain();
  }
};

const char* const kSetTimeout0ScriptText =
    "var last = performance.now();"
    "var elapsed;"
    "function setTimeoutCallback() {"
    "  var current = performance.now();"
    "  elapsed = current - last;"
    "}"
    "setTimeout(setTimeoutCallback, 0);";

TEST_F(DOMTimerTest, setTimeout_ZeroIsNotClampedToOne) {
  v8::HandleScope scope(GetPage().GetAgentGroupScheduler().Isolate());

  ExecuteScriptAndWaitUntilIdle(kSetTimeout0ScriptText);

  double time = ToDoubleValue(EvalExpression("elapsed"), scope);

  EXPECT_THAT(time, DoubleNear(0., kThreshold));
}

const char* const kSetInterval0ScriptText =
    "var last = performance.now();"
    "var elapsed;"
    "var interval;"
    "function setIntervalCallback() {"
    "  var current = performance.now();"
    "  elapsed = current - last;"
    "  clearInterval(interval);"
    "}"
    "interval = setInterval(setIntervalCallback, 0);";

TEST_F(DOMTimerTest, setInterval_ZeroIsNotClampedToOne) {
  v8::HandleScope scope(GetPage().GetAgentGroupScheduler().Isolate());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSetIntervalWithoutClamp);

  ExecuteScriptAndWaitUntilIdle(kSetInterval0ScriptText);

  double time = ToDoubleValue(EvalExpression("elapsed"), scope);

  EXPECT_THAT(time, DoubleNear(0., kThreshold));
}

TEST_F(DOMTimerTest, setInterval_ZeroIsClampedToOne) {
  v8::HandleScope scope(GetPage().GetAgentGroupScheduler().Isolate());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kSetIntervalWithoutClamp);

  ExecuteScriptAndWaitUntilIdle(kSetInterval0ScriptText);

  double time = ToDoubleValue(EvalExpression("elapsed"), scope);

  EXPECT_THAT(time, DoubleNear(1., kThreshold));
}

const char* const kSetTimeoutNestedScriptText =
    "var last = performance.now();"
    "var times = [];"
    "function nestSetTimeouts() {"
    "  var current = performance.now();"
    "  var elapsed = current - last;"
    "  last = current;"
    "  times.push(elapsed);"
    "  if (times.length < 8) {"
    "    setTimeout(nestSetTimeouts, 1);"
    "  }"
    "}"
    "setTimeout(nestSetTimeouts, 1);";

TEST_F(DOMTimerTest, setTimeout_ClampsAfter6Nestings) {
  v8::HandleScope scope(GetPage().GetAgentGroupScheduler().Isolate());

  ExecuteScriptAndWaitUntilIdle(kSetTimeoutNestedScriptText);

  auto times(ToDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(kExpectedTimings));
}

const char* const kSetIntervalScriptText =
    "var last = performance.now();"
    "var times = [];"
    "var id = setInterval(function() {"
    "  var current = performance.now();"
    "  var elapsed = current - last;"
    "  last = current;"
    "  times.push(elapsed);"
    "  if (times.length > 7) {"
    "    clearInterval(id);"
    "  }"
    "}, 1);";

TEST_F(DOMTimerTest, setInterval_ClampsAfter6Iterations) {
  v8::HandleScope scope(GetPage().GetAgentGroupScheduler().Isolate());

  ExecuteScriptAndWaitUntilIdle(kSetIntervalScriptText);

  auto times(ToDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(kExpectedTimings));
}

TEST_F(DOMTimerTest, setInterval_NestingResetsForLaterCalls) {
  v8::HandleScope scope(GetPage().GetAgentGroupScheduler().Isolate());

  ExecuteScriptAndWaitUntilIdle(kSetIntervalScriptText);

  // Run the setIntervalScript again to verify that the clamp imposed for
  // nesting beyond 4 levels is reset when setInterval is called again in the
  // original scope but after the original setInterval has completed.
  ExecuteScriptAndWaitUntilIdle(kSetIntervalScriptText);

  auto times(ToDoubleArray(EvalExpression("times"), scope));

  EXPECT_THAT(times, ElementsAreArray(kExpectedTimings));
}

// Regression test: Dispose() must cancel the pending task even when action_ is
// already null. During cppgc lazy sweeping, GC phase interactions can leave a
// DOMTimer with a live TimerBase handle but a null action_. If Dispose() does
// not call Stop(), the dangling Unretained(this) closure fires after the
// destructor, causing a use-after-free.
TEST_F(DOMTimerTest, DisposeWithNullActionCancelsTask) {
  v8::HandleScope scope(GetPage().GetAgentGroupScheduler().Isolate());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope script_scope(script_state);

  auto* action = MakeGarbageCollected<ScheduledAction>(
      script_state, *GetDocument().GetExecutionContext(), String("void 0"));
  auto* timer = MakeGarbageCollected<DOMTimer>(
      *GetDocument().GetExecutionContext(), action, base::Milliseconds(50),
      /*single_shot=*/true);
  ASSERT_TRUE(timer->IsActive());

  // Stop clears action_ and cancels the task.
  timer->Stop();
  ASSERT_FALSE(timer->IsActive());

  // Re-arm the timer so there is a pending task while action_ is null.
  timer->StartOneShot(base::Milliseconds(1), FROM_HERE);
  ASSERT_TRUE(timer->IsActive());

  // Simulate the pre-finalizer; must cancel the pending task.
  timer->Dispose();

  // Without the fix the uncancelled closure fires and crashes.
  FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(timer->IsActive());
}

}  // namespace

}  // namespace blink
