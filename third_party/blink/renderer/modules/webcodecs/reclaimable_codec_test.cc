// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/default_tick_clock.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class FakeReclaimableCodec final
    : public GarbageCollected<FakeReclaimableCodec>,
      public ReclaimableCodec {
 public:
  explicit FakeReclaimableCodec(ExecutionContext* context)
      : ReclaimableCodec(context) {}

  void SimulateActivity() {
    MarkCodecActive();
    reclaimed_ = false;
  }

  void SimulateReset() { PauseCodecReclamation(); }

  void OnCodecReclaimed(DOMException* ex) final { reclaimed_ = true; }

  // GarbageCollected override.
  void Trace(Visitor* visitor) const override {
    ReclaimableCodec::Trace(visitor);
  }

  bool reclaimed() const { return reclaimed_; }

 private:
  bool reclaimed_ = false;
};

}  // namespace

// Testing w/ flags allowing only reclamation of background codecs.
class ReclaimBackgroundOnlyTest : public testing::Test {
 public:
  ReclaimBackgroundOnlyTest() {
    std::vector<base::Feature> enabled_features{
        kReclaimInactiveWebCodecs, kOnlyReclaimBackgroundWebCodecs};
    std::vector<base::Feature> disabled_features{};
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Testing w/ flags allowing reclamation of both foreground and background
// codecs.
class ReclaimForegroundSameAsBackgroundTest : public testing::Test {
 public:
  ReclaimForegroundSameAsBackgroundTest() {
    std::vector<base::Feature> enabled_features{kReclaimInactiveWebCodecs};
    std::vector<base::Feature> disabled_features{
        kOnlyReclaimBackgroundWebCodecs};
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Testing kill-switch scenario w/ all flags disabled.
class ReclaimDisabledTest : public testing::Test {
 public:
  ReclaimDisabledTest() {
    std::vector<base::Feature> enabled_features{};
    std::vector<base::Feature> disabled_features{
        kReclaimInactiveWebCodecs, kOnlyReclaimBackgroundWebCodecs};
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

enum class TestParam {
  // Instructs test to use SimulateLifecycleStateForTesting(kHidden) to simulate
  // a backgrounding scenario.
  kSimulateBackgrounding,
  // Instructs test to expect that codec is already backgrounded and to refrain
  // from simulating backgrounding as above.
  kExpectAlreadyInBackground,
};

void TestBackgroundInactivityTimerStartStops(TestParam background_type) {
  V8TestingScope v8_scope;

  auto* codec = MakeGarbageCollected<FakeReclaimableCodec>(
      v8_scope.GetExecutionContext());

  if (background_type == TestParam::kSimulateBackgrounding) {
    EXPECT_FALSE(codec->is_backgrounded_for_testing());
    codec->SimulateLifecycleStateForTesting(
        scheduler::SchedulingLifecycleState::kHidden);
  } else {
    DCHECK_EQ(background_type, TestParam::kExpectAlreadyInBackground);
    EXPECT_TRUE(codec->is_backgrounded_for_testing());
  }

  // Codecs should not be reclaimable for inactivity until first activity.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  codec->SimulateReset();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // Activity should start the timer.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());

  // More activity should not stop the timer.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());

  // The timer should be stopped when asked.
  codec->SimulateReset();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // It should be possible to restart the timer after stopping it.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
}

void TestBackgroundInactivityTimerWorks(TestParam background_type) {
  V8TestingScope v8_scope;

  auto* codec = MakeGarbageCollected<FakeReclaimableCodec>(
      v8_scope.GetExecutionContext());

  if (background_type == TestParam::kSimulateBackgrounding) {
    EXPECT_FALSE(codec->is_backgrounded_for_testing());
    codec->SimulateLifecycleStateForTesting(
        scheduler::SchedulingLifecycleState::kHidden);
  } else {
    DCHECK_EQ(background_type, TestParam::kExpectAlreadyInBackground);
    EXPECT_TRUE(codec->is_backgrounded_for_testing());
  }

  // Codecs should not be reclaimable for inactivity until first activity.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // Activity should start the timer.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->reclaimed());

  // Fire when codec is fresh to ensure first tick isn't treated as idle.
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // One timer period should not be enough to reclaim the codec.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // Advancing an additional timer period should be enough to trigger
  // reclamation.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

TEST_F(ReclaimBackgroundOnlyTest, BackgroundInactivityTimerStartStops) {
  // Only background reclamation permitted, so simulate backgrouding.
  TestBackgroundInactivityTimerStartStops(TestParam::kSimulateBackgrounding);
}

TEST_F(ReclaimBackgroundOnlyTest, BackgroundInactivityTimerWorks) {
  // Only background reclamation permitted, so simulate backgrouding.
  TestBackgroundInactivityTimerWorks(TestParam::kSimulateBackgrounding);
}

TEST_F(ReclaimForegroundSameAsBackgroundTest,
       BackgroundInactivityTimerStartStops) {
  // Foreground codecs are treated as always backgrounded w/ these feature flags
  TestBackgroundInactivityTimerStartStops(
      TestParam::kExpectAlreadyInBackground);
}

TEST_F(ReclaimForegroundSameAsBackgroundTest, BackgroundInactivityTimerWorks) {
  // Foreground codecs are treated as always backgrounded w/ these feature flags
  TestBackgroundInactivityTimerWorks(TestParam::kExpectAlreadyInBackground);
}

TEST_F(ReclaimBackgroundOnlyTest, ForegroundInactivityTimerNeverStarts) {
  V8TestingScope v8_scope;

  auto* codec = MakeGarbageCollected<FakeReclaimableCodec>(
      v8_scope.GetExecutionContext());

  // Test codec should start in foreground when kOnlyReclaimBackgroundWebCodecs
  // enabled.
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Codecs should not be reclaimable for inactivity until first activity.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // First activity should not start timer while we remain in foreground.
  codec->SimulateActivity();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing time by any amount shouldn't change the above.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod * 100);
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Activity still shouldn't start the timer as we remain in foreground.
  codec->SimulateActivity();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

TEST_F(ReclaimBackgroundOnlyTest, ForegroundCodecReclaimedOnceBackgrounded) {
  V8TestingScope v8_scope;

  auto* codec = MakeGarbageCollected<FakeReclaimableCodec>(
      v8_scope.GetExecutionContext());

  // Test codec should start in foreground when kOnlyReclaimBackgroundWebCodecs
  // enabled.
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Codecs should not be reclaimable for inactivity until first activity.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // First activity should not start timer while we remain in foreground.
  codec->SimulateActivity();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Entering background should start timer.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kHidden);
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing 1 period shouldn't reclaim (it takes 2).
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // Re-entering foreground should stop the timer.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing any amount of time shouldn't reclaim while in foreground.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod * 100);
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Re-entering background should again start the timer.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kHidden);
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Fire newly backgrounded to ensure first tick isn't treated as idle.
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // Timer should be fresh such that one period is not enough to reclaim.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing twice through the period should finally reclaim.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_TRUE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

TEST_F(ReclaimBackgroundOnlyTest, RepeatLifecycleEventsDontBreakState) {
  V8TestingScope v8_scope;

  auto* codec = MakeGarbageCollected<FakeReclaimableCodec>(
      v8_scope.GetExecutionContext());

  // Test codec should start in foreground when kOnlyReclaimBackgroundWebCodecs
  // enabled.
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Duplicate kNotThrottled (foreground) shouldn't affect codec state.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Codecs should not be reclaimable for inactivity until first activity.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // First activity should not start timer while we remain in foreground.
  codec->SimulateActivity();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Entering background should start timer.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kHidden);
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing 1 period shouldn't reclaim (it takes 2).
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // Further background lifecycle progression shouldn't affect codec state.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kThrottled);
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Further background lifecycle progression shouldn't affect codec state.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kStopped);
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing one final time through the period should finally reclaim.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_TRUE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

TEST_F(ReclaimDisabledTest, ReclamationKillSwitch) {
  V8TestingScope v8_scope;

  auto* codec = MakeGarbageCollected<FakeReclaimableCodec>(
      v8_scope.GetExecutionContext());

  // Test codec should start in background when kOnlyReclaimBackgroundWebCodecs
  // is disabled.
  EXPECT_TRUE(codec->is_backgrounded_for_testing());

  // Codecs should not be reclaimable for inactivity until first activity.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // Reclamation disabled, so activity should not start the timer.
  codec->SimulateActivity();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing any period should not be enough to reclaim the codec.
  tick_clock.Advance(ReclaimableCodec::kTimerPeriod * 10);
  EXPECT_FALSE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

}  // namespace blink
