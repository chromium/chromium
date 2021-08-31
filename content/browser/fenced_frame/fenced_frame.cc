// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame.h"

#include "base/notreached.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "url/gurl.h"

namespace content {

FencedFrame::FencedFrame(RenderFrameHostImpl& owner_render_frame_host)
    : web_contents_(static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(&owner_render_frame_host))),
      owner_render_frame_host_(owner_render_frame_host),
      frame_tree_(std::make_unique<FrameTree>(
          web_contents_->GetBrowserContext(),
          /*delegate=*/this,
          /*navigation_controller_delegate=*/web_contents_,
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
                    /*main_frame_name=*/"");

  // TODO(crbug.com/1199679): This should be moved to FrameTree::Init.
  web_contents_->NotifySwappedFromRenderManager(
      /*old_frame=*/nullptr,
      frame_tree_->root()->render_manager()->current_frame_host(),
      /*is_main_frame=*/true);

  CreateProxyAndAttachToOuterFrameTree();
}

FencedFrame::~FencedFrame() {
  DCHECK(frame_tree_);
  frame_tree_->Shutdown();
  frame_tree_.reset();
  if (on_destroyed_callback_for_testing_)
    std::move(on_destroyed_callback_for_testing_).Run();
}

void FencedFrame::Navigate(const GURL& url) {
  FrameTreeNode* inner_root = frame_tree_->root();

  // TODO(crbug.com/1237552): Resolve the discussion around navigations being
  // treated as downloads, and implement the correct thing.
  blink::NavigationDownloadPolicy download_policy;

  // Note `initiator_frame_token` here *always* corresponds to the outer render
  // frame host, however crbug.com/1074422 points out that it is possible that
  // another same-origin-domain document can synchronously script the document
  // hosted in the outer render frame host, and thus be the true initiator of
  // the navigation even though this wouldn't be reflected here. See that bug
  // for more discussion and plans for an eventual resolution.
  const blink::LocalFrameToken initiator_frame_token =
      owner_render_frame_host_.GetFrameToken();
  inner_root->navigator().NavigateFromFrameProxy(
      inner_root->current_frame_host(), url, &initiator_frame_token,
      owner_render_frame_host_.GetProcess()->GetID(),
      owner_render_frame_host_.GetLastCommittedOrigin(),
      owner_render_frame_host_.GetSiteInstance(), content::Referrer(),
      ui::PAGE_TRANSITION_LINK,
      /*should_replace_current_entry=*/false, download_policy, "GET",
      /*post_body=*/nullptr, /*extra_headers=*/"",
      /*blob_url_loader_factory=*/nullptr,
      network::mojom::SourceLocation::New(), /*has_user_gesture=*/false,
      absl::nullopt);
}

void FencedFrame::DidStopLoading() {
  if (on_did_finish_loading_callback_for_testing_)
    std::move(on_did_finish_loading_callback_for_testing_).Run();
}

bool FencedFrame::IsHidden() {
  return web_contents_->IsHidden();
}

int FencedFrame::GetOuterDelegateFrameTreeNodeId() {
  DCHECK(outer_delegate_frame_tree_node_);
  return outer_delegate_frame_tree_node_->frame_tree_node_id();
}

RenderFrameProxyHost* FencedFrame::GetProxyToInnerMainFrame() {
  DCHECK(proxy_to_inner_main_frame_);
  return proxy_to_inner_main_frame_;
}

void FencedFrame::OnFrameTreeNodeDestroyed(
    FrameTreeNode* outer_delegate_frame_tree_node) {
  DCHECK_EQ(outer_delegate_frame_tree_node_, outer_delegate_frame_tree_node);
  owner_render_frame_host_.DestroyFencedFrame(*this);
  // Don't use `this` after this point, as it is destroyed.
}

void FencedFrame::CreateProxyAndAttachToOuterFrameTree() {
  // The fenced frame should not already be attached.
  DCHECK(!outer_delegate_frame_tree_node_);

  outer_delegate_frame_tree_node_ =
      owner_render_frame_host_.frame_tree()->AddFrame(
          &owner_render_frame_host_,
          owner_render_frame_host_.GetProcess()->GetID(),
          owner_render_frame_host_.GetProcess()->GetNextRoutingID(),
          /*frame_remote=*/mojo::NullAssociatedRemote(),
          mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
              .InitWithNewPipeAndPassReceiver(),
          /*policy_container_bind_params=*/nullptr,
          blink::mojom::TreeScopeType::kDocument, "", "", true,
          blink::LocalFrameToken(), base::UnguessableToken::Create(),
          blink::FramePolicy(), blink::mojom::FrameOwnerProperties(), false,
          blink::mojom::FrameOwnerElementType::kFencedframe);

  // Connect the outer delegate RenderFrameHost with the inner main
  // FrameTreeNode. This allows us to traverse from the outer delegate RFH
  // inward, to the inner fenced frame FrameTree.
  outer_delegate_frame_tree_node_->current_frame_host()
      ->set_inner_tree_main_frame_tree_node_id(
          frame_tree_->root()->frame_tree_node_id());

  // We observe the outer node because when it is destroyed by its parent
  // RenderFrameHostImpl, we respond to its destruction by destroying ourself
  // and the inner fenced frame FrameTree.
  outer_delegate_frame_tree_node_->AddObserver(this);

  FrameTreeNode* inner_root = frame_tree_->root();
  proxy_to_inner_main_frame_ =
      inner_root->render_manager()->CreateOuterDelegateProxy(
          owner_render_frame_host_.GetSiteInstance());

  inner_root->current_frame_host()->PropagateEmbeddingTokenToParentFrame();
}

const base::UnguessableToken& FencedFrame::GetDevToolsFrameToken() const {
  DCHECK(frame_tree_);
  return frame_tree_->GetMainFrame()->GetDevToolsFrameToken();
}

void FencedFrame::WaitForDidStopLoadingForTesting() {
  if (!frame_tree_->IsLoading())
    return;

  base::RunLoop run_loop;
  on_did_finish_loading_callback_for_testing_ = run_loop.QuitClosure();
  run_loop.Run();
}

void FencedFrame::SetOnDestroyedCallbackForTesting(base::OnceClosure cb) {
  DCHECK(!on_destroyed_callback_for_testing_);
  on_destroyed_callback_for_testing_ = std::move(cb);
}

}  // namespace content
