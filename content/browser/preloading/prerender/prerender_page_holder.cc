// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_page_holder.h"

#include "base/run_loop.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"

namespace content {

PrerenderPageHolder::PrerenderPageHolder(WebContentsImpl& web_contents)
    : web_contents_(web_contents),
      frame_tree_(
          std::make_unique<FrameTree>(web_contents.GetBrowserContext(),
                                      this,
                                      this,
                                      &web_contents,
                                      &web_contents,
                                      &web_contents,
                                      &web_contents,
                                      &web_contents,
                                      &web_contents,
                                      FrameTree::Type::kPrerender,
                                      base::UnguessableToken::Create())) {
  scoped_refptr<SiteInstance> site_instance =
      SiteInstance::Create(web_contents.GetBrowserContext());
  frame_tree_->Init(site_instance.get(),
                    /*renderer_initiated_creation=*/false,
                    /*main_frame_name=*/"", /*opener_for_origin=*/nullptr,
                    /*frame_policy=*/blink::FramePolicy());

  // Use the same SessionStorageNamespace as the primary page for the
  // prerendering page.
  frame_tree_->controller().SetSessionStorageNamespace(
      site_instance->GetStoragePartitionConfig(),
      web_contents_.GetPrimaryFrameTree()
          .controller()
          .GetSessionStorageNamespace(
              site_instance->GetStoragePartitionConfig()));

  // TODO(https://crbug.com/1199679): This should be moved to FrameTree::Init
  web_contents_.NotifySwappedFromRenderManager(
      /*old_frame=*/nullptr,
      frame_tree_->root()->render_manager()->current_frame_host());
}

PrerenderPageHolder::~PrerenderPageHolder() {
  // If we are still waiting on test loop, we can assume the page loading step
  // has been cancelled and the PageHolder is being discarded without
  // completing loading the page.
  if (on_wait_loading_finished_)
    std::move(on_wait_loading_finished_)
        .Run(PrerenderHost::LoadingOutcome::kPrerenderingCancelled);

  if (frame_tree_)
    frame_tree_->Shutdown();
}

void PrerenderPageHolder::DidStopLoading() {
  if (on_wait_loading_finished_) {
    std::move(on_wait_loading_finished_)
        .Run(PrerenderHost::LoadingOutcome::kLoadingCompleted);
  }
}

bool PrerenderPageHolder::IsHidden() {
  return true;
}

FrameTree* PrerenderPageHolder::LoadingTree() {
  // For prerendering loading tree is the same as its frame tree as loading is
  // done at a frame tree level in the background, unlike the loading visible
  // to the user where we account for nested frame tree loading state.
  return frame_tree_.get();
}

int PrerenderPageHolder::GetOuterDelegateFrameTreeNodeId() {
  // A prerendered FrameTree is not "inner to" or "nested inside" another
  // FrameTree; it exists in parallel to the primary FrameTree of the current
  // WebContents. Therefore, it must not attempt to access the primary
  // FrameTree in the sense of an "outer delegate" relationship, so we return
  // the invalid ID here.
  return FrameTreeNode::kFrameTreeNodeInvalidId;
}

bool PrerenderPageHolder::IsPortal() {
  return false;
}

void PrerenderPageHolder::ActivateAndShowRepostFormWarningDialog() {
  // Not supported, cancel pending reload.
  GetNavigationController().CancelPendingReload();
}

bool PrerenderPageHolder::ShouldPreserveAbortedURLs() {
  return false;
}

WebContents* PrerenderPageHolder::DeprecatedGetWebContents() {
  return GetWebContents();
}

std::unique_ptr<StoredPage> PrerenderPageHolder::Activate(
    NavigationRequest& navigation_request) {
  // There should be no ongoing main-frame navigation during activation.
  // TODO(https://crbug.com/1190644): Make sure sub-frame navigations are
  // fine.
  DCHECK(!frame_tree_->root()->HasNavigation());

  // Before the root's current_frame_host is cleared, collect the subframes of
  // `frame_tree_` whose FrameTree will need to be updated.
  FrameTree::NodeRange node_range = frame_tree_->Nodes();
  std::vector<FrameTreeNode*> subframe_nodes(std::next(node_range.begin()),
                                             node_range.end());

  // Before the root's current_frame_host is cleared, collect the replication
  // state so that it can be used for post-activation validation.
  blink::mojom::FrameReplicationState prior_replication_state =
      frame_tree_->root()->current_replication_state();

  // Update FrameReplicationState::has_received_user_gesture_before_nav of the
  // prerendered page.
  //
  // On regular navigation, it is updated via a renderer => browser IPC
  // (RenderFrameHostImpl::HadStickyUserActivationBeforeNavigationChanged),
  // which is sent from blink::DocumentLoader::CommitNavigation. However,
  // this doesn't happen on prerender page activation, so the value is not
  // correctly updated without this treatment.
  //
  // The updated value will be sent to the renderer on
  // blink::mojom::Page::ActivatePrerenderedPage.
  prior_replication_state.has_received_user_gesture_before_nav =
      navigation_request.frame_tree_node()
          ->has_received_user_gesture_before_nav();

  // frame_tree_->root(). Do not add any code between here and
  // frame_tree_.reset() that calls into observer functions to minimize the
  // duration of current_frame_host being null.
  //
  // TODO(https://crbug.com/1176148): Investigate how to combine taking the
  // prerendered page and frame_tree_ destruction.
  std::unique_ptr<StoredPage> page =
      frame_tree_->root()->render_manager()->TakePrerenderedPage();

  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  std::unique_ptr<NavigationEntryImpl> nav_entry =
      GetNavigationController()
          .GetEntryWithUniqueID(page->render_frame_host()->nav_entry_id())
          ->CloneWithoutSharing(context.get());

  navigation_request.SetPrerenderActivationNavigationState(
      std::move(nav_entry), prior_replication_state);

  FrameTree& target_frame_tree = GetPrimaryFrameTree();
  DCHECK_EQ(&target_frame_tree,
            navigation_request.frame_tree_node()->frame_tree());

  // We support activating the prerenderd page only to the topmost
  // RenderFrameHost.
  CHECK(!page->render_frame_host()->GetParentOrOuterDocumentOrEmbedder());

  page->render_frame_host()->SetFrameTreeNode(*(target_frame_tree.root()));
  // Copy frame name into the replication state of the primary main frame to
  // ensure that the replication state of the primary main frame after
  // activation matches the replication state stored in the renderer.
  // TODO(https://crbug.com/1237091): Copying frame name here is suboptimal
  // and ideally we'd do this at the same time when transferring the proxies
  // from the StoredPage into RenderFrameHostManager. However, this is a
  // temporary solution until we move this into BrowsingContextState,
  // along with RenderFrameProxyHost.
  page->render_frame_host()->frame_tree_node()->set_frame_name_for_activation(
      prior_replication_state.unique_name, prior_replication_state.name);
  for (auto& it : page->proxy_hosts()) {
    it.second->set_frame_tree_node(*(target_frame_tree.root()));
  }

  // Iterate over the root RenderFrameHost's subframes and update the
  // associated frame tree. Note that subframe proxies don't need their
  // FrameTrees independently updated, since their FrameTreeNodes don't
  // change, and FrameTree references in those FrameTreeNodes will be updated
  // through RenderFrameHosts.
  //
  // TODO(https://crbug.com/1199693): Need to investigate if and how
  // pending delete RenderFrameHost objects should be handled if prerendering
  // runs all of the unload handlers; they are not currently handled here.
  // This is because pending delete RenderFrameHosts can still receive and
  // process some messages while the RenderFrameHost FrameTree and
  // FrameTreeNode are stale.
  for (FrameTreeNode* subframe_node : subframe_nodes) {
    subframe_node->SetFrameTree(target_frame_tree);
  }

  page->render_frame_host()->ForEachRenderFrameHostIncludingSpeculative(
      [this](RenderFrameHostImpl* rfh) {
        // The visibility state of the prerendering page has not been
        // updated by
        // WebContentsImpl::UpdateVisibilityAndNotifyPageAndView(). So
        // updates the visibility state using the PageVisibilityState of
        // |web_contents|.
        rfh->render_view_host()->SetFrameTreeVisibility(
            web_contents_.GetPageVisibilityState());
      });

  frame_tree_->Shutdown();
  frame_tree_.reset();

  return page;
}

PrerenderHost::LoadingOutcome
PrerenderPageHolder::WaitForLoadCompletionForTesting() {
  PrerenderHost* prerender_host =
      web_contents_.GetPrerenderHostRegistry()->FindNonReservedHostById(
          frame_tree()->root()->frame_tree_node_id());
  if (!prerender_host) {
    // The prerender may be cancelled.
    return PrerenderHost::LoadingOutcome::kPrerenderingCancelled;
  }

  PrerenderHost::LoadingOutcome status =
      PrerenderHost::LoadingOutcome::kLoadingCompleted;

  if (!frame_tree_->IsLoadingIncludingInnerFrameTrees() &&
      prerender_host->GetInitialNavigationId().has_value())
    return status;

  base::RunLoop loop;
  on_wait_loading_finished_ =
      base::BindOnce(&PrerenderPageHolder::FinishWaitingForTesting,
                     loop.QuitClosure(), &status);
  loop.Run();
  return status;
}

}  // namespace content
