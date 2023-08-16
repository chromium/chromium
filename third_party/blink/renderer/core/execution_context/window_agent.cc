// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/window_agent.h"

#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

WindowAgent::WindowAgent(AgentGroupScheduler& agent_group_scheduler)
    : blink::Agent(agent_group_scheduler.Isolate(),
                   base::UnguessableToken::Create(),
                   v8::MicrotaskQueue::New(agent_group_scheduler.Isolate(),
                                           v8::MicrotasksPolicy::kScoped)),
      agent_group_scheduler_(&agent_group_scheduler) {
  agent_group_scheduler_->AddAgent(this);
}

WindowAgent::WindowAgent(AgentGroupScheduler& agent_group_scheduler,
                         bool is_origin_agent_cluster,
                         bool origin_agent_cluster_left_as_default)
    : blink::Agent(agent_group_scheduler.Isolate(),
                   base::UnguessableToken::Create(),
                   v8::MicrotaskQueue::New(agent_group_scheduler.Isolate(),
                                           v8::MicrotasksPolicy::kScoped),
                   is_origin_agent_cluster,
                   origin_agent_cluster_left_as_default),
      agent_group_scheduler_(&agent_group_scheduler) {
  agent_group_scheduler_->AddAgent(this);
}

WindowAgent::~WindowAgent() = default;

void WindowAgent::Trace(Visitor* visitor) const {
  blink::Agent::Trace(visitor);
  visitor->Trace(agent_group_scheduler_);
}

AgentGroupScheduler& WindowAgent::GetAgentGroupScheduler() {
  DCHECK(agent_group_scheduler_);
  return *agent_group_scheduler_;
}

bool WindowAgent::IsWindowAgent() const {
  return true;
}

void WindowAgent::PerformMicrotaskCheckpoint() {
  blink::Agent::PerformMicrotaskCheckpoint();
}

}  // namespace blink
