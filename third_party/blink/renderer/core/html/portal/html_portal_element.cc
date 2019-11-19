// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"

#include <utility>
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/referrer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/window_post_message_options.h"
#include "third_party/blink/renderer/core/html/html_unknown_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/portal_activate_options.h"
#include "third_party/blink/renderer/core/html/portal/portal_contents.h"
#include "third_party/blink/renderer/core/html/portal/portal_post_message_helper.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

HTMLPortalElement::HTMLPortalElement(
    Document& document,
    const base::UnguessableToken& portal_token,
    mojo::PendingAssociatedRemote<mojom::blink::Portal> remote_portal,
    mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
        portal_client_receiver)
    : HTMLFrameOwnerElement(html_names::kPortalTag, document) {
  if (remote_portal) {
    was_just_adopted_ = true;
    DCHECK(CanHaveGuestContents())
        << "<portal> element was created with an existing contents but is not "
           "permitted to have one";
    portal_ = MakeGarbageCollected<PortalContents>(
        *this, portal_token, std::move(remote_portal),
        std::move(portal_client_receiver));
  }
}

HTMLPortalElement::~HTMLPortalElement() {}

void HTMLPortalElement::Trace(Visitor* visitor) {
  HTMLFrameOwnerElement::Trace(visitor);
  visitor->Trace(portal_);
}

void HTMLPortalElement::ConsumePortal() {
  if (PortalContents* portal = std::exchange(portal_, nullptr))
    portal->Destroy();
}

void HTMLPortalElement::ExpireAdoptionLifetime() {
  was_just_adopted_ = false;

  // After dispatching the portalactivate event, we check to see if we need to
  // cleanup the portal hosting the predecessor. If the portal was created,
  // but wasn't inserted or activated, we destroy it.
  if (!CanHaveGuestContents())
    ConsumePortal();
}

// https://wicg.github.io/portals/#htmlportalelement-may-have-a-guest-browsing-context
HTMLPortalElement::GuestContentsEligibility
HTMLPortalElement::GetGuestContentsEligibility() const {
  // Non-HTML documents aren't eligible at all.
  if (!GetDocument().IsHTMLDocument())
    return GuestContentsEligibility::kIneligible;

  LocalFrame* frame = GetDocument().GetFrame();
  const bool is_connected = frame && isConnected();
  if (!is_connected && !was_just_adopted_)
    return GuestContentsEligibility::kIneligible;

  const bool is_top_level = frame && frame->IsMainFrame();
  if (!is_top_level)
    return GuestContentsEligibility::kNotTopLevel;

  return GuestContentsEligibility::kEligible;
}

void HTMLPortalElement::Navigate() {
  if (portal_) {
    portal_->Navigate(GetNonEmptyURLAttribute(html_names::kSrcAttr),
                      ReferrerPolicyAttribute());
  }
}

namespace {

BlinkTransferableMessage ActivateDataAsMessage(
    ScriptState* script_state,
    PortalActivateOptions* options,
    ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  Transferables transferables;
  if (options->hasTransfer()) {
    if (!SerializedScriptValue::ExtractTransferables(
            script_state->GetIsolate(), options->transfer(), transferables,
            exception_state))
      return {};
  }

  SerializedScriptValue::SerializeOptions serialize_options;
  serialize_options.transferables = &transferables;
  v8::Local<v8::Value> data = options->hasData()
                                  ? options->data().V8Value()
                                  : v8::Null(isolate).As<v8::Value>();

  BlinkTransferableMessage msg;
  msg.message = SerializedScriptValue::Serialize(
      isolate, data, serialize_options, exception_state);
  if (!msg.message)
    return {};

  msg.message->UnregisterMemoryAllocatedWithCurrentScriptContext();

  auto* execution_context = ExecutionContext::From(script_state);
  msg.ports = MessagePort::DisentanglePorts(
      execution_context, transferables.message_ports, exception_state);
  if (exception_state.HadException())
    return {};

  msg.sender_origin = execution_context->GetSecurityOrigin()->IsolatedCopy();

  // msg.user_activation is left out; we will probably handle user activation
  // explicitly for activate data.
  // TODO(crbug.com/936184): Answer this for good.

  if (ThreadDebugger* debugger = ThreadDebugger::From(isolate))
    msg.sender_stack_trace_id = debugger->StoreCurrentStackTrace("activate");

  if (msg.message->IsLockedToAgentCluster()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataCloneError,
        "Cannot send agent cluster-locked data (e.g. SharedArrayBuffer) "
        "through portal activation.");
    return {};
  }

  return msg;
}

}  // namespace

ScriptPromise HTMLPortalElement::activate(ScriptState* script_state,
                                          PortalActivateOptions* options,
                                          ExceptionState& exception_state) {
  if (!portal_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The HTMLPortalElement is not associated with a portal context.");
    return ScriptPromise();
  }
  if (DocumentPortals::From(GetDocument()).IsPortalInDocumentActivating()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "activate() has already been called on another "
        "HTMLPortalElement in this document.");
    return ScriptPromise();
  }
  if (GetDocument().GetPage()->InsidePortal()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot activate a portal that is inside another portal.");
    return ScriptPromise();
  }

  BlinkTransferableMessage data =
      ActivateDataAsMessage(script_state, options, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  PortalContents* portal = std::exchange(portal_, nullptr);
  return portal->Activate(script_state, std::move(data));
}

void HTMLPortalElement::postMessage(ScriptState* script_state,
                                    const ScriptValue& message,
                                    const String& target_origin,
                                    const HeapVector<ScriptValue>& transfer,
                                    ExceptionState& exception_state) {
  WindowPostMessageOptions* options = WindowPostMessageOptions::Create();
  options->setTargetOrigin(target_origin);
  if (!transfer.IsEmpty())
    options->setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void HTMLPortalElement::postMessage(ScriptState* script_state,
                                    const ScriptValue& message,
                                    const WindowPostMessageOptions* options,
                                    ExceptionState& exception_state) {
  if (!portal_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The HTMLPortalElement is not associated with a portal context");
    return;
  }

  scoped_refptr<const SecurityOrigin> target_origin =
      PostMessageHelper::GetTargetOrigin(options, GetDocument(),
                                         exception_state);
  if (exception_state.HadException())
    return;

  BlinkTransferableMessage transferable_message =
      PortalPostMessageHelper::CreateMessage(script_state, message, options,
                                             exception_state);
  if (exception_state.HadException())
    return;

  portal_->PostMessageToGuest(std::move(transferable_message), target_origin);
}

EventListener* HTMLPortalElement::onmessage() {
  return GetAttributeEventListener(event_type_names::kMessage);
}

void HTMLPortalElement::setOnmessage(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kMessage, listener);
}

EventListener* HTMLPortalElement::onmessageerror() {
  return GetAttributeEventListener(event_type_names::kMessageerror);
}

void HTMLPortalElement::setOnmessageerror(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kMessageerror, listener);
}

const base::UnguessableToken& HTMLPortalElement::GetToken() const {
  DCHECK(portal_ && portal_->IsValid());
  return portal_->GetToken();
}

HTMLPortalElement::InsertionNotificationRequest HTMLPortalElement::InsertedInto(
    ContainerNode& node) {
  auto result = HTMLFrameOwnerElement::InsertedInto(node);

  switch (GetGuestContentsEligibility()) {
    case GuestContentsEligibility::kIneligible:
      return result;

    case GuestContentsEligibility::kNotTopLevel:
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kRendering,
          mojom::ConsoleMessageLevel::kWarning,
          "Cannot use <portal> in a nested browsing context."));
      return result;

    case GuestContentsEligibility::kEligible:
      break;
  };

  if (portal_) {
    // The interface is already bound if the HTMLPortalElement is adopting the
    // predecessor.
    GetDocument().GetFrame()->Client()->AdoptPortal(this);
  } else {
    mojo::PendingAssociatedRemote<mojom::blink::Portal> portal;
    mojo::PendingAssociatedReceiver<mojom::blink::Portal> portal_receiver =
        portal.InitWithNewEndpointAndPassReceiver();

    mojo::PendingAssociatedRemote<mojom::blink::PortalClient> client;
    mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
        client_receiver = client.InitWithNewEndpointAndPassReceiver();

    RemoteFrame* portal_frame;
    base::UnguessableToken portal_token;
    std::tie(portal_frame, portal_token) =
        GetDocument().GetFrame()->Client()->CreatePortal(
            this, std::move(portal_receiver), std::move(client));

    portal_ = MakeGarbageCollected<PortalContents>(
        *this, portal_token, std::move(portal), std::move(client_receiver));

    Navigate();
  }
  probe::PortalRemoteFrameCreated(&GetDocument(), this);
  return result;
}

void HTMLPortalElement::RemovedFrom(ContainerNode& node) {
  DCHECK(!portal_) << "This element should have previously dissociated in "
                      "DisconnectContentFrame";
  HTMLFrameOwnerElement::RemovedFrom(node);
}

bool HTMLPortalElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         HTMLFrameOwnerElement::IsURLAttribute(attribute);
}

void HTMLPortalElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLFrameOwnerElement::ParseAttribute(params);

  if (params.name == html_names::kSrcAttr) {
    Navigate();
    return;
  }

  if (params.name == html_names::kReferrerpolicyAttr) {
    referrer_policy_ = network::mojom::ReferrerPolicy::kDefault;
    if (!params.new_value.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          params.new_value, kDoNotSupportReferrerPolicyLegacyKeywords,
          &referrer_policy_);
    }
    return;
  }

  struct {
    const QualifiedName& name;
    const AtomicString& event_name;
  } event_handler_attributes[] = {
      {html_names::kOnmessageAttr, event_type_names::kMessage},
      {html_names::kOnmessageerrorAttr, event_type_names::kMessageerror},
  };
  for (const auto& attribute : event_handler_attributes) {
    if (params.name == attribute.name) {
      SetAttributeEventListener(
          attribute.event_name,
          CreateAttributeEventListener(this, attribute.name, params.new_value));
      return;
    }
  }
}

LayoutObject* HTMLPortalElement::CreateLayoutObject(const ComputedStyle& style,
                                                    LegacyLayout) {
  return new LayoutIFrame(this);
}

void HTMLPortalElement::DisconnectContentFrame() {
  HTMLFrameOwnerElement::DisconnectContentFrame();
  ConsumePortal();
}

void HTMLPortalElement::AttachLayoutTree(AttachContext& context) {
  HTMLFrameOwnerElement::AttachLayoutTree(context);

  if (GetLayoutEmbeddedContent() && ContentFrame())
    SetEmbeddedContentView(ContentFrame()->View());
}

network::mojom::ReferrerPolicy HTMLPortalElement::ReferrerPolicyAttribute() {
  return referrer_policy_;
}

}  // namespace blink
