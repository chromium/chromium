// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_METRICS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_METRICS_HELPER_H_

#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace base {
namespace sequence_manager {
class TaskQueue;
}
}  // namespace base

namespace blink {
namespace scheduler {

constexpr int kUkmMetricVersion = 2;

// Helper class to take care of task metrics shared between main thread
// and worker threads of the renderer process, including per-thread
// task metrics.
//
// Each thread-specific scheduler should have its own subclass of MetricsHelper
// (MainThreadMetricsHelper, WorkerMetricsHelper, etc).
// Note that this is code reuse, not data reuse -- each thread should have its
// own instantiation of this class.
class PLATFORM_EXPORT MetricsHelper {
  DISALLOW_NEW();

 public:
  MetricsHelper(ThreadType thread_type, bool has_cpu_timing_for_each_task);
  MetricsHelper(const MetricsHelper&) = delete;
  MetricsHelper& operator=(const MetricsHelper&) = delete;
  ~MetricsHelper();

 protected:
  bool ShouldDiscardTask(
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

 protected:
  const ThreadType thread_type_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_METRICS_HELPER_H_
