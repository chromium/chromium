// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature.h"

#import <limits>

#import "base/time/time.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "testing/platform_test.h"

using WebPerformanceMetricsJavaScriptFeatureTest = PlatformTest;

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
// expected value for Aggregate First Contetnful Paint test caess.
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

// Simulates the event where a subframe loads before the main frame
// and has a faster first contentful paint.
TEST_F(WebPerformanceMetricsJavaScriptFeatureTest,
       AggregateFirstContentfulPaintWithSubframeLoadingFirst) {
  static const TestCase kTestCases[] = {
      {{{160, 10, 170}, false}, base::TimeDelta::Max()},
      {{{150, 30, 180}, true}, base::Milliseconds(20)}};
  ValidateAggregateFirstContentfulPaintTestCases(kTestCases);
}

// Simulates the event where the mainframe loads before the subframe
// and has a faster first contentful paint.
TEST_F(WebPerformanceMetricsJavaScriptFeatureTest,
       AggregateFirstContentfulPaintWithMainFrameLoadingFirst) {
  static const TestCase kTestCases[] = {
      {{{100, 40, 140}, true}, base::Milliseconds(40)},
      {{{250, 50, 300}, false}, base::TimeDelta::Max()}};
  ValidateAggregateFirstContentfulPaintTestCases(kTestCases);
}

// Simulates the event where the mainframe loads before the subframe
// and a slower first contentful paint.
TEST_F(WebPerformanceMetricsJavaScriptFeatureTest,
       AggregateFirstContentfulPaintWithOnlyMainFrame) {
  static const TestCase kTestCases[] = {
      {{{100, 40, 140}, true}, base::Milliseconds(40)}};
  ValidateAggregateFirstContentfulPaintTestCases(kTestCases);
}

// Tests the function responsible for calculating the
// absolute first contentful paint time.
TEST_F(WebPerformanceMetricsJavaScriptFeatureTest,
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
