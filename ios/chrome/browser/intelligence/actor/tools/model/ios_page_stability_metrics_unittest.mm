// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/ios_page_stability_metrics.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/page_content_annotations/core/page_stability_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class IOSPageStabilityMetricsTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

// Test that moving through monitoring, completion, delay, and done records
// the `kStableBeforeMinDelay` outcome and timing histograms.
TEST_F(IOSPageStabilityMetricsTest, LogsStableBeforeMinDelayOutcomeAndTiming) {
  IOSPageStabilityMetrics metrics;
  metrics.Start();

  task_environment_.FastForwardBy(base::Milliseconds(50));
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);

  task_environment_.FastForwardBy(base::Milliseconds(100));
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kMonitorCompleted);

  task_environment_.FastForwardBy(base::Milliseconds(50));
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kDelayCallback);

  task_environment_.FastForwardBy(base::Milliseconds(50));
  metrics.WillMoveToState(page_content_annotations::PageStabilityState::kDone);

  histogram_tester_.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(
          PageStabilityOutcome::kStableBeforeMinDelay),
      1);

  // Total time: 50 + 100 + 50 + 50 = 250ms
  histogram_tester_.ExpectUniqueTimeSample(
      kIOSActorPageStabilityTotalTimeToStableMetricName,
      base::Milliseconds(250), 1);

  // Monitoring to stable time: 100 + 50 + 50 = 200ms
  histogram_tester_.ExpectUniqueTimeSample(
      kIOSActorPageStabilityTimeFromMonitoringToStableMetricName,
      base::Milliseconds(200), 1);
}

// Test that reaching stability without a delay records the `kStable`
// outcome and timing histograms.
TEST_F(IOSPageStabilityMetricsTest, LogsStableOutcomeAndTiming) {
  IOSPageStabilityMetrics metrics;
  metrics.Start();

  task_environment_.FastForwardBy(base::Milliseconds(40));
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);

  task_environment_.FastForwardBy(base::Milliseconds(80));
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kMonitorCompleted);

  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kInvokeCallback);

  task_environment_.FastForwardBy(base::Milliseconds(10));
  metrics.WillMoveToState(page_content_annotations::PageStabilityState::kDone);

  histogram_tester_.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(PageStabilityOutcome::kStable),
      1);

  // Total time: 40 + 80 + 10 = 130ms
  histogram_tester_.ExpectUniqueTimeSample(
      kIOSActorPageStabilityTotalTimeToStableMetricName,
      base::Milliseconds(130), 1);

  // Monitoring to stable time: 80 + 10 = 90ms
  histogram_tester_.ExpectUniqueTimeSample(
      kIOSActorPageStabilityTimeFromMonitoringToStableMetricName,
      base::Milliseconds(90), 1);
}

// Test that timing out records the `kTimeout` outcome without duration timing.
TEST_F(IOSPageStabilityMetricsTest, LogsTimeoutOutcome) {
  IOSPageStabilityMetrics metrics;
  metrics.Start();

  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);
  task_environment_.FastForwardBy(base::Seconds(2));
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kTimeout);

  metrics.WillMoveToState(page_content_annotations::PageStabilityState::kDone);

  histogram_tester_.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(
          PageStabilityOutcome::kTimeout),
      1);
  histogram_tester_.ExpectTotalCount(
      kIOSActorPageStabilityTotalTimeToStableMetricName, 0);
}

// Test that frame destruction records the `kWebFrameGoingAway` outcome and
// duration.
TEST_F(IOSPageStabilityMetricsTest, LogsWebFrameGoingAwayOutcomeAndTiming) {
  IOSPageStabilityMetrics metrics;
  metrics.Start();

  task_environment_.FastForwardBy(base::Milliseconds(60));
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);

  task_environment_.FastForwardBy(base::Milliseconds(40));
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kRenderFrameGoingAway);

  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kInvokeCallback);
  metrics.WillMoveToState(page_content_annotations::PageStabilityState::kDone);

  histogram_tester_.ExpectUniqueSample(
      kIOSActorPageStabilityOutcomeMetricName,
      static_cast<base::HistogramBase::Sample32>(
          PageStabilityOutcome::kWebFrameGoingAway),
      1);

  histogram_tester_.ExpectUniqueTimeSample(
      kIOSActorPageStabilityTotalTimeToWebFrameGoingAwayMetricName,
      base::Milliseconds(100), 1);
}

// Test that flushing twice is idempotent and does not record double metrics.
TEST_F(IOSPageStabilityMetricsTest, FlushIsIdempotent) {
  IOSPageStabilityMetrics metrics;
  metrics.Start();

  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);
  metrics.WillMoveToState(
      page_content_annotations::PageStabilityState::kTimeout);
  metrics.Flush();
  metrics.Flush();

  histogram_tester_.ExpectTotalCount(kIOSActorPageStabilityOutcomeMetricName,
                                     1);
}

// Test that destructing an unstarted metrics instance does not record metrics.
TEST_F(IOSPageStabilityMetricsTest, UnstartedInstanceDestruction_NoMetrics) {
  {
    IOSPageStabilityMetrics metrics;
  }
  histogram_tester_.ExpectTotalCount(kIOSActorPageStabilityOutcomeMetricName,
                                     0);
}

}  // namespace actor
