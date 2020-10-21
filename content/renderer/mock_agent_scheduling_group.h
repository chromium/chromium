// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOCK_AGENT_SCHEDULING_GROUP_H_
#define CONTENT_RENDERER_MOCK_AGENT_SCHEDULING_GROUP_H_

#include "base/callback.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_export.h"
#include "content/renderer/agent_scheduling_group.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class RenderThread;

// A mock of `AgentSchedulingGroup`, that exists only to provide some test-only
// overrides of the base class methods. Meant to be used in unit tests, where
// the `AgentSchedulingGroup` is not actually wired up to its corresponding host
// in the browser process.
class MockAgentSchedulingGroup : public AgentSchedulingGroup {
 public:
  // `mojo_disconnect_handler` is an optional callback that will be called with
  // `this` when `receiver` is disconnected.
  MockAgentSchedulingGroup(
      RenderThread& render_thread,
      mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost>
          host_remote,
      mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver);

  mojom::RouteProvider* GetRemoteRouteProvider() override;
};

}  // namespace content

#endif
