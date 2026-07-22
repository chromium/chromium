// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXTENSION_SESSION_H_
#define REMOTING_HOST_HOST_EXTENSION_SESSION_H_

namespace remoting {

namespace protocol {
class ExtensionMessage;
class ClientStub;
}  // namespace protocol

// Created by a `HostExtension` to store `ClientSession` specific state, and to
// handle extension messages.
class HostExtensionSession {
 public:
  virtual ~HostExtensionSession() {}

  // Called when the host receives an `ExtensionMessage` for the `ClientSession`
  // associated with this `HostExtensionSession`.
  // It returns `true` if the message was handled, and `false` otherwise.
  virtual bool OnExtensionMessage(
      protocol::ClientStub* client_stub,
      const protocol::ExtensionMessage& message) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXTENSION_SESSION_H_
