// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_MESSAGES_H_
#define REMOTING_PROTOCOL_JINGLE_MESSAGES_H_

#include <list>
#include <memory>
#include <string>

#include "remoting/protocol/errors.h"
#include "remoting/signaling/signaling_address.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/webrtc/api/candidate.h"

namespace remoting::protocol {

class ContentDescription;

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
    NamedCandidate() = default;
    NamedCandidate(const std::string& name,
                   const cricket::Candidate& candidate);

    std::string name;
    cricket::Candidate candidate;
  };

  struct IceCredentials {
    IceCredentials() = default;
    IceCredentials(std::string channel,
                   std::string ufrag,
                   std::string password);

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
