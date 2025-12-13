// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/proto/messaging_service.h"

namespace remoting::internal {

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
