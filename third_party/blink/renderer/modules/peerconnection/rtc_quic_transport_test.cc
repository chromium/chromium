// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the RTCQuicTransport Blink bindings, QuicTransportProxy and
// QuicTransportHost by mocking out the underlying P2PQuicTransport.
// Everything is run on a single thread but with separate TestSimpleTaskRunners
// for the main thread / worker thread.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport_test.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_gather_options.h"
#include "third_party/webrtc/rtc_base/rtccertificategenerator.h"

namespace blink {
namespace {

using testing::_;
using testing::Assign;
using testing::ElementsAre;
using testing::Invoke;
using testing::Mock;

HeapVector<Member<RTCCertificate>> GenerateLocalRTCCertificates() {
  HeapVector<Member<RTCCertificate>> certificates;
  certificates.push_back(
      new RTCCertificate(rtc::RTCCertificateGenerator::GenerateCertificate(
          rtc::KeyParams::ECDSA(), absl::nullopt)));
  return certificates;
}

constexpr char kRemoteFingerprintAlgorithm1[] = "sha-256";
constexpr char kRemoteFingerprintValue1[] =
    "8E:57:5F:8E:65:D2:83:7B:05:97:BB:72:DE:09:DE:03:BD:95:9B:A0:03:10:50:82:"
    "5E:73:38:16:4C:E0:C5:84";

RTCDtlsFingerprint CreateRemoteFingerprint1() {
  RTCDtlsFingerprint dtls_fingerprint;
  dtls_fingerprint.setAlgorithm(kRemoteFingerprintAlgorithm1);
  dtls_fingerprint.setValue(kRemoteFingerprintValue1);
  return dtls_fingerprint;
}

RTCQuicParameters CreateRemoteRTCQuicParameters1() {
  HeapVector<RTCDtlsFingerprint> fingerprints;
  fingerprints.push_back(CreateRemoteFingerprint1());
  RTCQuicParameters quic_parameters;
  quic_parameters.setFingerprints(fingerprints);
  return quic_parameters;
}

}  // namespace

RTCQuicTransport* RTCQuicTransportTest::CreateQuicTransport(
    V8TestingScope& scope,
    RTCIceTransport* ice_transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    std::unique_ptr<MockP2PQuicTransport> mock_transport,
    P2PQuicTransport::Delegate** delegate_out) {
  return CreateQuicTransport(scope, ice_transport, certificates,
                             std::make_unique<MockP2PQuicTransportFactory>(
                                 std::move(mock_transport), delegate_out));
}

RTCQuicTransport* RTCQuicTransportTest::CreateQuicTransport(
    V8TestingScope& scope,
    RTCIceTransport* ice_transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    std::unique_ptr<MockP2PQuicTransportFactory> mock_factory) {
  return RTCQuicTransport::Create(scope.GetExecutionContext(), ice_transport,
                                  certificates, ASSERT_NO_EXCEPTION,
                                  std::move(mock_factory));
}

RTCQuicTransport* RTCQuicTransportTest::CreateConnectedQuicTransport(
    V8TestingScope& scope,
    P2PQuicTransport::Delegate** delegate_out) {
  return CreateConnectedQuicTransport(
      scope, std::make_unique<MockP2PQuicTransport>(), delegate_out);
}

RTCQuicTransport* RTCQuicTransportTest::CreateConnectedQuicTransport(
    V8TestingScope& scope,
    std::unique_ptr<MockP2PQuicTransport> mock_transport,
    P2PQuicTransport::Delegate** delegate_out) {
  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport), &delegate);
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  DCHECK(delegate);
  delegate->OnConnected();
  RunUntilIdle();
  DCHECK_EQ("connected", quic_transport->state());
  if (delegate_out) {
    *delegate_out = delegate;
  }
  return quic_transport;
}

// Test that calling start() creates a P2PQuicTransport with the correct
// P2PQuicTransportConfig. The config should have:
// 1. The P2PQuicPacketTransport returned by the MockIceTransportAdapter.
// 2. Server mode configured since the ICE role is 'controlling'.
// 3. The certificates passed in the RTCQuicTransport constructor.
TEST_F(RTCQuicTransportTest, P2PQuicTransportConstructedByStart) {
  V8TestingScope scope;

  auto quic_packet_transport = std::make_unique<MockP2PQuicPacketTransport>();
  auto* quic_packet_transport_ptr = quic_packet_transport.get();
  auto ice_transport_adapter_mock = std::make_unique<MockIceTransportAdapter>(
      std::move(quic_packet_transport));
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(ice_transport_adapter_mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificateGenerator::GenerateCertificate(rtc::KeyParams::ECDSA(),
                                                        absl::nullopt);
  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>(
      std::make_unique<MockP2PQuicTransport>());
  EXPECT_CALL(*mock_factory, OnCreateQuicTransport(_))
      .WillOnce(Invoke([quic_packet_transport_ptr,
                        certificate](const P2PQuicTransportConfig& config) {
        EXPECT_EQ(quic_packet_transport_ptr, config.packet_transport);
        EXPECT_TRUE(config.is_server);
        EXPECT_THAT(config.certificates, ElementsAre(certificate));
      }));
  HeapVector<Member<RTCCertificate>> certificates;
  certificates.push_back(new RTCCertificate(certificate));
  Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
      scope, ice_transport, certificates, std::move(mock_factory));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
}

// Test that calling start() creates a P2PQuicTransport with
// |config.is_server| = false if the RTCIceTransport role is 'controlled'.
TEST_F(RTCQuicTransportTest, P2PQuicTransportConstructedByStartClient) {
  V8TestingScope scope;

  auto ice_transport_adapter_mock = std::make_unique<MockIceTransportAdapter>(
      std::make_unique<MockP2PQuicPacketTransport>());
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(ice_transport_adapter_mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlled",
                       ASSERT_NO_EXCEPTION);

  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>(
      std::make_unique<MockP2PQuicTransport>());
  EXPECT_CALL(*mock_factory, OnCreateQuicTransport(_))
      .WillOnce(Invoke([](const P2PQuicTransportConfig& config) {
        EXPECT_FALSE(config.is_server);
      }));
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_factory));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
}

// Test that calling start() calls Start() on the P2PQuicTransport with the
// correct remote fingerprints.
TEST_F(RTCQuicTransportTest, StartPassesRemoteFingerprints) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, MockStart(_))
      .WillOnce(
          Invoke([](const std::vector<std::unique_ptr<rtc::SSLFingerprint>>&
                        remote_fingerprints) {
            ASSERT_EQ(1u, remote_fingerprints.size());
            EXPECT_EQ(kRemoteFingerprintAlgorithm1,
                      remote_fingerprints[0]->algorithm);
            EXPECT_EQ(kRemoteFingerprintValue1,
                      remote_fingerprints[0]->GetRfc4572Fingerprint());
          }));
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
}

// Test that calling stop() deletes the underlying P2PQuicTransport.
TEST_F(RTCQuicTransportTest, StopCallsStopThenDeletesQuicTransportAdapter) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  bool mock_deleted = false;
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, Stop()).Times(1);
  EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);

  quic_transport->stop();
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that calling stop() on the underlying RTCIceTransport deletes the
// underlying P2PQuicTransport.
TEST_F(RTCQuicTransportTest, RTCIceTransportStopDeletesP2PQuicTransport) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  bool mock_deleted = false;
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);

  ice_transport->stop();
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that the P2PQuicTransport is deleted when the underlying RTCIceTransport
// is ContextDestroyed.
TEST_F(RTCQuicTransportTest,
       RTCIceTransportContextDestroyedDeletesP2PQuicTransport) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  bool mock_deleted = false;
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);

  ice_transport->ContextDestroyed(scope.GetExecutionContext());
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

}  // namespace blink
