// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/dom_scheduler.h"

#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scheduler_post_task_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scheduler_post_task_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_task_priority.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/dom_task.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_continuation.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_info_impl.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {
WebSchedulingPriority WebSchedulingPriorityFromEnum(
    V8TaskPriority::Enum priority) {
  switch (priority) {
    case V8TaskPriority::Enum::kUserBlocking:
      return WebSchedulingPriority::kUserBlockingPriority;
    case V8TaskPriority::Enum::kUserVisible:
      return WebSchedulingPriority::kUserVisiblePriority;
    case V8TaskPriority::Enum::kBackground:
      return WebSchedulingPriority::kBackgroundPriority;
  }
  NOTREACHED();
}
V8TaskPriority::Enum V8TaskEnumFromWebSchedulingPriority(
    WebSchedulingPriority priority) {
  switch (priority) {
    case WebSchedulingPriority::kUserBlockingPriority:
      return V8TaskPriority::Enum::kUserBlocking;
    case WebSchedulingPriority::kUserVisiblePriority:
      return V8TaskPriority::Enum::kUserVisible;
    case WebSchedulingPriority::kBackgroundPriority:
      return V8TaskPriority::Enum::kBackground;
  }
  NOTREACHED();
}
}  // namespace

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

ScriptPromise<IDLAny> DOMScheduler::postTask(
    ScriptState* script_state,
    V8SchedulerPostTaskCallback* callback_function,
    SchedulerPostTaskOptions* options,
    ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    // The bindings layer implicitly converts thrown exceptions in
    // promise-returning functions to promise rejections.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current window is detached");
    return EmptyPromise();
  }

  AbortSignal* signal_option = options->getSignalOr(nullptr);
  if (signal_option && signal_option->aborted()) {
    return ScriptPromise<IDLAny>::Reject(script_state,
                                         signal_option->reason(script_state));
  }

  DOMTaskSignal* priority_source = nullptr;
  if (options->hasPriority()) {
    // The priority option overrides the signal for priority.
    priority_source = GetFixedPriorityTaskSignal(
        script_state,
        WebSchedulingPriorityFromEnum(options->priority().AsEnum()));
  } else if (IsA<DOMTaskSignal>(signal_option)) {
    priority_source = To<DOMTaskSignal>(signal_option);
  }
  // `priority_source` will be null if no signal and no priority were provided,
  // or only a plain `AbortSignal` was provided.
  if (!priority_source) {
    priority_source =
        GetFixedPriorityTaskSignal(script_state, kDefaultPriority);
  }

  auto* task_queue =
      GetTaskQueue(priority_source, WebSchedulingQueueType::kTaskQueue);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  MakeGarbageCollected<DOMTask>(resolver, callback_function, signal_option,
                                priority_source, task_queue,
                                base::Milliseconds(options->delay()));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> DOMScheduler::yield(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current window is detached");
    return EmptyPromise();
  }

  if (fixed_priority_continuation_queues_.empty()) {
    CreateFixedPriorityTaskQueues(GetExecutionContext(),
                                  WebSchedulingQueueType::kContinuationQueue,
                                  fixed_priority_continuation_queues_);
  }

  AbortSignal* abort_source = nullptr;
  DOMTaskSignal* priority_source = nullptr;
  if (auto* inherited_state =
          ScriptWrappableTaskState::GetCurrent(script_state->GetIsolate())) {
    abort_source = inherited_state->WrappedState()->AbortSource();
    priority_source = inherited_state->WrappedState()->PrioritySource();
  }

  if (abort_source && abort_source->aborted()) {
    return ScriptPromise<IDLUndefined>::Reject(
        script_state, abort_source->reason(script_state));
  }

  // `priority_source` will be null if there's nothing to inherit, i.e. yielding
  // from a non-postTask task.
  if (!priority_source) {
    priority_source =
        GetFixedPriorityTaskSignal(script_state, kDefaultPriority);
  }
  auto* task_queue =
      GetTaskQueue(priority_source, WebSchedulingQueueType::kContinuationQueue);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  MakeGarbageCollected<DOMTaskContinuation>(resolver, abort_source, task_queue);
  return resolver->Promise();
}

scheduler::TaskAttributionIdType DOMScheduler::taskId(
    ScriptState* script_state) {
  // `tracker` will be null if TaskAttributionInfrastructureDisabledForTesting
  // is enabled.
  if (auto* tracker =
          scheduler::TaskAttributionTracker::From(script_state->GetIsolate())) {
    // `task_state` is null if there's nothing to propagate.
    if (scheduler::TaskAttributionInfo* task_state = tracker->RunningTask()) {
      return task_state->Id().value();
    }
  }
  return 0;
}

void DOMScheduler::setTaskId(ScriptState* script_state,
                             scheduler::TaskAttributionIdType task_id) {
  if (!scheduler::TaskAttributionTracker::From(script_state->GetIsolate())) {
    // This will be null if TaskAttributionInfrastructureDisabledForTesting is
    // enabled.
    return;
  }
  auto* task_state = MakeGarbageCollected<TaskAttributionInfoImpl>(
      scheduler::TaskAttributionId(task_id),
      /*soft_navigation_context=*/nullptr);
  ScriptWrappableTaskState::SetCurrent(
      script_state, MakeGarbageCollected<ScriptWrappableTaskState>(task_state));
  auto* scheduler = ThreadScheduler::Current()->ToMainThreadScheduler();
  // This test API is only available on the main thread.
  CHECK(scheduler);
  // Clear `task_state` at the end of the current task since there might not be
  // a task scope on the stack to clear it.
  scheduler->ExecuteAfterCurrentTaskForTesting(
      WTF::BindOnce(
          [](ScriptState* script_state) {
            ScriptWrappableTaskState::SetCurrent(script_state, nullptr);
          },
          WrapPersistent(script_state)),
      ExecuteAfterCurrentTaskRestricted{});
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
      WebSchedulingPriorityFromEnum(signal->priority().AsEnum());
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

DOMTaskSignal* DOMScheduler::GetFixedPriorityTaskSignal(
    ScriptState* script_state,
    WebSchedulingPriority priority) {
  wtf_size_t index = static_cast<wtf_size_t>(priority);
  if (!fixed_priority_task_signals_[index]) {
    auto* signal = DOMTaskSignal::CreateFixedPriorityTaskSignal(
        script_state, V8TaskEnumFromWebSchedulingPriority(priority));
    CHECK(signal->HasFixedPriority());
    fixed_priority_task_signals_[index] = signal;
  }
  return fixed_priority_task_signals_[index].Get();
}

DOMScheduler::DOMTaskQueue* DOMScheduler::GetTaskQueue(
    DOMTaskSignal* task_signal,
    WebSchedulingQueueType queue_type) {
  if (task_signal->HasFixedPriority()) {
    auto priority =
        WebSchedulingPriorityFromEnum(task_signal->priority().AsEnum());
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
  task_queue->SetPriority(
      WebSchedulingPriorityFromEnum(signal->priority().AsEnum()));
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
