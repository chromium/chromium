// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/window_idle_tasks_manager.h"

#include <optional>

#include "base/check_op.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/scheduler/dom_scheduler.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/core/scheduler/scheduler_task_context.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_util.h"
#include "third_party/blink/renderer/core/scheduler/web_scheduling_task_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {

namespace {

// The valid id range is 1 to int max. int max is used instead of uint32_t max
// to keep the value in the 31-bit SMI range, and to be consistent with other
// IDs, e.g. setTimeout().
constexpr uint32_t kMaxCallbackId = std::numeric_limits<int>::max();

}  // namespace

// `V8IdleTask` is the adapter class for the conversion from
// `V8IdleRequestCallback` to `IdleTask`.
class WindowIdleTasksManager::V8IdleTask : public IdleTask {
 public:
  V8IdleTask(uint32_t id,
             WindowIdleTasksManager* manager,
             V8IdleRequestCallback* callback,
             ExecutionContext* scheduling_context)
      : id_(id), window_idle_tasks_manager_(manager), callback_(callback) {
    ScriptState* script_state = callback_->CallbackRelevantScriptState();
    auto* signal =
        DOMScheduler::scheduler(*scheduling_context)
            ->GetFixedPriorityTaskSignal(
                script_state, WebSchedulingPriority::kBackgroundPriority);
    web_scheduling_task_state_ = MakeGarbageCollected<WebSchedulingTaskState>(
        CaptureCurrentTaskState(scheduling_context),
        MakeGarbageCollected<SchedulerTaskContext>(
            scheduling_context, /*abort_source=*/nullptr, signal));
  }

  ~V8IdleTask() override = default;

  void invoke(IdleDeadline* deadline) override {
    std::optional<scheduler::TaskAttributionTracker::TaskScope>
        task_attribution_scope;
    if (auto* tracker =
            scheduler::TaskAttributionTracker::From(callback_->GetIsolate())) {
      task_attribution_scope = tracker->SetCurrentTaskState(
          web_scheduling_task_state_, TaskScopeType::kRequestIdleCallback);
    }
    callback_->InvokeAndReportException(nullptr, deadline);
    window_idle_tasks_manager_->OnV8IdleTaskComplete(id_);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(window_idle_tasks_manager_);
    visitor->Trace(callback_);
    visitor->Trace(web_scheduling_task_state_);
    IdleTask::Trace(visitor);
  }

 private:
  uint32_t id_;
  Member<WindowIdleTasksManager> window_idle_tasks_manager_;
  Member<V8IdleRequestCallback> callback_;
  Member<WebSchedulingTaskState> web_scheduling_task_state_;
};

// static
const char WindowIdleTasksManager::kSupplementName[] = "WindowIdleTasksManager";

// static
WindowIdleTasksManager& WindowIdleTasksManager::From(LocalDOMWindow& window) {
  WindowIdleTasksManager* manager =
      Supplement<LocalDOMWindow>::From<WindowIdleTasksManager>(&window);
  if (!manager) {
    manager = MakeGarbageCollected<WindowIdleTasksManager>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, manager);
  }
  return *manager;
}

WindowIdleTasksManager::WindowIdleTasksManager(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      ExecutionContextLifecycleObserver(&window),
      scripted_idle_task_controller_(
          MakeGarbageCollected<ScriptedIdleTaskController>(&window)) {}

uint32_t WindowIdleTasksManager::RequestIdleCallback(
    V8IdleRequestCallback* callback,
    const IdleRequestOptions* options,
    PassKey) {
  if (GetSupplementable()->IsContextDestroyed()) {
    return 0;
  }

  uint32_t web_exposed_id = NextIdleCallbackId();
  ScriptedIdleTaskController::CallbackId callback_id =
      scripted_idle_task_controller_->RegisterCallback(
          MakeGarbageCollected<V8IdleTask>(web_exposed_id, this, callback,
                                           GetSupplementable()),
          options);
  // Registering the callback is expected to succeed.
  CHECK_GT(callback_id, 0);

  auto result =
      web_exposed_ids_to_callback_ids_.insert(web_exposed_id, callback_id);
  CHECK(result.is_new_entry);
  return web_exposed_id;
}

void WindowIdleTasksManager::CancelIdleCallback(uint32_t id, PassKey) {
  if (id == 0 || id > kMaxCallbackId) {
    return;
  }
  auto iter = web_exposed_ids_to_callback_ids_.find(id);
  if (iter == web_exposed_ids_to_callback_ids_.end()) {
    return;
  }
  scripted_idle_task_controller_->CancelCallback(iter->value);
  web_exposed_ids_to_callback_ids_.erase(iter);
}

uint32_t WindowIdleTasksManager::NextIdleCallbackId() {
  bool limit_reached = false;
  while (true) {
    if (idle_callback_identifier_ == kMaxCallbackId) {
      // If we can't find an available index, something has gone wrong. Crash
      // instead of silently failing or infinitely looping.
      CHECK(!limit_reached);
      limit_reached = true;
      idle_callback_identifier_ = 1;
    } else {
      ++idle_callback_identifier_;
    }
    if (!web_exposed_ids_to_callback_ids_.Contains(idle_callback_identifier_)) {
      return idle_callback_identifier_;
    }
  }
}

void WindowIdleTasksManager::ContextDestroyed() {
  web_exposed_ids_to_callback_ids_.clear();
}

void WindowIdleTasksManager::Trace(Visitor* visitor) const {
  visitor->Trace(scripted_idle_task_controller_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void WindowIdleTasksManager::OnV8IdleTaskComplete(uint32_t id) {
  CHECK_NE(id, 0u);
  // Note: The callback itself might have called `CancelIdleCallback()`, so
  // `id` might have already been removed.
  web_exposed_ids_to_callback_ids_.erase(id);
}

}  // namespace blink
