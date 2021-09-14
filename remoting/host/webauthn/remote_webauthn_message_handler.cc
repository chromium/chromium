// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_message_handler.h"

#include "base/notreached.h"
#include "remoting/proto/remote_webauthn.pb.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {

RemoteWebAuthnMessageHandler::RemoteWebAuthnMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)) {}

RemoteWebAuthnMessageHandler::~RemoteWebAuthnMessageHandler() = default;

void RemoteWebAuthnMessageHandler::OnConnected() {
  NOTIMPLEMENTED();
}

void RemoteWebAuthnMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  auto remote_webauthn =
      protocol::ParseMessage<protocol::RemoteWebAuthn>(message.get());
  NOTIMPLEMENTED();
}

void RemoteWebAuthnMessageHandler::OnDisconnecting() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
