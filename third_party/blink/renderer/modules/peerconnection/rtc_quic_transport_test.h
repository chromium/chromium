// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LQUICNSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_TEST_H_

#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_transport_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport_test.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"

namespace blink {

class RTCQuicTransportTest : public RTCIceTransportTest {
 public:
  // Construct a new RTCQuicTransport with the given RTCIceTransport,
  // certificates, and mock P2PQuicTransport.
  // |delegate_out|, if non-null, will be populated once the P2PQuicTransport is
  // constructed on the worker thread.
  RTCQuicTransport* CreateQuicTransport(
      V8TestingScope& scope,
      RTCIceTransport* ice_transport,
      const HeapVector<Member<RTCCertificate>>& certificates,
      std::unique_ptr<MockP2PQuicTransport> mock_transport,
      P2PQuicTransport::Delegate** delegate_out = nullptr);

  // Construct a new RTCQuicTransport with the given RTCIceTransport,
  // certificates, and mock P2PQuicTransportFactory.
  RTCQuicTransport* CreateQuicTransport(
      V8TestingScope& scope,
      RTCIceTransport* ice_transport,
      const HeapVector<Member<RTCCertificate>>& certificates,
      std::unique_ptr<MockP2PQuicTransportFactory> mock_factory);

  // Construct a new RTCQuicTransport and RTCIceTransport and call start() on
  // both objects.
  RTCQuicTransport* CreateConnectedQuicTransport(
      V8TestingScope& scope,
      P2PQuicTransport::Delegate** delegate_out = nullptr);
  RTCQuicTransport* CreateConnectedQuicTransport(
      V8TestingScope& scope,
      std::unique_ptr<MockP2PQuicTransport> mock_transport,
      P2PQuicTransport::Delegate** delegate_out = nullptr);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_TEST_H_
