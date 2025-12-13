// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/scheduler_task_context.h"

#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"

namespace blink {

SchedulerTaskContext::SchedulerTaskContext(ExecutionContext* context,
                                           AbortSignal* abort_source,
                                           DOMTaskSignal* priority_source)
    : abort_source_(abort_source),
      priority_source_(priority_source),
      scheduler_execution_context_(context) {}

void SchedulerTaskContext::Trace(Visitor* visitor) const {
  visitor->Trace(abort_source_);
  visitor->Trace(priority_source_);
  visitor->Trace(scheduler_execution_context_);
}

AbortSignal* SchedulerTaskContext::AbortSource() {
  return abort_source_.Get();
}

DOMTaskSignal* SchedulerTaskContext::PrioritySource() {
  return priority_source_.Get();
}

bool SchedulerTaskContext::CanPropagateTo(
    const ExecutionContext& target) const {
  return &target == scheduler_execution_context_.Get();
}

}  // namespace blink
