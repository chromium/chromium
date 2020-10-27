// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/idle_time_estimator.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_queue.h"
#include "base/task/sequence_manager/test/test_task_time_observer.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class IdleTimeEstimatorForTest : public IdleTimeEstimator {
 public:
  IdleTimeEstimatorForTest(
      const scoped_refptr<MainThreadTaskQueue>& compositor_task_runner,
      const base::TickClock* clock,
      int sample_count,
      double estimation_percentile)
      : IdleTimeEstimator(compositor_task_runner,
                          clock,
                          sample_count,
                          estimation_percentile) {}
};

class IdleTimeEstimatorTest : public testing::Test {
 public:
  IdleTimeEstimatorTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        frame_length_(base::TimeDelta::FromMilliseconds(16)) {}

  ~IdleTimeEstimatorTest() override = default;

  void SetUp() override {
    manager_ = base::sequence_manager::SequenceManagerForTest::Create(
        nullptr, task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMockTickClock());
    compositor_task_queue_ =
        manager_->CreateTaskQueueWithType<MainThreadTaskQueue>(
            base::sequence_manager::TaskQueue::Spec("test_tq"),
            MainThreadTaskQueue::QueueCreationParams(
                MainThreadTaskQueue::QueueType::kCompositor),
            nullptr);
    estimator_.reset(new IdleTimeEstimatorForTest(
        compositor_task_queue_, task_environment_.GetMockTickClock(), 10, 50));
  }

  void SimulateFrameWithOneCompositorTask(int compositor_time) {
    base::TimeDelta non_idle_time =
        base::TimeDelta::FromMilliseconds(compositor_time);
    base::PendingTask task(FROM_HERE, base::OnceClosure());
    estimator_->WillProcessTask(task, /*was_blocked_or_low_priority=*/false);
    task_environment_.FastForwardBy(non_idle_time);
    estimator_->DidCommitFrameToCompositor();
    estimator_->DidProcessTask(task);
    if (non_idle_time < frame_length_)
      task_environment_.FastForwardBy(frame_length_ - non_idle_time);
  }

  void SimulateFrameWithTwoCompositorTasks(int compositor_time1,
                                           int compositor_time2) {
    base::TimeDelta non_idle_time1 =
        base::TimeDelta::FromMilliseconds(compositor_time1);
    base::TimeDelta non_idle_time2 =
        base::TimeDelta::FromMilliseconds(compositor_time2);
    base::PendingTask task(FROM_HERE, base::OnceClosure());
    estimator_->WillProcessTask(task, /*was_blocked_or_low_priority=*/false);
    task_environment_.FastForwardBy(non_idle_time1);
    estimator_->DidProcessTask(task);

    estimator_->WillProcessTask(task, /*was_blocked_or_low_priority=*/false);
    task_environment_.FastForwardBy(non_idle_time2);
    estimator_->DidCommitFrameToCompositor();
    estimator_->DidProcessTask(task);

    base::TimeDelta idle_time = frame_length_ - non_idle_time1 - non_idle_time2;
    task_environment_.FastForwardBy(idle_time);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::sequence_manager::SequenceManager> manager_;
  scoped_refptr<MainThreadTaskQueue> compositor_task_queue_;
  std::unique_ptr<IdleTimeEstimatorForTest> estimator_;
  const base::TimeDelta frame_length_;
  base::sequence_manager::TestTaskTimeObserver test_task_time_observer_;
};

TEST_F(IdleTimeEstimatorTest, InitialTimeEstimateWithNoData) {
  EXPECT_EQ(frame_length_, estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, BasicEstimation_SteadyState) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(5);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
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
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(9),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, NoIdleTime) {
  SimulateFrameWithOneCompositorTask(100);
  SimulateFrameWithOneCompositorTask(100);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, Clear) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(5);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
  estimator_->Clear();

  EXPECT_EQ(frame_length_, estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, Estimation_MultipleTasks) {
  SimulateFrameWithTwoCompositorTasks(1, 4);
  SimulateFrameWithTwoCompositorTasks(1, 4);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
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

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

}  // namespace scheduler
}  // namespace blink
