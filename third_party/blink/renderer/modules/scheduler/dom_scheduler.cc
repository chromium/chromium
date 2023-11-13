// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"

#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_post_task_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_post_task_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_yield_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_abortsignal_schedulersignalinherit.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_continuation.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

const char DOMScheduler::kSupplementName[] = "DOMScheduler";

DOMScheduler* DOMScheduler::scheduler(ExecutionContext& context) {
  DOMScheduler* scheduler =
      Supplement<ExecutionContext>::From<DOMScheduler>(context);
  if (!scheduler) {
    scheduler = MakeGarbageCollected<DOMScheduler>(&context);
    Supplement<ExecutionContext>::ProvideTo(context, scheduler);
  }
  return scheduler;
}

DOMScheduler::DOMScheduler(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      Supplement<ExecutionContext>(*context),
      fixed_priority_task_signals_(kWebSchedulingPriorityCount) {
  if (context->IsContextDestroyed()) {
    return;
  }
  CHECK(context->GetScheduler());
  CreateFixedPriorityTaskQueues(context, WebSchedulingQueueType::kTaskQueue,
                                fixed_priority_task_queues_);
}

void DOMScheduler::ContextDestroyed() {
  fixed_priority_task_queues_.clear();
  signal_to_task_queue_map_.clear();
}

void DOMScheduler::Trace(Visitor* visitor) const {
  visitor->Trace(fixed_priority_task_queues_);
  visitor->Trace(fixed_priority_continuation_queues_);
  visitor->Trace(fixed_priority_task_signals_);
  visitor->Trace(signal_to_task_queue_map_);
  visitor->Trace(signal_to_continuation_queue_map_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
}

ScriptPromise DOMScheduler::postTask(
    ScriptState* script_state,
    V8SchedulerPostTaskCallback* callback_function,
    SchedulerPostTaskOptions* options,
    ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    // The bindings layer implicitly converts thrown exceptions in
    // promise-returning functions to promise rejections.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current window is detached");
    return ScriptPromise();
  }

  SchedulingState state = GetSchedulingStateFromOptions(
      script_state, options->getSignalOr(nullptr),
      options->hasPriority()
          ? AtomicString(IDLEnumAsString(options->priority()))
          : g_null_atom);
  if (state.abort_source && state.abort_source->aborted()) {
    exception_state.RethrowV8Exception(
        ToV8Traits<IDLAny>::ToV8(script_state,
                                 state.abort_source->reason(script_state))
            .ToLocalChecked());
    return ScriptPromise();
  }

  auto* task_queue =
      GetTaskQueue(state.priority_source, WebSchedulingQueueType::kTaskQueue);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  MakeGarbageCollected<DOMTask>(resolver, callback_function, state.abort_source,
                                state.priority_source, task_queue,
                                base::Milliseconds(options->delay()));
  return resolver->Promise();
}

ScriptPromise DOMScheduler::yield(ScriptState* script_state,
                                  SchedulerYieldOptions* options,
                                  ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current window is detached");
    return ScriptPromise();
  }

  if (fixed_priority_continuation_queues_.empty()) {
    CreateFixedPriorityTaskQueues(GetExecutionContext(),
                                  WebSchedulingQueueType::kContinuationQueue,
                                  fixed_priority_continuation_queues_);
  }

  // Abort and priority can be inherited together or separately. Abort
  // inheritance only depends on the signal option. Signal inheritance implies
  // priority inheritance, but can be overridden by specifying a fixed
  // priority.
  absl::variant<AbortSignal*, InheritOption> signal_option(nullptr);
  if (options->hasSignal()) {
    if (options->signal()->IsSchedulerSignalInherit()) {
      // {signal: "inherit"}
      signal_option = InheritOption::kInherit;
    } else {
      // {signal: signalObject}
      signal_option = options->signal()->GetAsAbortSignal();
    }
  }

  absl::variant<AtomicString, InheritOption> priority_option(g_null_atom);
  if ((options->hasPriority() && options->priority() == "inherit")) {
    // {priority: "inherit"}
    priority_option = InheritOption::kInherit;
  } else if (!options->hasPriority() &&
             absl::holds_alternative<InheritOption>(signal_option)) {
    // {signal: "inherit"} with no priority override.
    priority_option = InheritOption::kInherit;
  } else if (options->hasPriority()) {
    // Priority override.
    priority_option = AtomicString(IDLEnumAsString(options->priority()));
  }

  SchedulingState state = GetSchedulingStateFromOptions(
      script_state, signal_option, priority_option);
  if (state.abort_source && state.abort_source->aborted()) {
    exception_state.RethrowV8Exception(
        ToV8Traits<IDLAny>::ToV8(script_state,
                                 state.abort_source->reason(script_state))
            .ToLocalChecked());
    return ScriptPromise();
  }

  CHECK(state.priority_source);
  auto* task_queue = GetTaskQueue(state.priority_source,
                                  WebSchedulingQueueType::kContinuationQueue);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  MakeGarbageCollected<DOMTaskContinuation>(resolver, state.abort_source,
                                            task_queue);
  return resolver->Promise();
}

scheduler::TaskAttributionIdType DOMScheduler::taskId(
    ScriptState* script_state) {
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  auto* tracker = scheduler->GetTaskAttributionTracker();
  if (!tracker) {
    // Can happen when a feature flag disables TaskAttribution.
    return 0;
  }
  scheduler::TaskAttributionInfo* task =
      scheduler->GetTaskAttributionTracker()->RunningTask(script_state);
  // task cannot be nullptr here, as a task has presumably already ran in order
  // for this API call to be called.
  DCHECK(task);
  return task->Id().value();
}

AtomicString DOMScheduler::isAncestor(
    ScriptState* script_state,
    scheduler::TaskAttributionIdType parentId) {
  scheduler::TaskAttributionTracker::AncestorStatus status =
      scheduler::TaskAttributionTracker::AncestorStatus::kNotAncestor;
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  auto* tracker = scheduler->GetTaskAttributionTracker();
  if (!tracker) {
    // Can happen when a feature flag disables TaskAttribution.
    return AtomicString("unknown");
  }
  status =
      tracker->IsAncestor(script_state, scheduler::TaskAttributionId(parentId));
  switch (status) {
    case scheduler::TaskAttributionTracker::AncestorStatus::kAncestor:
      return AtomicString("ancestor");
    case scheduler::TaskAttributionTracker::AncestorStatus::kNotAncestor:
      return AtomicString("not ancestor");
    case scheduler::TaskAttributionTracker::AncestorStatus::kUnknown:
      return AtomicString("unknown");
  }
  NOTREACHED();
  return AtomicString("not reached");
}

void DOMScheduler::CreateFixedPriorityTaskQueues(
    ExecutionContext* context,
    WebSchedulingQueueType queue_type,
    FixedPriorityTaskQueueVector& task_queues) {
  FrameOrWorkerScheduler* scheduler = context->GetScheduler();
  for (size_t i = 0; i < kWebSchedulingPriorityCount; i++) {
    auto priority = static_cast<WebSchedulingPriority>(i);
    std::unique_ptr<WebSchedulingTaskQueue> task_queue =
        scheduler->CreateWebSchedulingTaskQueue(queue_type, priority);
    task_queues.push_back(
        MakeGarbageCollected<DOMTaskQueue>(std::move(task_queue), priority));
  }
}

DOMScheduler::DOMTaskQueue* DOMScheduler::CreateDynamicPriorityTaskQueue(
    DOMTaskSignal* signal,
    WebSchedulingQueueType queue_type) {
  FrameOrWorkerScheduler* scheduler = GetExecutionContext()->GetScheduler();
  CHECK(scheduler);
  WebSchedulingPriority priority =
      WebSchedulingPriorityFromString(signal->priority());
  std::unique_ptr<WebSchedulingTaskQueue> task_queue =
      scheduler->CreateWebSchedulingTaskQueue(queue_type, priority);
  CHECK(task_queue);
  auto* dom_task_queue =
      MakeGarbageCollected<DOMTaskQueue>(std::move(task_queue), priority);
  auto* handle = signal->AddPriorityChangeAlgorithm(WTF::BindRepeating(
      &DOMScheduler::OnPriorityChange, WrapWeakPersistent(this),
      WrapWeakPersistent(signal), WrapWeakPersistent(dom_task_queue)));
  dom_task_queue->SetPriorityChangeHandle(handle);
  return dom_task_queue;
}

DOMScheduler::SchedulingState DOMScheduler::GetSchedulingStateFromOptions(
    ScriptState* script_state,
    absl::variant<AbortSignal*, InheritOption> signal_option,
    absl::variant<AtomicString, InheritOption> priority_option) {
  // `inherited_abort_source` and `inherited_priority_source` will be null if no
  // inheritance was specified or there's nothing to inherit, e.g. yielding from
  // a non-postTask task.
  // Note: The inherited signals will be from the original task, i.e. they don't
  // get reset by continuations.
  AbortSignal* inherited_abort_source = nullptr;
  DOMTaskSignal* inherited_priority_source = nullptr;
  if (absl::holds_alternative<InheritOption>(signal_option) ||
      absl::holds_alternative<InheritOption>(priority_option)) {
    CHECK(RuntimeEnabledFeatures::SchedulerYieldEnabled(
        ExecutionContext::From(script_state)));
    if (auto* inherited_state =
            ScriptWrappableTaskState::GetCurrent(script_state)) {
      inherited_abort_source = inherited_state->GetAbortSource();
      inherited_priority_source = inherited_state->GetPrioritySource();
    }
  }

  SchedulingState result;
  result.abort_source = absl::holds_alternative<AbortSignal*>(signal_option)
                            ? absl::get<AbortSignal*>(signal_option)
                            : inherited_abort_source;
  if (result.abort_source && result.abort_source->aborted()) {
    // This task or continuation won't be scheduled, so short-circuit.
    return result;
  }

  if (absl::holds_alternative<InheritOption>(priority_option)) {
    result.priority_source = inherited_priority_source;
  } else if (absl::get<AtomicString>(priority_option) != g_null_atom) {
    // The priority option overrides the signal for priority.
    result.priority_source = GetFixedPriorityTaskSignal(
        script_state, WebSchedulingPriorityFromString(
                          absl::get<AtomicString>(priority_option)));
  } else if (IsA<DOMTaskSignal>(absl::get<AbortSignal*>(signal_option))) {
    result.priority_source =
        To<DOMTaskSignal>(absl::get<AbortSignal*>(signal_option));
  }
  // `priority_source` is null if there was nothing to inherit or no signal or
  // priority was specified.
  if (!result.priority_source) {
    result.priority_source =
        GetFixedPriorityTaskSignal(script_state, kDefaultPriority);
  }
  return result;
}

DOMTaskSignal* DOMScheduler::GetFixedPriorityTaskSignal(
    ScriptState* script_state,
    WebSchedulingPriority priority) {
  wtf_size_t index = static_cast<wtf_size_t>(priority);
  if (!fixed_priority_task_signals_[index]) {
    auto* signal = DOMTaskSignal::CreateFixedPriorityTaskSignal(
        script_state, WebSchedulingPriorityToString(priority));
    CHECK(signal->HasFixedPriority());
    fixed_priority_task_signals_[index] = signal;
  }
  return fixed_priority_task_signals_[index].Get();
}

DOMScheduler::DOMTaskQueue* DOMScheduler::GetTaskQueue(
    DOMTaskSignal* task_signal,
    WebSchedulingQueueType queue_type) {
  if (task_signal->HasFixedPriority()) {
    auto priority = WebSchedulingPriorityFromString(task_signal->priority());
    return queue_type == WebSchedulingQueueType::kTaskQueue
               ? fixed_priority_task_queues_[static_cast<wtf_size_t>(priority)]
               : fixed_priority_continuation_queues_[static_cast<wtf_size_t>(
                     priority)];
  } else {
    SignalToTaskQueueMap& queue_map =
        queue_type == WebSchedulingQueueType::kTaskQueue
            ? signal_to_task_queue_map_
            : signal_to_continuation_queue_map_;
    if (queue_map.Contains(task_signal)) {
      return queue_map.at(task_signal);
    }
    // We haven't seen this task signal before, so create a task queue for it.
    auto* dom_task_queue =
        CreateDynamicPriorityTaskQueue(task_signal, queue_type);
    queue_map.insert(task_signal, dom_task_queue);
    return dom_task_queue;
  }
}

void DOMScheduler::OnPriorityChange(DOMTaskSignal* signal,
                                    DOMTaskQueue* task_queue) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    return;
  }
  DCHECK(signal);
  task_queue->SetPriority(WebSchedulingPriorityFromString(signal->priority()));
}

DOMScheduler::DOMTaskQueue::DOMTaskQueue(
    std::unique_ptr<WebSchedulingTaskQueue> task_queue,
    WebSchedulingPriority priority)
    : web_scheduling_task_queue_(std::move(task_queue)),
      task_runner_(web_scheduling_task_queue_->GetTaskRunner()),
      priority_(priority) {
  DCHECK(task_runner_);
}

void DOMScheduler::DOMTaskQueue::Trace(Visitor* visitor) const {
  visitor->Trace(priority_change_handle_);
}

void DOMScheduler::DOMTaskQueue::SetPriority(WebSchedulingPriority priority) {
  if (priority_ == priority)
    return;
  web_scheduling_task_queue_->SetPriority(priority);
  priority_ = priority;
}

DOMScheduler::DOMTaskQueue::~DOMTaskQueue() = default;

}  // namespace blink
