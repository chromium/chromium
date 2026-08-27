// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using WebPerformanceMetricsTabHelperTest = PlatformTest;

// Tests that Interaction to Next Paint is buffered in TabHelper and flushed
// exactly once on the subsequent navigation start.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_DeferredFlushOnNavigation) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  WebPerformanceMetricsTabHelper::CreateForWebState(&fake_web_state);
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(&fake_web_state);

  // Simulate first INP recording (e.g. from tab switch).
  tab_helper->SetFrameInteractionData("main_frame", {base::Milliseconds(80)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/true);
  tab_helper->SetFrameInteractionData("sub_frame_1", {base::Milliseconds(50)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/false);

  // Simulate updated INP recording for main frame (e.g. user returned and
  // interacted more).
  tab_helper->SetFrameInteractionData("main_frame", {base::Milliseconds(120)},
                                      /*interaction_count=*/2,
                                      /*is_main_frame=*/true);

  // No histogram sample should be recorded prior to navigation completion.
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 0);

  // Simulate next navigation starting to log the metric.
  web::FakeNavigationContext navigation_context;
  fake_web_state.OnNavigationStarted(&navigation_context);

  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 120,
      1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 120,
      1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 50, 1);
}

// Tests that Interaction to Next Paint is flushed when WebState is destroyed.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_FlushOnWebStateDestroyed) {
  base::HistogramTester histogram_tester;
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  WebPerformanceMetricsTabHelper::CreateForWebState(fake_web_state.get());
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(fake_web_state.get());

  tab_helper->SetFrameInteractionData("main_frame", {base::Milliseconds(95)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/true);
  tab_helper->SetFrameInteractionData("sub_frame_1", {base::Milliseconds(60)},
                                      /*interaction_count=*/1,
                                      /*is_main_frame=*/false);

  // Destroy WebState (tab closure).
  fake_web_state.reset();

  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 95,
      1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 95,
      1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 60, 1);
}

// Tests that the aggregate INP correctly merges multiple frames and discards
// high-percentile outliers across the entire page.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_AggregateHighPercentileAcrossFrames) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  WebPerformanceMetricsTabHelper::CreateForWebState(&fake_web_state);
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(&fake_web_state);

  // Main frame has 100 interactions with worst latency 45ms.
  tab_helper->SetFrameInteractionData("main_frame", {base::Milliseconds(45)},
                                      /*interaction_count=*/100,
                                      /*is_main_frame=*/true);

  // Subframe has 2 interactions: 300ms and 200ms.
  tab_helper->SetFrameInteractionData(
      "sub_frame", {base::Milliseconds(300), base::Milliseconds(200)},
      /*interaction_count=*/2, /*is_main_frame=*/false);

  // Flush the tab helper to verify that the message was recorded.
  tab_helper->FlushInteractionToNextPaintMetrics();

  // Total interactions = 102 -> outlier count = floor(102 / 50) = 2.
  // Merged worst durations = {300ms, 200ms, 45ms}.
  // Index 2 is selected -> 45ms for aggregate page INP!
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 45,
      1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 45,
      1);
  histogram_tester.ExpectUniqueSample(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 300,
      1);
}

// Tests that when no user interactions occurred on the page, no aggregate
// histogram sample is recorded on navigation start.
TEST_F(WebPerformanceMetricsTabHelperTest,
       InteractionToNextPaint_NoInteraction_NoFlush) {
  base::HistogramTester histogram_tester;
  web::FakeWebState fake_web_state;
  WebPerformanceMetricsTabHelper::CreateForWebState(&fake_web_state);

  web::FakeNavigationContext navigation_context;
  fake_web_state.OnNavigationStarted(&navigation_context);

  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kAggregateInteractionToNextPaintHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintMainFrameHistogram, 0);
  histogram_tester.ExpectTotalCount(
      web_performance_metrics::kInteractionToNextPaintSubFrameHistogram, 0);
}
