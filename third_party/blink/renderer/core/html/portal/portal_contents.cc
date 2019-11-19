// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_contents.h"

#include "base/compiler_specific.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/referrer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document_shutdown_observer.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html/portal/portal_post_message_helper.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

PortalContents::PortalContents(
    HTMLPortalElement& portal_element,
    const base::UnguessableToken& portal_token,
    mojo::PendingAssociatedRemote<mojom::blink::Portal> remote_portal,
    mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
        portal_client_receiver)
    : document_(portal_element.GetDocument()),
      portal_element_(&portal_element),
      portal_token_(portal_token),
      remote_portal_(std::move(remote_portal)),
      portal_client_receiver_(this, std::move(portal_client_receiver)) {
  remote_portal_.set_disconnect_handler(
      WTF::Bind(&PortalContents::Destroy, WrapWeakPersistent(this)));
  DocumentPortals::From(GetDocument()).RegisterPortalContents(this);
}

PortalContents::~PortalContents() {}

RemoteFrame* PortalContents::GetFrame() const {
  if (portal_element_)
    return To<RemoteFrame>(portal_element_->ContentFrame());
  return nullptr;
}

ScriptPromise PortalContents::Activate(ScriptState* script_state,
                                       BlinkTransferableMessage data) {
  DCHECK(!IsActivating());
  DCHECK(portal_element_);

  // Mark this contents as having activation in progress.
  DocumentPortals& document_portals = DocumentPortals::From(GetDocument());
  document_portals.SetActivatingPortalContents(this);
  activate_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // Request activation from the browser process.
  // This object (and thus the Mojo connection it owns) remains alive while the
  // renderer awaits the response.
  remote_portal_->Activate(
      std::move(data),
      WTF::Bind(&PortalContents::OnActivateResponse, WrapPersistent(this)));

  // Dissociate from the element. The element is expected to do the same.
  portal_element_ = nullptr;

  return activate_resolver_->Promise();
}

void PortalContents::OnActivateResponse(
    mojom::blink::PortalActivateResult result) {
  bool should_destroy_contents = false;
  switch (result) {
    case mojom::blink::PortalActivateResult::kPredecessorWasAdopted:
      GetDocument().GetPage()->SetInsidePortal(true);
      FALLTHROUGH;
    case mojom::blink::PortalActivateResult::kPredecessorWillUnload:
      activate_resolver_->Resolve();
      should_destroy_contents = true;
      break;

    case mojom::blink::PortalActivateResult::
        kRejectedDueToPredecessorNavigation: {
      if (!GetDocument().IsContextDestroyed()) {
        ScriptState* script_state = activate_resolver_->GetScriptState();
        ScriptState::Scope scope(script_state);
        // TODO(jbroman): It's slightly unfortunate to hard-code the string
        // HTMLPortalElement here. Ideally this would be threaded through from
        // there and carried with the ScriptPromiseResolver. See
        // https://crbug.com/991544.
        ExceptionState exception_state(script_state->GetIsolate(),
                                       ExceptionState::kExecutionContext,
                                       "HTMLPortalElement", "activate");
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "A top-level navigation is in progress.");
        activate_resolver_->Reject(exception_state);
      }
      break;
    }
    case mojom::blink::PortalActivateResult::kAbortedDueToBug:
      // This should never happen. Ignore this and wait for the frame to be
      // discarded by the browser, if it hasn't already.
      activate_resolver_->Detach();
      return;
  }

  DocumentPortals& document_portals = DocumentPortals::From(GetDocument());
  DCHECK_EQ(document_portals.GetActivatingPortalContents(), this);
  document_portals.ClearActivatingPortalContents();

  activate_resolver_ = nullptr;

  if (should_destroy_contents)
    Destroy();
}

void PortalContents::PostMessageToGuest(
    BlinkTransferableMessage message,
    const scoped_refptr<const SecurityOrigin>& target_origin) {
  remote_portal_->PostMessageToGuest(std::move(message), target_origin);
}

void PortalContents::Navigate(
    const KURL& url,
    network::mojom::ReferrerPolicy referrer_policy_to_use) {
  if (url.IsEmpty())
    return;

  if (!url.ProtocolIsInHTTPFamily()) {
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kRendering,
        mojom::ConsoleMessageLevel::kWarning,
        "Portals only allow navigation to protocols in the HTTP family."));
    return;
  }

  if (referrer_policy_to_use == network::mojom::ReferrerPolicy::kDefault)
    referrer_policy_to_use = GetDocument().GetReferrerPolicy();
  Referrer referrer = SecurityPolicy::GenerateReferrer(
      referrer_policy_to_use, GetDocument().GetSecurityOrigin(), url,
      GetDocument().OutgoingReferrer());
  auto mojo_referrer = mojom::blink::Referrer::New(
      KURL(NullURL(), referrer.referrer), referrer.referrer_policy);

  // There is a brief window of time between when Navigate is called and its
  // callback is run, during which the Portal's content frame is not marked as
  // loading. We use IncrementLoadEventDelayCount to block its document's load
  // event in this time window. Once it goes out of scope,
  // IncrementLoadEventDelayCount will call Document::CheckCompleted and fire
  // load events if necessary.
  std::unique_ptr<IncrementLoadEventDelayCount>
      increment_load_event_delay_count =
          std::make_unique<IncrementLoadEventDelayCount>(GetDocument());
  remote_portal_->Navigate(
      url, std::move(mojo_referrer),
      WTF::Bind([](std::unique_ptr<IncrementLoadEventDelayCount>
                       increment_load_event_delay_count) {},
                std::move(increment_load_event_delay_count)));
}

void PortalContents::Destroy() {
  DCHECK(!IsActivating());
  if (HTMLPortalElement* element = std::exchange(portal_element_, nullptr))
    element->ConsumePortal();
  portal_token_ = base::UnguessableToken();
  remote_portal_.reset();
  portal_client_receiver_.reset();
  DocumentPortals::From(GetDocument()).DeregisterPortalContents(this);
}

void PortalContents::ForwardMessageFromGuest(
    BlinkTransferableMessage message,
    const scoped_refptr<const SecurityOrigin>& source_origin,
    const scoped_refptr<const SecurityOrigin>& target_origin) {
  if (!IsValid() || !portal_element_)
    return;

  PortalPostMessageHelper::CreateAndDispatchMessageEvent(
      portal_element_, std::move(message), source_origin, target_origin);
}

void PortalContents::DispatchLoadEvent() {
  if (!IsValid() || !portal_element_)
    return;

  portal_element_->DispatchLoad();
  GetDocument().CheckCompleted();
}

void PortalContents::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(portal_element_);
  visitor->Trace(activate_resolver_);
}

}  // namespace blink
