// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_PEER_H_
#define NET_QUIC_QUIC_SESSION_POOL_PEER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/session_usage.h"
#include "net/quic/quic_session_key.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"
#include "url/scheme_host_port.h"

namespace quic {
class QuicAlarmFactory;
class QuicConfig;
}  // namespace quic

namespace net {

class NetLogWithSource;
class QuicChromiumClientSession;
class QuicCryptoClientConfigHandle;
class QuicSessionPool;

namespace test {

class QuicSessionPoolPeer {
 public:
  QuicSessionPoolPeer(const QuicSessionPoolPeer&) = delete;
  QuicSessionPoolPeer& operator=(const QuicSessionPoolPeer&) = delete;

  static const quic::QuicConfig* GetConfig(QuicSessionPool* factory);

  static std::unique_ptr<QuicCryptoClientConfigHandle> GetCryptoConfig(
      QuicSessionPool* factory,
      const NetworkAnonymizationKey& network_anonymization_key);

  static bool HasActiveSession(
      QuicSessionPool* factory,
      const quic::QuicServerId& server_id,
      PrivacyMode privacy_mode,
      const NetworkAnonymizationKey& network_anonymization_key,
      const ProxyChain& proxy_chain = ProxyChain::Direct(),
      SessionUsage session_usage = SessionUsage::kDestination,
      bool require_dns_https_alpn = false);

  static bool HasActiveJob(QuicSessionPool* factory,
                           const quic::QuicServerId& server_id,
                           PrivacyMode privacy_mode,
                           bool require_dns_https_alpn = false);

  static QuicChromiumClientSession* GetPendingSession(
      QuicSessionPool* factory,
      const quic::QuicServerId& server_id,
      PrivacyMode privacy_mode,
      url::SchemeHostPort destination);

  static QuicChromiumClientSession* GetActiveSession(
      QuicSessionPool* factory,
      const quic::QuicServerId& server_id,
      PrivacyMode privacy_mode,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey(),
      const ProxyChain& proxy_chain = ProxyChain::Direct(),
      SessionUsage session_usage = SessionUsage::kDestination,
      bool require_dns_https_alpn = false);

  static bool IsLiveSession(QuicSessionPool* factory,
                            QuicChromiumClientSession* session);

  static void SetTickClock(QuicSessionPool* factory,
                           const base::TickClock* tick_clock);

  static void SetTaskRunner(QuicSessionPool* factory,
                            base::SequencedTaskRunner* task_runner);

  static quic::QuicTime::Delta GetPingTimeout(QuicSessionPool* factory);

  static void SetYieldAfterPackets(QuicSessionPool* factory,
                                   int yield_after_packets);

  static void SetYieldAfterDuration(QuicSessionPool* factory,
                                    quic::QuicTime::Delta yield_after_duration);

  static bool CryptoConfigCacheIsEmpty(
      QuicSessionPool* factory,
      const quic::QuicServerId& quic_server_id,
      const NetworkAnonymizationKey& network_anonymization_key);

  static size_t GetNumDegradingSessions(QuicSessionPool* factory);

  static void SetAlarmFactory(
      QuicSessionPool* factory,
      std::unique_ptr<quic::QuicAlarmFactory> alarm_factory);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_PEER_H_
