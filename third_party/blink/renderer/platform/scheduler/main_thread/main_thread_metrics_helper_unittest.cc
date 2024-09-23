// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_page_scheduler.h"

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

  std::unique_ptr<FakeFrameScheduler> CreateFakeFrameSchedulerWithType(
      FrameStatus frame_status) {
    FakeFrameScheduler::Builder builder;
    switch (frame_status) {
      case FrameStatus::kNone:
      case FrameStatus::kDetached:
        return nullptr;
      case FrameStatus::kMainFrameVisible:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kMainFrameVisibleService:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetPageScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kMainFrameHidden:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kMainFrameHiddenService:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetPageScheduler(playing_view_.get());
        break;
      case FrameStatus::kMainFrameBackground:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame);
        break;
      case FrameStatus::kMainFrameBackgroundExemptSelf:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kMainFrameBackgroundExemptOther:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetPageScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kSameOriginVisible:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kSameOriginVisibleService:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetPageScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kSameOriginHidden:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kSameOriginHiddenService:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetPageScheduler(playing_view_.get());
        break;
      case FrameStatus::kSameOriginBackground:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe);
        break;
      case FrameStatus::kSameOriginBackgroundExemptSelf:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kSameOriginBackgroundExemptOther:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetPageScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kCrossOriginVisible:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOriginToNearestMainFrame(true)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kCrossOriginVisibleService:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOriginToNearestMainFrame(true)
            .SetPageScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kCrossOriginHidden:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOriginToNearestMainFrame(true)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kCrossOriginHiddenService:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOriginToNearestMainFrame(true)
            .SetPageScheduler(playing_view_.get());
        break;
      case FrameStatus::kCrossOriginBackground:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOriginToNearestMainFrame(true);
        break;
      case FrameStatus::kCrossOriginBackgroundExemptSelf:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOriginToNearestMainFrame(true)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kCrossOriginBackgroundExemptOther:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOriginToNearestMainFrame(true)
            .SetPageScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kCount:
        NOTREACHED_IN_MIGRATION();
        return nullptr;
    }
    return builder.Build();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  raw_ptr<MainThreadMetricsHelper> metrics_helper_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<FakePageScheduler> playing_view_ =
      FakePageScheduler::Builder().SetIsAudioPlaying(true).Build();
  std::unique_ptr<FakePageScheduler> throtting_exempt_view_ =
      FakePageScheduler::Builder().SetIsThrottlingExempt(true).Build();
};

TEST_F(MainThreadMetricsHelperTest, GetFrameStatusTest) {
  DCHECK_EQ(GetFrameStatus(nullptr), FrameStatus::kNone);

  FrameStatus frame_statuses_tested[] = {
      FrameStatus::kMainFrameVisible,
      FrameStatus::kSameOriginHidden,
      FrameStatus::kCrossOriginHidden,
      FrameStatus::kSameOriginBackground,
      FrameStatus::kMainFrameBackgroundExemptSelf,
      FrameStatus::kSameOriginVisibleService,
      FrameStatus::kCrossOriginHiddenService,
      FrameStatus::kMainFrameBackgroundExemptOther};
  for (FrameStatus frame_status : frame_statuses_tested) {
    std::unique_ptr<FakeFrameScheduler> frame =
        CreateFakeFrameSchedulerWithType(frame_status);
    EXPECT_EQ(GetFrameStatus(frame.get()), frame_status);
  }
}

TEST_F(MainThreadMetricsHelperTest, TaskQueueingDelay) {
  base::TimeTicks queue_time = Now();
  base::TimeDelta queue_duration = base::Microseconds(11);
  base::TimeDelta task_duration = base::Microseconds(97);
  RunTask(MainThreadTaskQueue::QueueType::kDefault, queue_time, queue_duration,
          task_duration);
  histogram_tester_->ExpectUniqueSample(
      "RendererScheduler.QueueingDuration.NormalPriority",
      queue_duration.InMicroseconds(), 1);
}

}  // namespace scheduler
}  // namespace blink
