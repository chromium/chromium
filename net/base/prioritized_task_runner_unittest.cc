// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/prioritized_task_runner.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class PrioritizedTaskRunnerTest : public testing::Test {
 public:
  PrioritizedTaskRunnerTest() {}

  void PushName(const std::string& task_name) {
    base::AutoLock auto_lock(callback_names_lock_);
    callback_names_.push_back(task_name);
  }

  std::string PushNameWithResult(const std::string& task_name) {
    PushName(task_name);
    std::string reply_name = task_name;
    base::ReplaceSubstringsAfterOffset(&reply_name, 0, "Task", "Reply");
    return reply_name;
  }

  std::vector<std::string> TaskOrder() {
    std::vector<std::string> out;
    for (const std::string& name : callback_names_) {
      if (base::StartsWith(name, "Task", base::CompareCase::SENSITIVE))
        out.push_back(name);
    }
    return out;
  }

  std::vector<std::string> ReplyOrder() {
    std::vector<std::string> out;
    for (const std::string& name : callback_names_) {
      if (base::StartsWith(name, "Reply", base::CompareCase::SENSITIVE))
        out.push_back(name);
    }
    return out;
  }

  // Adds a task to the task runner and waits for it to execute.
  void ProcessTaskRunner(base::TaskRunner* task_runner) {
    // Use a waitable event instead of a run loop as we need to be careful not
    // to run any tasks on this task runner while waiting.
    base::WaitableEvent waitable_event;

    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(
                              [](base::WaitableEvent* waitable_event) {
                                waitable_event->Signal();
                              },
                              &waitable_event));

    base::ScopedAllowBaseSyncPrimitivesForTesting sync;
    waitable_event.Wait();
  }

  // Adds a task to the |task_runner|, forcing it to wait for a conditional.
  // Call ReleaseTaskRunner to continue.
  void BlockTaskRunner(base::TaskRunner* task_runner) {
    waitable_event_.Reset();

    auto wait_function = [](base::WaitableEvent* waitable_event) {
      base::ScopedAllowBaseSyncPrimitivesForTesting sync;
      waitable_event->Wait();
    };
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(wait_function, &waitable_event_));
  }

  // Signals the task runner's conditional so that it can continue after calling
  // BlockTaskRunner.
  void ReleaseTaskRunner() { waitable_event_.Signal(); }

 protected:
  base::test::TaskEnvironment task_environment_;

  std::vector<std::string> callback_names_;
  base::Lock callback_names_lock_;
  base::WaitableEvent waitable_event_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrioritizedTaskRunnerTest);
};

TEST_F(PrioritizedTaskRunnerTest, PostTaskAndReplyThreadCheck) {
  auto task_runner =
      base::CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  auto prioritized_task_runner =
      base::MakeRefCounted<PrioritizedTaskRunner>(task_runner);

  base::RunLoop run_loop;

  auto thread_check = [](scoped_refptr<base::TaskRunner> expected_task_runner,
                         base::OnceClosure callback) {
    EXPECT_TRUE(expected_task_runner->RunsTasksInCurrentSequence());
    std::move(callback).Run();
  };

  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(thread_check, task_runner, base::DoNothing::Once()),
      base::BindOnce(thread_check, task_environment_.GetMainThreadTaskRunner(),
                     run_loop.QuitClosure()),
      0);

  run_loop.Run();
}

TEST_F(PrioritizedTaskRunnerTest, PostTaskAndReplyRunsBothTasks) {
  auto task_runner =
      base::CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  auto prioritized_task_runner =
      base::MakeRefCounted<PrioritizedTaskRunner>(task_runner);

  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Task"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Reply"),
      0);

  // Run the TaskRunner and both the Task and Reply should run.
  task_environment_.RunUntilIdle();
  EXPECT_EQ((std::vector<std::string>{"Task", "Reply"}), callback_names_);
}

TEST_F(PrioritizedTaskRunnerTest, PostTaskAndReplyTestPriority) {
  auto task_runner =
      base::CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  auto prioritized_task_runner =
      base::MakeRefCounted<PrioritizedTaskRunner>(task_runner);

  BlockTaskRunner(task_runner.get());
  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Task5"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Reply5"),
      5);

  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Task0"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Reply0"),
      0);

  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Task7"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Reply7"),
      7);
  ReleaseTaskRunner();

  // Run the TaskRunner and all of the tasks and replies should have run, in
  // priority order.
  task_environment_.RunUntilIdle();
  EXPECT_EQ((std::vector<std::string>{"Task0", "Task5", "Task7"}), TaskOrder());
  EXPECT_EQ((std::vector<std::string>{"Reply0", "Reply5", "Reply7"}),
            ReplyOrder());
}

// Ensure that replies are run in priority order.
TEST_F(PrioritizedTaskRunnerTest, PostTaskAndReplyTestReplyPriority) {
  auto task_runner =
      base::CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  auto prioritized_task_runner =
      base::MakeRefCounted<PrioritizedTaskRunner>(task_runner);

  // Add a couple of tasks to run right away, but don't run their replies yet.
  BlockTaskRunner(task_runner.get());
  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Task2"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Reply2"),
      2);

  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Task1"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Reply1"),
      1);
  ReleaseTaskRunner();

  // Run the current tasks (but not their replies).
  ProcessTaskRunner(task_runner.get());

  // Now post task 0 (highest priority) and run it. None of the replies have
  // been processed yet, so its reply should skip to the head of the queue.
  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Task0"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "Reply0"),
      0);
  ProcessTaskRunner(task_runner.get());

  // Run the replies.
  task_environment_.RunUntilIdle();

  EXPECT_EQ((std::vector<std::string>{"Task1", "Task2", "Task0"}), TaskOrder());
  EXPECT_EQ((std::vector<std::string>{"Reply0", "Reply1", "Reply2"}),
            ReplyOrder());
}

TEST_F(PrioritizedTaskRunnerTest, PriorityOverflow) {
  auto task_runner =
      base::CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  auto prioritized_task_runner =
      base::MakeRefCounted<PrioritizedTaskRunner>(task_runner);

  const uint32_t kMaxPriority = std::numeric_limits<uint32_t>::max();

  BlockTaskRunner(task_runner.get());
  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "TaskMinus1"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "ReplyMinus1"),
      kMaxPriority - 1);

  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "TaskMax"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "ReplyMax"),
      kMaxPriority);

  prioritized_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "TaskMaxPlus1"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this), "ReplyMaxPlus1"),
      kMaxPriority + 1);
  ReleaseTaskRunner();

  // Run the TaskRunner and all of the tasks and replies should have run, in
  // priority order.
  task_environment_.RunUntilIdle();
  EXPECT_EQ((std::vector<std::string>{"TaskMaxPlus1", "TaskMinus1", "TaskMax"}),
            TaskOrder());
  EXPECT_EQ(
      (std::vector<std::string>{"ReplyMaxPlus1", "ReplyMinus1", "ReplyMax"}),
      ReplyOrder());
}

TEST_F(PrioritizedTaskRunnerTest, PostTaskAndReplyWithResultRunsBothTasks) {
  auto task_runner =
      base::CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  auto prioritized_task_runner =
      base::MakeRefCounted<PrioritizedTaskRunner>(task_runner);

  prioritized_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushNameWithResult,
                     base::Unretained(this), "Task"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this)),
      0);

  // Run the TaskRunner and both the Task and Reply should run.
  task_environment_.RunUntilIdle();
  EXPECT_EQ((std::vector<std::string>{"Task", "Reply"}), callback_names_);
}

TEST_F(PrioritizedTaskRunnerTest, PostTaskAndReplyWithResultTestPriority) {
  auto task_runner =
      base::CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  auto prioritized_task_runner =
      base::MakeRefCounted<PrioritizedTaskRunner>(task_runner);

  BlockTaskRunner(task_runner.get());
  prioritized_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushNameWithResult,
                     base::Unretained(this), "Task0"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this)),
      0);

  prioritized_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushNameWithResult,
                     base::Unretained(this), "Task7"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this)),
      7);

  prioritized_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PrioritizedTaskRunnerTest::PushNameWithResult,
                     base::Unretained(this), "Task3"),
      base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                     base::Unretained(this)),
      3);
  ReleaseTaskRunner();

  // Run the TaskRunner and both the Task and Reply should run.
  task_environment_.RunUntilIdle();
  EXPECT_EQ((std::vector<std::string>{"Task0", "Task3", "Task7"}), TaskOrder());
  EXPECT_EQ((std::vector<std::string>{"Reply0", "Reply3", "Reply7"}),
            ReplyOrder());
}

TEST_F(PrioritizedTaskRunnerTest, OrderSamePriorityByPostOrder) {
  auto task_runner =
      base::CreateSequencedTaskRunner(base::TaskTraits(base::ThreadPool()));
  auto prioritized_task_runner =
      base::MakeRefCounted<PrioritizedTaskRunner>(task_runner);

  std::vector<int> expected;

  // Create 1000 tasks with random priorities between 1 and 3. Those that have
  // the same priorities should run in posting order.
  BlockTaskRunner(task_runner.get());
  for (int i = 0; i < 1000; i++) {
    int priority = base::RandInt(0, 2);
    int id = (priority * 1000) + i;

    expected.push_back(id);
    prioritized_task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PrioritizedTaskRunnerTest::PushName,
                       base::Unretained(this), base::NumberToString(id)),
        base::BindOnce(base::DoNothing::Once()), priority);
  }
  ReleaseTaskRunner();

  // This is the order the tasks should run on the queue.
  std::sort(expected.begin(), expected.end());

  task_environment_.RunUntilIdle();

  // This is the order that the tasks ran on the queue.
  std::vector<int> results;
  for (const std::string& result : callback_names_) {
    int result_id;
    EXPECT_TRUE(base::StringToInt(result, &result_id));
    results.push_back(result_id);
  }

  EXPECT_EQ(expected, results);
}

}  // namespace
}  // namespace net
