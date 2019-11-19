// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/leaky_bucket.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(LeakyBucketTest, LargeTimeDelta_BucketIsEmpty) {
  // This test triggers an integer underflow bug in the previous implementation.
  // The large (but reasonable) time skip should result in an empty bucket.
  // A bug (due to int64->int conversion) caused the bucket-level to be a large
  // positive value, resulting in a bucket-empty time in the far future.
  auto now = base::TimeTicks::Now();
  int rate = 1e6;
  LeakyBucket bucket(LeakyBucket::kUnlimitedDepth, rate);

  // Small random amount, should empty very quickly.
  bucket.RefillOrSpill(123, now);

  // The amount of drainage is 3000 * rate = 3e9, which overflows a signed
  // 32-bit int. This would be subtracted from the current level, under the
  // old implementation.
  now += base::TimeDelta::FromSeconds(3000);
  bucket.UpdateRate(rate, now);

  auto empty_time = bucket.GetEmptyTime();
  EXPECT_LE(empty_time, now);
}

}  // namespace remoting
