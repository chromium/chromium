// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_HOST_EXTENSION_H_
#define REMOTING_HOST_FAKE_HOST_EXTENSION_H_

#include <string>

#include "remoting/host/host_extension.h"

namespace remoting {

class ClientSessionDetails;
class HostExtensionSession;

namespace protocol {
class ClientStub;
}

// |HostExtension| implementation that can report a specified capability, and
// reports messages matching a specified type as having been handled.
class FakeExtension : public HostExtension {
 public:
  FakeExtension(const std::string& message_type,
                const std::string& capability);

  FakeExtension(const FakeExtension&) = delete;
  FakeExtension& operator=(const FakeExtension&) = delete;

  ~FakeExtension() override;

  // HostExtension interface.
  std::string capability() const override;
  std::unique_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSessionDetails* client_session_details,
      protocol::ClientStub* client_stub) override;

  // Accessors for testing.
  bool has_handled_message() const { return has_handled_message_; }
  bool was_instantiated() const { return was_instantiated_; }

 private:
  class Session;
  friend class Session;

  // The type name of extension messages that this fake consumes.
  std::string message_type_;

  // The capability this fake reports, and requires clients to support, if any.
  std::string capability_;

  // True if a message of |message_type_| has been processed by this extension.
  bool has_handled_message_ = false;

  // True if CreateExtensionSession() was called on this extension.
  bool was_instantiated_ = false;
};

} // namespace remoting

#endif  // REMOTING_HOST_FAKE_HOST_EXTENSION_H_
