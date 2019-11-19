// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/recording_task_time_observer.h"

using testing::ElementsAreArray;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace worker_thread_scheduler_unittest {

namespace {

void NopTask() {}

// Instantiated at the beginning of each test. |timeline_start_ticks_| can be
// used to offset the original Now() against future timings to helper
// readability of the test cases.
class ScopedSaveStartTicks {
 public:
  ScopedSaveStartTicks(base::TimeTicks now) {
    DCHECK(timeline_start_ticks_.is_null());
    timeline_start_ticks_ = now;
  }

  ~ScopedSaveStartTicks() { timeline_start_ticks_ = base::TimeTicks(); }

  static base::TimeTicks timeline_start_ticks_;
};

// static
base::TimeTicks ScopedSaveStartTicks::timeline_start_ticks_;

int TimeTicksToIntMs(const base::TimeTicks& time) {
  return static_cast<int>(
      (time - ScopedSaveStartTicks::timeline_start_ticks_).InMilliseconds());
}

void RecordTimelineTask(Vector<String>* timeline,
                        const base::TickClock* clock) {
  timeline->push_back(String::Format("run RecordTimelineTask @ %d",
                                     TimeTicksToIntMs(clock->NowTicks())));
}

void AppendToVectorTestTask(Vector<String>* vector, String value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(Vector<String>* vector,
                                String value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void TimelineIdleTestTask(Vector<String>* timeline, base::TimeTicks deadline) {
  timeline->push_back(String::Format("run TimelineIdleTestTask deadline %d",
                                     TimeTicksToIntMs(deadline)));
}

class WorkerThreadSchedulerForTest : public WorkerThreadScheduler {
 public:
  WorkerThreadSchedulerForTest(base::sequence_manager::SequenceManager* manager,
                               const base::TickClock* clock_,
                               Vector<String>* timeline)
      : WorkerThreadScheduler(ThreadType::kTestThread, manager, nullptr),
        clock_(clock_),
        timeline_(timeline) {}

  WorkerThreadSchedulerForTest(base::sequence_manager::SequenceManager* manager,
                               const base::TickClock* clock_,
                               Vector<String>* timeline,
                               WorkerSchedulerProxy* proxy)
      : WorkerThreadScheduler(ThreadType::kTestThread, manager, proxy),
        clock_(clock_),
        timeline_(timeline) {}

  using WorkerThreadScheduler::SetUkmRecorderForTest;
  using WorkerThreadScheduler::SetUkmTaskSamplingRateForTest;

  void AddTaskTimeObserver(base::sequence_manager::TaskTimeObserver* observer) {
    helper()->AddTaskTimeObserver(observer);
  }

  void RemoveTaskTimeObserver(
      base::sequence_manager::TaskTimeObserver* observer) {
    helper()->RemoveTaskTimeObserver(observer);
  }

  void set_on_microtask_checkpoint(base::OnceClosure cb) {
    on_microtask_checkpoint_ = std::move(cb);
  }

 private:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override {
    if (timeline_) {
      timeline_->push_back(
          String::Format("CanEnterLongIdlePeriod @ %d", TimeTicksToIntMs(now)));
    }
    return WorkerThreadScheduler::CanEnterLongIdlePeriod(
        now, next_long_idle_period_delay_out);
  }

  void IsNotQuiescent() override {
    if (timeline_) {
      timeline_->push_back(String::Format(
          "IsNotQuiescent @ %d", TimeTicksToIntMs(clock_->NowTicks())));
    }
    WorkerThreadScheduler::IsNotQuiescent();
  }

  void PerformMicrotaskCheckpoint() override {
    if (on_microtask_checkpoint_)
      std::move(on_microtask_checkpoint_).Run();
  }

  const base::TickClock* clock_;        // Not owned.
  Vector<String>* timeline_;            // Not owned.
  base::OnceClosure on_microtask_checkpoint_;
};

class WorkerThreadSchedulerTest : public testing::Test {
 public:
  WorkerThreadSchedulerTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        sequence_manager_(
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr,
                task_environment_.GetMainThreadTaskRunner(),
                task_environment_.GetMockTickClock())),
        scheduler_(new WorkerThreadSchedulerForTest(
            sequence_manager_.get(),
            task_environment_.GetMockTickClock(),
            &timeline_)) {
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
    timeline_.push_back(String::Format(
        "RunUntilIdle begin @ %d",
        TimeTicksToIntMs(task_environment_.GetMockTickClock()->NowTicks())));
    // RunUntilIdle with auto-advancing for the mock clock.
    task_environment_.FastForwardUntilNoTasksRemain();
    timeline_.push_back(String::Format(
        "RunUntilIdle end @ %d",
        TimeTicksToIntMs(task_environment_.GetMockTickClock()->NowTicks())));
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'I': Idle task
  void PostTestTasks(Vector<String>* run_order, const String& task_descriptor) {
    std::istringstream stream(task_descriptor.Utf8());
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'D':
          default_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE, base::BindOnce(&AppendToVectorIdleTestTask, run_order,
                                        String::FromUTF8(task)));
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
  base::test::TaskEnvironment task_environment_;
  // Needs to be initialized immediately after |task_environment_|, specifically
  // before |scheduler_|.
  ScopedSaveStartTicks save_start_ticks_{task_environment_.NowTicks()};
  std::unique_ptr<base::sequence_manager::SequenceManagerForTest>
      sequence_manager_;
  Vector<String> timeline_;
  std::unique_ptr<WorkerThreadSchedulerForTest> scheduler_;
  scoped_refptr<base::sequence_manager::TaskQueue> default_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThreadSchedulerTest);
};

}  // namespace

TEST_F(WorkerThreadSchedulerTest, TestPostDefaultTask) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "D3", "D4"));
}

TEST_F(WorkerThreadSchedulerTest, TestPostIdleTask) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1");

  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("I1"));
}

TEST_F(WorkerThreadSchedulerTest, TestPostDefaultAndIdleTasks) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D2", "D3", "D4", "I1"));
}

TEST_F(WorkerThreadSchedulerTest, TestPostDefaultDelayedAndIdleTasks) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D2 D3 D4");

  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AppendToVectorTestTask, &run_order, "DELAYED"),
      base::TimeDelta::FromMilliseconds(1000));

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("D2", "D3", "D4", "I1", "DELAYED"));
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

  String expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 0",   "Post default task",
      "RunUntilIdle begin @ 0",       "run RecordTimelineTask @ 0",
      "RunUntilIdle end @ 0",         "Post idle task",
      "RunUntilIdle begin @ 0",       "IsNotQuiescent @ 0",
      "CanEnterLongIdlePeriod @ 300", "run TimelineIdleTestTask deadline 350",
      "RunUntilIdle end @ 300"};

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

  String expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 0",
      "Post delayed and idle tasks",
      "RunUntilIdle begin @ 0",
      "CanEnterLongIdlePeriod @ 0",
      "run TimelineIdleTestTask deadline 20",  // Note the short 20ms deadline.
      "run RecordTimelineTask @ 20",
      "RunUntilIdle end @ 20"};

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

  String expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 0",
      "Post delayed and idle tasks",
      "RunUntilIdle begin @ 0",
      "CanEnterLongIdlePeriod @ 0",
      "run TimelineIdleTestTask deadline 50",  // Note the full 50ms deadline.
      "run RecordTimelineTask @ 500",
      "RunUntilIdle end @ 500"};

  EXPECT_THAT(timeline_, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerThreadSchedulerTest, TestPostIdleTaskAfterRunningUntilIdle) {
  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&NopTask),
      base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 I2 D3");

  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D3", "I1", "I2"));
}

void PostIdleTask(Vector<String>* timeline,
                  const base::TickClock* clock,
                  SingleThreadIdleTaskRunner* idle_task_runner) {
  timeline->push_back(String::Format("run PostIdleTask @ %d",
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

  String expected_timeline[] = {"CanEnterLongIdlePeriod @ 0",
                                "PostFirstIdleTask",
                                "RunUntilIdle begin @ 50",
                                "CanEnterLongIdlePeriod @ 50",
                                "run TimelineIdleTestTask deadline 80",
                                "run PostIdleTask @ 80",
                                "IsNotQuiescent @ 80",
                                "CanEnterLongIdlePeriod @ 380",
                                "run TimelineIdleTestTask deadline 430",
                                "RunUntilIdle end @ 380",
                                "Post RecordTimelineTask",
                                "RunUntilIdle begin @ 380",
                                "run RecordTimelineTask @ 380",
                                "RunUntilIdle end @ 380"};

  EXPECT_THAT(timeline_, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerThreadSchedulerTest, TestMicrotaskCheckpointTiming) {
  const base::TimeDelta kTaskTime = base::TimeDelta::FromMilliseconds(100);
  const base::TimeDelta kMicrotaskTime = base::TimeDelta::FromMilliseconds(200);

  base::TimeTicks start_time = task_environment_.NowTicks();
  default_task_runner_->PostTask(
      FROM_HERE, WTF::Bind(&base::test::TaskEnvironment::FastForwardBy,
                           base::Unretained(&task_environment_), kTaskTime));
  scheduler_->set_on_microtask_checkpoint(
      WTF::Bind(&base::test::TaskEnvironment::FastForwardBy,
                base::Unretained(&task_environment_), kMicrotaskTime));

  RecordingTaskTimeObserver observer;

  scheduler_->AddTaskTimeObserver(&observer);
  RunUntilIdle();
  scheduler_->RemoveTaskTimeObserver(&observer);

  // Expect that the duration of microtask is counted as a part of the preceding
  // task.
  ASSERT_EQ(1u, observer.result().size());
  EXPECT_EQ(start_time, observer.result().back().first);
  EXPECT_EQ(start_time + kTaskTime + kMicrotaskTime,
            observer.result().back().second);
}

namespace {

class FrameSchedulerDelegateWithUkmSourceId : public FrameScheduler::Delegate {
 public:
  FrameSchedulerDelegateWithUkmSourceId(ukm::SourceId source_id)
      : source_id_(source_id) {}

  ~FrameSchedulerDelegateWithUkmSourceId() override {}

  ukm::UkmRecorder* GetUkmRecorder() override { return nullptr; }

  ukm::SourceId GetUkmSourceId() override { return source_id_; }

  void UpdateTaskTime(base::TimeDelta time) override {}

  void UpdateActiveSchedulerTrackedFeatures(uint64_t features_mask) override {}

  const base::UnguessableToken& GetAgentClusterId() const override {
    return base::UnguessableToken::Null();
  }

 private:
  ukm::SourceId source_id_;
};

}  // namespace

class WorkerThreadSchedulerWithProxyTest : public testing::Test {
 public:
  WorkerThreadSchedulerWithProxyTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        sequence_manager_(
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr,
                task_environment_.GetMainThreadTaskRunner(),
                task_environment_.GetMockTickClock())) {
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
        sequence_manager_.get(), task_environment_.GetMockTickClock(),
        &timeline_, worker_scheduler_proxy_.get());

    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(5));

    scheduler_->Init();
  }

  ~WorkerThreadSchedulerWithProxyTest() override = default;

  void TearDown() override {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::sequence_manager::SequenceManagerForTest>
      sequence_manager_;
  Vector<String> timeline_;
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

  scheduler_->OnTaskCompleted(nullptr, task, &task_timing, nullptr);

  auto entries = ukm_recorder->GetEntriesByName("RendererSchedulerTask");

  EXPECT_EQ(entries.size(), static_cast<size_t>(1));

  ukm::TestUkmRecorder::ExpectEntryMetric(
      entries[0], "ThreadType", static_cast<int>(ThreadType::kTestThread));
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
