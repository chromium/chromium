/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/messaging/message_port.h"

#include <memory>

#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message_mojom_traits.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

MessagePort::MessagePort(ExecutionContext& execution_context)
    : ActiveScriptWrappable<MessagePort>({}),
      ExecutionContextLifecycleObserver(execution_context.IsContextDestroyed()
                                            ? nullptr
                                            : &execution_context),
      // Ports in a destroyed context start out in a closed state.
      closed_(execution_context.IsContextDestroyed()),
      task_runner_(execution_context.GetTaskRunner(TaskType::kPostedMessage)) {}

MessagePort::~MessagePort() {
  DCHECK(!started_ || !IsEntangled());
  if (!IsNeutered()) {
    // Disentangle before teardown. The MessagePortDescriptor will blow up if it
    // hasn't had its underlying handle returned to it before teardown.
    Disentangle();
  }
}

void MessagePort::postMessage(ScriptState* script_state,
                              const ScriptValue& message,
                              HeapVector<ScriptValue>& transfer,
                              ExceptionState& exception_state) {
  PostMessageOptions* options = PostMessageOptions::Create();
  if (!transfer.empty())
    options->setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void MessagePort::postMessage(ScriptState* script_state,
                              const ScriptValue& message,
                              const PostMessageOptions* options,
                              ExceptionState& exception_state) {
  if (!IsEntangled())
    return;
  DCHECK(GetExecutionContext());
  DCHECK(!IsNeutered());

  BlinkTransferableMessage msg;
  Transferables transferables;
  msg.message = PostMessageHelper::SerializeMessageByMove(
      script_state->GetIsolate(), message, options, transferables,
      exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(msg.message);

  // Make sure we aren't connected to any of the passed-in ports.
  for (unsigned i = 0; i < transferables.message_ports.size(); ++i) {
    if (transferables.message_ports[i] == this) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "Port at index " + String::Number(i) + " contains the source port.");
      return;
    }
  }
  msg.ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), transferables.message_ports,
      exception_state);
  if (exception_state.HadException())
    return;
  msg.user_activation = PostMessageHelper::CreateUserActivationSnapshot(
      GetExecutionContext(), options);

  msg.sender_origin =
      GetExecutionContext()->GetSecurityOrigin()->IsolatedCopy();

  ThreadDebugger* debugger = ThreadDebugger::From(script_state->GetIsolate());
  if (debugger)
    msg.sender_stack_trace_id = debugger->StoreCurrentStackTrace("postMessage");

  msg.sender_agent_cluster_id = GetExecutionContext()->GetAgentClusterID();
  msg.locked_to_sender_agent_cluster = msg.message->IsLockedToAgentCluster();

  auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker();
  // Only pass the parent task ID if we're in the main world, as isolated world
  // task tracking is not yet supported.
  if (tracker && script_state->World().IsMainWorld()) {
    msg.parent_task_id = tracker->RunningTaskAttributionId(script_state);
  }

  mojo::Message mojo_message =
      mojom::blink::TransferableMessage::WrapAsMessage(std::move(msg));
  connector_->Accept(&mojo_message);
}

MessagePortChannel MessagePort::Disentangle() {
  DCHECK(!IsNeutered());
  port_.GiveDisentangledHandle(connector_->PassMessagePipe());
  connector_ = nullptr;
  return MessagePortChannel(std::move(port_));
}

void MessagePort::start() {
  // Do nothing if we've been cloned or closed.
  if (!IsEntangled())
    return;

  DCHECK(GetExecutionContext());
  if (started_)
    return;

  started_ = true;
  connector_->StartReceiving(task_runner_);
}

void MessagePort::close() {
  if (closed_)
    return;
  // A closed port should not be neutered, so rather than merely disconnecting
  // from the mojo message pipe, also entangle with a new dangling message pipe.
  if (!IsNeutered()) {
    Disentangle().ReleaseHandle();
    MessagePortDescriptorPair pipe;
    Entangle(pipe.TakePort0());
  }
  closed_ = true;
}

void MessagePort::Entangle(MessagePortDescriptor port) {
  DCHECK(port.IsValid());
  DCHECK(!connector_);

  // If the context was already destroyed, there is no reason to actually
  // entangle the port and create a Connector. No messages will ever be able to
  // be sent or received anyway, as StartReceiving will never be called.
  if (!GetExecutionContext())
    return;

  port_ = std::move(port);
  connector_ = std::make_unique<mojo::Connector>(
      port_.TakeHandleToEntangle(GetExecutionContext()),
      mojo::Connector::SINGLE_THREADED_SEND);
  // The raw `this` is safe despite `this` being a garbage collected object
  // because we make sure that:
  // 1. This object will not be garbage collected while it is connected and
  //    the execution context is not destroyed, and
  // 2. when the execution context is destroyed, the connector_ is reset.
  connector_->set_incoming_receiver(this);
  connector_->set_connection_error_handler(
      WTF::BindOnce(&MessagePort::close, WrapWeakPersistent(this)));
}

void MessagePort::Entangle(MessagePortChannel channel) {
  Entangle(channel.ReleaseHandle());
}

const AtomicString& MessagePort::InterfaceName() const {
  return event_target_names::kMessagePort;
}

bool MessagePort::HasPendingActivity() const {
  // The spec says that entangled message ports should always be treated as if
  // they have a strong reference.
  // We'll also stipulate that the queue needs to be open (if the app drops its
  // reference to the port before start()-ing it, then it's not really entangled
  // as it's unreachable).
  return started_ && IsEntangled();
}

Vector<MessagePortChannel> MessagePort::DisentanglePorts(
    ExecutionContext* context,
    const MessagePortArray& ports,
    ExceptionState& exception_state) {
  if (!ports.size())
    return Vector<MessagePortChannel>();

  HeapHashSet<Member<MessagePort>> visited;
  bool has_closed_ports = false;

  // Walk the incoming array - if there are any duplicate ports, or null ports
  // or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
  for (unsigned i = 0; i < ports.size(); ++i) {
    MessagePort* port = ports[i];
    if (!port || port->IsNeutered() || visited.Contains(port)) {
      String type;
      if (!port)
        type = "null";
      else if (port->IsNeutered())
        type = "already neutered";
      else
        type = "a duplicate";
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "Port at index " + String::Number(i) + " is " + type + ".");
      return Vector<MessagePortChannel>();
    }
    if (port->closed_)
      has_closed_ports = true;
    visited.insert(port);
  }

  UseCounter::Count(context, WebFeature::kMessagePortsTransferred);
  if (has_closed_ports)
    UseCounter::Count(context, WebFeature::kMessagePortTransferClosedPort);

  // Passed-in ports passed validity checks, so we can disentangle them.
  Vector<MessagePortChannel> channels;
  channels.ReserveInitialCapacity(ports.size());
  for (unsigned i = 0; i < ports.size(); ++i)
    channels.push_back(ports[i]->Disentangle());
  return channels;
}

MessagePortArray* MessagePort::EntanglePorts(
    ExecutionContext& context,
    Vector<MessagePortChannel> channels) {
  return EntanglePorts(context,
                       WebVector<MessagePortChannel>(std::move(channels)));
}

MessagePortArray* MessagePort::EntanglePorts(
    ExecutionContext& context,
    WebVector<MessagePortChannel> channels) {
  // https://html.spec.whatwg.org/C/#message-ports
  // |ports| should be an empty array, not null even when there is no ports.
  wtf_size_t count = base::checked_cast<wtf_size_t>(channels.size());
  MessagePortArray* port_array = MakeGarbageCollected<MessagePortArray>(count);
  for (wtf_size_t i = 0; i < count; ++i) {
    auto* port = MakeGarbageCollected<MessagePort>(context);
    port->Entangle(std::move(channels[i]));
    (*port_array)[i] = port;
  }
  return port_array;
}

::MojoHandle MessagePort::EntangledHandleForTesting() const {
  return connector_->handle().value();
}

void MessagePort::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTarget::Trace(visitor);
}

bool MessagePort::Accept(mojo::Message* mojo_message) {
  TRACE_EVENT0("blink", "MessagePort::Accept");

  BlinkTransferableMessage message;
  if (!mojom::blink::TransferableMessage::DeserializeFromMessage(
          std::move(*mojo_message), &message)) {
    return false;
  }

  ExecutionContext* context = GetExecutionContext();
  // WorkerGlobalScope::close() in Worker onmessage handler should prevent
  // the next message from dispatching.
  if (auto* scope = DynamicTo<WorkerGlobalScope>(context)) {
    if (scope->IsClosing())
      return true;
  }

  Event* evt = CreateMessageEvent(message);
  // This unique_ptr is here to ensure that the TaskScope remains alive for the
  // lifetime of this function.
  std::unique_ptr<scheduler::TaskAttributionTracker::TaskScope>
      task_attribution_scope;
  if (message.sender_origin &&
      message.sender_origin->IsSameOriginWith(context->GetSecurityOrigin()) &&
      context->IsSameAgentCluster(message.sender_agent_cluster_id) &&
      context->IsWindow()) {
    // TODO(crbug.com/1351643): It is not correct to assume we're running in the
    // main world here. Even though we're in Window, this could be running in an
    // isolated world context. At the same time, even if we are running in such
    // a context, the TaskScope creation here will not leak any meaningful
    // information to that world. At worst, TaskAttributionTracking will return
    // the wrong ancestor for tasks initiated by MessagePort::PostMessage inside
    // of extensions. TaskScope is using the v8::Context in order to store the
    // current TaskAttributionId in the context's
    // EmbedderPreservedContinuationData, and it's only used later for
    // attributing continuations to that original task.
    // We cannot check `content->GetCurrentWorld()->IsMainWorld()` here, as the
    // v8::Context may still be empty (and hence
    // ExecutionContext::GetCurrentWorld returns null).
    if (ScriptState* script_state =
            ToScriptState(context, DOMWrapperWorld::MainWorld())) {
      DCHECK(ThreadScheduler::Current());
      if (auto* tracker =
              ThreadScheduler::Current()->GetTaskAttributionTracker()) {
        task_attribution_scope = tracker->CreateTaskScope(
            script_state, message.parent_task_id,
            scheduler::TaskAttributionTracker::TaskScopeType::kPostMessage);
      }
    }
  }

  v8::Isolate* isolate = context->GetIsolate();
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (debugger)
    debugger->ExternalAsyncTaskStarted(message.sender_stack_trace_id);
  DispatchEvent(*evt);
  if (debugger)
    debugger->ExternalAsyncTaskFinished(message.sender_stack_trace_id);
  return true;
}

Event* MessagePort::CreateMessageEvent(BlinkTransferableMessage& message) {
  ExecutionContext* context = GetExecutionContext();
  // Dispatch a messageerror event when the target is a remote origin that is
  // not allowed to access the message's data.
  if (message.message->IsOriginCheckRequired()) {
    const SecurityOrigin* target_origin = context->GetSecurityOrigin();
    if (!message.sender_origin ||
        !message.sender_origin->IsSameOriginWith(target_origin)) {
      return MessageEvent::CreateError();
    }
  }

  if (message.locked_to_sender_agent_cluster) {
    DCHECK(message.sender_agent_cluster_id);
    if (!context->IsSameAgentCluster(message.sender_agent_cluster_id)) {
      UseCounter::Count(
          context,
          WebFeature::kMessageEventSharedArrayBufferDifferentAgentCluster);
      return MessageEvent::CreateError();
    }
    const SecurityOrigin* target_origin = context->GetSecurityOrigin();
    if (!message.sender_origin ||
        !message.sender_origin->IsSameOriginWith(target_origin)) {
      UseCounter::Count(
          context, WebFeature::kMessageEventSharedArrayBufferSameAgentCluster);
    } else {
      UseCounter::Count(context,
                        WebFeature::kMessageEventSharedArrayBufferSameOrigin);
    }
  }

  if (!message.message->CanDeserializeIn(context))
    return MessageEvent::CreateError();

  MessagePortArray* ports = MessagePort::EntanglePorts(
      *GetExecutionContext(), std::move(message.ports));
  UserActivation* user_activation = nullptr;
  if (message.user_activation) {
    user_activation = MakeGarbageCollected<UserActivation>(
        message.user_activation->has_been_active,
        message.user_activation->was_active);
  }

  return MessageEvent::Create(ports, std::move(message.message),
                              user_activation);
}

}  // namespace blink
