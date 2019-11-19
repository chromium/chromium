// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

ParentExecutionContextTaskRunners* ParentExecutionContextTaskRunners::Create(
    ExecutionContext* context) {
  DCHECK(context);
  DCHECK(context->IsContextThread());
  return MakeGarbageCollected<ParentExecutionContextTaskRunners>(context);
}

ParentExecutionContextTaskRunners* ParentExecutionContextTaskRunners::Create() {
  return MakeGarbageCollected<ParentExecutionContextTaskRunners>(nullptr);
}

ParentExecutionContextTaskRunners::ParentExecutionContextTaskRunners(
    ExecutionContext* context)
    : ContextLifecycleObserver(context) {
  // For now we only support very limited task types. Sort in the TaskType enum
  // value order.
  for (auto type : {TaskType::kNetworking, TaskType::kPostedMessage,
                    TaskType::kWorkerAnimation, TaskType::kInternalDefault,
                    TaskType::kInternalLoading, TaskType::kInternalTest,
                    TaskType::kInternalMedia, TaskType::kInternalInspector}) {
    auto task_runner = context ? context->GetTaskRunner(type)
                               : Thread::Current()->GetTaskRunner();
    task_runners_.insert(type, std::move(task_runner));
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
ParentExecutionContextTaskRunners::Get(TaskType type) {
  MutexLocker lock(mutex_);
  return task_runners_.at(type);
}

void ParentExecutionContextTaskRunners::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
}

void ParentExecutionContextTaskRunners::ContextDestroyed(ExecutionContext*) {
  MutexLocker lock(mutex_);
  for (auto& entry : task_runners_)
    entry.value = Thread::Current()->GetTaskRunner();
}

}  // namespace blink
