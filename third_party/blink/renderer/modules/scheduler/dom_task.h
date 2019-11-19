// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class DOMTaskQueue;
class ScriptState;
class ScriptValue;
class V8Function;

class MODULES_EXPORT DOMTask : public ScriptWrappable,
                               ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMTask);

 public:
  // Creating a task also causes the DOMTask to be scheduled.
  DOMTask(DOMTaskQueue*,
          ExecutionContext*,
          V8Function*,
          const HeapVector<ScriptValue>& args,
          base::TimeDelta delay);

  // Task IDL Interface.
  AtomicString priority() const;
  AtomicString status() const;
  void cancel(ScriptState*);
  ScriptPromise result(ScriptState*);

  // Move this DOMTask to a different DOMTaskQueue. If the task is already
  // enqueued in the given task queue, this method has no effect.
  void MoveTo(DOMTaskQueue*);

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(Visitor*) override;

 private:
  // The Status associated with the task lifecycle. Tasks are "pending" before
  // they run, transition to "running" during execution, and end in "completed"
  // when execution completes.
  enum class Status {
    kPending,
    kRunning,
    kCanceled,
    kCompleted,
  };

  // Cancel a pending task. This will update the |status_| to kCanceled, and the
  // task will no longer be runnable.
  void CancelPendingTask();

  // Set |status_| and validate the state transition.
  void SetTaskStatus(Status);

  // Schedule the task. If |delay| is 0, the task will be queued immediately,
  // otherwise it's queued after |delay|.
  void Schedule(base::TimeDelta delay);

  // Entry point for running this DOMTask's |callback_|.
  void Invoke();
  // Internal step of Invoke that handles invoking the callback, including
  // catching any errors and retrieving the result.
  void InvokeInternal(ScriptState*);

  // Resolve |result_promise_| if both (a) the task has finished running (with
  // or without an error) or has been canceled, and (b) task.result has been
  // accessed and returned to userland. If invoked when either of these
  // conditions are not met, this is a no-op.
  void LazilyResolveOrRejectPromiseIfReady(ScriptState*);

  static bool IsValidStatusChange(Status from, Status to);
  static AtomicString TaskStatusToString(Status);

  Status status_;
  TaskHandle task_handle_;
  Member<DOMTaskQueue> task_queue_;
  Member<V8Function> callback_;
  HeapVector<ScriptValue> arguments_;
  const base::TimeDelta delay_;
  // Only set if |delay_| > 0 since Now() can be somewhat expensive. This
  // optimizes the case where there is no delay, which we expect to be the
  // common case.
  const base::TimeTicks queue_time_;

  using TaskResultPromise =
      ScriptPromiseProperty<Member<DOMTask>, ScriptValue, ScriptValue>;
  // Lazily initialized when the task.result property is accessed to avoid the
  // overhead of unnecessarily creating promises for every task if they aren't
  // going to be used.
  Member<TaskResultPromise> result_promise_;

  TraceWrapperV8Reference<v8::Value> result_value_;
  TraceWrapperV8Reference<v8::Value> exception_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_
