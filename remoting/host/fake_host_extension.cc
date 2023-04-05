// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_host_extension.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

class FakeExtension::Session : public HostExtensionSession {
 public:
  Session(FakeExtension* extension, const std::string& message_type);

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  ~Session() override = default;

  // HostExtensionSession interface.
  bool OnExtensionMessage(ClientSessionDetails* client_session_details,
                          protocol::ClientStub* client_stub,
                          const protocol::ExtensionMessage& message) override;

 private:
  raw_ptr<FakeExtension> extension_;
  std::string message_type_;
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
    : capability_(capability),
      session_(std::make_unique<Session>(this, message_type)) {
  session_ptr_ = session_.get();
}

FakeExtension::~FakeExtension() = default;

std::string FakeExtension::capability() const {
  return capability_;
}

std::unique_ptr<HostExtensionSession> FakeExtension::CreateExtensionSession(
    ClientSessionDetails* client_session_details,
    protocol::ClientStub* client_stub) {
  DCHECK(!was_instantiated());
  DCHECK(session_);
  was_instantiated_ = true;
  return std::move(session_);
}

// This can't be inlined in the class definition since it doesn't know that
// FakeExtension::Session is a subclass of HostExtensionSession yet.
HostExtensionSession* FakeExtension::extension_session() {
  return session_ptr_;
}

}  // namespace remoting
