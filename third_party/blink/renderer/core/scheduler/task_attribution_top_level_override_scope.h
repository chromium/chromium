// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_TOP_LEVEL_OVERRIDE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_TOP_LEVEL_OVERRIDE_SCOPE_H_

#include "base/memory/stack_allocated.h"
#include "base/types/pass_key.h"

namespace blink::scheduler {
class TaskAttributionTrackerImpl;
}  // namespace blink::scheduler

namespace blink {
class ExecutionContext;
class NavigateEvent;

// An RAII scope that forces Task Attribution to ignore the top-level
// requirement when propagating task state for the next task scope created. This
// allows propagation to occur when JS callback execution is too deeply nested
// under a ScriptState::Scope, which is needed for navigation API propagation.
//
// TODO(crbug.com/490536691): This class would not be needed if we had a better
// mechanism of detecting if JavaScript is currently executing.
class TaskAttributionTopLevelOverrideScope {
  STACK_ALLOCATED();

 public:
  // The type of scope. `Type::kDoNotOverride` is a noop, which can be used for
  // conditional scopes.
  enum class Type { kOverride, kDoNotOverride };

  using PassKeyType = base::PassKey<NavigateEvent>;

  TaskAttributionTopLevelOverrideScope(ExecutionContext*, Type, PassKeyType);
  TaskAttributionTopLevelOverrideScope(
      const TaskAttributionTopLevelOverrideScope&) = delete;
  const TaskAttributionTopLevelOverrideScope& operator=(
      const TaskAttributionTopLevelOverrideScope&) = delete;
  ~TaskAttributionTopLevelOverrideScope();

 private:
  scheduler::TaskAttributionTrackerImpl* tracker_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_TASK_ATTRIBUTION_TOP_LEVEL_OVERRIDE_SCOPE_H_
