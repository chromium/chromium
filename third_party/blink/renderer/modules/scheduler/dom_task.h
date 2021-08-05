// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {
class DOMTaskSignal;
class ScriptState;
class V8SchedulerPostTaskCallback;

// DOMTask represents a task scheduled via the web scheduling API. It will
// keep itself alive until DOMTask::Invoke is called, which may be after the
// callback's v8 context is invalid, in which case, the task will not be run.
class DOMTask final : public GarbageCollected<DOMTask> {
 public:
  DOMTask(ScriptPromiseResolver*,
          V8SchedulerPostTaskCallback*,
          DOMTaskSignal*,
          base::SingleThreadTaskRunner*,
          base::TimeDelta delay);

  virtual void Trace(Visitor*) const;

 private:
  // Entry point for running this DOMTask's |callback_|.
  void Invoke();
  // Internal step of Invoke that handles invoking the callback, including
  // catching any errors and retrieving the result.
  void InvokeInternal(ScriptState*);
  void OnAbort();

  void RecordTaskStartMetrics();

  TaskHandle task_handle_;
  Member<V8SchedulerPostTaskCallback> callback_;
  Member<ScriptPromiseResolver> resolver_;
  probe::AsyncTaskId async_task_id_;
  Member<DOMTaskSignal> signal_;
  const base::TimeTicks queue_time_;
  const base::TimeDelta delay_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_H_
