// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/cast_streaming/cast_message_port_impl.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "fuchsia/base/mem_buffer_util.h"
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

// Upper limit for pending FIDL messages. Messages are going to be pending as
// long as the other end of the MessagePort does not acknowledge the latest
// message. This is to prevent the queue from being overrun in case the other
// end of the FIDL MessagePort is misbehaving.
// This should cover the largest burst of messages from the Open Screen
// implementation.
constexpr size_t kMaxPendingFidlMessages = 10;

// Extracts |buffer| data into |sender_id|, |message_namespace| and |message|.
// Returns true on success.
bool ParseMessageBuffer(const fuchsia::mem::Buffer& buffer,
                        std::string* sender_id,
                        std::string* message_namespace,
                        std::string* message) {
  std::string string_buffer;
  if (!cr_fuchsia::StringFromMemBuffer(buffer, &string_buffer))
    return false;

  base::Optional<base::Value> converted_value =
      base::JSONReader::Read(string_buffer);
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

// Creates a WebMessage out of the |sender_id|, |message_namespace| and
// |message|.
fuchsia::web::WebMessage CreateWebMessage(const std::string& sender_id,
                                          const std::string& message_namespace,
                                          const std::string& message) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kKeyNamespace, message_namespace);
  value.SetStringKey(kKeySenderId, sender_id);
  value.SetStringKey(kKeyData, message);

  std::string json_message;
  CHECK(base::JSONWriter::Write(value, &json_message));

  fuchsia::mem::Buffer buffer;
  buffer.size = json_message.size();
  zx_status_t status = zx::vmo::create(json_message.size(), 0, &buffer.vmo);
  ZX_DCHECK(status == ZX_OK, status);
  status = buffer.vmo.write(json_message.data(), 0, json_message.size());
  ZX_DCHECK(status == ZX_OK, status);

  fuchsia::web::WebMessage web_message;
  web_message.set_data(std::move(buffer));

  return web_message;
}

}  // namespace

CastMessagePortImpl::CastMessagePortImpl(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request)
    : message_port_binding_(this, std::move(message_port_request)) {
  DVLOG(1) << __func__;
  DCHECK(message_port_binding_.is_bound());
  message_port_binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "MessagePort disconnected.";
    MaybeCloseWithEpitaph(ZX_ERR_BAD_STATE);
  });

  // Initialize the connection with the Cast Streaming Sender.
  PostMessage(kValueSystemSenderId, kSystemNamespace, kInitialConnectMessage);
}

CastMessagePortImpl::~CastMessagePortImpl() = default;

void CastMessagePortImpl::MaybeSendMessageToFidl() {
  DVLOG(3) << __func__;
  if (!receive_message_callback_ || pending_fidl_messages_.empty())
    return;

  receive_message_callback_(std::move(pending_fidl_messages_.front()));
  receive_message_callback_ = nullptr;
  pending_fidl_messages_.pop_front();
}

void CastMessagePortImpl::MaybeCloseWithEpitaph(zx_status_t epitaph) {
  if (message_port_binding_.is_bound())
    message_port_binding_.Close(epitaph);
  if (client_) {
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2CastSocketError));
  }
  pending_fidl_messages_.clear();
}

void CastMessagePortImpl::SetClient(
    openscreen::cast::MessagePort::Client* client,
    std::string client_sender_id) {
  DVLOG(2) << __func__;
  DCHECK_NE(!client_, !client);
  client_ = client;
  if (!client_)
    MaybeCloseWithEpitaph(ZX_OK);
}

void CastMessagePortImpl::ResetClient() {
  client_ = nullptr;
  MaybeCloseWithEpitaph(ZX_OK);
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
  if (!message_port_binding_.is_bound())
    return;

  if (pending_fidl_messages_.size() > kMaxPendingFidlMessages) {
    LOG(ERROR) << "Too many buffered Open Screen messages.";
    MaybeCloseWithEpitaph(ZX_ERR_BAD_STATE);
    return;
  }

  DVLOG(3) << "Received Open Screen message. SenderId: " << sender_id
           << ". Namespace: " << message_namespace << ". Message: " << message;

  pending_fidl_messages_.push_back(
      CreateWebMessage(sender_id, message_namespace, message));
  MaybeSendMessageToFidl();
}

void CastMessagePortImpl::PostMessage(
    fuchsia::web::WebMessage message,
    fuchsia::web::MessagePort::PostMessageCallback callback) {
  DVLOG(3) << __func__;

  // If |client_| was cleared, the binding should have been closed.
  DCHECK(client_);

  std::string sender_id;
  std::string message_namespace;
  std::string str_message;
  if (!ParseMessageBuffer(message.data(), &sender_id, &message_namespace,
                          &str_message)) {
    LOG(ERROR) << "Received bad message.";
    client_->OnError(
        openscreen::Error(openscreen::Error::Code::kCastV2InvalidMessage));
    return;
  }
  DVLOG(3) << "Received FIDL message. SenderId: " << sender_id
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
}

void CastMessagePortImpl::ReceiveMessage(
    fuchsia::web::MessagePort::ReceiveMessageCallback callback) {
  DVLOG(3) << __func__;
  if (receive_message_callback_) {
    MaybeCloseWithEpitaph(ZX_ERR_BAD_STATE);
    return;
  }

  receive_message_callback_ = std::move(callback);
  MaybeSendMessageToFidl();
}

}  // namespace cast_streaming
