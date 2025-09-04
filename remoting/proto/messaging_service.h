// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_MESSAGING_SERVICE_H_
#define REMOTING_PROTO_MESSAGING_SERVICE_H_

#include <optional>
#include <string>
#include <variant>

#include "base/time/time.h"

// This file defines structs for the MessagingService. For official builds,
// these structs are populated by code in //remoting/internal. For unofficial
// builds, they are populated by code in internal_stubs.h.
namespace remoting::internal {

// Message sent from the server when a channel is opened.
struct ChannelOpenStruct {
  // Represents the approximate lifetime of the channel.
  std::optional<base::TimeDelta> channel_lifetime;

  // The amount of time to wait for a channel active message before the client
  // should recreate the messaging channel.
  std::optional<base::TimeDelta> inactivity_timeout;
};

// Message sent from the server to indicate that the channel is active.
struct ChannelActiveStruct {};

// Used to identity a specific messaging endpoint.
struct EndpointIdStruct {
  // The username of the endpoint.
  std::string username;
};

// Used to send a `payload` between two messaging endpoints.
struct SimpleMessageStruct {
  SimpleMessageStruct();
  SimpleMessageStruct(const SimpleMessageStruct&);
  SimpleMessageStruct& operator=(const SimpleMessageStruct&);
  ~SimpleMessageStruct();

  // A sender-side generated id for this payload.
  std::string message_id;

  // The content to be sent to the other endpoint.
  std::string payload;

  // A sender-side timestamp for when the message was created.
  base::Time create_time;

  // A server-side timestamp for when the service receives the message.
  base::Time receive_time;

  // A server-side timestamp for when the service sent the message to the
  // destination.
  base::Time deliver_time;

  // A server initialized field to indicate the entity which sent the message.
  EndpointIdStruct sender_id;

  // A server initialized field to indicate the destination id used for routing.
  EndpointIdStruct destination_id;
};

// Request sent to `SendHostMessage`.
struct SendHostMessageRequestStruct {
  // The endpoint to send the message to.
  EndpointIdStruct destination_id;

  // The message to send.
  SimpleMessageStruct simple_message;
};

// Response received from the server after calling `SendHostMessage`.
struct SendHostMessageResponseStruct {};

// Request sent to `ReceiveClientMessages`.
struct ReceiveClientMessagesRequestStruct {};

// Response received from the server after calling `ReceiveClientMessages`. Note
// that because this is a streaming RPC, the host should expect to receive one
// or more of these messages during the lifetime of the channel.
struct ReceiveClientMessagesResponseStruct {
  ReceiveClientMessagesResponseStruct();
  ~ReceiveClientMessagesResponseStruct();

  // Each streaming response will contain one of the following messages.
  std::variant<ChannelOpenStruct, ChannelActiveStruct, SimpleMessageStruct>
      message;
};

}  // namespace remoting::internal

#endif  // REMOTING_PROTO_MESSAGING_SERVICE_H_
