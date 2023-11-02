// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"

#include <utility>
#include "third_party/blink/public/mojom/loader/referrer.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_portal_activate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window_post_message_options.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_unknown_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/portal_activation_delegate.h"
#include "third_party/blink/renderer/core/html/portal/portal_contents.h"
#include "third_party/blink/renderer/core/html/portal/portal_post_message_helper.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

HTMLPortalElement::HTMLPortalElement(
    Document& document,
    const PortalToken* portal_token,
    mojo::PendingAssociatedRemote<mojom::blink::Portal> remote_portal,
    mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
        portal_client_receiver)
    : HTMLFrameOwnerElement(html_names::kPortalTag, document),
      feature_handle_for_scheduler_(
          document.GetExecutionContext()
              ? document.GetExecutionContext()->GetScheduler()->RegisterFeature(
                    SchedulingPolicy::Feature::kPortal,
                    {SchedulingPolicy::DisableBackForwardCache()})
              : FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle()) {
  if (remote_portal) {
    // For adoption we must have a populated Portal with contents, which is
    // created post-prerendering.
    DCHECK(!GetDocument().IsPrerendering());
    DCHECK(portal_token);
    was_just_adopted_ = true;
    DCHECK(CanHaveGuestContents())
        << "<portal> element was created with an existing contents but is not "
           "permitted to have one";
    portal_ = MakeGarbageCollected<PortalContents>(
        *this, *portal_token, std::move(remote_portal),
        std::move(portal_client_receiver));
  }
  UseCounter::Count(document, WebFeature::kHTMLPortalElement);
}

HTMLPortalElement::~HTMLPortalElement() = default;

void HTMLPortalElement::Trace(Visitor* visitor) const {
  HTMLFrameOwnerElement::Trace(visitor);
  visitor->Trace(portal_);
}

void HTMLPortalElement::ConsumePortal() {
  if (portal_)
    portal_->Destroy();

  DCHECK(!portal_);
}

void HTMLPortalElement::ExpireAdoptionLifetime() {
  was_just_adopted_ = false;

  // After dispatching the portalactivate event, we check to see if we need to
  // cleanup the portal hosting the predecessor. If the portal was created,
  // but wasn't inserted or activated, we destroy it.
  if (!CanHaveGuestContents())
    ConsumePortal();
}

void HTMLPortalElement::PortalContentsWillBeDestroyed(PortalContents* portal) {
  DCHECK_EQ(portal_, portal);
  portal_ = nullptr;
}

String HTMLPortalElement::PreActivateChecksCommon() {
  if (!portal_)
    return "The HTMLPortalElement is not associated with a portal context.";

  if (DocumentPortals::GetOrCreate(GetDocument())
          .IsPortalInDocumentActivating())
    return "Another portal in this document is activating.";

  if (GetDocument().GetPage()->InsidePortal())
    return "Cannot activate a portal that is inside another portal.";

  if (GetDocument().BeforeUnloadStarted()) {
    return "Cannot activate portal while document is in beforeunload or has "
           "started unloading.";
  }

  return String();
}

void HTMLPortalElement::ActivateDefault() {
  ExecutionContext* context = GetExecutionContext();
  if (!CheckPortalsEnabledOrWarn() || !context)
    return;

  String pre_activate_error = PreActivateChecksCommon();
  if (pre_activate_error) {
    context->AddConsoleMessage(mojom::blink::ConsoleMessageSource::kRendering,
                               mojom::blink::ConsoleMessageLevel::kWarning,
                               pre_activate_error);
    return;
  }

  // Quickly encode undefined without actually invoking script.
  BlinkTransferableMessage data;
  data.message = SerializedScriptValue::UndefinedValue();
  data.message->UnregisterMemoryAllocatedWithCurrentScriptContext();
  data.sender_origin = context->GetSecurityOrigin()->IsolatedCopy();
  if (ThreadDebugger* debugger =
          ThreadDebugger::From(V8PerIsolateData::MainThreadIsolate())) {
    data.sender_stack_trace_id =
        debugger->StoreCurrentStackTrace("activate (implicit)");
  }
  data.sender_agent_cluster_id = context->GetAgentClusterID();

  PortalContents* portal = std::exchange(portal_, nullptr);
  portal->Activate(std::move(data),
                   PortalActivationDelegate::ForConsole(context));
}

bool HTMLPortalElement::CheckWithinFrameLimitOrWarn() const {
  if (IsCurrentlyWithinFrameLimit())
    return true;

  Document& document = GetDocument();
  document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kRendering,
      mojom::blink::ConsoleMessageLevel::kWarning,
      "An operation was prevented due to too many frames and portals present "
      "on the page."));
  return false;
}

bool HTMLPortalElement::CheckPortalsEnabledOrWarn() const {
  ExecutionContext* context = GetExecutionContext();
  if (RuntimeEnabledFeatures::PortalsEnabled(context))
    return true;

  context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kRendering,
      mojom::blink::ConsoleMessageLevel::kWarning,
      "An operation was prevented because a <portal> was moved to a document "
      "where it is not enabled. See "
      "https://www.chromium.org/blink/origin-trials/portals."));
  return false;
}

bool HTMLPortalElement::CheckPortalsEnabledOrThrow(
    ExceptionState& exception_state) const {
  if (RuntimeEnabledFeatures::PortalsEnabled(GetExecutionContext()))
    return true;

  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "An operation was prevented because a <portal> was moved to a document "
      "where it is not enabled. See "
      "https://www.chromium.org/blink/origin-trials/portals.");
  return false;
}

// https://wicg.github.io/portals/#htmlportalelement-may-have-a-guest-browsing-context
HTMLPortalElement::GuestContentsEligibility
HTMLPortalElement::GetGuestContentsEligibility() const {
  // Non-HTML documents aren't eligible at all.
  if (!IsA<HTMLDocument>(GetDocument()))
    return GuestContentsEligibility::kIneligible;

  LocalFrame* frame = GetDocument().GetFrame();
  const bool is_connected = frame && isConnected();
  if (!is_connected && !was_just_adopted_)
    return GuestContentsEligibility::kIneligible;

  const bool is_top_level =
      frame && frame->IsMainFrame() && !frame->IsInFencedFrameTree();
  if (!is_top_level)
    return GuestContentsEligibility::kNotTopLevel;

  // TODO(crbug.com/1051639): We need to find a long term solution to when/how
  // portals should work in sandboxed documents.
  if (frame->DomWindow()->GetSandboxFlags() !=
      network::mojom::blink::WebSandboxFlags::kNone) {
    return GuestContentsEligibility::kSandboxed;
  }

  if (!GetDocument().Url().ProtocolIsInHTTPFamily())
    return GuestContentsEligibility::kNotHTTPFamily;

  return GuestContentsEligibility::kEligible;
}

void HTMLPortalElement::Navigate() {
  if (!CheckPortalsEnabledOrWarn())
    return;

  if (!CheckWithinFrameLimitOrWarn())
    return;

  auto url = GetNonEmptyURLAttribute(html_names::kSrcAttr);

  if (url.PotentiallyDanglingMarkup())
    return;

  if (portal_)
    portal_->Navigate(url, ReferrerPolicyAttribute());
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
  msg.sender_agent_cluster_id = execution_context->GetAgentClusterID();

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
  if (!CheckPortalsEnabledOrThrow(exception_state))
    return ScriptPromise();

  String pre_activate_error = PreActivateChecksCommon();
  if (pre_activate_error) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      pre_activate_error);
    return ScriptPromise();
  }

  BlinkTransferableMessage data =
      ActivateDataAsMessage(script_state, options, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  PortalContents* portal = std::exchange(portal_, nullptr);
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  portal->Activate(std::move(data),
                   PortalActivationDelegate::ForPromise(
                       resolver, exception_state.GetContext()));
  return promise;
}

void HTMLPortalElement::postMessage(ScriptState* script_state,
                                    const ScriptValue& message,
                                    const PostMessageOptions* options,
                                    ExceptionState& exception_state) {
  if (!CheckPortalsEnabledOrThrow(exception_state) || !GetExecutionContext())
    return;

  if (!portal_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The HTMLPortalElement is not associated with a portal context");
    return;
  }

  BlinkTransferableMessage transferable_message =
      PortalPostMessageHelper::CreateMessage(script_state, message, options,
                                             exception_state);
  if (exception_state.HadException())
    return;

  portal_->PostMessageToGuest(std::move(transferable_message));
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

const PortalToken& HTMLPortalElement::GetToken() const {
  DCHECK(portal_ && portal_->IsValid());
  return portal_->GetToken().value();
}

Node::InsertionNotificationRequest HTMLPortalElement::InsertedInto(
    ContainerNode& node) {
  auto result = HTMLFrameOwnerElement::InsertedInto(node);

  if (portal_) {
    DCHECK(!GetDocument().IsPrerendering());
    if (IsPortalCreationOrAdoptionAllowed(&node)) {
      // The interface is already bound if the HTMLPortalElement is adopting the
      // predecessor.
      GetDocument().GetFrame()->Client()->AdoptPortal(this);
      probe::PortalRemoteFrameCreated(&GetDocument(), this);
    }
  } else {
    CreatePortalAndNavigate(&node);
  }
  return result;
}

void HTMLPortalElement::RemovedFrom(ContainerNode& node) {
  DCHECK(!portal_) << "This element should have previously dissociated in "
                      "DisconnectContentFrame";
  HTMLFrameOwnerElement::RemovedFrom(node);
}

void HTMLPortalElement::DefaultEventHandler(Event& event) {
  // Clicking (or equivalent operations via keyboard and other input modalities)
  // a portal element causes it to activate unless prevented.
  if (event.type() == event_type_names::kDOMActivate) {
    ActivateDefault();
    event.SetDefaultHandled();
  }

  if (HandleKeyboardActivation(event))
    return;
  HTMLFrameOwnerElement::DefaultEventHandler(event);
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
          JSEventHandlerForContentAttribute::Create(
              GetExecutionContext(), attribute.name, params.new_value));
      return;
    }
  }
}

LayoutObject* HTMLPortalElement::CreateLayoutObject(const ComputedStyle& style,
                                                    LegacyLayout) {
  return MakeGarbageCollected<LayoutIFrame>(this);
}

bool HTMLPortalElement::SupportsFocus() const {
  return true;
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

bool HTMLPortalElement::IsPortalCreationOrAdoptionAllowed(
    const ContainerNode* node) {
  if (!node) {
    return false;
  }
  // When adopting a predecessor, it is possible to insert a portal that's
  // eligible to have a guest contents to a node that's not connected. In this
  // case, do not create the portal frame yet.
  if (!node->isConnected()) {
    return false;
  }
  if (!CheckPortalsEnabledOrWarn()) {
    return false;
  }
  if (!CheckWithinFrameLimitOrWarn()) {
    return false;
  }
  if (!SubframeLoadingDisabler::CanLoadFrame(*this)) {
    return false;
  }
  switch (GetGuestContentsEligibility()) {
    case GuestContentsEligibility::kIneligible:
      return false;

    case GuestContentsEligibility::kNotTopLevel:
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Cannot use <portal> in a nested browsing context."));
      return false;

    case GuestContentsEligibility::kSandboxed:
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Cannot use <portal> in a sandboxed browsing context."));
      return false;

    case GuestContentsEligibility::kNotHTTPFamily:
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "<portal> use is restricted to the HTTP family."));
      return false;

    case GuestContentsEligibility::kEligible:
      break;
  };

  return true;
}

void HTMLPortalElement::CreatePortalAndNavigate(const ContainerNode* node) {
  if (GetDocument().IsPrerendering()) {
    GetDocument().AddPostPrerenderingActivationStep(
        WTF::BindOnce(&HTMLPortalElement::CreatePortalAndNavigate,
                      WrapWeakPersistent(this), WrapWeakPersistent(node)));
    return;
  }

  if (!IsPortalCreationOrAdoptionAllowed(node)) {
    return;
  }

  mojo::PendingAssociatedRemote<mojom::blink::Portal> portal;
  mojo::PendingAssociatedReceiver<mojom::blink::Portal> portal_receiver =
      portal.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<mojom::blink::PortalClient> client;
  mojo::PendingAssociatedReceiver<mojom::blink::PortalClient> client_receiver =
      client.InitWithNewEndpointAndPassReceiver();

  RemoteFrame* portal_frame;
  PortalToken portal_token;

  std::tie(portal_frame, portal_token) =
      GetDocument().GetFrame()->Client()->CreatePortal(
          this, std::move(portal_receiver), std::move(client));
  DCHECK(portal_frame);

  DCHECK(!portal_);
  portal_ = MakeGarbageCollected<PortalContents>(
      *this, portal_token, std::move(portal), std::move(client_receiver));

  Navigate();

  probe::PortalRemoteFrameCreated(&GetDocument(), this);
}

}  // namespace blink
