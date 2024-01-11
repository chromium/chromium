// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_VIEW_HOST_FACTORY_H_
#define CONTENT_TEST_TEST_RENDER_VIEW_HOST_FACTORY_H_

#include <stdint.h>

#include "content/browser/renderer_host/render_view_host_factory.h"

namespace content {

class AgentSchedulingGroupHostFactory;
class SiteInstanceGroup;
class RenderViewHostDelegate;
class RenderProcessHostFactory;

// Manages creation of the RenderViewHosts using our special subclass. This
// automatically registers itself when it goes in scope, and unregisters itself
// when it goes out of scope. Since you can't have more than one factory
// registered at a time, you can only have one of these objects at a time.
class TestRenderViewHostFactory : public RenderViewHostFactory {
 public:
  TestRenderViewHostFactory(RenderProcessHostFactory* rph_factory,
                            AgentSchedulingGroupHostFactory* asgh_factory);

  TestRenderViewHostFactory(const TestRenderViewHostFactory&) = delete;
  TestRenderViewHostFactory& operator=(const TestRenderViewHostFactory&) =
      delete;

  ~TestRenderViewHostFactory() override;

  virtual void set_render_process_host_factory(
      RenderProcessHostFactory* rph_factory);
  RenderViewHostImpl* CreateRenderViewHost(
      FrameTree* frame_tree,
      SiteInstanceGroup* group,
      const StoragePartitionConfig& storage_partition_config,
      RenderViewHostDelegate* delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int32_t routing_id,
      int32_t main_frame_routing_id,
      int32_t widget_routing_id,
      scoped_refptr<BrowsingContextState> main_browsing_context_state,
      CreateRenderViewHostCase create_case,
      std::optional<viz::FrameSinkId> frame_sink_id) override;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_VIEW_HOST_FACTORY_H_
