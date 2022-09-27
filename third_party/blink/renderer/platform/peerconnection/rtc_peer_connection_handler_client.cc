// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_peer_connection_handler_client.h"

namespace blink {

RTCPeerConnectionHandlerClient::~RTCPeerConnectionHandlerClient() = default;

void RTCPeerConnectionHandlerClient::ClosePeerConnection() {}

}  // namespace blink
