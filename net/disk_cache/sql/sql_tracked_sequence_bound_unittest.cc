// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_tracked_sequence_bound.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "net/disk_cache/sql/sql_async_task_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class DummyBackend {
 public:
  void VoidMethod() { did_run_void_method_ = true; }
  void VoidMethodWithArgs(int x) {
    did_run_void_method_with_args_ = true;
    arg_x_ = x;
  }
  int IntMethod() {
    did_run_int_method_ = true;
    return 42;
  }
  int IntMethodWithArgs(int x) {
    did_run_int_method_with_args_ = true;
    return x * 2;
  }

  static bool did_run_void_method_;
  static bool did_run_void_method_with_args_;
  static bool did_run_int_method_;
  static bool did_run_int_method_with_args_;
  static int arg_x_;
};

bool DummyBackend::did_run_void_method_ = false;
bool DummyBackend::did_run_void_method_with_args_ = false;
bool DummyBackend::did_run_int_method_ = false;
bool DummyBackend::did_run_int_method_with_args_ = false;
int DummyBackend::arg_x_ = 0;

class SqlTrackedSequenceBoundTest : public testing::Test {
 public:
  SqlTrackedSequenceBoundTest() = default;
  ~SqlTrackedSequenceBoundTest() override = default;

  void SetUp() override {
    DummyBackend::did_run_void_method_ = false;
    DummyBackend::did_run_void_method_with_args_ = false;
    DummyBackend::did_run_int_method_ = false;
    DummyBackend::did_run_int_method_with_args_ = false;
    DummyBackend::arg_x_ = 0;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  SqlAsyncTaskManager async_task_manager_;
};

TEST_F(SqlTrackedSequenceBoundTest, AsyncCall) {
  SqlTrackedSequenceBound<DummyBackend> tracked_sequence_bound(
      base::ThreadPool::CreateSequencedTaskRunner({}), async_task_manager_);

  tracked_sequence_bound.AsyncCall(&DummyBackend::VoidMethod);
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(DummyBackend::did_run_void_method_);
}

TEST_F(SqlTrackedSequenceBoundTest, AsyncCallWithArgs) {
  SqlTrackedSequenceBound<DummyBackend> tracked_sequence_bound(
      base::ThreadPool::CreateSequencedTaskRunner({}), async_task_manager_);

  tracked_sequence_bound.AsyncCall(&DummyBackend::VoidMethodWithArgs)
      .WithArgs(10);
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(DummyBackend::did_run_void_method_with_args_);
  EXPECT_EQ(10, DummyBackend::arg_x_);
}

TEST_F(SqlTrackedSequenceBoundTest, AsyncCallThen) {
  SqlTrackedSequenceBound<DummyBackend> tracked_sequence_bound(
      base::ThreadPool::CreateSequencedTaskRunner({}), async_task_manager_);

  bool then_called = false;
  tracked_sequence_bound.AsyncCall(&DummyBackend::VoidMethod)
      .Then(base::BindLambdaForTesting([&]() { then_called = true; }));
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(DummyBackend::did_run_void_method_);
  EXPECT_TRUE(then_called);
}

TEST_F(SqlTrackedSequenceBoundTest, AsyncCallWithArgsThen) {
  SqlTrackedSequenceBound<DummyBackend> tracked_sequence_bound(
      base::ThreadPool::CreateSequencedTaskRunner({}), async_task_manager_);

  bool then_called = false;
  tracked_sequence_bound.AsyncCall(&DummyBackend::VoidMethodWithArgs)
      .WithArgs(20)
      .Then(base::BindLambdaForTesting([&]() { then_called = true; }));
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(DummyBackend::did_run_void_method_with_args_);
  EXPECT_EQ(20, DummyBackend::arg_x_);
  EXPECT_TRUE(then_called);
}

TEST_F(SqlTrackedSequenceBoundTest, IntAsyncCall) {
  SqlTrackedSequenceBound<DummyBackend> tracked_sequence_bound(
      base::ThreadPool::CreateSequencedTaskRunner({}), async_task_manager_);

  EXPECT_CHECK_DEATH_WITH(
      tracked_sequence_bound.AsyncCall(&DummyBackend::IntMethod),
      "Then\\(\\) not invoked for a method that returns a non-void type");
}

TEST_F(SqlTrackedSequenceBoundTest, IntAsyncCallThen) {
  SqlTrackedSequenceBound<DummyBackend> tracked_sequence_bound(
      base::ThreadPool::CreateSequencedTaskRunner({}), async_task_manager_);

  int then_result = 0;
  tracked_sequence_bound.AsyncCall(&DummyBackend::IntMethod)
      .Then(base::BindLambdaForTesting(
          [&](int result) { then_result = result; }));
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(DummyBackend::did_run_int_method_);
  EXPECT_EQ(42, then_result);
}

TEST_F(SqlTrackedSequenceBoundTest, IntAsyncCallWithArgsThen) {
  SqlTrackedSequenceBound<DummyBackend> tracked_sequence_bound(
      base::ThreadPool::CreateSequencedTaskRunner({}), async_task_manager_);

  int then_result = 0;
  tracked_sequence_bound.AsyncCall(&DummyBackend::IntMethodWithArgs)
      .WithArgs(5)
      .Then(base::BindLambdaForTesting(
          [&](int result) { then_result = result; }));
  async_task_manager_.RunUntilAllTasksCompleteForTest();
  EXPECT_TRUE(DummyBackend::did_run_int_method_with_args_);
  EXPECT_EQ(10, then_result);
}

TEST_F(SqlTrackedSequenceBoundTest, MissingWithArgs) {
  SqlTrackedSequenceBound<DummyBackend> tracked_sequence_bound(
      base::ThreadPool::CreateSequencedTaskRunner({}), async_task_manager_);

  EXPECT_CHECK_DEATH_WITH(
      tracked_sequence_bound.AsyncCall(&DummyBackend::VoidMethodWithArgs),
      "Wrong number of arguments provided to WithArgs");
}

}  // namespace disk_cache
