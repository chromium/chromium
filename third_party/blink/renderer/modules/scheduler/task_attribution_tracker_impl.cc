// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task_attribution_tracker_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
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
  }
}

int64_t TaskAttributionIdToInt(std::optional<TaskAttributionId> id) {
  return id ? static_cast<int64_t>(id.value().value()) : -1;
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

TaskAttributionInfo* TaskAttributionTrackerImpl::RunningTask() const {
  if (ScriptWrappableTaskState* task_state =
          ScriptWrappableTaskState::GetCurrent(isolate_)) {
    return task_state->GetTask();
  }
  // There won't be a running task outside of a `TaskScope` or microtask
  // checkpoint.
  return nullptr;
}

TaskAttributionTracker::TaskScope TaskAttributionTrackerImpl::CreateTaskScope(
    ScriptState* script_state,
    TaskAttributionInfo* parent_task,
    TaskScopeType type) {
  return CreateTaskScope(script_state, parent_task, type,
                         /*abort_source=*/nullptr, /*priority_source=*/nullptr);
}

TaskAttributionTracker::TaskScope TaskAttributionTrackerImpl::CreateTaskScope(
    ScriptState* script_state,
    TaskAttributionInfo* parent_task,
    TaskScopeType type,
    AbortSignal* abort_source,
    DOMTaskSignal* priority_source) {
  CHECK(script_state);
  CHECK_EQ(script_state->GetIsolate(), isolate_);
  ScriptWrappableTaskState* previous_task_state =
      ScriptWrappableTaskState::GetCurrent(isolate_);

  // Always propagate the current state (`parent_task`) when given. Otherwise
  // create new state to begin propagating.
  TaskAttributionInfo* running_task_info = nullptr;
  if (!parent_task) {
    next_task_id_ = next_task_id_.NextId();
    running_task_info =
        MakeGarbageCollected<TaskAttributionInfo>(next_task_id_);
  } else {
    running_task_info = parent_task;
  }

  ScriptWrappableTaskState* running_task_state =
      MakeGarbageCollected<ScriptWrappableTaskState>(
          running_task_info, abort_source, priority_source);
  ScriptWrappableTaskState::SetCurrent(script_state, running_task_state);

  // Fire observer callbacks after updating the CPED to keep `RunningTask()` in
  // sync with what is passed to the observer.
  if (observer_) {
    observer_->OnCreateTaskScope(*running_task_info);
  }

  TRACE_EVENT_BEGIN(
      "scheduler", "BlinkTaskScope", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_blink_task_scope();
        data->set_type(ToProtoEnum(type));
        data->set_scope_task_id(running_task_info->Id().value());
        data->set_running_task_id_to_be_restored(TaskAttributionIdToInt(
            previous_task_state && previous_task_state->GetTask()
                ? std::optional<TaskAttributionId>(
                      previous_task_state->GetTask()->Id())
                : std::nullopt));
      });

  return TaskScope(this, script_state, previous_task_state);
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
