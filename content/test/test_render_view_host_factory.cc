// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host_factory.h"

#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/agent_scheduling_group_host_factory.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_render_widget_host.h"

namespace content {

TestRenderViewHostFactory::TestRenderViewHostFactory(
    RenderProcessHostFactory* rph_factory,
    AgentSchedulingGroupHostFactory* asgh_factory) {
  RenderProcessHostImpl::set_render_process_host_factory_for_testing(
      rph_factory);
  AgentSchedulingGroupHost::set_agent_scheduling_group_host_factory_for_testing(
      asgh_factory);
  RenderViewHostFactory::RegisterFactory(this);
}

TestRenderViewHostFactory::~TestRenderViewHostFactory() {
  RenderViewHostFactory::UnregisterFactory();
  RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
  AgentSchedulingGroupHost::set_agent_scheduling_group_host_factory_for_testing(
      nullptr);
}

void TestRenderViewHostFactory::set_render_process_host_factory(
    RenderProcessHostFactory* rph_factory) {
  RenderProcessHostImpl::set_render_process_host_factory_for_testing(
      rph_factory);
}

RenderViewHostImpl* TestRenderViewHostFactory::CreateRenderViewHost(
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
    std::optional<viz::FrameSinkId> frame_sink_id) {
  return new TestRenderViewHost(
      frame_tree, group, storage_partition_config,
      TestRenderWidgetHost::Create(
          frame_tree, widget_delegate,
          frame_sink_id.value_or(
              RenderWidgetHostImpl::DefaultFrameSinkId(*group, routing_id)),
          group->GetSafeRef(), widget_routing_id, /*hidden=*/false,
          /*renderer_initiated_creation=*/false),
      delegate, routing_id, main_frame_routing_id,
      std::move(main_browsing_context_state), create_case);
}

}  // namespace content
