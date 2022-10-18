// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_post_message_helper.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/transferables.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
BlinkTransferableMessage PortalPostMessageHelper::CreateMessage(
    ScriptState* script_state,
    const ScriptValue& message,
    const PostMessageOptions* options,
    ExceptionState& exception_state) {
  BlinkTransferableMessage transferable_message;
  Transferables transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByMove(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return {};
  DCHECK(serialized_message);
  transferable_message.message = serialized_message;

  // Disentangle the port in preparation for sending it to the remote context.
  auto* execution_context = ExecutionContext::From(script_state);
  transferable_message.ports = MessagePort::DisentanglePorts(
      execution_context, transferables.message_ports, exception_state);
  if (exception_state.HadException())
    return {};

  transferable_message.sender_origin =
      execution_context->GetSecurityOrigin()->IsolatedCopy();

  transferable_message.sender_agent_cluster_id =
      execution_context->GetAgentClusterID();

  if (ThreadDebugger* debugger =
          ThreadDebugger::From(script_state->GetIsolate())) {
    transferable_message.sender_stack_trace_id =
        debugger->StoreCurrentStackTrace("postMessage");
  }

  transferable_message.user_activation =
      PostMessageHelper::CreateUserActivationSnapshot(execution_context,
                                                      options);

  return transferable_message;
}

// static
void PortalPostMessageHelper::CreateAndDispatchMessageEvent(
    EventTarget* event_target,
    BlinkTransferableMessage message,
    scoped_refptr<const SecurityOrigin> source_origin) {
  DCHECK(event_target->ToPortalHost() ||
         IsA<HTMLPortalElement>(event_target->ToNode()));
  DCHECK(source_origin->IsSameOriginWith(
      event_target->GetExecutionContext()->GetSecurityOrigin()));

  ExecutionContext* context = event_target->GetExecutionContext();
  MessageEvent* event;
  if (message.message->CanDeserializeIn(context)) {
    UserActivation* user_activation = nullptr;
    if (message.user_activation) {
      user_activation = MakeGarbageCollected<UserActivation>(
          message.user_activation->has_been_active,
          message.user_activation->was_active);
    }
    event = MessageEvent::Create(message.ports, message.message,
                                 source_origin->ToString(), String(),
                                 event_target, user_activation);
    event->EntangleMessagePorts(context);
  } else {
    event = MessageEvent::CreateError(source_origin->ToString(), event_target);
  }

  ThreadDebugger* debugger = MainThreadDebugger::Instance();
  if (debugger)
    debugger->ExternalAsyncTaskStarted(message.sender_stack_trace_id);
  event_target->DispatchEvent(*event);
  if (debugger)
    debugger->ExternalAsyncTaskFinished(message.sender_stack_trace_id);
}

}  // namespace blink
