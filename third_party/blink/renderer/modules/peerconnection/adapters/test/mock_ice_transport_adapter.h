// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_ICE_TRANSPORT_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_ICE_TRANSPORT_ADAPTER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"

namespace blink {

class MockIceTransportAdapter : public testing::NiceMock<IceTransportAdapter> {
 public:
  MockIceTransportAdapter() : MockIceTransportAdapter(nullptr) {}
  MockIceTransportAdapter(
      std::unique_ptr<P2PQuicPacketTransport> packet_transport)
      : packet_transport_(std::move(packet_transport)) {
    ON_CALL(*this, packet_transport()).WillByDefault(testing::Invoke([this] {
      return packet_transport_.get();
    }));
  }
  ~MockIceTransportAdapter() override { Die(); }
  MOCK_METHOD0(Die, void());

  // IceTransportAdapter overrides.
  MOCK_METHOD4(StartGathering,
               void(const cricket::IceParameters&,
                    const cricket::ServerAddresses&,
                    const WebVector<cricket::RelayServerConfig>&,
                    IceTransportPolicy));
  MOCK_METHOD3(Start,
               void(const cricket::IceParameters&,
                    cricket::IceRole,
                    const Vector<cricket::Candidate>&));
  MOCK_METHOD1(HandleRemoteRestart, void(const cricket::IceParameters&));
  MOCK_METHOD1(AddRemoteCandidate, void(const cricket::Candidate&));
  MOCK_CONST_METHOD0(packet_transport, P2PQuicPacketTransport*());

 private:
  std::unique_ptr<P2PQuicPacketTransport> packet_transport_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_ICE_TRANSPORT_ADAPTER_H_
