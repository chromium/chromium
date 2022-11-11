// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

class FrameTree;
class FrameTreeNode;
class RenderFrameHostDelegate;
class RenderViewHostImpl;
class SiteInstance;

// A factory for creating RenderFrameHosts. There is a global factory function
// that can be installed for the purposes of testing to provide a specialized
// RenderFrameHostImpl class.
class CONTENT_EXPORT RenderFrameHostFactory {
 public:
  // Creates a new RenderFrameHostImpl using the currently registered factory,
  // or a regular RenderFrameHostImpl if no factory is registered.
  static std::unique_ptr<RenderFrameHostImpl> Create(
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
      scoped_refptr<BrowsingContextState> browsing_context_state);

  RenderFrameHostFactory(const RenderFrameHostFactory&) = delete;
  RenderFrameHostFactory& operator=(const RenderFrameHostFactory&) = delete;

  // Returns true if there is currently a globally-registered factory.
  static bool has_factory() { return !!factory_; }

 protected:
  RenderFrameHostFactory() {}
  virtual ~RenderFrameHostFactory() {}

  // You can derive from this class and specify an implementation for this
  // function to create an alternate kind of RenderFrameHostImpl for testing.
  virtual std::unique_ptr<RenderFrameHostImpl> CreateRenderFrameHost(
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
      scoped_refptr<BrowsingContextState> browsing_context_state) = 0;

  // Registers a factory to be called when new RenderFrameHostImpls are created.
  // We have only one global factory, so there must be no factory registered
  // before the call. This class does NOT take ownership of the pointer.
  static void RegisterFactory(RenderFrameHostFactory* factory);

  // Unregister the previously registered factory. With no factory registered,
  // regular RenderFrameHostImpls will be created.
  static void UnregisterFactory();

 private:
  // The current globally registered factory. This is null when we should create
  // regular RenderFrameHostImpls.
  static RenderFrameHostFactory* factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_FACTORY_H_
