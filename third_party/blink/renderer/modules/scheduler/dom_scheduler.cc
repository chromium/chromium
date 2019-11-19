// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_scheduler.h"

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_queue.h"
#include "third_party/blink/renderer/modules/scheduler/scheduler_post_task_options.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {

const char DOMScheduler::kSupplementName[] = "DOMScheduler";

DOMScheduler* DOMScheduler::From(Document& document) {
  DOMScheduler* scheduler = Supplement<Document>::From<DOMScheduler>(document);
  if (!scheduler) {
    scheduler = MakeGarbageCollected<DOMScheduler>(&document);
    Supplement<Document>::ProvideTo(document, scheduler);
  }
  return scheduler;
}

DOMScheduler::DOMScheduler(Document* document)
    : ContextLifecycleObserver(document) {
  if (document->IsContextDestroyed())
    return;
  DCHECK(document->GetScheduler());
  DCHECK(document->GetScheduler()->ToFrameScheduler());
  CreateGlobalTaskQueues(document);
}

void DOMScheduler::ContextDestroyed(ExecutionContext* context) {
  global_task_queues_.clear();
}

void DOMScheduler::Trace(Visitor* visitor) {
  visitor->Trace(global_task_queues_);
  visitor->Trace(current_task_queue_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

DOMTaskQueue* DOMScheduler::currentTaskQueue() {
  // The |current_task_queue_| will only be set if the task currently running
  // was scheduled through window.scheduler. In other cases, e.g. setTimeout,
  // |current_task_queue_| will be nullptr, in which case we return the default
  // priority global task queue.
  if (current_task_queue_)
    return current_task_queue_;
  return GetTaskQueue(WebSchedulingPriority::kDefaultPriority);
}

void DOMScheduler::OnTaskStarted(DOMTaskQueue* task_queue, DOMTask*) {
  // This is not reentrant; tasks are not nested. This allows us to set
  // |current_task_queue_| to nullptr when this task completes.
  DCHECK_EQ(current_task_queue_, nullptr);
  current_task_queue_ = task_queue;
}

void DOMScheduler::OnTaskCompleted(DOMTaskQueue* task_queue, DOMTask*) {
  DCHECK(current_task_queue_);
  DCHECK_EQ(current_task_queue_, task_queue);
  current_task_queue_ = nullptr;
}

DOMTaskQueue* DOMScheduler::getTaskQueue(AtomicString priority) {
  return GetTaskQueue(WebSchedulingPriorityFromString(priority));
}

DOMTaskQueue* DOMScheduler::GetTaskQueue(WebSchedulingPriority priority) {
  if (global_task_queues_.IsEmpty())
    return nullptr;
  return global_task_queues_[static_cast<int>(priority)];
}

DOMTask* DOMScheduler::postTask(V8Function* callback_function,
                                SchedulerPostTaskOptions* options,
                                const HeapVector<ScriptValue>& args) {
  DOMTaskQueue* task_queue = getTaskQueue(AtomicString(options->priority()));
  if (!task_queue)
    return nullptr;
  return task_queue->postTask(callback_function, options, args);
}

void DOMScheduler::CreateGlobalTaskQueues(Document* document) {
  for (size_t i = 0; i < kWebSchedulingPriorityCount; i++) {
    global_task_queues_.push_back(MakeGarbageCollected<DOMTaskQueue>(
        document, static_cast<WebSchedulingPriority>(i), this));
  }
}

}  // namespace blink
