// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_stats.h"

namespace blink {

class MockP2PQuicTransport : public testing::NiceMock<P2PQuicTransport> {
 public:
  ~MockP2PQuicTransport() override { Die(); }
  MOCK_METHOD0(Die, void());

  // P2PQuicTransport overrides.
  MOCK_METHOD0(Stop, void());
  void Start(StartConfig config) override { MockStart(config); }
  MOCK_METHOD1(MockStart, void(const StartConfig&));
  MOCK_METHOD0(CreateStream, P2PQuicStream*());
  MOCK_CONST_METHOD0(GetStats, P2PQuicTransportStats());
  MOCK_METHOD1(SendDatagram, void(Vector<uint8_t>));
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_H_
