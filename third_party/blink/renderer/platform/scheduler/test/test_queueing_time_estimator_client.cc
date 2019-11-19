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
