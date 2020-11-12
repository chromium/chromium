// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/cast_streaming/cast_message_port_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace cast_streaming {

namespace {

// TODO(b/156118960): Remove all these when Cast messages are handled by Open
// Screen.
const char kMirroringNamespace[] = "urn:x-cast:com.google.cast.webrtc";
const char kRemotingNamespace[] = "urn:x-cast:com.google.cast.remoting";
const char kSystemNamespace[] = "urn:x-cast:com.google.cast.system";
const char kInjectNamespace[] = "urn:x-cast:com.google.cast.inject";

const char kKeySenderId[] = "senderId";
const char kKeyNamespace[] = "namespace";
const char kKeyData[] = "data";
const char kKeyType[] = "type";
const char kKeyRequestId[] = "requestId";
const char kKeyCode[] = "code";

const char kValueSystemSenderId[] = "SystemSender";
const char kValueWrapped[] = "WRAPPED";
const char kValueError[] = "ERROR";
const char kValueWrappedError[] = "WRAPPED_ERROR";
const char kValueInjectNotSupportedError[] =
    R"({"code":"NOT_SUPPORTED","type":"ERROR"})";

const char kInitialConnectMessage[] = R"(
    {
      "type": "ready",
      "activeNamespaces": [
        "urn:x-cast:com.google.cast.webrtc",
        "urn:x-cast:com.google.cast.remoting",
        "urn:x-cast:com.google.cast.inject"
      ],
      "version": "2.0.0",
      "messagesVersion": "1.0"
    }
    )";

// Extracts |buffer| data into |sender_id|, |message_namespace| and |message|.
// Returns true on success.
bool ParseMessageBuffer(base::StringPiece buffer,
                        std::string* sender_id,
                        std::string* message_namespace,
                        std::string* message) {
  base::Optional<base::Value> converted_value = base::JSONReader::Read(buffer);
  if (!converted_value)
    return false;

  const std::string* sender_id_value =
      converted_value->FindStringPath(kKeySenderId);
  if (!sender_id_value)
    return false;
  *sender_id = *sender_id_value;

  const std::string* message_namespace_value =
      converted_value->FindStringPath(kKeyNamespace);
  if (!message_namespace_value)
    return false;
  *message_namespace = *message_namespace_value;

  const std::string* message_value = converted_value->FindStringPath(kKeyData);
  if (!message_value)
    return false;
  *message = *message_value;

  return true;
}

// Creates a message string out of the |sender_id|, |message_namespace| and
// |message|.
std::string CreateStringMessage(const std::string& sender_id,
                                const std::string& message_namespace,
                                const std::string& message) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kKeyNamespace, message_namespace);
  value.SetStringKey(kKeySenderId, sender_id);
  value.SetStringKey(kKeyData, message);

  std::string json_message;
  CHECK(base::JSONWriter::Write(value, &json_message));
  return json_message;
}

}  // namespace

CastMessagePortImpl::CastMessagePortImpl(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port)
    : message_port_(std::move(message_port)) {
  DVLOG(1) << __func__;
  message_port_->SetReceiver(this);

  // Initialize the connection with the Cast Streaming Sender.
  PostMessage(kValueSystemSenderId, kSystemNamespace, kInitialConnectMessage);
}

CastMessagePortImpl::~CastMessagePortImpl() = default;

void CastMessagePortImpl::MaybeClose() {
  if (message_port_)
    message_port_.reset();
  if (client_) {
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2CastSocketError));
  }
}

void CastMessagePortImpl::SetClient(
    openscreen::cast::MessagePort::Client* client,
    std::string client_sender_id) {
  DVLOG(2) << __func__;
  DCHECK_NE(!client_, !client);
  client_ = client;
  if (!client_)
    MaybeClose();
}

void CastMessagePortImpl::ResetClient() {
  client_ = nullptr;
  MaybeClose();
}

void CastMessagePortImpl::SendInjectResponse(const std::string& sender_id,
                                             const std::string& message) {
  base::Optional<base::Value> value = base::JSONReader::Read(message);
  if (!value) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": not a json payload:" << message;
    return;
  }

  if (!value->is_dict()) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": non-dictionary json payload: " << message;
    return;
  }

  const std::string* type = value->FindStringKey(kKeyType);
  if (!type) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no message type: " << message;
    return;
  }
  if (*type != kValueWrapped) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": unknown message type: " << *type;
    return;
  }

  base::Optional<int> request_id = value->FindIntKey(kKeyRequestId);
  if (!request_id) {
    LOG(ERROR) << "Malformed message from sender " << sender_id
               << ": no request id: " << message;
    return;
  }

  // Build the response message.
  base::Value response_value(base::Value::Type::DICTIONARY);
  response_value.SetKey(kKeyType, base::Value(kValueError));
  response_value.SetKey(kKeyRequestId, base::Value(request_id.value()));
  response_value.SetKey(kKeyData, base::Value(kValueInjectNotSupportedError));
  response_value.SetKey(kKeyCode, base::Value(kValueWrappedError));

  std::string json_message;
  CHECK(base::JSONWriter::Write(response_value, &json_message));
  PostMessage(sender_id, kInjectNamespace, json_message);
}

void CastMessagePortImpl::PostMessage(const std::string& sender_id,
                                      const std::string& message_namespace,
                                      const std::string& message) {
  DVLOG(3) << __func__;
  if (!message_port_)
    return;

  DVLOG(3) << "Received Open Screen message. SenderId: " << sender_id
           << ". Namespace: " << message_namespace << ". Message: " << message;
  message_port_->PostMessage(
      CreateStringMessage(sender_id, message_namespace, message));
}

bool CastMessagePortImpl::OnMessage(
    base::StringPiece message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  DVLOG(3) << __func__;

  // If |client_| was cleared, |message_port_| should have been reset.
  DCHECK(client_);

  if (!ports.empty()) {
    // We should never receive any ports for Cast Streaming.
    LOG(ERROR) << "Received ports on Cast Streaming MessagePort.";
    MaybeClose();
    return false;
  }

  std::string sender_id;
  std::string message_namespace;
  std::string str_message;
  if (!ParseMessageBuffer(message, &sender_id, &message_namespace,
                          &str_message)) {
    LOG(ERROR) << "Received bad message.";
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2InvalidMessage));
    return false;
  }
  DVLOG(3) << "Received Cast message. SenderId: " << sender_id
           << ". Namespace: " << message_namespace
           << ". Message: " << str_message;

  // TODO(b/156118960): Have Open Screen handle message namespaces.
  if (message_namespace == kMirroringNamespace ||
      message_namespace == kRemotingNamespace) {
    client_->OnMessage(sender_id, message_namespace, str_message);
  } else if (message_namespace == kInjectNamespace) {
    SendInjectResponse(sender_id, str_message);
  } else if (message_namespace != kSystemNamespace) {
    // System messages are ignored, log messages from unknown namespaces.
    DVLOG(2) << "Unknown message from " << sender_id
             << ", namespace=" << message_namespace
             << ", message=" << str_message;
  }

  return true;
}

void CastMessagePortImpl::OnPipeError() {
  DVLOG(3) << __func__;
  MaybeClose();
}

}  // namespace cast_streaming
