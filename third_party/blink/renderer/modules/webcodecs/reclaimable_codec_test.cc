// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_gauge.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// Set a high theshold, so we can fake pressure threshold notifications.
static constexpr size_t kTestPressureThreshold = 100;

namespace {

constexpr base::TimeDelta kTimerPeriod =
    ReclaimableCodec::kInactivityReclamationThreshold / 2;

class FakeReclaimableCodec final
    : public GarbageCollected<FakeReclaimableCodec>,
      public ReclaimableCodec {
 public:
  FakeReclaimableCodec(ReclaimableCodec::CodecType type,
                       ExecutionContext* context)
      : ReclaimableCodec(type, context) {}

  void SimulateActivity() {
    MarkCodecActive();
    reclaimed_ = false;
  }

  void SimulateReset() { ReleaseCodecPressure(); }

  void SimulatePressureExceeded() {
    ApplyCodecPressure();
    SetGlobalPressureExceededFlag(true);
  }

  void OnCodecReclaimed(DOMException* ex) final { reclaimed_ = true; }

  bool is_global_pressure_exceeded() {
    return global_pressure_exceeded_for_testing();
  }

  // GarbageCollected override.
  void Trace(Visitor* visitor) const override {
    ReclaimableCodec::Trace(visitor);
  }

  bool reclaimed() const { return reclaimed_; }

 private:
  // ContextLifecycleObserver override.
  void ContextDestroyed() override {}

  bool reclaimed_ = false;
};

}  // namespace

class BaseReclaimableCodecTest
    : public testing::TestWithParam<ReclaimableCodec::CodecType> {
 public:
  FakeReclaimableCodec* CreateCodec(ExecutionContext* context) {
    if (!is_gauge_threshold_set_) {
      CodecPressureGauge::GetInstance(GetParam())
          .set_pressure_threshold_for_testing(kTestPressureThreshold);

      is_gauge_threshold_set_ = true;
    }

    return MakeGarbageCollected<FakeReclaimableCodec>(GetParam(), context);
  }

 private:
  bool is_gauge_threshold_set_ = false;
};

// Testing w/ flags allowing only reclamation of background codecs.
class ReclaimBackgroundOnlyTest : public BaseReclaimableCodecTest {
 public:
  ReclaimBackgroundOnlyTest() {
    std::vector<base::test::FeatureRef> enabled_features{
        kReclaimInactiveWebCodecs, kOnlyReclaimBackgroundWebCodecs};
    std::vector<base::test::FeatureRef> disabled_features{};
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Testing w/ flags allowing reclamation of both foreground and background
// codecs.
class ReclaimForegroundSameAsBackgroundTest : public BaseReclaimableCodecTest {
 public:
  ReclaimForegroundSameAsBackgroundTest() {
    std::vector<base::test::FeatureRef> enabled_features{
        kReclaimInactiveWebCodecs};
    std::vector<base::test::FeatureRef> disabled_features{
        kOnlyReclaimBackgroundWebCodecs};
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Testing kill-switch scenario w/ all flags disabled.
class ReclaimDisabledTest : public BaseReclaimableCodecTest {
 public:
  ReclaimDisabledTest() {
    std::vector<base::test::FeatureRef> enabled_features{};
    std::vector<base::test::FeatureRef> disabled_features{
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

void TestBackgroundInactivityTimerStartStops(TestParam background_type,
                                             FakeReclaimableCodec* codec) {
  if (background_type == TestParam::kSimulateBackgrounding) {
    EXPECT_FALSE(codec->is_backgrounded_for_testing());
    codec->SimulateLifecycleStateForTesting(
        scheduler::SchedulingLifecycleState::kHidden);
  } else {
    DCHECK_EQ(background_type, TestParam::kExpectAlreadyInBackground);
    EXPECT_TRUE(codec->is_backgrounded_for_testing());
  }

  // Codecs should not be reclaimable for inactivity until pressure is exceeded.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  codec->SimulateReset();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // Exceeding pressure should start the timer.
  codec->SimulatePressureExceeded();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());

  // Activity should not stop the timer.
  codec->SimulateActivity();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());

  // The timer should be stopped when asked.
  codec->SimulateReset();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // It should be possible to restart the timer after stopping it.
  codec->SimulatePressureExceeded();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
}

void TestBackgroundInactivityTimerWorks(TestParam background_type,
                                        FakeReclaimableCodec* codec) {
  if (background_type == TestParam::kSimulateBackgrounding) {
    EXPECT_FALSE(codec->is_backgrounded_for_testing());
    codec->SimulateLifecycleStateForTesting(
        scheduler::SchedulingLifecycleState::kHidden);
  } else {
    DCHECK_EQ(background_type, TestParam::kExpectAlreadyInBackground);
    EXPECT_TRUE(codec->is_backgrounded_for_testing());
  }

  // Codecs should not be reclaimable for inactivity until pressure is exceeded.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // Exceeding pressure should start the timer.
  codec->SimulatePressureExceeded();
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->reclaimed());

  // Fire when codec is fresh to ensure first tick isn't treated as idle.
  codec->SimulateActivity();
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // One timer period should not be enough to reclaim the codec.
  tick_clock.Advance(kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // Advancing an additional timer period should be enough to trigger
  // reclamation.
  tick_clock.Advance(kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

TEST_P(ReclaimBackgroundOnlyTest, BackgroundInactivityTimerStartStops) {
  V8TestingScope v8_scope;

  // Only background reclamation permitted, so simulate backgrouding.
  TestBackgroundInactivityTimerStartStops(
      TestParam::kSimulateBackgrounding,
      CreateCodec(v8_scope.GetExecutionContext()));
}

TEST_P(ReclaimBackgroundOnlyTest, BackgroundInactivityTimerWorks) {
  V8TestingScope v8_scope;

  // Only background reclamation permitted, so simulate backgrouding.
  TestBackgroundInactivityTimerWorks(
      TestParam::kSimulateBackgrounding,
      CreateCodec(v8_scope.GetExecutionContext()));
}

TEST_P(ReclaimForegroundSameAsBackgroundTest,
       BackgroundInactivityTimerStartStops) {
  V8TestingScope v8_scope;

  // Foreground codecs are treated as always backgrounded w/ these feature flags
  TestBackgroundInactivityTimerStartStops(
      TestParam::kExpectAlreadyInBackground,
      CreateCodec(v8_scope.GetExecutionContext()));
}

TEST_P(ReclaimForegroundSameAsBackgroundTest, BackgroundInactivityTimerWorks) {
  V8TestingScope v8_scope;

  // Foreground codecs are treated as always backgrounded w/ these feature flags
  TestBackgroundInactivityTimerWorks(
      TestParam::kExpectAlreadyInBackground,
      CreateCodec(v8_scope.GetExecutionContext()));
}

TEST_P(ReclaimBackgroundOnlyTest, ForegroundInactivityTimerNeverStarts) {
  V8TestingScope v8_scope;

  auto* codec = CreateCodec(v8_scope.GetExecutionContext());

  // Test codec should start in foreground when kOnlyReclaimBackgroundWebCodecs
  // enabled.
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Codecs should not be reclaimable for inactivity until pressure is exceeded.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // Exceeded pressure should not start timer while we remain in foreground.
  codec->SimulatePressureExceeded();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // First activity should not start timer while we remain in foreground.
  codec->SimulateActivity();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing time by any amount shouldn't change the above.
  tick_clock.Advance(kTimerPeriod * 100);
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

TEST_P(ReclaimBackgroundOnlyTest, ForegroundCodecReclaimedOnceBackgrounded) {
  V8TestingScope v8_scope;

  auto* codec = CreateCodec(v8_scope.GetExecutionContext());

  // Test codec should start in foreground when kOnlyReclaimBackgroundWebCodecs
  // enabled.
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Codecs should not be reclaimable for inactivity until pressure is exceeded.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // Pressure should not start the timer while we are still in the foreground.
  codec->SimulatePressureExceeded();
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
  tick_clock.Advance(kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_FALSE(codec->reclaimed());

  // Re-entering foreground should stop the timer.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing any amount of time shouldn't reclaim while in foreground.
  tick_clock.Advance(kTimerPeriod * 100);
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
  tick_clock.Advance(kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing twice through the period should finally reclaim.
  tick_clock.Advance(kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_TRUE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

TEST_P(ReclaimBackgroundOnlyTest, RepeatLifecycleEventsDontBreakState) {
  V8TestingScope v8_scope;

  auto* codec = CreateCodec(v8_scope.GetExecutionContext());

  // Test codec should start in foreground when kOnlyReclaimBackgroundWebCodecs
  // enabled.
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Duplicate kNotThrottled (foreground) shouldn't affect codec state.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Codecs should not be reclaimable until pressure is exceeded.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // Applying pressure should not start the timer while we remain in the
  // foreground.
  codec->SimulatePressureExceeded();
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
  tick_clock.Advance(kTimerPeriod);
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
  tick_clock.Advance(kTimerPeriod);
  codec->SimulateActivityTimerFiredForTesting();
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_TRUE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

TEST_P(ReclaimDisabledTest, ReclamationKillSwitch) {
  V8TestingScope v8_scope;

  auto* codec = CreateCodec(v8_scope.GetExecutionContext());

  // Test codec should start in background when kOnlyReclaimBackgroundWebCodecs
  // is disabled.
  EXPECT_TRUE(codec->is_backgrounded_for_testing());

  // Codecs should not be reclaimable until pressure is exceeded.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  base::SimpleTestTickClock tick_clock;
  codec->set_tick_clock_for_testing(&tick_clock);

  // Reclamation disabled, so pressure should not start the timer.
  codec->SimulatePressureExceeded();
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());
  EXPECT_FALSE(codec->reclaimed());

  // Advancing any period should not be enough to reclaim the codec.
  tick_clock.Advance(kTimerPeriod * 10);
  EXPECT_FALSE(codec->reclaimed());

  // Restore default tick clock since |codec| is a garbage collected object that
  // may outlive the scope of this function.
  codec->set_tick_clock_for_testing(base::DefaultTickClock::GetInstance());
}

TEST_P(ReclaimBackgroundOnlyTest, PressureChangesUpdateTimer) {
  V8TestingScope v8_scope;

  auto* codec = CreateCodec(v8_scope.GetExecutionContext());

  // Test codec should start in foreground when kOnlyReclaimBackgroundWebCodecs
  // enabled.
  EXPECT_FALSE(codec->is_backgrounded_for_testing());

  // Codecs should not apply pressure by default.
  EXPECT_FALSE(codec->is_applying_codec_pressure());

  // Codecs should not be reclaimable by default.
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // Pressure must be exceeded for the timer to be active.
  codec->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kHidden);
  EXPECT_TRUE(codec->is_backgrounded_for_testing());
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // Applying pressure isn't enough to start reclamation, global pressure must
  // be exceeded.
  codec->ApplyCodecPressure();
  EXPECT_TRUE(codec->is_applying_codec_pressure());
  EXPECT_FALSE(codec->is_global_pressure_exceeded());
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // Setting/unsetting global pressure should start/stop idle reclamation.
  codec->SetGlobalPressureExceededFlag(true);
  EXPECT_TRUE(codec->is_applying_codec_pressure());
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());

  codec->SetGlobalPressureExceededFlag(false);
  EXPECT_TRUE(codec->is_applying_codec_pressure());
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  codec->SetGlobalPressureExceededFlag(true);
  EXPECT_TRUE(codec->is_applying_codec_pressure());
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());

  // Releasing codec pressure should stop the timer.
  codec->ReleaseCodecPressure();
  EXPECT_FALSE(codec->is_applying_codec_pressure());
  EXPECT_FALSE(codec->is_global_pressure_exceeded());
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  // Re-applying codec pressure should not start the timer: the global pressure
  // flag must be set again.
  codec->ApplyCodecPressure();
  EXPECT_TRUE(codec->is_applying_codec_pressure());
  EXPECT_FALSE(codec->IsReclamationTimerActiveForTesting());

  codec->SetGlobalPressureExceededFlag(true);
  EXPECT_TRUE(codec->is_applying_codec_pressure());
  EXPECT_TRUE(codec->IsReclamationTimerActiveForTesting());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ReclaimBackgroundOnlyTest,
    testing::Values(ReclaimableCodec::CodecType::kDecoder,
                    ReclaimableCodec::CodecType::kEncoder));

INSTANTIATE_TEST_SUITE_P(
    ,
    ReclaimForegroundSameAsBackgroundTest,
    testing::Values(ReclaimableCodec::CodecType::kDecoder,
                    ReclaimableCodec::CodecType::kEncoder));

INSTANTIATE_TEST_SUITE_P(
    ,
    ReclaimDisabledTest,
    testing::Values(ReclaimableCodec::CodecType::kDecoder,
                    ReclaimableCodec::CodecType::kEncoder));

}  // namespace blink
