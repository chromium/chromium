// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/capabilities/pending_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

class PendingOperationsTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_;
};

// Test that histogram is created with correct data.
TEST_F(PendingOperationsTest, OperationTiming) {
  const std::string kUmaPrefix = "Media.PendingOperations.";
  const std::string kOperation = "init";
  constexpr base::TimeDelta kInitDelay = base::Seconds(2);
  PendingOperations pending_operations(kUmaPrefix);
  PendingOperations::Id init_id = pending_operations.Start(kOperation);
  EXPECT_EQ(pending_operations.get_pending_ops_for_test().size(), 1u);
  task_environment_.FastForwardBy(kInitDelay);
  pending_operations.Complete(init_id);

  // No pending operations.
  EXPECT_TRUE(pending_operations.get_pending_ops_for_test().empty());
  // UMA histogram emitted.
  histogram_.ExpectUniqueSample(kUmaPrefix + kOperation,
                                kInitDelay.InMicroseconds(), 1);
}

// Test that timeout histogram works.
TEST_F(PendingOperationsTest, OperationTimeout) {
  const std::string kUmaPrefix = "Media.PendingOperations.";
  const std::string kOperation = "read";
  constexpr base::TimeDelta kLongTimeout = base::Hours(1);
  // Current setting of the pending operation timeout.
  constexpr base::TimeDelta kPendingOperationTimeout = base::Seconds(30);
  PendingOperations pending_operations(kUmaPrefix);
  pending_operations.Start(kOperation);
  EXPECT_EQ(pending_operations.get_pending_ops_for_test().size(), 1u);
  task_environment_.FastForwardBy(kLongTimeout);

  // No pending operations.
  EXPECT_TRUE(pending_operations.get_pending_ops_for_test().empty());
  // UMA histogram emitted, operation reported as timeout.
  histogram_.ExpectUniqueSample(kUmaPrefix + kOperation,
                                kPendingOperationTimeout.InMicroseconds(), 1);
}

// Nested operations.
struct SimulatedOperation {
  std::string name;
  base::TimeDelta start_time;
  base::TimeDelta stop_time;
  PendingOperations::Id id;
};

TEST_F(PendingOperationsTest, NestedOperation) {
  const std::string kUmaPrefix = "Media.PendingOperations.";
  PendingOperations pending_operations(kUmaPrefix);
  // A list of operations named after their start and stop time.
  SimulatedOperation operations[] = {
      {"0_10", base::Milliseconds(0), base::Milliseconds(10), 0},
      {"5_15", base::Milliseconds(5), base::Milliseconds(15), 0},
      {"6_30", base::Milliseconds(6), base::Milliseconds(30), 0},
      {"10_12", base::Milliseconds(10), base::Milliseconds(12), 0},
      {"20_27", base::Milliseconds(20), base::Milliseconds(27), 0},
      {"5_80", base::Milliseconds(5), base::Milliseconds(80), 0},
      {"30_60", base::Milliseconds(30), base::Milliseconds(60), 0},
      {"25_90", base::Milliseconds(25), base::Milliseconds(90), 0},
  };

  size_t expected_pending_operations = 0;
  // Run a loop from 0 to 100 ms.
  base::TimeDelta tick_length = base::Milliseconds(1);
  for (base::TimeDelta elapsed_time; elapsed_time < base::Milliseconds(100);
       elapsed_time += tick_length) {
    // Start/Complete each operation if the elapsed time has reached the
    // corresponding start/stop time.
    for (auto& operation : operations) {
      if (operation.start_time == elapsed_time) {
        operation.id = pending_operations.Start(operation.name);
        ++expected_pending_operations;
      }
      if (operation.stop_time == elapsed_time) {
        pending_operations.Complete(operation.id);
        --expected_pending_operations;
      }
    }
    EXPECT_EQ(pending_operations.get_pending_ops_for_test().size(),
              expected_pending_operations);
    task_environment_.FastForwardBy(tick_length);
  }

  for (const auto& operation : operations) {
    histogram_.ExpectUniqueSample(
        kUmaPrefix + operation.name,
        (operation.stop_time - operation.start_time).InMicroseconds(), 1);
  }
}

}  // namespace
}  // namespace media
