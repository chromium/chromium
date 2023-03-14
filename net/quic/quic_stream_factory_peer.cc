// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory_peer.h"

#include <string>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_stream_factory.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "url/scheme_host_port.h"

using std::string;

namespace net::test {

const quic::QuicConfig* QuicStreamFactoryPeer::GetConfig(
    QuicStreamFactory* factory) {
  return &factory->config_;
}

std::unique_ptr<QuicCryptoClientConfigHandle>
QuicStreamFactoryPeer::GetCryptoConfig(
    QuicStreamFactory* factory,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return factory->GetCryptoConfigForTesting(network_anonymization_key);
}

bool QuicStreamFactoryPeer::HasActiveSession(
    QuicStreamFactory* factory,
    const quic::QuicServerId& server_id,
    const NetworkAnonymizationKey& network_anonymization_key,
    bool require_dns_https_alpn) {
  return factory->HasActiveSession(
      QuicSessionKey(server_id, SocketTag(), network_anonymization_key,
                     SecureDnsPolicy::kAllow, require_dns_https_alpn));
}

bool QuicStreamFactoryPeer::HasActiveJob(QuicStreamFactory* factory,
                                         const quic::QuicServerId& server_id,
                                         bool require_dns_https_alpn) {
  return factory->HasActiveJob(
      QuicSessionKey(server_id, SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow, require_dns_https_alpn));
}

// static
QuicChromiumClientSession* QuicStreamFactoryPeer::GetPendingSession(
    QuicStreamFactory* factory,
    const quic::QuicServerId& server_id,
    url::SchemeHostPort destination) {
  QuicSessionKey session_key(server_id, SocketTag(), NetworkAnonymizationKey(),
                             SecureDnsPolicy::kAllow,
                             /*require_dns_https_alpn=*/false);
  QuicStreamFactory::QuicSessionAliasKey key(std::move(destination),
                                             session_key);
  DCHECK(factory->HasActiveJob(session_key));
  DCHECK_EQ(factory->all_sessions_.size(), 1u);
  DCHECK(key == factory->all_sessions_.begin()->second);
  return factory->all_sessions_.begin()->first;
}

QuicChromiumClientSession* QuicStreamFactoryPeer::GetActiveSession(
    QuicStreamFactory* factory,
    const quic::QuicServerId& server_id,
    const NetworkAnonymizationKey& network_anonymization_key,
    bool require_dns_https_alpn) {
  QuicSessionKey session_key(server_id, SocketTag(), network_anonymization_key,
                             SecureDnsPolicy::kAllow, require_dns_https_alpn);
  DCHECK(factory->HasActiveSession(session_key));
  return factory->active_sessions_[session_key];
}

bool QuicStreamFactoryPeer::HasLiveSession(QuicStreamFactory* factory,
                                           url::SchemeHostPort destination,
                                           const quic::QuicServerId& server_id,
                                           bool require_dns_https_alpn) {
  QuicSessionKey session_key =
      QuicSessionKey(server_id, SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow, require_dns_https_alpn);
  QuicStreamFactory::QuicSessionAliasKey alias_key(std::move(destination),
                                                   session_key);
  for (const auto& it : factory->all_sessions_) {
    if (it.second == alias_key)
      return true;
  }
  return false;
}

bool QuicStreamFactoryPeer::IsLiveSession(QuicStreamFactory* factory,
                                          QuicChromiumClientSession* session) {
  for (const auto& it : factory->all_sessions_) {
    if (it.first == session)
      return true;
  }
  return false;
}

void QuicStreamFactoryPeer::SetTaskRunner(
    QuicStreamFactory* factory,
    base::SequencedTaskRunner* task_runner) {
  factory->task_runner_ = task_runner;
}

void QuicStreamFactoryPeer::SetTickClock(QuicStreamFactory* factory,
                                         const base::TickClock* tick_clock) {
  factory->tick_clock_ = tick_clock;
}

quic::QuicTime::Delta QuicStreamFactoryPeer::GetPingTimeout(
    QuicStreamFactory* factory) {
  return factory->ping_timeout_;
}

void QuicStreamFactoryPeer::SetYieldAfterPackets(QuicStreamFactory* factory,
                                                 int yield_after_packets) {
  factory->yield_after_packets_ = yield_after_packets;
}

void QuicStreamFactoryPeer::SetYieldAfterDuration(
    QuicStreamFactory* factory,
    quic::QuicTime::Delta yield_after_duration) {
  factory->yield_after_duration_ = yield_after_duration;
}

bool QuicStreamFactoryPeer::CryptoConfigCacheIsEmpty(
    QuicStreamFactory* factory,
    const quic::QuicServerId& quic_server_id,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return factory->CryptoConfigCacheIsEmptyForTesting(quic_server_id,
                                                     network_anonymization_key);
}

void QuicStreamFactoryPeer::CacheDummyServerConfig(
    QuicStreamFactory* factory,
    const quic::QuicServerId& quic_server_id,
    const NetworkAnonymizationKey& network_anonymization_key) {
  // Minimum SCFG that passes config validation checks.
  const char scfg[] = {// SCFG
                       0x53, 0x43, 0x46, 0x47,
                       // num entries
                       0x01, 0x00,
                       // padding
                       0x00, 0x00,
                       // EXPY
                       0x45, 0x58, 0x50, 0x59,
                       // EXPY end offset
                       0x08, 0x00, 0x00, 0x00,
                       // Value
                       '1', '2', '3', '4', '5', '6', '7', '8'};

  string server_config(reinterpret_cast<const char*>(&scfg), sizeof(scfg));
  string source_address_token("test_source_address_token");
  string signature("test_signature");

  std::vector<string> certs;
  // Load a certificate that is valid for *.example.org
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
  DCHECK(cert);
  certs.emplace_back(x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()));

  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_handle =
      GetCryptoConfig(factory, network_anonymization_key);
  quic::QuicCryptoClientConfig::CachedState* cached =
      crypto_config_handle->GetConfig()->LookupOrCreate(quic_server_id);
  quic::QuicChromiumClock clock;
  cached->Initialize(server_config, source_address_token, certs, "", "",
                     signature, clock.WallNow(), quic::QuicWallTime::Zero());
  DCHECK(!cached->certs().empty());
}

int QuicStreamFactoryPeer::GetNumPushStreamsCreated(
    QuicStreamFactory* factory) {
  return factory->num_push_streams_created_;
}

size_t QuicStreamFactoryPeer::GetNumDegradingSessions(
    QuicStreamFactory* factory) {
  return factory->connectivity_monitor_.GetNumDegradingSessions();
}

void QuicStreamFactoryPeer::SetAlarmFactory(
    QuicStreamFactory* factory,
    std::unique_ptr<quic::QuicAlarmFactory> alarm_factory) {
  factory->alarm_factory_ = std::move(alarm_factory);
}

}  // namespace net::test
