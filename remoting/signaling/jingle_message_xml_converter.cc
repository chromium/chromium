// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_xml_converter.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/constants.h"
#include "remoting/base/name_value_map.h"
#include "remoting/signaling/content_description.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/webrtc/api/jsep.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting {

namespace {

const char kJabberNamespace[] = "jabber:client";
const jingle_xmpp::StaticQName kQNameIq = {kJabberNamespace, "iq"};

const char kJingleNamespace[] = "urn:xmpp:jingle:1";
const char kXmlNamespace[] = "http://www.w3.org/XML/1998/namespace";
const jingle_xmpp::StaticQName kQNameJingle = {kJingleNamespace, "jingle"};
const jingle_xmpp::StaticQName kQNameReason = {kJingleNamespace, "reason"};
const jingle_xmpp::StaticQName kQNameContent = {kJingleNamespace, "content"};

// Namespace for transport messages when using WebRTC.
const char kWebrtcTransportNamespace[] = "google:remoting:webrtc";
const jingle_xmpp::StaticQName kQNameWebrtcTransport = {
    kWebrtcTransportNamespace, "transport"};
const jingle_xmpp::StaticQName kQNameWebrtcCandidate = {
    kWebrtcTransportNamespace, "candidate"};
const jingle_xmpp::StaticQName kQNameSessionDescription = {
    kWebrtcTransportNamespace, "session-description"};

const char kEmptyNamespace[] = "";
const jingle_xmpp::StaticQName kQNameAction = {kEmptyNamespace, "action"};
const jingle_xmpp::StaticQName kQNameCreator = {kEmptyNamespace, "creator"};
const jingle_xmpp::StaticQName kQNameId = {kEmptyNamespace, "id"};
const jingle_xmpp::StaticQName kQNameInitiator = {kEmptyNamespace, "initiator"};
const jingle_xmpp::StaticQName kQNameName = {kEmptyNamespace, "name"};
const jingle_xmpp::StaticQName kQNameSdpMid = {kEmptyNamespace, "sdpMid"};
const jingle_xmpp::StaticQName kQNameSdpMLineIndex = {kEmptyNamespace,
                                                      "sdpMLineIndex"};
const jingle_xmpp::StaticQName kQNameSignature = {kEmptyNamespace, "signature"};
const jingle_xmpp::StaticQName kQNameSid = {kEmptyNamespace, "sid"};
const jingle_xmpp::StaticQName kQNameType = {kEmptyNamespace, "type"};

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
const jingle_xmpp::StaticQName kQNameHostAttributes = {kChromotingXmlNamespace,
                                                       "host-attributes"};
const jingle_xmpp::StaticQName kQNameHostConfiguration = {
    kChromotingXmlNamespace, "host-configuration"};

const jingle_xmpp::StaticQName kQNameAuthentication = {kChromotingXmlNamespace,
                                                       "authentication"};
const jingle_xmpp::StaticQName kQNameSpakeMessage = {kChromotingXmlNamespace,
                                                     "spake-message"};
const jingle_xmpp::StaticQName kQNameVerificationHash = {
    kChromotingXmlNamespace, "verification-hash"};
const jingle_xmpp::StaticQName kQNameCertificate = {kChromotingXmlNamespace,
                                                    "certificate"};
const jingle_xmpp::StaticQName kQNameHostToken = {kChromotingXmlNamespace,
                                                  "host-token"};
const jingle_xmpp::StaticQName kQNameSessionToken = {kChromotingXmlNamespace,
                                                     "session-token"};
const jingle_xmpp::StaticQName kQNamePairingInfo = {kChromotingXmlNamespace,
                                                    "pairing-info"};
const jingle_xmpp::StaticQName kQNamePairingFailed = {kChromotingXmlNamespace,
                                                      "pairing-failed"};
const jingle_xmpp::StaticQName kQNameTestId = {kChromotingXmlNamespace,
                                               "test-id"};
const jingle_xmpp::StaticQName kQNameTestKey = {kChromotingXmlNamespace,
                                                "test-key"};

const jingle_xmpp::StaticQName kQNameSupportedMethods = {kEmptyNamespace,
                                                         "supported-methods"};
const jingle_xmpp::StaticQName kQNameMethod = {kEmptyNamespace, "method"};
const jingle_xmpp::StaticQName kQNameClientId = {kEmptyNamespace, "client-id"};
const jingle_xmpp::StaticQName kQNameError = {kEmptyNamespace, "error"};

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

bool ParseWebrtcCandidate(const jingle_xmpp::XmlElement* element,
                          IceTransportInfo::NamedCandidate* candidate) {
  DCHECK(element->Name() == kQNameWebrtcCandidate);

  std::string candidate_str = element->BodyText();
  std::string sdp_mid = element->Attr(kQNameSdpMid);
  std::string sdp_mlineindex_str = element->Attr(kQNameSdpMLineIndex);
  int sdp_mlineindex;
  if (candidate_str.empty() || sdp_mid.empty() ||
      !base::StringToInt(sdp_mlineindex_str, &sdp_mlineindex)) {
    return false;
  }

  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::IceCandidate> webrtc_candidate =
      webrtc::IceCandidate::Create(sdp_mid, sdp_mlineindex, candidate_str,
                                   &error);
  if (!webrtc_candidate) {
    LOG(ERROR) << "Failed to parse incoming candidate: " << error.description
               << " line: " << error.line;
    return false;
  }

  candidate->name = sdp_mid;
  candidate->candidate = webrtc_candidate->candidate();
  candidate->sdp_m_line_index = sdp_mlineindex;

  return true;
}

XmlElement* FormatWebrtcCandidate(
    const IceTransportInfo::NamedCandidate& candidate) {
  auto result = std::make_unique<XmlElement>(kQNameWebrtcCandidate);

  webrtc::IceCandidate webrtc_candidate(
      candidate.name, *candidate.sdp_m_line_index, candidate.candidate);
  std::string candidate_str = webrtc_candidate.ToString();
  if (candidate_str.empty()) {
    LOG(ERROR) << "Failed to serialize local candidate.";
    return nullptr;
  }

  result->SetBodyText(candidate_str);
  result->SetAttr(kQNameSdpMid, candidate.name);
  result->SetAttr(kQNameSdpMLineIndex,
                  base::NumberToString(*candidate.sdp_m_line_index));

  return result.release();
}

XmlElement* FormatSessionDescription(const SessionDescription& description) {
  auto result = std::make_unique<XmlElement>(kQNameSessionDescription);
  std::string type_str;
  switch (description.type) {
    case SessionDescription::Type::kOffer:
      type_str = "offer";
      break;
    case SessionDescription::Type::kAnswer:
      type_str = "answer";
      break;
    default:
      NOTREACHED();
  }
  result->SetAttr(kQNameType, type_str);
  result->SetBodyText(description.sdp);
  if (!description.signature.empty()) {
    result->SetAttr(kQNameSignature, base::Base64Encode(description.signature));
  }
  return result.release();
}

bool ParseSessionDescription(const XmlElement* element,
                             SessionDescription* description) {
  DCHECK(element->Name() == kQNameSessionDescription);

  std::string type_str = element->Attr(kQNameType);
  if (type_str == "offer") {
    description->type = SessionDescription::Type::kOffer;
  } else if (type_str == "answer") {
    description->type = SessionDescription::Type::kAnswer;
  } else {
    return false;
  }

  description->sdp = element->BodyText();
  if (description->sdp.empty()) {
    return false;
  }

  std::string signature_str = element->Attr(kQNameSignature);
  if (!signature_str.empty()) {
    auto signature = base::Base64Decode(signature_str);
    if (signature) {
      description->signature = std::move(*signature);
    } else {
      return false;
    }
  }

  return true;
}

struct ContentTags {
  raw_ptr<const XmlElement> content = nullptr;
  raw_ptr<const XmlElement> webrtc_transport = nullptr;
};

bool ParseContentTags(const XmlElement* jingle_tag,
                      ContentTags& tags,
                      std::string* error) {
  tags.content = jingle_tag->FirstNamed(kQNameContent);
  if (!tags.content) {
    *error = "content tag is missing";
    return false;
  }

  std::string content_name = tags.content->Attr(kQNameName);
  if (content_name != ContentDescription::kChromotingContentName) {
    *error =
        base::StringPrintf("Unexpected content name: %s", content_name.c_str());
    return false;
  }

  tags.webrtc_transport = tags.content->FirstNamed(kQNameWebrtcTransport);
  return true;
}

bool ParseInitiateOrAccept(const XmlElement* jingle_tag,
                           JingleMessage::ActionType action,
                           JingleMessage* message,
                           std::string* error) {
  ContentTags tags;
  if (!ParseContentTags(jingle_tag, tags, error)) {
    return false;
  }

  std::optional<JingleTransportInfo> transport_info;
  if (tags.webrtc_transport) {
    transport_info.emplace();
    if (!JingleTransportInfoFromXml(tags.webrtc_transport, &*transport_info)) {
      *error = "Failed to parse JingleTransportInfo from XML (WebRTC)";
      return false;
    }
  }

  const XmlElement* description_tag =
      tags.content->FirstNamed(kQNameDescription);
  if (!description_tag) {
    *error = "Missing chromoting content description";
    return false;
  }

  message->description = ContentDescriptionFromXml(
      description_tag, tags.webrtc_transport != nullptr);
  if (!message->description) {
    *error = "Failed to parse content description";
    return false;
  }

  if (action == JingleMessage::ActionType::kSessionInitiate) {
    SessionInitiate initiate;
    initiate.authentication = message->description->authentication();
    initiate.transport_info = std::move(transport_info);
    message->SetPayload(std::move(initiate));
  } else {
    SessionAccept accept;
    accept.authentication = message->description->authentication();
    accept.transport_info = std::move(transport_info);
    message->SetPayload(std::move(accept));
  }
  return true;
}

bool ParseTransportInfoAction(const XmlElement* jingle_tag,
                              JingleMessage* message,
                              std::string* error) {
  ContentTags tags;
  if (!ParseContentTags(jingle_tag, tags, error)) {
    return false;
  }

  if (tags.webrtc_transport) {
    JingleTransportInfo transport_info;
    if (!JingleTransportInfoFromXml(tags.webrtc_transport, &transport_info)) {
      *error = "Failed to parse JingleTransportInfo from XML (WebRTC)";
      return false;
    }
    message->SetPayload(std::move(transport_info));
  } else {
    *error = "No transport found in transport-info message";
    return false;
  }

  return true;
}

bool ParseSessionInfoAction(const XmlElement* jingle_tag,
                            JingleMessage* message) {
  SessionInfo session_info;
  if (!SessionInfoFromXml(jingle_tag, &session_info)) {
    return false;
  }
  message->SetPayload(std::move(session_info));
  return true;
}

bool IsAuthenticatorMessage(const jingle_xmpp::XmlElement* message) {
  return message->Name() == kQNameAuthentication;
}

const jingle_xmpp::XmlElement* FindAuthenticatorMessage(
    const jingle_xmpp::XmlElement* message) {
  return message->FirstNamed(kQNameAuthentication);
}

std::unique_ptr<XmlElement> CreateIqElement(std::string_view type,
                                            std::string_view id,
                                            const SignalingAddress& to,
                                            const SignalingAddress& from) {
  auto iq = std::make_unique<XmlElement>(kQNameIq, /*useDefaultNs=*/true);
  iq->SetAttr(kQNameType, std::string(type));
  if (!id.empty()) {
    iq->SetAttr(kQNameId, std::string(id));
  }
  if (!to.empty()) {
    to.SetInMessage(iq.get(), SignalingAddress::TO);
  }
  if (!from.empty()) {
    from.SetInMessage(iq.get(), SignalingAddress::FROM);
  }
  return iq;
}

std::unique_ptr<XmlElement> CreateErrorElement(
    const JingleMessageReply& reply) {
  DCHECK_EQ(reply.reply_type, JingleMessageReply::REPLY_ERROR);
  auto error = std::make_unique<XmlElement>(QName(kJabberNamespace, "error"));

  DCHECK(reply.error_type.has_value());

  std::string type_attr;
  std::string error_text;
  QName name;
  switch (*reply.error_type) {
    case JingleMessageReply::BAD_REQUEST:
      type_attr = "modify";
      name = QName(kJabberNamespace, "bad-request");
      break;
    case JingleMessageReply::NOT_IMPLEMENTED:
      type_attr = "cancel";
      name = QName(kJabberNamespace, "feature-bad-request");
      break;
    case JingleMessageReply::INVALID_SID:
      type_attr = "modify";
      name = QName(kJabberNamespace, "item-not-found");
      error_text = "Invalid SID";
      break;
    case JingleMessageReply::UNEXPECTED_REQUEST:
      type_attr = "modify";
      name = QName(kJabberNamespace, "unexpected-request");
      break;
    case JingleMessageReply::UNSUPPORTED_INFO:
      type_attr = "modify";
      name = QName(kJabberNamespace, "feature-not-implemented");
      break;
    case JingleMessageReply::UNSPECIFIED:
      type_attr = "cancel";
      name = QName(kJabberNamespace, "unspecified-error");
      break;
    default:
      NOTREACHED();
  }

  if (!reply.text.empty()) {
    error_text = reply.text;
  }

  error->SetAttr(QName(kEmptyNamespace, "type"), type_attr);

  if (name.Namespace() != kJabberNamespace) {
    error->AddElement(
        new XmlElement(QName(kJabberNamespace, "undefined-condition")));
  }
  error->AddElement(new XmlElement(name));

  if (!error_text.empty()) {
    auto text_elem =
        std::make_unique<XmlElement>(QName(kJabberNamespace, "text"));
    text_elem->SetAttr(QName(kXmlNamespace, "lang"), "en");
    text_elem->SetBodyText(error_text);
    error->AddElement(text_elem.release());
  }

  return error;
}

}  // namespace

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageReplyToXml(
    const JingleMessageReply& reply) {
  auto iq =
      CreateIqElement((reply.reply_type == JingleMessageReply::REPLY_RESULT ||
                       !reply.error_type.has_value())
                          ? "result"
                          : "error",
                      reply.message_id, reply.to, reply.from);

  if (reply.reply_type == JingleMessageReply::REPLY_RESULT ||
      !reply.error_type.has_value()) {
    iq->AddElement(new XmlElement(kQNameJingle,
                                  /*useDefaultNs=*/true));
  } else {
    iq->AddElement(CreateErrorElement(reply).release());
  }

  return iq;
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageReplyToXml(
    const JingleMessageReply& reply,
    const JingleMessage& original_message) {
  auto iq = CreateIqElement(
      (reply.reply_type == JingleMessageReply::REPLY_RESULT ||
       !reply.error_type.has_value())
          ? "result"
          : "error",
      original_message.message_id, original_message.from, SignalingAddress());

  if (reply.reply_type == JingleMessageReply::REPLY_RESULT ||
      !reply.error_type.has_value()) {
    iq->AddElement(new XmlElement(kQNameJingle,
                                  /*useDefaultNs=*/true));
  } else {
    std::unique_ptr<XmlElement> original_xml =
        JingleMessageToXml(original_message);
    for (const XmlElement* child = original_xml->FirstElement(); child;
         child = child->NextElement()) {
      iq->AddElement(new XmlElement(*child));
    }
    iq->AddElement(CreateErrorElement(reply).release());
  }

  return iq;
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageReplyToXml(
    const JingleMessageReply& reply,
    const jingle_xmpp::XmlElement* request_stanza) {
  SignalingAddress original_from =
      SignalingAddress::Parse(request_stanza, SignalingAddress::FROM);

  auto iq = CreateIqElement(
      (reply.reply_type == JingleMessageReply::REPLY_RESULT ||
       !reply.error_type.has_value())
          ? "result"
          : "error",
      request_stanza->Attr(kQNameId), original_from, SignalingAddress());

  if (reply.reply_type == JingleMessageReply::REPLY_RESULT ||
      !reply.error_type.has_value()) {
    iq->AddElement(new XmlElement(kQNameJingle,
                                  /*useDefaultNs=*/true));
  } else {
    for (const XmlElement* child = request_stanza->FirstElement(); child;
         child = child->NextElement()) {
      iq->AddElement(new XmlElement(*child));
    }
    iq->AddElement(CreateErrorElement(reply).release());
  }

  return iq;
}

bool JingleMessageReplyFromXml(const jingle_xmpp::XmlElement* stanza,
                               JingleMessageReply* reply) {
  if (stanza->Name() != kQNameIq) {
    return false;
  }

  const std::string& type = stanza->Attr(kQNameType);
  if (type == "result") {
    reply->reply_type = JingleMessageReply::REPLY_RESULT;
    reply->error_type.reset();
    return true;
  }

  if (type != "error") {
    return false;
  }

  reply->reply_type = JingleMessageReply::REPLY_ERROR;

  const XmlElement* error_tag =
      stanza->FirstNamed(QName(kJabberNamespace, "error"));
  if (error_tag) {
    if (error_tag->FirstNamed(QName(kJabberNamespace, "bad-request"))) {
      reply->error_type = JingleMessageReply::BAD_REQUEST;
    } else if (error_tag->FirstNamed(
                   QName(kJabberNamespace, "feature-bad-request"))) {
      reply->error_type = JingleMessageReply::NOT_IMPLEMENTED;
    } else if (error_tag->FirstNamed(
                   QName(kJabberNamespace, "item-not-found"))) {
      reply->error_type = JingleMessageReply::INVALID_SID;
    } else if (error_tag->FirstNamed(
                   QName(kJabberNamespace, "unexpected-request"))) {
      reply->error_type = JingleMessageReply::UNEXPECTED_REQUEST;
    } else if (error_tag->FirstNamed(
                   QName(kJabberNamespace, "feature-not-implemented"))) {
      reply->error_type = JingleMessageReply::UNSUPPORTED_INFO;
    } else {
      reply->error_type = JingleMessageReply::UNSPECIFIED;
    }

    const XmlElement* text_tag =
        error_tag->FirstNamed(QName(kJabberNamespace, "text"));
    if (text_tag) {
      reply->text = text_tag->BodyText();
    }
  } else {
    reply->error_type = JingleMessageReply::UNSPECIFIED;
  }

  return true;
}

bool IsJingleMessage(const jingle_xmpp::XmlElement* stanza) {
  return stanza->Name() == kQNameIq && stanza->Attr(kQNameType) == "set" &&
         stanza->FirstNamed(kQNameJingle) != nullptr;
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessageToXml(
    const JingleMessage& message) {
  DCHECK(!message.to.empty());
  auto root =
      CreateIqElement("set", message.message_id, message.to, message.from);

  auto jingle_el =
      std::make_unique<XmlElement>(kQNameJingle, /*useDefaultNs=*/true);
  // Usually we would handle all initialization before adding the element but
  // this function is too complicated for that so we store a pointer to the
  // element since we know it is tied to the lifetime of `root` which is valid
  // within the function scope.
  XmlElement* jingle_tag = jingle_el.get();
  root->AddElement(jingle_el.release());

  jingle_tag->AddAttr(kQNameSid, message.sid);

  const char* action_attr =
      ValueToNameUnchecked(kActionTypes, message.action());
  if (!action_attr) {
    LOG(FATAL) << "Invalid action value " << static_cast<int>(message.action());
  }
  jingle_tag->AddAttr(kQNameAction, action_attr);

  for (const Attachment& attachment : message.attachments) {
    auto attachment_xml = AttachmentToXml(attachment);
    if (attachment_xml) {
      jingle_tag->AddElement(attachment_xml.release());
    }
  }

  if (message.action() == JingleMessage::ActionType::kSessionInfo) {
    if (auto* session_info = std::get_if<SessionInfo>(&message.payload())) {
      SessionInfoToXml(*session_info, jingle_tag);
    }
    return root;
  }

  if (message.action() == JingleMessage::ActionType::kSessionInitiate) {
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

  if (message.action() != JingleMessage::ActionType::kSessionTerminate) {
    auto content_tag = std::make_unique<XmlElement>(kQNameContent);
    content_tag->AddAttr(kQNameName,
                         ContentDescription::kChromotingContentName);
    content_tag->AddAttr(kQNameCreator, "initiator");

    if (message.description) {
      content_tag->AddElement(
          ContentDescriptionToXml(*message.description).release());
    }

    const JingleTransportInfo* transport_info = nullptr;
    if (auto* initiate = std::get_if<SessionInitiate>(&message.payload())) {
      if (initiate->transport_info) {
        transport_info = &*initiate->transport_info;
      }
    } else if (auto* accept = std::get_if<SessionAccept>(&message.payload())) {
      if (accept->transport_info) {
        transport_info = &*accept->transport_info;
      }
    } else if (auto* transport =
                   std::get_if<JingleTransportInfo>(&message.payload())) {
      transport_info = transport;
    }

    if (transport_info) {
      content_tag->AddElement(
          JingleTransportInfoToXml(*transport_info).release());
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
  if (stanza->Name() != kQNameIq) {
    *error = "Not an IQ stanza";
    return false;
  }

  std::string type = stanza->Attr(kQNameType);
  if (type != "set") {
    // This might be a result or error reply.
    *error =
        base::StringPrintf("Not a Jingle set message (type=%s)", type.c_str());
    return false;
  }

  const XmlElement* jingle_tag = stanza->FirstNamed(kQNameJingle);
  if (!jingle_tag) {
    *error = "jingle tag is missing";
    return false;
  }

  message->from = SignalingAddress::Parse(stanza, SignalingAddress::FROM);
  message->to = SignalingAddress::Parse(stanza, SignalingAddress::TO);
  if (message->from.empty() || message->to.empty()) {
    *error = "Missing signaling address";
    return false;
  }

  message->message_id = stanza->Attr(kQNameId);

  message->initiator = jingle_tag->Attr(kQNameInitiator);

  message->sid = jingle_tag->Attr(kQNameSid);
  if (message->sid.empty()) {
    *error = "sid attribute is missing";
    return false;
  }

  std::string action_str = jingle_tag->Attr(kQNameAction);
  if (action_str.empty()) {
    *error = "action attribute is missing";
    return false;
  }

  JingleMessage::ActionType action;
  if (!NameToValue(kActionTypes, action_str, &action)) {
    *error = base::StringPrintf("Unknown action %s", action_str.c_str());
    return false;
  }

  for (const XmlElement* attachments_tag =
           jingle_tag->FirstNamed(kQNameAttachments);
       attachments_tag;
       attachments_tag = attachments_tag->NextNamed(kQNameAttachments)) {
    Attachment attachment;
    if (AttachmentFromXml(attachments_tag, &attachment)) {
      message->attachments.emplace_back(std::move(attachment));
    } else {
      LOG(WARNING) << "Failed to convert attachment: "
                   << attachments_tag->Str();
    }
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

  switch (action) {
    case JingleMessage::ActionType::kSessionInitiate:
    case JingleMessage::ActionType::kSessionAccept:
      return ParseInitiateOrAccept(jingle_tag, action, message, error);

    case JingleMessage::ActionType::kTransportInfo:
      return ParseTransportInfoAction(jingle_tag, message, error);

    case JingleMessage::ActionType::kSessionTerminate:
      message->SetPayload(SessionTerminate());
      return true;

    case JingleMessage::ActionType::kSessionInfo:
      return ParseSessionInfoAction(jingle_tag, message);

    default:
      *error = "Unhandled action type";
      return false;
  }
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleTransportInfoToXml(
    const JingleTransportInfo& transport) {
  auto result = std::make_unique<XmlElement>(
      QName(transport.xml_namespace, "transport"), /*useDefaultNs=*/true);

  if (transport.xml_namespace == kWebrtcTransportNamespace) {
    if (transport.session_description) {
      result->AddElement(
          FormatSessionDescription(*transport.session_description));
    }
    for (const auto& candidate : transport.candidates) {
      auto* candidate_xml = FormatWebrtcCandidate(candidate);
      if (candidate_xml) {
        result->AddElement(candidate_xml);
      }
    }
  }

  return result;
}

bool JingleTransportInfoFromXml(const jingle_xmpp::XmlElement* element,
                                JingleTransportInfo* transport) {
  transport->xml_namespace = element->Name().Namespace();

  transport->candidates.clear();
  transport->session_description.reset();

  if (transport->xml_namespace == kWebrtcTransportNamespace) {
    const XmlElement* session_description_tag =
        element->FirstNamed(kQNameSessionDescription);
    if (session_description_tag) {
      SessionDescription session_description;
      if (!ParseSessionDescription(session_description_tag,
                                   &session_description)) {
        return false;
      }
      transport->session_description = std::move(session_description);
    }

    for (const XmlElement* candidate_tag =
             element->FirstNamed(kQNameWebrtcCandidate);
         candidate_tag;
         candidate_tag = candidate_tag->NextNamed(kQNameWebrtcCandidate)) {
      IceTransportInfo::NamedCandidate candidate;
      if (!ParseWebrtcCandidate(candidate_tag, &candidate)) {
        return false;
      }
      transport->candidates.push_back(std::move(candidate));
    }
  } else {
    return false;
  }

  return true;
}

std::unique_ptr<jingle_xmpp::XmlElement> AttachmentToXml(
    const Attachment& attachment) {
  auto result = std::make_unique<XmlElement>(kQNameAttachments);

  if (attachment.host_attributes) {
    auto attributes_tag = std::make_unique<XmlElement>(kQNameHostAttributes);
    attributes_tag->SetBodyText(
        base::JoinString(attachment.host_attributes->attribute, ","));
    result->AddElement(attributes_tag.release());
  }

  if (attachment.host_config) {
    auto config_tag = std::make_unique<XmlElement>(kQNameHostConfiguration);
    std::vector<std::string> pairs;
    for (const auto& setting : attachment.host_config->settings) {
      pairs.push_back(base::StringPrintf("%s:%s", setting.first.c_str(),
                                         setting.second.c_str()));
    }
    config_tag->SetBodyText(base::JoinString(pairs, ","));
    result->AddElement(config_tag.release());
  }

  return result;
}

bool AttachmentFromXml(const jingle_xmpp::XmlElement* element,
                       Attachment* attachment) {
  if (element->Name() != kQNameAttachments) {
    return false;
  }

  const XmlElement* attributes_tag = element->FirstNamed(kQNameHostAttributes);
  if (attributes_tag) {
    HostAttributesAttachment attributes;
    attributes.attribute =
        base::SplitString(attributes_tag->BodyText(), ",",
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    attachment->host_attributes = std::move(attributes);
  }

  const XmlElement* config_tag = element->FirstNamed(kQNameHostConfiguration);
  if (config_tag) {
    HostConfigAttachment config;
    base::StringPairs pairs;
    base::SplitStringIntoKeyValuePairs(config_tag->BodyText(), ':', ',',
                                       &pairs);
    for (const auto& pair : pairs) {
      config.settings[pair.first] = pair.second;
    }
    attachment->host_config = std::move(config);
  }

  return true;
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleAuthenticationToXml(
    const JingleAuthentication& authentication) {
  auto result = std::make_unique<XmlElement>(kQNameAuthentication);

  if (!authentication.supported_methods.empty()) {
    std::vector<std::string> method_names;
    for (const auto& method : authentication.supported_methods) {
      method_names.push_back(AuthenticationMethodToString(method));
    }
    result->SetAttr(kQNameSupportedMethods,
                    base::JoinString(method_names, ","));
  }

  if (authentication.method) {
    result->SetAttr(kQNameMethod,
                    AuthenticationMethodToString(*authentication.method));
  }

  if (!authentication.id.empty()) {
    result->SetAttr(kQNameId, authentication.id);
  }

  if (!authentication.spake_message.empty()) {
    auto spake_el = std::make_unique<XmlElement>(kQNameSpakeMessage);
    spake_el->SetBodyText(base::Base64Encode(authentication.spake_message));
    result->AddElement(spake_el.release());
  }

  if (!authentication.verification_hash.empty()) {
    auto hash_el = std::make_unique<XmlElement>(kQNameVerificationHash);
    hash_el->SetBodyText(base::Base64Encode(authentication.verification_hash));
    result->AddElement(hash_el.release());
  }

  if (!authentication.certificate.empty()) {
    auto cert_el = std::make_unique<XmlElement>(kQNameCertificate);
    cert_el->SetBodyText(base::Base64Encode(authentication.certificate));
    result->AddElement(cert_el.release());
  }

  if (!authentication.session_authz_host_token.empty()) {
    auto host_token_el = std::make_unique<XmlElement>(kQNameHostToken);
    host_token_el->SetBodyText(authentication.session_authz_host_token);
    result->AddElement(host_token_el.release());
  }

  if (!authentication.session_authz_session_token.empty()) {
    auto session_token_el = std::make_unique<XmlElement>(kQNameSessionToken);
    session_token_el->SetBodyText(authentication.session_authz_session_token);
    result->AddElement(session_token_el.release());
  }

  if (authentication.pairing_info) {
    auto pairing_el = std::make_unique<XmlElement>(kQNamePairingInfo);
    pairing_el->SetAttr(kQNameClientId, authentication.pairing_info->client_id);
    result->AddElement(pairing_el.release());
  }

  if (!authentication.pairing_error.empty()) {
    auto pairing_failed_el = std::make_unique<XmlElement>(kQNamePairingFailed);
    pairing_failed_el->SetAttr(kQNameError, authentication.pairing_error);
    result->AddElement(pairing_failed_el.release());
  }

  if (!authentication.test_id.empty()) {
    auto test_id_el = std::make_unique<XmlElement>(kQNameTestId);
    test_id_el->SetBodyText(authentication.test_id);
    result->AddElement(test_id_el.release());
  }

  if (!authentication.test_key.empty()) {
    auto test_key_el = std::make_unique<XmlElement>(kQNameTestKey);
    test_key_el->SetBodyText(base::Base64Encode(authentication.test_key));
    result->AddElement(test_key_el.release());
  }

  return result;
}

bool JingleAuthenticationFromXml(const jingle_xmpp::XmlElement* element,
                                 JingleAuthentication* authentication) {
  if (element->Name() != kQNameAuthentication) {
    return false;
  }

  std::string supported_methods_attr = element->Attr(kQNameSupportedMethods);
  if (!supported_methods_attr.empty()) {
    std::vector<std::string> method_names =
        base::SplitString(supported_methods_attr, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    for (const auto& name : method_names) {
      AuthenticationMethod method = ParseAuthenticationMethodString(name);
      if (method != AuthenticationMethod::INVALID) {
        authentication->supported_methods.push_back(method);
      }
    }
  }

  std::string method_attr = element->Attr(kQNameMethod);
  if (!method_attr.empty()) {
    authentication->method = ParseAuthenticationMethodString(method_attr);
  }

  authentication->id = element->Attr(kQNameId);

  const XmlElement* spake_el = element->FirstNamed(kQNameSpakeMessage);
  if (spake_el) {
    auto spake_message = base::Base64Decode(
        base::CollapseWhitespaceASCII(spake_el->BodyText(), false));
    if (spake_message.has_value()) {
      authentication->spake_message = std::move(*spake_message);
    } else {
      LOG(WARNING)
          << "Failed to decode the spake message in the incoming message";
      return false;
    }
  }

  const XmlElement* hash_el = element->FirstNamed(kQNameVerificationHash);
  if (hash_el) {
    auto verification_hash = base::Base64Decode(
        base::CollapseWhitespaceASCII(hash_el->BodyText(), false));
    if (verification_hash.has_value()) {
      authentication->verification_hash = std::move(*verification_hash);
    } else {
      LOG(WARNING)
          << "Failed to decode the verification hash in the incoming message";
      return false;
    }
  }

  const XmlElement* cert_el = element->FirstNamed(kQNameCertificate);
  if (cert_el) {
    auto certificate = base::Base64Decode(
        base::CollapseWhitespaceASCII(cert_el->BodyText(), false));
    if (certificate.has_value()) {
      authentication->certificate = std::move(*certificate);
    } else {
      LOG(WARNING)
          << "Failed to decode the certificate in the incoming message";
      return false;
    }
  }

  const XmlElement* host_token_el = element->FirstNamed(kQNameHostToken);
  if (host_token_el) {
    authentication->session_authz_host_token = host_token_el->BodyText();
  }

  const XmlElement* session_token_el = element->FirstNamed(kQNameSessionToken);
  if (session_token_el) {
    authentication->session_authz_session_token = session_token_el->BodyText();
  }

  const XmlElement* pairing_el = element->FirstNamed(kQNamePairingInfo);
  if (pairing_el) {
    JingleAuthentication::PairingInfo pairing_info;
    pairing_info.client_id = pairing_el->Attr(kQNameClientId);
    authentication->pairing_info = std::move(pairing_info);
  }

  const XmlElement* pairing_failed_el =
      element->FirstNamed(kQNamePairingFailed);
  if (pairing_failed_el) {
    authentication->pairing_error = pairing_failed_el->Attr(kQNameError);
  }

  const XmlElement* test_id_el = element->FirstNamed(kQNameTestId);
  if (test_id_el) {
    authentication->test_id = test_id_el->BodyText();
  }

  const XmlElement* test_key_el = element->FirstNamed(kQNameTestKey);
  if (test_key_el) {
    authentication->test_key = base::Base64Decode(test_key_el->BodyText())
                                   .value_or(std::vector<uint8_t>());
  }

  return true;
}

void SessionInfoToXml(const SessionInfo& session_info,
                      jingle_xmpp::XmlElement* jingle_element) {
  if (session_info.authentication) {
    jingle_element->AddElement(
        JingleAuthenticationToXml(*session_info.authentication).release());
  }
  if (session_info.generic_info) {
    auto generic_el = std::make_unique<XmlElement>(
        QName(session_info.generic_info->namespace_uri,
              session_info.generic_info->name));
    generic_el->SetBodyText(session_info.generic_info->body);
    jingle_element->AddElement(generic_el.release());
  }
}

bool SessionInfoFromXml(const jingle_xmpp::XmlElement* jingle_element,
                        SessionInfo* session_info) {
  const XmlElement* child = jingle_element->FirstElement();
  // Plugin messages are action independent, which should not be considered
  // as session-info.
  while (child && child->Name() == kQNameAttachments) {
    child = child->NextElement();
  }

  if (child) {
    if (IsAuthenticatorMessage(child)) {
      JingleAuthentication authentication;
      if (JingleAuthenticationFromXml(child, &authentication)) {
        session_info->authentication = std::move(authentication);
      }
    } else {
      SessionInfo::GenericInfo generic_info;
      generic_info.name = child->Name().LocalPart();
      generic_info.namespace_uri = child->Name().Namespace();
      generic_info.body = child->BodyText();
      session_info->generic_info = std::move(generic_info);
    }
  }

  return true;
}

std::unique_ptr<jingle_xmpp::XmlElement> ContentDescriptionToXml(
    const ContentDescription& description) {
  auto root = std::make_unique<XmlElement>(kQNameDescription, true);

  auto authentication_xml =
      JingleAuthenticationToXml(description.authentication());
  if (authentication_xml) {
    root->AddElement(authentication_xml.release());
  }

  return root;
}

std::unique_ptr<ContentDescription> ContentDescriptionFromXml(
    const jingle_xmpp::XmlElement* element,
    bool webrtc_transport) {
  if (element->Name() != kQNameDescription) {
    LOG(ERROR) << "Invalid description: " << element->Str();
    return nullptr;
  }
  std::unique_ptr<CandidateSessionConfig> config(
      CandidateSessionConfig::CreateEmpty());

  config->set_webrtc_supported(webrtc_transport);

  JingleAuthentication authentication;
  const XmlElement* child = FindAuthenticatorMessage(element);
  if (child) {
    JingleAuthenticationFromXml(child, &authentication);
  }

  return std::make_unique<ContentDescription>(std::move(config),
                                              authentication);
}

}  // namespace remoting
