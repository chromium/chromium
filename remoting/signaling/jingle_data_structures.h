// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_JINGLE_DATA_STRUCTURES_H_
#define REMOTING_SIGNALING_JINGLE_DATA_STRUCTURES_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "remoting/base/authentication_method.h"
#include "remoting/base/errors.h"
#include "remoting/signaling/signaling_address.h"
#include "third_party/webrtc/api/candidate.h"

namespace remoting {

class ContentDescription;

// Defines the set of fields needed to form a JabberId.
// See https://datatracker.ietf.org/doc/html/rfc7622
struct JabberId {
  JabberId();
  JabberId(const JabberId&);
  JabberId(JabberId&&);
  JabberId& operator=(const JabberId&);
  JabberId& operator=(JabberId&&);
  ~JabberId();

  // Represents a user or machine (e.g., a username or UUID). Although this is
  // optional in the spec, it is required for routing in our use case.
  std::string local_part;

  // Represents the domain of the service which is handling the message.
  // For example: corp.google.com is a Google Corp server.
  std::string domain_part;

  // Represents a specific user or connection. For FTL this is the registration
  // id which is associated with a specific machine or browser tab on the client
  // machine.
  std::string resource_part;
};

// Represents a WebRTC session description (SDP).
// See https://www.w3.org/TR/webrtc/#rtcsessiondescription-class
struct SessionDescription {
  SessionDescription();
  SessionDescription(const SessionDescription&);
  SessionDescription(SessionDescription&&);
  SessionDescription& operator=(const SessionDescription&);
  SessionDescription& operator=(SessionDescription&&);
  ~SessionDescription();

  // WebRTC SDP Type as defined in https://www.w3.org/TR/webrtc/#rtcsdptype.
  enum class Type {
    kUnspecified = 0,
    kOffer = 1,
    kAnswer = 2,
  };
  Type type = Type::kUnspecified;

  // A serialized string representation of the sdp.
  std::string sdp;

  // Raw HMAC bytes of the SDP description, for validation.
  std::vector<uint8_t> signature;
};

// Represents the authentication payload used in session-initiate,
// session-accept, and session-info messages.
struct JingleAuthentication {
  JingleAuthentication();
  JingleAuthentication(const JingleAuthentication&);
  JingleAuthentication(JingleAuthentication&&);
  JingleAuthentication& operator=(const JingleAuthentication&);
  JingleAuthentication& operator=(JingleAuthentication&&);
  ~JingleAuthentication();

  // The supported authentication methods.
  std::vector<remoting::AuthenticationMethod> supported_methods;

  // The current auth method.
  std::optional<remoting::AuthenticationMethod> method;

  // Raw SPAKE message bytes.
  std::vector<uint8_t> spake_message;

  // Raw verification hash bytes.
  std::vector<uint8_t> verification_hash;

  // Raw certificate bytes.
  std::vector<uint8_t> certificate;

  // Pairing information.
  struct PairingInfo {
    std::string client_id;
  };
  std::optional<PairingInfo> pairing_info;

  // Generic ID attribute, used by some authenticators (e.g. FakeAuthenticator).
  std::string id;

  // SessionAuthz host token.
  std::string session_authz_host_token;

  // SessionAuthz session token.
  std::string session_authz_session_token;

  // Pairing error message, if any.
  std::string pairing_error;

  // Fake values for testing.
  std::string test_id;
  std::vector<uint8_t> test_key;

  bool is_empty() const;
};

// Represents the ICE transport information including candidates and credentials
struct IceTransportInfo {
  IceTransportInfo();
  ~IceTransportInfo();

  // Represents an ICE candidate with an optional name and SDP m-line index.
  // TODO: joedow - Replace this with webrtc::IceCandidate post-chromotocol.
  struct NamedCandidate {
    NamedCandidate();
    NamedCandidate(const std::string& name,
                   const webrtc::Candidate& candidate,
                   std::optional<int> sdp_m_line_index = std::nullopt);
    NamedCandidate(const NamedCandidate&);
    NamedCandidate(NamedCandidate&&);
    NamedCandidate& operator=(const NamedCandidate&);
    NamedCandidate& operator=(NamedCandidate&&);
    ~NamedCandidate();

    std::string name;
    webrtc::Candidate candidate;
    std::optional<int> sdp_m_line_index;
  };

  // Represents ICE credentials for a specific channel.
  struct IceCredentials {
    IceCredentials();
    IceCredentials(std::string channel,
                   std::string ufrag,
                   std::string password);
    ~IceCredentials();

    std::string channel;
    std::string ufrag;
    std::string password;
  };

  std::list<IceCredentials> ice_credentials;
  std::list<NamedCandidate> candidates;
};

// Represents transport information for a Jingle session.
struct JingleTransportInfo {
  JingleTransportInfo();
  JingleTransportInfo(const JingleTransportInfo&);
  JingleTransportInfo(JingleTransportInfo&&);
  JingleTransportInfo& operator=(const JingleTransportInfo&);
  JingleTransportInfo& operator=(JingleTransportInfo&&);
  ~JingleTransportInfo();

  // The XML namespace for this transport (e.g., google:remoting:ice or
  // google:remoting:webrtc).
  // TODO: joedow - Remove this field when we no longer support chromotocol.
  std::string xml_namespace;

  std::vector<IceTransportInfo::IceCredentials> ice_credentials;
  std::vector<IceTransportInfo::NamedCandidate> candidates;

  std::optional<SessionDescription> session_description;
};

// Represents host attributes sent as an attachment.
struct HostAttributesAttachment {
  HostAttributesAttachment();
  HostAttributesAttachment(const HostAttributesAttachment&);
  HostAttributesAttachment(HostAttributesAttachment&&);
  HostAttributesAttachment& operator=(const HostAttributesAttachment&);
  HostAttributesAttachment& operator=(HostAttributesAttachment&&);
  ~HostAttributesAttachment();

  std::vector<std::string> attribute;
};

// Represents host configuration settings sent as an attachment.
struct HostConfigAttachment {
  HostConfigAttachment();
  HostConfigAttachment(const HostConfigAttachment&);
  HostConfigAttachment(HostConfigAttachment&&);
  HostConfigAttachment& operator=(const HostConfigAttachment&);
  HostConfigAttachment& operator=(HostConfigAttachment&&);
  ~HostConfigAttachment();

  std::map<std::string, std::string> settings;
};

// Represents a generic attachment that can contain host attributes or
// configuration.
struct Attachment {
  Attachment();
  Attachment(const Attachment&);
  Attachment(Attachment&&);
  Attachment& operator=(const Attachment&);
  Attachment& operator=(Attachment&&);
  ~Attachment();

  std::optional<HostAttributesAttachment> host_attributes;
  std::optional<HostConfigAttachment> host_config;
};

// Represents the payload for a "session-initiate" Jingle action.
struct SessionInitiate {
  SessionInitiate();
  SessionInitiate(const SessionInitiate&);
  SessionInitiate(SessionInitiate&&);
  SessionInitiate& operator=(const SessionInitiate&);
  SessionInitiate& operator=(SessionInitiate&&);
  ~SessionInitiate();

  std::optional<JingleAuthentication> authentication;
  std::optional<JingleTransportInfo> transport_info;
};

// Represents the payload for a "session-accept" Jingle action.
struct SessionAccept {
  SessionAccept();
  SessionAccept(const SessionAccept&);
  SessionAccept(SessionAccept&&);
  SessionAccept& operator=(const SessionAccept&);
  SessionAccept& operator=(SessionAccept&&);
  ~SessionAccept();

  std::optional<JingleAuthentication> authentication;
  std::optional<JingleTransportInfo> transport_info;
};

// Represents the payload for a "session-info" Jingle action.
struct SessionInfo {
  SessionInfo();
  SessionInfo(const SessionInfo&);
  SessionInfo(SessionInfo&&);
  SessionInfo& operator=(const SessionInfo&);
  SessionInfo& operator=(SessionInfo&&);
  ~SessionInfo();

  std::optional<JingleAuthentication> authentication;

  // Generic info for session-info messages that are not authentication.
  struct GenericInfo {
    GenericInfo();
    GenericInfo(const GenericInfo&);
    GenericInfo(GenericInfo&&);
    GenericInfo& operator=(const GenericInfo&);
    GenericInfo& operator=(GenericInfo&&);
    ~GenericInfo();

    std::string name;
    std::string namespace_uri;
    std::string body;
  };
  std::optional<GenericInfo> generic_info;
};

// Represents the payload for a "session-terminate" Jingle action.
struct SessionTerminate {
  SessionTerminate();
  SessionTerminate(const SessionTerminate&);
  SessionTerminate(SessionTerminate&&);
  SessionTerminate& operator=(const SessionTerminate&);
  SessionTerminate& operator=(SessionTerminate&&);
  ~SessionTerminate();

  // The reason for termination. See XEP-0166, section 7.2.
  enum class Reason {
    kUnspecified,
    kSuccess,
    kDecline,
    kCancel,
    kExpired,
    kGeneralError,
    kFailedApplication,
    kIncompatibleParameters,
    kUnknownReason,
  };

  Reason reason = Reason::kUnspecified;
  std::string error_code;
  std::string error_details;
  std::string error_location;
};

// Represents a Jingle message payload for session negotiation.
class JingleMessage {
 public:
  // The specific Jingle action this message represents.
  enum class ActionType {
    kUnknownAction,
    kSessionInitiate,
    kSessionAccept,
    kSessionTerminate,
    kSessionInfo,
    kTransportInfo,
  };

  // Structured data replacements for XML payloads.
  using Payload = std::variant<std::monostate /*unset value*/,
                               SessionInitiate,
                               SessionAccept,
                               SessionInfo,
                               JingleTransportInfo,
                               SessionTerminate>;

  JingleMessage();
  JingleMessage(const JingleMessage&);
  JingleMessage& operator=(const JingleMessage&);
  JingleMessage(JingleMessage&&);
  JingleMessage& operator=(JingleMessage&&);
  JingleMessage(const SignalingAddress& to,
                Payload payload_value,
                const std::string& sid_value);
  ~JingleMessage();

  static std::string GetActionName(ActionType action);

  static ActionType ActionFromPayload(const Payload& payload);

  ActionType action() const { return action_; }
  const Payload& payload() const { return payload_; }

  void SetPayload(Payload payload);

  // Unique identifier for the message.
  std::string message_id;

  SignalingAddress from;
  SignalingAddress to;

  // The unique identifier for this Jingle session.
  std::string sid;

  // JID of the session initiator.
  std::string initiator;

  std::unique_ptr<ContentDescription> description;
  std::vector<Attachment> attachments;

  // Value from the <reason> tag if it is present in the message. Useful mainly
  // for session-terminate messages, but Jingle spec allows it in any message.
  SessionTerminate::Reason reason = SessionTerminate::Reason::kUnspecified;

  // CRD-specific error code, provides more detail than `reason`.
  ErrorCode error_code = ErrorCode::UNKNOWN_ERROR;

  // Optional human-readable details about the error.
  std::string error_details;

  // Optional string indicating the code location where the error occurred.
  std::string error_location;

 private:
  friend bool JingleMessageFromXml(const jingle_xmpp::XmlElement* stanza,
                                   JingleMessage* message,
                                   std::string* error);

  ActionType action_ = ActionType::kUnknownAction;
  Payload payload_;
};

// Represents a response to a Jingle message, which can be either a success
// (RESULT) or an error (ERROR).
struct JingleMessageReply {
  enum ReplyType {
    REPLY_RESULT,
    REPLY_ERROR,
  };

  // Condition for an IQ-level error, distinct from a session-terminate error.
  enum ErrorType {
    UNSPECIFIED,
    BAD_REQUEST,
    NOT_IMPLEMENTED,
    INVALID_SID,
    UNEXPECTED_REQUEST,
    UNSUPPORTED_INFO,
  };

  JingleMessageReply();
  JingleMessageReply(ErrorType error);
  JingleMessageReply(ErrorType error, const std::string& text);
  JingleMessageReply(const JingleMessageReply&);
  JingleMessageReply& operator=(const JingleMessageReply&);
  JingleMessageReply(JingleMessageReply&&);
  JingleMessageReply& operator=(JingleMessageReply&&);
  ~JingleMessageReply();

  // Defines the role of this reply in the IQ request/response pattern.
  ReplyType reply_type = REPLY_RESULT;

  // Error details, present if reply_type = REPLY_ERROR.
  std::optional<ErrorType> error_type;

  // Optional descriptive text for the error.
  std::string text;

  // Unique identifier for the message, used for pairing with the request.
  std::string message_id;

  SignalingAddress from;
  SignalingAddress to;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_JINGLE_DATA_STRUCTURES_H_
