// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_MESSAGING_SERVICE_H_
#define REMOTING_PROTO_MESSAGING_SERVICE_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "remoting/base/authentication_method.h"
#include "remoting/proto/service_common.h"

// This file defines structs for the MessagingService. For official builds,
// these structs are populated by code in //remoting/internal. For unofficial
// builds, they are populated by code in internal_stubs.h.
namespace remoting::internal {

// Contains the address which the endpoint should use for signaling.
struct SignalingAddress {
  // The `jid`, a.k.a. Jabber ID, to use for this endpoint in XMPP messages.
  std::string jid;
};

// Defines the set of fields needed to form a JabberId.
// See https://datatracker.ietf.org/doc/html/rfc7622
struct JabberIdStruct {
  // Represents a user or machine (e.g., a username or UUID).
  std::string local_part;

  // Represents the domain of the service which is handling the message.
  std::string domain_part;

  // Represents a specific user or connection.
  std::string resource_part;
};

// Represents a WebRTC session description (SDP).
struct SessionDescriptionStruct {
  enum class SdpType {
    kUnspecified = 0,
    kOffer = 1,
    kAnswer = 2,
  };
  // The type of the session description.
  SdpType type = SdpType::kUnspecified;
  // The SDP content.
  std::string sdp;
  // A signature used to verify the SDP content.
  std::string signature;
};

// WebRTC ICE candidate details.
struct IceCandidateStruct {
  // The ICE candidate string.
  std::string candidate;
  // The identifier of the media stream.
  std::string sdp_mid;
  // The index of the media line in the SDP.
  int32_t sdp_m_line_index;
};

// Represents transport information for a Jingle session.
struct TransportInfoStruct {
  TransportInfoStruct();
  TransportInfoStruct(const TransportInfoStruct&);
  TransportInfoStruct& operator=(const TransportInfoStruct&);
  ~TransportInfoStruct();

  // A list of ICE candidates for the session.
  std::vector<IceCandidateStruct> candidates;
  // An optional session description.
  std::optional<SessionDescriptionStruct> session_description;
};

// Represents host attributes sent as an attachment.
struct HostAttributesAttachmentStruct {
  HostAttributesAttachmentStruct();
  HostAttributesAttachmentStruct(const HostAttributesAttachmentStruct&);
  HostAttributesAttachmentStruct& operator=(
      const HostAttributesAttachmentStruct&);
  ~HostAttributesAttachmentStruct();

  // A list of attributes describing the host.
  std::vector<std::string> attribute;
};

// Represents host configuration settings sent as an attachment.
struct HostConfigAttachmentStruct {
  HostConfigAttachmentStruct();
  HostConfigAttachmentStruct(const HostConfigAttachmentStruct&);
  HostConfigAttachmentStruct& operator=(const HostConfigAttachmentStruct&);
  ~HostConfigAttachmentStruct();

  // A map of configuration settings for the host.
  base::flat_map<std::string, std::string> settings;
};

// Represents a generic attachment that can contain host attributes or
// configuration.
struct AttachmentStruct {
  AttachmentStruct();
  AttachmentStruct(const AttachmentStruct&);
  AttachmentStruct& operator=(const AttachmentStruct&);
  ~AttachmentStruct();

  // Attributes for the host.
  std::optional<HostAttributesAttachmentStruct> host_attributes;
  // Configuration for the host.
  std::optional<HostConfigAttachmentStruct> host_config;
};

// Represents the authentication payload used in session-initiate,
// session-accept, and session-info messages.
struct AuthenticationStruct {
  AuthenticationStruct();
  AuthenticationStruct(const AuthenticationStruct&);
  AuthenticationStruct& operator=(const AuthenticationStruct&);
  ~AuthenticationStruct();

  // The set of authentication methods supported by the endpoint.
  std::vector<remoting::AuthenticationMethod> supported_methods;
  // The current authentication method being used.
  std::optional<remoting::AuthenticationMethod> method;
  // The SPAKE message for authentication.
  std::string spake_message;
  // The verification hash for authentication.
  std::string verification_hash;
  // The host token provided by the SessionAuthz service.
  std::string session_authz_host_token;
  // The session token provided by the SessionAuthz service.
  std::string session_authz_session_token;
};

// Jingle action "session-initiate".
struct SessionInitiateStruct {
  SessionInitiateStruct();
  SessionInitiateStruct(const SessionInitiateStruct&);
  SessionInitiateStruct& operator=(const SessionInitiateStruct&);
  ~SessionInitiateStruct();

  // The identity of the session initiator.
  JabberIdStruct initiator;
  // An optional authentication payload.
  std::optional<AuthenticationStruct> authentication;
};

// Jingle action "session-accept".
struct SessionAcceptStruct {
  SessionAcceptStruct();
  SessionAcceptStruct(const SessionAcceptStruct&);
  SessionAcceptStruct& operator=(const SessionAcceptStruct&);
  ~SessionAcceptStruct();

  // An optional authentication payload.
  std::optional<AuthenticationStruct> authentication;
};

// Jingle action "session-info".
struct SessionInfoStruct {
  SessionInfoStruct();
  SessionInfoStruct(const SessionInfoStruct&);
  SessionInfoStruct& operator=(const SessionInfoStruct&);
  ~SessionInfoStruct();

  // An optional authentication payload.
  std::optional<AuthenticationStruct> authentication;
};

// Jingle action "session-terminate".
struct SessionTerminateStruct {
  SessionTerminateStruct();
  SessionTerminateStruct(const SessionTerminateStruct&);
  SessionTerminateStruct& operator=(const SessionTerminateStruct&);
  ~SessionTerminateStruct();

  enum class Reason {
    kUnspecified = 0,
    kSuccess = 1,
    kDecline = 2,
    kCancel = 3,
    kExpired = 4,
    kGeneralError = 5,
    kFailedApplication = 6,
    kIncompatibleParameters = 7,
    kUnknownReason = 8,
  };
  // The reason for terminating the session.
  Reason reason = Reason::kUnspecified;
  // A machine-readable error code.
  std::string error_code;
  // Human-readable details about the error.
  std::string error_details;
  // The location in the code where the error occurred.
  std::string error_location;
};

// Represents a Jingle message payload for session negotiation.
struct JingleMessageStruct {
  JingleMessageStruct();
  JingleMessageStruct(const JingleMessageStruct&);
  JingleMessageStruct& operator=(const JingleMessageStruct&);
  ~JingleMessageStruct();

  // A unique identifier for the session.
  std::string session_id;
  // The Jingle action being performed.
  std::variant<std::monostate,
               SessionInitiateStruct,
               SessionAcceptStruct,
               SessionInfoStruct,
               TransportInfoStruct,
               SessionTerminateStruct>
      action;
  // A set of attachments associated with the message.
  std::vector<AttachmentStruct> attachments;
};

// A successful response to a SET stanza.
struct JingleReplyStruct {};

// Represents an IQ-level error.
struct ErrorStanzaStruct {
  enum class Condition {
    kUnspecified = 0,
    kBadRequest = 1,
    kNotImplemented = 2,
    kInvalidSid = 3,
    kUnexpectedRequest = 4,
    kUnsupportedInfo = 5,
  };
  // The condition or reason for the error.
  Condition condition = Condition::kUnspecified;
  // Optional descriptive text about the error.
  std::string text;
};

// Message sent from the server when a channel is opened.
struct ChannelOpenStruct {
  // Represents the approximate lifetime of the channel.
  std::optional<base::TimeDelta> channel_lifetime;

  // The amount of time to wait for a channel active message before the client
  // should recreate the messaging channel.
  std::optional<base::TimeDelta> inactivity_timeout;

  // Provides the address which the endpoint should use for signaling.
  SignalingAddress signaling_address;
};

// Message sent from the server to indicate that the channel is active.
struct ChannelActiveStruct {};

// Represents an XMPP IQ stanza.
struct IqStanzaStruct {
  IqStanzaStruct();
  IqStanzaStruct(const IqStanzaStruct&);
  IqStanzaStruct& operator=(const IqStanzaStruct&);
  ~IqStanzaStruct();

  // A unique identifier for the stanza.
  std::string id;
  // The sender of the stanza.
  JabberIdStruct sender;
  // The recipient of the stanza.
  JabberIdStruct receiver;

  // The payload content of the stanza.
  std::variant<std::monostate,
               JingleMessageStruct,
               JingleReplyStruct,
               ErrorStanzaStruct>
      payload;

  // Provides proof that the host is authorized to respond to client messages.
  std::string messaging_authz_token;
  // The IqStanza XML in serialized form.
  std::string xml;
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

struct OidcStruct {
  std::string redirect_uri;
  std::string code;
  std::string state;
};

struct SystemTestStruct {
  SystemTestStruct();
  SystemTestStruct(const SystemTestStruct&);
  SystemTestStruct& operator=(const SystemTestStruct&);
  ~SystemTestStruct();

  std::variant<BurstStruct,
               EncryptedStruct,
               OidcStruct,
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
