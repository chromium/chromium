// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
#define REMOTING_PROTOCOL_JINGLE_MESSAGES_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "remoting/base/authentication_method.h"
#include "remoting/protocol/errors.h"
#include "remoting/signaling/signaling_address.h"
#include "third_party/webrtc/api/candidate.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting::protocol {

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

// WebRTC session description (SDP).
// https://www.w3.org/TR/webrtc/#rtcsessiondescription-class
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

// The authentication payload used in session-initiate, session-accept, and
// session-info messages.
// TODO: joedow - Consider using sub-messages to limit the fields for each
// authenticator type.
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

struct IceTransportInfo {
  IceTransportInfo();
  ~IceTransportInfo();
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

  // Caller keeps ownership of |stanza|. |error| is set to debug error
  // message when parsing fails.
  bool ParseXml(const jingle_xmpp::XmlElement* stanza);
  std::unique_ptr<jingle_xmpp::XmlElement> ToXml() const;

  std::list<IceCredentials> ice_credentials;
  std::list<NamedCandidate> candidates;
};

struct JingleTransportInfo {
  JingleTransportInfo();
  JingleTransportInfo(const JingleTransportInfo&);
  JingleTransportInfo(JingleTransportInfo&&);
  JingleTransportInfo& operator=(const JingleTransportInfo&);
  JingleTransportInfo& operator=(JingleTransportInfo&&);
  ~JingleTransportInfo();

  // TODO: joedow - Remove this field when we no longer support chromotocol.
  std::string xml_namespace;

  std::vector<IceTransportInfo::IceCredentials> ice_credentials;
  std::vector<IceTransportInfo::NamedCandidate> candidates;

  std::optional<SessionDescription> session_description;
};

struct HostAttributesAttachment {
  HostAttributesAttachment();
  HostAttributesAttachment(const HostAttributesAttachment&);
  HostAttributesAttachment(HostAttributesAttachment&&);
  HostAttributesAttachment& operator=(const HostAttributesAttachment&);
  HostAttributesAttachment& operator=(HostAttributesAttachment&&);
  ~HostAttributesAttachment();

  std::vector<std::string> attribute;
};

struct HostConfigAttachment {
  HostConfigAttachment();
  HostConfigAttachment(const HostConfigAttachment&);
  HostConfigAttachment(HostConfigAttachment&&);
  HostConfigAttachment& operator=(const HostConfigAttachment&);
  HostConfigAttachment& operator=(HostConfigAttachment&&);
  ~HostConfigAttachment();

  std::map<std::string, std::string> settings;
};

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

struct SessionInfo {
  SessionInfo();
  SessionInfo(const SessionInfo&);
  SessionInfo(SessionInfo&&);
  SessionInfo& operator=(const SessionInfo&);
  SessionInfo& operator=(SessionInfo&&);
  ~SessionInfo();

  std::optional<JingleAuthentication> authentication;
};

struct SessionTerminate {
  SessionTerminate();
  SessionTerminate(const SessionTerminate&);
  SessionTerminate(SessionTerminate&&);
  SessionTerminate& operator=(const SessionTerminate&);
  SessionTerminate& operator=(SessionTerminate&&);
  ~SessionTerminate();

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

class JingleMessage {
 public:
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
  JingleMessage(const SignalingAddress& to,
                Payload payload_value,
                const std::string& sid_value);
  ~JingleMessage();

  // Caller keeps ownership of |stanza|.
  static bool IsJingleMessage(const jingle_xmpp::XmlElement* stanza);
  static std::string GetActionName(ActionType action);

  // Caller keeps ownership of |stanza|. |error| is set to debug error
  // message when parsing fails.
  bool ParseXml(const jingle_xmpp::XmlElement* stanza, std::string* error);

  std::unique_ptr<jingle_xmpp::XmlElement> ToXml() const;

  ActionType action() const { return action_; }
  const Payload& payload() const { return payload_; }

  void SetPayload(Payload payload);

  static ActionType ActionFromPayload(const Payload& payload);

  SignalingAddress from;
  SignalingAddress to;
  std::string sid;

  std::string initiator;

  std::unique_ptr<ContentDescription> description;
  std::vector<Attachment> attachments;

  // Legacy XML-based payloads, maintained for backward compatibility.
  std::unique_ptr<jingle_xmpp::XmlElement> info_legacy;

  // Value from the <reason> tag if it is present in the
  // message. Useful mainly for session-terminate messages, but Jingle
  // spec allows it in any message.
  SessionTerminate::Reason reason = SessionTerminate::Reason::kUnspecified;

  // Value from the <google:remoting:error-code> tag if it is present in the
  // message. Useful mainly for session-terminate messages. If it's UNKNOWN,
  // or reason is UNKNOWN_REASON, this field will be ignored in the xml output.
  ErrorCode error_code = ErrorCode::UNKNOWN_ERROR;

  // Value from the <google:remoting:error-details> tag if it is present in the
  // message. Useful mainly for session-terminate messages. If it's empty, or
  // reason is UNKNOWN_REASON, this field will be ignored in the xml output.
  std::string error_details;

  // Value from the <google:remoting:error-location> tag if it is present in the
  // message. Useful mainly for session-terminate messages. If it's empty, or
  // reason is UNKNOWN_REASON, this field will be ignored in the xml output.
  std::string error_location;

 private:
  friend bool JingleMessageFromXml(const jingle_xmpp::XmlElement* stanza,
                                   JingleMessage* message,
                                   std::string* error);

  ActionType action_ = ActionType::kUnknownAction;
  Payload payload_;
};

struct JingleMessageReply {
  enum ReplyType {
    REPLY_RESULT,
    REPLY_ERROR,
  };
  enum ErrorType {
    NONE,
    BAD_REQUEST,
    NOT_IMPLEMENTED,
    INVALID_SID,
    UNEXPECTED_REQUEST,
    UNSUPPORTED_INFO,
  };

  JingleMessageReply();
  JingleMessageReply(ErrorType error);
  JingleMessageReply(ErrorType error, const std::string& text);
  ~JingleMessageReply();

  // Formats reply stanza for the specified |request_stanza|. Id and
  // recepient as well as other information needed to generate a valid
  // reply are taken from |request_stanza|.
  std::unique_ptr<jingle_xmpp::XmlElement> ToXml(
      const jingle_xmpp::XmlElement* request_stanza) const;

  ReplyType type;
  ErrorType error_type;
  std::string text;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
