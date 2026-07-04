// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_DATA_CHANNEL_HANDLER_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_DATA_CHANNEL_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

class SecurityKeyAuthHandler;

// Binds a WebRTC NamedMessagePipe to the SecurityKeyAuthHandler.
// This class manages its own lifetime and deletes itself when the pipe is
// closed (behavior inherited from NamedMessagePipeHandler).
class SecurityKeyDataChannelHandler final
    : public protocol::NamedMessagePipeHandler {
 public:
  static constexpr char kChannelName[] = "security-key";

  SecurityKeyDataChannelHandler(
      std::unique_ptr<protocol::MessagePipe> pipe,
      base::WeakPtr<SecurityKeyAuthHandler> auth_handler,
      base::OnceClosure takeover_callback);

  SecurityKeyDataChannelHandler(const SecurityKeyDataChannelHandler&) = delete;
  SecurityKeyDataChannelHandler& operator=(
      const SecurityKeyDataChannelHandler&) = delete;

  ~SecurityKeyDataChannelHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

  base::WeakPtr<SecurityKeyDataChannelHandler> GetWeakPtr();

 private:
  // Called by SecurityKeyAuthHandler when it has a message to send to the
  // client.
  void SendMessageToClient(int connection_id, const std::string& data);

  // Closes the connection on the host and sends an error response to the
  // client, keeping the data channel open.
  void SendErrorToClientAndCloseConnection(int connection_id);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<SecurityKeyAuthHandler> auth_handler_;

  base::OnceClosure takeover_callback_;

  base::WeakPtrFactory<SecurityKeyDataChannelHandler> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_DATA_CHANNEL_HANDLER_H_
