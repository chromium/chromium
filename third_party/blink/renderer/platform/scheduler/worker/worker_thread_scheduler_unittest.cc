// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/scoped_task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"

using testing::ElementsAreArray;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace worker_thread_scheduler_unittest {

void NopTask() {}

int TimeTicksToIntMs(const base::TimeTicks& time) {
  return static_cast<int>((time - base::TimeTicks()).InMilliseconds());
}

void RecordTimelineTask(std::vector<std::string>* timeline,
                        const base::TickClock* clock) {
  timeline->push_back(base::StringPrintf("run RecordTimelineTask @ %d",
                                         TimeTicksToIntMs(clock->NowTicks())));
}

void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(std::vector<std::string>* vector,
                                std::string value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void TimelineIdleTestTask(std::vector<std::string>* timeline,
                          base::TimeTicks deadline) {
  timeline->push_back(base::StringPrintf("run TimelineIdleTestTask deadline %d",
                                         TimeTicksToIntMs(deadline)));
}

class WorkerThreadSchedulerForTest : public WorkerThreadScheduler {
 public:
  WorkerThreadSchedulerForTest(
      std::unique_ptr<base::sequence_manager::SequenceManager> manager,
      const base::TickClock* clock_,
      std::vector<std::string>* timeline)
      : WorkerThreadScheduler(WebThreadType::kTestThread,
                              std::move(manager),
                              nullptr),
        clock_(clock_),
        timeline_(timeline) {}

  WorkerThreadSchedulerForTest(
      std::unique_ptr<base::sequence_manager::SequenceManager> manager,
      const base::TickClock* clock_,
      std::vector<std::string>* timeline,
      WorkerSchedulerProxy* proxy)
      : WorkerThreadScheduler(WebThreadType::kTestThread,
                              std::move(manager),
                              proxy),
        clock_(clock_),
        timeline_(timeline) {}

  using ThreadSchedulerImpl::SetUkmTaskSamplingRateForTest;
  using WorkerThreadScheduler::SetUkmRecorderForTest;

 private:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override {
    if (timeline_) {
      timeline_->push_back(base::StringPrintf("CanEnterLongIdlePeriod @ %d",
                                              TimeTicksToIntMs(now)));
    }
    return WorkerThreadScheduler::CanEnterLongIdlePeriod(
        now, next_long_idle_period_delay_out);
  }

  void IsNotQuiescent() override {
    if (timeline_) {
      timeline_->push_back(base::StringPrintf(
          "IsNotQuiescent @ %d", TimeTicksToIntMs(clock_->NowTicks())));
    }
    WorkerThreadScheduler::IsNotQuiescent();
  }

  const base::TickClock* clock_;        // Not owned.
  std::vector<std::string>* timeline_;  // Not owned.
};

class WorkerThreadSchedulerTest : public testing::Test {
 public:
  WorkerThreadSchedulerTest()
      : task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED),
        scheduler_(new WorkerThreadSchedulerForTest(
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr,
                task_environment_.GetMainThreadTaskRunner(),
                task_environment_.GetMockTickClock()),
            task_environment_.GetMockTickClock(),
            &timeline_)) {
    // Null clock might trigger some assertions.
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(5));
    scheduler_->Init();
    default_task_queue_ = scheduler_->CreateTaskQueue("test_tq");
    default_task_runner_ = default_task_queue_->CreateTaskRunner(0);
    idle_task_runner_ = scheduler_->IdleTaskRunner();
  }

  ~WorkerThreadSchedulerTest() override = default;

  void TearDown() override {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  void RunUntilIdle() {
    timeline_.push_back(base::StringPrintf(
        "RunUntilIdle begin @ %d",
        TimeTicksToIntMs(task_environment_.GetMockTickClock()->NowTicks())));
    // RunUntilIdle with auto-advancing for the mock clock.
    task_environment_.FastForwardUntilNoTasksRemain();
    timeline_.push_back(base::StringPrintf(
        "RunUntilIdle end @ %d",
        TimeTicksToIntMs(task_environment_.GetMockTickClock()->NowTicks())));
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'I': Idle task
  void PostTestTasks(std::vector<std::string>* run_order,
                     const std::string& task_descriptor) {
    std::istringstream stream(task_descriptor);
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'D':
          default_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorTestTask, run_order, task));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorIdleTestTask, run_order, task));
          break;
        default:
          NOTREACHED();
      }
    }
  }

  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMaximumIdlePeriodMillis);
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  std::vector<std::string> timeline_;
  std::unique_ptr<WorkerThreadSchedulerForTest> scheduler_;
  scoped_refptr<base::sequence_manager::TaskQueue> default_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThreadSchedulerTest);
};

TEST_F(WorkerThreadSchedulerTest, TestPostDefaultTask) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("D3"), std::string("D4")));
}

TEST_F(WorkerThreadSchedulerTest, TestPostIdleTask) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1");

  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("I1")));
}

TEST_F(WorkerThreadSchedulerTest, TestPostDefaultAndIdleTasks) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D2"), std::string("D3"),
                                   std::string("D4"), std::string("I1")));
}

TEST_F(WorkerThreadSchedulerTest, TestPostDefaultDelayedAndIdleTasks) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D2 D3 D4");

  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "DELAYED"),
      base::TimeDelta::FromMilliseconds(1000));

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D2"), std::string("D3"),
                                   std::string("D4"), std::string("I1"),
                                   std::string("DELAYED")));
}

TEST_F(WorkerThreadSchedulerTest, TestIdleTaskWhenIsNotQuiescent) {
  timeline_.push_back("Post default task");
  // Post a delayed task timed to occur mid way during the long idle period.
  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordTimelineTask, base::Unretained(&timeline_),
                     base::Unretained(task_environment_.GetMockTickClock())));
  RunUntilIdle();

  timeline_.push_back("Post idle task");
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TimelineIdleTestTask, base::Unretained(&timeline_)));

  RunUntilIdle();

  std::string expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 5",   "Post default task",
      "RunUntilIdle begin @ 5",       "run RecordTimelineTask @ 5",
      "RunUntilIdle end @ 5",         "Post idle task",
      "RunUntilIdle begin @ 5",       "IsNotQuiescent @ 5",
      "CanEnterLongIdlePeriod @ 305", "run TimelineIdleTestTask deadline 355",
      "RunUntilIdle end @ 305"};

  EXPECT_THAT(timeline_, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerThreadSchedulerTest, TestIdleDeadlineWithPendingDelayedTask) {
  timeline_.push_back("Post delayed and idle tasks");
  // Post a delayed task timed to occur mid way during the long idle period.
  default_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RecordTimelineTask, base::Unretained(&timeline_),
                     base::Unretained(task_environment_.GetMockTickClock())),
      base::TimeDelta::FromMilliseconds(20));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TimelineIdleTestTask, base::Unretained(&timeline_)));

  RunUntilIdle();

  std::string expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 5",
      "Post delayed and idle tasks",
      "RunUntilIdle begin @ 5",
      "CanEnterLongIdlePeriod @ 5",
      "run TimelineIdleTestTask deadline 25",  // Note the short 20ms deadline.
      "run RecordTimelineTask @ 25",
      "RunUntilIdle end @ 25"};

  EXPECT_THAT(timeline_, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerThreadSchedulerTest,
       TestIdleDeadlineWithPendingDelayedTaskFarInTheFuture) {
  timeline_.push_back("Post delayed and idle tasks");
  // Post a delayed task timed to occur well after the long idle period.
  default_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RecordTimelineTask, base::Unretained(&timeline_),
                     base::Unretained(task_environment_.GetMockTickClock())),
      base::TimeDelta::FromMilliseconds(500));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TimelineIdleTestTask, base::Unretained(&timeline_)));

  RunUntilIdle();

  std::string expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 5",
      "Post delayed and idle tasks",
      "RunUntilIdle begin @ 5",
      "CanEnterLongIdlePeriod @ 5",
      "run TimelineIdleTestTask deadline 55",  // Note the full 50ms deadline.
      "run RecordTimelineTask @ 505",
      "RunUntilIdle end @ 505"};

  EXPECT_THAT(timeline_, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerThreadSchedulerTest, TestPostIdleTaskAfterRunningUntilIdle) {
  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&NopTask),
      base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 I2 D3");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D3"), std::string("I1"),
                                   std::string("I2")));
}

void PostIdleTask(std::vector<std::string>* timeline,
                  const base::TickClock* clock,
                  SingleThreadIdleTaskRunner* idle_task_runner) {
  timeline->push_back(base::StringPrintf("run PostIdleTask @ %d",
                                         TimeTicksToIntMs(clock->NowTicks())));

  idle_task_runner->PostIdleTask(
      FROM_HERE, base::BindOnce(&TimelineIdleTestTask, timeline));
}

TEST_F(WorkerThreadSchedulerTest, TestLongIdlePeriodTimeline) {
  // The scheduler should not run the initiate_next_long_idle_period task if
  // there are no idle tasks and no other task woke up the scheduler, thus
  // the idle period deadline shouldn't update at the end of the current long
  // idle period.
  base::TimeTicks idle_period_deadline =
      scheduler_->CurrentIdleTaskDeadlineForTesting();
  // Not printed in the timeline.
  task_environment_.FastForwardBy(maximum_idle_period_duration());

  base::TimeTicks new_idle_period_deadline =
      scheduler_->CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);

  // Post a task to post an idle task. Because the system is non-quiescent a
  // 300ms pause will occur before the next long idle period is initiated and
  // the idle task run.
  default_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PostIdleTask, base::Unretained(&timeline_),
                     base::Unretained(task_environment_.GetMockTickClock()),
                     base::Unretained(idle_task_runner_.get())),
      base::TimeDelta::FromMilliseconds(30));

  timeline_.push_back("PostFirstIdleTask");
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TimelineIdleTestTask, base::Unretained(&timeline_)));
  RunUntilIdle();
  new_idle_period_deadline = scheduler_->CurrentIdleTaskDeadlineForTesting();

  // Running a normal task will mark the system as non-quiescent.
  timeline_.push_back("Post RecordTimelineTask");
  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordTimelineTask, base::Unretained(&timeline_),
                     base::Unretained(task_environment_.GetMockTickClock())));
  RunUntilIdle();

  std::string expected_timeline[] = {"CanEnterLongIdlePeriod @ 5",
                                     "PostFirstIdleTask",
                                     "RunUntilIdle begin @ 55",
                                     "CanEnterLongIdlePeriod @ 55",
                                     "run TimelineIdleTestTask deadline 85",
                                     "run PostIdleTask @ 85",
                                     "IsNotQuiescent @ 85",
                                     "CanEnterLongIdlePeriod @ 385",
                                     "run TimelineIdleTestTask deadline 435",
                                     "RunUntilIdle end @ 385",
                                     "Post RecordTimelineTask",
                                     "RunUntilIdle begin @ 385",
                                     "run RecordTimelineTask @ 385",
                                     "RunUntilIdle end @ 385"};

  EXPECT_THAT(timeline_, ElementsAreArray(expected_timeline));
}

namespace {

class FrameSchedulerDelegateWithUkmSourceId : public FrameScheduler::Delegate {
 public:
  FrameSchedulerDelegateWithUkmSourceId(ukm::SourceId source_id)
      : source_id_(source_id) {}

  ~FrameSchedulerDelegateWithUkmSourceId() override {}

  ukm::UkmRecorder* GetUkmRecorder() override { return nullptr; }

  ukm::SourceId GetUkmSourceId() override { return source_id_; }

 private:
  ukm::SourceId source_id_;
};

}  // namespace

class WorkerThreadSchedulerWithProxyTest : public testing::Test {
 public:
  WorkerThreadSchedulerWithProxyTest()
      : task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED) {
    frame_scheduler_delegate_ =
        std::make_unique<FrameSchedulerDelegateWithUkmSourceId>(42);
    frame_scheduler_ = FakeFrameScheduler::Builder()
                           .SetIsPageVisible(false)
                           .SetFrameType(FrameScheduler::FrameType::kSubframe)
                           .SetIsCrossOrigin(true)
                           .SetDelegate(frame_scheduler_delegate_.get())
                           .Build();
    frame_scheduler_->SetCrossOrigin(true);

    worker_scheduler_proxy_ =
        std::make_unique<WorkerSchedulerProxy>(frame_scheduler_.get());

    scheduler_ = std::make_unique<WorkerThreadSchedulerForTest>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMockTickClock()),
        task_environment_.GetMockTickClock(), &timeline_,
        worker_scheduler_proxy_.get());

    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(5));

    scheduler_->Init();
  }

  ~WorkerThreadSchedulerWithProxyTest() override = default;

  void TearDown() override {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  std::vector<std::string> timeline_;
  std::unique_ptr<FrameScheduler::Delegate> frame_scheduler_delegate_;
  std::unique_ptr<FrameScheduler> frame_scheduler_;
  std::unique_ptr<WorkerSchedulerProxy> worker_scheduler_proxy_;
  std::unique_ptr<WorkerThreadSchedulerForTest> scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThreadSchedulerWithProxyTest);
};

TEST_F(WorkerThreadSchedulerWithProxyTest, UkmTaskRecording) {
  internal::ProcessState::Get()->is_process_backgrounded = true;

  std::unique_ptr<ukm::TestUkmRecorder> owned_ukm_recorder =
      std::make_unique<ukm::TestUkmRecorder>();
  ukm::TestUkmRecorder* ukm_recorder = owned_ukm_recorder.get();

  scheduler_->SetUkmTaskSamplingRateForTest(1);
  scheduler_->SetUkmRecorderForTest(std::move(owned_ukm_recorder));

  base::sequence_manager::FakeTask task(
      static_cast<int>(TaskType::kJavascriptTimer));
  base::sequence_manager::FakeTaskTiming task_timing(
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(200),
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(700),
      base::ThreadTicks() + base::TimeDelta::FromMilliseconds(250),
      base::ThreadTicks() + base::TimeDelta::FromMilliseconds(500));

  scheduler_->OnTaskCompleted(nullptr, task, task_timing);

  auto entries = ukm_recorder->GetEntriesByName("RendererSchedulerTask");

  EXPECT_EQ(entries.size(), static_cast<size_t>(1));

  ukm::TestUkmRecorder::ExpectEntryMetric(
      entries[0], "ThreadType", static_cast<int>(WebThreadType::kTestThread));
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[0], "RendererBackgrounded",
                                          true);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entries[0], "TaskType", static_cast<int>(TaskType::kJavascriptTimer));
  ukm::TestUkmRecorder::ExpectEntryMetric(
      entries[0], "FrameStatus",
      static_cast<int>(FrameStatus::kCrossOriginBackground));
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[0], "TaskDuration", 500000);
  ukm::TestUkmRecorder::ExpectEntryMetric(entries[0], "TaskCPUDuration",
                                          250000);
}

}  // namespace worker_thread_scheduler_unittest
}  // namespace scheduler
}  // namespace blink
