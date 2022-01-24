// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/cpu_time_metrics_internal.h"

#include "base/metrics/persistent_histogram_allocator.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/power_scheduler/power_mode.h"
#include "content/common/process_visibility_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace internal {
namespace {

void WorkForOneCpuSec(base::WaitableEvent* event) {
  auto initial_ticks = base::ThreadTicks::Now();
  while (!event->IsSignaled()) {
    if (base::ThreadTicks::Now() > initial_ticks + base::Seconds(1)) {
      event->Signal();
    }
  }
}

TEST(CpuTimeMetricsTest, RecordsMetricsForeground) {
  base::test::TaskEnvironment task_environment;

  base::HistogramTester histograms;
  base::Thread thread1("StackSamplingProfiler");

  thread1.StartAndWaitForTesting();
  ASSERT_TRUE(thread1.IsRunning());

  base::WaitableEvent event;

  power_scheduler::PowerModeArbiter arbiter;
  EXPECT_EQ(arbiter.GetActiveModeForTesting(),
            power_scheduler::PowerMode::kCharging);

  // Clear the initial kCharging vote.
  arbiter.SetOnBatteryPowerForTesting(/*on_battery_power=*/true);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(),
            power_scheduler::PowerMode::kIdle);

  // Create the ProcessCpuTimeMetrics instance and register it as the process
  // visibility observer.
  ProcessCpuTimeMetrics::SetIgnoreHistogramAllocatorForTesting(true);
  std::unique_ptr<ProcessCpuTimeMetrics> metrics =
      ProcessCpuTimeMetrics::CreateForTesting(&arbiter);
  metrics->WaitForCollectionForTesting();

  // Start out in the foreground and spend one CPU second there.
  ProcessVisibilityTracker::GetInstance()->OnProcessVisibilityChanged(true);
  metrics->WaitForCollectionForTesting();

  thread1.task_runner()->PostTask(
      FROM_HERE, BindOnce(&WorkForOneCpuSec, base::Unretained(&event)));

  // Wait until the thread has consumed one second of CPU time.
  event.Wait();

  // Update the state to background to trigger the collection of high level
  // metrics.
  ProcessVisibilityTracker::GetInstance()->OnProcessVisibilityChanged(false);
  metrics->WaitForCollectionForTesting();

  // The test process has no process-type command line flag, so is recognized as
  // the browser process. The thread created above is named like a sampling
  // profiler thread.
  static constexpr int kBrowserProcessBucket = 2;
  static constexpr int kSamplingProfilerThreadBucket = 24;

  // Expect that the CPU second spent by the thread above is represented in the
  // metrics.
  int browser_cpu_seconds = histograms.GetBucketCount(
      "Power.CpuTimeSecondsPerProcessType", kBrowserProcessBucket);
  EXPECT_GE(browser_cpu_seconds, 1);

  int browser_cpu_seconds_foreground = histograms.GetBucketCount(
      "Power.CpuTimeSecondsPerProcessType.Foreground", kBrowserProcessBucket);
  EXPECT_GE(browser_cpu_seconds_foreground, 1);

  int browser_cpu_seconds_power_mode_idle =
      histograms.GetBucketCount("Power.CpuTimeSecondsPerPowerMode.Browser",
                                internal::PowerModeForUma::kIdle);
  EXPECT_GE(browser_cpu_seconds_power_mode_idle, 1);

  // Thread breakdown requires periodic collection.
  int thread_cpu_seconds =
      histograms.GetBucketCount("Power.CpuTimeSecondsPerThreadType.Browser",
                                kSamplingProfilerThreadBucket);
  EXPECT_EQ(thread_cpu_seconds, 0);

  metrics->PerformFullCollectionForTesting();
  metrics->WaitForCollectionForTesting();

  thread_cpu_seconds =
      histograms.GetBucketCount("Power.CpuTimeSecondsPerThreadType.Browser",
                                kSamplingProfilerThreadBucket);
  EXPECT_GE(thread_cpu_seconds, 1);

  thread1.Stop();

  ProcessCpuTimeMetrics::SetIgnoreHistogramAllocatorForTesting(false);
}

TEST(CpuTimeMetricsTest, RecordsMetricsBackground) {
  base::test::TaskEnvironment task_environment;

  base::HistogramTester histograms;
  base::Thread thread1("StackSamplingProfiler");

  thread1.StartAndWaitForTesting();
  ASSERT_TRUE(thread1.IsRunning());

  base::WaitableEvent event;

  power_scheduler::PowerModeArbiter arbiter;
  EXPECT_EQ(arbiter.GetActiveModeForTesting(),
            power_scheduler::PowerMode::kCharging);
  auto voter = arbiter.NewVoter("Background");

  // Clear the initial kCharging vote.
  arbiter.SetOnBatteryPowerForTesting(/*on_battery_power=*/true);
  EXPECT_EQ(arbiter.GetActiveModeForTesting(),
            power_scheduler::PowerMode::kIdle);

  // Create the ProcessCpuTimeMetrics instance and register it as the process
  // visibility observer.
  ProcessCpuTimeMetrics::SetIgnoreHistogramAllocatorForTesting(true);
  std::unique_ptr<ProcessCpuTimeMetrics> metrics =
      ProcessCpuTimeMetrics::CreateForTesting(&arbiter);
  metrics->WaitForCollectionForTesting();

  // Start out in the background and spend one CPU second there.
  ProcessVisibilityTracker::GetInstance()->OnProcessVisibilityChanged(false);
  voter->VoteFor(power_scheduler::PowerMode::kBackground);
  metrics->WaitForCollectionForTesting();

  thread1.task_runner()->PostTask(
      FROM_HERE, BindOnce(&WorkForOneCpuSec, base::Unretained(&event)));

  // Wait until the thread has consumed one second of CPU time.
  event.Wait();

  // Update the state to foreground to trigger the collection of high level
  // metrics.
  ProcessVisibilityTracker::GetInstance()->OnProcessVisibilityChanged(true);
  voter->VoteFor(power_scheduler::PowerMode::kIdle);
  metrics->WaitForCollectionForTesting();

  // The test process has no process-type command line flag, so is recognized as
  // the browser process. The thread created above is named like a sampling
  // profiler thread.
  static constexpr int kBrowserProcessBucket = 2;
  static constexpr int kSamplingProfilerThreadBucket = 24;

  // Expect that the CPU second spent by the thread above is represented in the
  // metrics.
  int browser_cpu_seconds = histograms.GetBucketCount(
      "Power.CpuTimeSecondsPerProcessType", kBrowserProcessBucket);
  EXPECT_GE(browser_cpu_seconds, 1);

  int browser_cpu_seconds_background = histograms.GetBucketCount(
      "Power.CpuTimeSecondsPerProcessType.Background", kBrowserProcessBucket);
  EXPECT_GE(browser_cpu_seconds_background, 1);

  int browser_cpu_seconds_power_mode_background =
      histograms.GetBucketCount("Power.CpuTimeSecondsPerPowerMode.Browser",
                                internal::PowerModeForUma::kBackground);
  EXPECT_GE(browser_cpu_seconds_power_mode_background, 1);

  // Thread breakdown requires periodic collection.
  int thread_cpu_seconds =
      histograms.GetBucketCount("Power.CpuTimeSecondsPerThreadType.Browser",
                                kSamplingProfilerThreadBucket);
  EXPECT_EQ(thread_cpu_seconds, 0);

  metrics->PerformFullCollectionForTesting();
  metrics->WaitForCollectionForTesting();

  thread_cpu_seconds =
      histograms.GetBucketCount("Power.CpuTimeSecondsPerThreadType.Browser",
                                kSamplingProfilerThreadBucket);
  EXPECT_GE(thread_cpu_seconds, 1);

  thread1.Stop();

  ProcessCpuTimeMetrics::SetIgnoreHistogramAllocatorForTesting(false);
}

}  // namespace
}  // namespace internal
}  // namespace content
