// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session_peer.h"

#include <string>
#include <utility>

#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/quic/quic_migration_attempt_context.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_socket_config_step.h"
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
      session->session_key_.require_dns_https_alpn(),
      session->session_key_.disable_cert_verification_network_fetches(),
      session->session_key_.target_network());
}

// static
QuicChromiumClientStream* QuicChromiumClientSessionPeer::CreateOutgoingStream(
    QuicChromiumClientSession* session) {
  return session->ShouldCreateOutgoingBidirectionalStream()
             ? session->CreateOutgoingReliableStreamImpl(
                   TRAFFIC_ANNOTATION_FOR_TESTS,
                   /*max_stream_limit_pending_delay=*/base::TimeDelta())
             : nullptr;
}

// static
void QuicChromiumClientSessionPeer::SetNumTotalStreamsForTesting(
    QuicChromiumClientSession* session,
    size_t num_total_streams) {
  session->num_total_streams_ = num_total_streams;
}

// static
bool QuicChromiumClientSessionPeer::GetSessionGoingAway(
    QuicChromiumClientSession* session) {
  return session->going_away_;
}

// static
QuicMigrationAttemptCause
QuicChromiumClientSessionPeer::GetCurrentMigrationCause(
    QuicChromiumClientSession* session) {
  return session->current_migration_cause_;
}

// static
void QuicChromiumClientSessionPeer::DisableConnectionMigration(
    QuicChromiumClientSession* session) {
  session->connection_migration_disabled_ = true;
}

// static
void QuicChromiumClientSessionPeer::SetDefaultNetwork(
    QuicChromiumClientSession* session,
    handles::NetworkHandle network) {
  session->default_network_ = network;
}

// static
bool QuicChromiumClientSessionPeer::IsMigrateBackToDefaultNetworkTimerRunning(
    QuicChromiumClientSession* session) {
  return session->migrate_back_to_default_timer_.IsRunning();
}

// static
void QuicChromiumClientSessionPeer::OnCryptoHandshakeComplete(
    QuicChromiumClientSession* session) {
  session->OnCryptoHandshakeComplete();
}

// static
void QuicChromiumClientSessionPeer::SetEchConfigList(
    QuicChromiumClientSession* session,
    std::vector<uint8_t> ech_config_list) {
  session->ech_config_list_ = std::move(ech_config_list);
}

// static
void QuicChromiumClientSessionPeer::StartProbing(
    QuicChromiumClientSession* session,
    QuicMigrationAttemptCause migration_cause,
    handles::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address,
    base::OnceCallback<void(ProbingResult)> probing_callback) {
  session->StartProbing(migration_cause, network, peer_address,
                        std::move(probing_callback));
}

// static
bool QuicChromiumClientSessionPeer::CanFallbackToIpv4OnConnectError(
    const QuicChromiumClientSession* session,
    const QuicMigrationAttemptContext& context,
    int rv,
    QuicSocketConfigStep error_step) {
  return session->CanFallbackToIpv4OnConnectError(context, rv, error_step);
}

}  // namespace net::test
