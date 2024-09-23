// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_attribution_tracker_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_info_impl.h"
#include "third_party/blink/renderer/core/scheduler/web_scheduling_task_state.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink::scheduler {

namespace {

perfetto::protos::pbzero::BlinkTaskScope::TaskScopeType ToProtoEnum(
    TaskAttributionTracker::TaskScopeType type) {
  using ProtoType = perfetto::protos::pbzero::BlinkTaskScope::TaskScopeType;
  switch (type) {
    case TaskAttributionTracker::TaskScopeType::kCallback:
      return ProtoType::TASK_SCOPE_CALLBACK;
    case TaskAttributionTracker::TaskScopeType::kScheduledAction:
      return ProtoType::TASK_SCOPE_SCHEDULED_ACTION;
    case TaskAttributionTracker::TaskScopeType::kScriptExecution:
      return ProtoType::TASK_SCOPE_SCRIPT_EXECUTION;
    case TaskAttributionTracker::TaskScopeType::kPostMessage:
      return ProtoType::TASK_SCOPE_POST_MESSAGE;
    case TaskAttributionTracker::TaskScopeType::kPopState:
      return ProtoType::TASK_SCOPE_POP_STATE;
    case TaskAttributionTracker::TaskScopeType::kSchedulerPostTask:
      return ProtoType::TASK_SCOPE_SCHEDULER_POST_TASK;
    case TaskAttributionTracker::TaskScopeType::kRequestIdleCallback:
      return ProtoType::TASK_SCOPE_REQUEST_IDLE_CALLBACK;
    case TaskAttributionTracker::TaskScopeType::kXMLHttpRequest:
      return ProtoType::TASK_SCOPE_XML_HTTP_REQUEST;
    case TaskAttributionTracker::TaskScopeType::kSoftNavigation:
      return ProtoType::TASK_SCOPE_SOFT_NAVIGATION;
  }
}

}  // namespace

// static
std::unique_ptr<TaskAttributionTracker> TaskAttributionTrackerImpl::Create(
    v8::Isolate* isolate) {
  return base::WrapUnique(new TaskAttributionTrackerImpl(isolate));
}

TaskAttributionTrackerImpl::TaskAttributionTrackerImpl(v8::Isolate* isolate)
    : next_task_id_(0), isolate_(isolate) {
  CHECK(isolate_);
}

scheduler::TaskAttributionInfo* TaskAttributionTrackerImpl::RunningTask()
    const {
  if (ScriptWrappableTaskState* task_state =
          ScriptWrappableTaskState::GetCurrent(isolate_)) {
    return task_state->WrappedState()->GetTaskAttributionInfo();
  }
  // There won't be a running task outside of a `TaskScope` or microtask
  // checkpoint.
  return nullptr;
}

TaskAttributionTracker::TaskScope TaskAttributionTrackerImpl::CreateTaskScope(
    ScriptState* script_state,
    TaskAttributionInfo* task_state,
    TaskScopeType type) {
  return CreateTaskScope(script_state, task_state, type,
                         /*abort_source=*/nullptr, /*priority_source=*/nullptr);
}

TaskAttributionTracker::TaskScope TaskAttributionTrackerImpl::CreateTaskScope(
    ScriptState* script_state,
    SoftNavigationContext* soft_navigation_context) {
  next_task_id_ = next_task_id_.NextId();
  auto* task_state = MakeGarbageCollected<TaskAttributionInfoImpl>(
      next_task_id_, soft_navigation_context);
  return CreateTaskScope(script_state, task_state,
                         TaskScopeType::kSoftNavigation,
                         /*abort_source=*/nullptr, /*priority_source=*/nullptr);
}

TaskAttributionTracker::TaskScope TaskAttributionTrackerImpl::CreateTaskScope(
    ScriptState* script_state,
    TaskAttributionInfo* task_state,
    TaskScopeType type,
    AbortSignal* abort_source,
    DOMTaskSignal* priority_source) {
  CHECK(script_state);
  CHECK_EQ(script_state->GetIsolate(), isolate_);

  ScriptWrappableTaskState* previous_task_state =
      ScriptWrappableTaskState::GetCurrent(isolate_);
  WrappableTaskState* previous_unwrapped_task_state =
      previous_task_state ? previous_task_state->WrappedState() : nullptr;

  WrappableTaskState* running_task_state = nullptr;
  if (abort_source || priority_source) {
    running_task_state = MakeGarbageCollected<WebSchedulingTaskState>(
        task_state, abort_source, priority_source);
  } else {
    // If there's no scheduling state to propagate, we can just propagate the
    // same object.
    running_task_state = To<TaskAttributionInfoImpl>(task_state);
  }

  if (running_task_state != previous_unwrapped_task_state) {
    ScriptWrappableTaskState::SetCurrent(
        script_state,
        running_task_state
            ? MakeGarbageCollected<ScriptWrappableTaskState>(running_task_state)
            : nullptr);
  }

  TaskAttributionInfo* current =
      running_task_state ? running_task_state->GetTaskAttributionInfo()
                         : nullptr;
  TaskAttributionInfo* previous =
      previous_unwrapped_task_state
          ? previous_unwrapped_task_state->GetTaskAttributionInfo()
          : nullptr;

  // Fire observer callbacks after updating the CPED to keep `RunningTask()` in
  // sync with what is passed to the observer.
  //
  // TODO(crbug.com/40942324): The purpose of the `Observer` mechanism is so the
  // soft navigation layer can learn if an event ran while the scope is active,
  // which is why we filter out soft navigation task scopes. It might be better
  // to move event observation into event handling itself.
  if (observer_ && type != TaskScopeType::kSoftNavigation &&
      running_task_state) {
    observer_->OnCreateTaskScope(*current);
  }

  TRACE_EVENT_BEGIN(
      "scheduler", "BlinkTaskScope", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_blink_task_scope();
        data->set_type(ToProtoEnum(type));
        data->set_scope_task_id(current ? current->Id().value() : 0);
        data->set_running_task_id_to_be_restored(
            previous ? previous->Id().value() : 0);
      });

  return TaskScope(this, script_state, previous_task_state);
}

std::optional<TaskAttributionTracker::TaskScope>
TaskAttributionTrackerImpl::MaybeCreateTaskScopeForCallback(
    ScriptState* script_state,
    TaskAttributionInfo* task_state) {
  CHECK(script_state);

  // Always create a `TaskScope` if there's `task_state` to propagate.
  if (task_state) {
    return CreateTaskScope(script_state, task_state, TaskScopeType::kCallback);
  }

  // Even though we don't need to create a `TaskScope`, we still need to notify
  // the `observer_` since it relies on the callback to set up internal state.
  // And the `observer_` might not have been notified previously, e.g. if
  // the outermost `TaskScope` is for propagating soft navigation state.
  TaskAttributionInfo* current_task_state = RunningTask();
  if (observer_ && current_task_state) {
    observer_->OnCreateTaskScope(*current_task_state);
  }

  return std::nullopt;
}

void TaskAttributionTrackerImpl::OnTaskScopeDestroyed(
    const TaskScope& task_scope) {
  ScriptWrappableTaskState::SetCurrent(task_scope.script_state_,
                                       task_scope.previous_task_state_);
  TRACE_EVENT_END("scheduler");
}

TaskAttributionTracker::ObserverScope
TaskAttributionTrackerImpl::RegisterObserver(Observer* observer) {
  CHECK(observer);
  Observer* previous_observer = observer_.Get();
  observer_ = observer;
  return ObserverScope(this, observer, previous_observer);
}

void TaskAttributionTrackerImpl::OnObserverScopeDestroyed(
    const ObserverScope& observer_scope) {
  observer_ = observer_scope.PreviousObserver();
}

void TaskAttributionTrackerImpl::AddSameDocumentNavigationTask(
    TaskAttributionInfo* task) {
  same_document_navigation_tasks_.push_back(task);
}

void TaskAttributionTrackerImpl::ResetSameDocumentNavigationTasks() {
  same_document_navigation_tasks_.clear();
}

TaskAttributionInfo* TaskAttributionTrackerImpl::CommitSameDocumentNavigation(
    TaskAttributionId task_id) {
  // TODO(https://crbug.com/1464504): This may not handle cases where we have
  // multiple same document navigations that happen in the same process at the
  // same time.
  //
  // This pops all the same document navigation tasks that preceded the current
  // one, enabling them to be garbage collected.
  while (!same_document_navigation_tasks_.empty()) {
    auto task = same_document_navigation_tasks_.front();
    same_document_navigation_tasks_.pop_front();
    // TODO(https://crbug.com/1486774) - Investigate when |task| can be nullptr.
    if (task && task->Id() == task_id) {
      return task;
    }
  }
  return nullptr;
}

}  // namespace blink::scheduler
