// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"

#import <limits>
#import <map>
#import <string>
#import <vector>

#import "base/time/time.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using WebPerformanceMetricsJavaScriptFeatureUtilTest = PlatformTest;

namespace {

struct TestParams {
  web_performance_metrics::FirstContentfulPaint frame;
  bool is_main_frame;
};

struct TestCase {
  TestParams params;
  base::TimeDelta expected;
};

// Iterates over the test cases and validates the results against the
// expected value for Aggregate First Contentful Paint test cases.
template <int N>
void ValidateAggregateFirstContentfulPaintTestCases(
    const TestCase (&test_cases)[N]) {
  double absolute_aggregate_first_contentful_paint =
      std::numeric_limits<double>::max();

  // Stores the first subframe's absolute first contentful paint
  // and uses it in the calculation of the aggregate first
  // contentful paint which occurs on the appearance of the
  // main frame.
  for (const TestCase& test_case : test_cases) {
    if (test_case.params.is_main_frame) {
      base::TimeDelta aggregate_first_contentful_paint =
          web_performance_metrics::CalculateAggregateFirstContentfulPaint(
              absolute_aggregate_first_contentful_paint,
              test_case.params.frame);
      EXPECT_EQ(aggregate_first_contentful_paint, test_case.expected);
    } else if (absolute_aggregate_first_contentful_paint ==
               std::numeric_limits<double>::max()) {
      absolute_aggregate_first_contentful_paint =
          test_case.params.frame.absolute_time;
    }
  }
}

}  // namespace

// Tests the calculation when a subframe loads before the main frame
// and has a faster first contentful paint.
TEST_F(WebPerformanceMetricsJavaScriptFeatureUtilTest,
       AggregateFirstContentfulPaintWithSubframeLoadingFirst) {
  static const TestCase kTestCases[] = {
      {{{160, 10, 170}, false}, base::TimeDelta::Max()},
      {{{150, 30, 180}, true}, base::Milliseconds(20)}};
  ValidateAggregateFirstContentfulPaintTestCases(kTestCases);
}

// Tests the calculation when the main frame loads before the subframe
// and has a faster first contentful paint.
TEST_F(WebPerformanceMetricsJavaScriptFeatureUtilTest,
       AggregateFirstContentfulPaintWithMainFrameLoadingFirst) {
  static const TestCase kTestCases[] = {
      {{{100, 40, 140}, true}, base::Milliseconds(40)},
      {{{250, 50, 300}, false}, base::TimeDelta::Max()}};
  ValidateAggregateFirstContentfulPaintTestCases(kTestCases);
}

// Tests the calculation when only the main frame is present.
TEST_F(WebPerformanceMetricsJavaScriptFeatureUtilTest,
       AggregateFirstContentfulPaintWithOnlyMainFrame) {
  static const TestCase kTestCases[] = {
      {{{100, 40, 140}, true}, base::Milliseconds(40)}};
  ValidateAggregateFirstContentfulPaintTestCases(kTestCases);
}

// Tests the function responsible for calculating the
// absolute first contentful paint time.
TEST_F(WebPerformanceMetricsJavaScriptFeatureUtilTest,
       AbsoluteFirstContentfulPaint) {
  static const TestCase kTestCases[] = {
      {{{100, 40, 140}, true}, base::Milliseconds(140)},
      {{{120, 30, 150}, true}, base::Milliseconds(150)},
      {{{0, 90, 90}, true}, base::Milliseconds(90)},
      {{{100, 220, 320}, true}, base::Milliseconds(320)}};

  for (const TestCase& test_case : kTestCases) {
    base::TimeDelta result = base::Milliseconds(
        web_performance_metrics::CalculateAbsoluteFirstContentfulPaint(
            test_case.params.frame.navigation_start_time,
            test_case.params.frame.relative_time));
    EXPECT_EQ(result, test_case.expected);
  }
}

// Tests that CalculateInteractionToNextPaint correctly calculates the 98th
// percentile INP duration for a single list of durations.
TEST_F(WebPerformanceMetricsJavaScriptFeatureUtilTest,
       CalculateInteractionToNextPaint) {
  // Empty durations -> std::nullopt.
  EXPECT_FALSE(web_performance_metrics::CalculateInteractionToNextPaint({}, 0)
                   .has_value());

  // Single duration.
  std::optional<base::TimeDelta> single =
      web_performance_metrics::CalculateInteractionToNextPaint(
          {base::Milliseconds(150)}, 1);
  ASSERT_TRUE(single.has_value());
  EXPECT_EQ(single.value(), base::Milliseconds(150));

  // 100 interactions with 2 outliers (500ms, 400ms) and base duration 80ms.
  // outlier count = floor(100 / 50) = 2 -> index 2.
  std::optional<base::TimeDelta> with_outliers =
      web_performance_metrics::CalculateInteractionToNextPaint(
          {base::Milliseconds(500), base::Milliseconds(400),
           base::Milliseconds(80)},
          100);
  ASSERT_TRUE(with_outliers.has_value());
  EXPECT_EQ(with_outliers.value(), base::Milliseconds(80));
}

// Tests that CalculateAggregateInteractionToNextPaint correctly merges multiple
// frames and calculates the page-wide 98th percentile INP duration.
TEST_F(WebPerformanceMetricsJavaScriptFeatureUtilTest,
       CalculateAggregateInteractionToNextPaint) {
  // Empty frame map -> std::nullopt.
  EXPECT_FALSE(
      web_performance_metrics::CalculateAggregateInteractionToNextPaint({})
          .has_value());

  std::map<std::string, web_performance_metrics::FrameInteractionData>
      frame_map;
  frame_map["main"] = {
      .longest_durations = {base::Milliseconds(60)},
      .interaction_count = 100,
      .is_main_frame = true,
  };
  frame_map["sub1"] = {
      .longest_durations = {base::Milliseconds(300), base::Milliseconds(250)},
      .interaction_count = 2,
      .is_main_frame = false,
  };

  // Total count = 102 -> outlier count = floor(102 / 50) = 2.
  // Merged: {300ms, 250ms, 60ms}. Index 2 -> 60ms.
  std::optional<base::TimeDelta> result =
      web_performance_metrics::CalculateAggregateInteractionToNextPaint(
          frame_map);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), base::Milliseconds(60));
}
