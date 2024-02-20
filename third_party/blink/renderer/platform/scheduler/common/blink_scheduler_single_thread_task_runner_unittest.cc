// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/blink_scheduler_single_thread_task_runner.h"

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"

namespace blink::scheduler {

using base::sequence_manager::QueueName;
using base::sequence_manager::TaskQueue;
using base::test::TaskEnvironment;

namespace {

class TestObject {
 public:
  explicit TestObject(int* counter) : counter_(counter) {}

  ~TestObject() { ++(*counter_); }

 private:
  raw_ptr<int> counter_;
};

}  // namespace

class BlinkSchedulerSingleThreadTaskRunnerTest : public testing::Test {
 public:
  BlinkSchedulerSingleThreadTaskRunnerTest()
      : task_environment_(TaskEnvironment::TimeSource::MOCK_TIME,
                          TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {
    sequence_manager_ = base::sequence_manager::SequenceManagerForTest::Create(
        nullptr, task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMockTickClock());
    backup_task_queue_ =
        sequence_manager_->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));
    backup_task_runner_ = backup_task_queue_->CreateTaskRunner(
        static_cast<int>(TaskType::kInternalTest));
    test_task_queue_ =
        sequence_manager_->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));
    test_task_runner_ = test_task_queue_->CreateTaskRunner(
        static_cast<int>(TaskType::kInternalTest));
  }

  BlinkSchedulerSingleThreadTaskRunnerTest(
      const BlinkSchedulerSingleThreadTaskRunnerTest&) = delete;
  BlinkSchedulerSingleThreadTaskRunnerTest& operator=(
      const BlinkSchedulerSingleThreadTaskRunnerTest&) = delete;
  ~BlinkSchedulerSingleThreadTaskRunnerTest() override = default;

  void TearDown() override {
    ShutDownTestTaskQueue();
    ShutDownBackupTaskQueue();
    sequence_manager_.reset();
  }

 protected:
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTestTaskRunner() {
    return test_task_runner_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& GetBackupTaskRunner() {
    return backup_task_runner_;
  }

  void ShutDownTestTaskQueue() {
    if (!test_task_queue_) {
      return;
    }
    test_task_queue_.reset();
  }

  void ShutDownBackupTaskQueue() {
    if (!backup_task_queue_) {
      return;
    }
    backup_task_queue_.reset();
  }

  base::test::TaskEnvironment task_environment_;

 private:
  std::unique_ptr<base::sequence_manager::SequenceManagerForTest>
      sequence_manager_;

  base::sequence_manager::TaskQueue::Handle backup_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> backup_task_runner_;

  base::sequence_manager::TaskQueue::Handle test_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> test_task_runner_;
};

TEST_F(BlinkSchedulerSingleThreadTaskRunnerTest, TargetTaskRunnerOnly) {
  scoped_refptr<BlinkSchedulerSingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<BlinkSchedulerSingleThreadTaskRunner>(
          GetTestTaskRunner(), nullptr);
  int counter = 0;
  std::unique_ptr<TestObject> test_object =
      std::make_unique<TestObject>(&counter);

  bool result = task_runner->DeleteSoon(FROM_HERE, std::move(test_object));
  EXPECT_TRUE(result);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);
}

TEST_F(BlinkSchedulerSingleThreadTaskRunnerTest,
       TargetTaskRunnerOnlyShutDownAfterPosting) {
  scoped_refptr<BlinkSchedulerSingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<BlinkSchedulerSingleThreadTaskRunner>(
          GetTestTaskRunner(), nullptr);
  int counter = 0;
  std::unique_ptr<TestObject> test_object =
      std::make_unique<TestObject>(&counter);

  bool result = task_runner->DeleteSoon(FROM_HERE, std::move(test_object));
  EXPECT_TRUE(result);
  ShutDownTestTaskQueue();
  EXPECT_EQ(1, counter);
}

TEST_F(BlinkSchedulerSingleThreadTaskRunnerTest, BackupTaskRunner) {
  scoped_refptr<BlinkSchedulerSingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<BlinkSchedulerSingleThreadTaskRunner>(
          GetTestTaskRunner(), GetBackupTaskRunner());
  int counter = 0;
  std::unique_ptr<TestObject> test_object =
      std::make_unique<TestObject>(&counter);

  ShutDownTestTaskQueue();

  bool result = task_runner->DeleteSoon(FROM_HERE, std::move(test_object));
  EXPECT_TRUE(result);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);
}

TEST_F(BlinkSchedulerSingleThreadTaskRunnerTest,
       BackupTaskRunnerShutDownAfterPosting) {
  scoped_refptr<BlinkSchedulerSingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<BlinkSchedulerSingleThreadTaskRunner>(
          GetTestTaskRunner(), GetBackupTaskRunner());
  int counter = 0;
  std::unique_ptr<TestObject> test_object =
      std::make_unique<TestObject>(&counter);

  ShutDownTestTaskQueue();

  bool result = task_runner->DeleteSoon(FROM_HERE, std::move(test_object));
  EXPECT_TRUE(result);
  ShutDownBackupTaskQueue();
  EXPECT_EQ(1, counter);
}

TEST_F(BlinkSchedulerSingleThreadTaskRunnerTest,
       SynchronousDeleteAfterShutdownOnSameThread) {
  scoped_refptr<BlinkSchedulerSingleThreadTaskRunner> task_runner =
      base::MakeRefCounted<BlinkSchedulerSingleThreadTaskRunner>(
          GetTestTaskRunner(), GetBackupTaskRunner());
  ShutDownTestTaskQueue();
  ShutDownBackupTaskQueue();

  int counter = 0;
  std::unique_ptr<TestObject> test_object =
      std::make_unique<TestObject>(&counter);
  bool result = task_runner->DeleteSoon(FROM_HERE, std::move(test_object));
  EXPECT_TRUE(result);
  EXPECT_EQ(1, counter);
}

TEST_F(BlinkSchedulerSingleThreadTaskRunnerTest,
       PostingToShutDownThreadLeaksObject) {
  std::unique_ptr<NonMainThread> thread =
      NonMainThread::CreateThread(ThreadCreationParams(ThreadType::kTestThread)
                                      .SetThreadNameForTest("TestThread"));
  scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner =
      thread->GetTaskRunner();
  thread.reset();

  int counter = 0;
  std::unique_ptr<TestObject> test_object =
      std::make_unique<TestObject>(&counter);
  TestObject* unowned_test_object = test_object.get();
  bool result =
      thread_task_runner->DeleteSoon(FROM_HERE, std::move(test_object));
  // This should always return true.
  EXPECT_TRUE(result);
  EXPECT_EQ(0, counter);
  // Delete this manually since it leaked.
  delete (unowned_test_object);
}

}  // namespace blink::scheduler
