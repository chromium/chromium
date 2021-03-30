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

namespace {

static ScriptPromise RejectPromiseImmediately(ExceptionState& exception_state) {
  // The bindings layer implicitly converts thrown exceptions in
  // promise-returning functions to promise rejections.
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Current window is detached");
  return ScriptPromise();
}

}  // namespace

const char DOMScheduler::kSupplementName[] = "DOMScheduler";

DOMScheduler* DOMScheduler::scheduler(LocalDOMWindow& window) {
  DOMScheduler* scheduler =
      Supplement<LocalDOMWindow>::From<DOMScheduler>(window);
  if (!scheduler) {
    scheduler = MakeGarbageCollected<DOMScheduler>(&window);
    Supplement<LocalDOMWindow>::ProvideTo(window, scheduler);
  }
  return scheduler;
}

DOMScheduler::DOMScheduler(LocalDOMWindow* window)
    : ExecutionContextLifecycleObserver(window),
      Supplement<LocalDOMWindow>(*window) {
  if (window->IsContextDestroyed())
    return;
  DCHECK(window->GetScheduler());
  DCHECK(window->GetScheduler()->ToFrameScheduler());
  CreateFixedPriorityTaskQueues(window);
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
  Supplement<LocalDOMWindow>::Trace(visitor);
}

ScriptPromise DOMScheduler::postTask(
    ScriptState* script_state,
    V8SchedulerPostTaskCallback* callback_function,
    SchedulerPostTaskOptions* options,
    ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return RejectPromiseImmediately(exception_state);
  if (options->signal() && options->signal()->aborted())
    return RejectPromiseImmediately(exception_state);

  // Always honor the priority and the task signal if given.
  DOMTaskSignal* task_signal = nullptr;
  // TODO(crbug.com/1070871): Stop using APIs for non-null.
  if (!options->hasPriorityNonNull() && IsA<DOMTaskSignal>(options->signal())) {
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
        options->hasPriorityNonNull()
            ? WebSchedulingPriorityFromString(
                  AtomicString(IDLEnumAsString(options->priorityNonNull())))
            : kDefaultPriority;
    task_signal = CreateTaskSignalFor(priority);
    if (options->signal())
      task_signal->Follow(options->signal());
  }

  DCHECK(task_signal);
  DCHECK(signal_to_task_queue_map_.Contains(task_signal));
  auto* task_runner = signal_to_task_queue_map_.at(task_signal)
                          ->GetWebSchedulingTaskQueue()
                          ->GetTaskRunner()
                          .get();

  // TODO(shaseley): We need to figure out the behavior we want for delay. For
  // now, we use behavior that is very similar to setTimeout: negative delays
  // are treated as 0, and we use the Blink scheduler's delayed task behavior.
  // We don't, however, adjust the timeout based on nested calls (yet) or clamp
  // the value to a minimal delay.
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(std::max(0, options->delay()));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  MakeGarbageCollected<DOMTask>(this, resolver, callback_function, task_signal,
                                task_runner, delay);
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

void DOMScheduler::CreateFixedPriorityTaskQueues(LocalDOMWindow* window) {
  FrameScheduler* scheduler = window->GetScheduler()->ToFrameScheduler();
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
  FrameScheduler* scheduler =
      GetExecutionContext()->GetScheduler()->ToFrameScheduler();
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
