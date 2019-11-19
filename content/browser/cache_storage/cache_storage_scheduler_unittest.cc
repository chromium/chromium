// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_scheduler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/content_features.h"
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

  virtual void Run() {
    callback_count_++;
    run_loop_.Quit();
  }
  void Done() { scheduler_->CompleteOperationAndRunNext(id_); }

  int callback_count() const { return callback_count_; }
  CacheStorageSchedulerId id() const { return id_; }
  base::RunLoop& run_loop() { return run_loop_; }

 protected:
  CacheStorageScheduler* scheduler_;
  const CacheStorageSchedulerId id_;
  base::RunLoop run_loop_;
  int callback_count_;
};

class TestScheduler : public CacheStorageScheduler {
 public:
  TestScheduler()
      : CacheStorageScheduler(CacheStorageSchedulerClient::kStorage,
                              base::ThreadTaskRunnerHandle::Get()) {}

  void SetDoneStartingClosure(base::OnceClosure done_closure) {
    CHECK(!done_closure_);
    done_closure_ = std::move(done_closure);
  }

 protected:
  void DoneStartingAvailableOperations() override {
    if (done_closure_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    std::move(done_closure_));
    }
    CacheStorageScheduler::DoneStartingAvailableOperations();
  }

  base::OnceClosure done_closure_;
};

class CacheStorageSchedulerTest : public testing::Test {
 protected:
  CacheStorageSchedulerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        task1_(&scheduler_),
        task2_(&scheduler_),
        task3_(&scheduler_) {}

  BrowserTaskEnvironment task_environment_;
  TestScheduler scheduler_;
  TestTask task1_;
  TestTask task2_;
  TestTask task3_;
};

TEST_F(CacheStorageSchedulerTest, ScheduleOne) {
  base::RunLoop done_loop;
  scheduler_.SetDoneStartingClosure(done_loop.QuitClosure());
  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  task1_.run_loop().Run();
  done_loop.Run();
  EXPECT_EQ(1, task1_.callback_count());
}

TEST_F(CacheStorageSchedulerTest, ScheduledOperations) {
  base::RunLoop done_loop;
  scheduler_.SetDoneStartingClosure(done_loop.QuitClosure());
  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task1_.run_loop().Run();
  done_loop.Run();
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
      features::kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run the first exclusive op.
  task1_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Should run the second exclusive op after the first completes.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());
}

TEST_F(CacheStorageSchedulerTest, ScheduleTwoShared) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should run both shared ops in paralle.
  task1_.run_loop().Run();
  task2_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Completing the first op should trigger a check for new ops
  // which will not be present here.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop3;
  scheduler_.SetDoneStartingClosure(done_loop3.QuitClosure());

  // Completing the second op should result in the scheduler
  // becoming idle.
  task2_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
  done_loop3.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
}

TEST_F(CacheStorageSchedulerTest, ScheduleOneExclusiveOneShared) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run the first exclusive op.
  task1_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Should run the second shared op after the first is completed.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  task2_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
}

TEST_F(CacheStorageSchedulerTest, ScheduleOneSharedOneExclusive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run the first shared op.
  task1_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Should run the second exclusive op after the first completes.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  task2_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
}

TEST_F(CacheStorageSchedulerTest, ScheduleTwoSharedOneExclusive) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      task3_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task3_)));

  // Should run the two shared ops in parallel.
  task1_.run_loop().Run();
  task2_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Completing the first shared op should not allow the exclusive op
  // to run yet.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop3;
  scheduler_.SetDoneStartingClosure(done_loop3.QuitClosure());

  // The third exclusive op should run after both the preceding shared ops
  // complete.
  task2_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task3_.run_loop().Run();
  done_loop3.Run();
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
      features::kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      task3_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task3_)));

  // Should only run the first exclusive op.
  task1_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Should run both the shared ops in parallel after the first exclusive
  // op is completed.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  task3_.run_loop().Run();
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop3;
  scheduler_.SetDoneStartingClosure(done_loop3.QuitClosure());

  task2_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  done_loop3.Run();
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
      features::kCacheStorageParallelOps, {{"max_shared_ops", "3"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      task3_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task3_)));

  // Should only run the first shared op.
  task1_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Should run the exclusive op after the first op is completed.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());
  EXPECT_TRUE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop3;
  scheduler_.SetDoneStartingClosure(done_loop3.QuitClosure());

  // Should run the last shared op after the preceding exclusive op
  // is completed.
  task2_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task3_.run_loop().Run();
  done_loop3.Run();
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
      features::kCacheStorageParallelOps, {{"max_shared_ops", "1"}});

  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      task2_.id(), CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run one shared op since the max shared is set to 1.
  task1_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Should run the next shared op after the first completes.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_FALSE(scheduler_.IsRunningExclusiveOperation());
}

TEST_F(CacheStorageSchedulerTest, ScheduleByPriorityTwoNormalOneHigh) {
  scheduler_.ScheduleOperation(
      task1_.id(), CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kTest, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
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
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_EQ(0, task3_.callback_count());

  base::RunLoop done_loop3;
  scheduler_.SetDoneStartingClosure(done_loop3.QuitClosure());

  // Should run the high priority op next.
  task1_.Done();
  task3_.run_loop().Run();
  done_loop3.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Should run the final normal priority op after the high priority op
  // completes.
  task3_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
  EXPECT_EQ(1, task3_.callback_count());
}

}  // namespace cache_storage_scheduler_unittest
}  // namespace content
