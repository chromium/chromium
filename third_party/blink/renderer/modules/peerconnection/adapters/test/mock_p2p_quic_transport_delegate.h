// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_DELEGATE_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"

namespace blink {

class MockP2PQuicTransportDelegate
    : public testing::NiceMock<P2PQuicTransport::Delegate> {
 public:
  // P2PQuicTransport::Delegate overrides.
  MOCK_METHOD0(OnRemoteStopped, void());
  MOCK_METHOD2(OnConnectionFailed, void(const std::string&, bool));
  MOCK_METHOD1(OnConnected, void(P2PQuicNegotiatedParams));
  MOCK_METHOD1(OnStream, void(P2PQuicStream*));
  MOCK_METHOD1(OnDatagramReceived, void(Vector<uint8_t> datagram));
  MOCK_METHOD0(OnDatagramSent, void());
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_DELEGATE_H_
