// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session_peer.h"

#include "net/quic/quic_chromium_client_session.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace net {
namespace test {
// static
void QuicChromiumClientSessionPeer::SetHostname(
    QuicChromiumClientSession* session,
    const std::string& hostname) {
  quic::QuicServerId server_id(hostname,
                               session->session_key_.server_id().port(),
                               session->session_key_.privacy_mode());
  session->session_key_ =
      QuicSessionKey(server_id, SocketTag(), NetworkIsolationKey(),
                     false /* disable_secure_dns */);
}

// static
uint64_t QuicChromiumClientSessionPeer::GetPushedBytesCount(
    QuicChromiumClientSession* session) {
  return session->bytes_pushed_count_;
}

// static
uint64_t QuicChromiumClientSessionPeer::GetPushedAndUnclaimedBytesCount(
    QuicChromiumClientSession* session) {
  return session->bytes_pushed_and_unclaimed_count_;
}

// static
QuicChromiumClientStream* QuicChromiumClientSessionPeer::CreateOutgoingStream(
    QuicChromiumClientSession* session) {
  return session->ShouldCreateOutgoingBidirectionalStream()
             ? session->CreateOutgoingReliableStreamImpl(
                   TRAFFIC_ANNOTATION_FOR_TESTS)
             : nullptr;
}

}  // namespace test
}  // namespace net
