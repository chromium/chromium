// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_TRACING_AGENT_OBSERVER_MANAGER_H_
#define SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_TRACING_AGENT_OBSERVER_MANAGER_H_

#include "base/component_export.h"

namespace tracing {
namespace mojom {
class BackgroundTracingAgent;
}  // namespace mojom

class COMPONENT_EXPORT(BACKGROUND_TRACING_CPP) TracingAgentObserverManager {
 public:
  class AgentObserver {
   public:
    virtual void OnAgentAdded(
        tracing::mojom::BackgroundTracingAgent* agent) = 0;
    virtual void OnAgentRemoved(
        tracing::mojom::BackgroundTracingAgent* agent) = 0;
    virtual ~AgentObserver() = default;
  };

  virtual ~TracingAgentObserverManager() = default;

  virtual void AddAgentObserver(AgentObserver* observer) = 0;
  virtual void RemoveAgentObserver(AgentObserver* observer) = 0;

  static TracingAgentObserverManager* GetInstance();

 protected:
  static void SetInstance(TracingAgentObserverManager* instance);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_TRACING_AGENT_OBSERVER_MANAGER_H_
