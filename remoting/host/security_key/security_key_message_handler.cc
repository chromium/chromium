// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message_handler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "remoting/base/logging.h"
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
    base::OnceClosure error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message_read_stream.IsValid());
  DCHECK(message_write_stream.IsValid());
  DCHECK(ipc_client);
  DCHECK(!error_callback.is_null());
  DCHECK(error_callback_.is_null());

  if (!reader_) {
    reader_ = std::make_unique<SecurityKeyMessageReaderImpl>(
        std::move(message_read_stream));
  }

  if (!writer_) {
    writer_ = std::make_unique<SecurityKeyMessageWriterImpl>(
        std::move(message_write_stream));
  }

  ipc_client_ = std::move(ipc_client);
  error_callback_ = std::move(error_callback);

  reader_->Start(
      base::BindRepeating(&SecurityKeyMessageHandler::ProcessSecurityKeyMessage,
                          base::Unretained(this)),
      base::BindOnce(&SecurityKeyMessageHandler::OnError,
                     base::Unretained(this)));
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
  HOST_LOG << "Received message from pipe. type="
           << static_cast<int>(message_type);
  if (message_type == SecurityKeyMessageType::CONNECT) {
    HandleConnectRequest(message->payload());
  } else if (message_type == SecurityKeyMessageType::REQUEST) {
    HandleSecurityKeyRequest(message->payload());
  } else {
    // uint8_t is handled as char, so we use uint16_t to show number here.
    LOG(ERROR) << "Unknown message type: "
               << static_cast<uint16_t>(message_type);
    SendMessage(SecurityKeyMessageType::UNKNOWN_COMMAND);
  }
}

void SecurityKeyMessageHandler::HandleIpcConnectionChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendMessageWithPayload(SecurityKeyMessageType::CONNECT_RESPONSE,
                         std::string(1, kConnectResponseActiveSession));
}

void SecurityKeyMessageHandler::HandleIpcConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendMessageWithPayload(SecurityKeyMessageType::CONNECT_ERROR,
                         "Unknown error occurred during connection.");
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
        base::BindOnce(&SecurityKeyMessageHandler::HandleIpcConnectionChange,
                       base::Unretained(this)),
        base::BindOnce(&SecurityKeyMessageHandler::HandleIpcConnectionError,
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
          base::BindRepeating(
              &SecurityKeyMessageHandler::HandleSecurityKeyResponse,
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
    HOST_LOG << "Failed to send message to pipe. type="
             << static_cast<int>(message_type);
    OnError();
  } else {
    HOST_LOG << "Successfully sent message to pipe. type="
             << static_cast<int>(message_type);
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
