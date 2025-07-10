// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/web_scheduling_task_state.h"

#include "third_party/blink/renderer/core/scheduler/scheduler_task_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"

namespace blink {

WebSchedulingTaskState::WebSchedulingTaskState(
    scheduler::TaskAttributionInfo* task_state,
    SchedulerTaskContext* task_context)
    : subtask_propagatable_task_state_(task_state),
      scheduler_task_context_(task_context) {}

void WebSchedulingTaskState::Trace(Visitor* visitor) const {
  TaskAttributionTaskState::Trace(visitor);
  visitor->Trace(scheduler_task_context_);
  visitor->Trace(subtask_propagatable_task_state_);
}

scheduler::TaskAttributionInfo*
WebSchedulingTaskState::GetTaskAttributionInfo() {
  return subtask_propagatable_task_state_.Get();
}

SchedulerTaskContext* WebSchedulingTaskState::GetSchedulerTaskContext() {
  return scheduler_task_context_.Get();
}

}  // namespace blink
