// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_FAKE_TRACING_MANAGER_H_
#define SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_FAKE_TRACING_MANAGER_H_

#include <string>

#include "base/trace_event/named_trigger.h"
#include "services/tracing/public/cpp/background_tracing/tracing_agent_observer_manager.h"

namespace tracing {

class FakeTracingAgentObserverManager : public TracingAgentObserverManager {
 public:
  FakeTracingAgentObserverManager();
  ~FakeTracingAgentObserverManager() override;

  void AddAgentObserver(AgentObserver* observer) override {}
  void RemoveAgentObserver(AgentObserver* observer) override {}
};

class FakeNamedTriggerManager : public base::trace_event::NamedTriggerManager {
 public:
  FakeNamedTriggerManager();
  ~FakeNamedTriggerManager() override;

  bool DoEmitNamedTrigger(const std::string& trigger_name,
                          std::optional<int32_t> value,
                          uint64_t flow_id) override;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_FAKE_TRACING_MANAGER_H_
