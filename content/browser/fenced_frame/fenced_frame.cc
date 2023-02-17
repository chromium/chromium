// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame.h"

#include "base/notreached.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

FrameTreeNode* CreateDelegateFrameTreeNode(
    RenderFrameHostImpl* owner_render_frame_host) {
  return owner_render_frame_host->frame_tree()->AddFrame(
      &*owner_render_frame_host, owner_render_frame_host->GetProcess()->GetID(),
      owner_render_frame_host->GetProcess()->GetNextRoutingID(),
      // We're creating an dummy outer delegate node which will never have a
      // corresponding `RenderFrameImpl`, and therefore we pass null
      // remotes/receivers for connections that it would normally have to a
      // renderer process.
      /*frame_remote=*/mojo::NullAssociatedRemote(),
      /*browser_interface_broker_receiver=*/mojo::NullReceiver(),
      /*policy_container_bind_params=*/nullptr,
      /*associated_interface_provider_receiver=*/mojo::NullAssociatedReceiver(),
      blink::mojom::TreeScopeType::kDocument, "", "", true,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), false,
      blink::FrameOwnerElementType::kFencedframe,
      /*is_dummy_frame_for_inner_tree=*/true);
}

}  // namespace

FencedFrame::FencedFrame(
    base::SafeRef<RenderFrameHostImpl> owner_render_frame_host,
    blink::mojom::FencedFrameMode mode,
    bool was_discarded)
    : web_contents_(static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(&*owner_render_frame_host))),
      owner_render_frame_host_(owner_render_frame_host),
      outer_delegate_frame_tree_node_(
          CreateDelegateFrameTreeNode(&*owner_render_frame_host)),
      frame_tree_(
          std::make_unique<FrameTree>(web_contents_->GetBrowserContext(),
                                      /*delegate=*/this,
                                      /*navigation_controller_delegate=*/this,
                                      /*navigator_delegate=*/web_contents_,
                                      /*render_frame_delegate=*/web_contents_,
                                      /*render_view_delegate=*/web_contents_,
                                      /*render_widget_delegate=*/web_contents_,
                                      /*manager_delegate=*/web_contents_,
                                      /*page_delegate=*/web_contents_,
                                      FrameTree::Type::kFencedFrame)),
      mode_(mode) {
  if (was_discarded)
    frame_tree_->root()->set_was_discarded();
}

FencedFrame::~FencedFrame() {
  DCHECK(frame_tree_);
  frame_tree_->Shutdown();
  frame_tree_.reset();
}

void FencedFrame::Navigate(const GURL& url,
                           base::TimeTicks navigation_start_time) {
  // We don't need guard against a bad message in the case of prerendering since
  // we wouldn't even establish the mojo connection in that case.
  DCHECK_NE(RenderFrameHost::LifecycleState::kPrerendering,
            owner_render_frame_host_->GetLifecycleState());

  if (mode_ == blink::mojom::FencedFrameMode::kDefault &&
      !blink::IsValidFencedFrameURL(url)) {
    bad_message::ReceivedBadMessage(owner_render_frame_host_->GetProcess(),
                                    bad_message::FF_NAVIGATION_INVALID_URL);
    return;
  }

  if (mode_ == blink::mojom::FencedFrameMode::kOpaqueAds &&
      !blink::IsValidUrnUuidURL(url) && !blink::IsValidFencedFrameURL(url)) {
    bad_message::ReceivedBadMessage(owner_render_frame_host_->GetProcess(),
                                    bad_message::FF_NAVIGATION_INVALID_URL);
    return;
  }

  GURL validated_url = url;
  owner_render_frame_host_->GetSiteInstance()->GetProcess()->FilterURL(
      /*empty_allowed=*/false, &validated_url);

  FrameTreeNode* inner_root = frame_tree_->root();

  // TODO(crbug.com/1237552): Resolve the discussion around navigations being
  // treated as downloads, and implement the correct thing.
  blink::NavigationDownloadPolicy download_policy;

  // This method is only invoked in the context of the embedder navigating
  // the embeddee via a `src` attribute modification on the fenced frame
  // element. The `initiator_origin` is left as a new opaque origin since
  // we do not want to leak information from the outer frame tree to the
  // inner frame tree. Note that this will always create a "Sec-Fetch-Site" as
  // cross-site. Since we assign an opaque initiator_origin we do not
  // need to provide a `source_site_instance`.
  url::Origin initiator_origin;
  // Similarly, we don't want to leak information from the outer frame tree via
  // base url.
  GURL initiator_base_url;

  // TODO(yaoxia): implement this. This information will be propagated to the
  // `NavigationHandle`. Skip propagating here is fine for now, because we are
  // currently only interested navigation that occurs in the outermost RFH.
  blink::mojom::NavigationInitiatorActivationAndAdStatus
      initiator_activation_and_ad_status =
          blink::mojom::NavigationInitiatorActivationAndAdStatus::
              kDidNotStartWithTransientActivation;

  inner_root->navigator().NavigateFromFrameProxy(
      inner_root->current_frame_host(), validated_url,
      /*initiator_frame_token=*/nullptr,
      content::ChildProcessHost::kInvalidUniqueID, initiator_origin,
      initiator_base_url,
      /*source_site_instance=*/nullptr, content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_SUBFRAME,
      /*should_replace_current_entry=*/true, download_policy, "GET",
      /*post_body=*/nullptr, /*extra_headers=*/"",
      /*blob_url_loader_factory=*/nullptr,
      network::mojom::SourceLocation::New(), /*has_user_gesture=*/false,
      /*is_form_submission=*/false,
      /*impression=*/absl::nullopt, initiator_activation_and_ad_status,
      navigation_start_time,
      /*is_embedder_initiated_fenced_frame_navigation=*/true);
}

bool FencedFrame::IsHidden() {
  return web_contents_->IsHidden();
}

int FencedFrame::GetOuterDelegateFrameTreeNodeId() {
  DCHECK(outer_delegate_frame_tree_node_);
  return outer_delegate_frame_tree_node_->frame_tree_node_id();
}

RenderFrameHostImpl* FencedFrame::GetProspectiveOuterDocument() {
  // A fenced frame's outer document is known at initialization, so we could
  // never be in this unattached state.
  return nullptr;
}

bool FencedFrame::IsPortal() {
  return false;
}

FrameTree* FencedFrame::LoadingTree() {
  // TODO(crbug.com/1232528): Consider and fix the case when fenced frames are
  // being prerendered.
  return web_contents_->LoadingTree();
}

void FencedFrame::SetFocusedFrame(FrameTreeNode* node,
                                  SiteInstanceGroup* source) {
  web_contents_->SetFocusedFrame(node, source);
}

RenderFrameProxyHost*
FencedFrame::InitInnerFrameTreeAndReturnProxyToOuterFrameTree(
    blink::mojom::RemoteFrameInterfacesFromRendererPtr remote_frame_interfaces,
    const blink::RemoteFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token) {
  DCHECK(remote_frame_interfaces);
  DCHECK(outer_delegate_frame_tree_node_);

  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForFencedFrame(
          owner_render_frame_host_->GetSiteInstance());

  // Set the mandatory sandbox flags from the beginning.
  blink::FramePolicy frame_policy;
  frame_policy.sandbox_flags = blink::kFencedFrameForcedSandboxFlags;
  // Note that even though this is happening in response to an event in the
  // renderer (i.e., the creation of a <fencedframe> element), we are still
  // putting `renderer_initiated_creation` as false. This is because that
  // parameter is only used when a renderer is creating a new window and has
  // already created the main frame for the window, but wants the browser to
  // refrain from showing the main frame until the renderer signals the browser
  // via the mojom.LocalMainFrameHost.ShowCreatedWindow(). This flow does not
  // apply for fenced frames, portals, and prerendered nested FrameTrees, hence
  // the decision to mark it as false.
  frame_tree_->Init(site_instance.get(),
                    /*renderer_initiated_creation=*/false,
                    /*main_frame_name=*/"",
                    /*opener_for_origin=*/nullptr, frame_policy,
                    devtools_frame_token);
  // Note that pending frame policy will be passed as `frame_policy` in
  // `replication_state` in `mojom::CreateFrameParams`.
  // See `RenderFrameHostImpl::CreateRenderFrame`.
  frame_tree_->root()->SetPendingFramePolicy(frame_policy);

  // TODO(crbug.com/1199679): This should be moved to FrameTree::Init.
  web_contents_->NotifySwappedFromRenderManager(
      /*old_frame=*/nullptr,
      frame_tree_->root()->render_manager()->current_frame_host());

  // Connect the outer delegate RenderFrameHost with the inner main
  // FrameTreeNode. This allows us to traverse from the outer delegate RFH
  // inward, to the inner fenced frame FrameTree.
  outer_delegate_frame_tree_node_->current_frame_host()
      ->set_inner_tree_main_frame_tree_node_id(
          frame_tree_->root()->frame_tree_node_id());

  FrameTreeNode* inner_root = frame_tree_->root();
  // This is for use by the "outer" FrameTree (i.e., the one that
  // `owner_render_frame_host_` is associated with).
  RenderFrameProxyHost* proxy_host =
      inner_root->current_frame_host()
          ->browsing_context_state()
          ->CreateOuterDelegateProxy(
              owner_render_frame_host_->GetSiteInstance(), inner_root,
              frame_token);

  proxy_host->BindRemoteFrameInterfaces(
      std::move(remote_frame_interfaces->frame),
      std::move(remote_frame_interfaces->frame_host_receiver));

  inner_root->current_frame_host()->PropagateEmbeddingTokenToParentFrame();

  // We need to set the `proxy_host` as created because the
  // renderer side of this object is live. It is live because the creation of
  // the FencedFrame object occurs in a sync request from the renderer where the
  // other end of `proxy_host` lives.
  proxy_host->SetRenderFrameProxyCreated(true);

  RenderFrameHostManager* inner_render_manager = inner_root->render_manager();

  // For the newly minted FrameTree (in the constructor) we will have a new
  // RenderViewHost that does not yet have a RenderWidgetHostView for it.
  // Create a RenderWidgetHostViewChildFrame as this won't be a top-level
  // view. Set the associated view for the inner frame tree after it has
  // been created.
  RenderViewHost* rvh =
      inner_render_manager->current_frame_host()->GetRenderViewHost();
  if (!inner_render_manager->InitRenderView(
          inner_render_manager->current_frame_host()
              ->GetSiteInstance()
              ->group(),
          static_cast<RenderViewHostImpl*>(rvh), nullptr)) {
    return proxy_host;
  }

  RenderWidgetHostViewBase* child_rwhv =
      inner_render_manager->GetRenderWidgetHostView();
  CHECK(child_rwhv);
  CHECK(child_rwhv->IsRenderWidgetHostViewChildFrame());
  inner_render_manager->SetRWHViewForInnerFrameTree(
      static_cast<RenderWidgetHostViewChildFrame*>(child_rwhv));

  devtools_instrumentation::FencedFrameCreated(owner_render_frame_host_, this);

  return proxy_host;
}

const base::UnguessableToken& FencedFrame::GetDevToolsFrameToken() const {
  DCHECK(frame_tree_);
  return frame_tree_->GetMainFrame()->GetDevToolsFrameToken();
}

void FencedFrame::NotifyBeforeFormRepostWarningShow() {}

void FencedFrame::NotifyNavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {}

void FencedFrame::NotifyNavigationEntryChanged(
    const EntryChangedDetails& change_details) {}

void FencedFrame::NotifyNavigationListPruned(
    const PrunedDetails& pruned_details) {}

void FencedFrame::NotifyNavigationEntriesDeleted() {}

void FencedFrame::ActivateAndShowRepostFormWarningDialog() {
  // Not supported, cancel pending reload.
  frame_tree_->controller().CancelPendingReload();
}

bool FencedFrame::ShouldPreserveAbortedURLs() {
  return false;
}

WebContents* FencedFrame::DeprecatedGetWebContents() {
  return web_contents_;
}

void FencedFrame::UpdateOverridingUserAgent() {}

void FencedFrame::DidChangeFramePolicy(const blink::FramePolicy& frame_policy) {
  FrameTreeNode* inner_root = frame_tree_->root();
  const blink::FramePolicy& current_frame_policy =
      inner_root->pending_frame_policy();
  inner_root->SetPendingFramePolicy(blink::FramePolicy(
      current_frame_policy.sandbox_flags, frame_policy.container_policy,
      current_frame_policy.required_document_policy));
}

}  // namespace content
