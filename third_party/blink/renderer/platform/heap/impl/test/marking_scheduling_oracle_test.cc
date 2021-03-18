// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/impl/marking_scheduling_oracle.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

namespace blink {

namespace {
class MarkingSchedulingOracleTest : public testing::Test {
 public:
  static constexpr size_t kObjectSize = 1024 * 1024 * 1024;
};
}  // namespace

TEST_F(MarkingSchedulingOracleTest, FirstStepReturnsDefaultDuration) {
  MarkingSchedulingOracle oracle;
  oracle.SetElapsedTimeForTesting(0);
  EXPECT_EQ(MarkingSchedulingOracle::kDefaultIncrementalMarkingStepDuration,
            oracle.GetNextIncrementalStepDurationForTask(kObjectSize));
}

// If marking is not behind schedule and very small time passed between steps
// the oracle should return the minimum step duration.
TEST_F(MarkingSchedulingOracleTest, NoTimePassedReturnsMinimumDuration) {
  MarkingSchedulingOracle oracle;
  // Add incrementally marked bytes to tell oracle this is not the first step.
  oracle.UpdateIncrementalMarkingStats(
      MarkingSchedulingOracle::kMinimumMarkedBytesInStep,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(0));
  oracle.SetElapsedTimeForTesting(0);
  // Given marking speed set above, Minimum duration should be 1ms.
  EXPECT_EQ(1, oracle.GetNextIncrementalStepDurationForTask(kObjectSize)
                   .InMilliseconds());
}

TEST_F(MarkingSchedulingOracleTest, OracleDoesntExccedMaximumStepDuration) {
  MarkingSchedulingOracle oracle;
  // Add incrementally marked bytes to tell oracle this is not the first step.
  oracle.UpdateIncrementalMarkingStats(
      1,
      base::TimeDelta::FromMilliseconds(
          MarkingSchedulingOracle::kEstimatedMarkingTimeMs),
      base::TimeDelta::FromMilliseconds(0));
  oracle.SetElapsedTimeForTesting(
      MarkingSchedulingOracle::kEstimatedMarkingTimeMs);
  EXPECT_EQ(MarkingSchedulingOracle::kMaximumIncrementalMarkingStepDuration,
            oracle.GetNextIncrementalStepDurationForTask(kObjectSize));
}

TEST_F(MarkingSchedulingOracleTest, AheadOfScheduleReturnsMinimumDuration) {
  MarkingSchedulingOracle oracle;
  // Add incrementally marked bytes to tell oracle this is not the first step.
  oracle.UpdateIncrementalMarkingStats(
      MarkingSchedulingOracle::kMinimumMarkedBytesInStep,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(0));
  oracle.AddConcurrentlyMarkedBytes(0.6 * kObjectSize);
  oracle.SetElapsedTimeForTesting(
      0.5 * MarkingSchedulingOracle::kEstimatedMarkingTimeMs);
  // Given marking speed set above, Minimum duration should be 1ms.
  EXPECT_EQ(1, oracle.GetNextIncrementalStepDurationForTask(kObjectSize)
                   .InMilliseconds());
}

TEST_F(MarkingSchedulingOracleTest, BehindScheduleReturnsCorrectDuration) {
  static constexpr size_t kForegoundMarkingTime = 1;
  MarkingSchedulingOracle oracle;
  oracle.UpdateIncrementalMarkingStats(
      0.1 * kObjectSize,
      base::TimeDelta::FromMilliseconds(kForegoundMarkingTime),
      base::TimeDelta::FromMilliseconds(0));
  oracle.AddConcurrentlyMarkedBytes(0.25 * kObjectSize);
  oracle.SetElapsedTimeForTesting(
      0.5 * MarkingSchedulingOracle::kEstimatedMarkingTimeMs);
  EXPECT_EQ(1.5 * kForegoundMarkingTime,
            oracle.GetNextIncrementalStepDurationForTask(kObjectSize)
                .InMillisecondsF());
  oracle.AddConcurrentlyMarkedBytes(0.05 * kObjectSize);
  oracle.SetElapsedTimeForTesting(
      0.5 * MarkingSchedulingOracle::kEstimatedMarkingTimeMs);
  EXPECT_EQ(kForegoundMarkingTime,
            oracle.GetNextIncrementalStepDurationForTask(kObjectSize)
                .InMillisecondsF());
  oracle.AddConcurrentlyMarkedBytes(0.05 * kObjectSize);
  oracle.SetElapsedTimeForTesting(
      0.5 * MarkingSchedulingOracle::kEstimatedMarkingTimeMs);
  EXPECT_EQ(0.5 * kForegoundMarkingTime,
            oracle.GetNextIncrementalStepDurationForTask(kObjectSize)
                .InMillisecondsF());
}

}  // namespace blink
