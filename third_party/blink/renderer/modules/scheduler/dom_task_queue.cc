// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task_queue.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task.h"
#include "third_party/blink/renderer/modules/scheduler/task_queue_post_task_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

DOMTaskQueue::DOMTaskQueue(Document* document,
                           WebSchedulingPriority priority,
                           DOMScheduler* scheduler)
    : ContextLifecycleObserver(document),
      priority_(priority),
      web_scheduling_task_queue_(document->GetScheduler()
                                     ->ToFrameScheduler()
                                     ->CreateWebSchedulingTaskQueue(priority)),
      task_runner_(web_scheduling_task_queue_->GetTaskRunner()),
      scheduler_(scheduler) {
  DCHECK(!document->IsContextDestroyed());
}

void DOMTaskQueue::Trace(Visitor* visitor) {
  visitor->Trace(scheduler_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void DOMTaskQueue::ContextDestroyed(ExecutionContext* context) {
  web_scheduling_task_queue_.reset(nullptr);
}

AtomicString DOMTaskQueue::priority() const {
  return WebSchedulingPriorityToString(priority_);
}

DOMTask* DOMTaskQueue::postTask(V8Function* function,
                                TaskQueuePostTaskOptions* options,
                                const HeapVector<ScriptValue>& args) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return nullptr;

  // TODO(shaseley): We need to figure out the behavior we want for delay. For
  // now, we use behavior that is very similar to setTimeout: negative delays
  // are treated as 0, and we use the Blink scheduler's delayed task behavior.
  // We don't, however, adjust the timeout based on nested calls (yet) or clamp
  // the value to a minimal delay.
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(
      options->delay() > 0 ? options->delay() : 0);

  // For global task queues, we don't need to track the task objects separately;
  // tracking is handled by the |web_scheduling_task_queue_|.
  return MakeGarbageCollected<DOMTask>(this, GetExecutionContext(), function,
                                       args, delay);
}

void DOMTaskQueue::take(DOMTask* task) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;
  task->MoveTo(this);
}

}  // namespace blink
