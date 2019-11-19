// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

#include "base/bind.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/test_mock_time_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {

TestingPlatformSupportWithMockScheduler::
    TestingPlatformSupportWithMockScheduler()
    : test_task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::TestMockTimeTaskRunner::Type::kStandalone)) {
  DCHECK(IsMainThread());
  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));
  std::unique_ptr<base::sequence_manager::SequenceManagerForTest>
      sequence_manager = base::sequence_manager::SequenceManagerForTest::Create(
          nullptr, test_task_runner_, test_task_runner_->GetMockTickClock());
  sequence_manager_ = sequence_manager.get();

  scheduler_ = std::make_unique<scheduler::MainThreadSchedulerImpl>(
      std::move(sequence_manager), base::nullopt);
  main_thread_overrider_ = std::make_unique<ScopedMainThreadOverrider>(
      scheduler_->CreateMainThread());
  // Set the work batch size to one so TakePendingTasks behaves as expected.
  scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(1);
}

TestingPlatformSupportWithMockScheduler::
    ~TestingPlatformSupportWithMockScheduler() {
  scheduler_->Shutdown();
}

void TestingPlatformSupportWithMockScheduler::RunSingleTask() {
  base::circular_deque<base::TestPendingTask> tasks =
      test_task_runner_->TakePendingTasks();
  if (tasks.empty())
    return;
  // Scheduler doesn't post more than one task.
  DCHECK_EQ(tasks.size(), 1u);
  base::TestPendingTask task = std::move(tasks.front());
  tasks.clear();
  // Set clock to the beginning of task and run it.
  test_task_runner_->AdvanceMockTickClock(task.GetTimeToRun() -
                                          test_task_runner_->NowTicks());
  std::move(task.task).Run();
}

void TestingPlatformSupportWithMockScheduler::RunUntilIdle() {
  if (auto_advance_) {
    test_task_runner_->FastForwardUntilNoTasksRemain();
  } else {
    test_task_runner_->RunUntilIdle();
  }
}

void TestingPlatformSupportWithMockScheduler::RunForPeriodSeconds(
    double seconds) {
  RunForPeriod(base::TimeDelta::FromSecondsD(seconds));
}

void TestingPlatformSupportWithMockScheduler::RunForPeriod(
    base::TimeDelta period) {
  test_task_runner_->FastForwardBy(period);
}

void TestingPlatformSupportWithMockScheduler::AdvanceClockSeconds(
    double seconds) {
  AdvanceClock(base::TimeDelta::FromSecondsD(seconds));
}

void TestingPlatformSupportWithMockScheduler::AdvanceClock(
    base::TimeDelta duration) {
  test_task_runner_->AdvanceMockTickClock(duration);
}

void TestingPlatformSupportWithMockScheduler::SetAutoAdvanceNowToPendingTasks(
    bool auto_advance) {
  auto_advance_ = auto_advance;
}

scheduler::MainThreadSchedulerImpl*
TestingPlatformSupportWithMockScheduler::GetMainThreadScheduler() const {
  return scheduler_.get();
}

}  // namespace blink
