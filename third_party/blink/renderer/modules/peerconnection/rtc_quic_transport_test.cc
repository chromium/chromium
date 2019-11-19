// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the RTCQuicTransport Blink bindings, QuicTransportProxy and
// QuicTransportHost by mocking out the underlying P2PQuicTransport.
// Everything is run on a single thread but with separate TestSimpleTaskRunners
// for the main thread / worker thread.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport_test.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_quic_transport_stats.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_stats.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_gather_options.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/webrtc/rtc_base/rtc_certificate_generator.h"

namespace blink {
namespace {

using testing::_;
using testing::Assign;
using testing::ElementsAre;
using testing::Invoke;
using testing::Mock;
using testing::Return;

HeapVector<Member<RTCCertificate>> GenerateLocalRTCCertificates() {
  HeapVector<Member<RTCCertificate>> certificates;
  certificates.push_back(MakeGarbageCollected<RTCCertificate>(
      rtc::RTCCertificateGenerator::GenerateCertificate(rtc::KeyParams::ECDSA(),
                                                        absl::nullopt)));
  return certificates;
}

constexpr char kRemoteFingerprintAlgorithm1[] = "sha-256";
constexpr char kRemoteFingerprintValue1[] =
    "8E:57:5F:8E:65:D2:83:7B:05:97:BB:72:DE:09:DE:03:BD:95:9B:A0:03:10:50:82:"
    "5E:73:38:16:4C:E0:C5:84";
const size_t kKeyLength = 16;
const uint8_t kKey[kKeyLength] = {0, 1, 2,  3,  4,  5,  6,  7,
                                  8, 9, 10, 11, 12, 13, 14, 15};
// Arbitrary datagram.
const size_t kDatagramLength = 4;
const uint8_t kDatagram[kDatagramLength] = {0, 1, 2, 3};
const uint16_t kMaxDatagramLengthBytes = 1000;

RTCDtlsFingerprint* CreateRemoteFingerprint1() {
  RTCDtlsFingerprint* dtls_fingerprint = RTCDtlsFingerprint::Create();
  dtls_fingerprint->setAlgorithm(kRemoteFingerprintAlgorithm1);
  dtls_fingerprint->setValue(kRemoteFingerprintValue1);
  return dtls_fingerprint;
}

RTCQuicParameters* CreateRemoteRTCQuicParameters1() {
  HeapVector<Member<RTCDtlsFingerprint>> fingerprints;
  fingerprints.push_back(CreateRemoteFingerprint1());
  RTCQuicParameters* quic_parameters = RTCQuicParameters::Create();
  quic_parameters->setFingerprints(fingerprints);
  return quic_parameters;
}

// Sends datagrams without getting callbacks that they have been sent on the
// network until the buffer becomes full.
void FillDatagramBuffer(RTCQuicTransport* transport) {
  for (size_t i = 0; i < kMaxBufferedSendDatagrams; ++i) {
    transport->sendDatagram(DOMArrayBuffer::Create(kDatagram, kDatagramLength),
                            ASSERT_NO_EXCEPTION);
  }
}

static base::span<uint8_t> SpanFromDOMArrayBuffer(DOMArrayBuffer* buffer) {
  return base::span<uint8_t>(static_cast<uint8_t*>(buffer->Data()),
                             buffer->ByteLengthAsSizeT());
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
  P2PQuicNegotiatedParams params;
  params.set_max_datagram_length(kMaxDatagramLengthBytes);
  delegate->OnConnected(params);
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
  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>();
  EXPECT_CALL(*mock_factory, CreateQuicTransport(_, _, _))
      .WillOnce(Invoke([quic_packet_transport_ptr, certificate](
                           P2PQuicTransport::Delegate* delegate,
                           P2PQuicPacketTransport* packet_transport,
                           const P2PQuicTransportConfig& config) {
        EXPECT_EQ(quic_packet_transport_ptr, packet_transport);
        EXPECT_EQ(quic::Perspective::IS_SERVER, config.perspective);
        EXPECT_THAT(config.certificates, ElementsAre(certificate));
        return std::make_unique<MockP2PQuicTransport>();
      }));
  HeapVector<Member<RTCCertificate>> certificates;
  certificates.push_back(MakeGarbageCollected<RTCCertificate>(certificate));
  Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
      scope, ice_transport, certificates, std::move(mock_factory));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
}

// Test that calling connect() creates a P2PQuicTransport with the correct
// P2PQuicTransportConfig. The config should have:
// 1. The P2PQuicPacketTransport returned by the MockIceTransportAdapter.
// 2. Client mode configured.
TEST_F(RTCQuicTransportTest, P2PQuicTransportConstructedByConnect) {
  V8TestingScope scope;

  auto quic_packet_transport = std::make_unique<MockP2PQuicPacketTransport>();
  auto* quic_packet_transport_ptr = quic_packet_transport.get();
  auto ice_transport_adapter_mock = std::make_unique<MockIceTransportAdapter>(
      std::move(quic_packet_transport));
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(ice_transport_adapter_mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>();
  EXPECT_CALL(*mock_factory, CreateQuicTransport(_, _, _))
      .WillOnce(Invoke(
          [quic_packet_transport_ptr](P2PQuicTransport::Delegate* delegate,
                                      P2PQuicPacketTransport* packet_transport,
                                      const P2PQuicTransportConfig& config) {
            EXPECT_EQ(quic_packet_transport_ptr, packet_transport);
            EXPECT_EQ(quic::Perspective::IS_CLIENT, config.perspective);
            return std::make_unique<MockP2PQuicTransport>();
          }));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_factory));
  quic_transport->connect(ASSERT_NO_EXCEPTION);
}

// Test that calling listen() creates a P2PQuicTransport with the correct
// P2PQuicTransportConfig. The config should have:
// 1. The P2PQuicPacketTransport returned by the MockIceTransportAdapter.
// 2. Server mode configured.
TEST_F(RTCQuicTransportTest, P2PQuicTransportConstructedByListen) {
  V8TestingScope scope;

  auto quic_packet_transport = std::make_unique<MockP2PQuicPacketTransport>();
  auto* quic_packet_transport_ptr = quic_packet_transport.get();
  auto ice_transport_adapter_mock = std::make_unique<MockIceTransportAdapter>(
      std::move(quic_packet_transport));
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(ice_transport_adapter_mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>();
  EXPECT_CALL(*mock_factory, CreateQuicTransport(_, _, _))
      .WillOnce(Invoke(
          [quic_packet_transport_ptr](P2PQuicTransport::Delegate* delegate,
                                      P2PQuicPacketTransport* packet_transport,
                                      const P2PQuicTransportConfig& config) {
            EXPECT_EQ(quic_packet_transport_ptr, packet_transport);
            EXPECT_EQ(quic::Perspective::IS_SERVER, config.perspective);
            return std::make_unique<MockP2PQuicTransport>();
          }));

  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_factory));
  quic_transport->listen(DOMArrayBuffer::Create(kKey, kKeyLength),
                         ASSERT_NO_EXCEPTION);
}

// Test that calling start() creates a P2PQuicTransport with client perspective
// if the RTCIceTransport role is 'controlled'.
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
  EXPECT_CALL(*mock_factory, CreateQuicTransport(_, _, _))
      .WillOnce(Invoke([](P2PQuicTransport::Delegate* delegate,
                          P2PQuicPacketTransport* packet_transport,
                          const P2PQuicTransportConfig& config) {
        EXPECT_EQ(quic::Perspective::IS_CLIENT, config.perspective);
        return std::make_unique<MockP2PQuicTransport>();
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
      .WillOnce(Invoke([](const P2PQuicTransport::StartConfig& config) {
        ASSERT_EQ(1u, config.remote_fingerprints.size());
        EXPECT_EQ(kRemoteFingerprintAlgorithm1,
                  config.remote_fingerprints[0]->algorithm);
        EXPECT_EQ(kRemoteFingerprintValue1,
                  config.remote_fingerprints[0]->GetRfc4572Fingerprint());
      }));
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
}

// Test that calling start() with a started RTCIceTransport changes its state to
// connecting.
TEST_F(RTCQuicTransportTest, StartWithConnectedTransportChangesToConnecting) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ("connecting", quic_transport->state());
}

// Test that calling start() changes its state to connecting once
// RTCIceTransport starts.
TEST_F(RTCQuicTransportTest, StartChangesToConnectingWhenIceStarts) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ("new", quic_transport->state());

  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  EXPECT_EQ("connecting", quic_transport->state());
}

// Test that calling start() twice throws a kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartTwiceThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);

  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling start() after connect() throws a kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartAfterConnectThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->connect(ASSERT_NO_EXCEPTION);

  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling start() after listen() throws a kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartAfterListenThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));
  quic_transport->listen(DOMArrayBuffer::Create(kKey, kKeyLength),
                         ASSERT_NO_EXCEPTION);

  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling start() after stop() throws a kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartAfterStopThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));

  quic_transport->stop();
  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling start() after RTCIceTransport::stop() throws a
// kInvalidStateError.
TEST_F(RTCQuicTransportTest, StartAfterIceStopsThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport));

  ice_transport->stop();
  quic_transport->start(CreateRemoteRTCQuicParameters1(),
                        scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling connect() calls Start() on the P2PQuicTransport with the
// generated pre shared key from the local side.
TEST_F(RTCQuicTransportTest, ConnectPassesPreSharedKey) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  auto* mock_transport_ptr = mock_transport.get();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_transport));
  DOMArrayBuffer* key = quic_transport->getKey();
  std::string pre_shared_key(static_cast<const char*>(key->Data()),
                             key->ByteLengthAsSizeT());

  EXPECT_CALL(*mock_transport_ptr, MockStart(_))
      .WillOnce(
          Invoke([pre_shared_key](const P2PQuicTransport::StartConfig& config) {
            EXPECT_EQ(pre_shared_key, config.pre_shared_key);
          }));
  quic_transport->connect(ASSERT_NO_EXCEPTION);
}

// Test that calling listen() calls Start() on the P2PQuicTransport with the
// correct given pre shared key from the remote side.
TEST_F(RTCQuicTransportTest, ListenPassesPreSharedKey) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  auto* mock_transport_ptr = mock_transport.get();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_transport));

  std::string pre_shared_key = "foobar";
  EXPECT_CALL(*mock_transport_ptr, MockStart(_))
      .WillOnce(
          Invoke([pre_shared_key](const P2PQuicTransport::StartConfig& config) {
            EXPECT_EQ(pre_shared_key, config.pre_shared_key);
          }));

  quic_transport->listen(
      DOMArrayBuffer::Create(pre_shared_key.c_str(), pre_shared_key.length()),
      ASSERT_NO_EXCEPTION);
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

// Test that the P2PQuicTransport is deleted and the RTCQuicTransport goes to
// the "failed" state when the QUIC connection fails.
TEST_F(RTCQuicTransportTest,
       ConnectionFailedBecomesClosedAndDeletesP2PQuicTransport) {
  V8TestingScope scope;

  bool mock_deleted = false;
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(mock_transport), &delegate);
  DCHECK(delegate);
  delegate->OnConnectionFailed("test_failure", /*from_remote=*/false);
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
  EXPECT_EQ("failed", quic_transport->state());
}

// Test that after the connection fails, stop() will change the state
// of the transport to "closed".
TEST_F(RTCQuicTransportTest, StopAfterConnectionFailed) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport = CreateConnectedQuicTransport(
      scope, std::make_unique<MockP2PQuicTransport>(), &delegate);
  DCHECK(delegate);
  delegate->OnConnectionFailed("test_failure", /*from_remote=*/false);
  RunUntilIdle();

  EXPECT_EQ("failed", quic_transport->state());

  quic_transport->stop();
  EXPECT_EQ("closed", quic_transport->state());
}

// Test that the P2PQuicTransport is deleted when the underlying RTCIceTransport
// is ContextDestroyed.
TEST_F(RTCQuicTransportTest,
       RTCIceTransportContextDestroyedDeletesP2PQuicTransport) {
  bool mock_deleted = false;
  {
    V8TestingScope scope;

    Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
    ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                         ASSERT_NO_EXCEPTION);

    auto mock_transport = std::make_unique<MockP2PQuicTransport>();
    EXPECT_CALL(*mock_transport, Die()).WillOnce(Assign(&mock_deleted, true));

    Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
        scope, ice_transport, GenerateLocalRTCCertificates(),
        std::move(mock_transport));
    quic_transport->start(CreateRemoteRTCQuicParameters1(),
                          ASSERT_NO_EXCEPTION);
  }  // ContextDestroyed when V8TestingScope goes out of scope.

  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that the certificate passed to RTCQuicTransport is the same
// returned by getCertificates().
TEST_F(RTCQuicTransportTest, GetCertificatesReturnsGivenCertificates) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  auto certificates = GenerateLocalRTCCertificates();
  Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
      scope, ice_transport, certificates, std::move(mock_transport));
  auto returned_certificates = quic_transport->getCertificates();

  EXPECT_EQ(certificates[0], returned_certificates[0]);
}

// Test that the fingerprint returned by getLocalParameters() is
// the fingerprint of the certificate passed to the RTCQuicTransport.
TEST_F(RTCQuicTransportTest,
       GetLocalParametersReturnsGivenCertificatesFingerprints) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  auto certificates = GenerateLocalRTCCertificates();
  auto fingerprints = certificates[0]->getFingerprints();
  Persistent<RTCQuicTransport> quic_transport = CreateQuicTransport(
      scope, ice_transport, certificates, std::move(mock_transport));
  auto returned_fingerprints =
      quic_transport->getLocalParameters()->fingerprints();

  EXPECT_EQ(1u, returned_fingerprints.size());
  EXPECT_EQ(fingerprints.size(), returned_fingerprints.size());
  EXPECT_EQ(fingerprints[0]->value(), returned_fingerprints[0]->value());
  EXPECT_EQ(fingerprints[0]->algorithm(),
            returned_fingerprints[0]->algorithm());
}

TEST_F(RTCQuicTransportTest, ExpiredCertificateThrowsError) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  auto mock_factory = std::make_unique<MockP2PQuicTransportFactory>();
  HeapVector<Member<RTCCertificate>> certificates;
  certificates.push_back(MakeGarbageCollected<RTCCertificate>(
      rtc::RTCCertificateGenerator::GenerateCertificate(rtc::KeyParams::ECDSA(),
                                                        /*expires_ms=*/0)));
  RTCQuicTransport::Create(scope.GetExecutionContext(), ice_transport,
                           certificates, scope.GetExceptionState(),
                           std::move(mock_factory));
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

// Test that the key returned has at least 128 bits of entropy as required by
// QUIC.
TEST_F(RTCQuicTransportTest, GetKeyReturnsValidKey) {
  V8TestingScope scope;

  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, {}, std::move(mock_transport));
  auto* key = quic_transport->getKey();

  EXPECT_GE(key->ByteLengthAsSizeT(), 16u);
}

// Test that stats are converted correctly to the RTCQuicTransportStats
// dictionary.
TEST_F(RTCQuicTransportTest, OnStatsConvertsRTCStatsDictionary) {
  V8TestingScope scope;

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  MockP2PQuicTransport* mock_transport_ptr = mock_transport.get();
  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(mock_transport), &delegate);
  DCHECK(delegate);

  // Create some dummy values.
  P2PQuicTransportStats stats;
  stats.bytes_sent = 0;
  stats.packets_sent = 1;
  stats.stream_bytes_sent = 2;
  stats.stream_bytes_received = 3;
  stats.num_outgoing_streams_created = 4;
  stats.num_incoming_streams_created = 5;
  stats.bytes_received = 6;
  stats.packets_received = 7;
  stats.packets_processed = 8;
  stats.bytes_retransmitted = 9;
  stats.packets_retransmitted = 10;
  stats.packets_lost = 11;
  stats.packets_dropped = 12;
  stats.crypto_retransmit_count = 13;
  stats.min_rtt_us = 14;
  stats.srtt_us = 15;
  stats.max_packet_size = 16;
  stats.max_received_packet_size = 17;
  stats.estimated_bandwidth_bps = 18;
  stats.packets_reordered = 19;
  stats.blocked_frames_received = 20;
  stats.blocked_frames_sent = 21;
  stats.connectivity_probing_packets_received = 22;
  stats.num_datagrams_lost = 23;
  EXPECT_CALL(*mock_transport_ptr, GetStats()).WillOnce(Return(stats));
  ScriptPromise stats_promise =
      quic_transport->getStats(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_EQ(v8::Promise::kFulfilled,
            stats_promise.V8Value().As<v8::Promise>()->State());

  RTCQuicTransportStats* rtc_quic_stats =
      NativeValueTraits<RTCQuicTransportStats>::NativeValue(
          scope.GetIsolate(),
          stats_promise.V8Value().As<v8::Promise>()->Result(),
          ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(rtc_quic_stats->hasTimestamp());
  EXPECT_EQ(ConvertTimeTicksToDOMHighResTimeStamp(stats.timestamp),
            rtc_quic_stats->timestamp());
  ASSERT_TRUE(rtc_quic_stats->hasBytesSent());
  EXPECT_EQ(stats.bytes_sent, rtc_quic_stats->bytesSent());
  ASSERT_TRUE(rtc_quic_stats->hasPacketsSent());
  EXPECT_EQ(stats.packets_sent, rtc_quic_stats->packetsSent());
  ASSERT_TRUE(rtc_quic_stats->hasStreamBytesSent());
  EXPECT_EQ(stats.stream_bytes_sent, rtc_quic_stats->streamBytesSent());
  ASSERT_TRUE(rtc_quic_stats->hasStreamBytesSent());
  EXPECT_EQ(stats.stream_bytes_received, rtc_quic_stats->streamBytesReceived());
  ASSERT_TRUE(rtc_quic_stats->hasNumOutgoingStreamsCreated());
  EXPECT_EQ(stats.num_outgoing_streams_created,
            rtc_quic_stats->numOutgoingStreamsCreated());
  ASSERT_TRUE(rtc_quic_stats->hasNumIncomingStreamsCreated());
  EXPECT_EQ(stats.num_incoming_streams_created,
            rtc_quic_stats->numIncomingStreamsCreated());
  ASSERT_TRUE(rtc_quic_stats->hasBytesReceived());
  EXPECT_EQ(stats.bytes_received, rtc_quic_stats->bytesReceived());
  ASSERT_TRUE(rtc_quic_stats->hasPacketsReceived());
  EXPECT_EQ(stats.packets_received, rtc_quic_stats->packetsReceived());
  ASSERT_TRUE(rtc_quic_stats->hasPacketsProcessed());
  EXPECT_EQ(stats.packets_processed, rtc_quic_stats->packetsProcessed());
  ASSERT_TRUE(rtc_quic_stats->hasBytesRetransmitted());
  EXPECT_EQ(stats.bytes_retransmitted, rtc_quic_stats->bytesRetransmitted());
  ASSERT_TRUE(rtc_quic_stats->hasPacketsRetransmitted());
  EXPECT_EQ(stats.packets_retransmitted,
            rtc_quic_stats->packetsRetransmitted());
  ASSERT_TRUE(rtc_quic_stats->hasPacketsLost());
  EXPECT_EQ(stats.packets_lost, rtc_quic_stats->packetsLost());
  ASSERT_TRUE(rtc_quic_stats->hasPacketsDropped());
  EXPECT_EQ(stats.packets_dropped, rtc_quic_stats->packetsDropped());
  ASSERT_TRUE(rtc_quic_stats->hasCryptoRetransmitCount());
  EXPECT_EQ(stats.crypto_retransmit_count,
            rtc_quic_stats->cryptoRetransmitCount());
  ASSERT_TRUE(rtc_quic_stats->hasMinRttUs());
  EXPECT_EQ(stats.min_rtt_us, rtc_quic_stats->minRttUs());
  ASSERT_TRUE(rtc_quic_stats->hasSmoothedRttUs());
  EXPECT_EQ(stats.srtt_us, rtc_quic_stats->smoothedRttUs());
  ASSERT_TRUE(rtc_quic_stats->hasMaxPacketSize());
  EXPECT_EQ(stats.max_packet_size, rtc_quic_stats->maxPacketSize());
  ASSERT_TRUE(rtc_quic_stats->hasMaxReceivedPacketSize());
  EXPECT_EQ(stats.max_received_packet_size,
            rtc_quic_stats->maxReceivedPacketSize());
  ASSERT_TRUE(rtc_quic_stats->hasEstimatedBandwidthBps());
  EXPECT_EQ(stats.estimated_bandwidth_bps,
            rtc_quic_stats->estimatedBandwidthBps());
  ASSERT_TRUE(rtc_quic_stats->hasPacketsReordered());
  EXPECT_EQ(stats.packets_reordered, rtc_quic_stats->packetsReordered());
  ASSERT_TRUE(rtc_quic_stats->hasBlockedFramesReceived());
  EXPECT_EQ(stats.blocked_frames_received,
            rtc_quic_stats->blockedFramesReceived());
  ASSERT_TRUE(rtc_quic_stats->hasBlockedFramesSent());
  EXPECT_EQ(stats.blocked_frames_sent, rtc_quic_stats->blockedFramesSent());
  ASSERT_TRUE(rtc_quic_stats->hasConnectivityProbingPacketsReceived());
  EXPECT_EQ(stats.connectivity_probing_packets_received,
            rtc_quic_stats->connectivityProbingPacketsReceived());
  ASSERT_TRUE(rtc_quic_stats->hasNumDatagramsLost());
  EXPECT_EQ(stats.num_datagrams_lost, rtc_quic_stats->numDatagramsLost());
}

// Test that all promises are rejected if the connection closes before
// the OnStats callback is called.
TEST_F(RTCQuicTransportTest, FailedConnectionRejectsStatsPromises) {
  V8TestingScope scope;

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(mock_transport), &delegate);
  DCHECK(delegate);

  ScriptPromise promise_1 =
      quic_transport->getStats(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromise promise_2 =
      quic_transport->getStats(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  delegate->OnConnectionFailed("test_failure", /*from_remote=*/false);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise_1.V8Value().As<v8::Promise>()->State());
  EXPECT_EQ(v8::Promise::kRejected,
            promise_2.V8Value().As<v8::Promise>()->State());
}

// Test that all promises are rejected if the remote side closes before
// the OnStats callback is called.
TEST_F(RTCQuicTransportTest, RemoteStopRejectsStatsPromises) {
  V8TestingScope scope;

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(mock_transport), &delegate);
  DCHECK(delegate);

  ScriptPromise promise_1 =
      quic_transport->getStats(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromise promise_2 =
      quic_transport->getStats(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  delegate->OnRemoteStopped();

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise_1.V8Value().As<v8::Promise>()->State());
  EXPECT_EQ(v8::Promise::kRejected,
            promise_2.V8Value().As<v8::Promise>()->State());
}

// Test that calling getStats() after going to the "failed" state
// raises a kInvalidStateError.
TEST_F(RTCQuicTransportTest, FailedStateGetStatsRaisesInvalidStateError) {
  V8TestingScope scope;

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(mock_transport), &delegate);
  DCHECK(delegate);

  delegate->OnRemoteStopped();
  RunUntilIdle();

  quic_transport->getStats(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that the max datagram length is updated properly when the transport
// becomes connected.
TEST_F(RTCQuicTransportTest, MaxDatagramLengthComesFromNegotiatedParams) {
  V8TestingScope scope;
  Persistent<RTCIceTransport> ice_transport = CreateIceTransport(scope);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  P2PQuicTransport::Delegate* delegate = nullptr;
  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  Persistent<RTCQuicTransport> quic_transport =
      CreateQuicTransport(scope, ice_transport, GenerateLocalRTCCertificates(),
                          std::move(mock_transport), &delegate);
  quic_transport->start(CreateRemoteRTCQuicParameters1(), ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  P2PQuicNegotiatedParams params;
  uint16_t max_datagram_length = 10;
  params.set_max_datagram_length(max_datagram_length);
  delegate->OnConnected(params);
  RunUntilIdle();
  ASSERT_EQ("connected", quic_transport->state());
  bool is_null;
  EXPECT_EQ(max_datagram_length, quic_transport->maxDatagramLength(is_null));
  EXPECT_FALSE(is_null);
}

// Test that sending a datagram after the buffer is full will raise a
// kInvalidStateError.
TEST_F(RTCQuicTransportTest, SendingWhenNotReadyRaisesInvalidStateError) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);

  FillDatagramBuffer(quic_transport);
  RunUntilIdle();

  quic_transport->sendDatagram(
      DOMArrayBuffer::Create(kDatagram, kDatagramLength),
      scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that calling readyToSend before the last promise has resolved raises a
// kInvalidStateError.
TEST_F(RTCQuicTransportTest, ReadyToSendTwiceRaisesInvalidStateError) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);

  FillDatagramBuffer(quic_transport);
  RunUntilIdle();

  quic_transport->readyToSendDatagram(scope.GetScriptState(),
                                      ASSERT_NO_EXCEPTION);
  quic_transport->readyToSendDatagram(scope.GetScriptState(),
                                      scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that readyToSend promise will be pending until transport is no longer
// congestion control blocked.
TEST_F(RTCQuicTransportTest, ReadyToSendPromiseResolvesWhenReady) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);
  FillDatagramBuffer(quic_transport);
  RunUntilIdle();

  ScriptPromise promise = quic_transport->readyToSendDatagram(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());

  delegate->OnDatagramSent();
  RunUntilIdle();
  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that the pending readyToSend proimise will be rejected if stop() is
// called.
TEST_F(RTCQuicTransportTest, ReadyToSendPromiseRejectedWithStop) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);
  FillDatagramBuffer(quic_transport);
  RunUntilIdle();

  ScriptPromise promise = quic_transport->readyToSendDatagram(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());
  quic_transport->stop();
  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that the pending readyToSend proimise will be rejected if stop() is
// called on the RTCIceTransport.
TEST_F(RTCQuicTransportTest, ReadyToSendPromiseRejectedWithIceStop) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);
  FillDatagramBuffer(quic_transport);
  RunUntilIdle();

  ScriptPromise promise = quic_transport->readyToSendDatagram(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());
  quic_transport->transport()->stop();
  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that the pending readyToSend proimise will be rejected if connection
// fails.
TEST_F(RTCQuicTransportTest, ReadyToSendPromiseRejectedWithFailedConnection) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);
  FillDatagramBuffer(quic_transport);
  RunUntilIdle();

  ScriptPromise promise = quic_transport->readyToSendDatagram(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());
  delegate->OnConnectionFailed("test_failure", /*from_remote=*/false);
  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that the pending readyToSend proimise will be rejected if the remote
// side stops.
TEST_F(RTCQuicTransportTest, ReadyToSendPromiseRejectedWithRemoteStop) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);
  FillDatagramBuffer(quic_transport);
  RunUntilIdle();

  ScriptPromise promise = quic_transport->readyToSendDatagram(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());
  delegate->OnRemoteStopped();
  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that the pending receiveDatagrams proimise will be rejected if
// connection fails.
TEST_F(RTCQuicTransportTest,
       ReceiveDatagramsPromiseRejectedWithFailedConnection) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);
  ScriptPromise promise = quic_transport->receiveDatagrams(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());
  delegate->OnConnectionFailed("test_failure", /*from_remote=*/false);
  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that the pending receiveDatagrams proimise will be rejected if
// connection fails.
TEST_F(RTCQuicTransportTest, ReceiveDatagramsPromiseRejectedWithRemoteStop) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);

  ScriptPromise promise = quic_transport->receiveDatagrams(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());
  delegate->OnRemoteStopped();
  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that calling receiveDatagrams before the last promise has resolved
// raises a kInvalidStateError.
TEST_F(RTCQuicTransportTest, ReceiveDatagramsTwiceRaisesInvalidStateError) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);

  quic_transport->receiveDatagrams(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  quic_transport->receiveDatagrams(scope.GetScriptState(),
                                   scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that if datagrams are buffered before asking for a receiveDatagrams
// promise, that calling receiveDatagrams will return a promise immediately
// resolved with buffered datagrams.
TEST_F(RTCQuicTransportTest, ReceiveBufferedDatagrams) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &delegate);
  DCHECK(delegate);

  delegate->OnDatagramReceived({1, 2, 3});
  delegate->OnDatagramReceived({4, 5, 6});
  delegate->OnDatagramReceived({7, 8, 9});
  RunUntilIdle();

  ScriptPromise promise = quic_transport->receiveDatagrams(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());
  HeapVector<Member<DOMArrayBuffer>> received_datagrams =
      NativeValueTraits<IDLSequence<DOMArrayBuffer>>::NativeValue(
          scope.GetIsolate(), promise.V8Value().As<v8::Promise>()->Result(),
          ASSERT_NO_EXCEPTION);

  ASSERT_EQ(3u, received_datagrams.size());
  EXPECT_THAT(SpanFromDOMArrayBuffer(received_datagrams[0]),
              ElementsAre(1, 2, 3));
  EXPECT_THAT(SpanFromDOMArrayBuffer(received_datagrams[1]),
              ElementsAre(4, 5, 6));
  EXPECT_THAT(SpanFromDOMArrayBuffer(received_datagrams[2]),
              ElementsAre(7, 8, 9));
}

// Test that if datagrams are dropped once we have buffered the max amount.
TEST_F(RTCQuicTransportTest, ReceiveBufferedDatagramsLost) {
  V8TestingScope scope;

  auto mock_transport = std::make_unique<MockP2PQuicTransport>();
  MockP2PQuicTransport* mock_transport_ptr = mock_transport.get();
  P2PQuicTransport::Delegate* delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(mock_transport), &delegate);
  DCHECK(delegate);

  size_t num_dropped_datagrams = 5;
  size_t num_received_datagrams =
      kMaxBufferedRecvDatagrams + num_dropped_datagrams;
  for (size_t i = 0; i < num_received_datagrams; ++i) {
    delegate->OnDatagramReceived({1});
  }
  RunUntilIdle();

  ScriptPromise received_datagrams_promise = quic_transport->receiveDatagrams(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ASSERT_EQ(v8::Promise::kFulfilled,
            received_datagrams_promise.V8Value().As<v8::Promise>()->State());
  HeapVector<Member<DOMArrayBuffer>> received_datagrams =
      NativeValueTraits<IDLSequence<DOMArrayBuffer>>::NativeValue(
          scope.GetIsolate(),
          received_datagrams_promise.V8Value().As<v8::Promise>()->Result(),
          ASSERT_NO_EXCEPTION);

  EXPECT_CALL(*mock_transport_ptr, GetStats())
      .WillOnce(Return(P2PQuicTransportStats()));
  ScriptPromise stats_promise =
      quic_transport->getStats(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_EQ(v8::Promise::kFulfilled,
            stats_promise.V8Value().As<v8::Promise>()->State());

  RTCQuicTransportStats* rtc_quic_stats =
      NativeValueTraits<RTCQuicTransportStats>::NativeValue(
          scope.GetIsolate(),
          stats_promise.V8Value().As<v8::Promise>()->Result(),
          ASSERT_NO_EXCEPTION);

  EXPECT_EQ(kMaxBufferedRecvDatagrams, received_datagrams.size());
  ASSERT_TRUE(rtc_quic_stats->hasNumReceivedDatagramsDropped());
  EXPECT_EQ(num_dropped_datagrams,
            rtc_quic_stats->numReceivedDatagramsDropped());
}

}  // namespace blink
