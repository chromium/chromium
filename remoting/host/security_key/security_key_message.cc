// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"

namespace {

// Limit security key messages to 256KB.
const uint32_t kMaxSecurityKeyMessageByteCount = 256 * 1024;

}  // namespace

namespace remoting {

const int SecurityKeyMessage::kHeaderSizeBytes = 4;

const int SecurityKeyMessage::kMessageTypeSizeBytes = 1;

SecurityKeyMessage::SecurityKeyMessage() = default;

SecurityKeyMessage::~SecurityKeyMessage() = default;

bool SecurityKeyMessage::IsValidMessageSize(uint32_t message_size) {
  return message_size > 0 && message_size <= kMaxSecurityKeyMessageByteCount;
}

SecurityKeyMessageType SecurityKeyMessage::MessageTypeFromValue(int value) {
  // Note: The static_cast from enum value to int should be safe since the enum
  // type is an unsigned 8bit value.
  switch (value) {
    case static_cast<int>(SecurityKeyMessageType::CONNECT):
    case static_cast<int>(SecurityKeyMessageType::CONNECT_RESPONSE):
    case static_cast<int>(SecurityKeyMessageType::CONNECT_ERROR):
    case static_cast<int>(SecurityKeyMessageType::REQUEST):
    case static_cast<int>(SecurityKeyMessageType::REQUEST_RESPONSE):
    case static_cast<int>(SecurityKeyMessageType::REQUEST_ERROR):
    case static_cast<int>(SecurityKeyMessageType::UNKNOWN_COMMAND):
    case static_cast<int>(SecurityKeyMessageType::UNKNOWN_ERROR):
    case static_cast<int>(SecurityKeyMessageType::INVALID):
      return static_cast<SecurityKeyMessageType>(value);

    default:
      LOG(ERROR) << "Unknown message type passed in: " << value;
      return SecurityKeyMessageType::INVALID;
  }
}

std::unique_ptr<SecurityKeyMessage> SecurityKeyMessage::CreateMessageForTest(
    SecurityKeyMessageType type,
    const std::string& payload) {
  std::unique_ptr<SecurityKeyMessage> message(new SecurityKeyMessage());
  message->type_ = type;
  message->payload_ = payload;

  return message;
}

bool SecurityKeyMessage::ParseMessage(const std::string& message_data) {
  if (!IsValidMessageSize(message_data.size())) {
    return false;
  }

  // The first char of the message is the message type.
  type_ = MessageTypeFromValue(message_data[0]);
  if (type_ == SecurityKeyMessageType::INVALID) {
    return false;
  }

  payload_.clear();
  if (message_data.size() > kMessageTypeSizeBytes) {
    payload_ = message_data.substr(1);
  }

  return true;
}

}  // namespace remoting
