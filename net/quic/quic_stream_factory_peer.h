// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_FACTORY_PEER_H_
#define NET_QUIC_QUIC_STREAM_FACTORY_PEER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"

namespace quic {
class QuicAlarmFactory;
class QuicClientPushPromiseIndex;
class QuicConfig;
}  // namespace quic

namespace net {

class NetLogWithSource;
class QuicChromiumClientSession;
class QuicCryptoClientConfigHandle;
class QuicStreamFactory;

namespace test {

class QuicStreamFactoryPeer {
 public:
  static const quic::QuicConfig* GetConfig(QuicStreamFactory* factory);

  static std::unique_ptr<QuicCryptoClientConfigHandle> GetCryptoConfig(
      QuicStreamFactory* factory,
      const NetworkIsolationKey& network_isolation_key);

  static bool HasActiveSession(
      QuicStreamFactory* factory,
      const quic::QuicServerId& server_id,
      const NetworkIsolationKey& network_isolation_key = NetworkIsolationKey());

  static bool HasActiveJob(QuicStreamFactory* factory,
                           const quic::QuicServerId& server_id);

  static bool HasActiveCertVerifierJob(QuicStreamFactory* factory,
                                       const quic::QuicServerId& server_id);

  static QuicChromiumClientSession* GetPendingSession(
      QuicStreamFactory* factory,
      const quic::QuicServerId& server_id,
      const HostPortPair& destination);

  static QuicChromiumClientSession* GetActiveSession(
      QuicStreamFactory* factory,
      const quic::QuicServerId& server_id,
      const NetworkIsolationKey& network_isolation_key = NetworkIsolationKey());

  static bool HasLiveSession(QuicStreamFactory* factory,
                             const HostPortPair& destination,
                             const quic::QuicServerId& server_id);

  static bool IsLiveSession(QuicStreamFactory* factory,
                            QuicChromiumClientSession* session);

  static void SetTickClock(QuicStreamFactory* factory,
                           const base::TickClock* tick_clock);

  static void SetTaskRunner(QuicStreamFactory* factory,
                            base::SequencedTaskRunner* task_runner);

  static quic::QuicTime::Delta GetPingTimeout(QuicStreamFactory* factory);

  static bool GetRaceCertVerification(QuicStreamFactory* factory);

  static void SetRaceCertVerification(QuicStreamFactory* factory,
                                      bool race_cert_verification);

  // When using this method, the caller should be holding onto a live
  // NetworkIsolationKey, if it wants the results to stay alive in the
  // per-NetworkIsolationKey cache.
  static quic::QuicAsyncStatus StartCertVerifyJob(
      QuicStreamFactory* factory,
      const quic::QuicServerId& server_id,
      const NetworkIsolationKey& network_isolation_key,
      int cert_verify_flags,
      const NetLogWithSource& net_log);

  static void SetYieldAfterPackets(QuicStreamFactory* factory,
                                   int yield_after_packets);

  static void SetYieldAfterDuration(QuicStreamFactory* factory,
                                    quic::QuicTime::Delta yield_after_duration);

  static size_t GetNumberOfActiveJobs(QuicStreamFactory* factory,
                                      const quic::QuicServerId& server_id);

  static bool CryptoConfigCacheIsEmpty(
      QuicStreamFactory* factory,
      const quic::QuicServerId& quic_server_id,
      const NetworkIsolationKey& network_isolation_key);

  // Creates a dummy QUIC server config and caches it. Caller must be holding
  // onto a QuicCryptoClientConfigHandle for the corresponding
  // |network_isolation_key|.
  static void CacheDummyServerConfig(
      QuicStreamFactory* factory,
      const quic::QuicServerId& quic_server_id,
      const NetworkIsolationKey& network_isolation_key);

  static quic::QuicClientPushPromiseIndex* GetPushPromiseIndex(
      QuicStreamFactory* factory);

  static int GetNumPushStreamsCreated(QuicStreamFactory* factory);

  static void SetAlarmFactory(
      QuicStreamFactory* factory,
      std::unique_ptr<quic::QuicAlarmFactory> alarm_factory);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicStreamFactoryPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_FACTORY_PEER_H_
