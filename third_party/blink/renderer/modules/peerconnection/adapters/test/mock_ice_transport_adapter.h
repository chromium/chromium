// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_ICE_TRANSPORT_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_ICE_TRANSPORT_ADAPTER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter.h"

namespace blink {

class MockIceTransportAdapter : public testing::NiceMock<IceTransportAdapter> {
 public:
  MockIceTransportAdapter() = default;
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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_TEST_MOCK_ICE_TRANSPORT_ADAPTER_H_
