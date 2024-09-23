// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/threaded_object_proxy_base.h"

#include <memory>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/threaded_messaging_proxy_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void ThreadedObjectProxyBase::CountFeature(WebFeature feature) {
  if (!GetParentExecutionContextTaskRunners()) {
    DCHECK(GetParentAgentGroupTaskRunner());
    return;
  }

  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&ThreadedMessagingProxyBase::CountFeature,
                          MessagingProxyWeakPtr(), feature));
}

void ThreadedObjectProxyBase::CountWebDXFeature(WebDXFeature feature) {
  if (!GetParentExecutionContextTaskRunners()) {
    DCHECK(GetParentAgentGroupTaskRunner());
    return;
  }

  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&ThreadedMessagingProxyBase::CountWebDXFeature,
                          MessagingProxyWeakPtr(), feature));
}

void ThreadedObjectProxyBase::ReportConsoleMessage(
    mojom::ConsoleMessageSource source,
    mojom::ConsoleMessageLevel level,
    const String& message,
    SourceLocation* location) {
  if (!GetParentExecutionContextTaskRunners()) {
    DCHECK(GetParentAgentGroupTaskRunner());
    return;
  }

  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&ThreadedMessagingProxyBase::ReportConsoleMessage,
                          MessagingProxyWeakPtr(), source, level, message,
                          location->Clone()));
}

void ThreadedObjectProxyBase::DidCloseWorkerGlobalScope() {
  if (!GetParentExecutionContextTaskRunners()) {
    DCHECK(GetParentAgentGroupTaskRunner());

    PostCrossThreadTask(
        *GetParentAgentGroupTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&ThreadedMessagingProxyBase::TerminateGlobalScope,
                            MessagingProxyWeakPtr()));

    return;
  }

  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&ThreadedMessagingProxyBase::TerminateGlobalScope,
                          MessagingProxyWeakPtr()));
}

void ThreadedObjectProxyBase::DidTerminateWorkerThread() {
  if (!GetParentExecutionContextTaskRunners()) {
    DCHECK(GetParentAgentGroupTaskRunner());

    PostCrossThreadTask(
        *GetParentAgentGroupTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&ThreadedMessagingProxyBase::WorkerThreadTerminated,
                            MessagingProxyWeakPtr()));

    return;
  }

  // This will terminate the MessagingProxy.
  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&ThreadedMessagingProxyBase::WorkerThreadTerminated,
                          MessagingProxyWeakPtr()));
}

ParentExecutionContextTaskRunners*
ThreadedObjectProxyBase::GetParentExecutionContextTaskRunners() {
  return parent_execution_context_task_runners_.Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
ThreadedObjectProxyBase::GetParentAgentGroupTaskRunner() {
  return parent_agent_group_task_runner_;
}

ThreadedObjectProxyBase::ThreadedObjectProxyBase(
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners,
    scoped_refptr<base::SingleThreadTaskRunner> parent_agent_group_task_runner)
    : parent_execution_context_task_runners_(
          parent_execution_context_task_runners),
      parent_agent_group_task_runner_(parent_agent_group_task_runner) {}

}  // namespace blink
