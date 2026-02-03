// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_message_xml_converter.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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
const jingle_xmpp::StaticQName kQNameIq = {kJabberNamespace, "iq"};

const char kJingleNamespace[] = "urn:xmpp:jingle:1";
const jingle_xmpp::StaticQName kQNameJingle = {kJingleNamespace, "jingle"};
const jingle_xmpp::StaticQName kQNameReason = {kJingleNamespace, "reason"};
const jingle_xmpp::StaticQName kQNameContent = {kJingleNamespace, "content"};

// Namespace for transport messages when using standard ICE.
const char kIceTransportNamespace[] = "google:remoting:ice";
const jingle_xmpp::StaticQName kQNameIceTransport = {kIceTransportNamespace,
                                                     "transport"};
const jingle_xmpp::StaticQName kQNameIceCredentials = {kIceTransportNamespace,
                                                       "credentials"};
const jingle_xmpp::StaticQName kQNameIceCandidate = {kIceTransportNamespace,
                                                     "candidate"};

// Namespace for transport messages when using WebRTC.
const char kWebrtcTransportNamespace[] = "google:remoting:webrtc";
const jingle_xmpp::StaticQName kQNameWebrtcTransport = {
    kWebrtcTransportNamespace, "transport"};

const char kEmptyNamespace[] = "";
const jingle_xmpp::StaticQName kQNameAction = {kEmptyNamespace, "action"};
const jingle_xmpp::StaticQName kQNameAddress = {kEmptyNamespace, "address"};
const jingle_xmpp::StaticQName kQNameChannel = {kEmptyNamespace, "channel"};
const jingle_xmpp::StaticQName kQNameCreator = {kEmptyNamespace, "creator"};
const jingle_xmpp::StaticQName kQNameFoundation = {kEmptyNamespace,
                                                   "foundation"};
const jingle_xmpp::StaticQName kQNameGeneration = {kEmptyNamespace,
                                                   "generation"};
const jingle_xmpp::StaticQName kQNameInitiator = {kEmptyNamespace, "initiator"};
const jingle_xmpp::StaticQName kQNameName = {kEmptyNamespace, "name"};
const jingle_xmpp::StaticQName kQNamePassword = {kEmptyNamespace, "password"};
const jingle_xmpp::StaticQName kQNamePort = {kEmptyNamespace, "port"};
const jingle_xmpp::StaticQName kQNamePriority = {kEmptyNamespace, "priority"};
const jingle_xmpp::StaticQName kQNameProtocol = {kEmptyNamespace, "protocol"};
const jingle_xmpp::StaticQName kQNameSid = {kEmptyNamespace, "sid"};
const jingle_xmpp::StaticQName kQNameType = {kEmptyNamespace, "type"};
const jingle_xmpp::StaticQName kQNameUfrag = {kEmptyNamespace, "ufrag"};

// Chromoting namespace constants.
const jingle_xmpp::StaticQName kQNameDescription = {kChromotingXmlNamespace,
                                                    "description"};
const jingle_xmpp::StaticQName kQNameErrorCode = {kChromotingXmlNamespace,
                                                  "error-code"};
const jingle_xmpp::StaticQName kQNameErrorDetails = {kChromotingXmlNamespace,
                                                     "error-details"};
const jingle_xmpp::StaticQName kQNameErrorLocation = {kChromotingXmlNamespace,
                                                      "error-location"};
const jingle_xmpp::StaticQName kQNameAttachments = {kChromotingXmlNamespace,
                                                    "attachments"};

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
  DCHECK(element->Name() == kQNameIceCredentials);

  const std::string& channel = element->Attr(kQNameChannel);
  const std::string& ufrag = element->Attr(kQNameUfrag);
  const std::string& password = element->Attr(kQNamePassword);

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
  DCHECK(element->Name() == kQNameIceCandidate);

  const std::string& name = element->Attr(kQNameName);
  const std::string& foundation = element->Attr(kQNameFoundation);
  const std::string& address = element->Attr(kQNameAddress);
  const std::string& port_str = element->Attr(kQNamePort);
  const std::optional<webrtc::IceCandidateType> type =
      LegacyTypeNameToCandidateType(element->Attr(kQNameType));
  const std::string& protocol = element->Attr(kQNameProtocol);
  const std::string& priority_str = element->Attr(kQNamePriority);
  const std::string& generation_str = element->Attr(kQNameGeneration);

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
  auto result = std::make_unique<XmlElement>(kQNameIceCredentials);
  result->SetAttr(kQNameChannel, credentials.channel);
  result->SetAttr(kQNameUfrag, credentials.ufrag);
  result->SetAttr(kQNamePassword, credentials.password);
  return result.release();
}

XmlElement* FormatIceCandidate(
    const IceTransportInfo::NamedCandidate& candidate) {
  auto result = std::make_unique<XmlElement>(kQNameIceCandidate);
  result->SetAttr(kQNameName, candidate.name);
  result->SetAttr(kQNameFoundation, candidate.candidate.foundation());
  result->SetAttr(kQNameAddress,
                  candidate.candidate.address().ipaddr().ToString());
  result->SetAttr(kQNamePort,
                  base::NumberToString(candidate.candidate.address().port()));
  result->SetAttr(kQNameType, GetLegacyTypeName(candidate.candidate));
  result->SetAttr(kQNameProtocol, candidate.candidate.protocol());
  result->SetAttr(kQNamePriority,
                  base::NumberToString(candidate.candidate.priority()));
  result->SetAttr(kQNameGeneration,
                  base::NumberToString(candidate.candidate.generation()));
  return result.release();
}

}  // namespace

bool IsJingleMessage(const jingle_xmpp::XmlElement* stanza) {
  return stanza->Name() == kQNameIq && stanza->Attr(kQNameType) == "set" &&
         stanza->FirstNamed(kQNameJingle) != nullptr;
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageToXml(
    const JingleMessage& message) {
  auto root = std::make_unique<XmlElement>(kQNameIq, /*useDefaultNs=*/true);

  DCHECK(!message.to.empty());
  root->SetAttr(kQNameType, "set");

  auto jingle_el =
      std::make_unique<XmlElement>(kQNameJingle, /*useDefaultNs=*/true);
  // Usually we would handle all initialization before adding the element but
  // this function is too complicated for that so we store a pointer to the
  // element since we know it is tied to the lifetime of `root` which is valid
  // within the function scope.
  XmlElement* jingle_tag = jingle_el.get();
  root->AddElement(jingle_el.release());

  jingle_tag->AddAttr(kQNameSid, message.sid);

  message.to.SetInMessage(root.get(), SignalingAddress::TO);
  if (!message.from.empty()) {
    message.from.SetInMessage(root.get(), SignalingAddress::FROM);
  }

  const char* action_attr = ValueToNameUnchecked(kActionTypes, message.action);
  if (!action_attr) {
    LOG(FATAL) << "Invalid action value " << static_cast<int>(message.action);
  }
  jingle_tag->AddAttr(kQNameAction, action_attr);

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
    jingle_tag->AddAttr(kQNameInitiator, message.initiator);
  }

  if (message.reason != SessionTerminate::Reason::kUnspecified) {
    auto reason_tag = std::make_unique<XmlElement>(kQNameReason);
    reason_tag->AddElement(new XmlElement(
        QName(kJingleNamespace, ValueToName(kReasons, message.reason))));
    jingle_tag->AddElement(reason_tag.release());

    if (message.error_code != ErrorCode::UNKNOWN_ERROR) {
      auto error_code_tag = std::make_unique<XmlElement>(kQNameErrorCode);
      error_code_tag->SetBodyText(ErrorCodeToString(message.error_code));
      jingle_tag->AddElement(error_code_tag.release());
    }

    if (!message.error_details.empty()) {
      auto error_details_tag = std::make_unique<XmlElement>(kQNameErrorDetails);
      error_details_tag->SetBodyText(message.error_details);
      jingle_tag->AddElement(error_details_tag.release());
    }

    if (!message.error_location.empty()) {
      auto error_location_tag =
          std::make_unique<XmlElement>(kQNameErrorLocation);
      error_location_tag->SetBodyText(message.error_location);
      jingle_tag->AddElement(error_location_tag.release());
    }
  }

  if (message.action != JingleMessage::ActionType::kSessionTerminate) {
    auto content_tag = std::make_unique<XmlElement>(kQNameContent);
    content_tag->AddAttr(kQNameName,
                         ContentDescription::kChromotingContentName);
    content_tag->AddAttr(kQNameCreator, "initiator");

    if (message.description) {
      content_tag->AddElement(message.description->ToXml());
    }

    if (message.transport_info_legacy) {
      content_tag->AddElement(new XmlElement(*message.transport_info_legacy));
    } else if (message.description &&
               message.description->config()->webrtc_supported()) {
      content_tag->AddElement(new XmlElement(kQNameWebrtcTransport));
    }
    jingle_tag->AddElement(content_tag.release());
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

  const XmlElement* jingle_tag = stanza->FirstNamed(kQNameJingle);
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

  message->initiator = jingle_tag->Attr(kQNameInitiator);

  std::string action_str = jingle_tag->Attr(kQNameAction);
  if (action_str.empty()) {
    *error = "action attribute is missing";
    return false;
  }
  if (!NameToValue(kActionTypes, action_str, &message->action)) {
    *error = "Unknown action " + action_str;
    return false;
  }

  message->sid = jingle_tag->Attr(kQNameSid);
  if (message->sid.empty()) {
    *error = "sid attribute is missing";
    return false;
  }

  const XmlElement* attachments_tag = jingle_tag->FirstNamed(kQNameAttachments);
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

  const XmlElement* reason_tag = jingle_tag->FirstNamed(kQNameReason);
  if (reason_tag && reason_tag->FirstElement()) {
    if (!NameToValue(kReasons, reason_tag->FirstElement()->Name().LocalPart(),
                     &message->reason)) {
      message->reason = SessionTerminate::Reason::kUnknownReason;
    }
  }

  const XmlElement* error_code_tag = jingle_tag->FirstNamed(kQNameErrorCode);
  if (error_code_tag && !error_code_tag->BodyText().empty()) {
    if (!ParseErrorCode(error_code_tag->BodyText(), &message->error_code)) {
      LOG(WARNING) << "Unknown error-code received "
                   << error_code_tag->BodyText();
      message->error_code = ErrorCode::UNKNOWN_ERROR;
    }
  }

  const XmlElement* error_details_tag =
      jingle_tag->FirstNamed(kQNameErrorDetails);
  if (error_details_tag) {
    message->error_details = error_details_tag->BodyText();
  }

  const XmlElement* error_location_tag =
      jingle_tag->FirstNamed(kQNameErrorLocation);
  if (error_location_tag) {
    message->error_location = error_location_tag->BodyText();
  }

  if (message->action == JingleMessage::ActionType::kSessionTerminate) {
    return true;
  }

  const XmlElement* content_tag = jingle_tag->FirstNamed(kQNameContent);
  if (!content_tag) {
    *error = "content tag is missing";
    return false;
  }

  std::string content_name = content_tag->Attr(kQNameName);
  if (content_name != ContentDescription::kChromotingContentName) {
    *error = "Unexpected content name: " + content_name;
    return false;
  }

  const XmlElement* webrtc_transport_tag =
      content_tag->FirstNamed(kQNameWebrtcTransport);
  if (webrtc_transport_tag) {
    message->transport_info_legacy =
        std::make_unique<jingle_xmpp::XmlElement>(*webrtc_transport_tag);
  }

  message->description.reset();
  if (message->action == JingleMessage::ActionType::kSessionInitiate ||
      message->action == JingleMessage::ActionType::kSessionAccept) {
    const XmlElement* description_tag =
        content_tag->FirstNamed(kQNameDescription);
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
        content_tag->FirstNamed(kQNameIceTransport);
    if (ice_transport_tag) {
      message->transport_info_legacy =
          std::make_unique<jingle_xmpp::XmlElement>(*ice_transport_tag);
    }
  }

  return true;
}

std::unique_ptr<jingle_xmpp::XmlElement> IceTransportInfoToXml(
    const IceTransportInfo& transport) {
  auto result =
      std::make_unique<XmlElement>(kQNameIceTransport, /*useDefaultNs=*/true);
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
  if (element->Name() != kQNameIceTransport) {
    return false;
  }

  transport->ice_credentials.clear();
  transport->candidates.clear();

  for (const XmlElement* credentials_tag =
           element->FirstNamed(kQNameIceCredentials);
       credentials_tag;
       credentials_tag = credentials_tag->NextNamed(kQNameIceCredentials)) {
    IceTransportInfo::IceCredentials credentials;
    if (!ParseIceCredentials(credentials_tag, &credentials)) {
      return false;
    }
    transport->ice_credentials.push_back(credentials);
  }

  for (const XmlElement* candidate_tag =
           element->FirstNamed(kQNameIceCandidate);
       candidate_tag;
       candidate_tag = candidate_tag->NextNamed(kQNameIceCandidate)) {
    IceTransportInfo::NamedCandidate candidate;
    if (!ParseIceCandidate(candidate_tag, &candidate)) {
      return false;
    }
    transport->candidates.push_back(candidate);
  }

  return true;
}

}  // namespace remoting::protocol
