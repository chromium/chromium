// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal.h"

#include <unordered_map>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer_type_converters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {
using PortalTokenMap = std::
    unordered_map<base::UnguessableToken, Portal*, base::UnguessableTokenHash>;
base::LazyInstance<PortalTokenMap>::Leaky g_portal_token_map =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

Portal::Portal(RenderFrameHostImpl* owner_render_frame_host)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(owner_render_frame_host)),
      owner_render_frame_host_(owner_render_frame_host),
      portal_token_(base::UnguessableToken::Create()),
      portal_host_receiver_(this) {
  auto pair = g_portal_token_map.Get().emplace(portal_token_, this);
  DCHECK(pair.second);
}

Portal::~Portal() {
  WebContentsImpl* outer_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));
  devtools_instrumentation::PortalDetached(outer_contents_impl->GetMainFrame());

  FrameTreeNode* outer_node = FrameTreeNode::GloballyFindByID(
      portal_contents_impl_->GetOuterDelegateFrameTreeNodeId());
  if (outer_node)
    outer_node->RemoveObserver(this);
  portal_contents_impl_->set_portal(nullptr);

  g_portal_token_map.Get().erase(portal_token_);
}

// static
bool Portal::IsEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kPortals) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

// static
Portal* Portal::FromToken(const base::UnguessableToken& portal_token) {
  PortalTokenMap& portals = g_portal_token_map.Get();
  auto it = portals.find(portal_token);
  return it == portals.end() ? nullptr : it->second;
}

// static
Portal* Portal::Create(
    RenderFrameHostImpl* owner_render_frame_host,
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client) {
  if (!IsEnabled()) {
    mojo::ReportBadMessage(
        "blink.mojom.Portal can only be used if the Portals feature is "
        "enabled.");
    return nullptr;
  }

  auto portal_ptr = base::WrapUnique(new Portal(owner_render_frame_host));
  Portal* portal = portal_ptr.get();
  portal->binding_ = mojo::MakeStrongAssociatedBinding<blink::mojom::Portal>(
      std::move(portal_ptr), std::move(receiver));
  portal->client_.Bind(std::move(client));
  return portal;
}

// static
std::unique_ptr<Portal> Portal::CreateForTesting(
    RenderFrameHostImpl* owner_render_frame_host) {
  return base::WrapUnique(new Portal(owner_render_frame_host));
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
  if (!web_contents || !web_contents->IsPortal() ||
      !frame->frame_tree_node()->IsMainFrame()) {
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
}

RenderFrameProxyHost* Portal::CreateProxyAndAttachPortal() {
  WebContentsImpl* outer_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));

  service_manager::mojom::InterfaceProviderPtr interface_provider;
  auto interface_provider_request(mojo::MakeRequest(&interface_provider));

  // Create a FrameTreeNode in the outer WebContents to host the portal, in
  // response to the creation of a portal in the renderer process.
  FrameTreeNode* outer_node = outer_contents_impl->GetFrameTree()->AddFrame(
      owner_render_frame_host_->frame_tree_node(),
      owner_render_frame_host_->GetProcess()->GetID(),
      owner_render_frame_host_->GetProcess()->GetNextRoutingID(),
      std::move(interface_provider_request),
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
          .InitWithNewPipeAndPassReceiver(),
      blink::WebTreeScopeType::kDocument, "", "", true,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), false, blink::FrameOwnerElementType::kPortal);
  outer_node->AddObserver(this);

  bool web_contents_created = false;
  if (!portal_contents_) {
    // Create the Portal WebContents.
    WebContents::CreateParams params(outer_contents_impl->GetBrowserContext());
    SetPortalContents(WebContents::Create(params));
    web_contents_created = true;
  }

  DCHECK_EQ(portal_contents_.get(), portal_contents_impl_);
  DCHECK_EQ(portal_contents_impl_->portal(), this);
  DCHECK_EQ(portal_contents_impl_->GetDelegate(), this);

  outer_contents_impl->AttachInnerWebContents(std::move(portal_contents_),
                                              outer_node->current_frame_host(),
                                              false /* is_full_page */);

  FrameTreeNode* frame_tree_node =
      portal_contents_impl_->GetMainFrame()->frame_tree_node();
  RenderFrameProxyHost* proxy_host =
      frame_tree_node->render_manager()->GetProxyToOuterDelegate();
  proxy_host->SetRenderFrameProxyCreated(true);
  portal_contents_impl_->ReattachToOuterWebContentsFrame();

  if (web_contents_created)
    PortalWebContentsCreated(portal_contents_impl_);

  devtools_instrumentation::PortalAttached(outer_contents_impl->GetMainFrame());

  return proxy_host;
}

void Portal::Navigate(const GURL& url,
                      blink::mojom::ReferrerPtr referrer,
                      NavigateCallback callback) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    mojo::ReportBadMessage("Portal::Navigate tried to use non-HTTP protocol.");
    binding_->Close();  // Also deletes |this|.
    return;
  }

  GURL out_validated_url = url;
  owner_render_frame_host_->GetSiteInstance()->GetProcess()->FilterURL(
      false, &out_validated_url);

  FrameTreeNode* portal_root = portal_contents_impl_->GetFrameTree()->root();
  RenderFrameHostImpl* portal_frame = portal_root->current_frame_host();

  // TODO(lfg): Figure out download policies for portals.
  // https://github.com/WICG/portals/issues/150
  NavigationDownloadPolicy download_policy;

  portal_root->navigator()->NavigateFromFrameProxy(
      portal_frame, url, owner_render_frame_host_->GetLastCommittedOrigin(),
      owner_render_frame_host_->GetSiteInstance(),
      mojo::ConvertTo<Referrer>(referrer), ui::PAGE_TRANSITION_LINK, false,
      download_policy, "GET", nullptr, "", nullptr, false);

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
}  // namespace

void Portal::Activate(blink::TransferableMessage data,
                      ActivateCallback callback) {
  WebContentsImpl* outer_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));

  if (outer_contents->portal()) {
    mojo::ReportBadMessage("Portal::Activate called on nested portal");
    binding_->Close();  // Also deletes |this|.
    return;
  }

  DCHECK(owner_render_frame_host_->IsCurrent())
      << "The binding should have been closed when the portal's outer "
         "FrameTreeNode was deleted due to swap out.";

  // If a navigation in the main frame is occurring, stop it if possible and
  // reject the activation if it's too late. There are a few cases here:
  // - a different RenderFrameHost has been assigned to the FrameTreeNode
  // - the same RenderFrameHost is being used, but it is committing a navigation
  // - the FrameTreeNode holds a navigation request that can't turn back but has
  //   not yet been handed off to a RenderFrameHost
  FrameTreeNode* outer_root_node = owner_render_frame_host_->frame_tree_node();
  NavigationRequest* outer_navigation = outer_root_node->navigation_request();

  // WILL_PROCESS_RESPONSE is slightly early: it happens
  // immediately before READY_TO_COMMIT (unless it's deferred), but
  // WILL_PROCESS_RESPONSE is easier to hook for tests using a
  // NavigationThrottle.
  if (owner_render_frame_host_->HasPendingCommitNavigation() ||
      (outer_navigation &&
       outer_navigation->state() >= NavigationRequest::WILL_PROCESS_RESPONSE)) {
    std::move(callback).Run(blink::mojom::PortalActivateResult::
                                kRejectedDueToPredecessorNavigation);
    return;
  }
  outer_root_node->StopLoading();

  WebContentsDelegate* delegate = outer_contents->GetDelegate();
  bool is_loading = portal_contents_impl_->IsLoading();
  std::unique_ptr<WebContents> portal_contents;

  if (portal_contents_impl_->GetOuterWebContents()) {
    FrameTreeNode* outer_frame_tree_node = FrameTreeNode::GloballyFindByID(
        portal_contents_impl_->GetOuterDelegateFrameTreeNodeId());
    outer_frame_tree_node->RemoveObserver(this);
    portal_contents = portal_contents_impl_->DetachFromOuterWebContents();
    owner_render_frame_host_->RemoveChild(outer_frame_tree_node);
  } else {
    // Portals created for predecessor pages during activation may not be
    // attached to an outer WebContents, and may not have an outer frame tree
    // node created (i.e. CreateProxyAndAttachPortal isn't called). In this
    // case, we can skip a few of the detachment steps above.
    if (RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
            portal_contents_impl_->GetMainFrame()->GetView())) {
      view->Destroy();
    }
    portal_contents_impl_->CreateRenderWidgetHostViewForRenderManager(
        portal_contents_impl_->GetRenderViewHost());
    portal_contents = std::move(portal_contents_);
  }

  auto* outer_contents_main_frame_view = static_cast<RenderWidgetHostViewBase*>(
      outer_contents->GetMainFrame()->GetView());
  auto* portal_contents_main_frame_view =
      static_cast<RenderWidgetHostViewBase*>(
          portal_contents_impl_->GetMainFrame()->GetView());

  std::vector<std::unique_ptr<ui::TouchEvent>> touch_events;

  if (outer_contents_main_frame_view) {
    // Take fallback contents from previous WebContents so that the activation
    // is smooth without flashes.
    portal_contents_main_frame_view->TakeFallbackContentFrom(
        outer_contents_main_frame_view);
    touch_events =
        outer_contents_main_frame_view->ExtractAndCancelActiveTouches();
    FlushTouchEventQueues(outer_contents_main_frame_view->host());
  }

  std::unique_ptr<WebContents> predecessor_web_contents =
      delegate->SwapWebContents(outer_contents, std::move(portal_contents),
                                true, is_loading);
  CHECK_EQ(predecessor_web_contents.get(), outer_contents);

  if (outer_contents_main_frame_view) {
    portal_contents_main_frame_view->TransferTouches(touch_events);
    // Takes ownership of SyntheticGestureController from the predecessor's
    // RenderWidgetHost. This allows the controller to continue sending events
    // to the new RenderWidgetHostView.
    portal_contents_main_frame_view->host()->TakeSyntheticGestureController(
        outer_contents_main_frame_view->host());
    outer_contents_main_frame_view->Destroy();
  }

  portal_contents_impl_->set_portal(nullptr);

  mojo::PendingAssociatedRemote<blink::mojom::Portal> pending_portal;
  auto portal_receiver = pending_portal.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::PortalClient> pending_client;
  auto client_receiver = pending_client.InitWithNewEndpointAndPassReceiver();

  Portal* portal =
      Create(portal_contents_impl_->GetMainFrame(), std::move(portal_receiver),
             std::move(pending_client));
  portal->SetPortalContents(std::move(predecessor_web_contents));

  portal_contents_impl_->GetMainFrame()->OnPortalActivated(
      portal->portal_token_, std::move(pending_portal),
      std::move(client_receiver), std::move(data), std::move(callback));

  devtools_instrumentation::PortalActivated(outer_contents->GetMainFrame());
}

void Portal::PostMessageToGuest(
    blink::TransferableMessage message,
    const base::Optional<url::Origin>& target_origin) {
  portal_contents_impl_->GetMainFrame()->ForwardMessageFromHost(
      std::move(message), owner_render_frame_host_->GetLastCommittedOrigin(),
      target_origin);
}

void Portal::PostMessageToHost(
    blink::TransferableMessage message,
    const base::Optional<url::Origin>& target_origin) {
  DCHECK(GetPortalContents());
  if (target_origin) {
    if (target_origin != owner_render_frame_host_->GetLastCommittedOrigin())
      return;
  }
  client().ForwardMessageFromGuest(
      std::move(message),
      GetPortalContents()->GetMainFrame()->GetLastCommittedOrigin(),
      target_origin);
}

void Portal::OnFrameTreeNodeDestroyed(FrameTreeNode* frame_tree_node) {
  // Listens for the deletion of the FrameTreeNode corresponding to this portal
  // in the outer WebContents (not the FrameTreeNode of the document containing
  // it). If that outer FrameTreeNode goes away, this Portal should stop
  // accepting new messages and go away as well.
  binding_->Close();  // Also deletes |this|.
}

void Portal::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  // When the portal is in an orphaned state, the RenderFrameDeleted callback is
  // used to tie the portal's lifetime to its owner.
  if (render_frame_host == owner_render_frame_host_)
    binding_->Close();  // Also deletes |this|.
}

void Portal::WebContentsDestroyed() {
  binding_->Close();  // Also deletes |this|.
}

void Portal::LoadingStateChanged(WebContents* source,
                                 bool to_different_document) {
  DCHECK_EQ(source, portal_contents_impl_);
  if (!source->IsLoading())
    client_->DispatchLoadEvent();
}

void Portal::PortalWebContentsCreated(WebContents* portal_web_contents) {
  WebContentsImpl* outer_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));
  DCHECK(outer_contents->GetDelegate());
  outer_contents->GetDelegate()->PortalWebContentsCreated(portal_web_contents);
}

base::UnguessableToken Portal::GetDevToolsFrameToken() const {
  return portal_contents_impl_->GetMainFrame()->GetDevToolsFrameToken();
}

WebContentsImpl* Portal::GetPortalContents() {
  return portal_contents_impl_;
}

void Portal::SetBindingForTesting(
    mojo::StrongAssociatedBindingPtr<blink::mojom::Portal> binding) {
  binding_ = binding;
}

void Portal::SetClientForTesting(
    mojo::AssociatedRemote<blink::mojom::PortalClient> client) {
  client_ = std::move(client);
}

void Portal::SetPortalContents(std::unique_ptr<WebContents> web_contents) {
  portal_contents_ = std::move(web_contents);
  portal_contents_impl_ = static_cast<WebContentsImpl*>(portal_contents_.get());
  portal_contents_impl_->SetDelegate(this);
  portal_contents_impl_->set_portal(this);
}

}  // namespace content
