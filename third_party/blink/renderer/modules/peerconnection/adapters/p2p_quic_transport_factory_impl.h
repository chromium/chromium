// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_IMPL_H_

#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_impl.h"

namespace blink {

// For creating a P2PQuicTransportImpl to be used for the blink Web API -
// RTCQuicTransport.
//
// This object should be run entirely on the webrtc worker thread.
class MODULES_EXPORT P2PQuicTransportFactoryImpl final
    : public P2PQuicTransportFactory {
 public:
  P2PQuicTransportFactoryImpl(
      quic::QuicClock* clock,
      std::unique_ptr<quic::QuicAlarmFactory> alarm_factory);
  ~P2PQuicTransportFactoryImpl() override {}

  // QuicTransportFactoryInterface override.
  std::unique_ptr<P2PQuicTransport> CreateQuicTransport(
      P2PQuicTransport::Delegate* delegate,
      P2PQuicPacketTransport* packet_transport,
      const P2PQuicTransportConfig& config) override;

 private:
  // This is used to create a QuicChromiumConnectionHelper for the session.
  // Should outlive the P2PQuicTransportFactoryImpl.
  quic::QuicClock* clock_;
  // Used for alarms that drive the underlying QUIC library. Should use the same
  // clock as |clock_|.
  std::unique_ptr<quic::QuicAlarmFactory> alarm_factory_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_IMPL_H_
