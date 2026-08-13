// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature.h"

#import <limits>
#import <memory>

#import "base/time/time.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
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

class MockObserver : public WebPerformanceMetricsTabHelper::Observer {
 public:
  MOCK_METHOD(void,
              OnFirstContentfulPaint,
              (WebPerformanceMetricsTabHelper * tab_helper,
               double absolute_first_contentful_paint),
              (override));
};

// Test that observers registered with WebPerformanceMetricsTabHelper receive
// notifications when the aggregate First Contentful Paint is updated.
TEST_F(WebPerformanceMetricsJavaScriptFeatureTest, TabHelperObserver) {
  auto web_state = std::make_unique<web::FakeWebState>();
  WebPerformanceMetricsTabHelper::CreateForWebState(web_state.get());
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(web_state.get());
  ASSERT_THAT(tab_helper, testing::NotNull());

  MockObserver observer;
  tab_helper->AddObserver(&observer);

  EXPECT_CALL(observer, OnFirstContentfulPaint(tab_helper, 200.0));
  tab_helper->SetAggregateAbsoluteFirstContentfulPaint(200.0);

  WebPerformanceMetricsJavaScriptFeature* feature =
      WebPerformanceMetricsJavaScriptFeature::GetInstance();
  EXPECT_CALL(observer, OnFirstContentfulPaint(tab_helper, 150.0)).Times(1);
  feature->LogAggregateFirstContentfulPaint(web_state.get(), 100.0, 50.0, true);

  tab_helper->RemoveObserver(&observer);
}

// Test that LogAggregateFirstContentfulPaint updates the stored aggregate
// FCP time when a main frame reports an earlier absolute paint time.
TEST_F(WebPerformanceMetricsJavaScriptFeatureTest,
       LogAggregateFirstContentfulPaintUpdatesAggregate) {
  auto web_state = std::make_unique<web::FakeWebState>();
  WebPerformanceMetricsTabHelper::CreateForWebState(web_state.get());
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(web_state.get());
  ASSERT_THAT(tab_helper, testing::NotNull());

  // Set initial aggregate
  tab_helper->SetAggregateAbsoluteFirstContentfulPaint(200.0);

  WebPerformanceMetricsJavaScriptFeature* feature =
      WebPerformanceMetricsJavaScriptFeature::GetInstance();

  // Main frame reports FCP earlier than current aggregate (150 < 200)
  // navigation_start = 100, relative_fcp = 50 -> absolute_fcp = 150
  feature->LogAggregateFirstContentfulPaint(web_state.get(), 100.0, 50.0, true);

  EXPECT_DOUBLE_EQ(tab_helper->GetAggregateAbsoluteFirstContentfulPaint(),
                   150.0);

  // Main frame reports FCP later than current aggregate (180 > 150)
  // navigation_start = 100, relative_fcp = 80 -> absolute_fcp = 180
  feature->LogAggregateFirstContentfulPaint(web_state.get(), 100.0, 80.0, true);

  // Should NOT be updated because 180 > 150
  EXPECT_DOUBLE_EQ(tab_helper->GetAggregateAbsoluteFirstContentfulPaint(),
                   150.0);
}
