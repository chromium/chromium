// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_op_scheduler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace background_sync_op_scheduler_unittest {

class TestTask {
 public:
  explicit TestTask(BackgroundSyncOpScheduler* scheduler)
      : scheduler_(scheduler), callback_count_(0) {}

  virtual void Run() {
    callback_count_++;
    run_loop_.Quit();
  }
  void Done() { scheduler_->CompleteOperationAndRunNext(); }

  int callback_count() const { return callback_count_; }
  base::RunLoop& run_loop() { return run_loop_; }

 protected:
  raw_ptr<BackgroundSyncOpScheduler> scheduler_;
  base::RunLoop run_loop_;
  int callback_count_;
};

class TestScheduler : public BackgroundSyncOpScheduler {
 public:
  TestScheduler()
      : BackgroundSyncOpScheduler(
            base::SingleThreadTaskRunner::GetCurrentDefault()) {}

  void SetDoneStartingClosure(base::OnceClosure done_closure) {
    CHECK(!done_closure_);
    done_closure_ = std::move(done_closure);
  }

 protected:
  void DoneStartingAvailableOperations() override {
    if (done_closure_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(done_closure_));
    }
    BackgroundSyncOpScheduler::DoneStartingAvailableOperations();
  }

  base::OnceClosure done_closure_;
};

class BackgroundSyncOpSchedulerTest : public testing::Test {
 protected:
  BackgroundSyncOpSchedulerTest()
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

TEST_F(BackgroundSyncOpSchedulerTest, ScheduleOne) {
  base::RunLoop done_loop;
  scheduler_.SetDoneStartingClosure(done_loop.QuitClosure());
  scheduler_.ScheduleOperation(
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  task1_.run_loop().Run();
  done_loop.Run();
  EXPECT_EQ(1, task1_.callback_count());
}

TEST_F(BackgroundSyncOpSchedulerTest, ScheduledOperations) {
  base::RunLoop done_loop;
  scheduler_.SetDoneStartingClosure(done_loop.QuitClosure());
  scheduler_.ScheduleOperation(
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task1_.run_loop().Run();
  done_loop.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task1_.Done();
  EXPECT_FALSE(scheduler_.ScheduledOperations());
}

TEST_F(BackgroundSyncOpSchedulerTest, ScheduleTwoExclusive) {
  scheduler_.ScheduleOperation(
      base::BindOnce(&TestTask::Run, base::Unretained(&task1_)));
  base::RunLoop done_loop1;
  scheduler_.SetDoneStartingClosure(done_loop1.QuitClosure());
  scheduler_.ScheduleOperation(
      base::BindOnce(&TestTask::Run, base::Unretained(&task2_)));

  // Should only run the first exclusive op.
  task1_.run_loop().Run();
  done_loop1.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(0, task2_.callback_count());

  base::RunLoop done_loop2;
  scheduler_.SetDoneStartingClosure(done_loop2.QuitClosure());

  // Should run the second exclusive op after the first completes.
  task1_.Done();
  EXPECT_TRUE(scheduler_.ScheduledOperations());
  task2_.run_loop().Run();
  done_loop2.Run();
  EXPECT_EQ(1, task1_.callback_count());
  EXPECT_EQ(1, task2_.callback_count());
}

}  // namespace background_sync_op_scheduler_unittest
}  // namespace content
