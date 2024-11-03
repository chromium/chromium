// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GUEST_PAGE_HOLDER_IMPL_H_
#define CONTENT_BROWSER_GUEST_PAGE_HOLDER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_delegate.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/guest_page_holder.h"

namespace content {

class WebContentsImpl;

class GuestPageHolderImpl : public GuestPageHolder,
                            public FrameTree::Delegate,
                            public NavigationControllerDelegate {
 public:
  GuestPageHolderImpl(WebContentsImpl& owner_web_contents,
                      scoped_refptr<SiteInstance> site_instance,
                      base::WeakPtr<GuestPageHolder::Delegate> delegate);
  ~GuestPageHolderImpl() override;

  FrameTree& frame_tree() { return frame_tree_; }

  void set_outer_frame_tree_node_id(FrameTreeNodeId outer_frame_tree_node_id);

  // Returns the delegate, this may be null.
  GuestPageHolder::Delegate* delegate() { return delegate_.get(); }

  // GuestPageHolder implementation.
  NavigationController& GetController() override;
  RenderFrameHost* GetGuestMainFrame() override;

  // FrameTree::Delegate implementation.
  void LoadingStateChanged(LoadingState new_state) override;
  void DidStartLoading(FrameTreeNode* frame_tree_node) override;
  void DidStopLoading() override;
  bool IsHidden() override;
  FrameTreeNodeId GetOuterDelegateFrameTreeNodeId() override;
  RenderFrameHostImpl* GetProspectiveOuterDocument() override;
  FrameTree* LoadingTree() override;
  void SetFocusedFrame(FrameTreeNode* node, SiteInstanceGroup* source) override;
  FrameTree* GetOwnedPictureInPictureFrameTree() override;
  FrameTree* GetPictureInPictureOpenerFrameTree() override;

  // NavigationControllerDelegate implementation.
  void NotifyNavigationStateChangedFromController(
      InvalidateTypes changed_flags) override;
  void NotifyBeforeFormRepostWarningShow() override;
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override;
  void NotifyNavigationListPruned(const PrunedDetails& pruned_details) override;
  void NotifyNavigationEntriesDeleted() override;
  void ActivateAndShowRepostFormWarningDialog() override;
  bool ShouldPreserveAbortedURLs() override;
  void UpdateOverridingUserAgent() override;

 private:
  const raw_ref<WebContentsImpl> owner_web_contents_;

  base::WeakPtr<GuestPageHolder::Delegate> delegate_;

  // The outer FrameTreeNode is not known until the guest is attached.
  FrameTreeNodeId outer_frame_tree_node_id_;

  // This FrameTree contains the guest page. It has the type
  // `FrameTree::Type::kGuest`.
  FrameTree frame_tree_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GUEST_PAGE_HOLDER_IMPL_H_
