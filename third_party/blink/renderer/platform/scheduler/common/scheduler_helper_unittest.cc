// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/common/lazy_now.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_observer.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;

namespace blink {
namespace scheduler {
namespace scheduler_helper_unittest {

namespace {
void AppendToVectorTestTask(Vector<String>* vector, String value) {
  vector->push_back(value);
}

void AppendToVectorReentrantTask(base::SingleThreadTaskRunner* task_runner,
                                 Vector<int>* vector,
                                 int* reentrant_count,
                                 int max_reentrant_count) {
  vector->push_back((*reentrant_count)++);
  if (*reentrant_count < max_reentrant_count) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(AppendToVectorReentrantTask,
                                         base::Unretained(task_runner), vector,
                                         reentrant_count, max_reentrant_count));
  }
}

}  // namespace

class SchedulerHelperTest : public testing::Test {
 public:
  SchedulerHelperTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {
    auto settings = base::sequence_manager::SequenceManager::Settings::Builder()
                        .SetPrioritySettings(CreatePrioritySettings())
                        .Build();
    sequence_manager_ = base::sequence_manager::SequenceManagerForTest::Create(
        nullptr, task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMockTickClock(), std::move(settings));
    scheduler_helper_ = std::make_unique<NonMainThreadSchedulerHelper>(
        sequence_manager_.get(), nullptr, TaskType::kInternalTest);
    scheduler_helper_->AttachToCurrentThread();
    default_task_runner_ = scheduler_helper_->DefaultTaskRunner();
  }

  SchedulerHelperTest(const SchedulerHelperTest&) = delete;
  SchedulerHelperTest& operator=(const SchedulerHelperTest&) = delete;
  ~SchedulerHelperTest() override = default;

  void TearDown() override {
    // Check that all tests stop posting tasks.
    task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());
  }

  template <typename E>
  static void CallForEachEnumValue(E first,
                                   E last,
                                   const char* (*function)(E)) {
    for (E val = first; val < last;
         val = static_cast<E>(static_cast<int>(val) + 1)) {
      (*function)(val);
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::sequence_manager::SequenceManagerForTest>
      sequence_manager_;
  std::unique_ptr<NonMainThreadSchedulerHelper> scheduler_helper_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
};

TEST_F(SchedulerHelperTest, TestPostDefaultTask) {
  Vector<String> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D1"));
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D2"));
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D3"));
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D4"));

  task_environment_.RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "D3", "D4"));
}

TEST_F(SchedulerHelperTest, TestRentrantTask) {
  int count = 0;
  Vector<int> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(AppendToVectorReentrantTask,
                                base::RetainedRef(default_task_runner_),
                                &run_order, &count, 5));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(SchedulerHelperTest, IsShutdown) {
  EXPECT_FALSE(scheduler_helper_->IsShutdown());

  scheduler_helper_->Shutdown();
  EXPECT_TRUE(scheduler_helper_->IsShutdown());
}

TEST_F(SchedulerHelperTest, GetNumberOfPendingTasks) {
  Vector<String> run_order;
  scheduler_helper_->DefaultTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D1"));
  scheduler_helper_->DefaultTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "D2"));
  scheduler_helper_->ControlNonMainThreadTaskQueue()
      ->GetTaskRunnerWithDefaultTaskType()
      ->PostTask(FROM_HERE,
                 base::BindOnce(&AppendToVectorTestTask, &run_order, "C1"));
  EXPECT_EQ(3U, sequence_manager_->PendingTasksCount());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0U, sequence_manager_->PendingTasksCount());
}

namespace {
class MockTaskObserver : public base::TaskObserver {
 public:
  MOCK_METHOD1(DidProcessTask, void(const base::PendingTask& task));
  MOCK_METHOD2(WillProcessTask,
               void(const base::PendingTask& task,
                    bool was_blocked_or_low_priority));
};

void NopTask() {}
}  // namespace

TEST_F(SchedulerHelperTest, ObserversNotifiedFor_DefaultTaskRunner) {
  MockTaskObserver observer;
  scheduler_helper_->AddTaskObserver(&observer);

  scheduler_helper_->DefaultTaskRunner()->PostTask(FROM_HERE,
                                                   base::BindOnce(&NopTask));

  EXPECT_CALL(observer, WillProcessTask(_, _)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(1);
  task_environment_.RunUntilIdle();
}

TEST_F(SchedulerHelperTest, ObserversNotNotifiedFor_ControlTaskQueue) {
  MockTaskObserver observer;
  scheduler_helper_->AddTaskObserver(&observer);

  scheduler_helper_->ControlNonMainThreadTaskQueue()
      ->GetTaskRunnerWithDefaultTaskType()
      ->PostTask(FROM_HERE, base::BindOnce(&NopTask));

  EXPECT_CALL(observer, WillProcessTask(_, _)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  task_environment_.RunUntilIdle();
}

}  // namespace scheduler_helper_unittest
}  // namespace scheduler
}  // namespace blink
