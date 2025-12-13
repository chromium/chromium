// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_UTIL_H_

#include <optional>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_task_state.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

[[nodiscard]] inline scheduler::TaskAttributionInfo* CaptureCurrentTaskState(
    ExecutionContext* context) {
  if (!context) {
    return nullptr;
  }
  auto* tracker =
      scheduler::TaskAttributionTracker::From(context->GetIsolate());
  // `tracker` is null if `context` is not a Window or if
  // TaskAttributionInfrastructureDisabledForTesting is enabled.
  return tracker ? tracker->CurrentTaskState() : nullptr;
}

[[nodiscard]] inline scheduler::TaskAttributionInfo*
CaptureCurrentTaskStateIfMainWorld(ScriptState* script_state) {
  if (!script_state || !script_state->World().IsMainWorld()) {
    return nullptr;
  }
  return CaptureCurrentTaskState(ExecutionContext::From(script_state));
}

[[nodiscard]] inline std::optional<scheduler::TaskAttributionTracker::TaskScope>
SetCurrentTaskStateIfTopLevel(scheduler::TaskAttributionInfo* task_state,
                              ExecutionContext* context,
                              TaskScopeType type) {
  if (!context || context->IsContextDestroyed()) {
    return std::nullopt;
  }
  // `tracker` is null if `context` is not a Window or if
  // TaskAttributionInfrastructureDisabledForTesting is enabled.
  auto* tracker =
      scheduler::TaskAttributionTracker::From(context->GetIsolate());
  return tracker ? tracker->SetCurrentTaskStateIfTopLevel(task_state, type)
                 : std::nullopt;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_UTIL_H_
