// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/proto/messaging_service.h"

namespace remoting::internal {

TransportInfoStruct::TransportInfoStruct() = default;
TransportInfoStruct::TransportInfoStruct(const TransportInfoStruct&) = default;
TransportInfoStruct& TransportInfoStruct::operator=(
    const TransportInfoStruct&) = default;
TransportInfoStruct::~TransportInfoStruct() = default;

HostAttributesAttachmentStruct::HostAttributesAttachmentStruct() = default;
HostAttributesAttachmentStruct::HostAttributesAttachmentStruct(
    const HostAttributesAttachmentStruct&) = default;
HostAttributesAttachmentStruct& HostAttributesAttachmentStruct::operator=(
    const HostAttributesAttachmentStruct&) = default;
HostAttributesAttachmentStruct::~HostAttributesAttachmentStruct() = default;

HostConfigAttachmentStruct::HostConfigAttachmentStruct() = default;
HostConfigAttachmentStruct::HostConfigAttachmentStruct(
    const HostConfigAttachmentStruct&) = default;
HostConfigAttachmentStruct& HostConfigAttachmentStruct::operator=(
    const HostConfigAttachmentStruct&) = default;
HostConfigAttachmentStruct::~HostConfigAttachmentStruct() = default;

AttachmentStruct::AttachmentStruct() = default;
AttachmentStruct::AttachmentStruct(const AttachmentStruct&) = default;
AttachmentStruct& AttachmentStruct::operator=(const AttachmentStruct&) =
    default;
AttachmentStruct::~AttachmentStruct() = default;

AuthenticationStruct::AuthenticationStruct() = default;
AuthenticationStruct::AuthenticationStruct(const AuthenticationStruct&) =
    default;
AuthenticationStruct& AuthenticationStruct::operator=(
    const AuthenticationStruct&) = default;
AuthenticationStruct::~AuthenticationStruct() = default;

SessionInitiateStruct::SessionInitiateStruct() = default;
SessionInitiateStruct::SessionInitiateStruct(const SessionInitiateStruct&) =
    default;
SessionInitiateStruct& SessionInitiateStruct::operator=(
    const SessionInitiateStruct&) = default;
SessionInitiateStruct::~SessionInitiateStruct() = default;

SessionAcceptStruct::SessionAcceptStruct() = default;
SessionAcceptStruct::SessionAcceptStruct(const SessionAcceptStruct&) = default;
SessionAcceptStruct& SessionAcceptStruct::operator=(
    const SessionAcceptStruct&) = default;
SessionAcceptStruct::~SessionAcceptStruct() = default;

SessionInfoStruct::SessionInfoStruct() = default;
SessionInfoStruct::SessionInfoStruct(const SessionInfoStruct&) = default;
SessionInfoStruct& SessionInfoStruct::operator=(const SessionInfoStruct&) =
    default;
SessionInfoStruct::~SessionInfoStruct() = default;

SessionTerminateStruct::SessionTerminateStruct() = default;
SessionTerminateStruct::SessionTerminateStruct(const SessionTerminateStruct&) =
    default;
SessionTerminateStruct& SessionTerminateStruct::operator=(
    const SessionTerminateStruct&) = default;
SessionTerminateStruct::~SessionTerminateStruct() = default;

JingleMessageStruct::JingleMessageStruct() = default;
JingleMessageStruct::JingleMessageStruct(const JingleMessageStruct&) = default;
JingleMessageStruct& JingleMessageStruct::operator=(
    const JingleMessageStruct&) = default;
JingleMessageStruct::~JingleMessageStruct() = default;

HostOpenChannelResponseStruct::HostOpenChannelResponseStruct() = default;
HostOpenChannelResponseStruct::~HostOpenChannelResponseStruct() = default;

IqStanzaStruct::IqStanzaStruct() = default;
IqStanzaStruct::IqStanzaStruct(const IqStanzaStruct&) = default;
IqStanzaStruct& IqStanzaStruct::operator=(const IqStanzaStruct&) = default;
IqStanzaStruct::~IqStanzaStruct() = default;

PeerMessageStruct::PeerMessageStruct() = default;
PeerMessageStruct::PeerMessageStruct(const PeerMessageStruct&) = default;
PeerMessageStruct& PeerMessageStruct::operator=(const PeerMessageStruct&) =
    default;
PeerMessageStruct::~PeerMessageStruct() = default;

SystemTestStruct::SystemTestStruct() = default;
SystemTestStruct::SystemTestStruct(const SystemTestStruct&) = default;
SystemTestStruct& SystemTestStruct::operator=(const SystemTestStruct&) =
    default;
SystemTestStruct::~SystemTestStruct() = default;

}  // namespace remoting::internal
