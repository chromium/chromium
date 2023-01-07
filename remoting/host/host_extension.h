// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXTENSION_H_
#define REMOTING_HOST_HOST_EXTENSION_H_

#include <memory>
#include <string>

namespace remoting {

class ClientSessionDetails;
class HostExtensionSession;

namespace protocol {
class ClientStub;
}

// Extends |ChromotingHost| with new functionality, and can use extension
// messages to communicate with the client.
class HostExtension {
 public:
  virtual ~HostExtension() {}

  // Returns the name of the capability for this extension. This is merged into
  // the capabilities the host reports to the client, to determine whether a
  // HostExtensionSession should be created for a particular session.
  // Returning an empty string indicates that the extension is not associated
  // with a capability.
  virtual std::string capability() const = 0;

  // Creates an extension session, which handles extension messages for a
  // client session.
  // |client_session_details| provides session details and control methods.
  // |client_stub| may be used to send messages to the session.
  // Both interfaces are valid for the lifetime of the |HostExtensionSession|.
  virtual std::unique_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSessionDetails* client_session_details,
      protocol::ClientStub* client_stub) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXTENSION_H_
