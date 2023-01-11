// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOCK_AGENT_SCHEDULING_GROUP_H_
#define CONTENT_RENDERER_MOCK_AGENT_SCHEDULING_GROUP_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "content/common/associated_interfaces.mojom.h"
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
  static std::unique_ptr<MockAgentSchedulingGroup> Create(
      RenderThread& render_thread);
  MockAgentSchedulingGroup(
      base::PassKey<MockAgentSchedulingGroup> pass_key,
      RenderThread& render_thread,
      mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
          pending_receiver);
  MockAgentSchedulingGroup(
      base::PassKey<MockAgentSchedulingGroup> pass_key,
      RenderThread& render_thread,
      mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> pending_receiver);

 private:
  void Init();
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOCK_AGENT_SCHEDULING_GROUP_H_
