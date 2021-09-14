// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_MESSAGE_HANDLER_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_MESSAGE_HANDLER_H_

#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

class RemoteWebAuthnMessageHandler final
    : public protocol::NamedMessagePipeHandler {
 public:
  RemoteWebAuthnMessageHandler(const std::string& name,
                               std::unique_ptr<protocol::MessagePipe> pipe);
  ~RemoteWebAuthnMessageHandler() override;

  RemoteWebAuthnMessageHandler(const RemoteWebAuthnMessageHandler&) = delete;
  RemoteWebAuthnMessageHandler& operator=(const RemoteWebAuthnMessageHandler&) =
      delete;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_MESSAGE_HANDLER_H_