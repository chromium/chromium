// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/web_scheduling_task_state.h"

#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"

namespace blink {

WebSchedulingTaskState::WebSchedulingTaskState(
    scheduler::TaskAttributionInfo* task_state,
    AbortSignal* abort_source,
    DOMTaskSignal* priority_source)
    : subtask_propagatable_task_state_(task_state),
      abort_source_(abort_source),
      priority_source_(priority_source) {}

void WebSchedulingTaskState::Trace(Visitor* visitor) const {
  visitor->Trace(abort_source_);
  visitor->Trace(priority_source_);
  visitor->Trace(subtask_propagatable_task_state_);
}

AbortSignal* WebSchedulingTaskState::AbortSource() {
  return abort_source_.Get();
}

DOMTaskSignal* WebSchedulingTaskState::PrioritySource() {
  return priority_source_.Get();
}

scheduler::TaskAttributionInfo*
WebSchedulingTaskState::GetTaskAttributionInfo() {
  return subtask_propagatable_task_state_.Get();
}

}  // namespace blink
