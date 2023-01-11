// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_task_runner.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using testing::_;
using testing::Assign;
using testing::Invoke;

int observer_calls = 0;
int task_count = 0;
int observer_result;

void Observer(int result) {
  observer_calls++;
  observer_result = result;
}

class StartupTaskRunnerTest : public testing::Test {
 public:
  void SetUp() override {
    last_task_ = 0;
    observer_calls = 0;
    task_count = 0;
  }

  int Task1() {
    last_task_ = 1;
    task_count++;
    return 0;
  }

  int Task2() {
    last_task_ = 2;
    task_count++;
    return 0;
  }

  int FailingTask() {
    // Task returning failure
    last_task_ = 3;
    task_count++;
    return 1;
  }

  int GetLastTask() { return last_task_; }

 private:

  int last_task_;
};

// We can't use the real message loop, even if we want to, since doing so on
// Android requires a complex Java infrastructure. The test would have to built
// as a content_shell test; but content_shell startup invokes the class we are
// trying to test.
//
// The mocks are not directly in TaskRunnerProxy because reference counted
// objects seem to confuse the mocking framework

class MockTaskRunner {
 public:
  MOCK_METHOD2(PostDelayedTask, bool(const base::Location&, base::TimeDelta));
  MOCK_METHOD2(PostNonNestableDelayedTask,
               bool(const base::Location&, base::TimeDelta));
};

class TaskRunnerProxy : public base::SingleThreadTaskRunner {
 public:
  TaskRunnerProxy(MockTaskRunner* mock) : mock_(mock) {}
  bool RunsTasksInCurrentSequence() const override { return true; }
  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure closure,
                       base::TimeDelta delta) override {
    last_task_ = std::move(closure);
    return mock_->PostDelayedTask(location, delta);
  }
  bool PostNonNestableDelayedTask(const base::Location& location,
                                  base::OnceClosure closure,
                                  base::TimeDelta delta) override {
    last_task_ = std::move(closure);
    return mock_->PostNonNestableDelayedTask(location, delta);
  }

  base::OnceClosure TakeLastTaskClosure() { return std::move(last_task_); }

 private:
  ~TaskRunnerProxy() override {}

  raw_ptr<MockTaskRunner> mock_;
  base::OnceClosure last_task_;
};

TEST_F(StartupTaskRunnerTest, SynchronousExecution) {
  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _)).Times(0);
  EXPECT_CALL(mock_runner, PostNonNestableDelayedTask(_, _)).Times(0);

  StartupTaskRunner runner(base::BindOnce(&Observer), proxy);

  StartupTask task1 =
      base::BindOnce(&StartupTaskRunnerTest::Task1, base::Unretained(this));
  runner.AddTask(std::move(task1));
  EXPECT_EQ(GetLastTask(), 0);
  StartupTask task2 =
      base::BindOnce(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner.AddTask(std::move(task2));

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner.RunAllTasksNow();

  // On an immediate StartupTaskRunner the tasks should now all have run.
  EXPECT_EQ(GetLastTask(), 2);

  EXPECT_EQ(task_count, 2);
  EXPECT_EQ(observer_calls, 1);
  EXPECT_EQ(observer_result, 0);

  // Running the tasks asynchronously shouldn't do anything
  // In particular Post... should not be called
  runner.StartRunningTasksAsync();

  // No more tasks should be run and the observer should not have been called
  // again
  EXPECT_EQ(task_count, 2);
  EXPECT_EQ(observer_calls, 1);
}

TEST_F(StartupTaskRunnerTest, NullObserver) {
  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _)).Times(0);
  EXPECT_CALL(mock_runner, PostNonNestableDelayedTask(_, _)).Times(0);

  StartupTaskRunner runner(base::OnceCallback<void(int)>(), proxy);

  StartupTask task1 =
      base::BindOnce(&StartupTaskRunnerTest::Task1, base::Unretained(this));
  runner.AddTask(std::move(task1));
  EXPECT_EQ(GetLastTask(), 0);
  StartupTask task2 =
      base::BindOnce(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner.AddTask(std::move(task2));

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner.RunAllTasksNow();

  // On an immediate StartupTaskRunner the tasks should now all have run.
  EXPECT_EQ(GetLastTask(), 2);
  EXPECT_EQ(task_count, 2);

  // Running the tasks asynchronously shouldn't do anything
  // In particular Post... should not be called
  runner.StartRunningTasksAsync();

  // No more tasks should have been run
  EXPECT_EQ(task_count, 2);

  EXPECT_EQ(observer_calls, 0);
}

TEST_F(StartupTaskRunnerTest, SynchronousExecutionFailedTask) {
  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _)).Times(0);
  EXPECT_CALL(mock_runner, PostNonNestableDelayedTask(_, _)).Times(0);

  StartupTaskRunner runner(base::BindOnce(&Observer), proxy);

  StartupTask task3 = base::BindOnce(&StartupTaskRunnerTest::FailingTask,
                                     base::Unretained(this));
  runner.AddTask(std::move(task3));
  EXPECT_EQ(GetLastTask(), 0);
  StartupTask task2 =
      base::BindOnce(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner.AddTask(std::move(task2));

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner.RunAllTasksNow();

  // Only the first task should have run, since it failed
  EXPECT_EQ(GetLastTask(), 3);
  EXPECT_EQ(task_count, 1);
  EXPECT_EQ(observer_calls, 1);
  EXPECT_EQ(observer_result, 1);

  // After a failed task all remaining tasks should be cancelled
  // In particular Post... should not be called by running asynchronously
  runner.StartRunningTasksAsync();

  // The observer should only be called the first time the queue completes and
  // no more tasks should have run
  EXPECT_EQ(observer_calls, 1);
  EXPECT_EQ(task_count, 1);
}

TEST_F(StartupTaskRunnerTest, AsynchronousExecution) {

  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _)).Times(0);
  EXPECT_CALL(mock_runner, PostNonNestableDelayedTask(_, base::Milliseconds(0)))
      .Times(testing::Between(2, 3))
      .WillRepeatedly(testing::Return(true));

  StartupTaskRunner runner(base::BindOnce(&Observer), proxy);

  StartupTask task1 =
      base::BindOnce(&StartupTaskRunnerTest::Task1, base::Unretained(this));
  runner.AddTask(std::move(task1));
  StartupTask task2 =
      base::BindOnce(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner.AddTask(std::move(task2));

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner.StartRunningTasksAsync();

  // No tasks should have run yet, since we the message loop hasn't run.
  EXPECT_EQ(GetLastTask(), 0);

  // Fake the actual message loop. Each time a task is run a new task should
  // be added to the queue, hence updating "task". The loop should actually run
  // at most 3 times (once for each task plus possibly once for the observer),
  // the "4" is a backstop.
  for (int i = 0; i < 4 && observer_calls == 0; i++) {
    proxy->TakeLastTaskClosure().Run();
    EXPECT_EQ(i + 1, GetLastTask());
  }
  EXPECT_EQ(task_count, 2);
  EXPECT_EQ(observer_calls, 1);
  EXPECT_EQ(observer_result, 0);

  // Check that running synchronously now doesn't do anything

  runner.RunAllTasksNow();
  EXPECT_EQ(task_count, 2);
  EXPECT_EQ(observer_calls, 1);
}

TEST_F(StartupTaskRunnerTest, AsynchronousExecutionFailedTask) {

  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _)).Times(0);
  EXPECT_CALL(mock_runner, PostNonNestableDelayedTask(_, base::Milliseconds(0)))
      .Times(testing::Between(1, 2))
      .WillRepeatedly(testing::Return(true));

  StartupTaskRunner runner(base::BindOnce(&Observer), proxy);

  StartupTask task3 = base::BindOnce(&StartupTaskRunnerTest::FailingTask,
                                     base::Unretained(this));
  runner.AddTask(std::move(task3));
  StartupTask task2 =
      base::BindOnce(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner.AddTask(std::move(task2));

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner.StartRunningTasksAsync();

  // No tasks should have run yet, since we the message loop hasn't run.
  EXPECT_EQ(GetLastTask(), 0);

  // Fake the actual message loop. Each time a task is run a new task should
  // be added to the queue, hence updating "task". The loop should actually run
  // at most twice (once for the failed task plus possibly once for the
  // observer), the "4" is a backstop.
  for (int i = 0; i < 4 && observer_calls == 0; i++)
    proxy->TakeLastTaskClosure().Run();
  EXPECT_EQ(GetLastTask(), 3);
  EXPECT_EQ(task_count, 1);

  EXPECT_EQ(observer_calls, 1);
  EXPECT_EQ(observer_result, 1);

  // Check that running synchronously now doesn't do anything
  runner.RunAllTasksNow();
  EXPECT_EQ(observer_calls, 1);
  EXPECT_EQ(task_count, 1);
}
}  // namespace
}  // namespace content
