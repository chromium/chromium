// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_extension_session.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/client_stub.h"

namespace {

// Used as the type attribute of all Security Key protocol::ExtensionMessages.
const char kExtensionMessageType[] = "gnubby-auth";

// SecurityKey extension message data members.
const char kConnectionId[] = "connectionId";
const char kControlMessage[] = "control";
const char kControlOption[] = "option";
const char kDataMessage[] = "data";
const char kDataPayload[] = "data";
const char kErrorMessage[] = "error";
const char kSecurityKeyAuthV1[] = "auth-v1";
const char kMessageType[] = "type";

// Returns the command code (the first byte of the data) if it exists, or -1 if
// the data is empty.
unsigned int GetCommandCode(const std::string& data) {
  return data.empty() ? -1 : static_cast<unsigned int>(data[0]);
}

// Creates a string of byte data from a ListValue of numbers. Returns true if
// all of the list elements are numbers.
bool ConvertListToString(const base::ListValue& bytes, std::string* out) {
  out->clear();

  unsigned int byte_count = bytes.size();
  if (byte_count != 0) {
    out->reserve(byte_count);
    for (unsigned int i = 0; i < byte_count; i++) {
      auto value = bytes[i].GetIfInt();
      if (!value.has_value()) {
        return false;
      }
      out->push_back(static_cast<char>(*value));
    }
  }
  return true;
}

}  // namespace

namespace remoting {

SecurityKeyExtensionSession::SecurityKeyExtensionSession(
    base::WeakPtr<SecurityKeyAuthHandler> auth_handler,
    protocol::ClientStub* client_stub)
    : client_stub_(client_stub), auth_handler_(auth_handler) {
  DCHECK(client_stub_);
  if (auth_handler_) {
    auth_handler_->SetSendMessageCallback(
        base::BindRepeating(&SecurityKeyExtensionSession::SendMessageToClient,
                            weak_factory_.GetWeakPtr()),
        this);
  }
}

SecurityKeyExtensionSession::~SecurityKeyExtensionSession() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (auth_handler_) {
    auth_handler_->ClearSendMessageCallback(this);
  }
}

// Returns true if the |message| is a Security Key ExtensionMessage.
// This is done so the host does not pass |message| to other HostExtensions.
// TODO(joedow): Use |client_session_details| to disconnect the session if we
//               receive an invalid extension message.
bool SecurityKeyExtensionSession::OnExtensionMessage(
    ClientSessionDetails* client_session_details,
    protocol::ClientStub* client_stub,
    const protocol::ExtensionMessage& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (message.type() != kExtensionMessageType) {
    return false;
  }

  // 512KB maximum raw message size to prevent DoS/OOM on JSON parsing.
  constexpr size_t kMaxRawMessageSize = 524288;
  if (message.data().size() > kMaxRawMessageSize) {
    LOG(ERROR) << "Gnubby-auth message data exceeds maximum allowed size ("
               << message.data().size() << " > " << kMaxRawMessageSize << ").";
    return true;
  }

  std::optional<base::DictValue> value = base::JSONReader::ReadDict(
      message.data(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!value) {
    LOG(WARNING) << "Failed to retrieve data from gnubby-auth message.";
    return true;
  }

  const std::string* type = value->FindString(kMessageType);
  if (!type) {
    LOG(WARNING) << "Invalid gnubby-auth message format.";
    return true;
  }

  if (*type == kControlMessage) {
    ProcessControlMessage(*value);
  } else if (*type == kDataMessage) {
    ProcessDataMessage(*value);
  } else if (*type == kErrorMessage) {
    ProcessErrorMessage(*value);
  } else {
    VLOG(2) << "Unknown gnubby-auth message type: " << type;
  }

  return true;
}

void SecurityKeyExtensionSession::ProcessControlMessage(
    const base::DictValue& message_data) const {
  const std::string* option = message_data.FindString(kControlOption);
  if (!option) {
    LOG(WARNING) << "Could not extract control option from message.";
    return;
  }

  if (*option == kSecurityKeyAuthV1) {
    if (auth_handler_) {
      auth_handler_->CreateSecurityKeyConnection();
    }
  } else {
    VLOG(2) << "Invalid gnubby-auth control option: " << *option;
  }
}

void SecurityKeyExtensionSession::ProcessDataMessage(
    const base::DictValue& message_data) const {
  if (!auth_handler_) {
    return;
  }

  std::optional<int> connection_id_opt = message_data.FindInt(kConnectionId);
  if (!connection_id_opt.has_value()) {
    LOG(WARNING) << "Could not extract connection id from message.";
    return;
  }
  auto connection_id = *connection_id_opt;

  if (!auth_handler_->IsValidConnectionId(connection_id)) {
    LOG(WARNING) << "Unknown gnubby-auth data connection: '" << connection_id
                 << "'";
    return;
  }

  std::string response;
  const base::ListValue* bytes_list = message_data.FindList(kDataPayload);
  if (bytes_list) {
    // 64KB maximum payload size to prevent DoS/OOM.
    constexpr size_t kMaxPayloadSize = 65536;
    if (bytes_list->size() > kMaxPayloadSize) {
      LOG(ERROR) << "Gnubby data payload exceeds maximum size ("
                 << bytes_list->size() << " > " << kMaxPayloadSize << ").";
      auth_handler_->SendErrorAndCloseConnection(connection_id);
      return;
    }

    if (ConvertListToString(*bytes_list, &response)) {
      HOST_LOG << "Processing security key response: "
               << GetCommandCode(response);
      auth_handler_->SendClientResponse(connection_id, response);
      return;
    }
  }

  LOG(WARNING) << "Could not extract response data from message.";
  auth_handler_->SendErrorAndCloseConnection(connection_id);
}

void SecurityKeyExtensionSession::ProcessErrorMessage(
    const base::DictValue& message_data) const {
  if (!auth_handler_) {
    return;
  }

  std::optional<int> connection_id_opt = message_data.FindInt(kConnectionId);
  if (!connection_id_opt.has_value()) {
    LOG(WARNING) << "Could not extract connection id from message.";
    return;
  }
  auto connection_id = *connection_id_opt;

  if (auth_handler_->IsValidConnectionId(connection_id)) {
    HOST_LOG << "Sending security key error";
    auth_handler_->SendErrorAndCloseConnection(connection_id);
  } else {
    LOG(WARNING) << "Unknown gnubby-auth connection id: " << connection_id;
  }
}

void SecurityKeyExtensionSession::SendMessageToClient(int connection_id,
                                                      const std::string& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client_stub_);

  HOST_LOG << "Sending security key request: " << GetCommandCode(data);

  base::DictValue request_dict;
  request_dict.Set(kMessageType, kDataMessage);
  request_dict.Set(kConnectionId, connection_id);

  base::ListValue bytes;
  for (auto& byte : data) {
    bytes.Append(static_cast<unsigned char>(byte));
  }
  request_dict.Set(kDataPayload, std::move(bytes));

  protocol::ExtensionMessage message;
  message.set_type(kExtensionMessageType);
  message.set_data(base::WriteJson(request_dict).value());

  client_stub_->DeliverHostMessage(message);
}

}  // namespace remoting
