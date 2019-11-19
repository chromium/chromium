// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_FACTORY_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"

namespace content {

class FrameTree;
class FrameTreeNode;
class RenderFrameHostDelegate;
class RenderFrameHostImpl;
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
      int32_t widget_routing_id,
      bool renderer_initiated_creation);

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
      int32_t widget_routing_id,
      bool renderer_initiated_creation) = 0;

  // Registers a factory to be called when new RenderFrameHostImpls are created.
  // We have only one global factory, so there must be no factory registered
  // before the call. This class does NOT take ownership of the pointer.
  static void RegisterFactory(RenderFrameHostFactory* factory);

  // Unregister the previously registered factory. With no factory registered,
  // regular RenderFrameHostImpls will be created.
  static void UnregisterFactory();

 private:
  // The current globally registered factory. This is NULL when we should create
  // regular RenderFrameHostImpls.
  static RenderFrameHostFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_FACTORY_H_
