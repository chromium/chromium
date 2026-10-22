// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "content/browser/background_sync/background_sync_metrics.h"

namespace content {

class BackgroundSyncMetricsTest : public ::testing::Test {
 public:
  BackgroundSyncMetricsTest() = default;

  BackgroundSyncMetricsTest(const BackgroundSyncMetricsTest&) = delete;
  BackgroundSyncMetricsTest& operator=(const BackgroundSyncMetricsTest&) =
      delete;

  ~BackgroundSyncMetricsTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(BackgroundSyncMetricsTest, RecordOneShotEventResult) {
  BackgroundSyncMetrics::RecordOneShotEventResult(
      /* success= */ true,
      /* finished_in_foreground= */ true);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.Event.OneShotResultPattern",
      BackgroundSyncMetrics::ResultPattern::RESULT_PATTERN_SUCCESS_FOREGROUND,
      1);

  BackgroundSyncMetrics::RecordOneShotEventResult(
      /* success= */ false,
      /* finished_in_foreground= */ false);
  histogram_tester_.ExpectBucketCount(
      "BackgroundSync.Event.OneShotResultPattern",
      BackgroundSyncMetrics::ResultPattern::RESULT_PATTERN_FAILED_BACKGROUND,
      1);
}

}  // namespace content
