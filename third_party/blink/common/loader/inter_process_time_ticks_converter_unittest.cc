// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/inter_process_time_ticks_converter.h"

#include <stdint.h>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;

namespace blink {

namespace {

struct TestParams {
  LocalTimeTicks local_lower_bound;
  RemoteTimeTicks remote_lower_bound;
  RemoteTimeTicks remote_upper_bound;
  LocalTimeTicks local_upper_bound;
  RemoteTimeTicks test_time;
  RemoteTimeDelta test_delta;
};

struct TestResults {
  LocalTimeTicks result_time;
  LocalTimeDelta result_delta;
  int64_t skew;
};

LocalTimeTicks GetLocalTimeTicks(int64_t value) {
  return LocalTimeTicks::FromTimeTicks(base::TimeTicks() +
                                       base::Microseconds(value));
}

RemoteTimeTicks GetRemoteTimeTicks(int64_t value) {
  return RemoteTimeTicks::FromTimeTicks(base::TimeTicks() +
                                        base::Microseconds(value));
}

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks TicksFromMicroseconds(int64_t micros) {
  return base::TimeTicks() + base::Microseconds(micros);
}

TestResults RunTest(const TestParams& params) {
  InterProcessTimeTicksConverter converter(
      params.local_lower_bound, params.local_upper_bound,
      params.remote_lower_bound, params.remote_upper_bound);

  TestResults results;
  results.result_time = converter.ToLocalTimeTicks(params.test_time);
  results.result_delta = converter.ToLocalTimeDelta(params.test_delta);
  results.skew = converter.GetSkewForMetrics().InMicroseconds();
  return results;
}

TEST(InterProcessTimeTicksConverterTest, NullTime) {
  // Null / zero times should remain null.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(1);
  p.remote_lower_bound = GetRemoteTimeTicks(2);
  p.remote_upper_bound = GetRemoteTimeTicks(5);
  p.local_upper_bound = GetLocalTimeTicks(6);
  p.test_time = GetRemoteTimeTicks(0);
  p.test_delta = RemoteTimeDelta();
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(0), results.result_time);
  EXPECT_EQ(LocalTimeDelta(), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, NoSkew) {
  // All times are monotonic and centered, so no adjustment should occur.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(1);
  p.remote_lower_bound = GetRemoteTimeTicks(2);
  p.remote_upper_bound = GetRemoteTimeTicks(5);
  p.local_upper_bound = GetLocalTimeTicks(6);
  p.test_time = GetRemoteTimeTicks(3);
  p.test_delta = RemoteTimeDelta::FromMicroseconds(1);
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(3), results.result_time);
  EXPECT_EQ(LocalTimeDelta::FromMicroseconds(1), results.result_delta);
  EXPECT_EQ(0, results.skew);
}

TEST(InterProcessTimeTicksConverterTest, OffsetMidpoints) {
  // All times are monotonic, but not centered. Adjust the |remote_*| times so
  // they are centered within the |local_*| times.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(1);
  p.remote_lower_bound = GetRemoteTimeTicks(3);
  p.remote_upper_bound = GetRemoteTimeTicks(6);
  p.local_upper_bound = GetLocalTimeTicks(6);
  p.test_time = GetRemoteTimeTicks(4);
  p.test_delta = RemoteTimeDelta::FromMicroseconds(1);
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(3), results.result_time);
  EXPECT_EQ(LocalTimeDelta::FromMicroseconds(1), results.result_delta);
  EXPECT_EQ(1, results.skew);
}

TEST(InterProcessTimeTicksConverterTest, DoubleEndedSkew) {
  // |remote_lower_bound| occurs before |local_lower_bound| and
  // |remote_upper_bound| occurs after |local_upper_bound|. We must adjust both
  // bounds and scale down the delta. |test_time| is on the midpoint, so it
  // doesn't change. The ratio of local time to network time is 1:2, so we scale
  // |test_delta| to half.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(3);
  p.remote_lower_bound = GetRemoteTimeTicks(1);
  p.remote_upper_bound = GetRemoteTimeTicks(9);
  p.local_upper_bound = GetLocalTimeTicks(7);
  p.test_time = GetRemoteTimeTicks(5);
  p.test_delta = RemoteTimeDelta::FromMicroseconds(2);
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(5), results.result_time);
  EXPECT_EQ(LocalTimeDelta::FromMicroseconds(1), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, FrontEndSkew) {
  // |remote_upper_bound| is coherent, but |remote_lower_bound| is not. So we
  // adjust the lower bound and move |test_time| out. The scale factor is 2:3,
  // but since we use integers, the numbers truncate from 3.33 to 3 and 1.33
  // to 1.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(3);
  p.remote_lower_bound = GetRemoteTimeTicks(1);
  p.remote_upper_bound = GetRemoteTimeTicks(7);
  p.local_upper_bound = GetLocalTimeTicks(7);
  p.test_time = GetRemoteTimeTicks(3);
  p.test_delta = RemoteTimeDelta::FromMicroseconds(2);
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(4), results.result_time);
  EXPECT_EQ(LocalTimeDelta::FromMicroseconds(1), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, BackEndSkew) {
  // Like the previous test, but |remote_lower_bound| is coherent and
  // |remote_upper_bound| is skewed.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(1);
  p.remote_lower_bound = GetRemoteTimeTicks(1);
  p.remote_upper_bound = GetRemoteTimeTicks(7);
  p.local_upper_bound = GetLocalTimeTicks(5);
  p.test_time = GetRemoteTimeTicks(3);
  p.test_delta = RemoteTimeDelta::FromMicroseconds(2);
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(2), results.result_time);
  EXPECT_EQ(LocalTimeDelta::FromMicroseconds(1), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, Instantaneous) {
  // The bounds are all okay, but the |remote_lower_bound| and
  // |remote_upper_bound| have the same value. No adjustments should be made and
  // no divide-by-zero errors should occur.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(1);
  p.remote_lower_bound = GetRemoteTimeTicks(2);
  p.remote_upper_bound = GetRemoteTimeTicks(2);
  p.local_upper_bound = GetLocalTimeTicks(3);
  p.test_time = GetRemoteTimeTicks(2);
  p.test_delta = RemoteTimeDelta();
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(2), results.result_time);
  EXPECT_EQ(LocalTimeDelta(), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, OffsetInstantaneous) {
  // The bounds are all okay, but the |remote_lower_bound| and
  // |remote_upper_bound| have the same value and are offset from the midpoint
  // of |local_lower_bound| and |local_upper_bound|. An offset should be applied
  // to make the midpoints line up.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(1);
  p.remote_lower_bound = GetRemoteTimeTicks(3);
  p.remote_upper_bound = GetRemoteTimeTicks(3);
  p.local_upper_bound = GetLocalTimeTicks(3);
  p.test_time = GetRemoteTimeTicks(3);
  p.test_delta = RemoteTimeDelta();
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(2), results.result_time);
  EXPECT_EQ(LocalTimeDelta(), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DisjointInstantaneous) {
  // |local_lower_bound| and |local_upper_bound| are the same. No matter what
  // the other values are, they must fit within [local_lower_bound,
  // local_upper_bound].  So, all of the values should be adjusted so they are
  // exactly that value.
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(1);
  p.remote_lower_bound = GetRemoteTimeTicks(2);
  p.remote_upper_bound = GetRemoteTimeTicks(2);
  p.local_upper_bound = GetLocalTimeTicks(1);
  p.test_time = GetRemoteTimeTicks(2);
  p.test_delta = RemoteTimeDelta();
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(1), results.result_time);
  EXPECT_EQ(LocalTimeDelta(), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, RoundingNearEdges) {
  // Verify that rounding never causes a value to appear outside the given
  // |local_*| range.
  const int kMaxRange = 101;
  for (int i = 1; i < kMaxRange; ++i) {
    for (int j = 1; j < kMaxRange; ++j) {
      TestParams p;
      p.local_lower_bound = GetLocalTimeTicks(1);
      p.remote_lower_bound = GetRemoteTimeTicks(1);
      p.remote_upper_bound = GetRemoteTimeTicks(j);
      p.local_upper_bound = GetLocalTimeTicks(i);

      p.test_time = GetRemoteTimeTicks(1);
      p.test_delta = RemoteTimeDelta();
      TestResults results = RunTest(p);
      EXPECT_LE(GetLocalTimeTicks(1), results.result_time);
      EXPECT_EQ(LocalTimeDelta(), results.result_delta);

      p.test_time = GetRemoteTimeTicks(j);
      p.test_delta = RemoteTimeDelta::FromMicroseconds(j - 1);
      results = RunTest(p);
      EXPECT_LE(results.result_time, GetLocalTimeTicks(i));
      EXPECT_LE(results.result_delta, LocalTimeDelta::FromMicroseconds(i - 1));
    }
  }
}

TEST(InterProcessTimeTicksConverterTest, DisjointRanges) {
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(10);
  p.remote_lower_bound = GetRemoteTimeTicks(30);
  p.remote_upper_bound = GetRemoteTimeTicks(41);
  p.local_upper_bound = GetLocalTimeTicks(20);
  p.test_time = GetRemoteTimeTicks(41);
  p.test_delta = RemoteTimeDelta();
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(20), results.result_time);
  EXPECT_EQ(LocalTimeDelta(), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, LargeValue_LocalIsLargetThanRemote) {
  constexpr auto kWeek = base::TimeTicks::kMicrosecondsPerWeek;
  constexpr auto kHour = base::TimeTicks::kMicrosecondsPerHour;
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(4 * kWeek);
  p.remote_lower_bound = GetRemoteTimeTicks(4 * kWeek + 2 * kHour);
  p.remote_upper_bound = GetRemoteTimeTicks(4 * kWeek + 4 * kHour);
  p.local_upper_bound = GetLocalTimeTicks(4 * kWeek + 8 * kHour);

  p.test_time = GetRemoteTimeTicks(4 * kWeek + 3 * kHour);
  p.test_delta = RemoteTimeDelta();
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(4 * kWeek + 4 * kHour), results.result_time);
  EXPECT_EQ(LocalTimeDelta(), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, LargeValue_RemoteIsLargetThanLocal) {
  constexpr auto kWeek = base::TimeTicks::kMicrosecondsPerWeek;
  constexpr auto kHour = base::TimeTicks::kMicrosecondsPerHour;
  TestParams p;
  p.local_lower_bound = GetLocalTimeTicks(4 * kWeek);
  p.remote_lower_bound = GetRemoteTimeTicks(5 * kWeek);
  p.remote_upper_bound = GetRemoteTimeTicks(5 * kWeek + 2 * kHour);
  p.local_upper_bound = GetLocalTimeTicks(4 * kWeek + kHour);

  p.test_time = GetRemoteTimeTicks(5 * kWeek + kHour);
  p.test_delta = RemoteTimeDelta();
  TestResults results = RunTest(p);
  EXPECT_EQ(GetLocalTimeTicks(4 * kWeek + kHour / 2), results.result_time);
  EXPECT_EQ(LocalTimeDelta(), results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, ValuesOutsideOfRange) {
  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(TicksFromMicroseconds(15)),
      LocalTimeTicks::FromTimeTicks(TicksFromMicroseconds(20)),
      RemoteTimeTicks::FromTimeTicks(TicksFromMicroseconds(10)),
      RemoteTimeTicks::FromTimeTicks(TicksFromMicroseconds(25)));

  RemoteTimeTicks remote_ticks =
      RemoteTimeTicks::FromTimeTicks(TicksFromMicroseconds(10));
  int64_t result = converter.ToLocalTimeTicks(remote_ticks)
                       .ToTimeTicks()
                       .since_origin()
                       .InMicroseconds();
  EXPECT_EQ(15, result);

  remote_ticks = RemoteTimeTicks::FromTimeTicks(TicksFromMicroseconds(25));
  result = converter.ToLocalTimeTicks(remote_ticks)
               .ToTimeTicks()
               .since_origin()
               .InMicroseconds();
  EXPECT_EQ(20, result);

  remote_ticks = RemoteTimeTicks::FromTimeTicks(TicksFromMicroseconds(9));
  result = converter.ToLocalTimeTicks(remote_ticks)
               .ToTimeTicks()
               .since_origin()
               .InMicroseconds();
  EXPECT_EQ(14, result);
}

}  // anonymous namespace

}  // namespace blink
