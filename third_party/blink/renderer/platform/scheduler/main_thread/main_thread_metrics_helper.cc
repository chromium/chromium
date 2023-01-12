// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"

#include "base/cpu_reduction_experiment.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/scheduler/web_renderer_process_type.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {
namespace scheduler {

#define MAIN_THREAD_LOAD_METRIC_NAME "RendererScheduler.RendererMainThreadLoad5"
#define EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME \
  MAIN_THREAD_LOAD_METRIC_NAME ".Extension"
#define DURATION_PER_TASK_TYPE_METRIC_NAME \
  "RendererScheduler.TaskDurationPerTaskType2"

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

}  // namespace

MainThreadMetricsHelper::MainThreadMetricsHelper(
    MainThreadSchedulerImpl* main_thread_scheduler,
    bool has_cpu_timing_for_each_task,
    base::TimeTicks now,
    bool renderer_backgrounded)
    : MetricsHelper(ThreadType::kMainThread, has_cpu_timing_for_each_task),
      main_thread_scheduler_(main_thread_scheduler),
      renderer_shutting_down_(false),
      main_thread_load_tracker_(
          now,
          base::BindRepeating(
              &MainThreadMetricsHelper::RecordMainThreadTaskLoad,
              base::Unretained(this)),
          kThreadLoadTrackerReportingInterval),
      background_main_thread_load_tracker_(
          now,
          base::BindRepeating(
              &MainThreadMetricsHelper::RecordBackgroundMainThreadTaskLoad,
              base::Unretained(this)),
          kThreadLoadTrackerReportingInterval),
      foreground_main_thread_load_tracker_(
          now,
          base::BindRepeating(
              &MainThreadMetricsHelper::RecordForegroundMainThreadTaskLoad,
              base::Unretained(this)),
          kThreadLoadTrackerReportingInterval),
      no_use_case_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".UseCaseNone"),
      loading_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".UseCaseLoading"),
      input_handling_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".UseCaseInputHandling"),
      queueing_delay_histograms_{{QUEUEING_DELAY_HISTOGRAM_INIT("Control")},
                                 {QUEUEING_DELAY_HISTOGRAM_INIT("Highest")},
                                 {QUEUEING_DELAY_HISTOGRAM_INIT("VeryHigh")},
                                 {QUEUEING_DELAY_HISTOGRAM_INIT("High")},
                                 {QUEUEING_DELAY_HISTOGRAM_INIT("Normal")},
                                 {QUEUEING_DELAY_HISTOGRAM_INIT("Low")},
                                 {QUEUEING_DELAY_HISTOGRAM_INIT("BestEffort")}},
      total_task_time_reporter_(
          "Scheduler.Experimental.Renderer.TotalTime.Wall.MainThread.Positive",
          "Scheduler.Experimental.Renderer.TotalTime.Wall.MainThread.Negative"),
      main_thread_task_load_state_(MainThreadTaskLoadState::kUnknown) {
  main_thread_load_tracker_.Resume(now);
  if (renderer_backgrounded) {
    background_main_thread_load_tracker_.Resume(now);
  } else {
    foreground_main_thread_load_tracker_.Resume(now);
  }
}

MainThreadMetricsHelper::~MainThreadMetricsHelper() = default;

void MainThreadMetricsHelper::OnRendererForegrounded(base::TimeTicks now) {
  foreground_main_thread_load_tracker_.Resume(now);
  background_main_thread_load_tracker_.Pause(now);
}

void MainThreadMetricsHelper::OnRendererBackgrounded(base::TimeTicks now) {
  foreground_main_thread_load_tracker_.Pause(now);
  background_main_thread_load_tracker_.Resume(now);
}

void MainThreadMetricsHelper::OnRendererShutdown(base::TimeTicks now) {
  renderer_shutting_down_ = true;
  foreground_main_thread_load_tracker_.RecordIdle(now);
  background_main_thread_load_tracker_.RecordIdle(now);
  main_thread_load_tracker_.RecordIdle(now);
}

void MainThreadMetricsHelper::ResetForTest(base::TimeTicks now) {
  main_thread_load_tracker_ = ThreadLoadTracker(
      now,
      base::BindRepeating(&MainThreadMetricsHelper::RecordMainThreadTaskLoad,
                          base::Unretained(this)),
      kThreadLoadTrackerReportingInterval);

  background_main_thread_load_tracker_ = ThreadLoadTracker(
      now,
      base::BindRepeating(
          &MainThreadMetricsHelper::RecordBackgroundMainThreadTaskLoad,
          base::Unretained(this)),
      kThreadLoadTrackerReportingInterval);

  foreground_main_thread_load_tracker_ = ThreadLoadTracker(
      now,
      base::BindRepeating(
          &MainThreadMetricsHelper::RecordForegroundMainThreadTaskLoad,
          base::Unretained(this)),
      kThreadLoadTrackerReportingInterval);
}

void MainThreadMetricsHelper::RecordTaskMetrics(
    MainThreadTaskQueue* queue,
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (ShouldDiscardTask(task, task_timing))
    return;

  // Discard anomalously long idle periods.
  if (last_reported_task_ &&
      task_timing.start_time() - last_reported_task_.value() >
          kLongIdlePeriodDiscardingThreshold) {
    main_thread_load_tracker_.Reset(task_timing.end_time());
    foreground_main_thread_load_tracker_.Reset(task_timing.end_time());
    background_main_thread_load_tracker_.Reset(task_timing.end_time());
    return;
  }

  last_reported_task_ = task_timing.end_time();

  // We want to measure thread time here, but for efficiency reasons
  // we stick with wall time.
  main_thread_load_tracker_.RecordTaskTime(task_timing.start_time(),
                                           task_timing.end_time());
  foreground_main_thread_load_tracker_.RecordTaskTime(task_timing.start_time(),
                                                      task_timing.end_time());
  background_main_thread_load_tracker_.RecordTaskTime(task_timing.start_time(),
                                                      task_timing.end_time());

  // Don't log the metrics to evaluate impact of CPU reduction.
  // This code is deemed not useful anymore (crbug.com/1181870).
  // TODO(crbug.com/1295441: Fully remove the code once the experiment is over.
  if (base::IsRunningCpuReductionExperiment()) {
    return;
  }

  MetricsHelper::RecordCommonTaskMetrics(task, task_timing);

  total_task_time_reporter_.RecordAdditionalDuration(
      task_timing.wall_duration());

  if (queue && base::TimeTicks::IsHighResolution()) {
    base::TimeDelta elapsed =
        task_timing.start_time() - task.GetDesiredExecutionTime();
    queueing_delay_histograms_[queue->GetQueuePriority()].CountMicroseconds(
        elapsed);
  }

  // WARNING: All code below must be compatible with down-sampling.
  constexpr double kSamplingProbability = .01;
  if (!metrics_subsampler_.ShouldSample(kSamplingProbability)) {
    return;
  }

  base::TimeDelta duration = task_timing.wall_duration();
  UMA_HISTOGRAM_CUSTOM_COUNTS("RendererScheduler.TaskTime2",
                              base::saturated_cast<base::HistogramBase::Sample>(
                                  duration.InMicroseconds()),
                              1, 1000 * 1000, 50);

  TaskType task_type = static_cast<TaskType>(task.task_type);
  UseCase use_case =
      main_thread_scheduler_->main_thread_only().current_use_case;
  if (use_case == UseCase::kNone) {
    no_use_case_per_task_type_duration_reporter_.RecordTask(task_type,
                                                            duration);
  } else if (use_case == UseCase::kLoading) {
    loading_per_task_type_duration_reporter_.RecordTask(task_type, duration);
  } else {
    input_handling_per_task_type_duration_reporter_.RecordTask(task_type,
                                                               duration);
  }
}

void MainThreadMetricsHelper::RecordMainThreadTaskLoad(base::TimeTicks time,
                                                       double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  ReportLowThreadLoadForPageAlmostIdleSignal(load_percentage);

  UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME, load_percentage);

  if (main_thread_scheduler_->main_thread_only().process_type ==
      WebRendererProcessType::kExtensionRenderer) {
    UMA_HISTOGRAM_PERCENTAGE(EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME,
                             load_percentage);
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "MainThreadScheduler.RendererMainThreadLoad", load_percentage);
}

void MainThreadMetricsHelper::RecordForegroundMainThreadTaskLoad(
    base::TimeTicks time,
    double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  switch (main_thread_scheduler_->main_thread_only().process_type) {
    case WebRendererProcessType::kExtensionRenderer:
      UMA_HISTOGRAM_PERCENTAGE(EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME
                               ".Foreground",
                               load_percentage);
      break;
    case WebRendererProcessType::kRenderer:
      UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME ".Foreground",
                               load_percentage);

      base::TimeDelta time_since_foregrounded =
          time - main_thread_scheduler_->main_thread_only()
                     .background_status_changed_at;
      if (time_since_foregrounded > base::Minutes(1)) {
        UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME
                                 ".Foreground.AfterFirstMinute",
                                 load_percentage);
      }
      break;
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "MainThreadScheduler.RendererMainThreadLoad.Foreground",
                 load_percentage);
}

void MainThreadMetricsHelper::RecordBackgroundMainThreadTaskLoad(
    base::TimeTicks time,
    double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);

  switch (main_thread_scheduler_->main_thread_only().process_type) {
    case WebRendererProcessType::kExtensionRenderer:
      UMA_HISTOGRAM_PERCENTAGE(EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME
                               ".Background",
                               load_percentage);
      break;
    case WebRendererProcessType::kRenderer:
      UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME ".Background",
                               load_percentage);

      base::TimeDelta time_since_backgrounded =
          time - main_thread_scheduler_->main_thread_only()
                     .background_status_changed_at;
      if (time_since_backgrounded > base::Minutes(1)) {
        UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME
                                 ".Background.AfterFirstMinute",
                                 load_percentage);
      }
      if (time_since_backgrounded > base::Minutes(5)) {
        UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME
                                 ".Background.AfterFifthMinute",
                                 load_percentage);
      }
      if (time_since_backgrounded > base::Minutes(10)) {
        UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME
                                 ".Background.AfterTenthMinute",
                                 load_percentage);
      }
      break;
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "MainThreadScheduler.RendererMainThreadLoad.Background",
                 load_percentage);
}

void MainThreadMetricsHelper::ReportLowThreadLoadForPageAlmostIdleSignal(
    int load_percentage) {
  // Avoid sending IPCs when the renderer is shutting down as this wreaks havoc
  // in test harnesses. These messages aren't needed in production code either
  // as the endpoint receiving them dies shortly after and does nothing with
  // them.
  if (renderer_shutting_down_)
    return;

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

}  // namespace scheduler
}  // namespace blink
