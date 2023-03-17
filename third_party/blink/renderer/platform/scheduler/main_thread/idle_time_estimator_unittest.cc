// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/idle_time_estimator.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_time_observer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class IdleTimeEstimatorTest : public testing::Test {
 public:
  IdleTimeEstimatorTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        frame_length_(base::Milliseconds(16)) {}

  ~IdleTimeEstimatorTest() override = default;

  void SetUp() override {
    auto settings = base::sequence_manager::SequenceManager::Settings::Builder()
                        .SetPrioritySettings(CreatePrioritySettings())
                        .Build();
    manager_ = base::sequence_manager::SequenceManagerForTest::Create(
        nullptr, task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMockTickClock(), std::move(settings));
    estimator_ = std::make_unique<IdleTimeEstimator>(
        task_environment_.GetMockTickClock(), 10, 50);
    compositor_task_queue_1_ = NewTaskQueue();
    compositor_task_queue_2_ = NewTaskQueue();
    compositor_task_runner_1_ = compositor_task_queue_1_->CreateTaskRunner(
        TaskType::kMainThreadTaskQueueCompositor);
    compositor_task_runner_2_ = compositor_task_queue_2_->CreateTaskRunner(
        TaskType::kMainThreadTaskQueueCompositor);
    estimator_->AddCompositorTaskQueue(compositor_task_queue_1_);
    estimator_->AddCompositorTaskQueue(compositor_task_queue_2_);
  }

  scoped_refptr<MainThreadTaskQueue> NewTaskQueue() {
    return base::MakeRefCounted<MainThreadTaskQueue>(
        *manager_.get(),
        base::sequence_manager::TaskQueue::Spec(
            base::sequence_manager::QueueName::TEST_TQ),
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kCompositor),
        nullptr);
  }

  void SimulateFrameWithOneCompositorTask(int compositor_time) {
    base::TimeDelta non_idle_time = base::Milliseconds(compositor_time);
    PostTask(compositor_task_runner_1_, compositor_time, /*commit=*/true);
    if (non_idle_time < frame_length_)
      task_environment_.FastForwardBy(frame_length_ - non_idle_time);
  }

  void SimulateFrameWithTwoCompositorTasks(int compositor_time1,
                                           int compositor_time2) {
    base::TimeDelta non_idle_time1 = base::Milliseconds(compositor_time1);
    base::TimeDelta non_idle_time2 = base::Milliseconds(compositor_time2);
    PostTask(compositor_task_runner_1_, compositor_time1, /*commit=*/false);
    PostTask(compositor_task_runner_2_, compositor_time2, /*commit=*/true);
    base::TimeDelta idle_time = frame_length_ - non_idle_time1 - non_idle_time2;
    task_environment_.FastForwardBy(idle_time);
  }

  void PostTask(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                int compositor_time,
                bool commit) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::test::TaskEnvironment* task_environment,
               IdleTimeEstimator* estimator, int compositor_time, bool commit) {
              base::TimeDelta non_idle_time =
                  base::Milliseconds(compositor_time);
              task_environment->FastForwardBy(non_idle_time);
              if (commit)
                estimator->DidCommitFrameToCompositor();
            },
            &task_environment_, estimator_.get(), compositor_time, commit));
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::sequence_manager::SequenceManager> manager_;
  std::unique_ptr<IdleTimeEstimator> estimator_;
  scoped_refptr<MainThreadTaskQueue> compositor_task_queue_1_;
  scoped_refptr<MainThreadTaskQueue> compositor_task_queue_2_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_1_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_2_;
  const base::TimeDelta frame_length_;
  base::sequence_manager::TestTaskTimeObserver test_task_time_observer_;
};

TEST_F(IdleTimeEstimatorTest, InitialTimeEstimateWithNoData) {
  EXPECT_EQ(frame_length_, estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, BasicEstimation_SteadyState) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(5);

  EXPECT_EQ(base::Milliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, BasicEstimation_Variable) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(6);
  SimulateFrameWithOneCompositorTask(7);
  SimulateFrameWithOneCompositorTask(7);
  SimulateFrameWithOneCompositorTask(7);
  SimulateFrameWithOneCompositorTask(8);

  // We expect it to return the median.
  EXPECT_EQ(base::Milliseconds(9),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, NoIdleTime) {
  SimulateFrameWithOneCompositorTask(100);
  SimulateFrameWithOneCompositorTask(100);

  EXPECT_EQ(base::Milliseconds(0),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, Clear) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(5);

  EXPECT_EQ(base::Milliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
  estimator_->Clear();

  EXPECT_EQ(frame_length_, estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, Estimation_MultipleTasks) {
  SimulateFrameWithTwoCompositorTasks(1, 4);
  SimulateFrameWithTwoCompositorTasks(1, 4);

  EXPECT_EQ(base::Milliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, Estimation_MultipleTasks_WithSingleObserver) {
  // Observe only |compositor_task_queue_2_|
  estimator_->RemoveCompositorTaskQueue(compositor_task_queue_1_);
  SimulateFrameWithTwoCompositorTasks(1, 4);
  SimulateFrameWithTwoCompositorTasks(1, 4);

  EXPECT_EQ(base::Milliseconds(12),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, IgnoresNestedTasks) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(5);

  base::PendingTask task(FROM_HERE, base::OnceClosure());
  estimator_->WillProcessTask(task, /*was_blocked_or_low_priority=*/false);
  SimulateFrameWithTwoCompositorTasks(4, 4);
  SimulateFrameWithTwoCompositorTasks(4, 4);
  SimulateFrameWithTwoCompositorTasks(4, 4);
  SimulateFrameWithTwoCompositorTasks(4, 4);
  estimator_->DidCommitFrameToCompositor();
  estimator_->DidProcessTask(task);

  EXPECT_EQ(base::Milliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

}  // namespace scheduler
}  // namespace blink
