// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink::scheduler {

#define QUEUEING_DELAY_HISTOGRAM_INIT(name)                       \
  "RendererScheduler.QueueingDuration." name "Priority",          \
      kTimeBasedHistogramMinSample, kTimeBasedHistogramMaxSample, \
      kTimeBasedHistogramBucketCount

enum class MainThreadTaskLoadState { kLow, kHigh, kUnknown };

namespace {

constexpr base::TimeDelta kThreadLoadTrackerReportingInterval =
    base::Seconds(1);
constexpr base::TimeDelta kLongIdlePeriodDiscardingThreshold = base::Minutes(3);

// Main thread load percentage that is considered low.
constexpr int kMainThreadTaskLoadLowPercentage = 25;

// Threshold for discarding ultra-long tasks. It is assumed that ultra-long
// tasks are reporting glitches (e.g. system falling asleep on the middle of the
// task).
constexpr base::TimeDelta kLongTaskDiscardingThreshold = base::Seconds(30);

bool ShouldDiscardTask(
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  // TODO(altimin): Investigate the relationship between thread time and
  // wall time for discarded tasks.
  using State = base::sequence_manager::TaskQueue::TaskTiming::State;
  return task_timing.state() == State::Finished &&
         task_timing.wall_duration() > kLongTaskDiscardingThreshold;
}

}  // namespace

MainThreadMetricsHelper::MainThreadMetricsHelper(base::TimeTicks now,
                                                 bool in_background)
    : renderer_shutting_down_(false),
      main_thread_load_tracker_(
          now,
          base::BindRepeating(
              &MainThreadMetricsHelper::RecordMainThreadTaskLoad,
              base::Unretained(this)),
          kThreadLoadTrackerReportingInterval),
      // Order here must match TaskPriority (in descending priority order).
      queueing_delay_histograms_{
          {QUEUEING_DELAY_HISTOGRAM_INIT("Control")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("Highest")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("ExtremelyHigh")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("VeryHigh")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("HighContinuation")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("High")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("NormalContinuation")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("Normal")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("LowContinuation")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("Low")},
          {QUEUEING_DELAY_HISTOGRAM_INIT("BestEffort")}},
      main_thread_task_load_state_(MainThreadTaskLoadState::kUnknown) {
  if (!in_background) {
    last_foregrounded_time_ = now;
  }
  main_thread_load_tracker_.Resume(now);
}

MainThreadMetricsHelper::~MainThreadMetricsHelper() = default;

void MainThreadMetricsHelper::OnRendererShutdown(base::TimeTicks now) {
  renderer_shutting_down_ = true;
  main_thread_load_tracker_.RecordIdle(now);
}

void MainThreadMetricsHelper::ResetForTest(base::TimeTicks now) {
  main_thread_load_tracker_ = ThreadLoadTracker(
      now,
      base::BindRepeating(&MainThreadMetricsHelper::RecordMainThreadTaskLoad,
                          base::Unretained(this)),
      kThreadLoadTrackerReportingInterval);
}

void MainThreadMetricsHelper::DisableMetricsSubsamplingForTesting() {
  sampling_ratio_ = 1.;
}

void MainThreadMetricsHelper::RecordTaskMetrics(
    MainThreadTaskQueue* queue,
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (ShouldDiscardTask(task_timing)) {
    return;
  }

  // Discard anomalously long idle periods.
  if (last_reported_task_ &&
      task_timing.start_time() - last_reported_task_.value() >
          kLongIdlePeriodDiscardingThreshold) {
    main_thread_load_tracker_.Reset(task_timing.end_time());
    return;
  }

  last_reported_task_ = task_timing.end_time();

  // We want to measure thread time here, but for efficiency reasons
  // we stick with wall time.
  main_thread_load_tracker_.RecordTaskTime(task_timing.start_time(),
                                           task_timing.end_time());

  if (queue && base::TimeTicks::IsHighResolution() &&
      metrics_subsampler_.ShouldSample(sampling_ratio_)) {
    base::TimeDelta elapsed =
        task_timing.start_time() - task.GetDesiredExecutionTime();
    UNSAFE_TODO(queueing_delay_histograms_[static_cast<size_t>(
                                               queue->GetQueuePriority())]
                    .CountMicroseconds(elapsed));
  }
}

void MainThreadMetricsHelper::SetRendererBackgrounded(bool backgrounded,
                                                      base::TimeTicks now) {
  last_foregrounded_time_ = backgrounded ? base::TimeTicks() : now;
}

void MainThreadMetricsHelper::RecordMainThreadTaskLoad(base::TimeTicks time,
                                                       double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  ReportLowThreadLoadForPageAlmostIdleSignal(load_percentage);

  base::UmaHistogramPercentage("RendererScheduler.RendererMainThreadLoad6",
                               load_percentage);
  // This may discard data points if the renderer is no longer foregrounded, and
  // we are reporting in the past.
  if (!last_foregrounded_time_.is_null() &&
      (time - last_foregrounded_time_) >= kThreadLoadTrackerReportingInterval) {
    base::UmaHistogramPercentage(
        "RendererScheduler.RendererMainThreadLoad6.Foreground",
        load_percentage);
  }
}

void MainThreadMetricsHelper::ReportLowThreadLoadForPageAlmostIdleSignal(
    int load_percentage) {
  // Avoid sending IPCs when the renderer is shutting down as this wreaks havoc
  // in test harnesses. These messages aren't needed in production code either
  // as the endpoint receiving them dies shortly after and does nothing with
  // them.
  if (renderer_shutting_down_) {
    return;
  }

  if (auto* renderer_resource_coordinator =
          RendererResourceCoordinator::Get()) {
    // Avoid sending duplicate IPCs when the state doesn't change.
    if (load_percentage <= kMainThreadTaskLoadLowPercentage &&
        main_thread_task_load_state_ != MainThreadTaskLoadState::kLow) {
      renderer_resource_coordinator->SetMainThreadTaskLoadIsLow(true);
      main_thread_task_load_state_ = MainThreadTaskLoadState::kLow;
    } else if (load_percentage > kMainThreadTaskLoadLowPercentage &&
               main_thread_task_load_state_ != MainThreadTaskLoadState::kHigh) {
      renderer_resource_coordinator->SetMainThreadTaskLoadIsLow(false);
      main_thread_task_load_state_ = MainThreadTaskLoadState::kHigh;
    }
  }
}

}  // namespace blink::scheduler
