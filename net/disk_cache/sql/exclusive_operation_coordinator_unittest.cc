// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/exclusive_operation_coordinator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace disk_cache {

class ExclusiveOperationCoordinatorTest : public testing::Test {
 protected:
  using OperationHandle = ExclusiveOperationCoordinator::OperationHandle;

  ExclusiveOperationCoordinator coordinator_;
  std::vector<std::string> execution_log_;
};

TEST_F(ExclusiveOperationCoordinatorTest, ExclusiveOperationRunsWhenIdle) {
  bool executed = false;
  coordinator_.PostOrRunExclusiveOperation(base::BindLambdaForTesting(
      [&](std::unique_ptr<OperationHandle> handle) { executed = true; }));

  EXPECT_TRUE(executed);
}

TEST_F(ExclusiveOperationCoordinatorTest,
       ExclusiveOperationDoesNotRunWhileAnotherIsRunning) {
  std::unique_ptr<OperationHandle> exclusive_handle1;
  coordinator_.PostOrRunExclusiveOperation(
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("E1_start");
        exclusive_handle1 = std::move(handle);
      }));

  ASSERT_TRUE(exclusive_handle1);
  EXPECT_THAT(execution_log_, ElementsAre("E1_start"));

  // Post another exclusive operation. This should be queued.
  bool e2_executed = false;
  coordinator_.PostOrRunExclusiveOperation(
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("E2_start");
        e2_executed = true;
      }));

  // E2 should not have run yet because E1 is in flight. The call to
  // PostOrRunExclusiveOperation for E2 will call TryToRunNextOperation, which
  // should return early because `exclusive_operation_running_` is true.
  EXPECT_FALSE(e2_executed);
  EXPECT_THAT(execution_log_, ElementsAre("E1_start"));

  // Complete E1. Now E2 should run.
  execution_log_.push_back("E1_end");
  exclusive_handle1.reset();
  EXPECT_TRUE(e2_executed);
  EXPECT_THAT(execution_log_, ElementsAre("E1_start", "E1_end", "E2_start"));
}

TEST_F(ExclusiveOperationCoordinatorTest, NormalOperationsAreSerial) {
  const CacheEntryKey kKey("my_key");
  std::unique_ptr<OperationHandle> handle1;
  coordinator_.PostOrRunNormalOperation(
      kKey,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N1_start");
        handle1 = std::move(handle);
      }));

  ASSERT_TRUE(handle1);
  EXPECT_THAT(execution_log_, ElementsAre("N1_start"));

  std::unique_ptr<OperationHandle> handle2;
  coordinator_.PostOrRunNormalOperation(
      kKey,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N2_start");
        handle2 = std::move(handle);
      }));

  // N2 should not have run yet.
  EXPECT_FALSE(handle2);
  EXPECT_THAT(execution_log_, ElementsAre("N1_start"));

  // Complete N1.
  execution_log_.push_back("N1_end");
  handle1.reset();

  // Now N2 should run.
  ASSERT_TRUE(handle2);
  EXPECT_THAT(execution_log_, ElementsAre("N1_start", "N1_end", "N2_start"));
}

TEST_F(ExclusiveOperationCoordinatorTest,
       NormalOperationsWithDifferentKeysRunConcurrently) {
  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");
  std::unique_ptr<OperationHandle> handle1;
  std::unique_ptr<OperationHandle> handle2;

  coordinator_.PostOrRunNormalOperation(
      kKey1,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        handle1 = std::move(handle);
      }));

  coordinator_.PostOrRunNormalOperation(
      kKey2,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        handle2 = std::move(handle);
      }));

  // Both operations should have run and given us a handle.
  EXPECT_TRUE(handle1);
  EXPECT_TRUE(handle2);
}

TEST_F(ExclusiveOperationCoordinatorTest, KeyedNormalWaitsForExclusive) {
  std::unique_ptr<OperationHandle> exclusive_handle;
  coordinator_.PostOrRunExclusiveOperation(
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        exclusive_handle = std::move(handle);
      }));

  ASSERT_TRUE(exclusive_handle);

  bool normal_executed = false;
  coordinator_.PostOrRunNormalOperation(
      CacheEntryKey("some_key"),
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        normal_executed = true;
      }));

  // The normal operation should not have run yet.
  EXPECT_FALSE(normal_executed);

  // Now, complete the exclusive operation.
  exclusive_handle.reset();

  // The normal operation should run now.
  EXPECT_TRUE(normal_executed);
}

TEST_F(ExclusiveOperationCoordinatorTest, ExclusiveOperationWaitsForNormal) {
  const CacheEntryKey kKey("my_key");
  std::unique_ptr<OperationHandle> normal_handle;
  coordinator_.PostOrRunNormalOperation(
      kKey,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N1_start");
        normal_handle = std::move(handle);
      }));

  ASSERT_TRUE(normal_handle);
  EXPECT_THAT(execution_log_, ElementsAre("N1_start"));

  // Post an exclusive operation. This should be queued because a normal
  // operation is in flight.
  bool exclusive_executed = false;
  coordinator_.PostOrRunExclusiveOperation(
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("E1_start");
        exclusive_executed = true;
      }));

  // The exclusive operation should not have run yet. The call to
  // PostOrRunExclusiveOperation will call TryToRunNextOperation, which should
  // return early because active_normal_operations_ > 0.
  EXPECT_FALSE(exclusive_executed);
  EXPECT_THAT(execution_log_, ElementsAre("N1_start"));

  // Complete the normal operation. Now the exclusive operation should run.
  execution_log_.push_back("N1_end");
  normal_handle.reset();
  EXPECT_TRUE(exclusive_executed);
  EXPECT_THAT(execution_log_, ElementsAre("N1_start", "N1_end", "E1_start"));
}

TEST_F(ExclusiveOperationCoordinatorTest,
       QueuedKeyedNormalOperationsRunAfterExclusive) {
  std::unique_ptr<OperationHandle> exclusive_handle;
  coordinator_.PostOrRunExclusiveOperation(
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("E_start");
        exclusive_handle = std::move(handle);
      }));

  ASSERT_TRUE(exclusive_handle);

  const CacheEntryKey kKey1("key1");
  const CacheEntryKey kKey2("key2");

  std::unique_ptr<OperationHandle> n1a_handle;
  std::unique_ptr<OperationHandle> n2_handle;

  coordinator_.PostOrRunNormalOperation(
      kKey1,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N1a_run");
        n1a_handle = std::move(handle);
      }));

  coordinator_.PostOrRunNormalOperation(
      kKey2,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N2_run");
        n2_handle = std::move(handle);
      }));

  // This operation uses the same key as the first normal operation.
  coordinator_.PostOrRunNormalOperation(
      kKey1,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N1b_run");
      }));

  // Normal operations should be queued.
  EXPECT_THAT(execution_log_, ElementsAre("E_start"));

  // Complete the exclusive operation.
  execution_log_.push_back("E_end");
  exclusive_handle.reset();

  // N1a and N2 should have run now. N1b is queued behind N1a.
  EXPECT_TRUE(n1a_handle);
  EXPECT_TRUE(n2_handle);
  EXPECT_THAT(execution_log_,
              ElementsAre("E_start", "E_end", "N1a_run", "N2_run"));

  // Complete N1a.
  execution_log_.push_back("N1a_end");
  n1a_handle.reset();

  // N1b should run now.
  EXPECT_THAT(execution_log_, ElementsAre("E_start", "E_end", "N1a_run",
                                          "N2_run", "N1a_end", "N1b_run"));
}

TEST_F(ExclusiveOperationCoordinatorTest,
       OlderNormalOpRunsBeforeNewerExclusiveOp) {
  // Start an exclusive operation (E1).
  std::unique_ptr<OperationHandle> exclusive_handle1;
  coordinator_.PostOrRunExclusiveOperation(
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("E1_start");
        exclusive_handle1 = std::move(handle);
      }));
  ASSERT_TRUE(exclusive_handle1);
  EXPECT_THAT(execution_log_, ElementsAre("E1_start"));

  // Queue a normal operation (N1).
  const CacheEntryKey kKey("my_key");
  std::unique_ptr<OperationHandle> n1_handle;
  coordinator_.PostOrRunNormalOperation(
      kKey,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N1_start");
        n1_handle = std::move(handle);
      }));

  // Queue another exclusive operation (E2).
  std::unique_ptr<OperationHandle> exclusive_handle2;
  coordinator_.PostOrRunExclusiveOperation(
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("E2_start");
        exclusive_handle2 = std::move(handle);
      }));
  // Neither N1 nor E2 should have run yet.
  EXPECT_FALSE(n1_handle);
  EXPECT_FALSE(exclusive_handle2);
  EXPECT_THAT(execution_log_, ElementsAre("E1_start"));

  // Complete E1. N1 should run next.
  execution_log_.push_back("E1_end");
  exclusive_handle1.reset();

  ASSERT_TRUE(n1_handle);
  EXPECT_FALSE(exclusive_handle2);
  EXPECT_THAT(execution_log_, ElementsAre("E1_start", "E1_end", "N1_start"));

  // Complete N1. E2 should run now.
  execution_log_.push_back("N1_end");
  n1_handle.reset();

  ASSERT_TRUE(exclusive_handle2);
  EXPECT_THAT(execution_log_, ElementsAre("E1_start", "E1_end", "N1_start",
                                          "N1_end", "E2_start"));
}

TEST_F(ExclusiveOperationCoordinatorTest, ExclusiveOperationDoesNotStarve) {
  // Start a normal operation N1a.
  const CacheEntryKey kKey1("key1");
  std::unique_ptr<OperationHandle> n1a_handle;
  coordinator_.PostOrRunNormalOperation(
      kKey1,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N1a_start");
        n1a_handle = std::move(handle);
      }));

  ASSERT_TRUE(n1a_handle);
  EXPECT_THAT(execution_log_, ElementsAre("N1a_start"));

  // While N1 is running, queue an exclusive operation E1.
  std::unique_ptr<OperationHandle> e1_handle;
  coordinator_.PostOrRunExclusiveOperation(
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("E1_start");
        e1_handle = std::move(handle);
      }));

  // E1 should not run yet.
  EXPECT_FALSE(e1_handle);

  // While N1a is still running, queue another normal operation N1a with the
  // same key.
  std::unique_ptr<OperationHandle> n1b_handle;
  coordinator_.PostOrRunNormalOperation(
      kKey1,
      base::BindLambdaForTesting([&](std::unique_ptr<OperationHandle> handle) {
        execution_log_.push_back("N1b_start");
        n1b_handle = std::move(handle);
      }));

  // N1b should not run yet.
  EXPECT_FALSE(n1b_handle);
  EXPECT_THAT(execution_log_, ElementsAre("N1a_start"));

  // Complete N1.
  execution_log_.push_back("N1a_end");
  n1a_handle.reset();

  // E1 should run next, not N1b. This shows E1 isn't starved.
  ASSERT_TRUE(e1_handle);
  EXPECT_FALSE(n1b_handle);
  EXPECT_THAT(execution_log_, ElementsAre("N1a_start", "N1a_end", "E1_start"));

  // Complete E1.
  execution_log_.push_back("E1_end");
  e1_handle.reset();

  // N1b should run.
  ASSERT_TRUE(n1b_handle);
  EXPECT_THAT(execution_log_, ElementsAre("N1a_start", "N1a_end", "E1_start",
                                          "E1_end", "N1b_start"));
}

}  // namespace disk_cache
