// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_server.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_ice_event.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/api/make_ref_counted.h"
#include "third_party/webrtc/api/test/mock_datagram_connection.h"

namespace blink {
using testing::_;
using AllowSharedBufferSource = RtcReceivedPacket::AllowSharedBufferSource;

webrtc::Timestamp GetWebRTCTimeOrigin(LocalDOMWindow* window) {
  return webrtc::Timestamp::Micros(
      (WindowPerformance::GetTimeOrigin(window) - base::TimeTicks())
          .InMicroseconds());
}

RtcTransportConfig* CreateRtcTransportConfig() {
  auto* stun_server = RTCIceServer::Create();
  stun_server->setUrls(MakeGarbageCollected<V8UnionStringOrStringSequence>(
      "stun:stun1.example.net:19302"));
  auto* turn_server = RTCIceServer::Create();
  turn_server->setUrls(MakeGarbageCollected<V8UnionStringOrStringSequence>(
      "turn:turn.example.net:12345"));
  turn_server->setUsername("user");
  turn_server->setCredential("password");
  HeapVector<Member<RTCIceServer>> ice_servers;
  ice_servers.push_back(stun_server);
  ice_servers.push_back(turn_server);

  auto* config = RtcTransportConfig::Create();
  config->setIceServers(ice_servers);
  config->setIceControlling(true);
  return config;
}

RtcDtlsParameters* CreateRtcDtlsParameters() {
  auto* params = RtcDtlsParameters::Create();
  params->setFingerprintDigestAlgorithm("sha-256");
  params->setSslRole(
      V8RtcTransportSslRole(V8RtcTransportSslRole::Enum::kServer));
  Vector<uint8_t> fingerprint_data;
  fingerprint_data.Append("fingerprint", 11);
  params->setFingerprint(DOMArrayBuffer::Create(fingerprint_data));
  return params;
}

AllowSharedBufferSource* MakeArrayBufferSource(const Vector<uint8_t>& data) {
  return MakeGarbageCollected<AllowSharedBufferSource>(
      DOMArrayBuffer::Create(data));
}

class MockAsyncDatagramConnection : public AsyncDatagramConnection {
 public:
  MOCK_METHOD(void,
              AddRemoteCandidate,
              (const webrtc::Candidate& candidate),
              (override));
  MOCK_METHOD(void,
              Writable,
              (ScriptPromiseResolver<IDLBoolean> * resolver),
              (override));
  MOCK_METHOD(void,
              SetRemoteDtlsParameters,
              (String digestAlgorithm,
               Vector<uint8_t> fingerprint,
               webrtc::DatagramConnection::SSLRole ssl_role),
              (override));
  MOCK_METHOD(void,
              SendPackets,
              (std::unique_ptr<Vector<Vector<uint8_t>>> packet_payloads),
              (override));
  MOCK_METHOD(void, Terminate, (), (override));
};

class RtcTransportTest : public PageTestBase {
 public:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }

  void CreateInitializedTransport() {
    CreateInitializedTransport(CreateRtcTransportConfig());
  }

  void CreateInitializedTransport(RtcTransportConfig* config) {
    auto* context = GetDocument().GetExecutionContext();
    auto mock_connection =
        std::make_unique<testing::NiceMock<MockAsyncDatagramConnection>>();
    mock_connection_ = mock_connection.get();
    DummyExceptionStateForTesting exception_state;

    transport_ = RtcTransport::CreateForTests(context, config, exception_state,
                                              std::move(mock_connection));
    ASSERT_FALSE(exception_state.HadException());
  }

  void OnInitialized(std::unique_ptr<AsyncDatagramConnection> connection) {
    transport_->OnInitialized(std::move(connection));
  }

 protected:
  raw_ptr<MockAsyncDatagramConnection> mock_connection_;
  Persistent<RtcTransport> transport_;
};

TEST_F(RtcTransportTest, TerminateOnDestruction) {
  CreateInitializedTransport();
  EXPECT_CALL(*mock_connection_, Terminate()).Times(1);
}

TEST_F(RtcTransportTest, AddRemoteCandidate) {
  CreateInitializedTransport();
  auto* init = RtcTransportICECandidateInit::Create();
  init->setType(V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kHost));
  init->setAddress("1.2.3.4");
  init->setPort(1234);
  init->setUsernameFragment("username");
  init->setPassword("password");

  EXPECT_CALL(*mock_connection_, AddRemoteCandidate(testing::_))
      .WillOnce([](const webrtc::Candidate& candidate) {
        EXPECT_EQ(candidate.protocol(), "udp");
        EXPECT_EQ(candidate.address().ToString(), "1.2.3.4:1234");
        EXPECT_EQ(candidate.username(), "username");
        EXPECT_EQ(candidate.password(), "password");
        EXPECT_EQ(candidate.type(), webrtc::IceCandidateType::kHost);
      });

  transport_->addRemoteCandidate(init, ASSERT_NO_EXCEPTION);
}

TEST_F(RtcTransportTest, AddRemoteCandidateBeforeInitialization) {
  auto* context = GetDocument().GetExecutionContext();
  DummyExceptionStateForTesting exception_state;
  transport_ = RtcTransport::CreateForTests(context, CreateRtcTransportConfig(),
                                            exception_state);
  ASSERT_FALSE(exception_state.HadException());

  auto* init = RtcTransportICECandidateInit::Create();
  init->setType(V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kHost));
  init->setAddress("1.2.3.4");
  init->setPort(1234);
  init->setUsernameFragment("username");
  init->setPassword("password");
  transport_->addRemoteCandidate(init, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  auto mock_connection =
      std::make_unique<testing::NiceMock<MockAsyncDatagramConnection>>();
  mock_connection_ = mock_connection.get();

  EXPECT_CALL(*mock_connection_, AddRemoteCandidate(testing::_))
      .WillOnce([](const webrtc::Candidate& candidate) {
        EXPECT_EQ(candidate.protocol(), "udp");
        EXPECT_EQ(candidate.address().ToString(), "1.2.3.4:1234");
        EXPECT_EQ(candidate.username(), "username");
        EXPECT_EQ(candidate.password(), "password");
        EXPECT_EQ(candidate.type(), webrtc::IceCandidateType::kHost);
      });
  OnInitialized(std::move(mock_connection));
}

TEST_F(RtcTransportTest, AddInvalidRemoteCandidateBeforeInitialization) {
  auto* context = GetDocument().GetExecutionContext();
  DummyExceptionStateForTesting exception_state;
  transport_ = RtcTransport::CreateForTests(context, CreateRtcTransportConfig(),
                                            exception_state);
  ASSERT_FALSE(exception_state.HadException());

  auto* init = RtcTransportICECandidateInit::Create();
  init->setType(V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kHost));
  init->setAddress("invalidAddress");
  init->setPort(1234);
  init->setUsernameFragment("username");
  init->setPassword("password");

  transport_->addRemoteCandidate(init, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            static_cast<int>(DOMExceptionCode::kSyntaxError));
  EXPECT_EQ("Invalid address", exception_state.Message());
}

TEST_F(RtcTransportTest, SendPackets) {
  CreateInitializedTransport();
  auto* params1 = RtcSendPacketParameters::Create();
  Vector<uint8_t> data1;
  data1.Append("packet1", 7);
  params1->setData(MakeArrayBufferSource(data1));
  auto* params2 = RtcSendPacketParameters::Create();
  Vector<uint8_t> data2;
  data2.Append("packet2", 7);
  params2->setData(MakeArrayBufferSource(data2));

  HeapVector<Member<RtcSendPacketParameters>> packets;
  packets.push_back(params1);
  packets.push_back(params2);

  EXPECT_CALL(*mock_connection_, SendPackets(testing::_))
      .WillOnce([](std::unique_ptr<Vector<Vector<uint8_t>>> packet_payloads) {
        EXPECT_EQ(packet_payloads->size(), 2u);
        EXPECT_EQ(packet_payloads->at(0), String("packet1").RawByteSpan());
        EXPECT_EQ(packet_payloads->at(1), String("packet2").RawByteSpan());
      });

  transport_->sendPackets(packets);
}

TEST_F(RtcTransportTest, GetReceivedPackets) {
  CreateInitializedTransport();
  Vector<uint8_t> data;
  data.Append("packet", 6);
  int kReceiveTimeMillis = 12345;

  transport_->OnPacketReceivedOnMainThread(
      data, GetWebRTCTimeOrigin(GetDocument().domWindow()) +
                webrtc::TimeDelta::Millis(kReceiveTimeMillis));

  HeapVector<Member<RtcReceivedPacket>> packets =
      transport_->getReceivedPackets();
  EXPECT_EQ(packets.size(), 1u);

  auto* buffer =
      DOMArrayBuffer::Create(/*num_elements=*/6, /*element_byte_size=*/1);
  AllowSharedBufferSource* destination =
      MakeGarbageCollected<AllowSharedBufferSource>(buffer);

  DummyExceptionStateForTesting exception_state;
  packets[0]->copyPayloadTo(destination, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(buffer->ByteSpan(), String("packet").RawByteSpan());
  // The precision for DOMHighResTimestamp is 0.1ms. Test equality by making
  // sure the difference between expected and received  is less than 0.2ms.
  EXPECT_LT(std::abs(packets[0]->receiveTime() - kReceiveTimeMillis), 0.2);
}

TEST_F(RtcTransportTest, Writable) {
  CreateInitializedTransport();
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  EXPECT_CALL(*mock_connection_, Writable(testing::_))
      .WillOnce([](ScriptPromiseResolver<IDLBoolean>* resolver) {
        resolver->Resolve(true);
      });

  ScriptPromiseTester tester(script_state, transport_->writable(script_state));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().V8Value()->IsTrue());
}

TEST_F(RtcTransportTest, WritableUninitialized) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* context = scope.GetExecutionContext();
  DummyExceptionStateForTesting exception_state;

  auto* unitialized_transport = RtcTransport::CreateForTests(
      context, CreateRtcTransportConfig(), exception_state);

  ScriptPromiseTester tester(script_state,
                             unitialized_transport->writable(script_state));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_FALSE(tester.Value().V8Value()->IsTrue());
}

TEST_F(RtcTransportTest, OnWritableChange) {
  CreateInitializedTransport();

  // Register a listener for the writablechange event.
  auto* wait = MakeGarbageCollected<WaitForEvent>();
  base::RunLoop run_loop;
  wait->AddEventListener(transport_, event_type_names::kWritablechange);
  wait->AddCompletionClosure(run_loop.QuitClosure());

  // Notify of a writable change.
  transport_->OnWritableChangeOnMainThread();

  // Wait for event to be fired.
  run_loop.Run();
}

// Regression test for crbug.com/455519961
TEST_F(RtcTransportTest, NoCrashReceivingPacketsAfterContextDestroyed) {
  CreateInitializedTransport();
  Vector<uint8_t> data;

  GetDocument().GetExecutionContext()->NotifyContextDestroyed();

  transport_->OnPacketReceivedOnMainThread(data, webrtc::Timestamp::Millis(0));
}

TEST_F(RtcTransportTest, NoCrashReceivingCandidateAfterContextDestroyed) {
  CreateInitializedTransport();

  GetDocument().GetExecutionContext()->NotifyContextDestroyed();

  transport_->OnCandidateGatheredOnMainThread(webrtc::Candidate());
}

class RtcTransportParseStunServersTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    context_ = GetDocument().GetExecutionContext();
    mock_connection_ =
        std::make_unique<testing::NiceMock<MockAsyncDatagramConnection>>();
  }

 protected:
  Persistent<ExecutionContext> context_;
  std::unique_ptr<MockAsyncDatagramConnection> mock_connection_;
};

TEST_F(RtcTransportParseStunServersTest, SuccessWithUrlsSequence) {
  DummyExceptionStateForTesting exception_state;
  auto* config = RtcTransportConfig::Create();
  auto* ice_server = RTCIceServer::Create();
  Vector<String> urls;
  urls.push_back("stun:stun1.example.net:19302");
  urls.push_back("stun:stun2.example.net:19302");
  ice_server->setUrls(
      MakeGarbageCollected<V8UnionStringOrStringSequence>(urls));
  HeapVector<Member<RTCIceServer>> ice_servers;
  ice_servers.push_back(ice_server);
  config->setIceServers(ice_servers);

  auto* transport = RtcTransport::CreateForTests(
      context_, config, exception_state, std::move(mock_connection_));
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_NE(transport, nullptr);
}

TEST_F(RtcTransportParseStunServersTest, FailureMissingIceServers) {
  DummyExceptionStateForTesting exception_state;
  auto* config = RtcTransportConfig::Create();
  auto* transport = RtcTransport::CreateForTests(
      context_, config, exception_state, std::move(mock_connection_));
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kNotSupportedError));
  EXPECT_EQ(transport, nullptr);
}

TEST_F(RtcTransportParseStunServersTest, FailureMissingUrls) {
  DummyExceptionStateForTesting exception_state;
  auto* config = RtcTransportConfig::Create();
  auto* ice_server = RTCIceServer::Create();
  HeapVector<Member<RTCIceServer>> ice_servers;
  ice_servers.push_back(ice_server);
  config->setIceServers(ice_servers);

  auto* transport = RtcTransport::CreateForTests(
      context_, config, exception_state, std::move(mock_connection_));
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kNotSupportedError));
  EXPECT_EQ(transport, nullptr);
}

TEST_F(RtcTransportParseStunServersTest, FailureInvalidUrl) {
  DummyExceptionStateForTesting exception_state;
  auto* config = RtcTransportConfig::Create();
  auto* ice_server = RTCIceServer::Create();
  ice_server->setUrls(
      MakeGarbageCollected<V8UnionStringOrStringSequence>("invalid-url"));
  HeapVector<Member<RTCIceServer>> ice_servers;
  ice_servers.push_back(ice_server);
  config->setIceServers(ice_servers);

  auto* transport = RtcTransport::CreateForTests(
      context_, config, exception_state, std::move(mock_connection_));
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kNotSupportedError));
  EXPECT_EQ(transport, nullptr);
}

TEST_F(RtcTransportParseStunServersTest, FailureMissingTurnCredentials) {
  DummyExceptionStateForTesting exception_state;
  auto* config = RtcTransportConfig::Create();
  auto* ice_server = RTCIceServer::Create();
  ice_server->setUrls(MakeGarbageCollected<V8UnionStringOrStringSequence>(
      "turn:turn.example.org:12345"));
  HeapVector<Member<RTCIceServer>> ice_servers;
  ice_servers.push_back(ice_server);
  config->setIceServers(ice_servers);

  auto* transport = RtcTransport::CreateForTests(
      context_, config, exception_state, std::move(mock_connection_));
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            ToExceptionCode(DOMExceptionCode::kNotSupportedError));
  EXPECT_EQ(transport, nullptr);
}

TEST_F(RtcTransportTest, SetRemoteDtlsParameters) {
  CreateInitializedTransport();
  auto* params = CreateRtcDtlsParameters();

  EXPECT_CALL(
      *mock_connection_,
      SetRemoteDtlsParameters(testing::Eq(String("sha-256")), testing::_,
                              webrtc::DatagramConnection::SSLRole::kServer))
      .WillOnce([](String digestAlgorithm, Vector<uint8_t> fingerprint,
                   webrtc::DatagramConnection::SSLRole ssl_role) {
        EXPECT_EQ(fingerprint, String("fingerprint").RawByteSpan());
      });

  transport_->setRemoteDtlsParameters(params);
}

TEST_F(RtcTransportTest, AddRemoteCandidateInvalidAddress) {
  CreateInitializedTransport();
  auto* init = RtcTransportICECandidateInit::Create();
  init->setType(V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kHost));
  init->setAddress("invalid");
  init->setPort(1234);
  init->setUsernameFragment("username");
  init->setPassword("password");

  DummyExceptionStateForTesting exception_state;
  transport_->addRemoteCandidate(init, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(),
            static_cast<int>(DOMExceptionCode::kSyntaxError));
  EXPECT_EQ("Invalid address", exception_state.Message());
}

TEST_F(RtcTransportTest, OnIceCandidate) {
  CreateInitializedTransport();

  auto* wait = MakeGarbageCollected<WaitForEvent>();
  base::RunLoop run_loop;
  wait->AddEventListener(transport_, event_type_names::kIcecandidate);
  wait->AddCompletionClosure(run_loop.QuitClosure());

  webrtc::Candidate candidate;
  candidate.set_username("username");
  candidate.set_password("password");
  candidate.set_address(webrtc::SocketAddress("1.2.3.4", 1234));
  candidate.set_type(webrtc::IceCandidateType::kHost);
  transport_->OnCandidateGatheredOnMainThread(candidate);

  // Wait for event to be fired.
  run_loop.Run();

  auto* event = static_cast<RtcTransportIceEvent*>(wait->GetLastEvent());
  ASSERT_NE(event, nullptr);
  auto* ice_candidate = event->candidate();
  EXPECT_EQ(ice_candidate->usernameFragment(), "username");
  EXPECT_EQ(ice_candidate->password(), "password");
  EXPECT_EQ(ice_candidate->address(), "1.2.3.4");
  EXPECT_EQ(ice_candidate->port(), 1234);
  EXPECT_EQ(ice_candidate->type(),
            V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kHost));
}

TEST_F(RtcTransportTest, DtlsWireProtocol) {
  RtcTransportConfig* config = CreateRtcTransportConfig();
  config->setWireProtocol(V8RtcTransportWireProtocol::Enum::kDtls);
  CreateInitializedTransport(config);
}

class RtcTransportMultithreadedTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    mock_sync_connection_ = webrtc::make_ref_counted<
        testing::NiceMock<webrtc::MockDatagramConnection>>();
  }

  RtcTransport* CreateTransport(ExceptionState& exception_state) {
    return RtcTransport::CreateForTests(
        GetDocument().GetExecutionContext(), CreateRtcTransportConfig(),
        exception_state,
        /*async_datagram_connection=*/nullptr, mock_sync_connection_);
  }

 protected:
  webrtc::scoped_refptr<webrtc::MockDatagramConnection> mock_sync_connection_;
};

TEST_F(RtcTransportMultithreadedTest, SetRemoteDtlsParameters) {
  DummyExceptionStateForTesting exception_state;
  RtcTransport* transport = CreateTransport(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_sync_connection_, SetRemoteDtlsParameters(_, _, _, _))
      .WillOnce(testing::InvokeWithoutArgs([&] { run_loop.Quit(); }));

  transport->setRemoteDtlsParameters(CreateRtcDtlsParameters());
  run_loop.Run();
}

TEST_F(RtcTransportMultithreadedTest, SendPackets) {
  DummyExceptionStateForTesting exception_state;
  RtcTransport* transport = CreateTransport(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  const size_t kPacketCount = 3;

  size_t packets_sent = 0;
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_sync_connection_, SendPackets(_))
      .WillRepeatedly([&](webrtc::ArrayView<
                          webrtc::DatagramConnection::PacketSendParameters>
                              packet_payloads) {
        packets_sent += packet_payloads.size();
        if (packets_sent == kPacketCount) {
          run_loop.Quit();
        }
        return true;
      });

  HeapVector<Member<RtcSendPacketParameters>> packets;
  for (size_t i = 0; i < kPacketCount; i++) {
    auto* params = RtcSendPacketParameters::Create();
    Vector<uint8_t> data;
    data.Append("packet", 6);
    params->setData(MakeArrayBufferSource(data));
    packets.push_back(params);
  }
  transport->sendPackets(packets);
  run_loop.Run();
}

}  // namespace blink
