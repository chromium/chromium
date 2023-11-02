// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/test_echo_extension_session.h"

#include "base/check.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/client_stub.h"

namespace {
constexpr char kExtensionMessageType[] = "test-echo";
}

namespace remoting {

TestEchoExtensionSession::TestEchoExtensionSession() = default;

TestEchoExtensionSession::~TestEchoExtensionSession() = default;

bool TestEchoExtensionSession::OnExtensionMessage(
    ClientSessionDetails* client_session_details,
    protocol::ClientStub* client_stub,
    const protocol::ExtensionMessage& message) {
  DCHECK(client_stub);

  if (message.type() != kExtensionMessageType) {
    return false;
  }

  protocol::ExtensionMessage reply;
  reply.set_type("test-echo-reply");
  if (message.has_data()) {
    reply.set_data(message.data().substr(0, 16));
  }

  client_stub->DeliverHostMessage(reply);
  return true;
}

}  // namespace remoting
