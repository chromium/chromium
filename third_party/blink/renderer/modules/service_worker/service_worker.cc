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
#include <utility>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_state.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_service_worker_state.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
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
  if (!transfer.empty())
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

  msg.sender_agent_cluster_id = GetExecutionContext()->GetAgentClusterID();
  msg.locked_to_sender_agent_cluster = msg.message->IsLockedToAgentCluster();

  // Defer postMessage() from a prerendered page until page activation.
  // https://wicg.github.io/nav-speculation/prerendering.html#patch-service-workers
  if (GetExecutionContext()->IsWindow()) {
    Document* document = To<LocalDOMWindow>(GetExecutionContext())->document();
    if (document->IsPrerendering()) {
      document->AddPostPrerenderingActivationStep(
          WTF::BindOnce(&ServiceWorker::PostMessageInternal,
                        WrapWeakPersistent(this), std::move(msg)));
      return;
    }
  }

  PostMessageInternal(std::move(msg));
}

void ServiceWorker::PostMessageInternal(BlinkTransferableMessage message) {
  host_->PostMessageToServiceWorker(std::move(message));
}

ScriptPromise<IDLUndefined> ServiceWorker::InternalsTerminate(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  host_->TerminateForTesting(WTF::BindOnce(
      [](ScriptPromiseResolver<IDLUndefined>* resolver) {
        resolver->Resolve();
      },
      WrapPersistent(resolver)));
  return promise;
}

void ServiceWorker::StateChanged(mojom::blink::ServiceWorkerState new_state) {
  state_ = new_state;
  DispatchEvent(*Event::Create(event_type_names::kStatechange));
}

String ServiceWorker::scriptURL() const {
  return url_.GetString();
}

V8ServiceWorkerState ServiceWorker::state() const {
  switch (state_) {
    case mojom::blink::ServiceWorkerState::kParsed:
      return V8ServiceWorkerState(V8ServiceWorkerState::Enum::kParsed);
    case mojom::blink::ServiceWorkerState::kInstalling:
      return V8ServiceWorkerState(V8ServiceWorkerState::Enum::kInstalling);
    case mojom::blink::ServiceWorkerState::kInstalled:
      return V8ServiceWorkerState(V8ServiceWorkerState::Enum::kInstalled);
    case mojom::blink::ServiceWorkerState::kActivating:
      return V8ServiceWorkerState(V8ServiceWorkerState::Enum::kActivating);
    case mojom::blink::ServiceWorkerState::kActivated:
      return V8ServiceWorkerState(V8ServiceWorkerState::Enum::kActivated);
    case mojom::blink::ServiceWorkerState::kRedundant:
      return V8ServiceWorkerState(V8ServiceWorkerState::Enum::kRedundant);
  }
  NOTREACHED();
}

ServiceWorker* ServiceWorker::From(
    ExecutionContext* context,
    mojom::blink::ServiceWorkerObjectInfoPtr info) {
  if (!info)
    return nullptr;
  return From(context, WebServiceWorkerObjectInfo(info->version_id, info->state,
                                                  info->url,
                                                  std::move(info->host_remote),
                                                  std::move(info->receiver)));
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

  return ServiceWorkerContainer::From(*To<LocalDOMWindow>(context))
      ->GetOrCreateServiceWorker(std::move(info));
}

bool ServiceWorker::HasPendingActivity() const {
  if (was_stopped_)
    return false;
  return state_ != mojom::blink::ServiceWorkerState::kRedundant;
}

void ServiceWorker::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {}

void ServiceWorker::ContextDestroyed() {
  was_stopped_ = true;
}

ServiceWorker::ServiceWorker(ExecutionContext* execution_context,
                             WebServiceWorkerObjectInfo info)
    : AbstractWorker(execution_context),
      ActiveScriptWrappable<ServiceWorker>({}),
      url_(info.url),
      state_(info.state),
      host_(execution_context),
      receiver_(this, execution_context) {
  DCHECK_NE(mojom::blink::kInvalidServiceWorkerVersionId, info.version_id);
  host_.Bind(
      std::move(info.host_remote),
      execution_context->GetTaskRunner(blink::TaskType::kInternalDefault));
  receiver_.Bind(
      mojo::PendingAssociatedReceiver<mojom::blink::ServiceWorkerObject>(
          std::move(info.receiver)),
      execution_context->GetTaskRunner(blink::TaskType::kInternalDefault));
}

ServiceWorker::~ServiceWorker() = default;

void ServiceWorker::Trace(Visitor* visitor) const {
  visitor->Trace(host_);
  visitor->Trace(receiver_);
  AbstractWorker::Trace(visitor);
}

}  // namespace blink
