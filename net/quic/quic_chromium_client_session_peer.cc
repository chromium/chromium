// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session_peer.h"

#include <string>

#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/quic/quic_session_key.h"
#include "net/socket/socket_tag.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
namespace net::test {
// static
void QuicChromiumClientSessionPeer::SetHostname(
    QuicChromiumClientSession* session,
    const std::string& hostname) {
  quic::QuicServerId server_id(hostname,
                               session->session_key_.server_id().port());
  session->session_key_ = QuicSessionKey(
      server_id, session->session_key_.privacy_mode(),
      session->session_key_.proxy_chain(),
      session->session_key_.session_usage(), session->session_key_.socket_tag(),
      session->session_key_.network_anonymization_key(),
      session->session_key_.secure_dns_policy(),
      session->session_key_.require_dns_https_alpn());
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
