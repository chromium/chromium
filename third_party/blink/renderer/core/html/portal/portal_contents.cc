// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_contents.h"

#include "base/compiler_specific.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_id_helper.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-blink.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html/portal/portal_activation_delegate.h"
#include "third_party/blink/renderer/core/html/portal/portal_post_message_helper.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

PortalContents::PortalContents(
    HTMLPortalElement& portal_element,
    const PortalToken& portal_token,
    mojo::PendingAssociatedRemote<mojom::blink::Portal> remote_portal,
    mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
        portal_client_receiver)
    : document_(portal_element.GetDocument()),
      portal_element_(&portal_element),
      portal_token_(portal_token),
      remote_portal_(std::move(remote_portal)),
      portal_client_receiver_(this, std::move(portal_client_receiver)) {
  remote_portal_.set_disconnect_handler(WTF::BindOnce(
      &PortalContents::DisconnectHandler, WrapWeakPersistent(this)));
  DocumentPortals::GetOrCreate(GetDocument()).RegisterPortalContents(this);
}

PortalContents::~PortalContents() = default;

RemoteFrame* PortalContents::GetFrame() const {
  if (portal_element_)
    return To<RemoteFrame>(portal_element_->ContentFrame());
  return nullptr;
}

void PortalContents::Activate(BlinkTransferableMessage data,
                              PortalActivationDelegate* delegate) {
  DCHECK(!IsActivating());
  DCHECK(portal_element_);

  // Mark this contents as having activation in progress.
  auto* document_portals = DocumentPortals::Get(GetDocument());
  DCHECK(document_portals);
  document_portals->SetActivatingPortalContents(this);
  activation_delegate_ = delegate;

  uint64_t trace_id = base::trace_event::GetNextGlobalTraceId();
  TRACE_EVENT_WITH_FLOW0("navigation", "PortalContents::Activate",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  // Request activation from the browser process.
  // This object (and thus the Mojo connection it owns) remains alive while the
  // renderer awaits the response.
  remote_portal_->Activate(
      std::move(data), base::TimeTicks::Now(), trace_id,
      WTF::BindOnce(&PortalContents::OnActivateResponse, WrapPersistent(this)));

  // Dissociate from the element. The element is expected to do the same.
  portal_element_ = nullptr;
}

void PortalContents::OnActivateResponse(
    mojom::blink::PortalActivateResult result) {
  auto reject = [&](const char* message) {
    if (GetDocument().GetExecutionContext())
      activation_delegate_->ActivationDidFail(message);
  };

  bool should_destroy_contents = false;
  switch (result) {
    case mojom::blink::PortalActivateResult::kPredecessorWasAdopted:
      if (auto* page = GetDocument().GetPage())
        page->SetInsidePortal(true);
      [[fallthrough]];
    case mojom::blink::PortalActivateResult::kPredecessorWillUnload:
      activation_delegate_->ActivationDidSucceed();
      should_destroy_contents = true;
      break;

    case mojom::blink::PortalActivateResult::
        kRejectedDueToPredecessorNavigation:
      reject("A top-level navigation is in progress.");
      break;
    case mojom::blink::PortalActivateResult::kRejectedDueToPortalNotReady:
      reject("The portal was not yet ready or was blocked.");
      break;
    case mojom::blink::PortalActivateResult::kRejectedDueToErrorInPortal:
      reject("The portal is in an error state.");
      break;
    case mojom::blink::PortalActivateResult::kDisconnected:
      // Only called when |remote_portal_| is disconnected. This usually happens
      // when the browser/test runner is being shut down.
      activation_delegate_->ActivationWasAbandoned();
      break;
    case mojom::blink::PortalActivateResult::kNotImplemented:
      reject("Not implemented.");
      break;
    case mojom::blink::PortalActivateResult::kAbortedDueToBug:
      // This should never happen. Ignore this and wait for the frame to be
      // discarded by the browser, if it hasn't already.
      activation_delegate_->ActivationWasAbandoned();
      return;
  }

  auto* document_portals = DocumentPortals::Get(GetDocument());
  DCHECK(document_portals);
  DCHECK_EQ(document_portals->GetActivatingPortalContents(), this);
  document_portals->ClearActivatingPortalContents();

  activation_delegate_ = nullptr;

  if (should_destroy_contents)
    Destroy();
}

void PortalContents::PostMessageToGuest(BlinkTransferableMessage message) {
  remote_portal_->PostMessageToGuest(std::move(message));
}

void PortalContents::Navigate(
    const KURL& url,
    network::mojom::ReferrerPolicy referrer_policy_to_use) {
  if (url.IsEmpty())
    return;

  if (!url.ProtocolIsInHTTPFamily()) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kRendering,
        mojom::ConsoleMessageLevel::kWarning,
        "Portals only allow navigation to protocols in the HTTP family."));
    return;
  }

  ExecutionContext* context = GetDocument().GetExecutionContext();
  if (referrer_policy_to_use == network::mojom::ReferrerPolicy::kDefault)
    referrer_policy_to_use = context->GetReferrerPolicy();
  Referrer referrer = SecurityPolicy::GenerateReferrer(
      referrer_policy_to_use, url, context->OutgoingReferrer());
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
      WTF::BindOnce([](std::unique_ptr<IncrementLoadEventDelayCount>
                           increment_load_event_delay_count) {},
                    std::move(increment_load_event_delay_count)));
}

void PortalContents::Destroy() {
  DCHECK(!IsActivating());
  if (portal_element_) {
    portal_element_->PortalContentsWillBeDestroyed(this);
    portal_element_ = nullptr;
  }
  portal_token_ = absl::nullopt;
  remote_portal_.reset();
  portal_client_receiver_.reset();
  DocumentPortals::GetOrCreate(GetDocument()).DeregisterPortalContents(this);
}

void PortalContents::DisconnectHandler() {
  if (IsActivating())
    OnActivateResponse(mojom::blink::PortalActivateResult::kDisconnected);
  Destroy();
}

void PortalContents::ForwardMessageFromGuest(
    BlinkTransferableMessage message,
    const scoped_refptr<const SecurityOrigin>& source_origin) {
  if (!IsValid() || !portal_element_)
    return;

  PortalPostMessageHelper::CreateAndDispatchMessageEvent(
      portal_element_, std::move(message), source_origin);
}

void PortalContents::DispatchLoadEvent() {
  if (!IsValid() || !portal_element_)
    return;

  portal_element_->DispatchLoad();
  GetDocument().CheckCompleted();
}

void PortalContents::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(portal_element_);
  visitor->Trace(activation_delegate_);
}

}  // namespace blink
