// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace remoting {

// -----------------------------------------------------------------------------
// Introduction
// -----------------------------------------------------------------------------
//
// This file defines the message format and messages used to interact with the
// remote_security_key process which is used to forward security key requests
// to a remote client and return security key responses from the remote client.

// -----------------------------------------------------------------------------
// Message Format
// -----------------------------------------------------------------------------
//
// {Header: uint32_t}{Control Code: uint8_t}{Payload: optional, variable length}
//
// Header: Defines the length of the message (Control Code + Payload).
//         The header endianness is determined by the platform.
// Control Code: Contains a value representing the message type.
// Payload: An optional field of variable length.  Data being represented in
//          this field is dependent on the message type.
//          The endianess of the payload is dependent on the message type.
//          Multi-byte payloads which are part of the security key request and
//          response messages are expected to be big endian.  The bytes for
//          these messages will be transmitted across the network as-is.
//          The format for all other payloads is the endianness of the platform.

// -----------------------------------------------------------------------------
// Messages
// -----------------------------------------------------------------------------
// NOTE: Make sure SecurityKeyMessage::MessageTypeFromValue is updated when new
//       enum values are added/removed.
enum class SecurityKeyMessageType : uint8_t {
  INVALID = 0,

  // Sent to the remote_security_key process to ask it to establish a
  // connection to the Chromoting host if a security key enabled session
  // exists for the current user.
  // Payload length: 0
  CONNECT = 1,

  // Sent is sent by the remote_security_key process in response to a
  // |kConnectToSecurityKeyEnabledSessionMsg| request.
  // Payload length: 1 byte bool indicating connection state.
  // True(1): A connection with the Chromoting Host is established and ready
  //          to receive security key requests to forward.
  // False(0): No security key session was found.
  CONNECT_RESPONSE = 2,

  // Sent by the remote_security_key proces when an error occurs while
  // attempting to detect the existence of a Chromoting session or when
  // establishing a connection to it.
  // Payload length: variable.  If > 0 bytes, the bytes represent an error
  // string which is ascii encoded and does not include a null terminator.
  CONNECT_ERROR = 3,

  // Sent to the remote_security_key process to ask it to  forward the
  // security key request payload bytes to the remote machine.
  // Payload length: > 0 bytes consisting of the security key message to
  // forward to the remote machine using Length-Value format.
  REQUEST = 4,

  // Sent by the remote_security_key once a response has been received from
  // the remote machine.
  // Payload length: > 0 bytes consisting of the security key response from
  // the remote machine using Length-Value format.
  REQUEST_RESPONSE = 5,

  // Sent by the remote_security_key if an error occurs either in sending the
  // request data to the remote host or when receiving the response.
  // Payload length: variable.  If > 0 bytes, the bytes represent an error
  // string which is ascii encoded and does not include a null terminator.
  REQUEST_ERROR = 6,

  // Sent by the remote_security_key if it receives an unknown command.
  // Payload length: 0 bytes.
  UNKNOWN_COMMAND = 254,

  // Sent by the remote_security_key if an error occurs which does not conform
  // to any existing category.  No response to this message is expected.
  // Payload length: variable.  If > 0 bytes, the bytes represent an error
  // string which is ascii encoded and does not include a null terminator.
  UNKNOWN_ERROR = 255,
};

const uint8_t kConnectResponseNoSession = 0;
const uint8_t kConnectResponseActiveSession = 1;

class SecurityKeyMessage final {
 public:
  // The number of bytes used to represent the header.
  static const int kHeaderSizeBytes;

  // The number of bytes used to represent the message type.
  static const int kMessageTypeSizeBytes;

  SecurityKeyMessage();

  SecurityKeyMessage(const SecurityKeyMessage&) = delete;
  SecurityKeyMessage& operator=(const SecurityKeyMessage&) = delete;

  ~SecurityKeyMessage();

  // When given a header value (uint32_t), this method will return whether the
  // length is within the allowable size range.
  static bool IsValidMessageSize(uint32_t message_size);

  // Returns a SecurityKeyMessageType enum value corresponding to the
  // value passed in if it is valid, otherwise INVALID is returned.
  static SecurityKeyMessageType MessageTypeFromValue(int value);

  // Creates a message from the passed in values, no validation is done as this
  // method is only expected to be called from test code.
  static std::unique_ptr<SecurityKeyMessage> CreateMessageForTest(
      SecurityKeyMessageType type,
      const std::string& payload);

  // Parses |message_data| and initializes the internal members.  Returns true
  // if |message_data| was parsed and the instance was initialized successfully.
  bool ParseMessage(const std::string& message_data);

  SecurityKeyMessageType type() { return type_; }

  const std::string& payload() { return payload_; }

 private:
  SecurityKeyMessageType type_ = SecurityKeyMessageType::INVALID;
  std::string payload_;
};

// Used to pass security key message data between classes.
using SecurityKeyMessageCallback =
    base::RepeatingCallback<void(std::unique_ptr<SecurityKeyMessage> message)>;

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_H_
