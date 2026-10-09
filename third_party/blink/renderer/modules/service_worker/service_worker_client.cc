// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_client.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/messaging/delegated_capability.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_client_lifecycle_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_client_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_context_frame_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

ServiceWorkerClient::ServiceWorkerClient(
    const mojom::blink::ServiceWorkerClientInfo& info)
    : uuid_(info.client_uuid),
      url_(info.url.GetString()),
      type_(info.client_type),
      frame_type_(info.frame_type),
      lifecycle_state_(info.lifecycle_state) {}

ServiceWorkerClient::~ServiceWorkerClient() = default;

V8ClientType ServiceWorkerClient::type() const {
  switch (type_) {
    case mojom::ServiceWorkerClientType::kWindow:
      return V8ClientType(V8ClientType::Enum::kWindow);
    case mojom::ServiceWorkerClientType::kDedicatedWorker:
      return V8ClientType(V8ClientType::Enum::kWorker);
    case mojom::ServiceWorkerClientType::kSharedWorker:
      return V8ClientType(V8ClientType::Enum::kSharedworker);
    case mojom::ServiceWorkerClientType::kAll:
      // Should not happen.
      break;
  }
  NOTREACHED();
}

V8ContextFrameType ServiceWorkerClient::frameType(
    ScriptState* script_state) const {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kServiceWorkerClientFrameType);
  switch (frame_type_) {
    case mojom::RequestContextFrameType::kAuxiliary:
      return V8ContextFrameType(V8ContextFrameType::Enum::kAuxiliary);
    case mojom::RequestContextFrameType::kNested:
      return V8ContextFrameType(V8ContextFrameType::Enum::kNested);
    case mojom::RequestContextFrameType::kNone:
      return V8ContextFrameType(V8ContextFrameType::Enum::kNone);
    case mojom::RequestContextFrameType::kTopLevel:
      return V8ContextFrameType(V8ContextFrameType::Enum::kTopLevel);
  }
  NOTREACHED();
}

V8ClientLifecycleState ServiceWorkerClient::lifecycleState() const {
  switch (lifecycle_state_) {
    case mojom::ServiceWorkerClientLifecycleState::kActive:
      return V8ClientLifecycleState(V8ClientLifecycleState::Enum::kActive);
    case mojom::ServiceWorkerClientLifecycleState::kFrozen:
      return V8ClientLifecycleState(V8ClientLifecycleState::Enum::kFrozen);
  }
  NOTREACHED();
}

void ServiceWorkerClient::postMessage(ScriptState* script_state,
                                      const ScriptValue& message,
                                      HeapVector<ScriptObject> transfer,
                                      ExceptionState& exception_state) {
  PostMessageOptions* options = PostMessageOptions::Create();
  if (!transfer.empty())
    options->setTransfer(std::move(transfer));
  postMessage(script_state, message, options, exception_state);
}

void ServiceWorkerClient::postMessage(ScriptState* script_state,
                                      const ScriptValue& message,
                                      const PostMessageOptions* options,
                                      ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  Transferables transferables;

  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByCopy(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);

  auto* sw_scope = To<ServiceWorkerGlobalScope>(context);
  bool has_activation = sw_scope->IsWindowInteractionAllowed();

  mojom::blink::DelegatedCapability delegated_capability =
      mojom::blink::DelegatedCapability::kNone;
  if (options->hasDelegate()) {
    delegated_capability = PostMessageHelper::MapStringToDelegatedCapability(
        options->delegate(), context, exception_state);
    if (exception_state.HadException()) {
      return;
    }
    if (delegated_capability != mojom::blink::DelegatedCapability::kNone) {
      CHECK_EQ(delegated_capability, mojom::blink::DelegatedCapability::kPopup);
      if (type_ != mojom::blink::ServiceWorkerClientType::kWindow) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotSupportedError,
            "Delegation of 'popup' is only supported for WindowClient.");
        return;
      }
      // Transient popup capability is tracked on ServiceWorkerGlobalScope.
      // See crbug.com/542314185.
      if (!sw_scope->ConsumePopupCapabilityDelegation()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kNotAllowedError,
            "Delegation is not allowed without transient user activation.");
        return;
      }
    }
  }

  BlinkTransferableMessage msg;
  msg.message = serialized_message;
  msg.delegated_capability = delegated_capability;
  if (options->includeUserActivation()) {
    msg.user_activation =
        mojom::blink::UserActivationSnapshot::New(false,  // sticky
                                                  has_activation);
  }
  msg.sender_origin = context->GetSecurityOrigin()->IsolatedCopy();
  msg.ports = MessagePort::DisentanglePorts(
      context, transferables.message_ports, exception_state);
  if (exception_state.HadException())
    return;

  msg.sender_agent_cluster_id = context->GetAgentClusterID();
  msg.locked_to_sender_agent_cluster = msg.message->IsLockedToAgentCluster();

  To<ServiceWorkerGlobalScope>(context)
      ->GetServiceWorkerHost()
      ->PostMessageToClient(uuid_, std::move(msg));
}

}  // namespace blink
