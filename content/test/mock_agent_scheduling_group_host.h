// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_AGENT_SCHEDULING_GROUP_HOST_H_
#define CONTENT_TEST_MOCK_AGENT_SCHEDULING_GROUP_HOST_H_

#include <memory>

#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/agent_scheduling_group_host_factory.h"

namespace content {

class RenderProcessHost;

class MockAgentSchedulingGroupHost : public AgentSchedulingGroupHost {
 public:
  explicit MockAgentSchedulingGroupHost(RenderProcessHost& process);


 private:
};

class MockAgentSchedulingGroupHostFactory
    : public AgentSchedulingGroupHostFactory {
 public:
  MockAgentSchedulingGroupHostFactory();
  ~MockAgentSchedulingGroupHostFactory() override;

  std::unique_ptr<AgentSchedulingGroupHost> CreateAgentSchedulingGroupHost(
      RenderProcessHost& process) override;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_AGENT_SCHEDULING_GROUP_HOST_H_
