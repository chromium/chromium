// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_post_task_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_task_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"

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
      Supplement<ExecutionContext>(*context) {
  if (context->IsContextDestroyed())
    return;
  DCHECK(context->GetScheduler());
  CreateFixedPriorityTaskQueues(context);
}

void DOMScheduler::ContextDestroyed() {
  fixed_priority_task_queues_.clear();
  signal_to_task_queue_map_.clear();
}

void DOMScheduler::Trace(Visitor* visitor) const {
  visitor->Trace(fixed_priority_task_queues_);
  visitor->Trace(signal_to_task_queue_map_);
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
  if (options->hasSignal() && options->signal()->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "The task was aborted");
    return ScriptPromise();
  }

  // Always honor the priority and the task signal if given.
  DOMTaskSignal* task_signal = nullptr;
  if (!options->hasPriority() && options->hasSignal() &&
      IsA<DOMTaskSignal>(options->signal())) {
    // If only a signal is given, and it is a TaskSignal rather than an
    // basic AbortSignal, use it.
    task_signal = To<DOMTaskSignal>(options->signal());

    // If we haven't seen this TaskSignal before, then it was created by a
    // TaskController and has modifiable priority.
    if (!signal_to_task_queue_map_.Contains(task_signal))
      CreateTaskQueueFor(task_signal);
  } else {
    // Otherwise, construct an implicit TaskSignal. Have it follow the signal
    // if it was given, so that it can still honor any aborts, but have it
    // at the fixed given priority (or default if none was specified).
    //
    // An implicit TaskSignal, in addition to being read-only, won't own its
    // own task queue. Instead, it will use the appropriate task queue from
    // |fixed_priority_task_queues_|.
    WebSchedulingPriority priority =
        options->hasPriority() ? WebSchedulingPriorityFromString(AtomicString(
                                     IDLEnumAsString(options->priority())))
                               : kDefaultPriority;
    task_signal = CreateTaskSignalFor(priority);
    if (options->hasSignal())
      task_signal->Follow(script_state, options->signal());
  }

  DCHECK(task_signal);
  DCHECK(signal_to_task_queue_map_.Contains(task_signal));
  auto* task_runner = signal_to_task_queue_map_.at(task_signal)
                          ->GetWebSchedulingTaskQueue()
                          ->GetTaskRunner()
                          .get();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  MakeGarbageCollected<DOMTask>(resolver, callback_function, task_signal,
                                task_runner,
                                base::Milliseconds(options->delay()));
  return resolver->Promise();
}

DOMTaskSignal* DOMScheduler::currentTaskSignal(ScriptState* script_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return nullptr;

  v8::Local<v8::Value> embedder_data =
      script_state->GetContext()->GetContinuationPreservedEmbedderData();
  if (V8TaskSignal::HasInstance(embedder_data, script_state->GetIsolate()))
    return V8TaskSignal::ToImpl(v8::Local<v8::Object>::Cast(embedder_data));

  // TODO(shaseley): consider returning nullptr to reduce memory churn and so we
  // don't need to insert a mapping every time. This might also be beneficial
  // from on the client side to determine if the task was scheduled or not.
  return CreateTaskSignalFor(kDefaultPriority);
}

void DOMScheduler::CreateFixedPriorityTaskQueues(ExecutionContext* context) {
  FrameOrWorkerScheduler* scheduler = context->GetScheduler();
  for (size_t i = 0; i < kWebSchedulingPriorityCount; i++) {
    std::unique_ptr<WebSchedulingTaskQueue> task_queue =
        scheduler->CreateWebSchedulingTaskQueue(
            static_cast<WebSchedulingPriority>(i));
    fixed_priority_task_queues_.push_back(
        MakeGarbageCollected<DOMTaskQueue>(std::move(task_queue)));
  }
}

DOMTaskSignal* DOMScheduler::CreateTaskSignalFor(
    WebSchedulingPriority priority) {
  DOMTaskSignal* signal = MakeGarbageCollected<DOMTaskSignal>(
      GetSupplementable(), WebSchedulingPriorityToString(priority));
  DOMTaskQueue* task_queue =
      fixed_priority_task_queues_[static_cast<int>(priority)];
  signal_to_task_queue_map_.insert(signal, task_queue);
  return signal;
}

void DOMScheduler::CreateTaskQueueFor(DOMTaskSignal* signal) {
  FrameOrWorkerScheduler* scheduler = GetExecutionContext()->GetScheduler();
  DCHECK(scheduler);
  WebSchedulingPriority priority =
      WebSchedulingPriorityFromString(signal->priority());
  std::unique_ptr<WebSchedulingTaskQueue> task_queue =
      scheduler->CreateWebSchedulingTaskQueue(priority);
  signal_to_task_queue_map_.insert(
      signal, MakeGarbageCollected<DOMTaskQueue>(std::move(task_queue)));
  signal->AddPriorityChangeAlgorithm(WTF::Bind(&DOMScheduler::OnPriorityChange,
                                               WrapWeakPersistent(this),
                                               WrapWeakPersistent(signal)));
}

void DOMScheduler::OnPriorityChange(DOMTaskSignal* signal) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;
  DCHECK(signal);
  DCHECK(signal_to_task_queue_map_.Contains(signal));
  DOMTaskQueue* task_queue = signal_to_task_queue_map_.at(signal);
  task_queue->GetWebSchedulingTaskQueue()->SetPriority(
      WebSchedulingPriorityFromString(signal->priority()));
}

DOMScheduler::DOMTaskQueue::DOMTaskQueue(
    std::unique_ptr<WebSchedulingTaskQueue> task_queue)
    : web_scheduling_task_queue_(std::move(task_queue)) {}

DOMScheduler::DOMTaskQueue::~DOMTaskQueue() = default;

}  // namespace blink
