// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/bind_post_task.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

void Increment(int* value) {
  ++(*value);
}

void Add(int* sum, int a, int b) {
  *sum = a + b;
}

void SetIntFromUniquePtr(int* variable, std::unique_ptr<int> value) {
  *variable = *value;
}

int Multiply(int value) {
  return value * 5;
}

// Helper class from base/task/bind_post_task_unittest.cc, adapted for WTF.
class WTFSequenceRestrictionChecker {
 public:
  WTFSequenceRestrictionChecker(bool* set_on_destroy,
                                base::OnceClosure quit_closure)
      : set_on_destroy_(set_on_destroy),
        quit_closure_(std::move(quit_closure)) {}

  ~WTFSequenceRestrictionChecker() {
    EXPECT_TRUE(checker_.CalledOnValidSequence());
    *set_on_destroy_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void Run() { EXPECT_TRUE(checker_.CalledOnValidSequence()); }

 private:
  base::SequenceCheckerImpl checker_;
  bool* set_on_destroy_;
  base::OnceClosure quit_closure_;
};

}  // namespace

class BindPostTaskTest : public testing::Test {
 public:
  ~BindPostTaskTest() override {
    // Ensures boundg tasks destruct cleanly before teardown.
    CycleTasks();
  }

 protected:
  void CycleTasks() {
    task_runner_->PostTask(FROM_HERE, task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      task_environment_.GetMainThreadTaskRunner();
};

TEST_F(BindPostTaskTest, OnceFunctionNoArgs) {
  int count = 0;

  auto bound_task = BindPostTask(
      task_runner_,
      CrossThreadBindOnce(&Increment, CrossThreadUnretained(&count)));
  std::move(bound_task).Run();

  EXPECT_EQ(0, count);
  CycleTasks();
  EXPECT_EQ(1, count);
}

TEST_F(BindPostTaskTest, OnceFunctionWithArgs) {
  int sum = 0;

  auto bound_task = BindPostTask(
      task_runner_,
      CrossThreadBindOnce(&Add, CrossThreadUnretained(&sum), 5, 10));
  std::move(bound_task).Run();

  EXPECT_EQ(0, sum);
  CycleTasks();
  EXPECT_EQ(15, sum);
}

TEST_F(BindPostTaskTest, OnceWithBoundMoveOnlyArg) {
  int val = 0;

  auto inner_cb =
      CrossThreadBindOnce(&SetIntFromUniquePtr, CrossThreadUnretained(&val),
                          std::make_unique<int>(10));
  auto post_cb = BindPostTask(task_runner_, std::move(inner_cb));
  std::move(post_cb).Run();

  EXPECT_EQ(val, 0);
  CycleTasks();
  EXPECT_EQ(val, 10);
}

TEST_F(BindPostTaskTest, OnceWithUnboundMoveOnlyArg) {
  int val = 0;

  auto inner_cb =
      CrossThreadBindOnce(&SetIntFromUniquePtr, CrossThreadUnretained(&val));
  auto post_cb = BindPostTask(task_runner_, std::move(inner_cb));
  std::move(post_cb).Run(std::make_unique<int>(10));

  EXPECT_EQ(val, 0);
  CycleTasks();
  EXPECT_EQ(val, 10);
}

TEST_F(BindPostTaskTest, OnceWithIgnoreResult) {
  auto inner_cb = CrossThreadBindOnce(base::IgnoreResult(&Multiply));
  auto post_cb = BindPostTask(task_runner_, std::move(inner_cb));

  std::move(post_cb).Run(1);
  CycleTasks();
}

TEST_F(BindPostTaskTest, OnceRunDestroyedOnBound) {
  bool destroyed_on_main = false;
  auto checker = std::make_unique<WTFSequenceRestrictionChecker>(
      &destroyed_on_main, task_environment_.QuitClosure());

  auto cb_owning_checker = CrossThreadBindOnce(
      &WTFSequenceRestrictionChecker::Run, std::move(checker));
  auto post_cb = BindPostTask(task_runner_, std::move(cb_owning_checker));

  base::Thread other_thread("other_thread_once_run");
  ASSERT_TRUE(other_thread.Start());
  other_thread.task_runner()->PostTask(
      FROM_HERE, ConvertToBaseOnceCallback(std::move(post_cb)));
  other_thread.Stop();  // Flushes tasks and waits for completion.

  EXPECT_FALSE(destroyed_on_main);
  task_environment_.RunUntilQuit();
  EXPECT_TRUE(destroyed_on_main);
}

TEST_F(BindPostTaskTest, OnceNotRunDestroyedOnBound) {
  bool destroyed_on_main = false;
  auto checker = std::make_unique<WTFSequenceRestrictionChecker>(
      &destroyed_on_main, task_environment_.QuitClosure());

  auto cb_owning_checker = CrossThreadBindOnce(
      &WTFSequenceRestrictionChecker::Run, std::move(checker));
  auto post_cb = BindPostTask(task_runner_, std::move(cb_owning_checker));

  base::Thread other_thread("other_thread_once_not_run");
  ASSERT_TRUE(other_thread.Start());
  // Destroy post_cb on another thread. This should post back to main thread
  // for destruction of cb_owning_checker.
  other_thread.task_runner()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(
                     ConvertToBaseOnceCallback(std::move(post_cb))));
  other_thread.Stop();

  EXPECT_FALSE(destroyed_on_main);
  task_environment_.RunUntilQuit();
  EXPECT_TRUE(destroyed_on_main);
}

TEST_F(BindPostTaskTest, RepeatingFunctionNoArgs) {
  int count = 0;

  auto bound_task = BindPostTask(
      task_runner_,
      CrossThreadBindRepeating(&Increment, CrossThreadUnretained(&count)));

  bound_task.Run();
  EXPECT_EQ(0, count);
  CycleTasks();
  EXPECT_EQ(1, count);

  bound_task.Run();
  EXPECT_EQ(1, count);
  CycleTasks();
  EXPECT_EQ(2, count);
}

TEST_F(BindPostTaskTest, RepeatingFunctionWithArgs) {
  int sum = 0;

  auto bound_task = BindPostTask(
      task_runner_,
      CrossThreadBindRepeating(&Add, CrossThreadUnretained(&sum), 3, 4));

  bound_task.Run();
  EXPECT_EQ(0, sum);
  CycleTasks();
  EXPECT_EQ(7, sum);

  // Re-running a repeating callback will re-evaluate arguments if not owned.
  // In this case, the values are hardcoded, so the result is the same.
  sum = 0;
  bound_task.Run();
  EXPECT_EQ(0, sum);
  CycleTasks();
  EXPECT_EQ(7, sum);
}

TEST_F(BindPostTaskTest, RepeatingWithIgnoreResult) {
  auto inner_cb = CrossThreadBindRepeating(base::IgnoreResult(&Multiply));
  auto post_cb = BindPostTask(task_runner_, std::move(inner_cb));

  post_cb.Run(1);
  CycleTasks();

  post_cb.Run(2);
  CycleTasks();
}

TEST_F(BindPostTaskTest, RepeatingRunDestroyedOnBound) {
  bool destroyed_on_main = false;
  auto checker_ptr = std::make_unique<WTFSequenceRestrictionChecker>(
      &destroyed_on_main, task_environment_.QuitClosure());
  auto cb_owning_checker = CrossThreadBindRepeating(
      &WTFSequenceRestrictionChecker::Run, std::move(checker_ptr));
  auto post_cb = BindPostTask(task_runner_, std::move(cb_owning_checker));

  base::Thread other_thread("other_thread_repeating_run");
  ASSERT_TRUE(other_thread.Start());
  other_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(base::BindLambdaForTesting([&]() {
        post_cb.Run();
        post_cb.Run();
        post_cb.Reset();
      })));
  other_thread.Stop();

  EXPECT_FALSE(destroyed_on_main);
  task_environment_.RunUntilQuit();
  EXPECT_TRUE(destroyed_on_main);
}

TEST_F(BindPostTaskTest, RepeatingNotRunDestroyedOnBound) {
  bool destroyed_on_main = false;
  auto checker_ptr = std::make_unique<WTFSequenceRestrictionChecker>(
      &destroyed_on_main, task_environment_.QuitClosure());
  auto cb_owning_checker = CrossThreadBindRepeating(
      &WTFSequenceRestrictionChecker::Run, std::move(checker_ptr));
  auto post_cb = BindPostTask(task_runner_, std::move(cb_owning_checker));

  base::Thread other_thread("other_thread_repeating_run");
  ASSERT_TRUE(other_thread.Start());
  // Destroy post_cb on another thread. This should post back to main thread
  // for destruction of cb_owning_checker.
  other_thread.task_runner()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(
                     ConvertToBaseRepeatingCallback(std::move(post_cb))));
  other_thread.Stop();  // Flushes tasks and waits for completion.

  EXPECT_FALSE(destroyed_on_main);
  task_environment_.RunUntilQuit();
  EXPECT_TRUE(destroyed_on_main);
}

}  // namespace blink
