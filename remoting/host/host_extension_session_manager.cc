// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_extension_session_manager.h"

#include "base/check.h"
#include "remoting/base/capabilities.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/host_extension.h"
#include "remoting/host/host_extension_session.h"

namespace remoting {

HostExtensionSessionManager::HostExtensionSessionManager(
    const HostExtensions& extensions,
    ClientSessionDetails* client_session_details)
    : client_session_details_(client_session_details),
      client_stub_(nullptr),
      extensions_(extensions) {}

HostExtensionSessionManager::~HostExtensionSessionManager() = default;

std::string HostExtensionSessionManager::GetCapabilities() const {
  std::string capabilities;
  for (auto* extension : extensions_) {
    const std::string& capability = extension->capability();
    if (capability.empty()) {
      continue;
    }
    if (!capabilities.empty()) {
      capabilities.append(" ");
    }
    capabilities.append(capability);
  }
  return capabilities;
}

void HostExtensionSessionManager::OnNegotiatedCapabilities(
    protocol::ClientStub* client_stub,
    const std::string& capabilities) {
  DCHECK(client_stub);
  DCHECK(!client_stub_);

  client_stub_ = client_stub;

  for (auto* extension : extensions_) {
    // If the extension requires a capability that was not negotiated then do
    // not instantiate it.
    if (!extension->capability().empty() &&
        !HasCapability(capabilities, extension->capability())) {
      continue;
    }

    std::unique_ptr<HostExtensionSession> extension_session =
        extension->CreateExtensionSession(client_session_details_,
                                          client_stub_);
    DCHECK(extension_session);

    extension_sessions_.push_back(std::move(extension_session));
  }
}

bool HostExtensionSessionManager::OnExtensionMessage(
    const protocol::ExtensionMessage& message) {
  for (const auto& session : extension_sessions_) {
    if (session->OnExtensionMessage(client_session_details_, client_stub_,
                                    message)) {
      return true;
    }
  }
  return false;
}

}  // namespace remoting
