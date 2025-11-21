// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_MESSAGING_SERVICE_H_
#define REMOTING_PROTO_MESSAGING_SERVICE_H_

#include <optional>
#include <string>
#include <variant>

#include "base/time/time.h"
#include "remoting/proto/service_common.h"

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

struct IqStanzaStruct {
  IqStanzaStruct();
  IqStanzaStruct(const IqStanzaStruct&);
  IqStanzaStruct& operator=(const IqStanzaStruct&);
  ~IqStanzaStruct();
};

struct PingPongStruct {
  enum class Type {
    TYPE_UNSPECIFIED = 0,
    PING = 1,
    PONG = 2,
  };
  Type type;
  std::string rally_id;
  int32_t current_count;
  int32_t exchange_count;
  std::string payload;
};

struct BurstStruct {
  int32_t index;
  int32_t burst_count;
  std::string payload;
};

struct EncryptedStruct {
  std::string payload;
  std::string unencrypted_payload;
};

struct SimpleStruct {
  std::string payload;
};

struct ShareSessionTokenStruct {
  std::string messaging_authz_token;
};

struct SystemTestStruct {
  SystemTestStruct();
  SystemTestStruct(const SystemTestStruct&);
  SystemTestStruct& operator=(const SystemTestStruct&);
  ~SystemTestStruct();

  std::variant<BurstStruct,
               EncryptedStruct,
               PingPongStruct,
               SimpleStruct,
               ShareSessionTokenStruct>
      test_message;
};

// Used to send a `payload` between two messaging endpoints.
struct PeerMessageStruct {
  PeerMessageStruct();
  PeerMessageStruct(const PeerMessageStruct&);
  PeerMessageStruct& operator=(const PeerMessageStruct&);
  ~PeerMessageStruct();

  // A sender-side generated id for this payload.
  std::string message_id;

  // The content to be sent to the other endpoint.
  std::variant<IqStanzaStruct, SystemTestStruct> payload;
};

// Request sent to `SendHostMessage`.
struct HostSendMessageRequestStruct {
  // Provides proof that the host is authorized to respond to client messages.
  std::string messaging_authz_token;

  // The message to send.
  PeerMessageStruct peer_message;
};

struct MachineInfo {
  // The compiled version of the host agent or playground binary.
  std::string version;

  // Details about the operating system the binary is running on.
  OperatingSystemInfoStruct operating_system_info;
};

// Response received from the server after calling `SendHostMessage`.
struct HostSendMessageResponseStruct {};

// Request sent to `ReceiveClientMessages`.
struct HostOpenChannelRequestStruct {
  std::string username;
  std::string host_public_key;
  MachineInfo machine_info;
};

// Response received from the server after calling `ReceiveClientMessages`. Note
// that because this is a streaming RPC, the host should expect to receive one
// or more of these messages during the lifetime of the channel.
struct HostOpenChannelResponseStruct {
  HostOpenChannelResponseStruct();
  ~HostOpenChannelResponseStruct();

  // Each streaming response will contain one of the following messages.
  std::variant<ChannelOpenStruct, ChannelActiveStruct, PeerMessageStruct>
      message;
};

}  // namespace remoting::internal

#endif  // REMOTING_PROTO_MESSAGING_SERVICE_H_
