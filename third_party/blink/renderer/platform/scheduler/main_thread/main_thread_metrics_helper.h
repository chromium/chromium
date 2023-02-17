// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_METRICS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_METRICS_HELPER_H_

#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/scheduling_metrics/task_duration_metric_reporter.h"
#include "components/scheduling_metrics/total_duration_metric_reporter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/common/thread_load_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/use_case.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_status.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"

namespace blink {
namespace scheduler {

enum class MainThreadTaskLoadState;
class MainThreadTaskQueue;
class MainThreadSchedulerImpl;

enum class UkmRecordingStatus {
  kSuccess = 0,
  kErrorMissingFrame = 1,
  kErrorDetachedFrame = 2,
  kErrorMissingUkmRecorder = 3,

  kCount = 4,
};

// Helper class to take care of metrics on behalf of MainThreadScheduler.
// This class should be used only on the main thread.
class PLATFORM_EXPORT MainThreadMetricsHelper : public MetricsHelper {
 public:
  MainThreadMetricsHelper(MainThreadSchedulerImpl* main_thread_scheduler,
                          bool has_cpu_timing_for_each_task,
                          base::TimeTicks now,
                          bool renderer_backgrounded);
  MainThreadMetricsHelper(const MainThreadMetricsHelper&) = delete;
  MainThreadMetricsHelper& operator=(const MainThreadMetricsHelper&) = delete;
  ~MainThreadMetricsHelper();

  void RecordTaskMetrics(
      MainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  void OnRendererForegrounded(base::TimeTicks now);
  void OnRendererBackgrounded(base::TimeTicks now);
  void OnRendererShutdown(base::TimeTicks now);

  void RecordMainThreadTaskLoad(base::TimeTicks time, double load);
  void RecordForegroundMainThreadTaskLoad(base::TimeTicks time, double load);
  void RecordBackgroundMainThreadTaskLoad(base::TimeTicks time, double load);

  void ResetForTest(base::TimeTicks now);

 private:
  using TaskDurationPerQueueTypeMetricReporter =
      scheduling_metrics::TaskDurationMetricReporter<
          MainThreadTaskQueue::QueueType>;

  void ReportLowThreadLoadForPageAlmostIdleSignal(int load_percentage);

  MainThreadSchedulerImpl* main_thread_scheduler_;  // NOT OWNED

  // Set to true when OnRendererShutdown is called. Used to ensure that metrics
  // that need to cross IPC boundaries aren't sent, as they cause additional
  // useless tasks to be posted.
  bool renderer_shutting_down_;

  absl::optional<base::TimeTicks> last_reported_task_;

  ThreadLoadTracker main_thread_load_tracker_;
  ThreadLoadTracker background_main_thread_load_tracker_;
  ThreadLoadTracker foreground_main_thread_load_tracker_;

  using TaskDurationPerTaskTypeMetricReporter =
      scheduling_metrics::TaskDurationMetricReporter<TaskType>;

  // The next three reporters are used to report the duration per task type
  // split by renderer scheduler use case (check use_case.h for reference):
  // None, Loading, and User Input (aggregation of multiple input-handling
  // related use cases).
  TaskDurationPerTaskTypeMetricReporter
      no_use_case_per_task_type_duration_reporter_;
  TaskDurationPerTaskTypeMetricReporter
      loading_per_task_type_duration_reporter_;
  TaskDurationPerTaskTypeMetricReporter
      input_handling_per_task_type_duration_reporter_;

  static_assert(static_cast<size_t>(TaskPriority::kPriorityCount) == 7);
  CustomCountHistogram queueing_delay_histograms_[static_cast<size_t>(
      TaskPriority::kPriorityCount)];

  scheduling_metrics::TotalDurationMetricReporter total_task_time_reporter_;

  MainThreadTaskLoadState main_thread_task_load_state_;
  base::MetricsSubSampler metrics_subsampler_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_METRICS_HELPER_H_
