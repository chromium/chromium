// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_SCHEDULER_TASK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_SCHEDULER_TASK_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class AbortSignal;
class DOMTaskSignal;
class ExecutionContext;

class SchedulerTaskContext : public GarbageCollected<SchedulerTaskContext> {
 public:
  SchedulerTaskContext(ExecutionContext* scheduler_context,
                       AbortSignal* abort_source,
                       DOMTaskSignal* priority_source);

  AbortSignal* AbortSource();
  DOMTaskSignal* PrioritySource();

  bool CanPropagateTo(const ExecutionContext& target) const;

  void Trace(Visitor*) const;

 private:
  const Member<AbortSignal> abort_source_;
  const Member<DOMTaskSignal> priority_source_;
  const WeakMember<ExecutionContext> scheduler_execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_SCHEDULER_TASK_CONTEXT_H_
