// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_attribution_tracker_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_info_impl.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_task_state.h"
#include "third_party/blink/renderer/core/scheduler/web_scheduling_task_state.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "v8/include/v8-promise.h"

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
    case TaskAttributionTracker::TaskScopeType::kMiscEvent:
      return ProtoType::TASK_SCOPE_MISC_EVENT;
    case TaskAttributionTracker::TaskScopeType::kMicrotask:
      return ProtoType::TASK_SCOPE_MICROTASK;
  }
}

int64_t TaskStateIdForTracing(TaskAttributionTaskState* state) {
  TaskAttributionInfo* info = state ? state->GetTaskAttributionInfo() : nullptr;
  return info ? info->Id().value() : 0;
}

void BeginBlinkTaskStateTrace(TaskAttributionTaskState* task_state,
                              TaskAttributionTaskState* previous_task_state,
                              TaskAttributionTracker::TaskScopeType type) {
  TRACE_EVENT_BEGIN(
      TaskAttributionTracker::kTracingCategory, "BlinkTaskState",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_blink_task_scope();
        data->set_type(ToProtoEnum(type));
        data->set_scope_task_id(TaskStateIdForTracing(task_state));
        data->set_running_task_id_to_be_restored(
            TaskStateIdForTracing(previous_task_state));
      });
}

void EndBlinkTaskStateTrace() {
  TRACE_EVENT_END(TaskAttributionTracker::kTracingCategory);
}

void TaskAttributionPromiseHook(v8::PromiseHookType type,
                                v8::Local<v8::Promise> promise,
                                v8::Local<v8::Value> parent) {
  if (type == v8::PromiseHookType::kBefore) {
    BeginBlinkTaskStateTrace(
        TaskAttributionTaskState::GetCurrent(v8::Isolate::GetCurrent()),
        /*previous_task_state=*/nullptr,
        TaskAttributionTracker::TaskScopeType::kMicrotask);
  } else if (type == v8::PromiseHookType::kAfter) {
    EndBlinkTaskStateTrace();
  }
}

}  // namespace

// static
std::unique_ptr<TaskAttributionTracker> TaskAttributionTrackerImpl::Create(
    v8::Isolate* isolate) {
  return base::WrapUnique(new TaskAttributionTrackerImpl(isolate));
}

TaskAttributionTrackerImpl::TaskAttributionTrackerImpl(v8::Isolate* isolate)
    : isolate_(isolate) {
  CHECK(isolate_);
  if (base::FeatureList::IsEnabled(
          features::kTaskAttributionTraceMicrotaskTaskState)) {
    // Register a tracing state observer unless we're running in a test without
    // a task runner. Also note that setting the promise hook must be done on
    // the main thread, since otherwise the `isolate_` might not be the current
    // isolate, so we need to use an async observer.
    if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
      trace_event::AddTraceSessionObserver(this);
    }
    // If tracing was already enabled, we won't get a callback, so check if we
    // need to register the promise hook.
    if (IsTracingCategoryEnabled()) {
      isolate_->SetPromiseHook(TaskAttributionPromiseHook);
    }
  }
}

TaskAttributionTrackerImpl::~TaskAttributionTrackerImpl() {
  if (base::FeatureList::IsEnabled(
          features::kTaskAttributionTraceMicrotaskTaskState)) {
    // Note that it's safe to remove a non-existent observer.
    trace_event::RemoveTraceSessionObserver(this);
  }
}

scheduler::TaskAttributionInfo* TaskAttributionTrackerImpl::CurrentTaskState()
    const {
  if (TaskAttributionTaskState* task_state =
          TaskAttributionTaskState::GetCurrent(isolate_)) {
    return task_state->GetTaskAttributionInfo();
  }
  // There won't be any task state in CPED outside of a `TaskScope` or microtask
  // checkpoint, or if there is nothing to propagate.
  return nullptr;
}

std::optional<TaskAttributionTracker::TaskScope>
TaskAttributionTrackerImpl::SetCurrentTaskStateIfTopLevel(
    TaskAttributionInfo* task_state,
    TaskScopeType type) {
  // Don't propagate `task_state` if JavaScript is running, e.g. if dispatching
  // a synchronous event.
  if (!task_state || isolate_->InContext()) {
    return std::nullopt;
  }
  return SetCurrentTaskStateImpl(UnsafeTo<TaskAttributionInfoImpl>(task_state),
                                 type);
}

TaskAttributionTracker::TaskScope
TaskAttributionTrackerImpl::SetCurrentTaskState(
    WebSchedulingTaskState* task_state,
    TaskScopeType type) {
  CHECK(task_state);
  // Web scheduling tasks are top-level entry points that should not run in
  // nested event loops, so there should be no current task state.
  DCHECK(!TaskAttributionTaskState::GetCurrent(isolate_));
  return SetCurrentTaskStateImpl(task_state, type);
}

TaskAttributionTracker::TaskScope
TaskAttributionTrackerImpl::SetTaskStateVariable(
    SoftNavigationContext* soft_navigation_context) {
  auto* task_state = MakeGarbageCollected<TaskAttributionInfoImpl>(
      next_task_id_, soft_navigation_context);
  next_task_id_ = next_task_id_.NextId();
  return SetCurrentTaskStateImpl(task_state, TaskScopeType::kSoftNavigation);
}

TaskAttributionTracker::TaskScope
TaskAttributionTrackerImpl::SetCurrentTaskStateImpl(
    TaskAttributionTaskState* task_state,
    TaskScopeType type) {
  TaskAttributionTaskState* previous_task_state =
      TaskAttributionTaskState::GetCurrent(isolate_);
  if (task_state != previous_task_state) {
    TaskAttributionTaskState::SetCurrent(isolate_, task_state);
  }
  BeginBlinkTaskStateTrace(task_state, previous_task_state, type);
  return TaskScope(this, previous_task_state);
}

void TaskAttributionTrackerImpl::OnTaskScopeDestroyed(
    const TaskScope& task_scope) {
  TaskAttributionTaskState::SetCurrent(isolate_,
                                       task_scope.previous_task_state_);
  EndBlinkTaskStateTrace();
}

std::optional<TaskAttributionId>
TaskAttributionTrackerImpl::AsyncSameDocumentNavigationStarted() {
  scheduler::TaskAttributionInfo* task_state = CurrentTaskState();
  if (!task_state) {
    return std::nullopt;
  }
  same_document_navigation_tasks_.push_back(task_state);
  return task_state->Id();
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

void TaskAttributionTrackerImpl::OnStart(
    const perfetto::DataSourceBase::StartArgs&) {
  if (IsTracingCategoryEnabled()) {
    isolate_->SetPromiseHook(TaskAttributionPromiseHook);
  }
}

void TaskAttributionTrackerImpl::OnStop(
    const perfetto::DataSourceBase::StopArgs& args) {
  bool should_stop = !base::trace_event::IsCategoryEnabledOnStop(
      PERFETTO_GET_CATEGORY_INDEX(kTracingCategory), args);
  if (should_stop) {
    isolate_->SetPromiseHook(nullptr);
  }
}

void TaskAttributionTrackerImpl::BeginMicrotaskTrace() {
  BeginBlinkTaskStateTrace(TaskAttributionTaskState::GetCurrent(isolate_),
                           /*previous_task_state=*/nullptr,
                           TaskAttributionTracker::TaskScopeType::kMicrotask);
}

void TaskAttributionTrackerImpl::EndMicrotaskTrace() {
  EndBlinkTaskStateTrace();
}

}  // namespace blink::scheduler
