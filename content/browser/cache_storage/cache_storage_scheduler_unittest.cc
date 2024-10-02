// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_scheduler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace cache_storage_scheduler_unittest {

class TestTask {
 public:
  TestTask(CacheStorageScheduler* scheduler)
      : scheduler_(scheduler),
        id_(scheduler_->CreateId()),
        callback_count_(0) {}

  void Run() {
    callback_count_++;
    run_loop_.Quit();
  }
  void Done() { scheduler_->CompleteOperationAndRunNext(id_); }

  int callback_count() const { return callback_count_; }
  CacheStorageSchedulerId id() const { return id_; }
  base::RunLoop& run_loop() { return run_loop_; }

 protected:
  raw_ptr<CacheStorageScheduler> scheduler_;
  const CacheStorageSchedulerId id_;
  base::RunLoop run_loop_;
  int callback_count_;
};

class CacheStorageSchedulerTest : public testing::Test {
 protected:
  CacheStorageSchedulerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        task1_(&scheduler_),
        task2_(&scheduler_),
        task3_(&scheduler_) {}

  BrowserTaskEnvironment task_environment_;
  CacheStorageScheduler scheduler_{
      CacheStorageSchedulerClient::kStorage,
      base::SingleThreadTaskRunner::GetCurrentDefault()};
  TestTask task1_;
  TestTask task2_;
  TestTask task3_;
};

TEST_F(CacheStorageSchedulerTest, ScheduleOne) {
  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  // It's expected that the task will be executed synchronously.
  EXPECT_TRUE(task1_.run_loop().AnyQuitCalled());
}

TEST_F(CacheStorageSchedulerTest, ScheduledOperations) {
  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task1_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());
  task1_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());
}

TEST_F(CacheStorageSchedulerTest, ScheduleTwoExclusive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run the first exclusive op.
  task1_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  // Should run the second exclusive op after the first completes.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());
}

TEST_F(CacheStorageSchedulerTest, ScheduleTwoShared) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should run both shared ops in paralle.
  task1_.run_loop().Run();
  task2_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  // Completing the first op should trigger a check for new ops
  // which will not be present here.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  // Completing the second op should result in the scheduler
  // becoming idle.
  task2_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
}

TEST_F(CacheStorageSchedulerTest, ScheduleOneExclusiveOneShared) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run the first exclusive op.
  task1_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  // Should run the second shared op after the first is completed.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  task2_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
}

TEST_F(CacheStorageSchedulerTest, ScheduleOneSharedOneExclusive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run the first shared op.
  task1_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  // Should run the second exclusive op after the first completes.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  task2_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
}

TEST_F(CacheStorageSchedulerTest, ScheduleTwoSharedOneExclusive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));
  scheduler_.ScheduleOperation(
      task3_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task3_)));

  // Should run the two shared ops in parallel.
  task1_.run_loop().Run();
  task2_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  // Completing the first shared op should not allow the exclusive op
  // to run yet.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  // The third exclusive op should run after both the preceding shared ops
  // complete.
  task2_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task3_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  task3_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
}

TEST_F(CacheStorageSchedulerTest, ScheduleOneExclusiveTwoShared) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));
  scheduler_.ScheduleOperation(
      task3_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task3_)));

  // Should only run the first exclusive op.
  task1_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  // Should run both the shared ops in parallel after the first exclusive
  // op is completed.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  task3_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  task2_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  task3_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
}

TEST_F(CacheStorageSchedulerTest, ScheduleOneSharedOneExclusiveOneShared) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));
  scheduler_.ScheduleOperation(
      task3_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task3_)));

  // Should only run the first shared op.
  task1_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  // Should run the exclusive op after the first op is completed.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  // Should run the last shared op after the preceding exclusive op
  // is completed.
  task2_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task3_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  task3_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
}

TEST_F(CacheStorageSchedulerTest, ScheduleTwoSharedNotParallel) {
  // Disable parallelism
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kCacheStorageParallelOps, {{"max_shared_ops", "1"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run one shared op since the max shared is set to 1.
  task1_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  // Should run the next shared op after the first completes.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());
}

TEST_F(CacheStorageSchedulerTest, ScheduleByPriorityTwoNormalOneHigh) {
  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));
  scheduler_.ScheduleOperation(
      task3_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kHigh,
      base::BindOnce(&TestTask::Run, base::Unretained(&task3_)));

  // Should run the first normal priority op because the queue was empty
  // when it was added.
  task1_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());

  // Should run the high priority op next.
  task1_.Done();
  task3_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());

  // Should run the final normal priority op after the high priority op
  // completes.
  task3_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());
}

// Regression test for crbug.com/370069678 --- not crashing under ASAN indicates
// success.
TEST_F(CacheStorageSchedulerTest, TaskDeletesScheduler) {
  auto* scheduler = new CacheStorageScheduler(
      CacheStorageSchedulerClient::kStorage,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  scheduler->ScheduleOperation(
      1, CacheStorageSchedulerMode::kExclusive, CacheStorageSchedulerOp::kTest,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce([](CacheStorageScheduler* scheduler) { delete scheduler; },
                     scheduler));
}

}  // namespace cache_storage_scheduler_unittest
}  // namespace content
