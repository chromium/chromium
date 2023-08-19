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
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink::scheduler {

namespace {

static unsigned Hash(TaskAttributionId id) {
  return id.value() % TaskAttributionTrackerImpl::kVectorSize;
}

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

absl::optional<TaskAttributionId>
TaskAttributionTrackerImpl::RunningTaskAttributionId(
    ScriptState* script_state) const {
  ScriptWrappableTaskState* task_state =
      GetCurrentTaskContinuationData(script_state);
  // V8 embedder state may have no value in the case of a JSPromise that wasn't
  // yet resolved.
  return task_state ? task_state->GetTaskAttributionId() : running_task_id_;
}

void TaskAttributionTrackerImpl::InsertTaskAttributionIdPair(
    TaskAttributionId task_id,
    absl::optional<TaskAttributionId> parent_task_id) {
  unsigned task_id_hash = Hash(task_id);
  task_container_[task_id_hash] =
      TaskAttributionIdPair(parent_task_id, task_id);
}

TaskAttributionTrackerImpl::TaskAttributionIdPair&
TaskAttributionTrackerImpl::GetTaskAttributionIdPairFromTaskContainer(
    TaskAttributionId id) {
  unsigned slot = Hash(id);
  DCHECK_LT(slot, task_container_.size());
  return (task_container_[slot]);
}

template <typename F>
TaskAttributionTracker::AncestorStatus
TaskAttributionTrackerImpl::IsAncestorInternal(ScriptState* script_state,
                                               F is_ancestor) {
  DCHECK(script_state);
  if (!script_state->World().IsMainWorld()) {
    // As RunningTaskAttributionId will not return a TaskAttributionId for
    // non-main-world tasks, there's no point in testing their ancestry.
    return AncestorStatus::kNotAncestor;
  }

  absl::optional<TaskAttributionId> current_task_id =
      RunningTaskAttributionId(script_state);
  if (!current_task_id) {
    // TODO(yoav): This should not happen, but does. See crbug.com/1326872.
    return AncestorStatus::kNotAncestor;
  }
  if (is_ancestor(current_task_id.value())) {
    return AncestorStatus::kAncestor;
  }

  // Each slot in the `task_container_` contains a struct with a parent and
  // current task ID optionals. The loop "climbs" up that task dependency tree
  // until it either reaches the "root" task (which has no parent), or until it
  // finds a parent, which current task ID doesn't match the one its child
  // pointed at, indicating that the parent's slot in the array was overwritten.
  // In that case, it's returning the kUnknown value.
  const TaskAttributionIdPair& current_pair =
      GetTaskAttributionIdPairFromTaskContainer(current_task_id.value());
  absl::optional<TaskAttributionId> parent_id = current_pair.parent;
  DCHECK(current_pair.current);
  while (parent_id) {
    const TaskAttributionIdPair& parent_pair =
        GetTaskAttributionIdPairFromTaskContainer(parent_id.value());
    if (parent_pair.current && parent_pair.current != parent_id) {
      // Found a parent slot, but its ID doesn't match what we thought it would
      // be. That means we circled around the circular array, and we can no
      // longer know the ancestry.
      return AncestorStatus::kUnknown;
    }
    if (is_ancestor(parent_id.value())) {
      return AncestorStatus::kAncestor;
    }
    DCHECK(parent_pair.current.value() != current_task_id.value());
    DCHECK(!parent_pair.parent || parent_pair.parent < parent_id);
    parent_id = parent_pair.parent;
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
TaskAttributionTrackerImpl::CreateTaskScope(
    ScriptState* script_state,
    absl::optional<TaskAttributionId> parent_task_id,
    TaskScopeType type) {
  return CreateTaskScope(script_state, parent_task_id, type,
                         /*abort_source=*/nullptr, /*priority_source=*/nullptr);
}

std::unique_ptr<TaskAttributionTracker::TaskScope>
TaskAttributionTrackerImpl::CreateTaskScope(
    ScriptState* script_state,
    absl::optional<TaskAttributionId> parent_task_id,
    TaskScopeType type,
    AbortSignal* abort_source,
    DOMTaskSignal* priority_source) {
  absl::optional<TaskAttributionId> running_task_id_to_be_restored =
      running_task_id_;
  ScriptWrappableTaskState* continuation_task_state_to_be_restored =
      GetCurrentTaskContinuationData(script_state);

  next_task_id_ = next_task_id_.NextId();
  running_task_id_ = next_task_id_;

  InsertTaskAttributionIdPair(next_task_id_, parent_task_id);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  for (Observer* observer : observers_) {
    if (observer->GetExecutionContext() == execution_context) {
      observer->OnCreateTaskScope(next_task_id_);
    }
  }

  SetCurrentTaskContinuationData(
      script_state, MakeGarbageCollected<ScriptWrappableTaskState>(
                        next_task_id_, abort_source, priority_source));

  return std::make_unique<TaskScopeImpl>(
      script_state, this, next_task_id_, running_task_id_to_be_restored,
      continuation_task_state_to_be_restored, type, parent_task_id);
}

void TaskAttributionTrackerImpl::TaskScopeCompleted(
    const TaskScopeImpl& task_scope) {
  DCHECK(running_task_id_ == task_scope.GetTaskId());
  running_task_id_ = task_scope.RunningTaskIdToBeRestored();
  SetCurrentTaskContinuationData(
      task_scope.GetScriptState(),
      task_scope.ContinuationTaskStateToBeRestored());
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
    absl::optional<TaskAttributionId> running_task_id,
    ScriptWrappableTaskState* continuation_task_state,
    TaskScopeType type,
    absl::optional<TaskAttributionId> parent_task_id)
    : task_tracker_(task_tracker),
      scope_task_id_(scope_task_id),
      running_task_id_to_be_restored_(running_task_id),
      continuation_state_to_be_restored_(continuation_task_state),
      script_state_(script_state) {
  TRACE_EVENT_BEGIN(
      "scheduler", "BlinkTaskScope", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_blink_task_scope();
        data->set_type(ToProtoEnum(type));
        data->set_scope_task_id(scope_task_id_.value());
        data->set_running_task_id_to_be_restored(
            TaskAttributionIdToInt(running_task_id_to_be_restored_));
        data->set_continuation_task_id_to_be_restored(TaskAttributionIdToInt(
            continuation_state_to_be_restored_
                ? absl::make_optional(continuation_state_to_be_restored_
                                          ->GetTaskAttributionId())
                : absl::nullopt));
        data->set_parent_task_id(TaskAttributionIdToInt(parent_task_id));
      });
}

TaskAttributionTrackerImpl::TaskScopeImpl::~TaskScopeImpl() {
  task_tracker_->TaskScopeCompleted(*this);
  TRACE_EVENT_END("scheduler");
}

}  // namespace blink::scheduler
