// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_FACTORY_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_transport.h"

namespace blink {

class MockP2PQuicTransportFactory
    : public testing::NiceMock<P2PQuicTransportFactory> {
 public:
  MockP2PQuicTransportFactory(
      std::unique_ptr<MockP2PQuicTransport> mock_transport,
      P2PQuicTransport::Delegate** delegate_out = nullptr)
      : mock_transport_(std::move(mock_transport)),
        delegate_out_(delegate_out) {
    if (delegate_out) {
      // Ensure the caller has not left the delegate_out value floating.
      DCHECK_EQ(nullptr, *delegate_out);
    }
  }

  // P2PQuicTransportFactory overrides.
  std::unique_ptr<P2PQuicTransport> CreateQuicTransport(
      P2PQuicTransportConfig config) {
    DCHECK(mock_transport_);
    OnCreateQuicTransport(config);
    if (delegate_out_) {
      *delegate_out_ = config.delegate;
    }
    return std::move(mock_transport_);
  }

  MOCK_METHOD1(OnCreateQuicTransport, void(const P2PQuicTransportConfig&));

 private:
  std::unique_ptr<MockP2PQuicTransport> mock_transport_;
  P2PQuicTransport::Delegate** delegate_out_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_FACTORY_H_
