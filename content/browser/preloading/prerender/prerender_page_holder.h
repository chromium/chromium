// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_PAGE_HOLDER_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_PAGE_HOLDER_H_

#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

// PrerenderPageHolder creates a prerendering FrameTree and activates it on
// prerender page activation. This is created and owned by PrerenderHost.
class PrerenderPageHolder : public FrameTree::Delegate,
                            public NavigationControllerDelegate {
 public:
  explicit PrerenderPageHolder(WebContentsImpl& web_contents);
  ~PrerenderPageHolder() override;

  // FrameTree::Delegate

  // TODO(https://crbug.com/1199682): Correctly handle load events. Ignored for
  // now as it confuses WebContentsObserver instances because they can not
  // distinguish between the different FrameTrees.

  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool should_show_loading_ui) override {}
  void DidStopLoading() override;
  bool IsHidden() override;
  FrameTree* LoadingTree() override;
  void NotifyPageChanged(PageImpl& page) override {}
  int GetOuterDelegateFrameTreeNodeId() override;
  bool IsPortal() override;

  // NavigationControllerDelegate
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override {}
  void NotifyBeforeFormRepostWarningShow() override {}
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {}
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override {}
  void NotifyNavigationListPruned(
      const PrunedDetails& pruned_details) override {}
  void NotifyNavigationEntriesDeleted() override {}
  void ActivateAndShowRepostFormWarningDialog() override;
  bool ShouldPreserveAbortedURLs() override;
  WebContents* DeprecatedGetWebContents() override;
  void UpdateOverridingUserAgent() override {}

  NavigationControllerImpl& GetNavigationController() {
    return frame_tree_->controller();
  }

  WebContents* GetWebContents() { return &web_contents_; }

  FrameTree& GetPrimaryFrameTree() {
    return web_contents_.GetPrimaryFrameTree();
  }

  std::unique_ptr<StoredPage> Activate(NavigationRequest& navigation_request);

  PrerenderHost::LoadingOutcome WaitForLoadCompletionForTesting();

  FrameTree* frame_tree() { return frame_tree_.get(); }

 private:
  static void FinishWaitingForTesting(base::OnceClosure on_close,  // IN-TEST
                                      PrerenderHost::LoadingOutcome* result,
                                      PrerenderHost::LoadingOutcome status) {
    *result = status;
    std::move(on_close).Run();
  }

  // WebContents where this prerenderer is embedded.
  WebContentsImpl& web_contents_;

  // Used for testing, this closure is only set when waiting a page to be
  // either loaded for pre-rendering. |frame_tree_| provides us with a trigger
  // for when the page is loaded.
  base::OnceCallback<void(PrerenderHost::LoadingOutcome)>
      on_wait_loading_finished_;

  // Frame tree created for the prerenderer to load the page and prepare it for
  // a future activation. During activation, the prerendered page will be taken
  // out from |frame_tree_| and moved over to |web_contents_|'s primary frame
  // tree, while |frame_tree_| will be deleted.
  std::unique_ptr<FrameTree> frame_tree_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_PAGE_HOLDER_H_
