// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_FACTORY_H_

namespace content {

class AgentSchedulingGroupHost;
class RenderProcessHost;

// Factory object for creating AgentSchedulingGroupHosts. Using this factory
// allows tests to create `MockAgentSchedulingGroupHost`.
class AgentSchedulingGroupHostFactory {
 public:
  virtual ~AgentSchedulingGroupHostFactory() = default;
  virtual std::unique_ptr<AgentSchedulingGroupHost>
  CreateAgentSchedulingGroupHost(RenderProcessHost& process) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_AGENT_SCHEDULING_GROUP_HOST_FACTORY_H_
