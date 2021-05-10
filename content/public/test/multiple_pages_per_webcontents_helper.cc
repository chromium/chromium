// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/multiple_pages_per_webcontents_helper.h"

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

namespace {

class PageHolder : public TestPageHolder,
                   public FrameTree::Delegate,
                   public NavigationControllerDelegate {
 public:
  explicit PageHolder(WebContentsImpl* web_contents)
      : web_contents_(web_contents),
        frame_tree_(web_contents->GetBrowserContext(),
                    this,
                    this,
                    web_contents,
                    web_contents,
                    web_contents,
                    web_contents,
                    web_contents,
                    FrameTree::Type::kPrimary) {
    frame_tree_.Init(
        SiteInstance::Create(web_contents->GetBrowserContext()).get(), false,
        "");

    web_contents_->NotifySwappedFromRenderManager(
        nullptr, frame_tree_.root()->render_manager()->current_frame_host(),
        true);
  }

  ~PageHolder() override { frame_tree_.Shutdown(); }

  // TestPageHolder
  RenderFrameHost* GetMainFrame() override {
    return frame_tree_.root()->current_frame_host();
  }
  NavigationController& GetController() override {
    return frame_tree_.controller();
  }

  // FrameTree::Delegate
  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool to_different_document) override {}
  void DidStopLoading() override {}
  void DidChangeLoadProgress() override {}
  bool IsHidden() override { return true; }

  // NavigationControllerDelegate
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override {}
  void Stop() override { frame_tree_.StopLoading(); }
  bool IsBeingDestroyed() override { return false; }
  void NotifyBeforeFormRepostWarningShow() override {}
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {}
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override {}
  void NotifyNavigationListPruned(
      const PrunedDetails& pruned_details) override {}
  void NotifyNavigationEntriesDeleted() override {}
  void ActivateAndShowRepostFormWarningDialog() override {}
  bool ShouldPreserveAbortedURLs() override { return false; }
  WebContents* GetWebContents() override { return web_contents_; }
  void UpdateOverridingUserAgent() override {}

 private:
  WebContentsImpl* web_contents_;
  FrameTree frame_tree_;
};

}  // namespace

std::unique_ptr<TestPageHolder> CreatePageHolderForTests(
    WebContents* web_contents) {
  auto page_holder =
      std::make_unique<PageHolder>(static_cast<WebContentsImpl*>(web_contents));

  return std::move(page_holder);
}

}  // namespace content
