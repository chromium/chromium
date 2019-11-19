// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/scheduler/web_renderer_process_type.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {
namespace scheduler {

#define DURATION_PER_QUEUE_TYPE_METRIC_NAME \
  "RendererScheduler.TaskDurationPerQueueType3"
#define COUNT_PER_QUEUE_TYPE_METRIC_NAME \
  "RendererScheduler.TaskCountPerQueueType"
#define MAIN_THREAD_LOAD_METRIC_NAME "RendererScheduler.RendererMainThreadLoad5"
#define EXTENSIONS_MAIN_THREAD_LOAD_METRIC_NAME \
  MAIN_THREAD_LOAD_METRIC_NAME ".Extension"
#define DURATION_PER_FRAME_TYPE_METRIC_NAME \
  "RendererScheduler.TaskDurationPerFrameType3"
#define DURATION_PER_TASK_TYPE_METRIC_NAME \
  "RendererScheduler.TaskDurationPerTaskType2"
#define COUNT_PER_FRAME_METRIC_NAME "RendererScheduler.TaskCountPerFrameType"
#define COUNT_PER_FRAME_METRIC_NAME_WITH_SAFEPOINT \
  "RendererScheduler.TaskCountPerFrameType.HasSafePoint"
#define DURATION_PER_TASK_USE_CASE_NAME \
  "RendererScheduler.TaskDurationPerUseCase2"
#define QUEUEING_TIME_PER_QUEUE_TYPE_METRIC_NAME \
  "RendererScheduler.QueueingDurationPerQueueType"

// Same as UMA_HISTOGRAM_TIMES but for a broader view of this metric we end
// at 1 minute instead of 10 seconds.
#define QUEUEING_TIME_HISTOGRAM(name, sample)                               \
  UMA_HISTOGRAM_CUSTOM_TIMES(QUEUEING_TIME_PER_QUEUE_TYPE_METRIC_NAME name, \
                             sample, base::TimeDelta::FromMilliseconds(1),  \
                             base::TimeDelta::FromMinutes(1), 50)

enum class MainThreadTaskLoadState { kLow, kHigh, kUnknown };

namespace {

constexpr base::TimeDelta kThreadLoadTrackerReportingInterval =
    base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kLongIdlePeriodDiscardingThreshold =
    base::TimeDelta::FromMinutes(3);

// Main thread load percentage that is considered low.
constexpr int kMainThreadTaskLoadLowPercentage = 25;

}  // namespace

MainThreadMetricsHelper::PerQueueTypeDurationReporters::
    PerQueueTypeDurationReporters()
    : overall(DURATION_PER_QUEUE_TYPE_METRIC_NAME),
      foreground(DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Foreground"),
      foreground_first_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                              ".Foreground.FirstMinute"),
      foreground_second_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                               ".Foreground.SecondMinute"),
      foreground_third_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                              ".Foreground.ThirdMinute"),
      foreground_after_third_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                                    ".Foreground.AfterThirdMinute"),
      background(DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Background"),
      background_first_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                              ".Background.FirstMinute"),
      background_second_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                               ".Background.SecondMinute"),
      background_third_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                              ".Background.ThirdMinute"),
      background_fourth_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                               ".Background.FourthMinute"),
      background_fifth_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                              ".Background.FifthMinute"),
      background_after_fifth_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                                    ".Background.AfterFifthMinute"),
      background_after_tenth_minute(DURATION_PER_QUEUE_TYPE_METRIC_NAME
                                    ".Background.AfterTenthMinute"),
      background_keep_active_after_fifth_minute(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME
          ".Background.KeepAlive.AfterFifthMinute"),
      background_keep_active_after_tenth_minute(
          DURATION_PER_QUEUE_TYPE_METRIC_NAME
          ".Background.KeepAlive.AfterTenthMinute"),
      hidden(DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Hidden"),
      visible(DURATION_PER_QUEUE_TYPE_METRIC_NAME ".Visible"),
      hidden_music(DURATION_PER_QUEUE_TYPE_METRIC_NAME ".HiddenMusic") {}

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
      per_frame_status_duration_reporter_(DURATION_PER_FRAME_TYPE_METRIC_NAME),
      per_task_type_duration_reporter_(DURATION_PER_TASK_TYPE_METRIC_NAME),
      no_use_case_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".UseCaseNone"),
      loading_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".UseCaseLoading"),
      input_handling_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".UseCaseInputHandling"),
      foreground_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".Foreground"),
      background_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".Background"),
      background_after_fifth_minute_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".Background.AfterFifthMinute"),
      background_after_tenth_minute_per_task_type_duration_reporter_(
          DURATION_PER_TASK_TYPE_METRIC_NAME ".Background.AfterTenthMinute"),
      per_task_use_case_duration_reporter_(DURATION_PER_TASK_USE_CASE_NAME),
      total_task_time_reporter_(
          "Scheduler.Experimental.Renderer.TotalTime.Wall.MainThread.Positive",
          "Scheduler.Experimental.Renderer.TotalTime.Wall.MainThread.Negative"),
      main_thread_task_load_state_(MainThreadTaskLoadState::kUnknown),
      current_task_slice_start_time_(now),
      safepoints_in_current_toplevel_task_count_(0) {
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

namespace {

// Calculates the length of the intersection of two given time intervals.
base::TimeDelta DurationOfIntervalOverlap(base::TimeTicks start1,
                                          base::TimeTicks end1,
                                          base::TimeTicks start2,
                                          base::TimeTicks end2) {
  DCHECK_LE(start1, end1);
  DCHECK_LE(start2, end2);
  return std::max(std::min(end1, end2) - std::max(start1, start2),
                  base::TimeDelta());
}

}  // namespace

void MainThreadMetricsHelper::OnSafepointEntered(base::TimeTicks now) {
  current_task_slice_start_time_ = now;
}

void MainThreadMetricsHelper::OnSafepointExited(base::TimeTicks now) {
  safepoints_in_current_toplevel_task_count_++;
  RecordTaskSliceMetrics(now);
}

void MainThreadMetricsHelper::RecordTaskSliceMetrics(base::TimeTicks now) {
  UMA_HISTOGRAM_TIMES("RendererScheduler.TasksWithSafepoints.TaskSliceTime",
                      now - current_task_slice_start_time_);
}

void MainThreadMetricsHelper::RecordMetricsForTasksWithSafepoints(
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (safepoints_in_current_toplevel_task_count_ == 0)
    return;

  RecordTaskSliceMetrics(task_timing.end_time());

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "RendererScheduler.TasksWithSafepoints.TaskTime",
      task_timing.wall_duration(), base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromSeconds(1), 50);
  UMA_HISTOGRAM_COUNTS_100(
      "RendererScheduler.TasksWithSafepoints.SafepointCount",
      safepoints_in_current_toplevel_task_count_);
  safepoints_in_current_toplevel_task_count_ = 0;
}

void MainThreadMetricsHelper::RecordTaskMetrics(
    MainThreadTaskQueue* queue,
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (ShouldDiscardTask(queue, task, task_timing))
    return;

  MetricsHelper::RecordCommonTaskMetrics(queue, task, task_timing);

  total_task_time_reporter_.RecordAdditionalDuration(
      task_timing.wall_duration());

  MainThreadTaskQueue::QueueType queue_type =
      queue ? queue->queue_type() : MainThreadTaskQueue::QueueType::kDetached;
  base::TimeDelta duration = task_timing.wall_duration();

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

  UMA_HISTOGRAM_CUSTOM_COUNTS("RendererScheduler.TaskTime2",
                              base::saturated_cast<base::HistogramBase::Sample>(
                                  duration.InMicroseconds()),
                              1, 1000 * 1000, 50);

  // We want to measure thread time here, but for efficiency reasons
  // we stick with wall time.
  main_thread_load_tracker_.RecordTaskTime(task_timing.start_time(),
                                           task_timing.end_time());
  foreground_main_thread_load_tracker_.RecordTaskTime(task_timing.start_time(),
                                                      task_timing.end_time());
  background_main_thread_load_tracker_.RecordTaskTime(task_timing.start_time(),
                                                      task_timing.end_time());

  UMA_HISTOGRAM_ENUMERATION(COUNT_PER_QUEUE_TYPE_METRIC_NAME, queue_type,
                            MainThreadTaskQueue::QueueType::kCount);

  if (duration >= base::TimeDelta::FromMilliseconds(16)) {
    UMA_HISTOGRAM_ENUMERATION(
        COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan16ms", queue_type,
        MainThreadTaskQueue::QueueType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(50)) {
    UMA_HISTOGRAM_ENUMERATION(
        COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan50ms", queue_type,
        MainThreadTaskQueue::QueueType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(100)) {
    UMA_HISTOGRAM_ENUMERATION(
        COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan100ms", queue_type,
        MainThreadTaskQueue::QueueType::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(150)) {
    UMA_HISTOGRAM_ENUMERATION(
        COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan150ms", queue_type,
        MainThreadTaskQueue::QueueType::kCount);
  }

  if (duration >= base::TimeDelta::FromSeconds(1)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_QUEUE_TYPE_METRIC_NAME ".LongerThan1s",
                              queue_type,
                              MainThreadTaskQueue::QueueType::kCount);
  }

  per_queue_type_reporters_.overall.RecordTask(queue_type, duration);

  TaskType task_type = static_cast<TaskType>(task.task_type);
  per_task_type_duration_reporter_.RecordTask(task_type, duration);

  if (main_thread_scheduler_->main_thread_only().renderer_backgrounded) {
    per_queue_type_reporters_.background.RecordTask(queue_type, duration);

    // Collect detailed breakdown for first five minutes given that we stop
    // timers on mobile after five minutes.
    base::TimeTicks backgrounded_at =
        main_thread_scheduler_->main_thread_only().background_status_changed_at;

    per_queue_type_reporters_.background_first_minute.RecordTask(
        queue_type,
        DurationOfIntervalOverlap(
            task_timing.start_time(), task_timing.end_time(), backgrounded_at,
            backgrounded_at + base::TimeDelta::FromMinutes(1)));

    per_queue_type_reporters_.background_second_minute.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        task_timing.start_time(), task_timing.end_time(),
                        backgrounded_at + base::TimeDelta::FromMinutes(1),
                        backgrounded_at + base::TimeDelta::FromMinutes(2)));

    per_queue_type_reporters_.background_third_minute.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        task_timing.start_time(), task_timing.end_time(),
                        backgrounded_at + base::TimeDelta::FromMinutes(2),
                        backgrounded_at + base::TimeDelta::FromMinutes(3)));

    per_queue_type_reporters_.background_fourth_minute.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        task_timing.start_time(), task_timing.end_time(),
                        backgrounded_at + base::TimeDelta::FromMinutes(3),
                        backgrounded_at + base::TimeDelta::FromMinutes(4)));

    per_queue_type_reporters_.background_fifth_minute.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        task_timing.start_time(), task_timing.end_time(),
                        backgrounded_at + base::TimeDelta::FromMinutes(4),
                        backgrounded_at + base::TimeDelta::FromMinutes(5)));

    per_queue_type_reporters_.background_after_fifth_minute.RecordTask(
        queue_type,
        DurationOfIntervalOverlap(
            task_timing.start_time(), task_timing.end_time(),
            backgrounded_at + base::TimeDelta::FromMinutes(5),
            std::max(backgrounded_at + base::TimeDelta::FromMinutes(5),
                     task_timing.end_time())));

    per_queue_type_reporters_.background_after_tenth_minute.RecordTask(
        queue_type,
        DurationOfIntervalOverlap(
            task_timing.start_time(), task_timing.end_time(),
            backgrounded_at + base::TimeDelta::FromMinutes(10),
            std::max(backgrounded_at + base::TimeDelta::FromMinutes(10),
                     task_timing.end_time())));

    if (main_thread_scheduler_->main_thread_only()
            .keep_active_fetch_or_worker) {
      per_queue_type_reporters_.background_keep_active_after_fifth_minute
          .RecordTask(
              queue_type,
              DurationOfIntervalOverlap(
                  task_timing.start_time(), task_timing.end_time(),
                  backgrounded_at + base::TimeDelta::FromMinutes(5),
                  std::max(backgrounded_at + base::TimeDelta::FromMinutes(5),
                           task_timing.end_time())));
      per_queue_type_reporters_.background_keep_active_after_tenth_minute
          .RecordTask(
              queue_type,
              DurationOfIntervalOverlap(
                  task_timing.start_time(), task_timing.end_time(),
                  backgrounded_at + base::TimeDelta::FromMinutes(10),
                  std::max(backgrounded_at + base::TimeDelta::FromMinutes(10),
                           task_timing.end_time())));
    }

    background_per_task_type_duration_reporter_.RecordTask(task_type, duration);

    background_after_fifth_minute_per_task_type_duration_reporter_.RecordTask(
        task_type,
        DurationOfIntervalOverlap(
            task_timing.start_time(), task_timing.end_time(),
            backgrounded_at + base::TimeDelta::FromMinutes(5),
            std::max(backgrounded_at + base::TimeDelta::FromMinutes(5),
                     task_timing.end_time())));
    background_after_tenth_minute_per_task_type_duration_reporter_.RecordTask(
        task_type,
        DurationOfIntervalOverlap(
            task_timing.start_time(), task_timing.end_time(),
            backgrounded_at + base::TimeDelta::FromMinutes(10),
            std::max(backgrounded_at + base::TimeDelta::FromMinutes(10),
                     task_timing.end_time())));
  } else {
    per_queue_type_reporters_.foreground.RecordTask(queue_type, duration);

    // For foreground tabs we do not expect such a notable difference as it is
    // the case with background tabs, so we limit breakdown to three minutes.
    base::TimeTicks foregrounded_at =
        main_thread_scheduler_->main_thread_only().background_status_changed_at;

    per_queue_type_reporters_.foreground_first_minute.RecordTask(
        queue_type,
        DurationOfIntervalOverlap(
            task_timing.start_time(), task_timing.end_time(), foregrounded_at,
            foregrounded_at + base::TimeDelta::FromMinutes(1)));

    per_queue_type_reporters_.foreground_second_minute.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        task_timing.start_time(), task_timing.end_time(),
                        foregrounded_at + base::TimeDelta::FromMinutes(1),
                        foregrounded_at + base::TimeDelta::FromMinutes(2)));

    per_queue_type_reporters_.foreground_third_minute.RecordTask(
        queue_type, DurationOfIntervalOverlap(
                        task_timing.start_time(), task_timing.end_time(),
                        foregrounded_at + base::TimeDelta::FromMinutes(2),
                        foregrounded_at + base::TimeDelta::FromMinutes(3)));

    per_queue_type_reporters_.foreground_after_third_minute.RecordTask(
        queue_type,
        DurationOfIntervalOverlap(
            task_timing.start_time(), task_timing.end_time(),
            foregrounded_at + base::TimeDelta::FromMinutes(3),
            std::max(foregrounded_at + base::TimeDelta::FromMinutes(3),
                     task_timing.end_time())));

    foreground_per_task_type_duration_reporter_.RecordTask(task_type, duration);

    if (!task.queue_time.is_null()) {
      switch (queue_type) {
        case MainThreadTaskQueue::QueueType::kCompositor: {
          QUEUEING_TIME_HISTOGRAM(".Compositor",
                                  task_timing.start_time() - task.queue_time);
          break;
        }
        case MainThreadTaskQueue::QueueType::kInput: {
          QUEUEING_TIME_HISTOGRAM(".Input",
                                  task_timing.start_time() - task.queue_time);
          break;
        }
        case MainThreadTaskQueue::QueueType::kFrameLoading:
        case MainThreadTaskQueue::QueueType::kFrameLoadingControl: {
          QUEUEING_TIME_HISTOGRAM(".Loading",
                                  task_timing.start_time() - task.queue_time);
          break;
        }
        default: {
          QUEUEING_TIME_HISTOGRAM(".Other",
                                  task_timing.start_time() - task.queue_time);
        }
      }
    }
  }

  if (main_thread_scheduler_->main_thread_only().renderer_hidden) {
    per_queue_type_reporters_.hidden.RecordTask(queue_type, duration);

    if (main_thread_scheduler_->IsAudioPlaying()) {
      per_queue_type_reporters_.hidden_music.RecordTask(queue_type, duration);
    }
  } else {
    per_queue_type_reporters_.visible.RecordTask(queue_type, duration);
  }

  FrameStatus frame_status =
      GetFrameStatus(queue ? queue->GetFrameScheduler() : nullptr);
  per_frame_status_duration_reporter_.RecordTask(frame_status, duration);
  UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME, frame_status,
                            FrameStatus::kCount);
  if (duration >= base::TimeDelta::FromMilliseconds(16)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan16ms",
                              frame_status, FrameStatus::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(50)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan50ms",
                              frame_status, FrameStatus::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(100)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan100ms",
                              frame_status, FrameStatus::kCount);
  }

  if (duration >= base::TimeDelta::FromMilliseconds(150)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan150ms",
                              frame_status, FrameStatus::kCount);
  }

  if (duration >= base::TimeDelta::FromSeconds(1)) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME ".LongerThan1s",
                              frame_status, FrameStatus::kCount);
  }

  if (safepoints_in_current_toplevel_task_count_ > 0) {
    UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME_WITH_SAFEPOINT,
                              frame_status, FrameStatus::kCount);
    if (duration >= base::TimeDelta::FromMilliseconds(16)) {
      UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME_WITH_SAFEPOINT
                                ".LongerThan16ms",
                                frame_status, FrameStatus::kCount);
    }

    if (duration >= base::TimeDelta::FromMilliseconds(50)) {
      UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME_WITH_SAFEPOINT
                                ".LongerThan50ms",
                                frame_status, FrameStatus::kCount);
    }

    if (duration >= base::TimeDelta::FromMilliseconds(100)) {
      UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME_WITH_SAFEPOINT
                                ".LongerThan100ms",
                                frame_status, FrameStatus::kCount);
    }

    if (duration >= base::TimeDelta::FromMilliseconds(150)) {
      UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME_WITH_SAFEPOINT
                                ".LongerThan150ms",
                                frame_status, FrameStatus::kCount);
    }

    if (duration >= base::TimeDelta::FromSeconds(1)) {
      UMA_HISTOGRAM_ENUMERATION(COUNT_PER_FRAME_METRIC_NAME_WITH_SAFEPOINT
                                ".LongerThan1s",
                                frame_status, FrameStatus::kCount);
    }
    RecordMetricsForTasksWithSafepoints(task_timing);
  }

  UseCase use_case =
      main_thread_scheduler_->main_thread_only().current_use_case;
  per_task_use_case_duration_reporter_.RecordTask(use_case, duration);
  if (use_case == UseCase::kNone) {
    no_use_case_per_task_type_duration_reporter_.RecordTask(task_type,
                                                            duration);
  } else if (use_case == UseCase::kLoading) {
    loading_per_task_type_duration_reporter_.RecordTask(task_type, duration);
  } else {
    input_handling_per_task_type_duration_reporter_.RecordTask(task_type,
                                                               duration);
  }

  if (task_type == TaskType::kNetworkingWithURLLoaderAnnotation && queue) {
    if (queue->net_request_priority()) {
      UMA_HISTOGRAM_ENUMERATION(
          "RendererScheduler.ResourceLoadingTaskCountPerNetPriority",
          queue->net_request_priority().value(),
          net::RequestPriority::MAXIMUM_PRIORITY + 1);
    }

    UMA_HISTOGRAM_ENUMERATION(
        "RendererScheduler.ResourceLoadingTaskCountPerPriority",
        queue->GetQueuePriority(),
        base::sequence_manager::TaskQueue::QueuePriority::kQueuePriorityCount);
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
      if (time_since_foregrounded > base::TimeDelta::FromMinutes(1)) {
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
      if (time_since_backgrounded > base::TimeDelta::FromMinutes(1)) {
        UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME
                                 ".Background.AfterFirstMinute",
                                 load_percentage);
      }
      if (time_since_backgrounded > base::TimeDelta::FromMinutes(5)) {
        UMA_HISTOGRAM_PERCENTAGE(MAIN_THREAD_LOAD_METRIC_NAME
                                 ".Background.AfterFifthMinute",
                                 load_percentage);
      }
      if (time_since_backgrounded > base::TimeDelta::FromMinutes(10)) {
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
