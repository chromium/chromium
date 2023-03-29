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
  if (RuntimeEnabledFeatures::SchedulerYieldEnabled(context)) {
    CreateFixedPriorityTaskQueues(context,
                                  WebSchedulingQueueType::kContinuationQueue,
                                  fixed_priority_continuation_queues_);
  }
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

  auto* task_signal = GetTaskSignalFromOptions(
      script_state, exception_state, options->getSignalOr(nullptr),
      options->hasPriority()
          ? AtomicString(IDLEnumAsString(options->priority()))
          : g_null_atom);
  if (exception_state.HadException()) {
    // The given signal was aborted.
    return ScriptPromise();
  }

  CHECK(task_signal);
  auto* task_queue =
      GetTaskQueue(task_signal, WebSchedulingQueueType::kTaskQueue);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  MakeGarbageCollected<DOMTask>(resolver, callback_function, task_signal,
                                task_queue,
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

  auto* task_signal = GetTaskSignalFromOptions(script_state, exception_state,
                                               signal_option, priority_option);
  if (exception_state.HadException()) {
    // The given or inherited signal was aborted.
    return ScriptPromise();
  }

  CHECK(task_signal);
  auto* task_queue =
      GetTaskQueue(task_signal, WebSchedulingQueueType::kContinuationQueue);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  MakeGarbageCollected<DOMTaskContinuation>(resolver, task_signal, task_queue);
  return resolver->Promise();
}

scheduler::TaskAttributionIdType DOMScheduler::taskId(
    ScriptState* script_state) {
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  DCHECK(scheduler->GetTaskAttributionTracker());
  absl::optional<scheduler::TaskAttributionId> task_id =
      scheduler->GetTaskAttributionTracker()->RunningTaskAttributionId(
          script_state);
  // task_id cannot be unset here, as a task has presumably already ran in order
  // for this API call to be called.
  DCHECK(task_id);
  return task_id.value().value();
}

AtomicString DOMScheduler::isAncestor(
    ScriptState* script_state,
    scheduler::TaskAttributionIdType parentId) {
  scheduler::TaskAttributionTracker::AncestorStatus status =
      scheduler::TaskAttributionTracker::AncestorStatus::kNotAncestor;
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  scheduler::TaskAttributionTracker* tracker =
      scheduler->GetTaskAttributionTracker();
  DCHECK(tracker);
  status =
      tracker->IsAncestor(script_state, scheduler::TaskAttributionId(parentId));
  switch (status) {
    case scheduler::TaskAttributionTracker::AncestorStatus::kAncestor:
      return "ancestor";
    case scheduler::TaskAttributionTracker::AncestorStatus::kNotAncestor:
      return "not ancestor";
    case scheduler::TaskAttributionTracker::AncestorStatus::kUnknown:
      return "unknown";
  }
  NOTREACHED();
  return "not reached";
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

DOMTaskSignal* DOMScheduler::GetTaskSignalFromOptions(
    ScriptState* script_state,
    ExceptionState& exception_state,
    absl::variant<AbortSignal*, InheritOption> signal_option,
    absl::variant<AtomicString, InheritOption> priority_option) {
  // `inherited_signal` will be null if no inheritance was specified or there's
  // nothing to inherit, e.g. yielding from a non-postTask task.
  // Note: `inherited_signal` will be the one from the original task, i.e. it
  // doesn't get reset by continuations.
  DOMTaskSignal* inherited_signal = nullptr;
  if (absl::holds_alternative<InheritOption>(signal_option) ||
      absl::holds_alternative<InheritOption>(priority_option)) {
    CHECK(RuntimeEnabledFeatures::SchedulerYieldEnabled(
        ExecutionContext::From(script_state)));
    if (auto* inherited_state =
            ScriptWrappableTaskState::GetCurrent(script_state)) {
      inherited_signal = inherited_state->GetSignal();
    }
  }

  AbortSignal* abort_source =
      absl::holds_alternative<AbortSignal*>(signal_option)
          ? absl::get<AbortSignal*>(signal_option)
          : inherited_signal;
  // Short-circuit things now that we know if `abort_source` is aborted.
  if (abort_source && abort_source->aborted()) {
    exception_state.RethrowV8Exception(
        ToV8Traits<IDLAny>::ToV8(script_state,
                                 abort_source->reason(script_state))
            .ToLocalChecked());
    return nullptr;
  }

  DOMTaskSignal* priority_source = nullptr;
  if (absl::holds_alternative<InheritOption>(priority_option)) {
    priority_source = inherited_signal;
  } else if (absl::get<AtomicString>(priority_option) != g_null_atom) {
    // The priority option overrides the signal for priority.
    priority_source = GetFixedPriorityTaskSignal(
        script_state, WebSchedulingPriorityFromString(
                          absl::get<AtomicString>(priority_option)));
  } else if (IsA<DOMTaskSignal>(absl::get<AbortSignal*>(signal_option))) {
    priority_source = To<DOMTaskSignal>(absl::get<AbortSignal*>(signal_option));
  }
  // `priority_source` is null if there was nothing to inherit or no signal or
  // priority was specified.
  if (!priority_source) {
    priority_source =
        GetFixedPriorityTaskSignal(script_state, kDefaultPriority);
  }

  // The priority and abort sources are the same non-null task signal, so use
  // that signal.
  if (priority_source == abort_source) {
    return priority_source;
  }

  // `priority_source` is already settled for abort and priority, and there is
  // no abort source to combine with. Use `priority_source` rather than creating
  // a new one.
  if (priority_source->HasFixedPriority() && !priority_source->CanAbort() &&
      (!abort_source || !abort_source->CanAbort())) {
    return priority_source;
  }

  // Otherwise there are separate priority and abort sources. Create a
  // composite signal from the sources and use that.
  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    HeapVector<Member<AbortSignal>> abort_source_signals;
    if (abort_source) {
      abort_source_signals.push_back(abort_source);
    }
    return MakeGarbageCollected<DOMTaskSignal>(
        script_state, priority_source->priority(), priority_source,
        abort_source_signals);
  } else {
    // Fall back to use Follow if composition isn't enabled (kill switch path).
    CHECK(priority_source->HasFixedPriority());
    CHECK_EQ(priority_source->GetSignalType(),
             AbortSignal::SignalType::kInternal);
    //  `priority_source` wasn't returned earlier because internal signals are
    //  never settled. Even though an abort algorithm will be added, it's safe
    //  to just use this signal.
    if (!abort_source) {
      return priority_source;
    }
    auto* result_signal = DOMTaskSignal::CreateFixedPriorityTaskSignal(
        script_state, priority_source->priority());
    result_signal->Follow(script_state, abort_source);
    return result_signal;
  }
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
  return fixed_priority_task_signals_[index];
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
