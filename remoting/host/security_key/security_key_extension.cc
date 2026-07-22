// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_extension.h"

#include "remoting/host/security_key/security_key_extension_session.h"

namespace remoting {

// static
const char SecurityKeyExtension::kCapability[] = "securityKey";

SecurityKeyExtension::SecurityKeyExtension(
    base::WeakPtr<SecurityKeyAuthHandler> auth_handler)
    : auth_handler_(auth_handler) {}

SecurityKeyExtension::~SecurityKeyExtension() = default;

std::string SecurityKeyExtension::capability() const {
  return kCapability;
}

std::unique_ptr<HostExtensionSession>
SecurityKeyExtension::CreateExtensionSession(
    protocol::ClientStub* client_stub) {
  return std::make_unique<SecurityKeyExtensionSession>(auth_handler_,
                                                       client_stub);
}

}  // namespace remoting
