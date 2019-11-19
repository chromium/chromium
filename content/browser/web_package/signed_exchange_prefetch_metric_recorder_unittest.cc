// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SignedExchangePrefetchMetricRecorderTest : public ::testing::Test {
 public:
  SignedExchangePrefetchMetricRecorderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    metric_recorder_ =
        base::MakeRefCounted<SignedExchangePrefetchMetricRecorder>(
            &test_tick_clock_);
  }

  void FastForwardBy(const base::TimeDelta fast_forward_delta) {
    test_clock_.Advance(fast_forward_delta);
    test_tick_clock_.Advance(fast_forward_delta);
    task_environment_.FastForwardBy(fast_forward_delta);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  const base::HistogramTester histogram_tester_;
  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;
  scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder_;
};

TEST_F(SignedExchangePrefetchMetricRecorderTest, PrecisionRecall) {
  const base::Time response_time = test_clock_.Now();
  metric_recorder_->OnSignedExchangePrefetchFinished(
      GURL("https://example.org/success.sxg"), response_time);

  FastForwardBy(base::TimeDelta::FromMilliseconds(100));

  const base::Time response_time2 = test_clock_.Now();
  metric_recorder_->OnSignedExchangePrefetchFinished(
      GURL("https://example.org/prefetch_unused.sxg"), response_time2);

  FastForwardBy(base::TimeDelta::FromMilliseconds(100));

  histogram_tester_.ExpectTotalCount(
      "SignedExchange.Prefetch.Precision.30Seconds", 0);
  histogram_tester_.ExpectTotalCount("SignedExchange.Prefetch.Recall.30Seconds",
                                     0);

  metric_recorder_->OnSignedExchangeNonPrefetch(
      GURL("https://example.org/success.sxg"), response_time);

  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.Prefetch.Precision.30Seconds", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.Prefetch.Recall.30Seconds", true, 1);

  const base::Time response_time3 = test_clock_.Now();
  metric_recorder_->OnSignedExchangeNonPrefetch(
      GURL("https://example.org/not_prefetched.sxg"), response_time3);

  histogram_tester_.ExpectBucketCount(
      "SignedExchange.Prefetch.Recall.30Seconds", true, 1);
  histogram_tester_.ExpectBucketCount(
      "SignedExchange.Prefetch.Recall.30Seconds", false, 1);

  FastForwardBy(base::TimeDelta::FromMilliseconds(35000));

  histogram_tester_.ExpectBucketCount(
      "SignedExchange.Prefetch.Precision.30Seconds", true, 1);
  histogram_tester_.ExpectBucketCount(
      "SignedExchange.Prefetch.Precision.30Seconds", false, 1);
}

TEST_F(SignedExchangePrefetchMetricRecorderTest, DuplicatePrefetch) {
  GURL url("https://example.org/foo.sxg");
  const base::Time response_time = test_clock_.Now();

  metric_recorder_->OnSignedExchangePrefetchFinished(url, response_time);

  FastForwardBy(base::TimeDelta::FromMilliseconds(100));

  metric_recorder_->OnSignedExchangePrefetchFinished(url, response_time);

  FastForwardBy(base::TimeDelta::FromMilliseconds(100));

  metric_recorder_->OnSignedExchangeNonPrefetch(url, response_time);

  FastForwardBy(base::TimeDelta::FromMilliseconds(35000));

  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.Prefetch.Precision.30Seconds", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.Prefetch.Recall.30Seconds", true, 1);
}

}  // namespace content
