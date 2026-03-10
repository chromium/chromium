// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jingle_message_proto_converter.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

namespace {

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

  if (std::holds_alternative<SessionInitiate>(message.payload())) {
    jingle->mutable_session_initiate();
  } else if (std::holds_alternative<SessionAccept>(message.payload())) {
    jingle->mutable_session_accept();
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
  } else if (std::holds_alternative<SessionInfo>(message.payload())) {
    jingle->mutable_session_info();
  } else if (std::holds_alternative<JingleTransportInfo>(message.payload())) {
    jingle->mutable_transport_info();
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
    message->SetPayload(SessionInitiate());
  } else if (jingle.has_session_accept()) {
    message->SetPayload(SessionAccept());
  } else if (jingle.has_session_terminate()) {
    SessionTerminate terminate;
    terminate.reason =
        ProtoTerminateReasonToJingle(jingle.session_terminate().reason());
    terminate.error_code = jingle.session_terminate().error_code();
    terminate.error_details = jingle.session_terminate().error_details();
    message->SetPayload(std::move(terminate));
  } else if (jingle.has_session_info()) {
    message->SetPayload(SessionInfo());
  } else if (jingle.has_transport_info()) {
    message->SetPayload(JingleTransportInfo());
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
