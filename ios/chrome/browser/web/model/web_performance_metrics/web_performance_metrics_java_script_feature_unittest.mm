// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "base/values.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

using WebPerformanceMetricsJavaScriptFeatureTest = PlatformTest;

// Tests that ScriptMessageReceived parses an INP message and updates
// the metric.
TEST_F(WebPerformanceMetricsJavaScriptFeatureTest,
       ScriptMessageReceived_InteractionToNextPaint) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  WebPerformanceMetricsTabHelper::CreateForWebState(&fake_web_state);
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(&fake_web_state);

  base::DictValue main_frame_dict;
  main_frame_dict.Set(web_performance_metrics::kMetricKey,
                      web_performance_metrics::kInteractionToNextPaintMetric);
  base::ListValue main_durations;
  main_durations.Append(85.0);
  main_frame_dict.Set(web_performance_metrics::kDurationsKey,
                      std::move(main_durations));
  main_frame_dict.Set(web_performance_metrics::kInteractionCountKey, 1);
  main_frame_dict.Set(web_performance_metrics::kFrameIdKey, "main_1");
  web::ScriptMessage main_frame_message(
      std::make_unique<base::Value>(std::move(main_frame_dict)),
      /*is_user_interacting=*/true, /*is_main_frame=*/true,
      /*request_url=*/GURL("https://chromium.org"),
      url::Origin::Create(GURL("https://chromium.org")));

  WebPerformanceMetricsJavaScriptFeature::GetInstance()->ScriptMessageReceived(
      &fake_web_state, main_frame_message);

  // Directly flush the tab helper to verify that the message was recorded.
  tab_helper->FlushInteractionToNextPaintMetrics();

  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 85,
      1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 85,
      1);
}

// Tests that LogInteractionToNextPaint parses dictionary payloads and records
// interaction timing data for both main frame and subframes.
TEST_F(WebPerformanceMetricsJavaScriptFeatureTest, LogInteractionToNextPaint) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  WebPerformanceMetricsTabHelper::CreateForWebState(&fake_web_state);

  // 1. Direct call with main frame dictionary.
  base::DictValue main_dict;
  base::ListValue main_durations;
  main_durations.Append(110.0);
  main_dict.Set(web_performance_metrics::kDurationsKey,
                std::move(main_durations));
  main_dict.Set(web_performance_metrics::kInteractionCountKey, 1);
  main_dict.Set(web_performance_metrics::kFrameIdKey, "main_frame_1");

  WebPerformanceMetricsJavaScriptFeature::GetInstance()
      ->LogInteractionToNextPaint(&fake_web_state, main_dict,
                                  /*is_main_frame=*/true);

  // 2. Direct call with subframe dictionary.
  base::DictValue sub_dict;
  base::ListValue sub_durations;
  sub_durations.Append(75.0);
  sub_dict.Set(web_performance_metrics::kDurationsKey,
               std::move(sub_durations));
  sub_dict.Set(web_performance_metrics::kInteractionCountKey, 1);
  sub_dict.Set(web_performance_metrics::kFrameIdKey, "sub_frame_1");

  WebPerformanceMetricsJavaScriptFeature::GetInstance()
      ->LogInteractionToNextPaint(&fake_web_state, sub_dict,
                                  /*is_main_frame=*/false);

  // 3. Direct call with empty durations (should be safely ignored).
  base::DictValue empty_dict;
  empty_dict.Set(web_performance_metrics::kDurationsKey, base::ListValue());
  empty_dict.Set(web_performance_metrics::kFrameIdKey, "empty_frame");
  WebPerformanceMetricsJavaScriptFeature::GetInstance()
      ->LogInteractionToNextPaint(&fake_web_state, empty_dict,
                                  /*is_main_frame=*/false);

  // Trigger navigation flush.
  web::FakeNavigationContext navigation_context;
  fake_web_state.OnNavigationStarted(&navigation_context);

  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 110,
      1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 75, 1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 110,
      1);
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
