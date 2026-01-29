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
const char kXmlNamespace[] = "http://www.w3.org/XML/1998/namespace";

const NameMapElement<JingleMessage::ActionType> kActionTypes[] = {
    {JingleMessage::SESSION_INITIATE, "session-initiate"},
    {JingleMessage::SESSION_ACCEPT, "session-accept"},
    {JingleMessage::SESSION_TERMINATE, "session-terminate"},
    {JingleMessage::SESSION_INFO, "session-info"},
    {JingleMessage::TRANSPORT_INFO, "transport-info"},
};

const NameMapElement<JingleMessage::Reason> kReasons[] = {
    {JingleMessage::SUCCESS, "success"},
    {JingleMessage::DECLINE, "decline"},
    {JingleMessage::CANCEL, "cancel"},
    {JingleMessage::EXPIRED, "expired"},
    {JingleMessage::GENERAL_ERROR, "general-error"},
    {JingleMessage::FAILED_APPLICATION, "failed-application"},
    {JingleMessage::INCOMPATIBLE_PARAMETERS, "incompatible-parameters"},
};

}  // namespace

// static
bool JingleMessage::IsJingleMessage(const jingle_xmpp::XmlElement* stanza) {
  return stanza->Name() == QName(kJabberNamespace, "iq") &&
         stanza->Attr(QName(std::string(), "type")) == "set" &&
         stanza->FirstNamed(QName(kJingleNamespace, "jingle")) != nullptr;
}

// static
std::string JingleMessage::GetActionName(ActionType action) {
  return ValueToName(kActionTypes, action);
}

JingleMessage::JingleMessage() = default;

JingleMessage::JingleMessage(const SignalingAddress& to,
                             ActionType action,
                             const std::string& sid)
    : to(to), action(action), sid(sid) {}

JingleMessage::~JingleMessage() = default;

bool JingleMessage::ParseXml(const jingle_xmpp::XmlElement* stanza,
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

  from = SignalingAddress::Parse(stanza, SignalingAddress::FROM);
  to = SignalingAddress::Parse(stanza, SignalingAddress::TO);
  if (from.empty() || to.empty()) {
    *error = "Missing signaling address";
    return false;
  }

  initiator = jingle_tag->Attr(QName(kEmptyNamespace, "initiator"));

  std::string action_str = jingle_tag->Attr(QName(kEmptyNamespace, "action"));
  if (action_str.empty()) {
    *error = "action attribute is missing";
    return false;
  }
  if (!NameToValue(kActionTypes, action_str, &action)) {
    *error = "Unknown action " + action_str;
    return false;
  }

  sid = jingle_tag->Attr(QName(kEmptyNamespace, "sid"));
  if (sid.empty()) {
    *error = "sid attribute is missing";
    return false;
  }

  const XmlElement* attachments_tag =
      jingle_tag->FirstNamed(QName(kChromotingXmlNamespace, "attachments"));
  if (attachments_tag) {
    attachments = std::make_unique<XmlElement>(*attachments_tag);
  } else {
    attachments.reset();
  }

  if (action == SESSION_INFO) {
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
      info = std::make_unique<XmlElement>(*child);
    } else {
      info.reset();
    }
    return true;
  }

  const XmlElement* reason_tag =
      jingle_tag->FirstNamed(QName(kJingleNamespace, "reason"));
  if (reason_tag && reason_tag->FirstElement()) {
    if (!NameToValue(kReasons, reason_tag->FirstElement()->Name().LocalPart(),
                     &reason)) {
      reason = UNKNOWN_REASON;
    }
  }

  const XmlElement* error_code_tag =
      jingle_tag->FirstNamed(QName(kChromotingXmlNamespace, "error-code"));
  if (error_code_tag && !error_code_tag->BodyText().empty()) {
    if (!ParseErrorCode(error_code_tag->BodyText(), &error_code)) {
      LOG(WARNING) << "Unknown error-code received "
                   << error_code_tag->BodyText();
      error_code = ErrorCode::UNKNOWN_ERROR;
    }
  }

  const XmlElement* error_details_tag =
      jingle_tag->FirstNamed(QName(kChromotingXmlNamespace, "error-details"));
  if (error_details_tag) {
    error_details = error_details_tag->BodyText();
  }

  const XmlElement* error_location_tag =
      jingle_tag->FirstNamed(QName(kChromotingXmlNamespace, "error-location"));
  if (error_location_tag) {
    error_location = error_location_tag->BodyText();
  }

  if (action == SESSION_TERMINATE) {
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
    transport_info =
        std::make_unique<jingle_xmpp::XmlElement>(*webrtc_transport_tag);
  }

  description.reset();
  if (action == SESSION_INITIATE || action == SESSION_ACCEPT) {
    const XmlElement* description_tag =
        content_tag->FirstNamed(QName(kChromotingXmlNamespace, "description"));
    if (!description_tag) {
      *error = "Missing chromoting content description";
      return false;
    }

    description = ContentDescription::ParseXml(description_tag,
                                               webrtc_transport_tag != nullptr);
    if (!description.get()) {
      *error = "Failed to parse content description";
      return false;
    }
  }

  if (!webrtc_transport_tag) {
    const XmlElement* ice_transport_tag =
        content_tag->FirstNamed(QName(kIceTransportNamespace, "transport"));
    if (ice_transport_tag) {
      transport_info =
          std::make_unique<jingle_xmpp::XmlElement>(*ice_transport_tag);
    }
  }

  return true;
}

std::unique_ptr<jingle_xmpp::XmlElement> JingleMessage::ToXml() const {
  std::unique_ptr<XmlElement> root(
      new XmlElement(QName("jabber:client", "iq"), true));

  DCHECK(!to.empty());
  root->SetAttr(QName(kEmptyNamespace, "type"), "set");

  XmlElement* jingle_tag =
      new XmlElement(QName(kJingleNamespace, "jingle"), true);
  root->AddElement(jingle_tag);
  jingle_tag->AddAttr(QName(kEmptyNamespace, "sid"), sid);

  to.SetInMessage(root.get(), SignalingAddress::TO);
  if (!from.empty()) {
    from.SetInMessage(root.get(), SignalingAddress::FROM);
  }

  const char* action_attr = ValueToName(kActionTypes, action);
  if (!action_attr) {
    LOG(FATAL) << "Invalid action value " << action;
  }
  jingle_tag->AddAttr(QName(kEmptyNamespace, "action"), action_attr);

  if (attachments) {
    jingle_tag->AddElement(new XmlElement(*attachments));
  }

  if (action == SESSION_INFO) {
    if (info.get()) {
      jingle_tag->AddElement(new XmlElement(*info.get()));
    }
    return root;
  }

  if (action == SESSION_INITIATE) {
    jingle_tag->AddAttr(QName(kEmptyNamespace, "initiator"), initiator);
  }

  if (reason != UNKNOWN_REASON) {
    XmlElement* reason_tag = new XmlElement(QName(kJingleNamespace, "reason"));
    jingle_tag->AddElement(reason_tag);
    reason_tag->AddElement(
        new XmlElement(QName(kJingleNamespace, ValueToName(kReasons, reason))));

    if (error_code != ErrorCode::UNKNOWN_ERROR) {
      XmlElement* error_code_tag =
          new XmlElement(QName(kChromotingXmlNamespace, "error-code"));
      jingle_tag->AddElement(error_code_tag);
      error_code_tag->SetBodyText(ErrorCodeToString(error_code));
    }

    if (!error_details.empty()) {
      XmlElement* error_details_tag =
          new XmlElement(QName(kChromotingXmlNamespace, "error-details"));
      jingle_tag->AddElement(error_details_tag);
      error_details_tag->SetBodyText(error_details);
    }

    if (!error_location.empty()) {
      XmlElement* error_location_tag =
          new XmlElement(QName(kChromotingXmlNamespace, "error-location"));
      jingle_tag->AddElement(error_location_tag);
      error_location_tag->SetBodyText(error_location);
    }
  }

  if (action != SESSION_TERMINATE) {
    XmlElement* content_tag =
        new XmlElement(QName(kJingleNamespace, "content"));
    jingle_tag->AddElement(content_tag);

    content_tag->AddAttr(QName(kEmptyNamespace, "name"),
                         ContentDescription::kChromotingContentName);
    content_tag->AddAttr(QName(kEmptyNamespace, "creator"), "initiator");

    if (description) {
      content_tag->AddElement(description->ToXml());
    }

    if (transport_info) {
      content_tag->AddElement(new XmlElement(*transport_info));
    } else if (description && description->config()->webrtc_supported()) {
      content_tag->AddElement(
          new XmlElement(QName(kWebrtcTransportNamespace, "transport")));
    }
  }

  return root;
}

void JingleMessage::AddAttachment(std::unique_ptr<XmlElement> attachment) {
  DCHECK(attachment);
  if (!attachments) {
    attachments = std::make_unique<XmlElement>(
        QName(kChromotingXmlNamespace, "attachments"));
  }
  attachments->AddElement(attachment.release());
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

IceTransportInfo::NamedCandidate::NamedCandidate(
    const std::string& name,
    const webrtc::Candidate& candidate)
    : name(name), candidate(candidate) {}

IceTransportInfo::IceCredentials::IceCredentials(std::string channel,
                                                 std::string ufrag,
                                                 std::string password)
    : channel(channel), ufrag(ufrag), password(password) {}

IceTransportInfo::IceTransportInfo() = default;
IceTransportInfo::~IceTransportInfo() = default;

bool IceTransportInfo::ParseXml(const jingle_xmpp::XmlElement* element) {
  return IceTransportInfoFromXml(element, this);
}

std::unique_ptr<jingle_xmpp::XmlElement> IceTransportInfo::ToXml() const {
  return IceTransportInfoToXml(*this);
}

}  // namespace remoting::protocol
