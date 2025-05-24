// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/scheduler_task_context.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

SchedulerTaskContext::SchedulerTaskContext(ExecutionContext* context,
                                           AbortSignal* abort_source,
                                           DOMTaskSignal* priority_source)
    : abort_source_(abort_source),
      priority_source_(priority_source),
      security_origin_(
          base::WrapRefCounted(context->GetMutableSecurityOrigin())),
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
  return RuntimeEnabledFeatures::
                 SchedulerYieldDisallowCrossFrameInheritanceEnabled()
             ? &target == scheduler_execution_context_.Get()
             : target.GetSecurityOrigin()->CanAccess(security_origin_.get());
}

}  // namespace blink
