// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_metrics.h"

#import "base/metrics/histogram_base.h"
#import "base/metrics/histogram_samples.h"
#import "base/metrics/statistics_recorder.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for PageContextWrapperMetrics.
class PageContextWrapperMetricsTest : public PlatformTest {
 public:
  PageContextWrapperMetricsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PageContextWrapperMetricsTest() override = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    metrics_ = [[PageContextWrapperMetrics alloc] init];
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  PageContextWrapperMetrics* metrics() { return metrics_; }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  PageContextWrapperMetrics* metrics_;
};

// Tests that the overall task timer is started on initialization.
TEST_F(PageContextWrapperMetricsTest, TestInitialization) {
  // The overall task is started at init. When it finishes, a metric should be
  // logged.
  [metrics() executionFinishedForTask:PageContextTask::kOverall
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];

  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.Overall.Success.Latency", 1);
}

// Tests that starting and finishing a single subtask logs the correct metric.
TEST_F(PageContextWrapperMetricsTest, TestSingleSubtask) {
  [metrics() executionStartedForTask:PageContextTask::kScreenshot];
  [metrics() executionFinishedForTask:PageContextTask::kScreenshot
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];
  [metrics() executionFinishedForTask:PageContextTask::kOverall
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];

  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.Overall.Success.Latency", 1);
  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.Screenshot.Success.Latency", 1);
}

// Tests that multiple subtasks can be tracked and logged.
TEST_F(PageContextWrapperMetricsTest, TestMultipleSubtasks) {
  [metrics() executionStartedForTask:PageContextTask::kScreenshot];
  [metrics() executionFinishedForTask:PageContextTask::kScreenshot
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];

  [metrics() executionStartedForTask:PageContextTask::kAnnotatedPageContent];
  [metrics() executionFinishedForTask:PageContextTask::kAnnotatedPageContent
                 withCompletionStatus:PageContextCompletionStatus::kFailure];

  [metrics() executionStartedForTask:PageContextTask::kPDF];
  [metrics() executionFinishedForTask:PageContextTask::kPDF
                 withCompletionStatus:PageContextCompletionStatus::kTimeout];

  [metrics() executionStartedForTask:PageContextTask::kInnerText];
  [metrics() executionFinishedForTask:PageContextTask::kInnerText
                 withCompletionStatus:PageContextCompletionStatus::kProtected];

  [metrics() executionFinishedForTask:PageContextTask::kOverall
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];

  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.Overall.Success.Latency", 1);
  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.Screenshot.Success.Latency", 1);
  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.AnnotatedPageContent.Failure.Latency", 1);
  histogram_tester()->ExpectTotalCount("IOS.PageContext.PDF.Timeout.Latency",
                                       1);
  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.InnerText.PageProtected.Latency", 1);
}

// Tests that a metric is not logged for a subtask that was started but not
// finished.
TEST_F(PageContextWrapperMetricsTest, TestUnfinishedSubtask) {
  [metrics() executionStartedForTask:PageContextTask::kScreenshot];
  [metrics() executionFinishedForTask:PageContextTask::kOverall
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];

  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.Overall.Success.Latency", 1);
  histogram_tester()->ExpectTotalCount(
      "IOS.PageContext.Screenshot.Success.Latency", 0);
}

// Tests that the logged latency metric is accurate.
TEST_F(PageContextWrapperMetricsTest, TestLatencyValue) {
  base::TimeDelta overall_latency = base::Milliseconds(250);
  base::TimeDelta screenshot_latency = base::Milliseconds(100);

  // Test screenshot subtask.
  [metrics() executionStartedForTask:PageContextTask::kScreenshot];
  task_environment().FastForwardBy(screenshot_latency);
  [metrics() executionFinishedForTask:PageContextTask::kScreenshot
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];

  // Finish overall task.
  task_environment().FastForwardBy(overall_latency - screenshot_latency);
  [metrics() executionFinishedForTask:PageContextTask::kOverall
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];

  // Verify screenshot latency.
  histogram_tester()->ExpectUniqueSample(
      "IOS.PageContext.Screenshot.Success.Latency",
      screenshot_latency.InMilliseconds(), 1);

  // Verify overall latency.
  histogram_tester()->ExpectUniqueSample(
      "IOS.PageContext.Overall.Success.Latency",
      overall_latency.InMilliseconds(), 1);
}

using PageContextWrapperMetricsDeathTest = PageContextWrapperMetricsTest;

// Tests that the class CHECKs when a timer is started for a task that is
// already running.
TEST_F(PageContextWrapperMetricsDeathTest, TestStartTaskAlreadyRunning) {
  [metrics() executionStartedForTask:PageContextTask::kScreenshot];
  EXPECT_DEATH_IF_SUPPORTED(
      [metrics() executionStartedForTask:PageContextTask::kScreenshot], "");
  // Finish tasks to avoid leaks.
  [metrics() executionFinishedForTask:PageContextTask::kScreenshot
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];
  [metrics() executionFinishedForTask:PageContextTask::kOverall
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];
}

// Tests that the class NOTREACHEDs when trying to start the overall task timer
// again.
TEST_F(PageContextWrapperMetricsDeathTest, TestStartOverallTaskAgain) {
  EXPECT_DEATH_IF_SUPPORTED(
      [metrics() executionStartedForTask:PageContextTask::kOverall], "");
  // Finish overall task to avoid leaks.
  [metrics() executionFinishedForTask:PageContextTask::kOverall
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];
}

// Tests that the class CHECKs when a timer is finished for a task that was
// never started.
TEST_F(PageContextWrapperMetricsDeathTest, TestFinishTaskNotStarted) {
  EXPECT_DEATH_IF_SUPPORTED(
      [metrics()
          executionFinishedForTask:PageContextTask::kScreenshot
              withCompletionStatus:PageContextCompletionStatus::kSuccess],
      "");
  // Finish overall task to avoid leaks.
  [metrics() executionFinishedForTask:PageContextTask::kOverall
                 withCompletionStatus:PageContextCompletionStatus::kSuccess];
}
