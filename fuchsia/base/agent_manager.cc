// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/agent_manager.h"

#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

namespace cr_fuchsia {

AgentManager::AgentManager(const sys::ServiceDirectory* incoming)
    : component_context_(
          incoming->Connect<fuchsia::modular::ComponentContext>()) {}

AgentManager::~AgentManager() = default;

void AgentManager::ConnectToAgentServiceUnsafe(base::StringPiece agent,
                                               base::StringPiece interface,
                                               zx::channel request) {
  auto it = agents_.find(agent);
  if (it == agents_.end()) {
    it = agents_.emplace(agent.as_string(), AgentConnection()).first;
    component_context_->ConnectToAgent(agent.as_string(),
                                       it->second.services.NewRequest(),
                                       it->second.controller.NewRequest());
    it->second.services.set_error_handler(
        [this, agent = agent.as_string()](zx_status_t status) {
          ZX_LOG(WARNING, status) << "Agent disconnected: " << agent;
          agents_.erase(agent);
        });
  }
  it->second.services->ConnectToService(interface.as_string(),
                                        std::move(request));
}

AgentManager::AgentConnection::AgentConnection() = default;

AgentManager::AgentConnection::AgentConnection(AgentConnection&& other) =
    default;

AgentManager::AgentConnection& AgentManager::AgentConnection::operator=(
    AgentConnection&& other) = default;

AgentManager::AgentConnection::~AgentConnection() = default;

}  // namespace cr_fuchsia
