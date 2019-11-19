// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message_handler.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "remoting/host/security_key/security_key_ipc_client.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "remoting/host/security_key/security_key_message_reader_impl.h"
#include "remoting/host/security_key/security_key_message_writer_impl.h"

namespace remoting {

SecurityKeyMessageHandler::SecurityKeyMessageHandler() = default;

SecurityKeyMessageHandler::~SecurityKeyMessageHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SecurityKeyMessageHandler::Start(
    base::File message_read_stream,
    base::File message_write_stream,
    std::unique_ptr<SecurityKeyIpcClient> ipc_client,
    const base::Closure& error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message_read_stream.IsValid());
  DCHECK(message_write_stream.IsValid());
  DCHECK(ipc_client);
  DCHECK(!error_callback.is_null());
  DCHECK(error_callback_.is_null());

  if (!reader_) {
    reader_.reset(
        new SecurityKeyMessageReaderImpl(std::move(message_read_stream)));
  }

  if (!writer_) {
    writer_.reset(
        new SecurityKeyMessageWriterImpl(std::move(message_write_stream)));
  }

  ipc_client_ = std::move(ipc_client);
  error_callback_ = error_callback;

  reader_->Start(
      base::Bind(&SecurityKeyMessageHandler::ProcessSecurityKeyMessage,
                 base::Unretained(this)),
      base::Bind(&SecurityKeyMessageHandler::OnError, base::Unretained(this)));
}

void SecurityKeyMessageHandler::SetSecurityKeyMessageReaderForTest(
    std::unique_ptr<SecurityKeyMessageReader> reader) {
  DCHECK(!reader_);
  reader_ = std::move(reader);
}

void SecurityKeyMessageHandler::SetSecurityKeyMessageWriterForTest(
    std::unique_ptr<SecurityKeyMessageWriter> writer) {
  DCHECK(!writer_);
  writer_ = std::move(writer);
}

void SecurityKeyMessageHandler::ProcessSecurityKeyMessage(
    std::unique_ptr<SecurityKeyMessage> message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SecurityKeyMessageType message_type = message->type();
  if (message_type == SecurityKeyMessageType::CONNECT) {
    HandleConnectRequest(message->payload());
  } else if (message_type == SecurityKeyMessageType::REQUEST) {
    HandleSecurityKeyRequest(message->payload());
  } else {
    LOG(ERROR) << "Unknown message type: "
               << static_cast<uint8_t>(message_type);
    SendMessage(SecurityKeyMessageType::UNKNOWN_COMMAND);
  }
}

void SecurityKeyMessageHandler::HandleIpcConnectionChange(
    bool connection_established) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (connection_established) {
    SendMessageWithPayload(SecurityKeyMessageType::CONNECT_RESPONSE,
                           std::string(1, kConnectResponseActiveSession));
  } else {
    SendMessageWithPayload(SecurityKeyMessageType::CONNECT_RESPONSE,
                           std::string(1, kConnectResponseNoSession));
    // We expect the server to close the IPC channel in this scenario.
    expect_ipc_channel_close_ = true;
  }
}

void SecurityKeyMessageHandler::HandleIpcConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!expect_ipc_channel_close_) {
    SendMessageWithPayload(SecurityKeyMessageType::CONNECT_ERROR,
                           "Unknown error occurred during connection.");
  }
}

void SecurityKeyMessageHandler::HandleSecurityKeyResponse(
    const std::string& response_data) {
  if (response_data.compare(kSecurityKeyConnectionError) == 0) {
    SendMessageWithPayload(SecurityKeyMessageType::REQUEST_ERROR,
                           "An error occurred during the request.");
    return;
  }

  if (response_data.empty()) {
    SendMessageWithPayload(SecurityKeyMessageType::REQUEST_ERROR,
                           "Invalid client response received.");
    return;
  }

  SendMessageWithPayload(SecurityKeyMessageType::REQUEST_RESPONSE,
                         response_data);
}

void SecurityKeyMessageHandler::HandleConnectRequest(
    const std::string& message_payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!message_payload.empty()) {
    SendMessageWithPayload(SecurityKeyMessageType::CONNECT_ERROR,
                           "Unexpected payload data received.");
    return;
  }

  if (ipc_client_->CheckForSecurityKeyIpcServerChannel()) {
    // If we find an IPC server, then attempt to establish a connection.
    ipc_client_->EstablishIpcConnection(
        base::Bind(&SecurityKeyMessageHandler::HandleIpcConnectionChange,
                   base::Unretained(this)),
        base::Bind(&SecurityKeyMessageHandler::HandleIpcConnectionError,
                   base::Unretained(this)));
  } else {
    SendMessageWithPayload(SecurityKeyMessageType::CONNECT_RESPONSE,
                           std::string(1, kConnectResponseNoSession));
  }
}

void SecurityKeyMessageHandler::HandleSecurityKeyRequest(
    const std::string& message_payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (message_payload.empty()) {
    SendMessageWithPayload(SecurityKeyMessageType::REQUEST_ERROR,
                           "Request sent without request data.");
    return;
  }

  if (!ipc_client_->SendSecurityKeyRequest(
          message_payload,
          base::Bind(&SecurityKeyMessageHandler::HandleSecurityKeyResponse,
                     base::Unretained(this)))) {
    SendMessageWithPayload(SecurityKeyMessageType::REQUEST_ERROR,
                           "Failed to send request data.");
  }
}

void SecurityKeyMessageHandler::SendMessage(
    SecurityKeyMessageType message_type) {
  if (!writer_->WriteMessage(message_type)) {
    OnError();
  }
}

void SecurityKeyMessageHandler::SendMessageWithPayload(
    SecurityKeyMessageType message_type,
    const std::string& message_payload) {
  if (!writer_->WriteMessageWithPayload(message_type, message_payload)) {
    OnError();
  }
}

void SecurityKeyMessageHandler::OnError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ipc_client_.reset();
  writer_.reset();
  reader_.reset();

  if (!error_callback_.is_null()) {
    std::move(error_callback_).Run();
  }
}

}  // namespace remoting
