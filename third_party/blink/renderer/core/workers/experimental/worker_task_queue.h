// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_WORKER_TASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_WORKER_TASK_QUEUE_H_

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class AbortSignal;
class Document;
class ExceptionState;
class ExecutionContext;
class ScriptState;
class ScriptValue;
class Task;

class CORE_EXPORT WorkerTaskQueue : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WorkerTaskQueue* Create(ExecutionContext*,
                                 const String&,
                                 ExceptionState&);
  ~WorkerTaskQueue() override = default;

  ScriptPromise postFunction(ScriptState*,
                             const ScriptValue& task,
                             AbortSignal*,
                             const Vector<ScriptValue>& arguments);

  Task* postTask(ScriptState*,
                 const ScriptValue& task,
                 const Vector<ScriptValue>& arguments);

  void Trace(blink::Visitor*) override;

 private:
  WorkerTaskQueue(Document*, TaskType);

  Member<Document> document_;
  const TaskType task_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_WORKER_TASK_QUEUE_H_
