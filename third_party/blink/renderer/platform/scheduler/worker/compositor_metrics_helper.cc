// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_metrics_helper.h"

namespace blink {
namespace scheduler {

CompositorMetricsHelper::CompositorMetricsHelper() = default;

CompositorMetricsHelper::~CompositorMetricsHelper() = default;

void CompositorMetricsHelper::RecordTaskMetrics(
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  if (ShouldDiscardTask(task, task_timing))
    return;

  // Any needed metrics should be recorded here.
}

}  // namespace scheduler
}  // namespace blink
