// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_controller_delegate.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"

class GURL;

namespace content {

class RenderFrameHostImpl;
class RenderFrameProxyHost;
class WebContentsImpl;

// This is the browser-side host object for the <fencedframe> element
// implemented in Blink. This is only used for the MPArch version of fenced
// frames, not the ShadowDOM implementation. It is owned by and stored directly
// on `RenderFrameHostImpl`.
class CONTENT_EXPORT FencedFrame : public blink::mojom::FencedFrameOwnerHost,
                                   public FrameTree::Delegate,
                                   public NavigationControllerDelegate {
 public:
  explicit FencedFrame(
      base::SafeRef<RenderFrameHostImpl> owner_render_frame_host);
  ~FencedFrame() override;

  void Bind(mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
                receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // blink::mojom::FencedFrameOwnerHost implementation.
  void Navigate(const GURL& url,
                base::TimeTicks navigation_start_time) override;

  // FrameTree::Delegate.
  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool should_show_loading_ui) override {}
  void DidStopLoading() override {}
  void DidChangeLoadProgress() override {}
  bool IsHidden() override;
  void NotifyPageChanged(PageImpl& page) override {}
  int GetOuterDelegateFrameTreeNodeId() override;
  bool IsPortal() override;
  FrameTree* LoadingTree() override;

  RenderFrameProxyHost* GetProxyToInnerMainFrame();

  // Returns the devtools frame token of the fenced frame's inner FrameTree's
  // main frame.
  const base::UnguessableToken& GetDevToolsFrameToken() const;

  RenderFrameHostImpl* GetInnerRoot() { return frame_tree_->GetMainFrame(); }

 private:
  // NavigationControllerDelegate
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override;
  void NotifyBeforeFormRepostWarningShow() override;
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override;
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override;
  void NotifyNavigationListPruned(const PrunedDetails& pruned_details) override;
  void NotifyNavigationEntriesDeleted() override;
  void ActivateAndShowRepostFormWarningDialog() override;
  bool ShouldPreserveAbortedURLs() override;
  WebContents* DeprecatedGetWebContents() override;
  void UpdateOverridingUserAgent() override;

  // Called when a fenced frame is created from a synchronous IPC from the
  // renderer. This creates a proxy to the main frame of the inner `FrameTree`,
  // for use by the embedding RenderFrameHostImpl.
  void CreateProxyAndAttachToOuterFrameTree();

  const raw_ptr<WebContentsImpl> web_contents_;

  // This is the RenderFrameHostImpl that owns the <fencedframe> element in the
  // renderer, as such this object never outlives the RenderFrameHostImpl (and
  // SafeRef will crash safely in the case of a bug). The FencedFrame may be
  // detached and destroyed before the `owner_render_frame_host_` if removed
  // from the DOM by the renderer. Otherwise, it will be detached and destroyed
  // with the current document in the ancestor `owner_render_frame_host_`.
  base::SafeRef<RenderFrameHostImpl> owner_render_frame_host_;

  // The FrameTreeNode in the outer FrameTree that represents the inner fenced
  // frame FrameTree. It is a "dummy" child FrameTreeNode that `this` is
  // responsible for adding as a child of `owner_render_frame_host_`; it is
  // initially null, and only set in the constructor (indirectly via
  // `CreateProxyAndAttachToOuterFrameTree()`).
  // Furthermore, the lifetime of `this` is directly tied to it (see
  // `OnFrameTreeNodeDestroyed()`).
  raw_ptr<FrameTreeNode> outer_delegate_frame_tree_node_ = nullptr;
  // This is for use by the "outer" FrameTree (i.e., the one that
  // `owner_render_frame_host_` is associated with). It is set in the
  // constructor. Initially null, and only set in the constructor (indirectly
  // via `CreateProxyAndAttachToOuterFrameTree()`).
  raw_ptr<RenderFrameProxyHost> proxy_to_inner_main_frame_ = nullptr;

  // The FrameTree that we create to host the "inner" fenced frame contents.
  std::unique_ptr<FrameTree> frame_tree_;

  // Receives messages from the frame owner element in Blink.
  mojo::AssociatedReceiver<blink::mojom::FencedFrameOwnerHost> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_
