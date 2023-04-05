// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_METRICS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_METRICS_HELPER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/common/thread_load_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_origin_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT WorkerMetricsHelper : public MetricsHelper {
 public:
  explicit WorkerMetricsHelper(ThreadType thread_type,
                               bool has_cpu_timing_for_each_task);
  WorkerMetricsHelper(const WorkerMetricsHelper&) = delete;
  WorkerMetricsHelper& operator=(const WorkerMetricsHelper&) = delete;
  ~WorkerMetricsHelper();

  void RecordTaskMetrics(
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  void SetParentFrameType(FrameOriginType frame_type);

 private:
  scheduling_metrics::TaskDurationMetricReporter<FrameOriginType>
      dedicated_worker_per_parent_frame_status_duration_reporter_;
  scheduling_metrics::TaskDurationMetricReporter<FrameOriginType>
      background_dedicated_worker_per_parent_frame_status_duration_reporter_;

  absl::optional<FrameOriginType> parent_frame_type_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_METRICS_HELPER_H_
