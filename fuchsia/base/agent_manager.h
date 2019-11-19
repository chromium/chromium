// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_AGENT_MANAGER_H_
#define FUCHSIA_BASE_AGENT_MANAGER_H_

#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/containers/flat_map.h"
#include "base/fuchsia/default_context.h"
#include "base/strings/string_piece.h"

namespace cr_fuchsia {

// Connects to the ComponentContext service from the supplied ServiceDirectory,
// and uses it to connect-to and manage one or more Agents used by the caller.
class AgentManager {
 public:
  explicit AgentManager(const sys::ServiceDirectory* incoming);
  ~AgentManager();

  // Connects to |agent| so satisfying the specified |request|.
  // |agent| will be kept alive until this AgentManager is destroyed.
  template <typename Interface>
  void ConnectToAgentService(base::StringPiece agent,
                             fidl::InterfaceRequest<Interface> request) {
    ConnectToAgentServiceUnsafe(agent, Interface::Name_, request.TakeChannel());
  }

  template <typename Interface>
  fidl::InterfacePtr<Interface> ConnectToAgentService(base::StringPiece agent) {
    fidl::InterfacePtr<Interface> ptr;
    ConnectToAgentService(agent, ptr.NewRequest());
    return ptr;
  }

 private:
  // Holds a pointer to Agent-provided services, and keeps the Agent alive.
  struct AgentConnection {
    AgentConnection();
    AgentConnection(AgentConnection&& other);
    ~AgentConnection();

    AgentConnection& operator=(AgentConnection&& other);

    fuchsia::sys::ServiceProviderPtr services;
    fuchsia::modular::AgentControllerPtr controller;
  };

  void ConnectToAgentServiceUnsafe(base::StringPiece agent,
                                   base::StringPiece interface,
                                   zx::channel request);

  const fuchsia::modular::ComponentContextPtr component_context_;

  // Cache of resources for Agents which we're actively using. All |agents_|
  // are kept alive until the AgentManager is torn down, for now.
  base::flat_map<std::string, AgentConnection> agents_;

  DISALLOW_COPY_AND_ASSIGN(AgentManager);
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_AGENT_MANAGER_H_
