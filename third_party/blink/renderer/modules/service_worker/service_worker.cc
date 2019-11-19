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

#include "third_party/blink/renderer/modules/service_worker/service_worker.h"

#include <memory>
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_state.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/messaging/post_message_options.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_container.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

const AtomicString& ServiceWorker::InterfaceName() const {
  return event_target_names::kServiceWorker;
}

void ServiceWorker::postMessage(ScriptState* script_state,
                                const ScriptValue& message,
                                HeapVector<ScriptValue>& transfer,
                                ExceptionState& exception_state) {
  PostMessageOptions* options = PostMessageOptions::Create();
  if (!transfer.IsEmpty())
    options->setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void ServiceWorker::postMessage(ScriptState* script_state,
                                const ScriptValue& message,
                                const PostMessageOptions* options,
                                ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Failed to post a message: No associated provider is available.");
    return;
  }

  Transferables transferables;

  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByCopy(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);

  BlinkTransferableMessage msg;
  msg.message = serialized_message;
  msg.sender_origin =
      GetExecutionContext()->GetSecurityOrigin()->IsolatedCopy();
  msg.ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), transferables.message_ports,
      exception_state);
  if (exception_state.HadException())
    return;

  host_->PostMessageToServiceWorker(std::move(msg));
}

ScriptPromise ServiceWorker::InternalsTerminate(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  host_->TerminateForTesting(
      WTF::Bind([](ScriptPromiseResolver* resolver) { resolver->Resolve(); },
                WrapPersistent(resolver)));
  return promise;
}

void ServiceWorker::StateChanged(mojom::blink::ServiceWorkerState new_state) {
  state_ = new_state;
  this->DispatchEvent(*Event::Create(event_type_names::kStatechange));
}

String ServiceWorker::scriptURL() const {
  return url_.GetString();
}

String ServiceWorker::state() const {
  switch (state_) {
    case mojom::blink::ServiceWorkerState::kParsed:
      return "parsed";
    case mojom::blink::ServiceWorkerState::kInstalling:
      return "installing";
    case mojom::blink::ServiceWorkerState::kInstalled:
      return "installed";
    case mojom::blink::ServiceWorkerState::kActivating:
      return "activating";
    case mojom::blink::ServiceWorkerState::kActivated:
      return "activated";
    case mojom::blink::ServiceWorkerState::kRedundant:
      return "redundant";
  }
  NOTREACHED();
  return g_null_atom;
}

ServiceWorker* ServiceWorker::From(
    ExecutionContext* context,
    mojom::blink::ServiceWorkerObjectInfoPtr info) {
  if (!info)
    return nullptr;
  return From(context,
              WebServiceWorkerObjectInfo(
                  info->version_id, info->state, info->url,
                  info->host_remote.PassHandle(), info->receiver.PassHandle()));
}

ServiceWorker* ServiceWorker::From(ExecutionContext* context,
                                   WebServiceWorkerObjectInfo info) {
  if (!context)
    return nullptr;
  if (info.version_id == mojom::blink::kInvalidServiceWorkerVersionId)
    return nullptr;

  if (auto* scope = DynamicTo<ServiceWorkerGlobalScope>(context)) {
    return scope->GetOrCreateServiceWorker(std::move(info));
  }

  return ServiceWorkerContainer::From(To<Document>(context))
      ->GetOrCreateServiceWorker(std::move(info));
}

bool ServiceWorker::HasPendingActivity() const {
  if (was_stopped_)
    return false;
  return state_ != mojom::blink::ServiceWorkerState::kRedundant;
}

void ServiceWorker::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {}

void ServiceWorker::ContextDestroyed(ExecutionContext*) {
  was_stopped_ = true;
}

ServiceWorker::ServiceWorker(ExecutionContext* execution_context,
                             WebServiceWorkerObjectInfo info)
    : AbstractWorker(execution_context),
      was_stopped_(false),
      url_(info.url),
      state_(info.state) {
  DCHECK_NE(mojom::blink::kInvalidServiceWorkerVersionId, info.version_id);
  host_.Bind(
      mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerObjectHost>(
          std::move(info.host_remote),
          mojom::blink::ServiceWorkerObjectHost::Version_),
      execution_context->GetTaskRunner(blink::TaskType::kInternalDefault));
  receiver_.Bind(
      mojo::PendingAssociatedReceiver<mojom::blink::ServiceWorkerObject>(
          std::move(info.receiver)),
      execution_context->GetTaskRunner(blink::TaskType::kInternalDefault));
}

ServiceWorker::~ServiceWorker() = default;

void ServiceWorker::Dispose() {
  host_.reset();
  receiver_.reset();
}

void ServiceWorker::Trace(blink::Visitor* visitor) {
  AbstractWorker::Trace(visitor);
}

}  // namespace blink
