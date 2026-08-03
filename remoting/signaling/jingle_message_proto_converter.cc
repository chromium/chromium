// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_proto_converter.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "remoting/signaling/content_description.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"
#include "third_party/webrtc/api/jsep.h"

namespace remoting {

namespace {

ftl::AuthenticationMethod AuthMethodToProto(AuthenticationMethod method) {
  switch (method) {
    case AuthenticationMethod::INVALID:
      return ftl::AUTHENTICATION_METHOD_UNSPECIFIED;
    case AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519:
      return ftl::AUTHENTICATION_METHOD_SPAKE2_CURVE25519;
    case AuthenticationMethod::PAIRED_SPAKE2_CURVE25519:
      return ftl::AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519;
    case AuthenticationMethod::CLOUD_SESSION_AUTHZ_SPAKE2_CURVE25519:
      return ftl::AUTHENTICATION_METHOD_CLOUD_SESSION_AUTHZ_SPAKE2_CURVE25519;
    case AuthenticationMethod::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519:
      return ftl::AUTHENTICATION_METHOD_CORP_SESSION_AUTHZ_SPAKE2_CURVE25519;
  }
  NOTREACHED();
}

AuthenticationMethod ProtoToAuthMethod(ftl::AuthenticationMethod method) {
  switch (method) {
    case ftl::AUTHENTICATION_METHOD_UNSPECIFIED:
      return AuthenticationMethod::INVALID;
    case ftl::AUTHENTICATION_METHOD_SPAKE2_CURVE25519:
      return AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519;
    case ftl::AUTHENTICATION_METHOD_PAIRED_SPAKE2_CURVE25519:
      return AuthenticationMethod::PAIRED_SPAKE2_CURVE25519;
    case ftl::AUTHENTICATION_METHOD_CLOUD_SESSION_AUTHZ_SPAKE2_CURVE25519:
      return AuthenticationMethod::CLOUD_SESSION_AUTHZ_SPAKE2_CURVE25519;
    case ftl::AUTHENTICATION_METHOD_CORP_SESSION_AUTHZ_SPAKE2_CURVE25519:
      return AuthenticationMethod::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519;
    default:
      return AuthenticationMethod::INVALID;
  }
}

void JingleAuthenticationToProto(const JingleAuthentication& auth,
                                 ftl::Authentication* proto) {
  for (auto method : auth.supported_methods) {
    proto->add_supported_methods(AuthMethodToProto(method));
  }
  if (auth.method) {
    proto->set_method(AuthMethodToProto(*auth.method));
  }
  if (!auth.spake_message.empty()) {
    proto->set_spake_message(auth.spake_message.data(),
                             auth.spake_message.size());
  }
  if (!auth.verification_hash.empty()) {
    proto->set_verification_hash(auth.verification_hash.data(),
                                 auth.verification_hash.size());
  }
  if (!auth.session_authz_host_token.empty()) {
    proto->set_session_authz_host_token(auth.session_authz_host_token);
  }
  if (!auth.session_authz_session_token.empty()) {
    proto->set_session_authz_session_token(auth.session_authz_session_token);
  }
}

void JingleAuthenticationFromProto(const ftl::Authentication& proto,
                                   JingleAuthentication* auth) {
  for (int i = 0; i < proto.supported_methods_size(); ++i) {
    auth->supported_methods.push_back(
        ProtoToAuthMethod(proto.supported_methods(i)));
  }
  if (proto.has_method()) {
    auth->method = ProtoToAuthMethod(proto.method());
  }
  if (proto.has_spake_message()) {
    auth->spake_message.assign(proto.spake_message().begin(),
                               proto.spake_message().end());
  }
  if (proto.has_verification_hash()) {
    auth->verification_hash.assign(proto.verification_hash().begin(),
                                   proto.verification_hash().end());
  }
  if (proto.has_session_authz_host_token()) {
    auth->session_authz_host_token = proto.session_authz_host_token();
  }
  if (proto.has_session_authz_session_token()) {
    auth->session_authz_session_token = proto.session_authz_session_token();
  }
}

ftl::SessionDescription::SdpType SdpTypeToProto(SessionDescription::Type type) {
  switch (type) {
    case SessionDescription::Type::kUnspecified:
      return ftl::SessionDescription::SDP_TYPE_UNSPECIFIED;
    case SessionDescription::Type::kOffer:
      return ftl::SessionDescription::OFFER;
    case SessionDescription::Type::kAnswer:
      return ftl::SessionDescription::ANSWER;
  }
  NOTREACHED();
}

SessionDescription::Type ProtoToSdpType(ftl::SessionDescription::SdpType type) {
  switch (type) {
    case ftl::SessionDescription::SDP_TYPE_UNSPECIFIED:
      return SessionDescription::Type::kUnspecified;
    case ftl::SessionDescription::OFFER:
      return SessionDescription::Type::kOffer;
    case ftl::SessionDescription::ANSWER:
      return SessionDescription::Type::kAnswer;
    default:
      return SessionDescription::Type::kUnspecified;
  }
}

void JingleTransportInfoToProto(const JingleTransportInfo& transport,
                                ftl::TransportInfo* proto) {
  if (transport.session_description) {
    ftl::SessionDescription* proto_sdp = proto->mutable_session_description();
    proto_sdp->set_type(SdpTypeToProto(transport.session_description->type));
    proto_sdp->set_sdp(transport.session_description->sdp);
    if (!transport.session_description->signature.empty()) {
      proto_sdp->set_signature(transport.session_description->signature.data(),
                               transport.session_description->signature.size());
    }
  }

  for (const auto& candidate : transport.candidates) {
    if (!candidate.sdp_m_line_index.has_value()) {
      LOG(WARNING) << "Ignoring candidate without sdp_m_line_index";
      continue;
    }
    ftl::IceCandidate* proto_candidate = proto->add_candidates();
    webrtc::IceCandidate webrtc_candidate(
        candidate.name, *candidate.sdp_m_line_index, candidate.candidate);
    std::string candidate_str = webrtc_candidate.ToString();
    proto_candidate->set_candidate(candidate_str);
    proto_candidate->set_sdp_mid(candidate.name);
    proto_candidate->set_sdp_m_line_index(*candidate.sdp_m_line_index);
  }
}

bool JingleTransportInfoFromProto(const ftl::TransportInfo& proto,
                                  JingleTransportInfo* transport) {
  if (proto.has_session_description()) {
    SessionDescription sdp;
    sdp.type = ProtoToSdpType(proto.session_description().type());
    sdp.sdp = proto.session_description().sdp();
    if (proto.session_description().has_signature()) {
      sdp.signature.assign(proto.session_description().signature().begin(),
                           proto.session_description().signature().end());
    }
    transport->session_description = std::move(sdp);
  }

  for (int i = 0; i < proto.candidates_size(); ++i) {
    const ftl::IceCandidate& proto_candidate = proto.candidates(i);
    if (!proto_candidate.has_sdp_m_line_index() ||
        !proto_candidate.has_sdp_mid() || !proto_candidate.has_candidate()) {
      LOG(WARNING) << "Incomplete candidate in proto, skipping";
      continue;
    }
    IceTransportInfo::NamedCandidate candidate;
    candidate.name = proto_candidate.sdp_mid();
    candidate.sdp_m_line_index = proto_candidate.sdp_m_line_index();

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidate> webrtc_candidate =
        webrtc::IceCandidate::Create(candidate.name,
                                     *candidate.sdp_m_line_index,
                                     proto_candidate.candidate(), &error);
    if (!webrtc_candidate) {
      LOG(WARNING) << "Failed to parse incoming candidate: "
                   << error.description << " line: " << error.line
                   << ", skipping";
      continue;
    }
    candidate.candidate = webrtc_candidate->candidate();
    transport->candidates.push_back(std::move(candidate));
  }
  return true;
}

void SignalingAddressToJabberId(const SignalingAddress& address,
                                ftl::JabberId* jabber_id) {
  if (address.empty()) {
    return;
  }

  // For FTL, the ID is usually the email address.
  // SignalingAddress might contain more info, but JabberId splits it.
  // This is a simplified mapping.
  std::string username;
  std::string registration_id;
  if (address.GetFtlInfo(&username, &registration_id)) {
    jabber_id->set_local_part(username);
    jabber_id->set_resource_part(registration_id);
  } else {
    jabber_id->set_local_part(address.id());
  }
}

SignalingAddress JabberIdToSignalingAddress(const ftl::JabberId& jabber_id) {
  if (jabber_id.local_part().empty()) {
    return SignalingAddress();
  }

  if (!jabber_id.resource_part().empty()) {
    return SignalingAddress::CreateFtlSignalingAddress(
        jabber_id.local_part(), jabber_id.resource_part());
  }

  return SignalingAddress(jabber_id.local_part());
}

ftl::SessionTerminate::Reason JingleTerminateReasonToProto(
    SessionTerminate::Reason reason) {
  switch (reason) {
    case SessionTerminate::Reason::kUnspecified:
      return ftl::SessionTerminate::REASON_UNSPECIFIED;
    case SessionTerminate::Reason::kSuccess:
      return ftl::SessionTerminate::SUCCESS;
    case SessionTerminate::Reason::kDecline:
      return ftl::SessionTerminate::DECLINE;
    case SessionTerminate::Reason::kCancel:
      return ftl::SessionTerminate::CANCEL;
    case SessionTerminate::Reason::kExpired:
      return ftl::SessionTerminate::EXPIRED;
    case SessionTerminate::Reason::kGeneralError:
      return ftl::SessionTerminate::GENERAL_ERROR;
    case SessionTerminate::Reason::kFailedApplication:
      return ftl::SessionTerminate::FAILED_APPLICATION;
    case SessionTerminate::Reason::kIncompatibleParameters:
      return ftl::SessionTerminate::INCOMPATIBLE_PARAMETERS;
    case SessionTerminate::Reason::kUnknownReason:
      return ftl::SessionTerminate::UNKNOWN_REASON;
  }
}

SessionTerminate::Reason ProtoTerminateReasonToJingle(
    ftl::SessionTerminate::Reason reason) {
  switch (reason) {
    case ftl::SessionTerminate::REASON_UNSPECIFIED:
      return SessionTerminate::Reason::kUnspecified;
    case ftl::SessionTerminate::SUCCESS:
      return SessionTerminate::Reason::kSuccess;
    case ftl::SessionTerminate::DECLINE:
      return SessionTerminate::Reason::kDecline;
    case ftl::SessionTerminate::CANCEL:
      return SessionTerminate::Reason::kCancel;
    case ftl::SessionTerminate::EXPIRED:
      return SessionTerminate::Reason::kExpired;
    case ftl::SessionTerminate::GENERAL_ERROR:
      return SessionTerminate::Reason::kGeneralError;
    case ftl::SessionTerminate::FAILED_APPLICATION:
      return SessionTerminate::Reason::kFailedApplication;
    case ftl::SessionTerminate::INCOMPATIBLE_PARAMETERS:
      return SessionTerminate::Reason::kIncompatibleParameters;
    case ftl::SessionTerminate::UNKNOWN_REASON:
      return SessionTerminate::Reason::kUnknownReason;
  }
}

}  // namespace

ftl::IqStanza JingleMessageToProto(const JingleMessage& message) {
  ftl::IqStanza stanza;
  stanza.set_id(message.message_id);
  SignalingAddressToJabberId(message.from, stanza.mutable_sender());
  SignalingAddressToJabberId(message.to, stanza.mutable_receiver());

  ftl::JingleMessage* jingle = stanza.mutable_jingle();
  jingle->set_session_id(message.sid);

  if (const auto* initiate = std::get_if<SessionInitiate>(&message.payload())) {
    ftl::SessionInitiate* proto_initiate = jingle->mutable_session_initiate();
    if (!message.initiator.empty()) {
      SignalingAddressToJabberId(SignalingAddress(message.initiator),
                                 proto_initiate->mutable_initiator());
    }
    if (initiate->authentication) {
      JingleAuthenticationToProto(*initiate->authentication,
                                  proto_initiate->mutable_authentication());
    }
  } else if (const auto* accept =
                 std::get_if<SessionAccept>(&message.payload())) {
    ftl::SessionAccept* proto_accept = jingle->mutable_session_accept();
    if (accept->authentication) {
      JingleAuthenticationToProto(*accept->authentication,
                                  proto_accept->mutable_authentication());
    }
  } else if (auto* terminate =
                 std::get_if<SessionTerminate>(&message.payload())) {
    ftl::SessionTerminate* proto_terminate =
        jingle->mutable_session_terminate();
    proto_terminate->set_reason(
        JingleTerminateReasonToProto(terminate->reason));
    if (!terminate->error_code.empty()) {
      proto_terminate->set_error_code(terminate->error_code);
    }
    if (!terminate->error_details.empty()) {
      proto_terminate->set_error_details(terminate->error_details);
    }
  } else if (const auto* session_info =
                 std::get_if<SessionInfo>(&message.payload())) {
    ftl::SessionInfo* proto_session_info = jingle->mutable_session_info();
    if (session_info->authentication) {
      JingleAuthenticationToProto(*session_info->authentication,
                                  proto_session_info->mutable_authentication());
    }
  } else if (const auto* transport =
                 std::get_if<JingleTransportInfo>(&message.payload())) {
    JingleTransportInfoToProto(*transport, jingle->mutable_transport_info());
  } else {
    NOTREACHED() << "Unknown message payload.";
  }
  return stanza;
}

bool JingleMessageFromProto(const ftl::IqStanza& stanza,
                            JingleMessage* message,
                            std::string* error) {
  if (!stanza.has_jingle()) {
    *error = "Stanza missing Jingle payload";
    return false;
  }

  const ftl::JingleMessage& jingle = stanza.jingle();
  message->message_id = stanza.id();
  message->from = JabberIdToSignalingAddress(stanza.sender());
  message->to = JabberIdToSignalingAddress(stanza.receiver());
  message->sid = jingle.session_id();

  if (jingle.has_session_initiate()) {
    SessionInitiate initiate;
    JingleAuthentication auth;
    if (jingle.session_initiate().has_authentication()) {
      JingleAuthenticationFromProto(jingle.session_initiate().authentication(),
                                    &auth);
      initiate.authentication = auth;
    }
    message->description = std::make_unique<ContentDescription>(auth);
    if (jingle.session_initiate().has_initiator()) {
      message->initiator =
          JabberIdToSignalingAddress(jingle.session_initiate().initiator())
              .id();
    }
    message->SetPayload(std::move(initiate));
  } else if (jingle.has_session_accept()) {
    SessionAccept accept;
    JingleAuthentication auth;
    if (jingle.session_accept().has_authentication()) {
      JingleAuthenticationFromProto(jingle.session_accept().authentication(),
                                    &auth);
      accept.authentication = auth;
    }
    message->description = std::make_unique<ContentDescription>(auth);
    message->SetPayload(std::move(accept));
  } else if (jingle.has_session_terminate()) {
    SessionTerminate terminate;
    terminate.reason =
        ProtoTerminateReasonToJingle(jingle.session_terminate().reason());
    terminate.error_code = jingle.session_terminate().error_code();
    terminate.error_details = jingle.session_terminate().error_details();
    message->SetPayload(std::move(terminate));
  } else if (jingle.has_session_info()) {
    SessionInfo session_info;
    if (jingle.session_info().has_authentication()) {
      JingleAuthentication auth;
      JingleAuthenticationFromProto(jingle.session_info().authentication(),
                                    &auth);
      session_info.authentication = auth;
    }
    message->SetPayload(std::move(session_info));
  } else if (jingle.has_transport_info()) {
    JingleTransportInfo transport;
    if (!JingleTransportInfoFromProto(jingle.transport_info(), &transport)) {
      *error = "Failed to parse TransportInfo from proto";
      return false;
    }
    message->SetPayload(std::move(transport));
  } else {
    *error = "Unknown Jingle action";
    return false;
  }

  return true;
}

ftl::IqStanza JingleMessageReplyToProto(const JingleMessageReply& reply) {
  ftl::IqStanza stanza;
  stanza.set_id(reply.message_id);
  SignalingAddressToJabberId(reply.from, stanza.mutable_sender());
  SignalingAddressToJabberId(reply.to, stanza.mutable_receiver());

  if (reply.reply_type == JingleMessageReply::REPLY_RESULT) {
    stanza.mutable_reply();
  } else {
    // Ensure we set the error case even if error_type is missing.
    ftl::ErrorStanza* error = stanza.mutable_error();
    if (!reply.text.empty()) {
      error->set_text(reply.text);
    }
    if (reply.error_type.has_value()) {
      switch (*reply.error_type) {
        case JingleMessageReply::BAD_REQUEST:
          error->set_condition(ftl::ErrorStanza::BAD_REQUEST);
          break;
        case JingleMessageReply::NOT_IMPLEMENTED:
          error->set_condition(ftl::ErrorStanza::NOT_IMPLEMENTED);
          break;
        case JingleMessageReply::INVALID_SID:
          error->set_condition(ftl::ErrorStanza::INVALID_SID);
          break;
        case JingleMessageReply::UNEXPECTED_REQUEST:
          error->set_condition(ftl::ErrorStanza::UNEXPECTED_REQUEST);
          break;
        case JingleMessageReply::UNSUPPORTED_INFO:
          error->set_condition(ftl::ErrorStanza::UNSUPPORTED_INFO);
          break;
        case JingleMessageReply::UNSPECIFIED:
          error->set_condition(ftl::ErrorStanza::CONDITION_UNSPECIFIED);
          break;
      }
    }
  }
  return stanza;
}

bool JingleMessageReplyFromProto(const ftl::IqStanza& stanza,
                                 JingleMessageReply* reply) {
  reply->message_id = stanza.id();
  reply->from = JabberIdToSignalingAddress(stanza.sender());
  reply->to = JabberIdToSignalingAddress(stanza.receiver());

  if (stanza.has_reply()) {
    reply->reply_type = JingleMessageReply::REPLY_RESULT;
    return true;
  } else if (stanza.has_error()) {
    reply->reply_type = JingleMessageReply::REPLY_ERROR;
    const ftl::ErrorStanza& error = stanza.error();
    switch (error.condition()) {
      case ftl::ErrorStanza::BAD_REQUEST:
        reply->error_type = JingleMessageReply::BAD_REQUEST;
        break;
      case ftl::ErrorStanza::NOT_IMPLEMENTED:
        reply->error_type = JingleMessageReply::NOT_IMPLEMENTED;
        break;
      case ftl::ErrorStanza::INVALID_SID:
        reply->error_type = JingleMessageReply::INVALID_SID;
        break;
      case ftl::ErrorStanza::UNEXPECTED_REQUEST:
        reply->error_type = JingleMessageReply::UNEXPECTED_REQUEST;
        break;
      case ftl::ErrorStanza::UNSUPPORTED_INFO:
        reply->error_type = JingleMessageReply::UNSUPPORTED_INFO;
        break;
      case ftl::ErrorStanza::CONDITION_UNSPECIFIED:
        reply->error_type = JingleMessageReply::UNSPECIFIED;
        break;
      default:
        reply->error_type = JingleMessageReply::UNSPECIFIED;
        break;
    }
    reply->text = error.text();
    return true;
  }

  return false;
}

}  // namespace remoting
