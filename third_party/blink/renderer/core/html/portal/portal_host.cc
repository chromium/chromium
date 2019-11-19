// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_host.h"

#include <utility>
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/window_post_message_options.h"
#include "third_party/blink/renderer/core/html/portal/dom_window_portal_host.h"
#include "third_party/blink/renderer/core/html/portal/portal_post_message_helper.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

PortalHost::PortalHost(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void PortalHost::Trace(Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
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
  return GetSupplementable()->document();
}

PortalHost* PortalHost::ToPortalHost() {
  return this;
}

Document* PortalHost::GetDocument() const {
  return To<Document>(GetExecutionContext());
}

void PortalHost::OnPortalActivated() {
  portal_host_.reset();
}

void PortalHost::postMessage(ScriptState* script_state,
                             const ScriptValue& message,
                             const String& target_origin,
                             HeapVector<ScriptValue>& transfer,
                             ExceptionState& exception_state) {
  WindowPostMessageOptions* options = WindowPostMessageOptions::Create();
  options->setTargetOrigin(target_origin);
  if (!transfer.IsEmpty())
    options->setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void PortalHost::postMessage(ScriptState* script_state,
                             const ScriptValue& message,
                             const WindowPostMessageOptions* options,
                             ExceptionState& exception_state) {
  if (!DOMWindowPortalHost::ShouldExposePortalHost(*GetSupplementable())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The document is no longer inside a portal");
    return;
  }

  scoped_refptr<const SecurityOrigin> target_origin =
      PostMessageHelper::GetTargetOrigin(options, *GetDocument(),
                                         exception_state);
  if (exception_state.HadException())
    return;

  BlinkTransferableMessage transferable_message =
      PortalPostMessageHelper::CreateMessage(script_state, message, options,
                                             exception_state);
  if (exception_state.HadException())
    return;

  GetPortalHostInterface().PostMessageToHost(std::move(transferable_message),
                                             target_origin);
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
    scoped_refptr<const SecurityOrigin> source_origin,
    scoped_refptr<const SecurityOrigin> target_origin) {
  DCHECK(GetDocument()->GetPage()->InsidePortal());
  PortalPostMessageHelper::CreateAndDispatchMessageEvent(
      this, std::move(message), source_origin, target_origin);
}

mojom::blink::PortalHost& PortalHost::GetPortalHostInterface() {
  if (!portal_host_) {
    DCHECK(GetDocument()->GetFrame());
    GetDocument()
        ->GetFrame()
        ->GetRemoteNavigationAssociatedInterfaces()
        ->GetInterface(&portal_host_);
  }
  return *portal_host_.get();
}

}  // namespace blink
