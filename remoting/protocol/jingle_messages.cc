// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_messages.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/base/name_value_map.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_message_xml_converter.h"
#include "remoting/protocol/session_plugin.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting::protocol {

namespace {

const char kEmptyNamespace[] = "";
const char kJabberNamespace[] = "jabber:client";
const char kJingleNamespace[] = "urn:xmpp:jingle:1";
const char kXmlNamespace[] = "http://www.w3.org/XML/1998/namespace";

const NameMapElement<JingleMessage::ActionType> kActionTypes[] = {
    {JingleMessage::ActionType::kSessionInitiate, "session-initiate"},
    {JingleMessage::ActionType::kSessionAccept, "session-accept"},
    {JingleMessage::ActionType::kSessionTerminate, "session-terminate"},
    {JingleMessage::ActionType::kSessionInfo, "session-info"},
    {JingleMessage::ActionType::kTransportInfo, "transport-info"},
};

}  // namespace

// static
bool JingleMessage::IsJingleMessage(const jingle_xmpp::XmlElement* stanza) {
  return remoting::protocol::IsJingleMessage(stanza);
}

// static
std::string JingleMessage::GetActionName(ActionType action) {
  return ValueToName(kActionTypes, action);
}

// static
JingleMessage::ActionType JingleMessage::ActionFromPayload(
    const Payload& payload) {
  return std::visit(absl::Overload(
                        [](const std::monostate&) {
                          return JingleMessage::ActionType::kUnknownAction;
                        },
                        [](const SessionInitiate&) {
                          return JingleMessage::ActionType::kSessionInitiate;
                        },
                        [](const SessionAccept&) {
                          return JingleMessage::ActionType::kSessionAccept;
                        },
                        [](const SessionInfo&) {
                          return JingleMessage::ActionType::kSessionInfo;
                        },
                        [](const JingleTransportInfo&) {
                          return JingleMessage::ActionType::kTransportInfo;
                        },
                        [](const SessionTerminate&) {
                          return JingleMessage::ActionType::kSessionTerminate;
                        }),
                    payload);
}

JingleMessage::JingleMessage() = default;

JingleMessage::JingleMessage(const SignalingAddress& to,
                             Payload payload,
                             const std::string& sid)
    : to(to), sid(sid) {
  SetPayload(std::move(payload));
}

JingleMessage::~JingleMessage() = default;

void JingleMessage::SetPayload(Payload payload) {
  payload_ = std::move(payload);
  action_ = ActionFromPayload(payload_);
}

bool JingleMessage::ParseXml(const jingle_xmpp::XmlElement* stanza,
                             std::string* error) {
  return JingleMessageFromXml(stanza, this, error);
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessage::ToXml() const {
  return JingleMessageToXml(*this);
}

void JingleMessage::AddAttachment(std::unique_ptr<XmlElement> attachment) {
  DCHECK(attachment);
  if (!attachments_legacy) {
    attachments_legacy = std::make_unique<XmlElement>(
        QName(kChromotingXmlNamespace, "attachments"));
  }
  attachments_legacy->AddElement(attachment.release());
}

JingleMessageReply::JingleMessageReply()
    : type(REPLY_RESULT), error_type(NONE) {}

JingleMessageReply::JingleMessageReply(ErrorType error)
    : type(error != NONE ? REPLY_ERROR : REPLY_RESULT), error_type(error) {}

JingleMessageReply::JingleMessageReply(ErrorType error,
                                       const std::string& text_value)
    : type(REPLY_ERROR), error_type(error), text(text_value) {}

JingleMessageReply::~JingleMessageReply() = default;

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageReply::ToXml(
    const jingle_xmpp::XmlElement* request_stanza) const {
  std::unique_ptr<XmlElement> iq(
      new XmlElement(QName(kJabberNamespace, "iq"), true));

  iq->SetAttr(QName(kEmptyNamespace, "id"),
              request_stanza->Attr(QName(kEmptyNamespace, "id")));

  SignalingAddress original_from;
  original_from =
      SignalingAddress::Parse(request_stanza, SignalingAddress::FROM);
  DCHECK(!original_from.empty());

  if (type == REPLY_RESULT) {
    iq->SetAttr(QName(kEmptyNamespace, "type"), "result");
    XmlElement* jingle =
        new XmlElement(QName(kJingleNamespace, "jingle"), true);
    iq->AddElement(jingle);
    original_from.SetInMessage(iq.get(), SignalingAddress::TO);
    return iq;
  }

  DCHECK_EQ(type, REPLY_ERROR);

  iq->SetAttr(QName(kEmptyNamespace, "type"), "error");
  original_from.SetInMessage(iq.get(), SignalingAddress::TO);

  for (const jingle_xmpp::XmlElement* child = request_stanza->FirstElement();
       child != nullptr; child = child->NextElement()) {
    iq->AddElement(new jingle_xmpp::XmlElement(*child));
  }

  jingle_xmpp::XmlElement* error =
      new jingle_xmpp::XmlElement(QName(kJabberNamespace, "error"));
  iq->AddElement(error);

  std::string type_attr;
  std::string error_text;
  QName name;
  switch (error_type) {
    case BAD_REQUEST:
      type_attr = "modify";
      name = QName(kJabberNamespace, "bad-request");
      break;
    case NOT_IMPLEMENTED:
      type_attr = "cancel";
      name = QName(kJabberNamespace, "feature-bad-request");
      break;
    case INVALID_SID:
      type_attr = "modify";
      name = QName(kJabberNamespace, "item-not-found");
      error_text = "Invalid SID";
      break;
    case UNEXPECTED_REQUEST:
      type_attr = "modify";
      name = QName(kJabberNamespace, "unexpected-request");
      break;
    case UNSUPPORTED_INFO:
      type_attr = "modify";
      name = QName(kJabberNamespace, "feature-not-implemented");
      break;
    default:
      NOTREACHED();
  }

  if (!text.empty()) {
    error_text = text;
  }

  error->SetAttr(QName(kEmptyNamespace, "type"), type_attr);

  // If the error name is not in the standard namespace, we have
  // to first add some error from that namespace.
  if (name.Namespace() != kJabberNamespace) {
    error->AddElement(new jingle_xmpp::XmlElement(
        QName(kJabberNamespace, "undefined-condition")));
  }
  error->AddElement(new jingle_xmpp::XmlElement(name));

  if (!error_text.empty()) {
    // It's okay to always use English here. This text is for
    // debugging purposes only.
    jingle_xmpp::XmlElement* text_elem =
        new jingle_xmpp::XmlElement(QName(kJabberNamespace, "text"));
    text_elem->SetAttr(QName(kXmlNamespace, "lang"), "en");
    text_elem->SetBodyText(error_text);
    error->AddElement(text_elem);
  }

  return iq;
}

IceTransportInfo::IceTransportInfo() = default;
IceTransportInfo::~IceTransportInfo() = default;

bool IceTransportInfo::ParseXml(const jingle_xmpp::XmlElement* element) {
  return IceTransportInfoFromXml(element, this);
}

std::unique_ptr<jingle_xmpp::XmlElement> IceTransportInfo::ToXml() const {
  return IceTransportInfoToXml(*this);
}

JabberId::JabberId() = default;
JabberId::JabberId(const JabberId&) = default;
JabberId::JabberId(JabberId&&) = default;
JabberId& JabberId::operator=(const JabberId&) = default;
JabberId& JabberId::operator=(JabberId&&) = default;
JabberId::~JabberId() = default;

SessionDescription::SessionDescription() = default;
SessionDescription::SessionDescription(const SessionDescription&) = default;
SessionDescription::SessionDescription(SessionDescription&&) = default;
SessionDescription& SessionDescription::operator=(const SessionDescription&) =
    default;
SessionDescription& SessionDescription::operator=(SessionDescription&&) =
    default;
SessionDescription::~SessionDescription() = default;

JingleAuthentication::JingleAuthentication() = default;
JingleAuthentication::JingleAuthentication(const JingleAuthentication&) =
    default;
JingleAuthentication::JingleAuthentication(JingleAuthentication&&) = default;
JingleAuthentication& JingleAuthentication::operator=(
    const JingleAuthentication&) = default;
JingleAuthentication& JingleAuthentication::operator=(JingleAuthentication&&) =
    default;
JingleAuthentication::~JingleAuthentication() = default;

IceTransportInfo::NamedCandidate::NamedCandidate() = default;

IceTransportInfo::NamedCandidate::NamedCandidate(
    const std::string& name,
    const webrtc::Candidate& candidate,
    std::optional<int> sdp_m_line_index)
    : name(name), candidate(candidate), sdp_m_line_index(sdp_m_line_index) {}

IceTransportInfo::NamedCandidate::NamedCandidate(const NamedCandidate&) =
    default;
IceTransportInfo::NamedCandidate::NamedCandidate(NamedCandidate&&) = default;
IceTransportInfo::NamedCandidate& IceTransportInfo::NamedCandidate::operator=(
    const NamedCandidate&) = default;
IceTransportInfo::NamedCandidate& IceTransportInfo::NamedCandidate::operator=(
    NamedCandidate&&) = default;

IceTransportInfo::NamedCandidate::~NamedCandidate() = default;

IceTransportInfo::IceCredentials::IceCredentials() = default;

IceTransportInfo::IceCredentials::IceCredentials(std::string channel,
                                                 std::string ufrag,
                                                 std::string password)
    : channel(channel), ufrag(ufrag), password(password) {}

IceTransportInfo::IceCredentials::~IceCredentials() = default;

HostAttributesAttachment::HostAttributesAttachment() = default;
HostAttributesAttachment::HostAttributesAttachment(
    const HostAttributesAttachment&) = default;
HostAttributesAttachment::HostAttributesAttachment(HostAttributesAttachment&&) =
    default;
HostAttributesAttachment& HostAttributesAttachment::operator=(
    const HostAttributesAttachment&) = default;
HostAttributesAttachment& HostAttributesAttachment::operator=(
    HostAttributesAttachment&&) = default;
HostAttributesAttachment::~HostAttributesAttachment() = default;

HostConfigAttachment::HostConfigAttachment() = default;
HostConfigAttachment::HostConfigAttachment(const HostConfigAttachment&) =
    default;
HostConfigAttachment::HostConfigAttachment(HostConfigAttachment&&) = default;
HostConfigAttachment& HostConfigAttachment::operator=(
    const HostConfigAttachment&) = default;
HostConfigAttachment& HostConfigAttachment::operator=(HostConfigAttachment&&) =
    default;
HostConfigAttachment::~HostConfigAttachment() = default;

Attachment::Attachment() = default;
Attachment::Attachment(const Attachment&) = default;
Attachment::Attachment(Attachment&&) = default;
Attachment& Attachment::operator=(const Attachment&) = default;
Attachment& Attachment::operator=(Attachment&&) = default;
Attachment::~Attachment() = default;

SessionInitiate::SessionInitiate() = default;
SessionInitiate::SessionInitiate(const SessionInitiate&) = default;
SessionInitiate::SessionInitiate(SessionInitiate&&) = default;
SessionInitiate& SessionInitiate::operator=(const SessionInitiate&) = default;
SessionInitiate& SessionInitiate::operator=(SessionInitiate&&) = default;
SessionInitiate::~SessionInitiate() = default;

SessionAccept::SessionAccept() = default;
SessionAccept::SessionAccept(const SessionAccept&) = default;
SessionAccept::SessionAccept(SessionAccept&&) = default;
SessionAccept& SessionAccept::operator=(const SessionAccept&) = default;
SessionAccept& SessionAccept::operator=(SessionAccept&&) = default;
SessionAccept::~SessionAccept() = default;

SessionInfo::SessionInfo() = default;
SessionInfo::SessionInfo(const SessionInfo&) = default;
SessionInfo::SessionInfo(SessionInfo&&) = default;
SessionInfo& SessionInfo::operator=(const SessionInfo&) = default;
SessionInfo& SessionInfo::operator=(SessionInfo&&) = default;
SessionInfo::~SessionInfo() = default;

JingleTransportInfo::JingleTransportInfo() = default;
JingleTransportInfo::JingleTransportInfo(const JingleTransportInfo&) = default;
JingleTransportInfo::JingleTransportInfo(JingleTransportInfo&&) = default;
JingleTransportInfo& JingleTransportInfo::operator=(
    const JingleTransportInfo&) = default;
JingleTransportInfo& JingleTransportInfo::operator=(JingleTransportInfo&&) =
    default;
JingleTransportInfo::~JingleTransportInfo() = default;

SessionTerminate::SessionTerminate() = default;
SessionTerminate::SessionTerminate(const SessionTerminate&) = default;
SessionTerminate::SessionTerminate(SessionTerminate&&) = default;
SessionTerminate& SessionTerminate::operator=(const SessionTerminate&) =
    default;
SessionTerminate& SessionTerminate::operator=(SessionTerminate&&) = default;
SessionTerminate::~SessionTerminate() = default;

}  // namespace remoting::protocol
