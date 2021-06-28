// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_host_extension.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/macros.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

class FakeExtension::Session : public HostExtensionSession {
 public:
  Session(FakeExtension* extension, const std::string& message_type);
  ~Session() override = default;

  // HostExtensionSession interface.
  bool OnExtensionMessage(ClientSessionDetails* client_session_details,
                          protocol::ClientStub* client_stub,
                          const protocol::ExtensionMessage& message) override;

 private:
  FakeExtension* extension_;
  std::string message_type_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

FakeExtension::Session::Session(FakeExtension* extension,
                                const std::string& message_type)
    : extension_(extension), message_type_(message_type) {}

bool FakeExtension::Session::OnExtensionMessage(
    ClientSessionDetails* client_session_details,
    protocol::ClientStub* client_stub,
    const protocol::ExtensionMessage& message) {
  if (message.type() == message_type_) {
    extension_->has_handled_message_ = true;
    return true;
  }
  return false;
}

FakeExtension::FakeExtension(const std::string& message_type,
                             const std::string& capability)
    : message_type_(message_type), capability_(capability) {}

FakeExtension::~FakeExtension() = default;

std::string FakeExtension::capability() const {
  return capability_;
}

std::unique_ptr<HostExtensionSession> FakeExtension::CreateExtensionSession(
    ClientSessionDetails* client_session_details,
    protocol::ClientStub* client_stub) {
  DCHECK(!was_instantiated());
  was_instantiated_ = true;
  return std::make_unique<Session>(this, message_type_);
}

} // namespace remoting
