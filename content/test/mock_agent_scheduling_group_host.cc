// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/test/mock_agent_scheduling_group_host.h"

#include "content/browser/renderer_host/agent_scheduling_group_host.h"

namespace content {

class RenderProcessHost;

MockAgentSchedulingGroupHost::MockAgentSchedulingGroupHost(
    RenderProcessHost& process)
    : AgentSchedulingGroupHost(process) {}

bool MockAgentSchedulingGroupHost::Send(IPC::Message* message) {
  // Save the message in the sink.
  sink_.OnMessageReceived(*message);
  delete message;
  return true;
}

MockAgentSchedulingGroupHostFactory::MockAgentSchedulingGroupHostFactory() =
    default;

MockAgentSchedulingGroupHostFactory::~MockAgentSchedulingGroupHostFactory() =
    default;

std::unique_ptr<AgentSchedulingGroupHost>
MockAgentSchedulingGroupHostFactory::CreateAgentSchedulingGroupHost(
    RenderProcessHost& process) {
  return std::make_unique<MockAgentSchedulingGroupHost>(process);
}

}  // namespace content
