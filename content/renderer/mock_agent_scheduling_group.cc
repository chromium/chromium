// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mock_agent_scheduling_group.h"

#include <tuple>

#include "content/renderer/render_thread_impl.h"

namespace {

static features::MBIMode GetMBIMode() {
  return base::FeatureList::IsEnabled(features::kMBIMode)
             ? features::kMBIModeParam.Get()
             : features::MBIMode::kLegacy;
}

}  // namespace

namespace content {

// static
std::unique_ptr<MockAgentSchedulingGroup> MockAgentSchedulingGroup::Create(
    RenderThread& render_thread) {
  auto agent_scheduling_group =
      (GetMBIMode() == features::MBIMode::kLegacy)
          ? std::make_unique<MockAgentSchedulingGroup>(
                base::PassKey<MockAgentSchedulingGroup>(), render_thread,
                mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>())
          : std::make_unique<MockAgentSchedulingGroup>(
                base::PassKey<MockAgentSchedulingGroup>(), render_thread,
                mojo::PendingReceiver<IPC::mojom::ChannelBootstrap>());
  agent_scheduling_group->Init();
  return agent_scheduling_group;
}

MockAgentSchedulingGroup::MockAgentSchedulingGroup(
    base::PassKey<MockAgentSchedulingGroup> pass_key,
    RenderThread& render_thread,
    mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
        pending_receiver)
    : AgentSchedulingGroup(render_thread, std::move(pending_receiver)) {}

MockAgentSchedulingGroup::MockAgentSchedulingGroup(
    base::PassKey<MockAgentSchedulingGroup> pass_key,
    RenderThread& render_thread,
    mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> pending_receiver)
    : AgentSchedulingGroup(render_thread, std::move(pending_receiver)) {}

void MockAgentSchedulingGroup::Init() {
  mojo::AssociatedRemote<mojom::AgentSchedulingGroupHost>
      agent_scheduling_group_host;
  std::ignore =
      agent_scheduling_group_host.BindNewEndpointAndPassDedicatedReceiver();
  mojo::AssociatedRemote<mojom::RouteProvider> browser_route_provider;
  std::ignore =
      browser_route_provider.BindNewEndpointAndPassDedicatedReceiver();

  BindAssociatedInterfaces(
      agent_scheduling_group_host.Unbind(),
      mojo::PendingAssociatedReceiver<mojom::RouteProvider>());
}

}  // namespace content
