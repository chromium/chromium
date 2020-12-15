// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mock_agent_scheduling_group.h"

#include "base/no_destructor.h"
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
  return (GetMBIMode() == features::MBIMode::kLegacy)
             ? std::make_unique<MockAgentSchedulingGroup>(
                   render_thread, mojo::PendingAssociatedReceiver<
                                      mojom::AgentSchedulingGroup>())
             : std::make_unique<MockAgentSchedulingGroup>(
                   render_thread,
                   mojo::PendingReceiver<IPC::mojom::ChannelBootstrap>());
}

MockAgentSchedulingGroup::MockAgentSchedulingGroup(
    RenderThread& render_thread,
    mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
        pending_receiver)
    : AgentSchedulingGroup(render_thread, std::move(pending_receiver)) {}

MockAgentSchedulingGroup::MockAgentSchedulingGroup(
    RenderThread& render_thread,
    mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> pending_receiver)
    : AgentSchedulingGroup(render_thread, std::move(pending_receiver)) {}

mojom::RouteProvider* MockAgentSchedulingGroup::GetRemoteRouteProvider() {
  DCHECK(!RenderThreadImpl::current());
  static base::NoDestructor<mojo::Remote<mojom::RouteProvider>> static_remote;
  if (!static_remote->is_bound()) {
    ignore_result(static_remote->BindNewPipeAndPassReceiver());
  }
  DCHECK(static_remote->is_bound());
  return static_remote->get();
}

}  // namespace content
