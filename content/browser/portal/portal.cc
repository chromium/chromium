// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal.h"

#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/bad_message.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_type.h"
#include "content/public/common/referrer_type_converters.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"

namespace content {

namespace {
void CreatePortalRenderWidgetHostView(WebContentsImpl* web_contents,
                                      RenderViewHostImpl* render_view_host) {
  if (auto* view = render_view_host->GetWidget()->GetView())
    view->Destroy();
  web_contents->CreateRenderWidgetHostViewForRenderManager(render_view_host);
}
}  // namespace

Portal::Portal(RenderFrameHostImpl* owner_render_frame_host)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(owner_render_frame_host)),
      owner_render_frame_host_(owner_render_frame_host) {}

Portal::Portal(RenderFrameHostImpl* owner_render_frame_host,
               std::unique_ptr<WebContents> existing_web_contents)
    : Portal(owner_render_frame_host) {
  portal_contents_.SetOwned(std::move(existing_web_contents));
  portal_contents_->NotifyInsidePortal(true);
}

Portal::~Portal() {
  devtools_instrumentation::PortalDetached(
      GetPortalHostContents()->GetPrimaryMainFrame());
  Observe(nullptr);
}

// static
bool Portal::IsEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kPortals);
}

// static
void Portal::BindPortalHostReceiver(
    RenderFrameHostImpl* frame,
    mojo::PendingAssociatedReceiver<blink::mojom::PortalHost>
        pending_receiver) {
  if (!IsEnabled()) {
    mojo::ReportBadMessage(
        "blink.mojom.PortalHost can only be used if the Portals feature is "
        "enabled.");
    return;
  }

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(frame));

  // This guards against the blink::mojom::PortalHost interface being used
  // outside the main frame of a Portal's guest.
  if (!web_contents || !web_contents->IsPortal() || !frame->is_main_frame()) {
    mojo::ReportBadMessage(
        "blink.mojom.PortalHost can only be used by the the main frame of a "
        "Portal's guest.");
    return;
  }

  // This binding may already be bound to another request, and in such cases,
  // we rebind with the new request. An example scenario is a new document after
  // a portal navigation trying to create a connection, but the old document
  // hasn't been destroyed yet (and the pipe hasn't been closed).
  auto& receiver = web_contents->portal()->portal_host_receiver_;
  if (receiver.is_bound())
    receiver.reset();
  receiver.Bind(std::move(pending_receiver));
  receiver.SetFilter(frame->CreateMessageFilterForAssociatedReceiver(
      blink::mojom::PortalHost::Name_));
}

void Portal::Bind(
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client) {
  DCHECK(!receiver_.is_bound());
  DCHECK(!client_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&Portal::Close, base::Unretained(this)));
  client_.Bind(std::move(client));
}

void Portal::DestroySelf() {
  // Deletes |this|.
  owner_render_frame_host_->DestroyPortal(this);
}

RenderFrameProxyHost* Portal::CreateProxyAndAttachPortal(
    blink::mojom::RemoteFrameInterfacesFromRendererPtr
        remote_frame_interfaces) {
  DCHECK(remote_frame_interfaces);
  WebContentsImpl* outer_contents_impl = GetPortalHostContents();

  // Check if portal has already been attached.
  if (portal_contents_ && portal_contents_->GetOuterWebContents()) {
    mojo::ReportBadMessage(
        "Trying to attach a portal that has already been attached.");
    return nullptr;
  }

  // Create a FrameTreeNode in the outer WebContents to host the portal, in
  // response to the creation of a portal in the renderer process.
  FrameTreeNode* outer_node =
      outer_contents_impl->GetPrimaryFrameTree().AddFrame(
          owner_render_frame_host_,
          owner_render_frame_host_->GetProcess()->GetID(),
          owner_render_frame_host_->GetProcess()->GetNextRoutingID(),
          // `outer_node` is just a dummy outer delegate node, which will never
          // have a corresponding `RenderFrameImpl`, and therefore we pass null
          // remotes/receivers for connections that it would normally have to a
          // renderer process.
          /*frame_remote=*/mojo::NullAssociatedRemote(),
          /*browser_interface_broker_receiver=*/mojo::NullReceiver(),
          // The PolicyContainerHost remote is sent to Blink in the
          // CreateRenderView mojo message.
          /*policy_container_bind_params=*/nullptr,
          /*associated_interface_provider_receiver=*/
          mojo::NullAssociatedReceiver(),
          blink::mojom::TreeScopeType::kDocument, "", "", true,
          blink::LocalFrameToken(), base::UnguessableToken::Create(),
          blink::DocumentToken(), blink::FramePolicy(),
          blink::mojom::FrameOwnerProperties(), false,
          blink::FrameOwnerElementType::kPortal,
          /*is_dummy_frame_for_inner_tree=*/true);
  outer_node->AddObserver(this);

  bool web_contents_created = false;
  if (!portal_contents_) {
    // Create the Portal WebContents.
    WebContents::CreateParams params(outer_contents_impl->GetBrowserContext());
    portal_contents_.SetOwned(base::WrapUnique(
        static_cast<WebContentsImpl*>(WebContents::Create(params).release())));
    outer_contents_impl->InnerWebContentsCreated(portal_contents_.get());
    web_contents_created = true;
  }

  DCHECK(portal_contents_.OwnsContents());
  DCHECK_EQ(portal_contents_->portal(), this);
  DCHECK_EQ(portal_contents_->GetDelegate(), this);

  DCHECK(!is_closing_) << "Portal should not be shutting down when contents "
                          "ownership is yielded";
  outer_contents_impl->AttachInnerWebContents(
      portal_contents_.ReleaseOwnership(), outer_node->current_frame_host(),
      std::move(remote_frame_interfaces->frame),
      std::move(remote_frame_interfaces->frame_host_receiver),
      /*is_full_page=*/false);

  // Create the view for all RenderViewHosts that don't have a
  // RenderWidgetHostViewChildFrame view.
  portal_contents_->GetPrimaryFrameTree().ForEachRenderViewHost(
      [this](RenderViewHostImpl* rvh) {
        if (!rvh->GetWidget()->GetView() ||
            !rvh->GetWidget()->GetView()->IsRenderWidgetHostViewChildFrame()) {
          CreatePortalRenderWidgetHostView(portal_contents_.get(), rvh);
        }
      });

  RenderFrameProxyHost* proxy_host =
      portal_contents_->GetPrimaryMainFrame()->GetProxyToOuterDelegate();
  proxy_host->SetRenderFrameProxyCreated(true);
  portal_contents_->ReattachToOuterWebContentsFrame();

  if (web_contents_created)
    PortalWebContentsCreated(portal_contents_.get());

  outer_contents_impl->NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);

  devtools_instrumentation::PortalAttached(
      outer_contents_impl->GetPrimaryMainFrame());

  return proxy_host;
}

void Portal::Close() {
  if (is_closing_)
    return;
  is_closing_ = true;
  receiver_.reset();

  // If the contents is attached to its outer `WebContents`, and therefore not
  // owned by `portal_contents_`, we can destroy ourself right now.
  if (!portal_contents_.OwnsContents()) {
    DestroySelf();  // Deletes this.
    return;
  }

  // Otherwise if the portal contents is not attached to an outer `WebContents`,
  // we have to manage the destruction process ourself. We start by calling
  // `WebContentsImpl::ClosePage()`, which will go through the proper unload
  // handler dance, and eventually come back and call `Portal::CloseContents()`,
  // which for orphaned contents, will finally invoke `DestroySelf()`.
  portal_contents_->ClosePage();
}

void Portal::Navigate(const GURL& url,
                      blink::mojom::ReferrerPtr referrer,
                      NavigateCallback callback) {
  // |RenderFrameHostImpl::CreatePortal| doesn't allow portals to be created in
  // a prerender page.
  DCHECK_NE(RenderFrameHost::LifecycleState::kPrerendering,
            owner_render_frame_host_->GetLifecycleState());

  if (!url.SchemeIsHTTPOrHTTPS()) {
    mojo::ReportBadMessage("Portal::Navigate tried to use non-HTTP protocol.");
    DestroySelf();  // Also deletes |this|.
    return;
  }

  GURL out_validated_url = url;
  owner_render_frame_host_->GetSiteInstance()->GetProcess()->FilterURL(
      false, &out_validated_url);

  FrameTreeNode* portal_root = portal_contents_->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* portal_frame = portal_root->current_frame_host();

  // Use a default download policy, since downloads are explicitly disabled in
  // `CanDownload()`.
  blink::NavigationDownloadPolicy download_policy;

  // Navigations in portals do not affect the host's session history. Upon
  // activation, only the portal's last committed entry is merged with the
  // host's session history. Hence, a portal maintaining multiple session
  // history entries is not useful and would introduce unnecessary complexity.
  // We therefore have portal navigations done with replacement, so that we only
  // have one entry at a time.
  // TODO(mcnee): There are still corner cases (e.g. using window.opener when
  // it's remote) that could cause a portal to navigate without replacement.
  // Fix this so that we can enforce this as an invariant.
  constexpr bool should_replace_entry = true;

  // TODO(crbug.com/1290239): Measure the start time in the renderer process.
  const auto navigation_start_time = base::TimeTicks::Now();

  // TODO(yaoxia): figure out if we need to propagate this.
  blink::mojom::NavigationInitiatorActivationAndAdStatus
      initiator_activation_and_ad_status =
          blink::mojom::NavigationInitiatorActivationAndAdStatus::
              kDidNotStartWithTransientActivation;

  // TODO(https://crbug.com/1074422): It is possible for a portal to be
  // navigated by a frame other than the owning frame. Find a way to route the
  // correct initiator origin and initiator base url of the portal navigation to
  // this call.
  const blink::LocalFrameToken frame_token =
      owner_render_frame_host_->GetFrameToken();
  absl::optional<GURL> initiator_base_url;
  if (!owner_render_frame_host_->GetInheritedBaseUrl().is_empty() &&
      (out_validated_url.IsAboutBlank() || out_validated_url.IsAboutSrcdoc())) {
    // Note: GetInheritedBaseUrl() will only be non-empty when
    // blink::features::IsNewBaseUrlInheritanceBehaviour is true.
    initiator_base_url = owner_render_frame_host_->GetInheritedBaseUrl();
  }
  portal_root->navigator().NavigateFromFrameProxy(
      portal_frame, out_validated_url, &frame_token,
      owner_render_frame_host_->GetProcess()->GetID(),
      owner_render_frame_host_->GetLastCommittedOrigin(), initiator_base_url,
      owner_render_frame_host_->GetSiteInstance(),
      mojo::ConvertTo<Referrer>(referrer), ui::PAGE_TRANSITION_LINK,
      should_replace_entry, download_policy, "GET", nullptr, "", nullptr,
      network::mojom::SourceLocation::New(), false,
      /*is_form_submission=*/false,
      /*impression=*/absl::nullopt, initiator_activation_and_ad_status,
      navigation_start_time);

  std::move(callback).Run();
}

namespace {
void FlushTouchEventQueues(RenderWidgetHostImpl* host) {
  host->input_router()->FlushTouchEventQueue();
  std::unique_ptr<RenderWidgetHostIterator> child_widgets =
      host->GetEmbeddedRenderWidgetHosts();
  while (RenderWidgetHost* child_widget = child_widgets->GetNextHost())
    FlushTouchEventQueues(static_cast<RenderWidgetHostImpl*>(child_widget));
}

// Copies |predecessor_contents|'s navigation entries to
// |activated_contents|. |activated_contents| will have its last committed entry
// combined with the entries in |predecessor_contents|. |predecessor_contents|
// will only keep its last committed entry.
// TODO(914108): This currently only covers the basic cases for history
// traversal across portal activations. The design is still being discussed.
void TakeHistoryForActivation(WebContentsImpl* activated_contents,
                              WebContentsImpl* predecessor_contents) {
  NavigationControllerImpl& activated_controller =
      activated_contents->GetController();
  NavigationControllerImpl& predecessor_controller =
      predecessor_contents->GetController();

  // Activation would have discarded any pending entry in the host contents.
  DCHECK(!predecessor_controller.GetPendingEntry());

  // TODO(mcnee): Once we enforce that a portal contents does not build up its
  // own history, make this DCHECK that we only have a single committed entry,
  // possibly with a new pending entry.
  if (activated_controller.GetPendingEntryIndex() != -1) {
    return;
  }
  DCHECK(activated_controller.GetLastCommittedEntry());
  DCHECK(activated_controller.CanPruneAllButLastCommitted());

  // TODO(mcnee): Allow for portal activations to replace history entries and to
  // traverse existing history entries.
  activated_controller.CopyStateFromAndPrune(&predecessor_controller,
                                             false /* replace_entry */);

  // The predecessor may be adopted as a portal, so it should now only have a
  // single committed entry.
  DCHECK(predecessor_controller.CanPruneAllButLastCommitted());
  predecessor_controller.PruneAllButLastCommitted();
}
}  // namespace

void Portal::Activate(blink::TransferableMessage data,
                      base::TimeTicks activation_time,
                      uint64_t trace_id,
                      ActivateCallback callback) {
  if (GetPortalHostContents()->portal()) {
    mojo::ReportBadMessage("Portal::Activate called on nested portal");
    DestroySelf();
    return;
  }

  if (is_activating_) {
    mojo::ReportBadMessage("Portal::Activate called twice on the same portal");
    DestroySelf();
    return;
  }

  for (Portal* portal : owner_render_frame_host()->GetPortals()) {
    if (portal != this && portal->is_activating_) {
      mojo::ReportBadMessage(
          "Portal::Activate called on portal whose owner RenderFrameHost has "
          "another portal that is activating");
      DestroySelf();
      return;
    }
  }

  is_activating_ = true;
  WebContentsImpl* outer_contents = GetPortalHostContents();
  outer_contents->GetDelegate()->UpdateInspectedWebContentsIfNecessary(
      outer_contents, portal_contents_.get(),
      base::BindOnce(&Portal::ActivateImpl, weak_factory_.GetWeakPtr(),
                     std::move(data), activation_time, trace_id,
                     std::move(callback)));
}

namespace {
const char* kCrossOriginPostMessageError =
    "postMessage failed because portal is not same origin with its host";
}

void Portal::PostMessageToGuest(blink::TransferableMessage message) {
  if (!IsSameOrigin()) {
    owner_render_frame_host()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        kCrossOriginPostMessageError);

    devtools_instrumentation::DidRejectCrossOriginPortalMessage(
        owner_render_frame_host());
    return;
  }
  portal_contents_->GetPrimaryMainFrame()->ForwardMessageFromHost(
      std::move(message), owner_render_frame_host_->GetLastCommittedOrigin());
}

void Portal::PostMessageToHost(blink::TransferableMessage message) {
  DCHECK(GetPortalContents());
  if (!IsSameOrigin()) {
    GetPortalContents()->GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        kCrossOriginPostMessageError);

    devtools_instrumentation::DidRejectCrossOriginPortalMessage(
        GetPortalContents()->GetPrimaryMainFrame());
    return;
  }
  client().ForwardMessageFromGuest(
      std::move(message),
      GetPortalContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

void Portal::OnFrameTreeNodeDestroyed(FrameTreeNode* frame_tree_node) {
  // Listens for the deletion of the FrameTreeNode corresponding to this portal
  // in the outer WebContents (not the FrameTreeNode of the document containing
  // it). If that outer FrameTreeNode goes away, this Portal should stop
  // accepting new messages and go away as well.

  Close();  // May delete |this|.
}

void Portal::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  // Even though this object is owned (via unique_ptr by the RenderFrameHost),
  // explicitly observing RenderFrameDeleted is necessary because it happens
  // earlier than the destructor, notably before Mojo teardown.
  if (render_frame_host == owner_render_frame_host_)
    DestroySelf();  // Deletes |this|.
}

void Portal::WebContentsDestroyed() {
  DestroySelf();  // Deletes |this|.
}

void Portal::LoadingStateChanged(WebContents* source,
                                 bool should_show_loading_ui) {
  DCHECK_EQ(source, portal_contents_.get());
  if (!source->IsLoading())
    client_->DispatchLoadEvent();
}

void Portal::PortalWebContentsCreated(WebContents* portal_web_contents) {
  WebContentsImpl* outer_contents = GetPortalHostContents();
  DCHECK(outer_contents->GetDelegate());
  outer_contents->GetDelegate()->PortalWebContentsCreated(portal_web_contents);
}

void Portal::CloseContents(WebContents* web_contents) {
  DCHECK_EQ(web_contents, portal_contents_.get());
  if (portal_contents_->GetOuterWebContents()) {
    // This portal was still attached, we shouldn't have received a request to
    // close it.
    bad_message::ReceivedBadMessage(
        web_contents->GetPrimaryMainFrame()->GetProcess(),
        bad_message::RWH_CLOSE_PORTAL);
  } else {
    // Orphaned portal was closed.
    DestroySelf();  // Deletes |this|.
  }
}

void Portal::NavigationStateChanged(WebContents* source,
                                    InvalidateTypes changed_flags) {
  WebContents* outer_contents = GetPortalHostContents();
  // Can be null in tests.
  if (!outer_contents->GetDelegate())
    return;
  outer_contents->GetDelegate()->NavigationStateChanged(source, changed_flags);
}

bool Portal::ShouldFocusPageAfterCrash() {
  return false;
}

void Portal::CanDownload(const GURL& url,
                         const std::string& request_method,
                         base::OnceCallback<void(bool)> callback) {
  // Downloads are not allowed in portals.
  owner_render_frame_host()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StringPrintf("Download in a portal (from %s) was blocked.",
                         url.spec().c_str()));
  std::move(callback).Run(false);
}

base::UnguessableToken Portal::GetDevToolsFrameToken() const {
  return portal_contents_->GetPrimaryMainFrame()->GetDevToolsFrameToken();
}

WebContentsImpl* Portal::GetPortalContents() const {
  return portal_contents_.get();
}

WebContentsImpl* Portal::GetPortalHostContents() const {
  return static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));
}

bool Portal::IsSameOrigin() const {
  return owner_render_frame_host_->GetLastCommittedOrigin().IsSameOriginWith(
      portal_contents_->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

std::pair<bool, blink::mojom::PortalActivateResult> Portal::CanActivate() {
  WebContentsImpl* outer_contents = GetPortalHostContents();

  if (outer_contents->GetOuterWebContents()) {
    // TODO(crbug.com/942534): Support portals in guest views.
    NOTIMPLEMENTED();
    return std::make_pair(false,
                          blink::mojom::PortalActivateResult::kNotImplemented);
  }

  DCHECK(owner_render_frame_host_->IsActive())
      << "The binding should have been closed when the portal's outer "
         "FrameTreeNode was deleted due to swap out.";

  DCHECK(portal_contents_);
  NavigationControllerImpl& portal_controller =
      portal_contents_->GetController();
  NavigationControllerImpl& predecessor_controller =
      outer_contents->GetController();

  // If no navigation has yet committed in the portal, it cannot be activated as
  // this would lead to an empty tab contents (without even an about:blank).
  if (portal_contents_->GetPrimaryMainFrame()->is_initial_empty_document()) {
    return std::make_pair(
        false,
        blink::mojom::PortalActivateResult::kRejectedDueToPortalNotReady);
  }
  DCHECK(predecessor_controller.GetLastCommittedEntry());

  // Error pages and interstitials may not host portals due to the HTTP(S)
  // restriction.
  DCHECK_EQ(PAGE_TYPE_NORMAL,
            predecessor_controller.GetLastCommittedEntry()->GetPageType());

  // If the portal is crashed or is showing an error page, reject activation.
  if (portal_contents_->IsCrashed() ||
      portal_controller.GetLastCommittedEntry()->GetPageType() !=
          PAGE_TYPE_NORMAL) {
    return std::make_pair(
        false, blink::mojom::PortalActivateResult::kRejectedDueToErrorInPortal);
  }

  // If a navigation in the main frame is occurring, stop it if possible and
  // reject the activation if it's too late or if an ongoing navigation takes
  // precedence. There are a few cases here:
  // - a different RenderFrameHost has been assigned to the FrameTreeNode
  // - the same RenderFrameHost is being used, but it is committing a navigation
  // - the FrameTreeNode holds a navigation request that can't turn back but has
  //   not yet been handed off to a RenderFrameHost
  FrameTreeNode* outer_root_node = owner_render_frame_host_->frame_tree_node();
  NavigationRequest* outer_navigation = outer_root_node->navigation_request();
  const bool has_user_gesture =
      owner_render_frame_host_->HasTransientUserActivation();

  // WILL_PROCESS_RESPONSE is slightly early: it happens
  // immediately before READY_TO_COMMIT (unless it's deferred), but
  // WILL_PROCESS_RESPONSE is easier to hook for tests using a
  // NavigationThrottle.
  if (owner_render_frame_host_->HasPendingCommitNavigation() ||
      (outer_navigation &&
       outer_navigation->state() >= NavigationRequest::WILL_PROCESS_RESPONSE) ||
      Navigator::ShouldIgnoreIncomingRendererRequest(outer_navigation,
                                                     has_user_gesture)) {
    return std::make_pair(false, blink::mojom::PortalActivateResult::
                                     kRejectedDueToPredecessorNavigation);
  }
  return std::make_pair(true,
                        blink::mojom::PortalActivateResult::kAbortedDueToBug);
}

void Portal::ActivateImpl(blink::TransferableMessage data,
                          base::TimeTicks activation_time,
                          uint64_t trace_id,
                          ActivateCallback callback) {
  WebContentsImpl* outer_contents = GetPortalHostContents();
  WebContentsDelegate* delegate = outer_contents->GetDelegate();

  is_activating_ = false;

  bool can_activate;
  blink::mojom::PortalActivateResult activate_error;
  std::tie(can_activate, activate_error) = CanActivate();
  if (!can_activate) {
    outer_contents->GetDelegate()->UpdateInspectedWebContentsIfNecessary(
        portal_contents_.get(), outer_contents, base::DoNothing());
    std::move(callback).Run(activate_error);
    return;
  }

  FrameTreeNode* outer_root_node = owner_render_frame_host_->frame_tree_node();
  outer_root_node->navigator().CancelNavigation(
      outer_root_node, NavigationDiscardReason::kCommittedNavigation);

  DCHECK(!is_closing_) << "Portal should not be shutting down when contents "
                          "ownership is yielded";

  std::unique_ptr<WebContents> successor_contents;

  if (portal_contents_->GetOuterWebContents()) {
    FrameTreeNode* outer_frame_tree_node = FrameTreeNode::GloballyFindByID(
        portal_contents_->GetOuterDelegateFrameTreeNodeId());
    outer_frame_tree_node->RemoveObserver(this);
    successor_contents = portal_contents_->DetachFromOuterWebContents();
    owner_render_frame_host_->RemoveChild(outer_frame_tree_node);
  } else {
    // Portals created for predecessor pages during activation may not be
    // attached to an outer WebContents, and may not have an outer frame tree
    // node created (i.e. CreateProxyAndAttachPortal isn't called). In this
    // case, we can skip a few of the detachment steps above.
    portal_contents_->GetPrimaryFrameTree().ForEachRenderViewHost(
        [this](RenderViewHostImpl* rvh) {
          CreatePortalRenderWidgetHostView(portal_contents_.get(), rvh);
        });
    successor_contents = portal_contents_.ReleaseOwnership();
  }
  DCHECK(!portal_contents_.OwnsContents());

  // This assumes that the delegate keeps the new contents alive long enough to
  // notify it of activation, at least.
  WebContentsImpl* successor_contents_raw =
      static_cast<WebContentsImpl*>(successor_contents.get());

  auto* outer_contents_main_frame_view = static_cast<RenderWidgetHostViewBase*>(
      outer_contents->GetPrimaryMainFrame()->GetView());
  DCHECK(!outer_contents->GetPrimaryFrameTree()
              .root()
              ->render_manager()
              ->speculative_frame_host());
  auto* portal_contents_main_frame_view =
      static_cast<RenderWidgetHostViewBase*>(
          successor_contents_raw->GetPrimaryMainFrame()->GetView());

  std::vector<std::unique_ptr<ui::TouchEvent>> touch_events;

  if (outer_contents_main_frame_view) {
    // Take fallback contents from previous WebContents so that the activation
    // is smooth without flashes.
    portal_contents_main_frame_view->TakeFallbackContentFrom(
        outer_contents_main_frame_view);
    touch_events =
        outer_contents_main_frame_view->ExtractAndCancelActiveTouches();
    outer_contents->GetView()->CancelDragDropForPortalActivation();
    FlushTouchEventQueues(outer_contents_main_frame_view->host());
  }

  TakeHistoryForActivation(successor_contents_raw, outer_contents);

  successor_contents_raw->set_portal(nullptr);

  // It's important we call this before destroying the outer contents'
  // RenderWidgetHostView, otherwise the dialog may not be cleaned up correctly.
  // See crbug.com/1292261 for more details.
  outer_contents->CancelActiveAndPendingDialogs();

  std::unique_ptr<WebContents> predecessor_web_contents =
      delegate->ActivatePortalWebContents(outer_contents,
                                          std::move(successor_contents));

  // Some unusual delegates cannot yet handle this. And we cannot handle them
  // not handling it. Since this code is likely to be rewritten, this has been
  // promoted to a CHECK to avoid any concern of bad behavior down the line.
  // This shouldn't happen in any _supported_ configuration.
  CHECK_EQ(predecessor_web_contents.get(), outer_contents);

  devtools_instrumentation::PortalActivated(*this);

  if (outer_contents_main_frame_view) {
    portal_contents_main_frame_view->TransferTouches(touch_events);
    // Takes ownership of SyntheticGestureController from the predecessor's
    // RenderWidgetHost. This allows the controller to continue sending events
    // to the new RenderWidgetHostView.
    portal_contents_main_frame_view->host()->TakeSyntheticGestureController(
        outer_contents_main_frame_view->host());
    outer_contents_main_frame_view->Destroy();
  }

  // Remove page focus from the now orphaned predecessor.
  outer_contents->GetPrimaryMainFrame()->GetRenderWidgetHost()->Blur();

  // These pointers are cleared so that they don't dangle in the event this
  // object isn't immediately deleted. It isn't done sooner because
  // ActivatePortalWebContents misbehaves if the WebContents doesn't appear to
  // be a portal at that time.
  portal_contents_.Clear();

  mojo::PendingAssociatedRemote<blink::mojom::Portal> pending_portal;
  auto portal_receiver = pending_portal.InitWithNewEndpointAndPassReceiver();
  mojo::PendingAssociatedRemote<blink::mojom::PortalClient> pending_client;
  auto client_receiver = pending_client.InitWithNewEndpointAndPassReceiver();

  RenderFrameHostImpl* successor_main_frame =
      successor_contents_raw->GetPrimaryMainFrame();
  auto predecessor = std::make_unique<Portal>(
      successor_main_frame, std::move(predecessor_web_contents));
  predecessor->Bind(std::move(portal_receiver), std::move(pending_client));
  successor_main_frame->OnPortalActivated(
      std::move(predecessor), std::move(pending_portal),
      std::move(client_receiver), std::move(data), trace_id,
      std::move(callback));

  // Notifying of activation happens later than ActivatePortalWebContents so
  // that it is observed after predecessor_web_contents has been moved into a
  // portal.
  DCHECK(outer_contents->IsPortal());
  successor_contents_raw->DidActivatePortal(outer_contents, activation_time);
}

Portal::WebContentsHolder::WebContentsHolder(Portal* portal)
    : portal_(portal) {}

Portal::WebContentsHolder::~WebContentsHolder() {
  Clear();
}

bool Portal::WebContentsHolder::OwnsContents() const {
  DCHECK(!owned_contents_ || contents_ == owned_contents_.get());
  return owned_contents_ != nullptr;
}

void Portal::WebContentsHolder::SetUnowned(WebContentsImpl* web_contents) {
  Clear();
  contents_ = web_contents;
  contents_->SetDelegate(portal_);
  contents_->set_portal(portal_);
}

void Portal::WebContentsHolder::SetOwned(
    std::unique_ptr<WebContents> web_contents) {
  SetUnowned(static_cast<WebContentsImpl*>(web_contents.get()));
  owned_contents_ = std::move(web_contents);
  if (owned_contents_) {
    owned_contents_->SetOwnerLocationForDebug(FROM_HERE);
  }
}

void Portal::WebContentsHolder::Clear() {
  if (!contents_)
    return;

  FrameTreeNode* outer_node = FrameTreeNode::GloballyFindByID(
      contents_->GetOuterDelegateFrameTreeNodeId());
  if (outer_node)
    outer_node->RemoveObserver(portal_);

  if (contents_->GetDelegate() == portal_)
    contents_->SetDelegate(nullptr);
  contents_->set_portal(nullptr);

  contents_ = nullptr;
  owned_contents_ = nullptr;
}

}  // namespace content
