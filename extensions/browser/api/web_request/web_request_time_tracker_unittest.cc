// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_time_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const base::TimeDelta kRequestDelta = base::Milliseconds(100);
const base::TimeDelta kLongRequestDelta = base::Milliseconds(200);
const base::TimeDelta kTinyDelay = base::Milliseconds(1);
const base::TimeDelta kModerateDelay = base::Milliseconds(25);
const base::TimeDelta kExcessiveDelay = base::Milliseconds(75);
}  // namespace

// Test the basis recording of histograms.
TEST(ExtensionWebRequestTimeTrackerTest, Histograms) {
  base::HistogramTester histogram_tester;

  ExtensionWebRequestTimeTracker tracker;
  base::TimeTicks start;

  tracker.LogRequestStartTime(1, start, false, false);
  tracker.LogRequestStartTime(2, start, true, false);
  tracker.LogRequestStartTime(3, start, true, false);
  tracker.LogRequestStartTime(4, start, true, true);
  tracker.IncrementTotalBlockTime(1, kTinyDelay);
  tracker.IncrementTotalBlockTime(2, kModerateDelay);
  tracker.IncrementTotalBlockTime(2, kModerateDelay);
  tracker.IncrementTotalBlockTime(3, kExcessiveDelay);
  tracker.LogRequestEndTime(1, start + kRequestDelta);
  tracker.LogRequestEndTime(2, start + kRequestDelta);
  tracker.LogRequestEndTime(3, start + kRequestDelta);
  tracker.LogRequestEndTime(4, start + kLongRequestDelta);

  histogram_tester.ExpectTimeBucketCount("Extensions.NetworkDelay", kTinyDelay,
                                         1);
  histogram_tester.ExpectTimeBucketCount("Extensions.NetworkDelay",
                                         2 * kModerateDelay, 1);
  histogram_tester.ExpectTimeBucketCount("Extensions.NetworkDelay",
                                         kExcessiveDelay, 1);
  histogram_tester.ExpectTotalCount("Extensions.NetworkDelay", 3);

  histogram_tester.ExpectBucketCount("Extensions.NetworkDelayPercentage", 1, 1);
  histogram_tester.ExpectBucketCount("Extensions.NetworkDelayPercentage",
                                     2 * 25, 1);
  histogram_tester.ExpectBucketCount("Extensions.NetworkDelayPercentage", 75,
                                     1);
  histogram_tester.ExpectTotalCount("Extensions.NetworkDelayPercentage", 3);

  histogram_tester.ExpectTimeBucketCount(
      "Extensions.WebRequest.TotalRequestTime", kRequestDelta, 2);
  histogram_tester.ExpectTimeBucketCount(
      "Extensions.WebRequest.TotalRequestTime", kLongRequestDelta, 1);
  histogram_tester.ExpectTotalCount("Extensions.WebRequest.TotalRequestTime",
                                    3);

  histogram_tester.ExpectTimeBucketCount(
      "Extensions.WebRequest.TotalBlockingRequestTime", kRequestDelta, 3);
  histogram_tester.ExpectTotalCount(
      "Extensions.WebRequest.TotalBlockingRequestTime", 3);

  histogram_tester.ExpectTimeBucketCount(
      "Extensions.WebRequest.TotalExtraHeadersRequestTime", kLongRequestDelta,
      1);
  histogram_tester.ExpectTotalCount(
      "Extensions.WebRequest.TotalExtraHeadersRequestTime", 1);

  EXPECT_TRUE(tracker.request_time_logs_.empty());
}
