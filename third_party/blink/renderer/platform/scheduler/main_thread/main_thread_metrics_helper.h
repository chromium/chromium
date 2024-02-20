// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_METRICS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_METRICS_HELPER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/time/time.h"
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
  void ReportLowThreadLoadForPageAlmostIdleSignal(int load_percentage);

  raw_ptr<MainThreadSchedulerImpl> main_thread_scheduler_;  // NOT OWNED

  // Set to true when OnRendererShutdown is called. Used to ensure that metrics
  // that need to cross IPC boundaries aren't sent, as they cause additional
  // useless tasks to be posted.
  bool renderer_shutting_down_;

  std::optional<base::TimeTicks> last_reported_task_;

  ThreadLoadTracker main_thread_load_tracker_;
  ThreadLoadTracker background_main_thread_load_tracker_;
  ThreadLoadTracker foreground_main_thread_load_tracker_;

  // When adding a new renderer priority, initialize an entry in the constructor
  // and update histograms.xml.
  static_assert(
      static_cast<size_t>(TaskPriority::kPriorityCount) == 11,
      "Queueing delay histograms must be kept in sync with TaskPriority.");
  CustomCountHistogram queueing_delay_histograms_[static_cast<size_t>(
      TaskPriority::kPriorityCount)];

  MainThreadTaskLoadState main_thread_task_load_state_;
  base::MetricsSubSampler metrics_subsampler_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_METRICS_HELPER_H_
