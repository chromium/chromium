/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/workers/dedicated_worker_object_proxy.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

DedicatedWorkerObjectProxy::~DedicatedWorkerObjectProxy() = default;

void DedicatedWorkerObjectProxy::PostMessageToWorkerObject(
    BlinkTransferableMessage message) {
  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kPostedMessage),
      FROM_HERE,
      CrossThreadBindOnce(
          &DedicatedWorkerMessagingProxy::PostMessageToWorkerObject,
          messaging_proxy_weak_ptr_, std::move(message)));
}

void DedicatedWorkerObjectProxy::ProcessMessageFromWorkerObject(
    BlinkTransferableMessage message,
    WorkerThread* worker_thread) {
  To<WorkerGlobalScope>(worker_thread->GlobalScope())
      ->ReceiveMessage(std::move(message));
}

void DedicatedWorkerObjectProxy::ProcessCustomEventFromWorkerObject(
    CustomEventMessage message,
    WorkerThread* worker_thread,
    CrossThreadFunction<Event*(ScriptState*, CustomEventMessage)>
        event_factory_callback,
    CrossThreadFunction<Event*(ScriptState*)> event_factory_error_callback) {
  To<WorkerGlobalScope>(worker_thread->GlobalScope())
      ->ReceiveCustomEvent(std::move(event_factory_callback),
                           std::move(event_factory_error_callback),
                           std::move(message));
}

void DedicatedWorkerObjectProxy::ProcessUnhandledException(
    int exception_id,
    WorkerThread* worker_thread) {
  WorkerGlobalScope* global_scope =
      To<WorkerGlobalScope>(worker_thread->GlobalScope());
  global_scope->ExceptionUnhandled(exception_id);
}

void DedicatedWorkerObjectProxy::ReportException(
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    int exception_id) {
  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerMessagingProxy::DispatchErrorEvent,
                          messaging_proxy_weak_ptr_, error_message,
                          location->Clone(), exception_id));
}

void DedicatedWorkerObjectProxy::DidFailToFetchClassicScript() {
  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalLoading),
      FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerMessagingProxy::DidFailToFetchScript,
                          messaging_proxy_weak_ptr_));
}

void DedicatedWorkerObjectProxy::DidFailToFetchModuleScript() {
  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalLoading),
      FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerMessagingProxy::DidFailToFetchScript,
                          messaging_proxy_weak_ptr_));
}

void DedicatedWorkerObjectProxy::DidEvaluateTopLevelScript(bool success) {
  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalLoading),
      FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerMessagingProxy::DidEvaluateScript,
                          messaging_proxy_weak_ptr_, success));
}

DedicatedWorkerObjectProxy::DedicatedWorkerObjectProxy(
    DedicatedWorkerMessagingProxy* messaging_proxy_weak_ptr,
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners,
    const DedicatedWorkerToken& token)
    : ThreadedObjectProxyBase(parent_execution_context_task_runners,
                              /*parent_agent_group_task_runner=*/nullptr),
      token_(token),
      messaging_proxy_weak_ptr_(messaging_proxy_weak_ptr) {}

CrossThreadWeakPersistent<ThreadedMessagingProxyBase>
DedicatedWorkerObjectProxy::MessagingProxyWeakPtr() {
  return messaging_proxy_weak_ptr_;
}

}  // namespace blink
