// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_WEB_SCHEDULING_TASK_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_WEB_SCHEDULING_TASK_STATE_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink::scheduler {
class TaskAttributionInfo;
}  // namespace blink::scheduler

namespace blink {
class AbortSignal;
class DOMTaskSignal;

class MODULES_EXPORT WebSchedulingTaskState final
    : public ScriptWrappableTaskState {
 public:
  WebSchedulingTaskState(scheduler::TaskAttributionInfo*,
                         AbortSignal* abort_source,
                         DOMTaskSignal* priority_source);

  // `ScriptWrappableTaskState` implementation:
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

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_WEB_SCHEDULING_TASK_STATE_H_
