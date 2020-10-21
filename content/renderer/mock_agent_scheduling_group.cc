// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mock_agent_scheduling_group.h"

#include "base/no_destructor.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

MockAgentSchedulingGroup::MockAgentSchedulingGroup(
    RenderThread& render_thread,
    mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost> host_remote,
    mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver)
    : AgentSchedulingGroup(render_thread,
                           std::move(host_remote),
                           std::move(receiver)) {}

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
