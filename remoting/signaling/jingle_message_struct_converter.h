// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_JINGLE_MESSAGE_STRUCT_CONVERTER_H_
#define REMOTING_SIGNALING_JINGLE_MESSAGE_STRUCT_CONVERTER_H_

#include <string>

#include "remoting/proto/messaging_service.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

// Converts between SignalingAddress and JabberIdStruct.
internal::JabberIdStruct SignalingAddressToJabberIdStruct(
    const SignalingAddress& address);
SignalingAddress JabberIdStructToSignalingAddress(
    const internal::JabberIdStruct& jabber_id);

// Conversions for nested structs.
internal::SessionDescriptionStruct SessionDescriptionToStruct(
    const SessionDescription& description);
SessionDescription SessionDescriptionFromStruct(
    const internal::SessionDescriptionStruct& struct_val);

internal::IceCandidateStruct IceCandidateToStruct(
    const IceTransportInfo::NamedCandidate& candidate);
IceTransportInfo::NamedCandidate IceCandidateFromStruct(
    const internal::IceCandidateStruct& struct_val);

internal::TransportInfoStruct TransportInfoToStruct(
    const JingleTransportInfo& transport);
JingleTransportInfo TransportInfoFromStruct(
    const internal::TransportInfoStruct& struct_val);

internal::HostAttributesAttachmentStruct HostAttributesToStruct(
    const HostAttributesAttachment& attributes);
HostAttributesAttachment HostAttributesFromStruct(
    const internal::HostAttributesAttachmentStruct& struct_val);

internal::HostConfigAttachmentStruct HostConfigToStruct(
    const HostConfigAttachment& config);
HostConfigAttachment HostConfigFromStruct(
    const internal::HostConfigAttachmentStruct& struct_val);

internal::AttachmentStruct AttachmentToStruct(const Attachment& attachment);
Attachment AttachmentFromStruct(const internal::AttachmentStruct& struct_val);

internal::AuthenticationStruct AuthenticationToStruct(
    const JingleAuthentication& auth);
JingleAuthentication AuthenticationFromStruct(
    const internal::AuthenticationStruct& struct_val);

internal::SessionInitiateStruct SessionInitiateToStruct(
    const SessionInitiate& initiate);
SessionInitiate SessionInitiateFromStruct(
    const internal::SessionInitiateStruct& struct_val);

internal::SessionAcceptStruct SessionAcceptToStruct(
    const SessionAccept& accept);
SessionAccept SessionAcceptFromStruct(
    const internal::SessionAcceptStruct& struct_val);

internal::SessionInfoStruct SessionInfoToStruct(const SessionInfo& info);
SessionInfo SessionInfoFromStruct(
    const internal::SessionInfoStruct& struct_val);

internal::SessionTerminateStruct SessionTerminateToStruct(
    const SessionTerminate& terminate);
SessionTerminate SessionTerminateFromStruct(
    const internal::SessionTerminateStruct& struct_val);

// Converts between JingleMessage and its struct representation.
internal::IqStanzaStruct JingleMessageToStruct(const JingleMessage& message);
bool JingleMessageFromStruct(const internal::IqStanzaStruct& stanza,
                             JingleMessage* message,
                             std::string* error);

// Converts between JingleMessageReply and its struct representation.
internal::IqStanzaStruct JingleMessageReplyToStruct(
    const JingleMessageReply& reply);
bool JingleMessageReplyFromStruct(const internal::IqStanzaStruct& stanza,
                                  JingleMessageReply* reply);

}  // namespace remoting

#endif  // REMOTING_SIGNALING_JINGLE_MESSAGE_STRUCT_CONVERTER_H_
