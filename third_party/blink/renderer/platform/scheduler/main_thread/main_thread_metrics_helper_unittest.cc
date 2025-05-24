// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

using base::sequence_manager::TaskQueue;
using base::sequence_manager::FakeTask;
using base::sequence_manager::FakeTaskTiming;

namespace blink {
namespace scheduler {

using QueueType = MainThreadTaskQueue::QueueType;
using base::Bucket;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

class MainThreadMetricsHelperTest : public testing::Test {
 public:
  MainThreadMetricsHelperTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
  MainThreadMetricsHelperTest(const MainThreadMetricsHelperTest&) = delete;
  MainThreadMetricsHelperTest& operator=(const MainThreadMetricsHelperTest&) =
      delete;

  ~MainThreadMetricsHelperTest() override = default;

  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    auto settings = base::sequence_manager::SequenceManager::Settings::Builder()
                        .SetPrioritySettings(CreatePrioritySettings())
                        .Build();
    scheduler_ = std::make_unique<MainThreadSchedulerImpl>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMockTickClock(), std::move(settings)));
    metrics_helper_ = &scheduler_->main_thread_only().metrics_helper;
  }

  void TearDown() override {
    metrics_helper_ = nullptr;
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  base::TimeTicks Now() {
    return task_environment_.GetMockTickClock()->NowTicks();
  }

  void FastForwardTo(base::TimeTicks time) {
    CHECK_LE(Now(), time);
    task_environment_.FastForwardBy(time - Now());
  }

  void RunTask(MainThreadTaskQueue::QueueType queue_type,
               base::TimeTicks queue_time,
               base::TimeDelta queue_duration,
               base::TimeDelta task_duration) {
    base::TimeTicks start_time = queue_time + queue_duration;
    base::TimeTicks end_time = start_time + task_duration;
    FastForwardTo(end_time);
    scoped_refptr<MainThreadTaskQueue> queue;
    if (queue_type != MainThreadTaskQueue::QueueType::kDetached) {
      queue = scheduler_->GetHelper().NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(queue_type));
    }

    FakeTask task;
    task.queue_time = queue_time;
    metrics_helper_->RecordTaskMetrics(queue.get(), task,
                                       FakeTaskTiming(start_time, end_time));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  raw_ptr<MainThreadMetricsHelper> metrics_helper_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(MainThreadMetricsHelperTest, TaskQueueingDelay) {
  metrics_helper_->DisableMetricsSubsamplingForTesting();
  base::TimeTicks queue_time = Now();
  base::TimeDelta queue_duration = base::Microseconds(11);
  base::TimeDelta task_duration = base::Microseconds(97);
  RunTask(MainThreadTaskQueue::QueueType::kDefault, queue_time, queue_duration,
          task_duration);
  histogram_tester_->ExpectUniqueSample(
      "RendererScheduler.QueueingDuration.NormalPriority",
      queue_duration.InMicroseconds(), 1);
}

TEST_F(MainThreadMetricsHelperTest, MainThreadLoad) {
  base::TimeTicks now = Now();
  base::TimeTicks queue_time = now;
  base::TimeDelta queue_duration = base::Milliseconds(11);
  metrics_helper_->SetRendererBackgrounded(true, now);
  RunTask(MainThreadTaskQueue::QueueType::kDefault, queue_time, queue_duration,
          base::Milliseconds(1500));
  histogram_tester_->ExpectTotalCount(
      "RendererScheduler.RendererMainThreadLoad6", 1);
  // Not foreground.
  histogram_tester_->ExpectTotalCount(
      "RendererScheduler.RendererMainThreadLoad6.Foreground", 0);

  histogram_tester_ = std::make_unique<base::HistogramTester>();
  now = Now();
  queue_time = now;
  metrics_helper_->SetRendererBackgrounded(false, now);
  task_environment_.AdvanceClock(base::Milliseconds(10));
  RunTask(MainThreadTaskQueue::QueueType::kDefault, queue_time, queue_duration,
          base::Milliseconds(1500));
  histogram_tester_->ExpectTotalCount(
      "RendererScheduler.RendererMainThreadLoad6", 2);
  histogram_tester_->ExpectTotalCount(
      "RendererScheduler.RendererMainThreadLoad6.Foreground", 1);

  histogram_tester_ = std::make_unique<base::HistogramTester>();
  now = Now();
  queue_time = now;
  metrics_helper_->SetRendererBackgrounded(true, now);
  task_environment_.AdvanceClock(base::Milliseconds(10));
  RunTask(MainThreadTaskQueue::QueueType::kDefault, queue_time, queue_duration,
          base::Milliseconds(1500));
  histogram_tester_->ExpectTotalCount(
      "RendererScheduler.RendererMainThreadLoad6", 1);
  histogram_tester_->ExpectTotalCount(
      "RendererScheduler.RendererMainThreadLoad6.Foreground", 0);
}

}  // namespace scheduler
}  // namespace blink
