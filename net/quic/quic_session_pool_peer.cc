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
    QuicSessionPool* factory,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return factory->GetCryptoConfigForTesting(network_anonymization_key);
}

bool QuicSessionPoolPeer::HasActiveSession(
    QuicSessionPool* factory,
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    bool require_dns_https_alpn) {
  return factory->HasActiveSession(
      QuicSessionKey(server_id, privacy_mode, proxy_chain, session_usage,
                     SocketTag(), network_anonymization_key,
                     SecureDnsPolicy::kAllow, require_dns_https_alpn));
}

bool QuicSessionPoolPeer::HasActiveJob(QuicSessionPool* factory,
                                       const quic::QuicServerId& server_id,
                                       PrivacyMode privacy_mode,
                                       bool require_dns_https_alpn) {
  return factory->HasActiveJob(QuicSessionKey(
      server_id, privacy_mode, ProxyChain::Direct(), SessionUsage::kDestination,
      SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      require_dns_https_alpn));
}

// static
QuicChromiumClientSession* QuicSessionPoolPeer::GetPendingSession(
    QuicSessionPool* factory,
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    url::SchemeHostPort destination) {
  QuicSessionKey session_key(server_id, privacy_mode, ProxyChain::Direct(),
                             SessionUsage::kDestination, SocketTag(),
                             NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                             /*require_dns_https_alpn=*/false);
  QuicSessionAliasKey key(std::move(destination), session_key);
  DCHECK(factory->HasActiveJob(session_key));
  DCHECK_EQ(factory->all_sessions_.size(), 1u);
  QuicChromiumClientSession* session = factory->all_sessions_.begin()->get();
  DCHECK(key == session->session_alias_key());
  return session;
}

QuicChromiumClientSession* QuicSessionPoolPeer::GetActiveSession(
    QuicSessionPool* factory,
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    const ProxyChain& proxy_chain,
    SessionUsage session_usage,
    bool require_dns_https_alpn) {
  QuicSessionKey session_key(server_id, privacy_mode, proxy_chain,
                             session_usage, SocketTag(),
                             network_anonymization_key, SecureDnsPolicy::kAllow,
                             require_dns_https_alpn);
  DCHECK(factory->HasActiveSession(session_key));
  return factory->active_sessions_[session_key];
}

bool QuicSessionPoolPeer::IsLiveSession(QuicSessionPool* factory,
                                        QuicChromiumClientSession* session) {
  return base::Contains(factory->all_sessions_, session);
}

void QuicSessionPoolPeer::SetTaskRunner(
    QuicSessionPool* factory,
    base::SequencedTaskRunner* task_runner) {
  factory->task_runner_ = task_runner;
}

void QuicSessionPoolPeer::SetTickClock(QuicSessionPool* factory,
                                       const base::TickClock* tick_clock) {
  factory->tick_clock_ = tick_clock;
}

quic::QuicTime::Delta QuicSessionPoolPeer::GetPingTimeout(
    QuicSessionPool* factory) {
  return factory->ping_timeout_;
}

void QuicSessionPoolPeer::SetYieldAfterPackets(QuicSessionPool* factory,
                                               int yield_after_packets) {
  factory->yield_after_packets_ = yield_after_packets;
}

void QuicSessionPoolPeer::SetYieldAfterDuration(
    QuicSessionPool* factory,
    quic::QuicTime::Delta yield_after_duration) {
  factory->yield_after_duration_ = yield_after_duration;
}

bool QuicSessionPoolPeer::CryptoConfigCacheIsEmpty(
    QuicSessionPool* factory,
    const quic::QuicServerId& quic_server_id,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return factory->CryptoConfigCacheIsEmptyForTesting(quic_server_id,
                                                     network_anonymization_key);
}

size_t QuicSessionPoolPeer::GetNumDegradingSessions(QuicSessionPool* factory) {
  return factory->connectivity_monitor_.GetNumDegradingSessions();
}

void QuicSessionPoolPeer::SetAlarmFactory(
    QuicSessionPool* factory,
    std::unique_ptr<quic::QuicAlarmFactory> alarm_factory) {
  factory->alarm_factory_ = std::move(alarm_factory);
}

}  // namespace net::test
