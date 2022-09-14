// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web_state_list/session_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/platform_test.h"

namespace {

using SessionMetricsTest = PlatformTest;

TEST_F(SessionMetricsTest, RecordMetrics) {
  base::HistogramTester histogram_tester;

  SessionMetrics session_metrics;
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateActivated();
  session_metrics.OnWebStateActivated();
  session_metrics.RecordAndClearSessionMetrics(
      MetricsToRecordFlags::kOpenedTabCount |
      MetricsToRecordFlags::kClosedTabCount |
      MetricsToRecordFlags::kActivatedTabCount);

  histogram_tester.ExpectUniqueSample("Session.NewTabCounts", 2, 1);
  histogram_tester.ExpectUniqueSample("Session.ClosedTabCounts", 2, 1);
  histogram_tester.ExpectUniqueSample("Session.OpenedTabCounts", 2, 1);
}
TEST_F(SessionMetricsTest, RecordWebStateInsertedMetric) {
  base::HistogramTester histogram_tester;

  SessionMetrics session_metrics;
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateActivated();
  session_metrics.OnWebStateActivated();
  session_metrics.RecordAndClearSessionMetrics(
      MetricsToRecordFlags::kOpenedTabCount);

  histogram_tester.ExpectUniqueSample("Session.NewTabCounts", 2, 1);
  histogram_tester.ExpectUniqueSample("Session.ClosedTabCounts", 0, 0);
  histogram_tester.ExpectUniqueSample("Session.OpenedTabCounts", 0, 0);
}

TEST_F(SessionMetricsTest, RecordWebStateDetachedMetric) {
  base::HistogramTester histogram_tester;

  SessionMetrics session_metrics;
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateActivated();
  session_metrics.OnWebStateActivated();
  session_metrics.RecordAndClearSessionMetrics(
      MetricsToRecordFlags::kClosedTabCount);

  histogram_tester.ExpectUniqueSample("Session.NewTabCounts", 0, 0);
  histogram_tester.ExpectUniqueSample("Session.ClosedTabCounts", 2, 1);
  histogram_tester.ExpectUniqueSample("Session.OpenedTabCounts", 0, 0);
}

TEST_F(SessionMetricsTest, RecordWebStateActivatedMetric) {
  base::HistogramTester histogram_tester;

  SessionMetrics session_metrics;
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateActivated();
  session_metrics.OnWebStateActivated();
  session_metrics.RecordAndClearSessionMetrics(
      MetricsToRecordFlags::kActivatedTabCount);

  histogram_tester.ExpectUniqueSample("Session.NewTabCounts", 0, 0);
  histogram_tester.ExpectUniqueSample("Session.ClosedTabCounts", 0, 0);
  histogram_tester.ExpectUniqueSample("Session.OpenedTabCounts", 2, 1);
}

TEST_F(SessionMetricsTest, RecordNoMetric) {
  base::HistogramTester histogram_tester;

  SessionMetrics session_metrics;
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateActivated();
  session_metrics.OnWebStateActivated();
  session_metrics.RecordAndClearSessionMetrics(
      MetricsToRecordFlags::kNoMetrics);

  histogram_tester.ExpectUniqueSample("Session.NewTabCounts", 0, 0);
  histogram_tester.ExpectUniqueSample("Session.ClosedTabCounts", 0, 0);
  histogram_tester.ExpectUniqueSample("Session.OpenedTabCounts", 0, 0);
}

TEST_F(SessionMetricsTest, RecordClearMetrics) {
  base::HistogramTester histogram_tester;

  SessionMetrics session_metrics;
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateInserted();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateDetached();
  session_metrics.OnWebStateActivated();
  session_metrics.OnWebStateActivated();
  session_metrics.RecordAndClearSessionMetrics(
      MetricsToRecordFlags::kNoMetrics);

  histogram_tester.ExpectUniqueSample("Session.NewTabCounts", 0, 0);
  histogram_tester.ExpectUniqueSample("Session.ClosedTabCounts", 0, 0);
  histogram_tester.ExpectUniqueSample("Session.OpenedTabCounts", 0, 0);

  session_metrics.RecordAndClearSessionMetrics(
      MetricsToRecordFlags::kOpenedTabCount |
      MetricsToRecordFlags::kClosedTabCount |
      MetricsToRecordFlags::kActivatedTabCount);

  histogram_tester.ExpectUniqueSample("Session.NewTabCounts", 0, 1);
  histogram_tester.ExpectUniqueSample("Session.ClosedTabCounts", 0, 1);
  histogram_tester.ExpectUniqueSample("Session.OpenedTabCounts", 0, 1);
}

}  // namespace
