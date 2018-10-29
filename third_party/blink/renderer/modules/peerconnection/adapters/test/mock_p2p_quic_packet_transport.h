// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_PACKET_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_PACKET_TRANSPORT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"

namespace blink {

class MockP2PQuicPacketTransport : public P2PQuicPacketTransport {
 public:
  // P2PQuicPacketTransport overrides.
  MOCK_METHOD1(WritePacket, int(const QuicPacket&));
  MOCK_METHOD1(SetReceiveDelegate, void(ReceiveDelegate*));
  MOCK_METHOD1(SetWriteObserver, void(WriteObserver*));
  MOCK_METHOD0(Writable, bool());
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_PACKET_TRANSPORT_H_
