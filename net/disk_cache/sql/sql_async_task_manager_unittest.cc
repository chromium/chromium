// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_async_task_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/disk_cache/sql/sql_async_task_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class SqlAsyncTaskManagerTest : public testing::Test {
 public:
  SqlAsyncTaskManagerTest() = default;

 protected:
  void RunOnAllTasksComplete(base::OnceClosure callback) {
    manager_.RunOnAllTasksCompleteForTest(std::move(callback));
  }

  base::test::TaskEnvironment task_environment_;
  SqlAsyncTaskManager manager_;
};

TEST_F(SqlAsyncTaskManagerTest, Basic) {
  std::unique_ptr<SqlAsyncTaskToken> token = manager_.StartTask();
  token.reset();

  manager_.RunUntilAllTasksCompleteForTest();
}

TEST_F(SqlAsyncTaskManagerTest, RunImmediatelyIfNoTasks) {
  manager_.RunUntilAllTasksCompleteForTest();
}

TEST_F(SqlAsyncTaskManagerTest, MultipleTasks) {
  std::unique_ptr<SqlAsyncTaskToken> token1 = manager_.StartTask();
  std::unique_ptr<SqlAsyncTaskToken> token2 = manager_.StartTask();
  std::unique_ptr<SqlAsyncTaskToken> token3 = manager_.StartTask();

  token2.reset();
  token1.reset();
  token3.reset();

  manager_.RunUntilAllTasksCompleteForTest();
}

TEST_F(SqlAsyncTaskManagerTest, StartTaskAfterCountZeroInSameTask) {
  // This test verifies that if StartTask is called in the same task
  // after pending_task_count_ becomes 0, it doesn't trigger the callback.
  base::RunLoop run_loop;
  bool completed = false;

  RunOnAllTasksComplete(base::BindLambdaForTesting([&]() {
    completed = true;
    run_loop.Quit();
  }));

  {
    std::unique_ptr<SqlAsyncTaskToken> token1 = manager_.StartTask();
    // pending_task_count_ becomes 0, posts CheckAndRunCallback.
    token1.reset();

    // StartTask in the same task.
    std::unique_ptr<SqlAsyncTaskToken> token2 = manager_.StartTask();

    // The posted CheckAndRunCallback should see pending_task_count_ is 1.
    base::RunLoop run_loop_quiescence;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_quiescence.QuitClosure());
    run_loop_quiescence.Run();
    EXPECT_FALSE(completed);

    token2.reset();  // posts another CheckAndRunCallback.
  }

  run_loop.Run();
  EXPECT_TRUE(completed);
}

TEST_F(SqlAsyncTaskManagerTest, MultiStepQuiescence) {
  // This simulates a multi-step process where one task's completion
  // triggers the next one.
  base::RunLoop run_loop;
  int step = 0;

  RunOnAllTasksComplete(base::BindLambdaForTesting([&]() {
    EXPECT_EQ(3, step);
    run_loop.Quit();
  }));

  base::RepeatingClosure run_step;
  run_step = base::BindLambdaForTesting([&]() {
    if (step < 3) {
      std::unique_ptr<SqlAsyncTaskToken> token = manager_.StartTask();
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindLambdaForTesting([run_step, t = std::move(token),
                                                 &step]() mutable {
            step++;
            t.reset();  // count might hit 0 here.
            run_step
                .Run();  // if step < 3, count becomes 1 again in the same task.
          }));
    }
  });

  run_step.Run();

  run_loop.Run();
  EXPECT_EQ(3, step);
}

TEST_F(SqlAsyncTaskManagerTest, ManagerDestroyedFirst) {
  auto local_manager = std::make_unique<SqlAsyncTaskManager>();
  std::unique_ptr<SqlAsyncTaskToken> token = local_manager->StartTask();

  local_manager.reset();
  // Token destruction should not crash even if manager is gone.
  token.reset();
}

TEST_F(SqlAsyncTaskManagerTest, MultipleTasksStartedAndDestroyed) {
  std::unique_ptr<SqlAsyncTaskToken> token1 = manager_.StartTask();
  token1.reset();

  std::unique_ptr<SqlAsyncTaskToken> token2 = manager_.StartTask();
  token2.reset();

  manager_.RunUntilAllTasksCompleteForTest();
}

}  // namespace disk_cache
