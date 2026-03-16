// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_struct_converter.h"

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

using internal::AttachmentStruct;
using internal::AuthenticationStruct;
using internal::ErrorStanzaStruct;
using internal::HostAttributesAttachmentStruct;
using internal::HostConfigAttachmentStruct;
using internal::IceCandidateStruct;
using internal::IqStanzaStruct;
using internal::JabberIdStruct;
using internal::JingleMessageStruct;
using internal::JingleReplyStruct;
using internal::SessionAcceptStruct;
using internal::SessionDescriptionStruct;
using internal::SessionInfoStruct;
using internal::SessionInitiateStruct;
using internal::SessionTerminateStruct;
using internal::TransportInfoStruct;

namespace {

SessionDescriptionStruct::SdpType ToSdpTypeStruct(
    SessionDescription::Type type) {
  switch (type) {
    case SessionDescription::Type::kUnspecified:
      return SessionDescriptionStruct::SdpType::kUnspecified;
    case SessionDescription::Type::kOffer:
      return SessionDescriptionStruct::SdpType::kOffer;
    case SessionDescription::Type::kAnswer:
      return SessionDescriptionStruct::SdpType::kAnswer;
    default:
      NOTREACHED();
  }
}

SessionDescription::Type FromSdpTypeStruct(
    SessionDescriptionStruct::SdpType type) {
  switch (type) {
    case SessionDescriptionStruct::SdpType::kUnspecified:
      return SessionDescription::Type::kUnspecified;
    case SessionDescriptionStruct::SdpType::kOffer:
      return SessionDescription::Type::kOffer;
    case SessionDescriptionStruct::SdpType::kAnswer:
      return SessionDescription::Type::kAnswer;
    default:
      NOTREACHED();
  }
}

SessionTerminateStruct::Reason ToTerminateReasonStruct(
    SessionTerminate::Reason reason) {
  switch (reason) {
    case SessionTerminate::Reason::kUnspecified:
      return SessionTerminateStruct::Reason::kUnspecified;
    case SessionTerminate::Reason::kSuccess:
      return SessionTerminateStruct::Reason::kSuccess;
    case SessionTerminate::Reason::kDecline:
      return SessionTerminateStruct::Reason::kDecline;
    case SessionTerminate::Reason::kCancel:
      return SessionTerminateStruct::Reason::kCancel;
    case SessionTerminate::Reason::kExpired:
      return SessionTerminateStruct::Reason::kExpired;
    case SessionTerminate::Reason::kGeneralError:
      return SessionTerminateStruct::Reason::kGeneralError;
    case SessionTerminate::Reason::kFailedApplication:
      return SessionTerminateStruct::Reason::kFailedApplication;
    case SessionTerminate::Reason::kIncompatibleParameters:
      return SessionTerminateStruct::Reason::kIncompatibleParameters;
    case SessionTerminate::Reason::kUnknownReason:
      return SessionTerminateStruct::Reason::kUnknownReason;
    default:
      NOTREACHED();
  }
}

SessionTerminate::Reason FromTerminateReasonStruct(
    SessionTerminateStruct::Reason reason) {
  switch (reason) {
    case SessionTerminateStruct::Reason::kUnspecified:
      return SessionTerminate::Reason::kUnspecified;
    case SessionTerminateStruct::Reason::kSuccess:
      return SessionTerminate::Reason::kSuccess;
    case SessionTerminateStruct::Reason::kDecline:
      return SessionTerminate::Reason::kDecline;
    case SessionTerminateStruct::Reason::kCancel:
      return SessionTerminate::Reason::kCancel;
    case SessionTerminateStruct::Reason::kExpired:
      return SessionTerminate::Reason::kExpired;
    case SessionTerminateStruct::Reason::kGeneralError:
      return SessionTerminate::Reason::kGeneralError;
    case SessionTerminateStruct::Reason::kFailedApplication:
      return SessionTerminate::Reason::kFailedApplication;
    case SessionTerminateStruct::Reason::kIncompatibleParameters:
      return SessionTerminate::Reason::kIncompatibleParameters;
    case SessionTerminateStruct::Reason::kUnknownReason:
      return SessionTerminate::Reason::kUnknownReason;
    default:
      NOTREACHED();
  }
}

ErrorStanzaStruct::Condition ToErrorConditionStruct(
    JingleMessageReply::ErrorType type) {
  switch (type) {
    case JingleMessageReply::BAD_REQUEST:
      return ErrorStanzaStruct::Condition::kBadRequest;
    case JingleMessageReply::NOT_IMPLEMENTED:
      return ErrorStanzaStruct::Condition::kNotImplemented;
    case JingleMessageReply::INVALID_SID:
      return ErrorStanzaStruct::Condition::kInvalidSid;
    case JingleMessageReply::UNEXPECTED_REQUEST:
      return ErrorStanzaStruct::Condition::kUnexpectedRequest;
    case JingleMessageReply::UNSUPPORTED_INFO:
      return ErrorStanzaStruct::Condition::kUnsupportedInfo;
    case JingleMessageReply::UNSPECIFIED:
    default:
      return ErrorStanzaStruct::Condition::kUnspecified;
  }
}

JingleMessageReply::ErrorType FromErrorConditionStruct(
    ErrorStanzaStruct::Condition condition) {
  switch (condition) {
    case ErrorStanzaStruct::Condition::kBadRequest:
      return JingleMessageReply::BAD_REQUEST;
    case ErrorStanzaStruct::Condition::kNotImplemented:
      return JingleMessageReply::NOT_IMPLEMENTED;
    case ErrorStanzaStruct::Condition::kInvalidSid:
      return JingleMessageReply::INVALID_SID;
    case ErrorStanzaStruct::Condition::kUnexpectedRequest:
      return JingleMessageReply::UNEXPECTED_REQUEST;
    case ErrorStanzaStruct::Condition::kUnsupportedInfo:
      return JingleMessageReply::UNSUPPORTED_INFO;
    case ErrorStanzaStruct::Condition::kUnspecified:
    default:
      return JingleMessageReply::UNSPECIFIED;
  }
}

}  // namespace

JabberIdStruct SignalingAddressToJabberIdStruct(
    const SignalingAddress& address) {
  JabberIdStruct jabber_id;
  if (address.empty()) {
    return jabber_id;
  }

  std::string_view email_part;
  if (auto split = base::SplitStringOnce(address.id(), '/');
      split.has_value()) {
    email_part = split->first;
    jabber_id.resource_part = std::string(split->second);
  } else {
    email_part = address.id();
  }

  if (auto split = base::SplitStringOnce(email_part, '@'); split.has_value()) {
    jabber_id.local_part = std::string(split->first);
    jabber_id.domain_part = std::string(split->second);
  } else {
    jabber_id.local_part = std::string(email_part);
  }

  return jabber_id;
}

SignalingAddress JabberIdStructToSignalingAddress(
    const JabberIdStruct& jabber_id) {
  if (jabber_id.local_part.empty()) {
    return SignalingAddress();
  }

  std::string id = jabber_id.local_part;
  if (!jabber_id.domain_part.empty()) {
    id += "@" + jabber_id.domain_part;
  }
  if (!jabber_id.resource_part.empty()) {
    id += "/" + jabber_id.resource_part;
  }
  return SignalingAddress(id);
}

SessionDescriptionStruct SessionDescriptionToStruct(
    const SessionDescription& description) {
  SessionDescriptionStruct struct_val;
  struct_val.type = ToSdpTypeStruct(description.type);
  struct_val.sdp = description.sdp;
  struct_val.signature =
      std::string(description.signature.begin(), description.signature.end());
  return struct_val;
}

SessionDescription SessionDescriptionFromStruct(
    const SessionDescriptionStruct& struct_val) {
  SessionDescription description;
  description.type = FromSdpTypeStruct(struct_val.type);
  description.sdp = struct_val.sdp;
  description.signature = std::vector<uint8_t>(struct_val.signature.begin(),
                                               struct_val.signature.end());
  return description;
}

IceCandidateStruct IceCandidateToStruct(
    const IceTransportInfo::NamedCandidate& candidate) {
  IceCandidateStruct struct_val;
  struct_val.candidate = candidate.candidate.ToCandidateAttribute(true);
  struct_val.sdp_mid = candidate.name;
  struct_val.sdp_m_line_index = candidate.sdp_m_line_index.value_or(-1);
  return struct_val;
}

IceTransportInfo::NamedCandidate IceCandidateFromStruct(
    const IceCandidateStruct& struct_val) {
  IceTransportInfo::NamedCandidate candidate;
  candidate.name = struct_val.sdp_mid;
  if (struct_val.sdp_m_line_index >= 0) {
    candidate.sdp_m_line_index = struct_val.sdp_m_line_index;
  } else {
    candidate.sdp_m_line_index = std::nullopt;
  }
  // Use ParseCandidateString to populate the candidate fields.
  auto parse_result =
      webrtc::Candidate::ParseCandidateString(struct_val.candidate);
  if (parse_result.ok()) {
    candidate.candidate = parse_result.value();
  } else {
    LOG(WARNING) << "Failed to parse candidate string: "
                 << struct_val.candidate;
  }
  return candidate;
}

TransportInfoStruct TransportInfoToStruct(
    const JingleTransportInfo& transport) {
  TransportInfoStruct struct_val;
  for (const auto& candidate : transport.candidates) {
    struct_val.candidates.push_back(IceCandidateToStruct(candidate));
  }
  if (transport.session_description.has_value()) {
    struct_val.session_description =
        SessionDescriptionToStruct(*transport.session_description);
  }
  return struct_val;
}

JingleTransportInfo TransportInfoFromStruct(
    const TransportInfoStruct& struct_val) {
  JingleTransportInfo transport;
  for (const auto& candidate_struct : struct_val.candidates) {
    transport.candidates.push_back(IceCandidateFromStruct(candidate_struct));
  }
  if (struct_val.session_description.has_value()) {
    transport.session_description =
        SessionDescriptionFromStruct(*struct_val.session_description);
  }
  return transport;
}

HostAttributesAttachmentStruct HostAttributesToStruct(
    const HostAttributesAttachment& attributes) {
  HostAttributesAttachmentStruct struct_val;
  struct_val.attribute = attributes.attribute;
  return struct_val;
}

HostAttributesAttachment HostAttributesFromStruct(
    const HostAttributesAttachmentStruct& struct_val) {
  HostAttributesAttachment attributes;
  attributes.attribute = struct_val.attribute;
  return attributes;
}

HostConfigAttachmentStruct HostConfigToStruct(
    const HostConfigAttachment& config) {
  HostConfigAttachmentStruct struct_val;
  for (const auto& pair : config.settings) {
    struct_val.settings.insert(pair);
  }
  return struct_val;
}

HostConfigAttachment HostConfigFromStruct(
    const HostConfigAttachmentStruct& struct_val) {
  HostConfigAttachment config;
  for (const auto& pair : struct_val.settings) {
    config.settings.insert(pair);
  }
  return config;
}

AttachmentStruct AttachmentToStruct(const Attachment& attachment) {
  AttachmentStruct struct_val;
  if (attachment.host_attributes.has_value()) {
    struct_val.host_attributes =
        HostAttributesToStruct(*attachment.host_attributes);
  }
  if (attachment.host_config.has_value()) {
    struct_val.host_config = HostConfigToStruct(*attachment.host_config);
  }
  return struct_val;
}

Attachment AttachmentFromStruct(const AttachmentStruct& struct_val) {
  Attachment attachment;
  if (struct_val.host_attributes.has_value()) {
    attachment.host_attributes =
        HostAttributesFromStruct(*struct_val.host_attributes);
  }
  if (struct_val.host_config.has_value()) {
    attachment.host_config = HostConfigFromStruct(*struct_val.host_config);
  }
  return attachment;
}

AuthenticationStruct AuthenticationToStruct(const JingleAuthentication& auth) {
  AuthenticationStruct struct_val;
  struct_val.supported_methods = auth.supported_methods;
  struct_val.method = auth.method;
  struct_val.spake_message =
      std::string(auth.spake_message.begin(), auth.spake_message.end());
  struct_val.verification_hash =
      std::string(auth.verification_hash.begin(), auth.verification_hash.end());
  struct_val.session_authz_host_token = auth.session_authz_host_token;
  struct_val.session_authz_session_token = auth.session_authz_session_token;
  return struct_val;
}

JingleAuthentication AuthenticationFromStruct(
    const AuthenticationStruct& struct_val) {
  JingleAuthentication auth;
  auth.supported_methods = struct_val.supported_methods;
  auth.method = struct_val.method;
  auth.spake_message = std::vector<uint8_t>(struct_val.spake_message.begin(),
                                            struct_val.spake_message.end());
  auth.verification_hash = std::vector<uint8_t>(
      struct_val.verification_hash.begin(), struct_val.verification_hash.end());
  auth.session_authz_host_token = struct_val.session_authz_host_token;
  auth.session_authz_session_token = struct_val.session_authz_session_token;
  return auth;
}

SessionInitiateStruct SessionInitiateToStruct(const SessionInitiate& initiate) {
  SessionInitiateStruct struct_val;
  if (initiate.authentication.has_value()) {
    struct_val.authentication =
        AuthenticationToStruct(*initiate.authentication);
  }
  return struct_val;
}

SessionInitiate SessionInitiateFromStruct(
    const SessionInitiateStruct& struct_val) {
  SessionInitiate initiate;
  if (struct_val.authentication.has_value()) {
    initiate.authentication =
        AuthenticationFromStruct(*struct_val.authentication);
  }
  return initiate;
}

SessionAcceptStruct SessionAcceptToStruct(const SessionAccept& accept) {
  SessionAcceptStruct struct_val;
  if (accept.authentication.has_value()) {
    struct_val.authentication = AuthenticationToStruct(*accept.authentication);
  }
  return struct_val;
}

SessionAccept SessionAcceptFromStruct(const SessionAcceptStruct& struct_val) {
  SessionAccept accept;
  if (struct_val.authentication.has_value()) {
    accept.authentication =
        AuthenticationFromStruct(*struct_val.authentication);
  }
  return accept;
}

SessionInfoStruct SessionInfoToStruct(const SessionInfo& info) {
  SessionInfoStruct struct_val;
  if (info.authentication.has_value()) {
    struct_val.authentication = AuthenticationToStruct(*info.authentication);
  }
  return struct_val;
}

SessionInfo SessionInfoFromStruct(const SessionInfoStruct& struct_val) {
  SessionInfo info;
  if (struct_val.authentication.has_value()) {
    info.authentication = AuthenticationFromStruct(*struct_val.authentication);
  }
  return info;
}

SessionTerminateStruct SessionTerminateToStruct(
    const SessionTerminate& terminate) {
  SessionTerminateStruct struct_val;
  struct_val.reason = ToTerminateReasonStruct(terminate.reason);
  struct_val.error_code = terminate.error_code;
  struct_val.error_details = terminate.error_details;
  struct_val.error_location = terminate.error_location;
  return struct_val;
}

SessionTerminate SessionTerminateFromStruct(
    const SessionTerminateStruct& struct_val) {
  SessionTerminate terminate;
  terminate.reason = FromTerminateReasonStruct(struct_val.reason);
  terminate.error_code = struct_val.error_code;
  terminate.error_details = struct_val.error_details;
  terminate.error_location = struct_val.error_location;
  return terminate;
}

IqStanzaStruct JingleMessageToStruct(const JingleMessage& message) {
  IqStanzaStruct stanza;
  stanza.id = message.message_id;
  stanza.sender = SignalingAddressToJabberIdStruct(message.from);
  stanza.receiver = SignalingAddressToJabberIdStruct(message.to);
  stanza.xml = message.ToSerializedXml();

  JingleMessageStruct jingle_struct;
  jingle_struct.session_id = message.sid;
  for (const auto& attachment : message.attachments) {
    jingle_struct.attachments.push_back(AttachmentToStruct(attachment));
  }

  std::visit(absl::Overload{
                 [&](const SessionInitiate& arg) {
                   auto initiate_struct = SessionInitiateToStruct(arg);
                   initiate_struct.initiator = SignalingAddressToJabberIdStruct(
                       SignalingAddress(message.initiator));
                   jingle_struct.action = std::move(initiate_struct);
                 },
                 [&](const SessionAccept& arg) {
                   jingle_struct.action = SessionAcceptToStruct(arg);
                 },
                 [&](const SessionInfo& arg) {
                   jingle_struct.action = SessionInfoToStruct(arg);
                 },
                 [&](const JingleTransportInfo& arg) {
                   jingle_struct.action = TransportInfoToStruct(arg);
                 },
                 [&](const SessionTerminate& arg) {
                   jingle_struct.action = SessionTerminateToStruct(arg);
                 },
                 [](std::monostate) { NOTREACHED(); }},
             message.payload());

  stanza.payload = std::move(jingle_struct);
  return stanza;
}

bool JingleMessageFromStruct(const IqStanzaStruct& stanza,
                             JingleMessage* message,
                             std::string* error) {
  const auto* jingle_struct = std::get_if<JingleMessageStruct>(&stanza.payload);
  if (jingle_struct) {
    message->sid = jingle_struct->session_id;
    for (const auto& attachment_struct : jingle_struct->attachments) {
      message->attachments.push_back(AttachmentFromStruct(attachment_struct));
    }

    std::visit(absl::Overload{
                   [&](const SessionInitiateStruct& arg) {
                     message->SetPayload(SessionInitiateFromStruct(arg));
                     message->initiator =
                         JabberIdStructToSignalingAddress(arg.initiator).id();
                   },
                   [&](const SessionAcceptStruct& arg) {
                     message->SetPayload(SessionAcceptFromStruct(arg));
                   },
                   [&](const SessionInfoStruct& arg) {
                     message->SetPayload(SessionInfoFromStruct(arg));
                   },
                   [&](const TransportInfoStruct& arg) {
                     message->SetPayload(TransportInfoFromStruct(arg));
                   },
                   [&](const SessionTerminateStruct& arg) {
                     message->SetPayload(SessionTerminateFromStruct(arg));
                   },
                   [](std::monostate) { NOTREACHED(); }},
               jingle_struct->action);
  } else {
    auto xml_stanza = base::WrapUnique<jingle_xmpp::XmlElement>(
        jingle_xmpp::XmlElement::ForStr(stanza.xml));
    if (!xml_stanza ||
        !JingleMessageFromXml(xml_stanza.get(), message, error)) {
      return false;
    }
  }

  // Top-level overrides from struct.
  if (!stanza.id.empty()) {
    message->message_id = stanza.id;
  }
  if (!stanza.sender.local_part.empty()) {
    message->from = JabberIdStructToSignalingAddress(stanza.sender);
  }
  if (!stanza.receiver.local_part.empty()) {
    message->to = JabberIdStructToSignalingAddress(stanza.receiver);
  }
  return true;
}

IqStanzaStruct JingleMessageReplyToStruct(const JingleMessageReply& reply) {
  IqStanzaStruct stanza;
  stanza.id = reply.message_id;
  stanza.sender = SignalingAddressToJabberIdStruct(reply.from);
  stanza.receiver = SignalingAddressToJabberIdStruct(reply.to);
  stanza.xml = reply.ToSerializedXml();

  if (reply.reply_type == JingleMessageReply::REPLY_RESULT) {
    stanza.payload = JingleReplyStruct();
  } else {
    ErrorStanzaStruct error_struct;
    if (reply.error_type.has_value()) {
      error_struct.condition = ToErrorConditionStruct(*reply.error_type);
    }
    error_struct.text = reply.text;
    stanza.payload = std::move(error_struct);
  }

  return stanza;
}

bool JingleMessageReplyFromStruct(const IqStanzaStruct& stanza,
                                  JingleMessageReply* reply) {
  const auto* reply_struct = std::get_if<JingleReplyStruct>(&stanza.payload);
  const auto* error_struct = std::get_if<ErrorStanzaStruct>(&stanza.payload);

  if (reply_struct) {
    reply->reply_type = JingleMessageReply::REPLY_RESULT;
  } else if (error_struct) {
    reply->reply_type = JingleMessageReply::REPLY_ERROR;
    reply->error_type = FromErrorConditionStruct(error_struct->condition);
    reply->text = error_struct->text;
  } else {
    auto xml_stanza = base::WrapUnique<jingle_xmpp::XmlElement>(
        jingle_xmpp::XmlElement::ForStr(stanza.xml));
    if (!xml_stanza || !JingleMessageReplyFromXml(xml_stanza.get(), reply)) {
      return false;
    }
  }

  // Top-level overrides from struct.
  if (!stanza.id.empty()) {
    reply->message_id = stanza.id;
  }
  if (!stanza.sender.local_part.empty()) {
    reply->from = JabberIdStructToSignalingAddress(stanza.sender);
  }
  if (!stanza.receiver.local_part.empty()) {
    reply->to = JabberIdStructToSignalingAddress(stanza.receiver);
  }
  return true;
}

}  // namespace remoting
