// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task_attribution_tracker_impl.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_script_wrappable_task_id.h"
#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_id.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink::scheduler {

namespace {

static unsigned Hash(TaskId id) {
  return id.value() % TaskAttributionTrackerImpl::kVectorSize;
}

}  // namespace

TaskAttributionTrackerImpl::TaskAttributionTrackerImpl()
    : next_task_id_(0), v8_adapter_(std::make_unique<V8Adapter>()) {}

absl::optional<TaskId> TaskAttributionTrackerImpl::RunningTaskId(
    ScriptState* script_state) const {
  DCHECK(v8_adapter_);
  absl::optional<TaskId> task_id = v8_adapter_->GetValue(script_state);

  // V8 embedder state may have no value in the case of a JSPromise that wasn't
  // yet resolved.
  return task_id ? task_id : running_task_id_;
}

void TaskAttributionTrackerImpl::InsertTaskIdPair(
    TaskId task_id,
    absl::optional<TaskId> parent_task_id) {
  unsigned task_id_hash = Hash(task_id);
  task_container_[task_id_hash] = TaskIdPair(parent_task_id, task_id);
}

TaskAttributionTrackerImpl::TaskIdPair&
TaskAttributionTrackerImpl::GetTaskIdPairFromTaskContainer(TaskId id) {
  unsigned slot = Hash(id);
  DCHECK_LT(slot, task_container_.size());
  return (task_container_[slot]);
}

TaskAttributionTracker::AncestorStatus TaskAttributionTrackerImpl::IsAncestor(
    ScriptState* script_state,
    TaskId ancestor_id) {
  absl::optional<TaskId> current_task_id = RunningTaskId(script_state);
  DCHECK(current_task_id);
  if (current_task_id.value() == ancestor_id) {
    return AncestorStatus::kAncestor;
  }

  // Each slot in the `task_container_` contains a struct with a parent and
  // current task ID optionals. The loop "climbs" up that task dependency tree
  // until it either reaches the "root" task (which has no parent), or until it
  // finds a parent, which current task ID doesn't match the one its child
  // pointed at, indicating that the parent's slot in the array was overwritten.
  // In that case, it's returning the kUnknown value.
  const TaskIdPair& current_pair =
      GetTaskIdPairFromTaskContainer(current_task_id.value());
  absl::optional<TaskId> parent_id = current_pair.parent;
  DCHECK(current_pair.current);
  while (parent_id) {
    const TaskIdPair& parent_pair =
        GetTaskIdPairFromTaskContainer(parent_id.value());
    if (parent_pair.current && parent_pair.current != parent_id) {
      // Found a parent slot, but its ID doesn't match what we thought it would
      // be. That means we circled around the circular array, and we can no
      // longer know the ancestry.
      return AncestorStatus::kUnknown;
    }
    if (parent_id.value() == ancestor_id) {
      return AncestorStatus::kAncestor;
    }
    DCHECK(parent_pair.current.value() != current_task_id.value());
    DCHECK(!parent_pair.parent || parent_pair.parent < parent_id);
    parent_id = parent_pair.parent;
  }
  return AncestorStatus::kNotAncestor;
}

std::unique_ptr<TaskAttributionTracker::TaskScope>
TaskAttributionTrackerImpl::CreateTaskScope(
    ScriptState* script_state,
    absl::optional<TaskId> parent_task_id) {
  next_task_id_ = next_task_id_.NextTaskId();
  running_task_id_ = next_task_id_;

  InsertTaskIdPair(next_task_id_, parent_task_id);
  running_task_ids_.push_back(next_task_id_);

  SaveTaskIdStateInV8(script_state, next_task_id_);
  return std::make_unique<TaskScopeImpl>(script_state, this, next_task_id_);
}

void TaskAttributionTrackerImpl::TaskScopeCompleted(ScriptState* script_state,
                                                    TaskId scope_id) {
  DCHECK(!running_task_ids_.IsEmpty());
  DCHECK(running_task_ids_.back() == scope_id);
  running_task_ids_.pop_back();
  if (!running_task_ids_.IsEmpty()) {
    running_task_id_ = running_task_ids_.back();
  } else {
    running_task_id_ = absl::nullopt;
  }
  SaveTaskIdStateInV8(script_state, running_task_id_);
}

void TaskAttributionTrackerImpl::SaveTaskIdStateInV8(
    ScriptState* script_state,
    absl::optional<TaskId> task_id) {
  DCHECK(v8_adapter_);
  v8_adapter_->SetValue(script_state, task_id);
}

// TaskScope's implementation
//////////////////////////////////////
TaskAttributionTrackerImpl::TaskScopeImpl::TaskScopeImpl(
    ScriptState* script_state,
    TaskAttributionTrackerImpl* task_tracker,
    TaskId scope_task_id)
    : task_tracker_(task_tracker),
      scope_task_id_(scope_task_id),
      script_state_(script_state) {}

TaskAttributionTrackerImpl::TaskScopeImpl::~TaskScopeImpl() {
  task_tracker_->TaskScopeCompleted(script_state_, scope_task_id_);
}

// V8Adapter's implementation
//////////////////////////////////////
absl::optional<TaskId> TaskAttributionTrackerImpl::V8Adapter::GetValue(
    ScriptState* script_state) {
  DCHECK(script_state);
  if (!script_state->ContextIsValid()) {
    return absl::nullopt;
  }

  v8::Local<v8::Context> context = script_state->GetContext();
  DCHECK(!context.IsEmpty());
  v8::Local<v8::Value> v8_value =
      context->GetContinuationPreservedEmbedderData();
  if (v8_value->IsNullOrUndefined()) {
    return absl::nullopt;
  }
  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(isolate);
  // If not empty, the value must be a ScriptWrappableTaskId.
  NonThrowableExceptionState exception_state;
  ScriptWrappableTaskId* script_wrappable_task_id =
      NativeValueTraits<ScriptWrappableTaskId>::NativeValue(isolate, v8_value,
                                                            exception_state);
  DCHECK(script_wrappable_task_id);
  return *script_wrappable_task_id;
}

void TaskAttributionTrackerImpl::V8Adapter::SetValue(
    ScriptState* script_state,
    absl::optional<TaskId> task_id) {
  DCHECK(script_state);
  if (!script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();
  DCHECK(!context.IsEmpty());

  if (task_id) {
    ScriptWrappableTaskId* script_wrappable_task_id =
        MakeGarbageCollected<ScriptWrappableTaskId>(task_id.value());
    context->SetContinuationPreservedEmbedderData(
        ToV8Traits<ScriptWrappableTaskId>::ToV8(script_state,
                                                script_wrappable_task_id)
            .ToLocalChecked());
  } else {
    context->SetContinuationPreservedEmbedderData(v8::Local<v8::Value>());
  }
}

}  // namespace blink::scheduler
