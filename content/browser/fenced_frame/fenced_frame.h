// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_

#include <memory>
#include <string>

#include "base/memory/safe_ref.h"
#include "content/browser/renderer_host/frame_tree_node.h"
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
                                   public FrameTreeNode::Observer {
 public:
  explicit FencedFrame(
      base::SafeRef<RenderFrameHostImpl> owner_render_frame_host);
  ~FencedFrame() override;

  void Bind(mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
                receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // blink::mojom::FencedFrameOwnerHost implementation.
  void Navigate(const GURL& url) override;

  // FrameTree::Delegate.
  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool to_different_document) override {}
  void DidStopLoading() override;
  void DidChangeLoadProgress() override {}
  bool IsHidden() override;
  void NotifyPageChanged(PageImpl& page) override {}
  int GetOuterDelegateFrameTreeNodeId() override;

  // FrameTreeNode::Observer.
  // We are monitoring the destruction of the outer delegate dummy
  // FrameTreeNode. That node is a direct child of `owner_render_frame_host_`,
  // so in order to make the lifetime of `this` fenced frame perfectly match
  // that of a traditional child node, we tie ourselves directly to its
  // destruction.
  void OnFrameTreeNodeDestroyed(FrameTreeNode*) override;

  // TODO(crbug.com/1123606): Make FencedFrame a NavigationControllerDelegate
  // to suppress certain events about the fenced frame from being exposed to the
  // outer WebContents.

  RenderFrameProxyHost* GetProxyToInnerMainFrame();

  // Returns the devtools frame token of the fenced frame's inner FrameTree's
  // main frame.
  const base::UnguessableToken& GetDevToolsFrameToken() const;

  // For testing only.
  void WaitForDidStopLoadingForTesting();

  RenderFrameHostImpl* GetInnerRoot() { return frame_tree_->GetMainFrame(); }

 private:
  // Called when a fenced frame is created from a synchronous IPC from the
  // renderer. This creates a proxy to the main frame of the inner `FrameTree`,
  // for use by the embedding RenderFrameHostImpl.
  void CreateProxyAndAttachToOuterFrameTree();

  WebContentsImpl* const web_contents_;

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
  FrameTreeNode* outer_delegate_frame_tree_node_ = nullptr;
  // This is for use by the "outer" FrameTree (i.e., the one that
  // `owner_render_frame_host_` is associated with). It is set in the
  // constructor. Initially null, and only set in the constructor (indirectly
  // via `CreateProxyAndAttachToOuterFrameTree()`).
  RenderFrameProxyHost* proxy_to_inner_main_frame_ = nullptr;

  // The FrameTree that we create to host the "inner" fenced frame contents.
  std::unique_ptr<FrameTree> frame_tree_;

  base::OnceClosure on_did_finish_loading_callback_for_testing_;

  // Receives messages from the frame owner element in Blink.
  mojo::AssociatedReceiver<blink::mojom::FencedFrameOwnerHost> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_
