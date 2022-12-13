// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

#include "base/metrics/histogram_samples.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestCustomCountHistogram : public CustomCountHistogram {
 public:
  TestCustomCountHistogram(const char* name,
                           base::HistogramBase::Sample min,
                           base::HistogramBase::Sample max,
                           int32_t bucket_count)
      : CustomCountHistogram(name, min, max, bucket_count) {}

  base::HistogramBase* Histogram() { return histogram_; }
};

class ScopedUsHistogramTimerTest : public testing::Test {
 public:
  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

TEST_F(ScopedUsHistogramTimerTest, Basic) {
  TestCustomCountHistogram scoped_us_counter(
      "ScopedUsHistogramTimerTest.Basic", kTimeBasedHistogramMinSample,
      kTimeBasedHistogramMaxSample, kTimeBasedHistogramBucketCount);
  {
    ScopedUsHistogramTimer timer(scoped_us_counter,
                                 test_task_runner_->GetMockTickClock());
    test_task_runner_->FastForwardBy(base::Milliseconds(500));
  }
  // 500ms == 500000us
  EXPECT_EQ(500000, scoped_us_counter.Histogram()->SnapshotSamples()->sum());
}

TEST_F(ScopedUsHistogramTimerTest, BasicHighRes) {
  TestCustomCountHistogram scoped_us_counter(
      "ScopedHighResUsHistogramTimerTest.Basic", kTimeBasedHistogramMinSample,
      kTimeBasedHistogramMaxSample, kTimeBasedHistogramBucketCount);
  {
    ScopedHighResUsHistogramTimer timer(scoped_us_counter,
                                        test_task_runner_->GetMockTickClock());
    test_task_runner_->FastForwardBy(base::Milliseconds(500));
  }
  int64_t expected = base::TimeTicks::IsHighResolution() ? 500000 : 0;
  EXPECT_EQ(expected, scoped_us_counter.Histogram()->SnapshotSamples()->sum());
}

}  // namespace blink
