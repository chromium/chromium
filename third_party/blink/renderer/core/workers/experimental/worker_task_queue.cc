// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/worker_task_queue.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/workers/experimental/task.h"
#include "third_party/blink/renderer/core/workers/experimental/thread_pool.h"

namespace blink {

WorkerTaskQueue* WorkerTaskQueue::Create(ExecutionContext* context,
                                         const String& type,
                                         ExceptionState& exception_state) {
  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The context provided is invalid.");
    return nullptr;
  }

  auto* document = DynamicTo<Document>(context);
  if (!document) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "WorkerTaskQueue can only be constructed from a document.");
    return nullptr;
  }
  DCHECK(type == "user-interaction" || type == "background");
  TaskType task_type = type == "user-interaction" ? TaskType::kUserInteraction
                                                  : TaskType::kIdleTask;
  return new WorkerTaskQueue(document, task_type);
}

WorkerTaskQueue::WorkerTaskQueue(Document* document, TaskType task_type)
    : document_(document), task_type_(task_type) {}

ScriptPromise WorkerTaskQueue::postFunction(
    ScriptState* script_state,
    const ScriptValue& task,
    AbortSignal* signal,
    const Vector<ScriptValue>& arguments) {
  DCHECK(document_->IsContextThread());
  DCHECK(task.IsFunction());

  ThreadPoolTask* thread_pool_task = new ThreadPoolTask(
      ThreadPool::From(*document_), script_state, task, arguments, task_type_);
  if (signal) {
    signal->AddAlgorithm(
        WTF::Bind(&ThreadPoolTask::Cancel, thread_pool_task->GetWeakPtr()));
  }
  return thread_pool_task->GetResult();
}

Task* WorkerTaskQueue::postTask(ScriptState* script_state,
                                const ScriptValue& function,
                                const Vector<ScriptValue>& arguments) {
  DCHECK(document_->IsContextThread());
  DCHECK(function.IsFunction());

  ThreadPoolTask* thread_pool_task =
      new ThreadPoolTask(ThreadPool::From(*document_), script_state, function,
                         arguments, task_type_);
  return new Task(thread_pool_task);
}

void WorkerTaskQueue::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(document_);
}

}  // namespace blink
