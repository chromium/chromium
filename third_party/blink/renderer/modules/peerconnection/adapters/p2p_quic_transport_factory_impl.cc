// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory_impl.h"
#include "net/third_party/quiche/src/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_writer.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_config_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_stream_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_impl.h"
#include "third_party/webrtc/rtc_base/rtc_certificate.h"

namespace blink {

P2PQuicTransportFactoryImpl::P2PQuicTransportFactoryImpl(
    quic::QuicClock* clock,
    std::unique_ptr<quic::QuicAlarmFactory> alarm_factory)
    : clock_(clock), alarm_factory_(std::move(alarm_factory)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// The P2PQuicTransportImpl is created with Chromium specific QUIC objects:
// QuicClock, QuicRandom, QuicConnectionHelper and QuicAlarmFactory.
std::unique_ptr<P2PQuicTransport>
P2PQuicTransportFactoryImpl::CreateQuicTransport(
    P2PQuicTransport::Delegate* delegate,
    P2PQuicPacketTransport* packet_transport,
    const P2PQuicTransportConfig& config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate);
  DCHECK(packet_transport);

  quic::QuicRandom* quic_random = quic::QuicRandom::GetInstance();
  return P2PQuicTransportImpl::Create(
      clock_, alarm_factory_.get(), quic_random, delegate, packet_transport,
      std::move(config),
      std::make_unique<P2PQuicCryptoConfigFactoryImpl>(quic_random),
      std::make_unique<P2PQuicCryptoStreamFactoryImpl>());
}
}  // namespace blink
