// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

base::TimeTicks AdvanceTime(base::TimeTicks* time, int milliseconds) {
  *time += base::Milliseconds(milliseconds);
  return *time;
}

}  // namespace

using CrossProcessTimeDelta = ServiceWorkerMetrics::CrossProcessTimeDelta;
using StartSituation = ServiceWorkerMetrics::StartSituation;

TEST(ServiceWorkerMetricsTest, EmbeddedWorkerStartTiming) {
  ServiceWorkerMetrics::StartTimes times;
  auto current = base::TimeTicks::Now();
  times.local_start = current;
  times.local_start_worker_sent = AdvanceTime(&current, 11);
  times.remote_start_worker_received = AdvanceTime(&current, 33);
  times.remote_script_evaluation_start = AdvanceTime(&current, 55);
  times.remote_script_evaluation_end = AdvanceTime(&current, 77);
  times.local_end = AdvanceTime(&current, 22);

  StartSituation start_situation = StartSituation::EXISTING_READY_PROCESS;
  base::HistogramTester histogram_tester;

  ServiceWorkerMetrics::RecordStartWorkerTiming(times, start_situation);

  // Total duration.
  histogram_tester.ExpectTimeBucketCount("ServiceWorker.StartTiming.Duration",
                                         times.local_end - times.local_start,
                                         1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.Duration.ExistingReadyProcess",
      times.local_end - times.local_start, 1);

  // SentStartWorker milestone.
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.StartToSentStartWorker",
      times.local_start_worker_sent - times.local_start, 1);

  // ReceivedStartWorker milestone.
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.StartToReceivedStartWorker",
      times.remote_start_worker_received - times.local_start, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.SentStartWorkerToReceivedStartWorker",
      times.remote_start_worker_received - times.local_start_worker_sent, 1);

  // ScriptEvaluationStart milestone.
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.StartToScriptEvaluationStart",
      times.remote_script_evaluation_start - times.local_start, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.ReceivedStartWorkerToScriptEvaluationStart",
      times.remote_script_evaluation_start - times.remote_start_worker_received,
      1);

  // ScriptEvaluationEnd milestone.
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.StartToScriptEvaluationEnd",
      times.remote_script_evaluation_end - times.local_start, 1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.ScriptEvaluationStartToScriptEvaluationEnd",
      times.remote_script_evaluation_end - times.remote_script_evaluation_start,
      1);

  // End milestone.
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.ScriptEvaluationEndToEnd",
      times.local_end - times.remote_script_evaluation_end, 1);

  // Clock consistency.
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.StartTiming.ClockConsistency",
      CrossProcessTimeDelta::NORMAL, 1);
}

TEST(ServiceWorkerMetricsTest, EmbeddedWorkerStartTiming_BrowserStartup) {
  ServiceWorkerMetrics::StartTimes times;
  auto current = base::TimeTicks::Now();
  times.local_start = current;
  times.local_start_worker_sent = AdvanceTime(&current, 11);
  times.remote_start_worker_received = AdvanceTime(&current, 66);
  times.remote_script_evaluation_start = AdvanceTime(&current, 55);
  times.remote_script_evaluation_end = AdvanceTime(&current, 77);
  times.local_end = AdvanceTime(&current, 22);

  StartSituation start_situation = StartSituation::DURING_STARTUP;
  base::HistogramTester histogram_tester;

  ServiceWorkerMetrics::RecordStartWorkerTiming(times, start_situation);

  // Total duration.
  histogram_tester.ExpectTimeBucketCount("ServiceWorker.StartTiming.Duration",
                                         times.local_end - times.local_start,
                                         1);
  histogram_tester.ExpectTimeBucketCount(
      "ServiceWorker.StartTiming.Duration.DuringStartup",
      times.local_end - times.local_start, 1);
}

TEST(ServiceWorkerMetricsTest,
     EmbeddedWorkerStartTiming_NegativeLatencyForStartIPC) {
  ServiceWorkerMetrics::StartTimes times;
  auto current = base::TimeTicks::Now();
  times.local_start = current;
  times.local_start_worker_sent = AdvanceTime(&current, 11);
  // Go back in time.
  times.remote_start_worker_received = AdvanceTime(&current, -777);
  times.remote_script_evaluation_start = AdvanceTime(&current, 55);
  times.remote_script_evaluation_end = AdvanceTime(&current, 77);
  times.local_end = AdvanceTime(&current, 22);

  StartSituation start_situation = StartSituation::EXISTING_READY_PROCESS;
  base::HistogramTester histogram_tester;

  ServiceWorkerMetrics::RecordStartWorkerTiming(times, start_situation);

  // Duration and breakdowns should not be logged.
  histogram_tester.ExpectTotalCount("ServiceWorker.StartTiming.Duration", 0);
  // Just test one arbitrarily chosen breakdown metric.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.StartTiming.StartToScriptEvaluationStart", 0);

  // Clock consistency.
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.StartTiming.ClockConsistency",
      CrossProcessTimeDelta::NEGATIVE, 1);
}

TEST(ServiceWorkerMetricsTest,
     EmbeddedWorkerStartTiming_NegativeLatencyForStartedIPC) {
  ServiceWorkerMetrics::StartTimes times;
  auto current = base::TimeTicks::Now();
  times.local_start = current;
  times.local_start_worker_sent = AdvanceTime(&current, 11);
  times.remote_start_worker_received = AdvanceTime(&current, 777);
  times.remote_script_evaluation_start = AdvanceTime(&current, 55);
  times.remote_script_evaluation_end = AdvanceTime(&current, 77);
  // Go back in time.
  times.local_end = AdvanceTime(&current, -123);

  StartSituation start_situation = StartSituation::EXISTING_READY_PROCESS;
  base::HistogramTester histogram_tester;

  ServiceWorkerMetrics::RecordStartWorkerTiming(times, start_situation);

  // Duration and breakdowns should not be logged.
  histogram_tester.ExpectTotalCount("ServiceWorker.StartTiming.Duration", 0);
  // Just test one arbitrarily chosen breakdown metric.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.StartTiming.ScriptEvaluationStartToScriptEvaluationEnd",
      0);

  // Clock consistency.
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.StartTiming.ClockConsistency",
      CrossProcessTimeDelta::NEGATIVE, 1);
}

}  // namespace content
