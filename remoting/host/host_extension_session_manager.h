// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXTENSION_SESSION_MANAGER_H_
#define REMOTING_HOST_HOST_EXTENSION_SESSION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace remoting {

class ClientSessionDetails;
class HostExtension;
class HostExtensionSession;

namespace protocol {
class ClientStub;
class ExtensionMessage;
}

// Helper class used to create and manage a set of HostExtensionSession
// instances depending upon the set of registered HostExtensions, and the
// set of capabilities negotiated between client and host.
class HostExtensionSessionManager {
 public:
  using HostExtensions = std::vector<HostExtension*>;

  // Creates an extension manager for the specified |extensions|.
  HostExtensionSessionManager(const HostExtensions& extensions,
                              ClientSessionDetails* client_session_details);
  virtual ~HostExtensionSessionManager();

  // Returns the union of all capabilities supported by registered extensions.
  std::string GetCapabilities() const;

  // Handles completion of authentication and capabilities negotiation, creating
  // the set of HostExtensionSessions to match the client's capabilities.
  void OnNegotiatedCapabilities(protocol::ClientStub* client_stub,
                                const std::string& capabilities);

  // Passes |message| to each HostExtensionSession in turn until the message
  // is handled, or none remain. Returns true if the message was handled.
  // It is not valid for more than one extension to handle the same message.
  bool OnExtensionMessage(const protocol::ExtensionMessage& message);

 private:
  using HostExtensionSessions =
      std::vector<std::unique_ptr<HostExtensionSession>>;

  // Passed to HostExtensionSessions to allow them to send messages,
  // disconnect the session, etc.
  ClientSessionDetails* client_session_details_;
  protocol::ClientStub* client_stub_;

  // The HostExtensions to instantiate for the session, if it reaches the
  // authenticated state.
  HostExtensions extensions_;

  // The instantiated HostExtensionSessions, used to handle extension messages.
  HostExtensionSessions extension_sessions_;

  DISALLOW_COPY_AND_ASSIGN(HostExtensionSessionManager);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXTENSION_SESSION_MANAGER_H_
