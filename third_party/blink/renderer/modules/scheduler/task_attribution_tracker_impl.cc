// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task_attribution_tracker_impl.h"

#include <memory>
#include <utility>

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
  }
}

int64_t TaskAttributionIdToInt(absl::optional<TaskAttributionId> id) {
  return id ? static_cast<int64_t>(id.value().value()) : -1;
}

}  // namespace

TaskAttributionTrackerImpl::TaskAttributionTrackerImpl() : next_task_id_(0) {}

TaskAttributionInfo* TaskAttributionTrackerImpl::RunningTask(
    ScriptState* script_state) const {
  ScriptWrappableTaskState* task_state =
      GetCurrentTaskContinuationData(script_state);

  // V8 embedder state may have no value in the case of a JSPromise that wasn't
  // yet resolved.
  return task_state ? task_state->GetTask() : running_task_.Get();
}

template <typename F>
TaskAttributionTracker::AncestorStatus
TaskAttributionTrackerImpl::IsAncestorInternal(ScriptState* script_state,
                                               F is_ancestor) {
  DCHECK(script_state);
  if (!script_state->World().IsMainWorld()) {
    // As RunningTask will not return a TaskAttributionInfo for
    // non-main-world tasks, there's no point in testing their ancestry.
    return AncestorStatus::kNotAncestor;
  }

  TaskAttributionInfo* current_task = RunningTask(script_state);

  while (current_task) {
    TaskAttributionInfo* parent_task = current_task->Parent();
    if (is_ancestor(current_task->Id())) {
      return AncestorStatus::kAncestor;
    }
    current_task = parent_task;
  }
  return AncestorStatus::kNotAncestor;
}

TaskAttributionTracker::AncestorStatus TaskAttributionTrackerImpl::IsAncestor(
    ScriptState* script_state,
    TaskAttributionId ancestor_id) {
  return IsAncestorInternal(
      script_state,
      [&](const TaskAttributionId& task_id) { return task_id == ancestor_id; });
}

TaskAttributionTracker::AncestorStatus
TaskAttributionTrackerImpl::HasAncestorInSet(
    ScriptState* script_state,
    const WTF::HashSet<scheduler::TaskAttributionIdType>& set) {
  return IsAncestorInternal(script_state,
                            [&](const TaskAttributionId& task_id) {
                              return set.Contains(task_id.value());
                            });
}

std::unique_ptr<TaskAttributionTracker::TaskScope>
TaskAttributionTrackerImpl::CreateTaskScope(ScriptState* script_state,
                                            TaskAttributionInfo* parent_task,
                                            TaskScopeType type) {
  return CreateTaskScope(script_state, parent_task, type,
                         /*abort_source=*/nullptr, /*priority_source=*/nullptr);
}

std::unique_ptr<TaskAttributionTracker::TaskScope>
TaskAttributionTrackerImpl::CreateTaskScope(ScriptState* script_state,
                                            TaskAttributionInfo* parent_task,
                                            TaskScopeType type,
                                            AbortSignal* abort_source,
                                            DOMTaskSignal* priority_source) {
  TaskAttributionInfo* running_task_to_be_restored = running_task_;
  ScriptWrappableTaskState* continuation_task_state_to_be_restored =
      GetCurrentTaskContinuationData(script_state);

  next_task_id_ = next_task_id_.NextId();
  running_task_ =
      MakeGarbageCollected<TaskAttributionInfo>(next_task_id_, parent_task);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  for (Observer* observer : observers_) {
    if (observer->GetExecutionContext() == execution_context) {
      observer->OnCreateTaskScope(next_task_id_);
    }
  }

  SetCurrentTaskContinuationData(
      script_state, MakeGarbageCollected<ScriptWrappableTaskState>(
                        running_task_.Get(), abort_source, priority_source));

  return std::make_unique<TaskScopeImpl>(
      script_state, this, next_task_id_, running_task_to_be_restored,
      continuation_task_state_to_be_restored, type,
      parent_task ? absl::optional<TaskAttributionId>(parent_task->Id())
                  : absl::nullopt);
}

void TaskAttributionTrackerImpl::TaskScopeCompleted(
    const TaskScopeImpl& task_scope) {
  DCHECK(running_task_);
  DCHECK(running_task_->Id() == task_scope.GetTaskId());
  running_task_ = task_scope.RunningTaskToBeRestored();
  SetCurrentTaskContinuationData(
      task_scope.GetScriptState(),
      task_scope.ContinuationTaskStateToBeRestored());
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
    if (task && (task->Id() == task_id)) {
      return task;
    }
  }
  return nullptr;
}

void TaskAttributionTrackerImpl::SetCurrentTaskContinuationData(
    ScriptState* script_state,
    ScriptWrappableTaskState* task_state) {
  ScriptWrappableTaskState::SetCurrent(script_state, task_state);
}

ScriptWrappableTaskState*
TaskAttributionTrackerImpl::GetCurrentTaskContinuationData(
    ScriptState* script_state) const {
  return ScriptWrappableTaskState::GetCurrent(script_state);
}

// TaskScope's implementation
//////////////////////////////////////
TaskAttributionTrackerImpl::TaskScopeImpl::TaskScopeImpl(
    ScriptState* script_state,
    TaskAttributionTrackerImpl* task_tracker,
    TaskAttributionId scope_task_id,
    TaskAttributionInfo* running_task,
    ScriptWrappableTaskState* continuation_task_state,
    TaskScopeType type,
    absl::optional<TaskAttributionId> parent_task_id)
    : task_tracker_(task_tracker),
      scope_task_id_(scope_task_id),
      running_task_to_be_restored_(running_task),
      continuation_state_to_be_restored_(continuation_task_state),
      script_state_(script_state) {
  TRACE_EVENT_BEGIN(
      "scheduler", "BlinkTaskScope", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_blink_task_scope();
        data->set_type(ToProtoEnum(type));
        data->set_scope_task_id(scope_task_id_.value());
        data->set_running_task_id_to_be_restored(TaskAttributionIdToInt(
            running_task_to_be_restored_ ? running_task_to_be_restored_->Id()
                                         : TaskAttributionId()));
        data->set_continuation_task_id_to_be_restored(TaskAttributionIdToInt(
            continuation_state_to_be_restored_ &&
                    continuation_state_to_be_restored_->GetTask()
                ? absl::optional<TaskAttributionId>(
                      continuation_state_to_be_restored_->GetTask()->Id())
                : absl::nullopt));
        data->set_parent_task_id(TaskAttributionIdToInt(parent_task_id));
      });
}

TaskAttributionTrackerImpl::TaskScopeImpl::~TaskScopeImpl() {
  task_tracker_->TaskScopeCompleted(*this);
  TRACE_EVENT_END("scheduler");
}

}  // namespace blink::scheduler
