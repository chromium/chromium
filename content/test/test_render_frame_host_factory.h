// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_HOST_FACTORY_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_HOST_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "content/browser/renderer_host/render_frame_host_factory.h"

namespace content {

// Manages creation of the RenderFrameHostImpls; when registered, all created
// RenderFrameHostsImpls will be TestRenderFrameHosts. This
// automatically registers itself when it goes in scope, and unregisters itself
// when it goes out of scope. Since you can't have more than one factory
// registered at a time, you can only have one of these objects at a time.
class TestRenderFrameHostFactory : public RenderFrameHostFactory {
 public:
  TestRenderFrameHostFactory();

  TestRenderFrameHostFactory(const TestRenderFrameHostFactory&) = delete;
  TestRenderFrameHostFactory& operator=(const TestRenderFrameHostFactory&) =
      delete;

  ~TestRenderFrameHostFactory() override;

 protected:
  // RenderFrameHostFactory implementation.
  std::unique_ptr<RenderFrameHostImpl> CreateRenderFrameHost(
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
      scoped_refptr<BrowsingContextState> browsing_context_state) override;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_HOST_FACTORY_H_
