// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_data_structures.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/base/name_value_map.h"
#include "remoting/signaling/content_description.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace remoting {

namespace {

const NameMapElement<JingleMessage::ActionType> kActionTypes[] = {
    {JingleMessage::ActionType::kSessionInitiate, "session-initiate"},
    {JingleMessage::ActionType::kSessionAccept, "session-accept"},
    {JingleMessage::ActionType::kSessionTerminate, "session-terminate"},
    {JingleMessage::ActionType::kSessionInfo, "session-info"},
    {JingleMessage::ActionType::kTransportInfo, "transport-info"},
};

}  // namespace

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

JingleMessage::JingleMessage(const JingleMessage& other)
    : message_id(other.message_id),
      from(other.from),
      to(other.to),
      sid(other.sid),
      initiator(other.initiator),
      attachments(other.attachments),
      reason(other.reason),
      error_code(other.error_code),
      error_details(other.error_details),
      error_location(other.error_location),
      action_(other.action_),
      payload_(other.payload_) {
  if (other.description) {
    description = other.description->Clone();
  }
}

JingleMessage& JingleMessage::operator=(const JingleMessage& other) {
  if (this == &other) {
    return *this;
  }

  message_id = other.message_id;
  from = other.from;
  to = other.to;
  sid = other.sid;
  initiator = other.initiator;
  attachments = other.attachments;
  reason = other.reason;
  error_code = other.error_code;
  error_details = other.error_details;
  error_location = other.error_location;
  action_ = other.action_;
  payload_ = other.payload_;
  if (other.description) {
    description = other.description->Clone();
  } else {
    description.reset();
  }

  return *this;
}

JingleMessage::JingleMessage(JingleMessage&&) = default;

JingleMessage& JingleMessage::operator=(JingleMessage&&) = default;

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

JingleMessageReply::JingleMessageReply() = default;

JingleMessageReply::JingleMessageReply(ErrorType error)
    : JingleMessageReply(error, std::string()) {}

JingleMessageReply::JingleMessageReply(ErrorType error,
                                       const std::string& text_value)
    : reply_type(REPLY_ERROR), error_type(error), text(text_value) {}

JingleMessageReply::JingleMessageReply(const JingleMessageReply&) = default;

JingleMessageReply& JingleMessageReply::operator=(const JingleMessageReply&) =
    default;

JingleMessageReply::JingleMessageReply(JingleMessageReply&&) = default;

JingleMessageReply& JingleMessageReply::operator=(JingleMessageReply&&) =
    default;

JingleMessageReply::~JingleMessageReply() = default;

IceTransportInfo::IceTransportInfo() = default;
IceTransportInfo::~IceTransportInfo() = default;

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

bool JingleAuthentication::is_empty() const {
  return supported_methods.empty() && !method && spake_message.empty() &&
         verification_hash.empty() && certificate.empty() && !pairing_info &&
         session_authz_host_token.empty() &&
         session_authz_session_token.empty() && pairing_error.empty() &&
         id.empty() && test_id.empty() && test_key.empty();
}

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

SessionInfo::GenericInfo::GenericInfo() = default;
SessionInfo::GenericInfo::GenericInfo(const GenericInfo&) = default;
SessionInfo::GenericInfo::GenericInfo(GenericInfo&&) = default;
SessionInfo::GenericInfo& SessionInfo::GenericInfo::operator=(
    const GenericInfo&) = default;
SessionInfo::GenericInfo& SessionInfo::GenericInfo::operator=(GenericInfo&&) =
    default;
SessionInfo::GenericInfo::~GenericInfo() = default;

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

}  // namespace remoting
