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

// WebRTC ICE candidate details.
// https://www.w3.org/TR/webrtc/#rtcicecandidate-interface
struct IceCandidate {
  IceCandidate();
  IceCandidate(const IceCandidate&);
  IceCandidate(IceCandidate&&);
  IceCandidate& operator=(const IceCandidate&);
  IceCandidate& operator=(IceCandidate&&);
  ~IceCandidate();

  // The ICE candidate string, containing foundation, component, priority,
  // address, port, type, etc.
  std::string candidate;

  // If present, identifies the media stream ("mid") associated with the
  // candidate.
  std::string sdp_mid;

  // If present, indicates the zero-based index of the m-line in the SDP
  // associated with the candidate.
  std::optional<int> sdp_m_line_index;
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

  // Base64-encoded HMAC of the SDP description, for validation.
  std::vector<uint8_t> signature;
};

// The authentication payload used in session-initiate, session-accept, and
// session-info messages.
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

  // Base64-encoded SPAKE message.
  std::vector<uint8_t> spake_message;

  // Base64-encoded verification hash.
  std::vector<uint8_t> verification_hash;

  // SessionAuthz host token.
  std::vector<uint8_t> session_authz_host_token;

  // SessionAuthz session token.
  std::vector<uint8_t> session_authz_session_token;
};

struct JingleMessage {
  enum ActionType {
    UNKNOWN_ACTION,
    SESSION_INITIATE,
    SESSION_ACCEPT,
    SESSION_TERMINATE,
    SESSION_INFO,
    TRANSPORT_INFO,
  };

  enum Reason {
    UNKNOWN_REASON,
    SUCCESS,
    DECLINE,
    CANCEL,
    EXPIRED,
    GENERAL_ERROR,
    FAILED_APPLICATION,
    INCOMPATIBLE_PARAMETERS,
  };

  JingleMessage();
  JingleMessage(const SignalingAddress& to,
                ActionType action_value,
                const std::string& sid_value);
  ~JingleMessage();

  // Caller keeps ownership of |stanza|.
  static bool IsJingleMessage(const jingle_xmpp::XmlElement* stanza);
  static std::string GetActionName(ActionType action);

  // Caller keeps ownership of |stanza|. |error| is set to debug error
  // message when parsing fails.
  bool ParseXml(const jingle_xmpp::XmlElement* stanza, std::string* error);

  // Adds an XmlElement into |attachments|. This function implicitly creates
  // |attachments| if it's empty, and |attachment| should not be an empty
  // unique_ptr.
  void AddAttachment(std::unique_ptr<jingle_xmpp::XmlElement> attachment);

  std::unique_ptr<jingle_xmpp::XmlElement> ToXml() const;

  SignalingAddress from;
  SignalingAddress to;
  ActionType action = UNKNOWN_ACTION;
  std::string sid;

  std::string initiator;

  std::unique_ptr<ContentDescription> description;

  std::unique_ptr<jingle_xmpp::XmlElement> transport_info;

  // Content of session-info messages.
  std::unique_ptr<jingle_xmpp::XmlElement> info;

  // Content of plugin message. The node is read or written by all plugins, and
  // ActionType independent.
  std::unique_ptr<jingle_xmpp::XmlElement> attachments;

  // Value from the <reason> tag if it is present in the
  // message. Useful mainly for session-terminate messages, but Jingle
  // spec allows it in any message.
  Reason reason = UNKNOWN_REASON;

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

struct IceTransportInfo {
  IceTransportInfo();
  ~IceTransportInfo();
  struct NamedCandidate {
    NamedCandidate();
    NamedCandidate(const std::string& name, const webrtc::Candidate& candidate);

    std::string name;
    webrtc::Candidate candidate;
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

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
