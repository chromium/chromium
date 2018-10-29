// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"

namespace blink {

class MockP2PQuicTransport : public testing::NiceMock<P2PQuicTransport> {
 public:
  ~MockP2PQuicTransport() override { Die(); }
  MOCK_METHOD0(Die, void());

  // P2PQuicTransport overrides.
  MOCK_METHOD0(Stop, void());
  void Start(std::vector<std::unique_ptr<rtc::SSLFingerprint>>
                 remote_fingerprints) override {
    MockStart(remote_fingerprints);
  }
  MOCK_METHOD1(MockStart,
               void(const std::vector<std::unique_ptr<rtc::SSLFingerprint>>&));
  MOCK_METHOD0(CreateStream, P2PQuicStream*());
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_P2P_QUIC_TRANSPORT_H_
