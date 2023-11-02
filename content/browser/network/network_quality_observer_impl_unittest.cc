// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_quality_observer_impl.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(NetworkQualityObserverImplTest, TestObserverNotified) {
  content::BrowserTaskEnvironment task_environment(
      content::BrowserTaskEnvironment::IO_MAINLOOP);

  network::TestNetworkQualityTracker test_network_quality_tracker;

  NetworkQualityObserverImpl impl(&test_network_quality_tracker);

  test_network_quality_tracker.ReportRTTsAndThroughputForTesting(
      base::Milliseconds(1), 100);

  base::RunLoop().RunUntilIdle();

  base::HistogramTester histogram_tester;

  test_network_quality_tracker.ReportRTTsAndThroughputForTesting(
      base::Milliseconds(500), 100);

  // RTT changed from 1 msec to 500 msec.
  histogram_tester.ExpectBucketCount(
      "NQE.ContentObserver.NetworkQualityMeaningfullyChanged", 1, 1);

  test_network_quality_tracker.ReportRTTsAndThroughputForTesting(
      base::Milliseconds(625), 100);
  // RTT changed from 500 msec to 625 msec.
  histogram_tester.ExpectBucketCount(
      "NQE.ContentObserver.NetworkQualityMeaningfullyChanged", 1, 2);

  test_network_quality_tracker.ReportRTTsAndThroughputForTesting(
      base::Milliseconds(626), 100);
  // RTT changed from 625 msec to 626 msec which is not a meaningful change.
  histogram_tester.ExpectBucketCount(
      "NQE.ContentObserver.NetworkQualityMeaningfullyChanged", 1, 2);
  EXPECT_LE(1, histogram_tester.GetBucketCount(
                   "NQE.ContentObserver.NetworkQualityMeaningfullyChanged", 0));
}

}  // namespace
}  // namespace content
