// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/battery_metrics.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using Delta = AndroidBatteryMetrics::EnergyConsumedTracker::Delta;

bool operator==(const Delta& lhs, const Delta& rhs) {
  return lhs.subsystem == rhs.subsystem &&
         lhs.energy_consumed_mwh == rhs.energy_consumed_mwh;
}

namespace {

using LoadingScenario = performance_scenarios::LoadingScenario;
using InputScenario = performance_scenarios::InputScenario;
using Subsystem = AndroidBatteryMetrics::EnergyConsumedTracker::Subsystem;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(PerformanceScenarioTracker, UnknownScenarioByDefault) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  EXPECT_EQ(tracker.GetMetricSuffix(),
            "UnknownLoadingScenario_UnknownInputScenario");
}

TEST(PerformanceScenarioTracker, UpdateSetsNewScenario) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  tracker.UpdateLoadingScenario(LoadingScenario::kNoPageLoading);
  tracker.UpdateInputScenario(InputScenario::kNoInput);
  EXPECT_EQ(tracker.GetMetricSuffix(), "NoPageLoading_NoInput");
}

TEST(PerformanceScenarioTracker, InputTypingTakesPrecedenceOverNoInput) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  tracker.UpdateInputScenario(InputScenario::kNoInput);
  tracker.UpdateInputScenario(InputScenario::kTyping);
  tracker.UpdateInputScenario(InputScenario::kNoInput);
  EXPECT_EQ(tracker.GetMetricSuffix(), "UnknownLoadingScenario_Typing");
}

TEST(PerformanceScenarioTracker, InputTapTakesPrecedenceOverTyping) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  tracker.UpdateInputScenario(InputScenario::kNoInput);
  tracker.UpdateInputScenario(InputScenario::kTyping);
  tracker.UpdateInputScenario(InputScenario::kTap);
  tracker.UpdateInputScenario(InputScenario::kTyping);
  tracker.UpdateInputScenario(InputScenario::kNoInput);
  EXPECT_EQ(tracker.GetMetricSuffix(), "UnknownLoadingScenario_Tap");
}

TEST(PerformanceScenarioTracker, InputScrollTakesPrecedence) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  tracker.UpdateInputScenario(InputScenario::kNoInput);
  tracker.UpdateInputScenario(InputScenario::kTyping);
  tracker.UpdateInputScenario(InputScenario::kTap);
  tracker.UpdateInputScenario(InputScenario::kScroll);
  tracker.UpdateInputScenario(InputScenario::kTap);
  tracker.UpdateInputScenario(InputScenario::kTyping);
  tracker.UpdateInputScenario(InputScenario::kNoInput);
  EXPECT_EQ(tracker.GetMetricSuffix(), "UnknownLoadingScenario_Scroll");
}

TEST(PerformanceScenarioTracker, FocusedLoadingTakesPrecedence) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  tracker.UpdateLoadingScenario(LoadingScenario::kNoPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kBackgroundPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kVisiblePageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kFocusedPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kVisiblePageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kBackgroundPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kNoPageLoading);
  // No loading -> background loading -> visible loading -> focused loading ->
  // visible loading -> background loading -> no loading. The overall scenario
  // is "FocusedPageLoading".
  EXPECT_EQ(tracker.GetMetricSuffix(),
            "FocusedPageLoading_UnknownInputScenario");
}

TEST(PerformanceScenarioTracker,
     VisibleLoadingTakesPrecedenceOverBackgroundLoading) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  tracker.UpdateLoadingScenario(LoadingScenario::kNoPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kBackgroundPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kVisiblePageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kBackgroundPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kNoPageLoading);
  // No loading -> background loading -> visible loading ->
  // background loading -> no loading. The overall scenario is
  // "VisiblePageLoading".
  EXPECT_EQ(tracker.GetMetricSuffix(),
            "VisiblePageLoading_UnknownInputScenario");
}

TEST(PerformanceScenarioTracker,
     BackgroundLoadingTakesPrecedenceOverNoLoading) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  tracker.UpdateLoadingScenario(LoadingScenario::kNoPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kBackgroundPageLoading);
  tracker.UpdateLoadingScenario(LoadingScenario::kNoPageLoading);
  // No loading -> background loading -> no loading. The overall scenario is
  // "BackgroundPageLoading".
  EXPECT_EQ(tracker.GetMetricSuffix(),
            "BackgroundPageLoading_UnknownInputScenario");
}

TEST(PerformanceScenarioTracker, ResetSetsLatestScenario) {
  AndroidBatteryMetrics::PerformanceScenarioTracker tracker;
  // Visible loading + typing first.
  tracker.UpdateLoadingScenario(LoadingScenario::kVisiblePageLoading);
  tracker.UpdateInputScenario(InputScenario::kTyping);
  // No loading and no input after it.
  tracker.UpdateLoadingScenario(LoadingScenario::kNoPageLoading);
  tracker.UpdateInputScenario(InputScenario::kNoInput);
  // Visible loading and typing take precedence (see Precedence tests above).
  ASSERT_EQ(tracker.GetMetricSuffix(), "VisiblePageLoading_Typing");

  tracker.UseLatestScenarios();
  // Visible loading + typing is no longer in the tracked window.
  EXPECT_EQ(tracker.GetMetricSuffix(), "NoPageLoading_NoInput");
}

TEST(EnergyConsumedTracker, InitiallyEmptyDeltas) {
  AndroidBatteryMetrics::EnergyConsumedTracker tracker;
  // No previous values in the tracker, so no deltas to report.
  EXPECT_THAT(tracker.GetDeltas({{"CPU", 10000}}), IsEmpty());
}

TEST(EnergyConsumedTracker, AllKnownSubsystems) {
  AndroidBatteryMetrics::EnergyConsumedTracker tracker;
  tracker.UpdatePowerMonitorReadings(
      {{"CPU", 3600}, {"GPU", 10 * 3600}, {"DISPLAY", 100 * 3600}});
  EXPECT_THAT(
      tracker.GetDeltas(
          {{"CPU", 2 * 3600}, {"GPU", 20 * 3600}, {"DISPLAY", 200 * 3600}}),
      UnorderedElementsAre(Delta{Subsystem::kCpu, 1},
                           Delta{Subsystem::kGpu, 10},
                           Delta{Subsystem::kDisplay, 100}));
}

TEST(EnergyConsumedTracker, UnknownSubsystemsReportedAsOther) {
  AndroidBatteryMetrics::EnergyConsumedTracker tracker;
  tracker.UpdatePowerMonitorReadings(
      {{"Unknown1", 3600}, {"Unknown2", 10 * 3600}});
  // All deltas from unknown subsystems reported together as "Other".
  EXPECT_THAT(
      tracker.GetDeltas({{"Unknown1", 2 * 3600}, {"Unknown2", 20 * 3600}}),
      UnorderedElementsAre(Delta{Subsystem::kOther, 11}));
}

TEST(EnergyConsumedTracker, SumsUpDifferentCpus) {
  AndroidBatteryMetrics::EnergyConsumedTracker tracker;
  tracker.UpdatePowerMonitorReadings(
      {{"CPU/0", 3600}, {"CPU/1", 10 * 3600}, {"CPU/2", 100 * 3600}});
  EXPECT_THAT(
      tracker.GetDeltas(
          {{"CPU/0", 2 * 3600}, {"CPU/1", 20 * 3600}, {"CPU/2", 200 * 3600}}),
      UnorderedElementsAre(Delta{Subsystem::kCpu, 111}));
}

TEST(EnergyConsumedTracker, DoesNotReportNegativeValues) {
  AndroidBatteryMetrics::EnergyConsumedTracker tracker;
  tracker.UpdatePowerMonitorReadings({{"CPU", 10 * 3600}});
  // The new value is lower than the previous one. The tracker should report 0,
  // not a negative value.
  EXPECT_THAT(tracker.GetDeltas({{"CPU", 3600}}),
              UnorderedElementsAre(Delta{Subsystem::kCpu, 0}));
}

TEST(EnergyConsumedTracker, TreatsZerosAsErrors) {
  AndroidBatteryMetrics::EnergyConsumedTracker tracker;
  tracker.UpdatePowerMonitorReadings({{"CPU", 3600}});
  // 0 value for total energy is an error. The tracker should not report any
  // deltas.
  EXPECT_THAT(tracker.GetDeltas({{"CPU", 0}}), IsEmpty());
}

}  // namespace
}  // namespace content
