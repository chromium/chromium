// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_factory.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

// static
RenderFrameHostFactory* RenderFrameHostFactory::factory_ = nullptr;

// static
std::unique_ptr<RenderFrameHostImpl> RenderFrameHostFactory::Create(
    SiteInstance* site_instance,
    scoped_refptr<RenderViewHostImpl> render_view_host,
    RenderFrameHostDelegate* delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int32_t routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    const blink::LocalFrameToken& frame_token,
    const blink::DocumentToken& document_token,
    base::UnguessableToken devtools_frame_token,
    bool renderer_initiated_creation,
    RenderFrameHostImpl::LifecycleStateImpl lifecycle_state,
    scoped_refptr<BrowsingContextState> browsing_context_state) {
  if (factory_) {
    return factory_->CreateRenderFrameHost(
        site_instance, std::move(render_view_host), delegate, frame_tree,
        frame_tree_node, routing_id, std::move(frame_remote), frame_token,
        document_token, devtools_frame_token, renderer_initiated_creation,
        lifecycle_state, std::move(browsing_context_state));
  }
  return base::WrapUnique(new RenderFrameHostImpl(
      site_instance, std::move(render_view_host), delegate, frame_tree,
      frame_tree_node, routing_id, std::move(frame_remote), frame_token,
      document_token, devtools_frame_token, renderer_initiated_creation,
      lifecycle_state, std::move(browsing_context_state),
      frame_tree_node->frame_owner_element_type(), frame_tree_node->parent(),
      frame_tree_node->fenced_frame_status()));
}

// static
void RenderFrameHostFactory::RegisterFactory(RenderFrameHostFactory* factory) {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = factory;
}

// static
void RenderFrameHostFactory::UnregisterFactory() {
  DCHECK(factory_) << "No factory to unregister.";
  factory_ = nullptr;
}

}  // namespace content
