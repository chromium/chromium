// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_STREAM_FACTORY_PEER_H_
#define NET_QUIC_QUIC_STREAM_FACTORY_PEER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
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
class QuicStreamFactory;

namespace test {

class QuicStreamFactoryPeer {
 public:
  QuicStreamFactoryPeer(const QuicStreamFactoryPeer&) = delete;
  QuicStreamFactoryPeer& operator=(const QuicStreamFactoryPeer&) = delete;

  static const quic::QuicConfig* GetConfig(QuicStreamFactory* factory);

  static std::unique_ptr<QuicCryptoClientConfigHandle> GetCryptoConfig(
      QuicStreamFactory* factory,
      const NetworkAnonymizationKey& network_anonymization_key);

  static bool HasActiveSession(
      QuicStreamFactory* factory,
      const quic::QuicServerId& server_id,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey());

  static bool HasActiveJob(QuicStreamFactory* factory,
                           const quic::QuicServerId& server_id);

  static QuicChromiumClientSession* GetPendingSession(
      QuicStreamFactory* factory,
      const quic::QuicServerId& server_id,
      url::SchemeHostPort destination);

  static QuicChromiumClientSession* GetActiveSession(
      QuicStreamFactory* factory,
      const quic::QuicServerId& server_id,
      const NetworkAnonymizationKey& network_anonymization_key =
          NetworkAnonymizationKey());

  static bool HasLiveSession(QuicStreamFactory* factory,
                             url::SchemeHostPort destination,
                             const quic::QuicServerId& server_id);

  static bool IsLiveSession(QuicStreamFactory* factory,
                            QuicChromiumClientSession* session);

  static void SetTickClock(QuicStreamFactory* factory,
                           const base::TickClock* tick_clock);

  static void SetTaskRunner(QuicStreamFactory* factory,
                            base::SequencedTaskRunner* task_runner);

  static quic::QuicTime::Delta GetPingTimeout(QuicStreamFactory* factory);

  static void SetYieldAfterPackets(QuicStreamFactory* factory,
                                   int yield_after_packets);

  static void SetYieldAfterDuration(QuicStreamFactory* factory,
                                    quic::QuicTime::Delta yield_after_duration);

  static size_t GetNumberOfActiveJobs(QuicStreamFactory* factory,
                                      const quic::QuicServerId& server_id);

  static bool CryptoConfigCacheIsEmpty(
      QuicStreamFactory* factory,
      const quic::QuicServerId& quic_server_id,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Creates a dummy QUIC server config and caches it. Caller must be holding
  // onto a QuicCryptoClientConfigHandle for the corresponding
  // |network_anonymization_key|.
  static void CacheDummyServerConfig(
      QuicStreamFactory* factory,
      const quic::QuicServerId& quic_server_id,
      const NetworkAnonymizationKey& network_anonymization_key);

  static int GetNumPushStreamsCreated(QuicStreamFactory* factory);

  static size_t GetNumDegradingSessions(QuicStreamFactory* factory);

  static void SetAlarmFactory(
      QuicStreamFactory* factory,
      std::unique_ptr<quic::QuicAlarmFactory> alarm_factory);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_QUIC_STREAM_FACTORY_PEER_H_
