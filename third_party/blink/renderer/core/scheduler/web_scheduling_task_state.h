// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_WEB_SCHEDULING_TASK_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_WEB_SCHEDULING_TASK_STATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink::scheduler {
class TaskAttributionInfo;
}  // namespace blink::scheduler

namespace blink {
class AbortSignal;
class DOMTaskSignal;

class CORE_EXPORT WebSchedulingTaskState final
    : public GarbageCollected<WebSchedulingTaskState>,
      public WrappableTaskState {
 public:
  WebSchedulingTaskState(scheduler::TaskAttributionInfo*,
                         AbortSignal* abort_source,
                         DOMTaskSignal* priority_source);

  // `WrappableTaskState` implementation:
  AbortSignal* AbortSource() override;
  DOMTaskSignal* PrioritySource() override;
  scheduler::TaskAttributionInfo* GetTaskAttributionInfo() override;
  void Trace(Visitor*) const override;

 private:
  const Member<scheduler::TaskAttributionInfo> subtask_propagatable_task_state_;
  const Member<AbortSignal> abort_source_;
  const Member<DOMTaskSignal> priority_source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_WEB_SCHEDULING_TASK_STATE_H_
