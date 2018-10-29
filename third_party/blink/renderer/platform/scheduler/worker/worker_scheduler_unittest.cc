// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"

#include <memory>
#include "base/macros.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using testing::ElementsAreArray;
using testing::ElementsAre;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace worker_scheduler_unittest {

void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void RunChainedTask(scoped_refptr<base::sequence_manager::TaskQueue> task_queue,
                    int count,
                    base::TimeDelta duration,
                    scoped_refptr<base::TestMockTimeTaskRunner> environment,
                    std::vector<base::TimeTicks>* tasks) {
  tasks->push_back(environment->GetMockTickClock()->NowTicks());

  environment->AdvanceMockTickClock(duration);

  if (count == 1)
    return;

  // Add a delay of 50ms to ensure that wake-up based throttling does not affect
  // us.
  task_queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RunChainedTask, task_queue, count - 1, duration,
                     environment, base::Unretained(tasks)),
      base::TimeDelta::FromMilliseconds(50));
}

class WorkerThreadSchedulerForTest : public WorkerThreadScheduler {
 public:
  WorkerThreadSchedulerForTest(
      WebThreadType thread_type,
      std::unique_ptr<base::sequence_manager::SequenceManager> manager,
      WorkerSchedulerProxy* proxy)
      : WorkerThreadScheduler(thread_type, std::move(manager), proxy) {}

  const std::unordered_set<WorkerScheduler*>& worker_schedulers() {
    return GetWorkerSchedulersForTesting();
  }

  using WorkerThreadScheduler::CreateTaskQueueThrottler;
  using WorkerThreadScheduler::SetCPUTimeBudgetPoolForTesting;
};

class WorkerSchedulerForTest : public WorkerScheduler {
 public:
  explicit WorkerSchedulerForTest(
      WorkerThreadSchedulerForTest* thread_scheduler)
      : WorkerScheduler(thread_scheduler, nullptr) {}

  using WorkerScheduler::UnthrottleableTaskQueue;
  using WorkerScheduler::ThrottleableTaskQueue;
};

class WorkerSchedulerTest : public testing::Test {
 public:
  WorkerSchedulerTest()
      : mock_task_runner_(new base::TestMockTimeTaskRunner()),
        scheduler_(new WorkerThreadSchedulerForTest(
            WebThreadType::kTestThread,
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr,
                mock_task_runner_,
                mock_task_runner_->GetMockTickClock()),
            nullptr /* proxy */)) {
    mock_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMicroseconds(5000));
  }

  ~WorkerSchedulerTest() override = default;

  void SetUp() override {
    scheduler_->Init();
    worker_scheduler_ =
        std::make_unique<WorkerSchedulerForTest>(scheduler_.get());
  }

  void TearDown() override {
    if (worker_scheduler_) {
      worker_scheduler_->Dispose();
      worker_scheduler_.reset();
    }
  }

  const base::TickClock* GetClock() {
    return mock_task_runner_->GetMockTickClock();
  }

  void RunUntilIdle() { mock_task_runner_->FastForwardUntilNoTasksRemain(); }

  // Helper for posting a task.
  void PostTestTask(std::vector<std::string>* run_order,
                    const std::string& task_descriptor) {
    worker_scheduler_->GetTaskRunner(TaskType::kInternalTest)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&AppendToVectorTestTask,
                             WTF::Unretained(run_order), task_descriptor));
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;

  std::unique_ptr<WorkerThreadSchedulerForTest> scheduler_;
  std::unique_ptr<WorkerSchedulerForTest> worker_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(WorkerSchedulerTest);
};

TEST_F(WorkerSchedulerTest, TestPostTasks) {
  std::vector<std::string> run_order;
  PostTestTask(&run_order, "T1");
  PostTestTask(&run_order, "T2");
  RunUntilIdle();
  PostTestTask(&run_order, "T3");
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2", "T3"));

  // Tasks should not run after the scheduler is disposed of.
  worker_scheduler_->Dispose();
  run_order.clear();
  PostTestTask(&run_order, "T4");
  PostTestTask(&run_order, "T5");
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  worker_scheduler_.reset();
}

TEST_F(WorkerSchedulerTest, RegisterWorkerSchedulers) {
  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::ElementsAre(worker_scheduler_.get()));

  std::unique_ptr<WorkerSchedulerForTest> worker_scheduler2 =
      std::make_unique<WorkerSchedulerForTest>(scheduler_.get());

  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::UnorderedElementsAre(worker_scheduler_.get(),
                                            worker_scheduler2.get()));

  worker_scheduler_->Dispose();
  worker_scheduler_.reset();

  EXPECT_THAT(scheduler_->worker_schedulers(),
              testing::ElementsAre(worker_scheduler2.get()));

  worker_scheduler2->Dispose();

  EXPECT_THAT(scheduler_->worker_schedulers(), testing::ElementsAre());
}

TEST_F(WorkerSchedulerTest, ThrottleWorkerScheduler) {
  scheduler_->CreateTaskQueueThrottler();

  EXPECT_FALSE(scheduler_->task_queue_throttler()->IsThrottled(
      worker_scheduler_->ThrottleableTaskQueue().get()));

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);
  EXPECT_TRUE(scheduler_->task_queue_throttler()->IsThrottled(
      worker_scheduler_->ThrottleableTaskQueue().get()));

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);
  EXPECT_TRUE(scheduler_->task_queue_throttler()->IsThrottled(
      worker_scheduler_->ThrottleableTaskQueue().get()));

  // Ensure that two calls with kThrottled do not mess with throttling
  // refcount.
  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kNotThrottled);
  EXPECT_FALSE(scheduler_->task_queue_throttler()->IsThrottled(
      worker_scheduler_->ThrottleableTaskQueue().get()));
}

TEST_F(WorkerSchedulerTest, ThrottleWorkerScheduler_CreateThrottled) {
  scheduler_->CreateTaskQueueThrottler();

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);

  std::unique_ptr<WorkerSchedulerForTest> worker_scheduler2 =
      std::make_unique<WorkerSchedulerForTest>(scheduler_.get());

  // Ensure that newly created scheduler is throttled.
  EXPECT_TRUE(scheduler_->task_queue_throttler()->IsThrottled(
      worker_scheduler2->ThrottleableTaskQueue().get()));

  worker_scheduler2->Dispose();
}

TEST_F(WorkerSchedulerTest, ThrottleWorkerScheduler_RunThrottledTasks) {
  scheduler_->CreateTaskQueueThrottler();
  scheduler_->SetCPUTimeBudgetPoolForTesting(nullptr);

  // Create a new |worker_scheduler| to ensure that it's properly initialised.
  worker_scheduler_->Dispose();
  worker_scheduler_ =
      std::make_unique<WorkerSchedulerForTest>(scheduler_.get());

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);

  std::vector<base::TimeTicks> tasks;

  worker_scheduler_->ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RunChainedTask,
                                worker_scheduler_->ThrottleableTaskQueue(), 5,
                                base::TimeDelta(), mock_task_runner_,
                                base::Unretained(&tasks)));

  RunUntilIdle();

  EXPECT_THAT(tasks,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(2),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(4),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(5)));
}

TEST_F(WorkerSchedulerTest,
       ThrottleWorkerScheduler_RunThrottledTasks_CPUBudget) {
  scheduler_->CreateTaskQueueThrottler();

  scheduler_->cpu_time_budget_pool()->SetTimeBudgetRecoveryRate(
      GetClock()->NowTicks(), 0.01);

  // Create a new |worker_scheduler| to ensure that it's properly initialised.
  worker_scheduler_->Dispose();
  worker_scheduler_ =
      std::make_unique<WorkerSchedulerForTest>(scheduler_.get());

  scheduler_->OnLifecycleStateChanged(SchedulingLifecycleState::kThrottled);

  std::vector<base::TimeTicks> tasks;

  worker_scheduler_->ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RunChainedTask,
                                worker_scheduler_->ThrottleableTaskQueue(), 5,
                                base::TimeDelta::FromMilliseconds(100),
                                mock_task_runner_, base::Unretained(&tasks)));

  RunUntilIdle();

  EXPECT_THAT(
      tasks, ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                         base::TimeTicks() + base::TimeDelta::FromSeconds(11),
                         base::TimeTicks() + base::TimeDelta::FromSeconds(21),
                         base::TimeTicks() + base::TimeDelta::FromSeconds(31),
                         base::TimeTicks() + base::TimeDelta::FromSeconds(41)));
}

}  // namespace worker_scheduler_unittest
}  // namespace scheduler
}  // namespace blink
