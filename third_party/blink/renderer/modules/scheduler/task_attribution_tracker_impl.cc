// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task_attribution_tracker_impl.h"

#include <memory>
#include <optional>

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
  ScriptWrappableTaskState* task_state =
      ScriptWrappableTaskState::GetCurrent(isolate_);

  // V8 embedder state may have no value in the case of a JSPromise that wasn't
  // yet resolved.
  return task_state ? task_state->GetTask() : running_task_.Get();
}

bool TaskAttributionTrackerImpl::IsAncestor(const TaskAttributionInfo& task,
                                            TaskAttributionId ancestor_id) {
  const TaskAttributionInfo* ancestor_task = nullptr;
  ForEachAncestor(task, [&](const TaskAttributionInfo& ancestor) {
    if (ancestor.Id() == ancestor_id) {
      ancestor_task = &ancestor;
      return IterationStatus::kStop;
    }
    return IterationStatus::kContinue;
  });
  return !!ancestor_task;
}

void TaskAttributionTrackerImpl::ForEachAncestor(
    const TaskAttributionInfo& task,
    base::FunctionRef<IterationStatus(const TaskAttributionInfo& task)>
        visitor) {
  const TaskAttributionInfo* current_task = &task;
  while (current_task) {
    const TaskAttributionInfo* parent_task = current_task->Parent();
    if (visitor(*current_task) == IterationStatus::kStop) {
      return;
    }
    current_task = parent_task;
  }
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
  TaskAttributionInfo* running_task_to_be_restored = running_task_;
  ScriptWrappableTaskState* continuation_task_state_to_be_restored =
      ScriptWrappableTaskState::GetCurrent(isolate_);

  // This compresses the task graph when encountering long task chains.
  // TODO(crbug.com/1501999): Consider compressing the task graph further.
  if (!parent_task || !parent_task->MaxChainLengthReached()) {
    next_task_id_ = next_task_id_.NextId();
    running_task_ =
        MakeGarbageCollected<TaskAttributionInfo>(next_task_id_, parent_task);
  } else {
    running_task_ = parent_task;
  }

  if (observer_) {
    observer_->OnCreateTaskScope(*running_task_);
  }

  ScriptWrappableTaskState::SetCurrent(
      script_state, MakeGarbageCollected<ScriptWrappableTaskState>(
                        running_task_.Get(), abort_source, priority_source));

  std::optional<TaskAttributionId> parent_task_id =
      running_task_->Parent()
          ? std::optional<TaskAttributionId>(running_task_->Parent()->Id())
          : std::nullopt;
  TRACE_EVENT_BEGIN(
      "scheduler", "BlinkTaskScope", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_blink_task_scope();
        data->set_type(ToProtoEnum(type));
        data->set_scope_task_id(running_task_->Id().value());
        data->set_running_task_id_to_be_restored(TaskAttributionIdToInt(
            running_task_to_be_restored ? running_task_to_be_restored->Id()
                                        : TaskAttributionId()));
        data->set_continuation_task_id_to_be_restored(TaskAttributionIdToInt(
            continuation_task_state_to_be_restored &&
                    continuation_task_state_to_be_restored->GetTask()
                ? std::optional<TaskAttributionId>(
                      continuation_task_state_to_be_restored->GetTask()->Id())
                : std::nullopt));
        data->set_parent_task_id(TaskAttributionIdToInt(parent_task_id));
      });

  return TaskScope(this, script_state, running_task_to_be_restored,
                   continuation_task_state_to_be_restored);
}

void TaskAttributionTrackerImpl::OnTaskScopeDestroyed(
    const TaskScope& task_scope) {
  DCHECK(running_task_);
  running_task_ = task_scope.previous_running_task_;
  ScriptWrappableTaskState::SetCurrent(
      task_scope.script_state_, task_scope.previous_continuation_task_state_);
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
