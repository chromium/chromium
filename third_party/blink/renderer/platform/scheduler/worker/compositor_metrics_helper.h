// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_METRICS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_METRICS_HELPER_H_

#include "third_party/blink/renderer/platform/scheduler/common/metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT CompositorMetricsHelper : public MetricsHelper {
 public:
  explicit CompositorMetricsHelper(bool has_cpu_timing_for_each_task);
  ~CompositorMetricsHelper();

  void RecordTaskMetrics(
      NonMainThreadTaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorMetricsHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_COMPOSITOR_METRICS_HELPER_H_
