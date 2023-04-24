// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_host.h"

#include <utility>

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window_post_message_options.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/portal/dom_window_portal_host.h"
#include "third_party/blink/renderer/core/html/portal/portal_post_message_helper.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

PortalHost::PortalHost(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), portal_host_(&window) {}

void PortalHost::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(portal_host_);
}

// static
const char PortalHost::kSupplementName[] = "PortalHost";

// static
PortalHost& PortalHost::From(LocalDOMWindow& window) {
  PortalHost* portal_host =
      Supplement<LocalDOMWindow>::From<PortalHost>(window);
  if (!portal_host) {
    portal_host = MakeGarbageCollected<PortalHost>(window);
    Supplement<LocalDOMWindow>::ProvideTo<PortalHost>(window, portal_host);
  }
  return *portal_host;
}

const AtomicString& PortalHost::InterfaceName() const {
  return event_target_names::kPortalHost;
}

ExecutionContext* PortalHost::GetExecutionContext() const {
  return GetSupplementable();
}

PortalHost* PortalHost::ToPortalHost() {
  return this;
}

void PortalHost::OnPortalActivated() {
  portal_host_.reset();
}

void PortalHost::postMessage(ScriptState* script_state,
                             const ScriptValue& message,
                             const PostMessageOptions* options,
                             ExceptionState& exception_state) {
  if (!DOMWindowPortalHost::ShouldExposePortalHost(*GetSupplementable())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The document is no longer inside a portal");
    return;
  }

  BlinkTransferableMessage transferable_message =
      PortalPostMessageHelper::CreateMessage(script_state, message, options,
                                             exception_state);
  if (exception_state.HadException())
    return;

  GetPortalHostInterface().PostMessageToHost(std::move(transferable_message));
}

EventListener* PortalHost::onmessage() {
  return GetAttributeEventListener(event_type_names::kMessage);
}

void PortalHost::setOnmessage(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kMessage, listener);
}

EventListener* PortalHost::onmessageerror() {
  return GetAttributeEventListener(event_type_names::kMessageerror);
}

void PortalHost::setOnmessageerror(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kMessageerror, listener);
}

void PortalHost::ReceiveMessage(
    BlinkTransferableMessage message,
    scoped_refptr<const SecurityOrigin> source_origin) {
  DCHECK(GetSupplementable()->GetFrame()->GetPage()->InsidePortal());
  PortalPostMessageHelper::CreateAndDispatchMessageEvent(
      this, std::move(message), source_origin);
}

mojom::blink::PortalHost& PortalHost::GetPortalHostInterface() {
  if (!portal_host_) {
    DCHECK(GetSupplementable()->GetFrame());
    AssociatedInterfaceProvider* provider =
        GetSupplementable()
            ->GetFrame()
            ->GetRemoteNavigationAssociatedInterfaces();
    provider->GetInterface(
        portal_host_.BindNewEndpointAndPassReceiver(provider->GetTaskRunner()));
  }
  return *portal_host_.get();
}

}  // namespace blink
