// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_interference_recorder.h"

#include "base/task/sequence_manager/task_queue.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"

namespace blink {
namespace scheduler {
namespace frame_interference_recorder_test {

using base::sequence_manager::LazyNow;
using base::sequence_manager::Task;

constexpr base::TimeDelta kDelay = base::TimeDelta::FromSeconds(10);

class FrameInterferenceRecorderTest : public testing::Test {
 public:
  class MockFrameInterferenceRecorder : public FrameInterferenceRecorder {
   public:
    MockFrameInterferenceRecorder(FrameInterferenceRecorderTest* outer)
        : FrameInterferenceRecorder(/* sampling_rate */ 1), outer_(outer) {}

    void RecordHistogram(const MainThreadTaskQueue* queue,
                         base::TimeDelta sample) override {
      MockRecordHistogram(queue, sample);
    }

    FrameScheduler* GetFrameSchedulerForQueue(
        const MainThreadTaskQueue* queue) override {
      if (queue == outer_->queue_frame_agent_a_ ||
          queue == outer_->other_queue_frame_agent_a_)
        return outer_->frame_agent_a_.get();
      if (queue == outer_->queue_frame_agent_b_)
        return outer_->frame_agent_b_.get();
      if (queue == outer_->queue_frame_agent_c_)
        return outer_->frame_agent_c_.get();
      if (queue == outer_->queue_other_frame_agent_c_)
        return outer_->other_frame_agent_c_.get();
      return nullptr;
    }
    const base::UnguessableToken& GetAgentClusterIdForQueue(
        const MainThreadTaskQueue* queue) override {
      if (queue == outer_->queue_frame_agent_a_ ||
          queue == outer_->other_queue_frame_agent_a_)
        return outer_->agent_a_;
      if (queue == outer_->queue_frame_agent_b_)
        return outer_->agent_b_;
      if (queue == outer_->queue_frame_agent_c_ ||
          queue == outer_->queue_other_frame_agent_c_)
        return outer_->agent_c_;
      return base::UnguessableToken::Null();
    }

    MOCK_METHOD2(MockRecordHistogram,
                 void(const MainThreadTaskQueue*, base::TimeDelta));

   private:
    FrameInterferenceRecorderTest* const outer_;
  };

  class ScopedExpectSample {
   public:
    ScopedExpectSample(FrameInterferenceRecorderTest* test,
                       MainThreadTaskQueue* queue,
                       base::TimeDelta expected)
        : test_(test) {
      EXPECT_CALL(test_->recorder_, MockRecordHistogram(queue, expected));
    }
    ~ScopedExpectSample() { testing::Mock::VerifyAndClear(&test_->recorder_); }

   private:
    FrameInterferenceRecorderTest* const test_;
  };

  FrameInterferenceRecorderTest() = default;

  static base::sequence_manager::EnqueueOrder EnqueueOrder(int enqueue_order) {
    return base::sequence_manager::EnqueueOrder::FromIntForTesting(
        enqueue_order);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  base::TimeTicks NowTicks() const { return task_environment_.NowTicks(); }

  void OnTaskReady(FrameScheduler* frame_scheduler,
                   base::sequence_manager::EnqueueOrder enqueue_order) {
    base::sequence_manager::LazyNow lazy_now(GetMockTickClock());
    recorder_.OnTaskReady(frame_scheduler, enqueue_order, &lazy_now);
  }

  const base::TickClock* GetMockTickClock() const {
    return task_environment_.GetMockTickClock();
  }

  testing::StrictMock<MockFrameInterferenceRecorder> recorder_{this};

  base::UnguessableToken agent_a_ = base::UnguessableToken::Create();
  base::UnguessableToken agent_b_ = base::UnguessableToken::Create();
  base::UnguessableToken agent_c_ = base::UnguessableToken::Create();

  std::unique_ptr<FrameScheduler> frame_agent_a_ = CreateDummyFrameScheduler();
  std::unique_ptr<FrameScheduler> frame_agent_b_ = CreateDummyFrameScheduler();
  std::unique_ptr<FrameScheduler> frame_agent_c_ = CreateDummyFrameScheduler();
  std::unique_ptr<FrameScheduler> other_frame_agent_c_ =
      CreateDummyFrameScheduler();

  scoped_refptr<MainThreadTaskQueue> queue_frame_agent_a_ =
      CreateMainThreadTaskQueue();
  scoped_refptr<MainThreadTaskQueue> other_queue_frame_agent_a_ =
      CreateMainThreadTaskQueue();
  scoped_refptr<MainThreadTaskQueue> queue_frame_agent_b_ =
      CreateMainThreadTaskQueue();
  scoped_refptr<MainThreadTaskQueue> queue_frame_agent_c_ =
      CreateMainThreadTaskQueue();
  scoped_refptr<MainThreadTaskQueue> queue_other_frame_agent_c_ =
      CreateMainThreadTaskQueue();

  // GetFrameSchedulerForQueue() will return nullptr for this queue.
  scoped_refptr<MainThreadTaskQueue> queue_no_frame_ =
      CreateMainThreadTaskQueue();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<MainThreadTaskQueue> CreateMainThreadTaskQueue() {
    return WrapRefCounted(new MainThreadTaskQueue(
        nullptr, base::sequence_manager::TaskQueue::Spec(""),
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kDefault),
        nullptr));
  }
};

// Verify that zero interference is recorded if no task runs between when a
// frame task is posted and when it runs.
TEST_F(FrameInterferenceRecorderTest, NoInterferenceSingleTask) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(1));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(), EnqueueOrder(1),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), end);
  }
}

// Verify that zero interference is recorded when tasks from the same queue run.
TEST_F(FrameInterferenceRecorderTest, NoInterferenceMultipleTasksSameQueue) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(1));
  FastForwardBy(kDelay);
  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(2));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(), EnqueueOrder(1),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(), EnqueueOrder(2),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), end);
  }
}

// Verify that zero interference is recorded when tasks from different queues
// associated with the same frame run.
TEST_F(FrameInterferenceRecorderTest, NoInterferenceMultipleQueuesSameAgent) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(1));
  FastForwardBy(kDelay);
  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(2));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(), EnqueueOrder(1),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, other_queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(other_queue_frame_agent_a_.get(), EnqueueOrder(2),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(other_queue_frame_agent_a_.get(), end);
  }
}

// Verify that zero interference is recorded when tasks from different frame
// associated with the same agent run.
TEST_F(FrameInterferenceRecorderTest, NoInterferenceMultipleFramesSameAgent) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_c_.get(), EnqueueOrder(1));
  FastForwardBy(kDelay);
  OnTaskReady(other_frame_agent_c_.get(), EnqueueOrder(2));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_c_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_c_.get(), EnqueueOrder(1),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_c_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_other_frame_agent_c_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_other_frame_agent_c_.get(), EnqueueOrder(2),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_other_frame_agent_c_.get(), end);
  }
}

// Verify that zero interference is recorded when a non-frame task runs between
// when a frame task is ready and when it runs.
TEST_F(FrameInterferenceRecorderTest, NoInterferenceNoAgentQueue) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(nullptr, EnqueueOrder(1));
  FastForwardBy(kDelay);
  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(2));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    recorder_.OnTaskStarted(queue_no_frame_.get(), EnqueueOrder(1), start);
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_no_frame_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, other_queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(other_queue_frame_agent_a_.get(), EnqueueOrder(2),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(other_queue_frame_agent_a_.get(), end);
  }
}

// Verify that interference is recorded when a task from another agent runs
// between when a agent task becomes ready and when it runs.
TEST_F(FrameInterferenceRecorderTest, InterferenceFromOneOtherAgent) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(1));
  FastForwardBy(kDelay);
  OnTaskReady(frame_agent_b_.get(), EnqueueOrder(2));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(), EnqueueOrder(1),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_b_.get(),
                                       kDelay);
      recorder_.OnTaskStarted(queue_frame_agent_b_.get(), EnqueueOrder(2),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_b_.get(), end);
  }
}

// Verify that interference is recorded correctly when tasks from multiple
// agents run.
TEST_F(FrameInterferenceRecorderTest, InterferenceFromManyOtherFrames) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(1));
  // Add FastForwardBy()'s in between; those shouldn't matter.
  FastForwardBy(kDelay * 32);
  OnTaskReady(frame_agent_b_.get(), EnqueueOrder(2));
  FastForwardBy(kDelay * 64);
  OnTaskReady(frame_agent_c_.get(), EnqueueOrder(3));
  FastForwardBy(kDelay * 128);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(), EnqueueOrder(1),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), end);
  }

  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(4));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      // Had to wait for task 1.
      ScopedExpectSample expect_sample(this, queue_frame_agent_b_.get(),
                                       kDelay);
      recorder_.OnTaskStarted(queue_frame_agent_b_.get(), EnqueueOrder(2),
                              start);
    }
    FastForwardBy(2 * kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_b_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      // Had to wait for tasks 1 and 2.
      ScopedExpectSample expect_sample(this, queue_frame_agent_c_.get(),
                                       3 * kDelay);
      recorder_.OnTaskStarted(queue_frame_agent_c_.get(), EnqueueOrder(3),
                              start);
    }
    FastForwardBy(4 * kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_c_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      // Had to wait for tasks 2 and 3.
      ScopedExpectSample expect_sample(this, other_queue_frame_agent_a_.get(),
                                       6 * kDelay);
      recorder_.OnTaskStarted(other_queue_frame_agent_a_.get(), EnqueueOrder(4),
                              start);
    }
    FastForwardBy(8 * kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(other_queue_frame_agent_a_.get(), end);
  }
}

// Verify that interference is recorded correctly when there are nested tasks.
TEST_F(FrameInterferenceRecorderTest, Nesting) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(1));
  FastForwardBy(kDelay);
  OnTaskReady(frame_agent_b_.get(), EnqueueOrder(2));
  FastForwardBy(kDelay);
  OnTaskReady(frame_agent_b_.get(), EnqueueOrder(3));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(), EnqueueOrder(1),
                              start);
    }
    FastForwardBy(kDelay);

    // Run task 2 nested.
    {
      // When a nested loop is entered, complete the current task.
      recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), NowTicks());

      const base::TimeTicks nested_start = NowTicks();
      {
        ScopedExpectSample expect_sample(this, queue_frame_agent_b_.get(),
                                         kDelay);
        recorder_.OnTaskStarted(queue_frame_agent_b_.get(), EnqueueOrder(2),
                                nested_start);
      }
      FastForwardBy(8 * kDelay);
      const base::TimeTicks nested_end = NowTicks();
      recorder_.OnTaskCompleted(queue_frame_agent_b_.get(), nested_end);

      // When a nested loop is exited, resume the task that was running when the
      // nested loop was entered.
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(),
                              base::sequence_manager::EnqueueOrder::none(),
                              NowTicks());
    }

    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      // Only includes the execution time of task 1, not the nested execution
      // time of task 2, which is from the same frame.
      ScopedExpectSample expect_sample(this, queue_frame_agent_b_.get(),
                                       2 * kDelay);
      recorder_.OnTaskStarted(queue_frame_agent_b_.get(), EnqueueOrder(3),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_b_.get(), end);
  }
}

// Verify that interference is recorded correctly when a task becomes ready
// while another task is running.
TEST_F(FrameInterferenceRecorderTest, ReadyDuringRun) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_a_.get(), EnqueueOrder(1));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_a_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_a_.get(), EnqueueOrder(1),
                              start);
    }

    FastForwardBy(kDelay);
    // Post task 2 in the middle of running task 1.
    OnTaskReady(frame_agent_b_.get(), EnqueueOrder(2));
    FastForwardBy(kDelay);

    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_a_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_b_.get(),
                                       kDelay);
      recorder_.OnTaskStarted(queue_frame_agent_b_.get(), EnqueueOrder(2),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_b_.get(), end);
  }
}

// Verify that OnFrameSchedulerDestroyed doesn't clear data associated to an
// agent that is referred to by other frames.
TEST_F(FrameInterferenceRecorderTest, OnFrameSchedulerDestroyed) {
  SCOPED_TRACE(NowTicks());

  OnTaskReady(frame_agent_c_.get(), EnqueueOrder(1));
  FastForwardBy(kDelay);
  OnTaskReady(other_frame_agent_c_.get(), EnqueueOrder(2));
  FastForwardBy(kDelay);

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_frame_agent_c_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_frame_agent_c_.get(), EnqueueOrder(1),
                              start);
    }
    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_frame_agent_c_.get(), end);
  }

  {
    const base::TimeTicks start = NowTicks();
    {
      ScopedExpectSample expect_sample(this, queue_other_frame_agent_c_.get(),
                                       base::TimeDelta());
      recorder_.OnTaskStarted(queue_other_frame_agent_c_.get(), EnqueueOrder(2),
                              start);
    }
    // This should not clear data associated with |agent_c_|.
    recorder_.OnFrameSchedulerDestroyed(frame_agent_c_.get());

    FastForwardBy(kDelay);
    const base::TimeTicks end = NowTicks();
    recorder_.OnTaskCompleted(queue_other_frame_agent_c_.get(), end);
  }
}

}  // namespace frame_interference_recorder_test
}  // namespace scheduler
}  // namespace blink
