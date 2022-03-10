// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame.h"

#include "base/notreached.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "url/gurl.h"

namespace content {

namespace {

FrameTreeNode* CreateDelegateFrameTreeNode(
    RenderFrameHostImpl* owner_render_frame_host) {
  return owner_render_frame_host->frame_tree()->AddFrame(
      &*owner_render_frame_host, owner_render_frame_host->GetProcess()->GetID(),
      owner_render_frame_host->GetProcess()->GetNextRoutingID(),
      /*frame_remote=*/mojo::NullAssociatedRemote(),
      /*browser_interface_broker_receiver=*/mojo::NullReceiver(),
      /*policy_container_bind_params=*/nullptr,
      blink::mojom::TreeScopeType::kDocument, "", "", true,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), false,
      blink::FrameOwnerElementType::kFencedframe,
      /*is_dummy_frame_for_inner_tree=*/true);
}

}  // namespace

FencedFrame::FencedFrame(
    base::SafeRef<RenderFrameHostImpl> owner_render_frame_host)
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
                                      FrameTree::Type::kFencedFrame)) {
  scoped_refptr<SiteInstance> site_instance =
      SiteInstance::Create(web_contents_->GetBrowserContext());
  // Note that even though this is happening in response to an event in the
  // renderer (i.e., the creation of a <fencedframe> element), we are still
  // putting `renderer_initiated_creation` as false. This is because that
  // parameter is only used when a renderer is creating a new window and has
  // already created the main frame for the window, but wants the browser to
  // refrain from showing the main frame until the renderer signals the browser
  // via the mojom.LocalMainFrameHost.ShowCreatedWindow(). This flow does not
  // apply for fenced frames, portals, and prerendered nested FrameTrees, hence
  // the decision to mark it as false.
  frame_tree_->Init(site_instance.get(), /*renderer_initiated_creation=*/false,
                    /*main_frame_name=*/"", /*opener=*/nullptr,
                    /*frame_policy=*/blink::FramePolicy());

  // TODO(crbug.com/1199679): This should be moved to FrameTree::Init.
  web_contents_->NotifySwappedFromRenderManager(
      /*old_frame=*/nullptr,
      frame_tree_->root()->render_manager()->current_frame_host());

  CreateProxyAndAttachToOuterFrameTree();

  devtools_instrumentation::FencedFrameCreated(owner_render_frame_host_, this);
}

FencedFrame::~FencedFrame() {
  DCHECK(frame_tree_);
  frame_tree_->Shutdown();
  frame_tree_.reset();
}

void FencedFrame::Navigate(const GURL& url,
                           base::TimeTicks navigation_start_time) {
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

  inner_root->navigator().NavigateFromFrameProxy(
      inner_root->current_frame_host(), url, /*initiator_frame_token=*/nullptr,
      owner_render_frame_host_->GetProcess()->GetID(), initiator_origin,
      /*source_site_instance=*/nullptr, content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_SUBFRAME,
      /*should_replace_current_entry=*/true, download_policy, "GET",
      /*post_body=*/nullptr, /*extra_headers=*/"",
      /*blob_url_loader_factory=*/nullptr,
      network::mojom::SourceLocation::New(), /*has_user_gesture=*/false,
      /*impression=*/absl::nullopt, navigation_start_time);
}

bool FencedFrame::IsHidden() {
  return web_contents_->IsHidden();
}

int FencedFrame::GetOuterDelegateFrameTreeNodeId() {
  DCHECK(outer_delegate_frame_tree_node_);
  return outer_delegate_frame_tree_node_->frame_tree_node_id();
}

bool FencedFrame::IsPortal() {
  return false;
}

FrameTree* FencedFrame::LoadingTree() {
  // TODO(crbug.com/1232528): Consider and fix the case when fenced frames are
  // being prerendered.
  return web_contents_->LoadingTree();
}

RenderFrameProxyHost* FencedFrame::GetProxyToInnerMainFrame() {
  DCHECK(proxy_to_inner_main_frame_);
  return proxy_to_inner_main_frame_;
}

void FencedFrame::CreateProxyAndAttachToOuterFrameTree() {
  DCHECK(outer_delegate_frame_tree_node_);
  // Connect the outer delegate RenderFrameHost with the inner main
  // FrameTreeNode. This allows us to traverse from the outer delegate RFH
  // inward, to the inner fenced frame FrameTree.
  outer_delegate_frame_tree_node_->current_frame_host()
      ->set_inner_tree_main_frame_tree_node_id(
          frame_tree_->root()->frame_tree_node_id());

  FrameTreeNode* inner_root = frame_tree_->root();
  proxy_to_inner_main_frame_ =
      inner_root->render_manager()->CreateOuterDelegateProxy(
          owner_render_frame_host_->GetSiteInstance());

  inner_root->current_frame_host()->PropagateEmbeddingTokenToParentFrame();

  // We need to set the `proxy_to_inner_main_frame_` as created because the
  // renderer side of this object is live. It is live because the creation of
  // the FencedFrame object occurs in a sync request from the renderer where the
  // other end of `proxy_to_inner_main_frame_` lives.
  proxy_to_inner_main_frame_->SetRenderFrameProxyCreated(true);

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
    return;
  }

  RenderWidgetHostViewBase* child_rwhv =
      inner_render_manager->GetRenderWidgetHostView();
  CHECK(child_rwhv);
  CHECK(child_rwhv->IsRenderWidgetHostViewChildFrame());
  inner_render_manager->SetRWHViewForInnerFrameTree(
      static_cast<RenderWidgetHostViewChildFrame*>(child_rwhv));
}

const base::UnguessableToken& FencedFrame::GetDevToolsFrameToken() const {
  DCHECK(frame_tree_);
  return frame_tree_->GetMainFrame()->GetDevToolsFrameToken();
}

void FencedFrame::NotifyNavigationStateChanged(InvalidateTypes changed_flags) {}

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

}  // namespace content
