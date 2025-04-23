// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/battery_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using LoadingScenario = performance_scenarios::LoadingScenario;
using InputScenario = performance_scenarios::InputScenario;

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

}  // namespace
}  // namespace content
