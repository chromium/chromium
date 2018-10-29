// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/test/test_queueing_time_estimator_client.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

// This is a duplicate of the defines in queueing_time_estimator.cc.
#define FRAME_STATUS_PREFIX \
  "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
#define TASK_QUEUE_PREFIX "RendererScheduler.ExpectedQueueingTimeByTaskQueue2."

const char* GetReportingMessageFromQueueType(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    case MainThreadTaskQueue::QueueType::kDefault:
      return TASK_QUEUE_PREFIX "Default";
    case MainThreadTaskQueue::QueueType::kUnthrottled:
      return TASK_QUEUE_PREFIX "Unthrottled";
    case MainThreadTaskQueue::QueueType::kFrameLoading:
      return TASK_QUEUE_PREFIX "FrameLoading";
    case MainThreadTaskQueue::QueueType::kCompositor:
      return TASK_QUEUE_PREFIX "Compositor";
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
      return TASK_QUEUE_PREFIX "FrameThrottleable";
    case MainThreadTaskQueue::QueueType::kFramePausable:
      return TASK_QUEUE_PREFIX "FramePausable";
    case MainThreadTaskQueue::QueueType::kControl:
    case MainThreadTaskQueue::QueueType::kIdle:
    case MainThreadTaskQueue::QueueType::kTest:
    case MainThreadTaskQueue::QueueType::kFrameLoadingControl:
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
    case MainThreadTaskQueue::QueueType::kV8:
    case MainThreadTaskQueue::QueueType::kOther:
    case MainThreadTaskQueue::QueueType::kCount:
    // Using default here as well because there are some values less than COUNT
    // that have been removed and do not correspond to any QueueType.
    default:
      return TASK_QUEUE_PREFIX "Other";
  }
}

const char* GetReportingMessageFromFrameStatus(FrameStatus frame_status) {
  switch (frame_status) {
    case FrameStatus::kMainFrameVisible:
    case FrameStatus::kMainFrameVisibleService:
      return FRAME_STATUS_PREFIX "MainFrameVisible";
    case FrameStatus::kMainFrameHidden:
    case FrameStatus::kMainFrameHiddenService:
      return FRAME_STATUS_PREFIX "MainFrameHidden";
    case FrameStatus::kMainFrameBackground:
    case FrameStatus::kMainFrameBackgroundExemptSelf:
    case FrameStatus::kMainFrameBackgroundExemptOther:
      return FRAME_STATUS_PREFIX "MainFrameBackground";
    case FrameStatus::kSameOriginVisible:
    case FrameStatus::kSameOriginVisibleService:
      return FRAME_STATUS_PREFIX "SameOriginVisible";
    case FrameStatus::kSameOriginHidden:
    case FrameStatus::kSameOriginHiddenService:
      return FRAME_STATUS_PREFIX "SameOriginHidden";
    case FrameStatus::kSameOriginBackground:
    case FrameStatus::kSameOriginBackgroundExemptSelf:
    case FrameStatus::kSameOriginBackgroundExemptOther:
      return FRAME_STATUS_PREFIX "SameOriginBackground";
    case FrameStatus::kCrossOriginVisible:
    case FrameStatus::kCrossOriginVisibleService:
      return FRAME_STATUS_PREFIX "CrossOriginVisible";
    case FrameStatus::kCrossOriginHidden:
    case FrameStatus::kCrossOriginHiddenService:
      return FRAME_STATUS_PREFIX "CrossOriginHidden";
    case FrameStatus::kCrossOriginBackground:
    case FrameStatus::kCrossOriginBackgroundExemptSelf:
    case FrameStatus::kCrossOriginBackgroundExemptOther:
      return FRAME_STATUS_PREFIX "CrossOriginBackground";
    case FrameStatus::kNone:
    case FrameStatus::kDetached:
      return FRAME_STATUS_PREFIX "Other";
    case FrameStatus::kCount:
      NOTREACHED();
      return "";
  }
  NOTREACHED();
  return "";
}

void TestQueueingTimeEstimatorClient::OnQueueingTimeForWindowEstimated(
    base::TimeDelta queueing_time,
    bool is_disjoint_window) {
  expected_queueing_times_.push_back(queueing_time);
  // Mimic RendererSchedulerImpl::OnQueueingTimeForWindowEstimated.
  if (is_disjoint_window) {
    UMA_HISTOGRAM_TIMES("RendererScheduler.ExpectedTaskQueueingDuration",
                        queueing_time);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "RendererScheduler.ExpectedTaskQueueingDuration3",
        base::saturated_cast<base::HistogramBase::Sample>(
            queueing_time.InMicroseconds()),
        MainThreadSchedulerImpl::kMinExpectedQueueingTimeBucket,
        MainThreadSchedulerImpl::kMaxExpectedQueueingTimeBucket,
        MainThreadSchedulerImpl::kNumberExpectedQueueingTimeBuckets);
  }
}

void TestQueueingTimeEstimatorClient::OnReportFineGrainedExpectedQueueingTime(
    const char* split_description,
    base::TimeDelta queueing_time) {
  if (split_eqts_.find(split_description) == split_eqts_.end())
    split_eqts_[split_description] = std::vector<base::TimeDelta>();
  split_eqts_[split_description].push_back(queueing_time);
  // Mimic MainThreadSchedulerImpl::OnReportFineGrainedExpectedQueueingTime.
  base::UmaHistogramCustomCounts(
      split_description,
      base::saturated_cast<base::HistogramBase::Sample>(
          queueing_time.InMicroseconds()),
      MainThreadSchedulerImpl::kMinExpectedQueueingTimeBucket,
      MainThreadSchedulerImpl::kMaxExpectedQueueingTimeBucket,
      MainThreadSchedulerImpl::kNumberExpectedQueueingTimeBuckets);
}

const std::vector<base::TimeDelta>&
TestQueueingTimeEstimatorClient::QueueTypeValues(QueueType queue_type) {
  return split_eqts_[GetReportingMessageFromQueueType(queue_type)];
}

const std::vector<base::TimeDelta>&
TestQueueingTimeEstimatorClient::FrameStatusValues(FrameStatus frame_status) {
  return split_eqts_[GetReportingMessageFromFrameStatus(frame_status)];
}

QueueingTimeEstimatorForTest::QueueingTimeEstimatorForTest(
    TestQueueingTimeEstimatorClient* client,
    base::TimeDelta window_duration,
    int steps_per_window,
    base::TimeTicks time)
    : QueueingTimeEstimator(client,
                            window_duration,
                            steps_per_window,
                            kLaunchingProcessIsBackgrounded) {
  // If initial state is disabled, enable the estimator.
  if (kLaunchingProcessIsBackgrounded) {
    this->OnRecordingStateChanged(false, time);
  }
}

}  // namespace scheduler
}  // namespace blink
