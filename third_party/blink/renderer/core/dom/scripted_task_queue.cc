// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_task_queue.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_task_queue_post_callback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"

namespace blink {

namespace {

class TaskQueuePostCallbackWrapper {
 public:
  static std::unique_ptr<TaskQueuePostCallbackWrapper> Create(
      int id,
      ScriptedTaskQueue* task_queue) {
    return std::unique_ptr<TaskQueuePostCallbackWrapper>(
        new TaskQueuePostCallbackWrapper(id, task_queue));
  }

  void TaskFired() {
    if (task_queue_)
      task_queue_->CallbackFired(id_);
  }

 private:
  TaskQueuePostCallbackWrapper(int id, ScriptedTaskQueue* task_queue)
      : id_(id), task_queue_(task_queue) {}

  int id_;
  WeakPersistent<ScriptedTaskQueue> task_queue_;
};

}  // namespace

class ScriptedTaskQueue::WrappedCallback
    : public GarbageCollected<WrappedCallback> {
  WTF_MAKE_NONCOPYABLE(WrappedCallback);

 public:
  WrappedCallback(V8TaskQueuePostCallback* callback,
                  ScriptPromiseResolver* resolver)
      : callback_(callback), resolver_(resolver) {}

  void Trace(Visitor* visitor) {
    visitor->Trace(callback_);
    visitor->Trace(resolver_);
  }

  void Invoke() {
    callback_->InvokeAndReportException(nullptr);
    resolver_->Resolve();
  }

  void Reject() { resolver_->Reject(); }

 private:
  TraceWrapperMember<V8TaskQueuePostCallback> callback_;
  Member<ScriptPromiseResolver> resolver_;
};

ScriptedTaskQueue::ScriptedTaskQueue(ExecutionContext* context,
                                     TaskType task_type)
    : PausableObject(context) {
  task_runner_ = GetExecutionContext()->GetTaskRunner(task_type);
  PauseIfNeeded();
}

void ScriptedTaskQueue::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_tasks_);
  ScriptWrappable::Trace(visitor);
  PausableObject::Trace(visitor);
}

ScriptPromise ScriptedTaskQueue::postTask(ScriptState* script_state,
                                          V8TaskQueuePostCallback* callback,
                                          AbortSignal* signal) {
  CallbackId id = next_callback_id_++;

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  if (signal) {
    if (signal->aborted()) {
      resolver->Reject();
      return resolver->Promise();
    }

    signal->AddAlgorithm(
        WTF::Bind(&ScriptedTaskQueue::AbortTask, WrapPersistent(this), id));
  }

  pending_tasks_.Set(id, new WrappedCallback(callback, resolver));

  auto callback_wrapper = TaskQueuePostCallbackWrapper::Create(id, this);
  task_runner_->PostTask(FROM_HERE,
                         WTF::Bind(&TaskQueuePostCallbackWrapper::TaskFired,
                                   std::move(callback_wrapper)));

  return resolver->Promise();
}

void ScriptedTaskQueue::CallbackFired(CallbackId id) {
  if (paused_) {
    paused_tasks_.push_back(id);
    return;
  }

  auto task_iter = pending_tasks_.find(id);
  if (task_iter == pending_tasks_.end())
    return;

  task_iter->value->Invoke();
  // Can't use the iterator here since running the task
  // might invalidate it.
  pending_tasks_.erase(id);
}

void ScriptedTaskQueue::AbortTask(CallbackId id) {
  auto task_iter = pending_tasks_.find(id);
  if (task_iter == pending_tasks_.end())
    return;

  task_iter->value->Reject();
  pending_tasks_.erase(id);
}

void ScriptedTaskQueue::ContextDestroyed(ExecutionContext*) {
  pending_tasks_.clear();
  paused_tasks_.clear();
}

void ScriptedTaskQueue::Pause() {
  paused_ = true;
}

void ScriptedTaskQueue::Unpause() {
  paused_ = false;

  for (auto& task_id : paused_tasks_) {
    auto callback_wrapper = TaskQueuePostCallbackWrapper::Create(task_id, this);
    task_runner_->PostTask(FROM_HERE,
                           WTF::Bind(&TaskQueuePostCallbackWrapper::TaskFired,
                                     std::move(callback_wrapper)));
  }

  paused_tasks_.clear();
}

}  // namespace blink
