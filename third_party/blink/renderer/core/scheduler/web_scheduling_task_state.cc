// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/web_scheduling_task_state.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/scheduler/scheduler_task_context.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_info_impl.h"
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

TaskAttributionTaskState* WebSchedulingTaskState::ForkAndSetVariable(
    const scheduler::TaskAttributionId next_task_id,
    ResourceTimingContext* resource_timing_context) {
  scheduler::TaskAttributionInfo* current_task_attribution_info =
      MakeGarbageCollected<TaskAttributionInfoImpl>(
          next_task_id,
          GetTaskAttributionInfo()
              ? GetTaskAttributionInfo()->GetSoftNavigationContext()
              : nullptr,
          resource_timing_context);
  return MakeGarbageCollected<WebSchedulingTaskState>(
      current_task_attribution_info, GetSchedulerTaskContext());
}

TaskAttributionTaskState* WebSchedulingTaskState::ForkAndSetVariable(
    const scheduler::TaskAttributionId next_task_id,
    SoftNavigationContext* soft_navigation_context) {
  // TODO(crbug.com/475261410):  `SoftNavigationContext` is not expected to be
  // created in web scheduling tasks and continuations, but this didn't hold in
  // the case of dispatching simulated clicks to a form control's label. This
  // has been fixed, but to be safe, this is being marked as non-fatal to flush
  // out any other cases. Everything below the NOTREACHED should be removed once
  // we're confident there are no more such cases.
  NOTREACHED(base::NotFatalUntil::M149);

  scheduler::TaskAttributionInfo* current_task_attribution_info =
      MakeGarbageCollected<TaskAttributionInfoImpl>(
          next_task_id, soft_navigation_context,
          GetTaskAttributionInfo()
              ? GetTaskAttributionInfo()->GetResourceTimingContext()
              : nullptr);
  return MakeGarbageCollected<WebSchedulingTaskState>(
      current_task_attribution_info, GetSchedulerTaskContext());
}

}  // namespace blink
