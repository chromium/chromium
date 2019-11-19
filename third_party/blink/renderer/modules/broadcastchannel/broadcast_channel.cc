// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/broadcastchannel/broadcast_channel.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// To ensure proper ordering of messages send to/from multiple BroadcastChannel
// instances in the same thread this uses one BroadcastChannelService
// connection as basis for all connections to channels from the same thread. The
// actual connections used to send/receive messages are then created using
// associated interfaces, ensuring proper message ordering.
mojo::Remote<mojom::blink::BroadcastChannelProvider>&
GetThreadSpecificProvider() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojo::Remote<mojom::blink::BroadcastChannelProvider>>,
      provider, ());
  if (!provider.IsSet()) {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        provider->BindNewPipeAndPassReceiver());
  }
  return *provider;
}

}  // namespace

// static
BroadcastChannel* BroadcastChannel::Create(ExecutionContext* execution_context,
                                           const String& name,
                                           ExceptionState& exception_state) {
  // Record BroadcastChannel usage in third party context. Don't record if the
  // frame is same-origin to the top frame, or if we can't tell whether the
  // frame was ever cross-origin or not.
  Document* document = DynamicTo<Document>(execution_context);
  if (document && document->TopFrameOrigin() &&
      !document->TopFrameOrigin()->CanAccess(document->GetSecurityOrigin())) {
    UseCounter::Count(document, WebFeature::kThirdPartyBroadcastChannel);
  }

  if (execution_context->GetSecurityOrigin()->IsOpaque()) {
    // TODO(mek): Decide what to do here depending on
    // https://github.com/whatwg/html/issues/1319
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Can't create BroadcastChannel in an opaque origin");
    return nullptr;
  }
  return MakeGarbageCollected<BroadcastChannel>(execution_context, name);
}

BroadcastChannel::~BroadcastChannel() = default;

void BroadcastChannel::Dispose() {
  close();
}

void BroadcastChannel::postMessage(const ScriptValue& message,
                                   ExceptionState& exception_state) {
  if (!receiver_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Channel is closed");
    return;
  }
  scoped_refptr<SerializedScriptValue> value = SerializedScriptValue::Serialize(
      message.GetIsolate(), message.V8Value(),
      SerializedScriptValue::SerializeOptions(), exception_state);
  if (exception_state.HadException())
    return;

  BlinkCloneableMessage msg;
  msg.message = std::move(value);
  msg.sender_origin =
      GetExecutionContext()->GetSecurityOrigin()->IsolatedCopy();
  remote_client_->OnMessage(std::move(msg));
}

void BroadcastChannel::close() {
  remote_client_.reset();
  if (receiver_.is_bound())
    receiver_.reset();
  feature_handle_for_scheduler_.reset();
}

const AtomicString& BroadcastChannel::InterfaceName() const {
  return event_target_names::kBroadcastChannel;
}

bool BroadcastChannel::HasPendingActivity() const {
  return receiver_.is_bound() && HasEventListeners(event_type_names::kMessage);
}

void BroadcastChannel::ContextDestroyed(ExecutionContext*) {
  close();
}

void BroadcastChannel::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

void BroadcastChannel::OnMessage(BlinkCloneableMessage message) {
  // Queue a task to dispatch the event.
  MessageEvent* event;
  if (!message.locked_agent_cluster_id ||
      GetExecutionContext()->IsSameAgentCluster(
          *message.locked_agent_cluster_id)) {
    event = MessageEvent::Create(
        nullptr, std::move(message.message),
        GetExecutionContext()->GetSecurityOrigin()->ToString());
  } else {
    event = MessageEvent::CreateError(
        GetExecutionContext()->GetSecurityOrigin()->ToString());
  }
  // <specdef
  // href="https://html.spec.whatwg.org/multipage/web-messaging.html#dom-broadcastchannel-postmessage">
  // <spec>The tasks must use the DOM manipulation task source, and, for
  // those where the event loop specified by the target BroadcastChannel
  // object's BroadcastChannel settings object is a window event loop,
  // must be associated with the responsible document specified by that
  // target BroadcastChannel object's BroadcastChannel settings object.
  // </spec>
  EnqueueEvent(*event, TaskType::kDOMManipulation);
}

void BroadcastChannel::OnError() {
  close();
}

BroadcastChannel::BroadcastChannel(ExecutionContext* execution_context,
                                   const String& name)
    : ContextLifecycleObserver(execution_context),
      origin_(execution_context->GetSecurityOrigin()),
      name_(name),
      feature_handle_for_scheduler_(
          execution_context->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kBroadcastChannel,
              {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {
  mojo::Remote<mojom::blink::BroadcastChannelProvider>& provider =
      GetThreadSpecificProvider();

  // Note: We cannot associate per-frame task runner here, but postTask
  //       to it manually via EnqueueEvent, since the current expectation
  //       is to receive messages even after close for which queued before
  //       close.
  //       https://github.com/whatwg/html/issues/1319
  //       Relying on Mojo binding will cancel the enqueued messages
  //       at close().

  // Local BroadcastChannelClient for messages send from the browser to this
  // channel and Remote BroadcastChannelClient for messages send from this
  // channel to the browser.
  provider->ConnectToChannel(origin_, name_,
                             receiver_.BindNewEndpointAndPassRemote(),
                             remote_client_.BindNewEndpointAndPassReceiver());
  receiver_.set_disconnect_handler(
      WTF::Bind(&BroadcastChannel::OnError, WrapWeakPersistent(this)));
  remote_client_.set_disconnect_handler(
      WTF::Bind(&BroadcastChannel::OnError, WrapWeakPersistent(this)));
}

}  // namespace blink
