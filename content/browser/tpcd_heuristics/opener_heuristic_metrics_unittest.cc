// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tpcd_heuristics/opener_heuristic_metrics.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace content {

TEST(OpenerHeuristicsMetricsTest, BucketizeHoursSinceLastInteraction) {
  constexpr int kMaximum = base::Days(30).InHours();

  // The input value is clamped to be between 0 and 30 days.
  for (int time : {base::TimeDelta::Min().InHours(), 0}) {
    EXPECT_EQ(Bucketize3PCDHeuristicSample(time, kMaximum), 0);
  }
  for (int time : {kMaximum, base::TimeDelta::Max().InHours()}) {
    EXPECT_EQ(Bucketize3PCDHeuristicSample(time, kMaximum), kMaximum);
  }

  std::set<int32_t> seen_values;
  int32_t last_value = 0;
  for (int time = 0; time <= kMaximum; ++time) {
    int32_t value = Bucketize3PCDHeuristicSample(time, kMaximum);
    // Values get placed in increasing buckets
    ASSERT_GE(value, last_value);
    seen_values.insert(value);
    last_value = value;
  }
  // Exactly 50 buckets
  ASSERT_EQ(seen_values.size(), 50u);
}

// TODO(crbug.com/40281179): The test is flaky across platforms.
TEST(OpenerHeuristicsMetricsTest, DISABLED_BucketizeSecondsSinceCommitted) {
  constexpr int64_t kMaximum = base::Minutes(3).InSeconds();

  // The input value is clamped to be between 0 and 3 minutes.
  for (int64_t time : {base::TimeDelta::Min().InSeconds(), int64_t{0}}) {
    EXPECT_EQ(Bucketize3PCDHeuristicSample(time, kMaximum), 0);
  }
  for (int64_t time : {kMaximum, base::TimeDelta::Max().InSeconds()}) {
    EXPECT_EQ(Bucketize3PCDHeuristicSample(time, kMaximum), kMaximum);
  }

  std::set<int32_t> seen_values;
  int32_t last_value = 0;
  for (int64_t time = 0; time <= kMaximum; ++time) {
    int32_t value = Bucketize3PCDHeuristicSample(time, kMaximum);
    // Values get placed in increasing buckets
    ASSERT_GE(value, last_value);
    seen_values.insert(value);
    last_value = value;
  }
  // Exactly 50 buckets
  ASSERT_EQ(seen_values.size(), 50u);
}

}  // namespace content
