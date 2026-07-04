// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_data_channel_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/logging.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/proto/security_key.pb.h"

namespace remoting {

namespace {
// 64KB maximum payload size to prevent DoS/OOM.
constexpr size_t kMaxPayloadSize = 65536;
// 64KB payload + 1KB overhead for protobuf framing.
constexpr size_t kMaxMessageSize = kMaxPayloadSize + 1024;
}  // namespace

SecurityKeyDataChannelHandler::SecurityKeyDataChannelHandler(
    std::unique_ptr<protocol::MessagePipe> pipe,
    base::WeakPtr<SecurityKeyAuthHandler> auth_handler,
    base::OnceClosure takeover_callback)
    : protocol::NamedMessagePipeHandler(kChannelName, std::move(pipe)),
      auth_handler_(auth_handler),
      takeover_callback_(std::move(takeover_callback)) {
  DCHECK(auth_handler_);
}

SecurityKeyDataChannelHandler::~SecurityKeyDataChannelHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auth_handler_) {
    auth_handler_->ClearSendMessageCallback(this);
  }
}

void SecurityKeyDataChannelHandler::OnConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "SecurityKey data channel connected.";

  if (auth_handler_) {
    auth_handler_->SetSendMessageCallback(
        base::BindRepeating(&SecurityKeyDataChannelHandler::SendMessageToClient,
                            weak_factory_.GetWeakPtr()),
        this);
    auth_handler_->CreateSecurityKeyConnection();
  }

  if (takeover_callback_) {
    std::move(takeover_callback_).Run();
  }
}

void SecurityKeyDataChannelHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(message->locked());

  if (message->total_bytes() > kMaxMessageSize) {
    LOG(ERROR) << "Received message exceeding maximum size: "
               << message->total_bytes();
    return;
  }

  protocol::SecurityKeyMessage proto;
  CompoundBufferInputStream stream(message.get());
  if (!proto.ParseFromZeroCopyStream(&stream)) {
    LOG(ERROR) << "Failed to parse SecurityKeyMessage.";
    return;
  }

  if (!proto.has_connection_id()) {
    LOG(ERROR) << "SecurityKeyMessage missing connection_id.";
    return;
  }

  if (proto.error()) {
    HOST_LOG << "Received error from client for connection "
             << proto.connection_id();
    if (auth_handler_) {
      auth_handler_->SendErrorAndCloseConnection(proto.connection_id());
    }
    return;
  }

  if (!proto.has_data()) {
    LOG(ERROR) << "SecurityKeyMessage missing data.";
    return;
  }

  // Input Validation: Enforce strict 64KB limit to prevent DoS/OOM.
  if (proto.data().size() > kMaxPayloadSize) {
    LOG(ERROR) << "SecurityKeyMessage data exceeds maximum allowed size ("
               << proto.data().size() << " > " << kMaxPayloadSize << ").";
    SendErrorToClientAndCloseConnection(proto.connection_id());
    return;
  }

  if (auth_handler_) {
    // Copy for now, will be optimized in the next CL.
    auth_handler_->SendClientResponse(proto.connection_id(), proto.data());
  }
}

void SecurityKeyDataChannelHandler::OnDisconnecting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "SecurityKey data channel disconnecting.";

  if (auth_handler_) {
    auth_handler_->ClearSendMessageCallback(this);
    // TODO(crbug.com/517007701): Close all connections when the interface
    // supports it.
  }
}

void SecurityKeyDataChannelHandler::SendMessageToClient(
    int connection_id,
    const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!connected()) {
    LOG(WARNING) << "Dropped message to client: channel not connected.";
    return;
  }

  // Prevent sending oversized payloads (defense in depth).
  if (data.size() > kMaxPayloadSize) {
    LOG(ERROR)
        << "Request data from local socket exceeds maximum allowed size ("
        << data.size() << " > " << kMaxPayloadSize << ").";
    SendErrorToClientAndCloseConnection(connection_id);
    return;
  }

  protocol::SecurityKeyMessage proto;
  proto.set_connection_id(connection_id);
  proto.set_data(data);  // Copy for now, will be optimized in the next CL.

  Send(proto, base::DoNothing());
}

void SecurityKeyDataChannelHandler::SendErrorToClientAndCloseConnection(
    int connection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auth_handler_) {
    auth_handler_->SendErrorAndCloseConnection(connection_id);
  }
  protocol::SecurityKeyMessage error_proto;
  error_proto.set_connection_id(connection_id);
  error_proto.set_error(true);
  Send(error_proto, base::DoNothing());
}

base::WeakPtr<SecurityKeyDataChannelHandler>
SecurityKeyDataChannelHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
