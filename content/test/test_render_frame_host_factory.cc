// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host_factory.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "content/test/test_render_frame_host.h"

namespace content {

TestRenderFrameHostFactory::TestRenderFrameHostFactory() {
  RenderFrameHostFactory::RegisterFactory(this);
}

TestRenderFrameHostFactory::~TestRenderFrameHostFactory() {
  RenderFrameHostFactory::UnregisterFactory();
}

std::unique_ptr<RenderFrameHostImpl>
TestRenderFrameHostFactory::CreateRenderFrameHost(
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
  DCHECK(!renderer_initiated_creation);
  return std::make_unique<TestRenderFrameHost>(
      site_instance, std::move(render_view_host), delegate, frame_tree,
      frame_tree_node, routing_id, std::move(frame_remote), frame_token,
      document_token, devtools_frame_token, lifecycle_state,
      std::move(browsing_context_state));
}

}  // namespace content
