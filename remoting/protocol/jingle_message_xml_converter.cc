// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_message_xml_converter.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/base/name_value_map.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_messages.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting::protocol {

namespace {

const char kJabberNamespace[] = "jabber:client";
const char kJingleNamespace[] = "urn:xmpp:jingle:1";

// Namespace for transport messages when using standard ICE.
const char kIceTransportNamespace[] = "google:remoting:ice";
// Namespace for transport messages when using WebRTC.
const char kWebrtcTransportNamespace[] = "google:remoting:webrtc";

const char kEmptyNamespace[] = "";

const int kPortMin = 1000;
const int kPortMax = 65535;

const NameMapElement<JingleMessage::ActionType> kActionTypes[] = {
    {JingleMessage::ActionType::kSessionInitiate, "session-initiate"},
    {JingleMessage::ActionType::kSessionAccept, "session-accept"},
    {JingleMessage::ActionType::kSessionTerminate, "session-terminate"},
    {JingleMessage::ActionType::kSessionInfo, "session-info"},
    {JingleMessage::ActionType::kTransportInfo, "transport-info"},
};

const NameMapElement<SessionTerminate::Reason> kReasons[] = {
    {SessionTerminate::Reason::kSuccess, "success"},
    {SessionTerminate::Reason::kDecline, "decline"},
    {SessionTerminate::Reason::kCancel, "cancel"},
    {SessionTerminate::Reason::kExpired, "expired"},
    {SessionTerminate::Reason::kGeneralError, "general-error"},
    {SessionTerminate::Reason::kFailedApplication, "failed-application"},
    {SessionTerminate::Reason::kIncompatibleParameters,
     "incompatible-parameters"},
};

// The type names "local" and "stun" are not standard but JingleMessage has
// used them from the start. So in order to remain backwards compatible,
// we check specifically for those types and override the candidate type name
// in those cases.
std::string_view GetLegacyTypeName(const webrtc::Candidate& c) {
  if (c.is_local()) {
    return "local";
  }
  if (c.is_stun()) {
    return "stun";
  }
  return c.type_name();
}

std::optional<webrtc::IceCandidateType> LegacyTypeNameToCandidateType(
    std::string_view type) {
  if (type == "local" || type == "host") {
    return webrtc::IceCandidateType::kHost;
  }
  if (type == "stun" || type == "srflx") {
    return webrtc::IceCandidateType::kSrflx;
  }
  if (type == "prflx") {
    return webrtc::IceCandidateType::kPrflx;
  }
  if (type == "relay") {
    return webrtc::IceCandidateType::kRelay;
  }
  return std::nullopt;
}

bool ParseIceCredentials(const jingle_xmpp::XmlElement* element,
                         IceTransportInfo::IceCredentials* credentials) {
  DCHECK(element->Name() == QName(kIceTransportNamespace, "credentials"));

  const std::string& channel = element->Attr(QName(kEmptyNamespace, "channel"));
  const std::string& ufrag = element->Attr(QName(kEmptyNamespace, "ufrag"));
  const std::string& password =
      element->Attr(QName(kEmptyNamespace, "password"));

  if (channel.empty() || ufrag.empty() || password.empty()) {
    return false;
  }

  credentials->channel = channel;
  credentials->ufrag = ufrag;
  credentials->password = password;

  return true;
}

bool ParseIceCandidate(const jingle_xmpp::XmlElement* element,
                       IceTransportInfo::NamedCandidate* candidate) {
  DCHECK(element->Name() == QName(kIceTransportNamespace, "candidate"));

  const std::string& name = element->Attr(QName(kEmptyNamespace, "name"));
  const std::string& foundation =
      element->Attr(QName(kEmptyNamespace, "foundation"));
  const std::string& address = element->Attr(QName(kEmptyNamespace, "address"));
  const std::string& port_str = element->Attr(QName(kEmptyNamespace, "port"));
  const std::optional<webrtc::IceCandidateType> type =
      LegacyTypeNameToCandidateType(
          element->Attr(QName(kEmptyNamespace, "type")));
  const std::string& protocol =
      element->Attr(QName(kEmptyNamespace, "protocol"));
  const std::string& priority_str =
      element->Attr(QName(kEmptyNamespace, "priority"));
  const std::string& generation_str =
      element->Attr(QName(kEmptyNamespace, "generation"));

  int port;
  unsigned priority;
  uint32_t generation;
  if (name.empty() || foundation.empty() || address.empty() ||
      !base::StringToInt(port_str, &port) || port < kPortMin ||
      port > kPortMax || !type || protocol.empty() ||
      !base::StringToUint(priority_str, &priority) ||
      !base::StringToUint(generation_str, &generation)) {
    return false;
  }

  candidate->name = name;
  candidate->candidate = {candidate->candidate.component(),
                          protocol,
                          webrtc::SocketAddress(address, port),
                          priority,
                          candidate->candidate.username(),
                          candidate->candidate.password(),
                          *type,
                          generation,
                          foundation};

  return true;
}

XmlElement* FormatIceCredentials(
    const IceTransportInfo::IceCredentials& credentials) {
  XmlElement* result =
      new XmlElement(QName(kIceTransportNamespace, "credentials"));
  result->SetAttr(QName(kEmptyNamespace, "channel"), credentials.channel);
  result->SetAttr(QName(kEmptyNamespace, "ufrag"), credentials.ufrag);
  result->SetAttr(QName(kEmptyNamespace, "password"), credentials.password);
  return result;
}

XmlElement* FormatIceCandidate(
    const IceTransportInfo::NamedCandidate& candidate) {
  XmlElement* result =
      new XmlElement(QName(kIceTransportNamespace, "candidate"));
  result->SetAttr(QName(kEmptyNamespace, "name"), candidate.name);
  result->SetAttr(QName(kEmptyNamespace, "foundation"),
                  candidate.candidate.foundation());
  result->SetAttr(QName(kEmptyNamespace, "address"),
                  candidate.candidate.address().ipaddr().ToString());
  result->SetAttr(QName(kEmptyNamespace, "port"),
                  base::NumberToString(candidate.candidate.address().port()));
  result->SetAttr(QName(kEmptyNamespace, "type"),
                  GetLegacyTypeName(candidate.candidate));
  result->SetAttr(QName(kEmptyNamespace, "protocol"),
                  candidate.candidate.protocol());
  result->SetAttr(QName(kEmptyNamespace, "priority"),
                  base::NumberToString(candidate.candidate.priority()));
  result->SetAttr(QName(kEmptyNamespace, "generation"),
                  base::NumberToString(candidate.candidate.generation()));
  return result;
}

}  // namespace

bool IsJingleMessage(const jingle_xmpp::XmlElement* stanza) {
  return stanza->Name() == QName(kJabberNamespace, "iq") &&
         stanza->Attr(QName(std::string(), "type")) == "set" &&
         stanza->FirstNamed(QName(kJingleNamespace, "jingle")) != nullptr;
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageToXml(
    const JingleMessage& message) {
  std::unique_ptr<XmlElement> root(
      new XmlElement(QName("jabber:client", "iq"), true));

  DCHECK(!message.to.empty());
  root->SetAttr(QName(kEmptyNamespace, "type"), "set");

  XmlElement* jingle_tag =
      new XmlElement(QName(kJingleNamespace, "jingle"), true);
  root->AddElement(jingle_tag);
  jingle_tag->AddAttr(QName(kEmptyNamespace, "sid"), message.sid);

  message.to.SetInMessage(root.get(), SignalingAddress::TO);
  if (!message.from.empty()) {
    message.from.SetInMessage(root.get(), SignalingAddress::FROM);
  }

  const char* action_attr = ValueToNameUnchecked(kActionTypes, message.action);
  if (!action_attr) {
    LOG(FATAL) << "Invalid action value " << static_cast<int>(message.action);
  }
  jingle_tag->AddAttr(QName(kEmptyNamespace, "action"), action_attr);

  if (message.attachments_legacy) {
    jingle_tag->AddElement(new XmlElement(*message.attachments_legacy));
  }

  if (message.action == JingleMessage::ActionType::kSessionInfo) {
    if (message.info_legacy) {
      jingle_tag->AddElement(new XmlElement(*message.info_legacy));
    }
    return root;
  }

  if (message.action == JingleMessage::ActionType::kSessionInitiate) {
    jingle_tag->AddAttr(QName(kEmptyNamespace, "initiator"), message.initiator);
  }

  if (message.reason != SessionTerminate::Reason::kUnspecified) {
    XmlElement* reason_tag = new XmlElement(QName(kJingleNamespace, "reason"));
    jingle_tag->AddElement(reason_tag);
    reason_tag->AddElement(new XmlElement(
        QName(kJingleNamespace, ValueToName(kReasons, message.reason))));

    if (message.error_code != ErrorCode::UNKNOWN_ERROR) {
      XmlElement* error_code_tag =
          new XmlElement(QName(kChromotingXmlNamespace, "error-code"));
      jingle_tag->AddElement(error_code_tag);
      error_code_tag->SetBodyText(ErrorCodeToString(message.error_code));
    }

    if (!message.error_details.empty()) {
      XmlElement* error_details_tag =
          new XmlElement(QName(kChromotingXmlNamespace, "error-details"));
      jingle_tag->AddElement(error_details_tag);
      error_details_tag->SetBodyText(message.error_details);
    }

    if (!message.error_location.empty()) {
      XmlElement* error_location_tag =
          new XmlElement(QName(kChromotingXmlNamespace, "error-location"));
      jingle_tag->AddElement(error_location_tag);
      error_location_tag->SetBodyText(message.error_location);
    }
  }

  if (message.action != JingleMessage::ActionType::kSessionTerminate) {
    XmlElement* content_tag =
        new XmlElement(QName(kJingleNamespace, "content"));
    jingle_tag->AddElement(content_tag);

    content_tag->AddAttr(QName(kEmptyNamespace, "name"),
                         ContentDescription::kChromotingContentName);
    content_tag->AddAttr(QName(kEmptyNamespace, "creator"), "initiator");

    if (message.description) {
      content_tag->AddElement(message.description->ToXml());
    }

    if (message.transport_info_legacy) {
      content_tag->AddElement(new XmlElement(*message.transport_info_legacy));
    } else if (message.description &&
               message.description->config()->webrtc_supported()) {
      content_tag->AddElement(
          new XmlElement(QName(kWebrtcTransportNamespace, "transport")));
    }
  }

  return root;
}

bool JingleMessageFromXml(const jingle_xmpp::XmlElement* stanza,
                          JingleMessage* message,
                          std::string* error) {
  if (!IsJingleMessage(stanza)) {
    *error = "Not a jingle message";
    return false;
  }

  const XmlElement* jingle_tag =
      stanza->FirstNamed(QName(kJingleNamespace, "jingle"));
  if (!jingle_tag) {
    *error = "Not a jingle message";
    return false;
  }

  message->from = SignalingAddress::Parse(stanza, SignalingAddress::FROM);
  message->to = SignalingAddress::Parse(stanza, SignalingAddress::TO);
  if (message->from.empty() || message->to.empty()) {
    *error = "Missing signaling address";
    return false;
  }

  message->initiator = jingle_tag->Attr(QName(kEmptyNamespace, "initiator"));

  std::string action_str = jingle_tag->Attr(QName(kEmptyNamespace, "action"));
  if (action_str.empty()) {
    *error = "action attribute is missing";
    return false;
  }
  if (!NameToValue(kActionTypes, action_str, &message->action)) {
    *error = "Unknown action " + action_str;
    return false;
  }

  message->sid = jingle_tag->Attr(QName(kEmptyNamespace, "sid"));
  if (message->sid.empty()) {
    *error = "sid attribute is missing";
    return false;
  }

  const XmlElement* attachments_tag =
      jingle_tag->FirstNamed(QName(kChromotingXmlNamespace, "attachments"));
  if (attachments_tag) {
    message->attachments_legacy =
        std::make_unique<XmlElement>(*attachments_tag);
  } else {
    message->attachments_legacy.reset();
  }

  if (message->action == JingleMessage::ActionType::kSessionInfo) {
    // session-info messages may contain arbitrary information not
    // defined by the Jingle protocol. We don't need to parse it.
    const XmlElement* child = jingle_tag->FirstElement();
    // Plugin messages are action independent, which should not be considered as
    // session-info.
    if (child == attachments_tag) {
      child = child->NextElement();
    }
    if (child) {
      // session-info is allowed to be empty.
      message->info_legacy = std::make_unique<XmlElement>(*child);
    } else {
      message->info_legacy.reset();
    }
    return true;
  }

  const XmlElement* reason_tag =
      jingle_tag->FirstNamed(QName(kJingleNamespace, "reason"));
  if (reason_tag && reason_tag->FirstElement()) {
    if (!NameToValue(kReasons, reason_tag->FirstElement()->Name().LocalPart(),
                     &message->reason)) {
      message->reason = SessionTerminate::Reason::kUnknownReason;
    }
  }

  const XmlElement* error_code_tag =
      jingle_tag->FirstNamed(QName(kChromotingXmlNamespace, "error-code"));
  if (error_code_tag && !error_code_tag->BodyText().empty()) {
    if (!ParseErrorCode(error_code_tag->BodyText(), &message->error_code)) {
      LOG(WARNING) << "Unknown error-code received "
                   << error_code_tag->BodyText();
      message->error_code = ErrorCode::UNKNOWN_ERROR;
    }
  }

  const XmlElement* error_details_tag =
      jingle_tag->FirstNamed(QName(kChromotingXmlNamespace, "error-details"));
  if (error_details_tag) {
    message->error_details = error_details_tag->BodyText();
  }

  const XmlElement* error_location_tag =
      jingle_tag->FirstNamed(QName(kChromotingXmlNamespace, "error-location"));
  if (error_location_tag) {
    message->error_location = error_location_tag->BodyText();
  }

  if (message->action == JingleMessage::ActionType::kSessionTerminate) {
    return true;
  }

  const XmlElement* content_tag =
      jingle_tag->FirstNamed(QName(kJingleNamespace, "content"));
  if (!content_tag) {
    *error = "content tag is missing";
    return false;
  }

  std::string content_name = content_tag->Attr(QName(kEmptyNamespace, "name"));
  if (content_name != ContentDescription::kChromotingContentName) {
    *error = "Unexpected content name: " + content_name;
    return false;
  }

  const XmlElement* webrtc_transport_tag =
      content_tag->FirstNamed(QName(kWebrtcTransportNamespace, "transport"));
  if (webrtc_transport_tag) {
    message->transport_info_legacy =
        std::make_unique<jingle_xmpp::XmlElement>(*webrtc_transport_tag);
  }

  message->description.reset();
  if (message->action == JingleMessage::ActionType::kSessionInitiate ||
      message->action == JingleMessage::ActionType::kSessionAccept) {
    const XmlElement* description_tag =
        content_tag->FirstNamed(QName(kChromotingXmlNamespace, "description"));
    if (!description_tag) {
      *error = "Missing chromoting content description";
      return false;
    }

    message->description = ContentDescription::ParseXml(
        description_tag, webrtc_transport_tag != nullptr);
    if (!message->description.get()) {
      *error = "Failed to parse content description";
      return false;
    }
  }

  if (!webrtc_transport_tag) {
    const XmlElement* ice_transport_tag =
        content_tag->FirstNamed(QName(kIceTransportNamespace, "transport"));
    if (ice_transport_tag) {
      message->transport_info_legacy =
          std::make_unique<jingle_xmpp::XmlElement>(*ice_transport_tag);
    }
  }

  return true;
}

std::unique_ptr<jingle_xmpp::XmlElement> IceTransportInfoToXml(
    const IceTransportInfo& transport) {
  std::unique_ptr<jingle_xmpp::XmlElement> result(
      new XmlElement(QName(kIceTransportNamespace, "transport"), true));
  for (const auto& credentials : transport.ice_credentials) {
    result->AddElement(FormatIceCredentials(credentials));
  }
  for (const auto& candidate : transport.candidates) {
    result->AddElement(FormatIceCandidate(candidate));
  }
  return result;
}

bool IceTransportInfoFromXml(const jingle_xmpp::XmlElement* element,
                             IceTransportInfo* transport) {
  if (element->Name() != QName(kIceTransportNamespace, "transport")) {
    return false;
  }

  transport->ice_credentials.clear();
  transport->candidates.clear();

  QName qn_credentials(kIceTransportNamespace, "credentials");
  for (const XmlElement* credentials_tag = element->FirstNamed(qn_credentials);
       credentials_tag;
       credentials_tag = credentials_tag->NextNamed(qn_credentials)) {
    IceTransportInfo::IceCredentials credentials;
    if (!ParseIceCredentials(credentials_tag, &credentials)) {
      return false;
    }
    transport->ice_credentials.push_back(credentials);
  }

  QName qn_candidate(kIceTransportNamespace, "candidate");
  for (const XmlElement* candidate_tag = element->FirstNamed(qn_candidate);
       candidate_tag; candidate_tag = candidate_tag->NextNamed(qn_candidate)) {
    IceTransportInfo::NamedCandidate candidate;
    if (!ParseIceCandidate(candidate_tag, &candidate)) {
      return false;
    }
    transport->candidates.push_back(candidate);
  }

  return true;
}

}  // namespace remoting::protocol
