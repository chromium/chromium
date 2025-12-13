// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_peer.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/socket_tag.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "url/scheme_host_port.h"

using std::string;

namespace net::test {

const quic::QuicConfig* QuicSessionPoolPeer::GetConfig(
    QuicSessionPool* factory) {
  return &factory->config_;
}

std::unique_ptr<QuicCryptoClientConfigHandle>
QuicSessionPoolPeer::GetCryptoConfig(
    QuicSessionPool* pool,
    QuicSessionPool::QuicCryptoClientConfigKey key) {
  return pool->GetCryptoConfigForTesting(std::move(key));
}

bool QuicSessionPoolPeer::HasActiveSession(
    QuicSessionPool* pool,
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    bool require_dns_https_alpn,
    bool disable_cert_verification_network_fetches) {
  return pool->HasActiveSession(QuicSessionKey(
      server_id, privacy_mode, proxy_chain, session_usage, SocketTag(),
      network_anonymization_key, SecureDnsPolicy::kAllow,
      require_dns_https_alpn, disable_cert_verification_network_fetches));
}

bool QuicSessionPoolPeer::HasActiveJob(QuicSessionPool* pool,
                                       const quic::QuicServerId& server_id,
                                       PrivacyMode privacy_mode,
                                       bool require_dns_https_alpn) {
  return pool->HasActiveJob(QuicSessionKey(
      server_id, privacy_mode, ProxyChain::Direct(), SessionUsage::kDestination,
      SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      require_dns_https_alpn,
      /*disable_cert_verification_network_fetches=*/false));
}

// static
QuicChromiumClientSession* QuicSessionPoolPeer::GetPendingSession(
    QuicSessionPool* pool,
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    url::SchemeHostPort destination) {
  QuicSessionKey session_key(
      server_id, privacy_mode, ProxyChain::Direct(), SessionUsage::kDestination,
      SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*require_dns_https_alpn=*/false,
      /*disable_cert_verification_network_fetches=*/false);
  QuicSessionAliasKey key(std::move(destination), session_key);
  DCHECK(pool->HasActiveJob(session_key));
  DCHECK_EQ(pool->all_sessions_.size(), 1u);
  QuicChromiumClientSession* session = pool->all_sessions_.begin()->get();
  DCHECK(key == session->session_alias_key());
  return session;
}

QuicChromiumClientSession* QuicSessionPoolPeer::GetActiveSession(
    QuicSessionPool* pool,
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    bool require_dns_https_alpn,
    bool disable_cert_verification_network_fetches) {
  QuicSessionKey session_key(
      server_id, privacy_mode, proxy_chain, session_usage, SocketTag(),
      network_anonymization_key, SecureDnsPolicy::kAllow,
      require_dns_https_alpn, disable_cert_verification_network_fetches);
  DCHECK(pool->HasActiveSession(session_key));
  return pool->active_sessions_[session_key];
}

bool QuicSessionPoolPeer::IsLiveSession(QuicSessionPool* pool,
                                        QuicChromiumClientSession* session) {
  return base::Contains(pool->all_sessions_, session);
}

void QuicSessionPoolPeer::SetTaskRunner(
    QuicSessionPool* pool,
    base::SequencedTaskRunner* task_runner) {
  pool->task_runner_ = task_runner;
}

void QuicSessionPoolPeer::SetTickClock(QuicSessionPool* pool,
                                       const base::TickClock* tick_clock) {
  pool->tick_clock_ = tick_clock;
}

quic::QuicTime::Delta QuicSessionPoolPeer::GetPingTimeout(
    QuicSessionPool* pool) {
  return pool->ping_timeout_;
}

void QuicSessionPoolPeer::SetYieldAfterPackets(QuicSessionPool* pool,
                                               int yield_after_packets) {
  pool->yield_after_packets_ = yield_after_packets;
}

void QuicSessionPoolPeer::SetYieldAfterDuration(
    QuicSessionPool* pool,
    quic::QuicTime::Delta yield_after_duration) {
  pool->yield_after_duration_ = yield_after_duration;
}

bool QuicSessionPoolPeer::CryptoConfigCacheIsEmpty(
    QuicSessionPool* pool,
    const quic::QuicServerId& quic_server_id,
    QuicSessionPool::QuicCryptoClientConfigKey key) {
  return pool->CryptoConfigCacheIsEmptyForTesting(quic_server_id,
                                                  std::move(key));
}

size_t QuicSessionPoolPeer::GetNumDegradingSessions(QuicSessionPool* pool) {
  return pool->connectivity_monitor_.GetNumDegradingSessions();
}

void QuicSessionPoolPeer::SetAlarmFactory(
    QuicSessionPool* pool,
    std::unique_ptr<quic::QuicAlarmFactory> alarm_factory) {
  pool->alarm_factory_ = std::move(alarm_factory);
}

}  // namespace net::test
