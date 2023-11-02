// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session_peer.h"

#include "net/dns/public/secure_dns_policy.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace net::test {
// static
void QuicChromiumClientSessionPeer::SetHostname(
    QuicChromiumClientSession* session,
    const std::string& hostname) {
  quic::QuicServerId server_id(hostname,
                               session->session_key_.server_id().port(),
                               session->session_key_.privacy_mode());
  session->session_key_ =
      QuicSessionKey(server_id, SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow, /*require_dns_https_alpn=*/false);
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

// static
bool QuicChromiumClientSessionPeer::GetSessionGoingAway(
    QuicChromiumClientSession* session) {
  return session->going_away_;
}

// static
MigrationCause QuicChromiumClientSessionPeer::GetCurrentMigrationCause(
    QuicChromiumClientSession* session) {
  return session->current_migration_cause_;
}

}  // namespace net::test
