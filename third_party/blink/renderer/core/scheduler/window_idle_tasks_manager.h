// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_WINDOW_IDLE_TASKS_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_WINDOW_IDLE_TASKS_MANAGER_H_

#include <cstdint>

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {
class IdleRequestOptions;
class LocalDOMWindow;
class V8IdleRequestCallback;
class WindowIdleTasks;
class WindowIdleTasksManagerTest;

// This class manages idle tasks scheduled with requestIdleCallback()
// (https://www.w3.org/TR/requestidlecallback/). Internal idle tasks should use
// either ScriptedIdleTaskController (if IdleRequestOptions or an IdleDeadline
// is needed), or ThreadScheduler::PostIdleTask (otherwise).
class CORE_EXPORT WindowIdleTasksManager final
    : public GarbageCollected<WindowIdleTasksManager>,
      public Supplement<LocalDOMWindow>,
      public ExecutionContextLifecycleObserver {
 public:
  static WindowIdleTasksManager& From(LocalDOMWindow&);
  static const char kSupplementName[];

  using PassKey = base::PassKey<WindowIdleTasks, WindowIdleTasksManagerTest>;

  explicit WindowIdleTasksManager(LocalDOMWindow&);
  ~WindowIdleTasksManager() override = default;

  void Trace(Visitor*) const override;

  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  // https://www.w3.org/TR/requestidlecallback/#the-requestidlecallback-method
  uint32_t RequestIdleCallback(V8IdleRequestCallback*,
                               const IdleRequestOptions*,
                               PassKey);

  // https://www.w3.org/TR/requestidlecallback/#the-cancelidlecallback-method
  void CancelIdleCallback(uint32_t id, PassKey);

  ScriptedIdleTaskController& GetScriptedIdleTaskController() const {
    return *scripted_idle_task_controller_;
  }

 private:
  class V8IdleTask;
  friend class WindowIdleTasksManagerTest;

  void OnV8IdleTaskComplete(uint32_t id);

  uint32_t NextIdleCallbackId();

  // https://www.w3.org/TR/requestidlecallback/#window_extensions
  uint32_t idle_callback_identifier_ = 0;

  // Map from web-exposed idle callback identifier to internal, global
  // CallbackId.
  HashMap<uint32_t, ScriptedIdleTaskController::CallbackId>
      web_exposed_ids_to_callback_ids_;

  const Member<ScriptedIdleTaskController> scripted_idle_task_controller_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_WINDOW_IDLE_TASKS_MANAGER_H_
