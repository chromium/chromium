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
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/webrtc/api/candidate.h"

namespace blink {

class MockAsyncDatagramConnection : public AsyncDatagramConnection {
 public:
  MOCK_METHOD(void,
              Writable,
              (ScriptPromiseResolver<IDLBoolean> * resolver),
              (override));
  MOCK_METHOD(void, Terminate, (), (override));
};

class RtcTransportTest : public PageTestBase {
 public:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }

  void CreateInitializedTransport() {
    auto* context = GetDocument().GetExecutionContext();
    auto mock_connection =
        std::make_unique<testing::NiceMock<MockAsyncDatagramConnection>>();
    mock_connection_ = mock_connection.get();
    DummyExceptionStateForTesting exception_state;

    transport_ = RtcTransport::CreateForTests(
        context, CreateRtcTransportConfig(), exception_state,
        std::move(mock_connection));
    ASSERT_FALSE(exception_state.HadException());
  }

  RtcTransportConfig* CreateRtcTransportConfig() {
    auto* ice_server = RTCIceServer::Create();
    ice_server->setUrls(MakeGarbageCollected<V8UnionStringOrStringSequence>(
        "stun:stun1.example.net:19302"));
    HeapVector<Member<RTCIceServer>> ice_servers;
    ice_servers.push_back(ice_server);

    auto* config = RtcTransportConfig::Create();
    config->setIceServers(ice_servers);
    return config;
  }

 protected:
  raw_ptr<MockAsyncDatagramConnection> mock_connection_;
  Persistent<RtcTransport> transport_;
};

TEST_F(RtcTransportTest, TerminateOnDestruction) {
  CreateInitializedTransport();
  EXPECT_CALL(*mock_connection_, Terminate()).Times(1);
}

TEST_F(RtcTransportTest, GetReceivedPackets) {
  CreateInitializedTransport();
  Vector<uint8_t> data;
  data.Append("packet", 6);
  transport_->OnPacketReceivedOnMainThread(data);

  HeapVector<Member<RtcReceivedPacket>> packets =
      transport_->getReceivedPackets();
  EXPECT_EQ(packets.size(), 1u);
  EXPECT_EQ(packets[0]->data()->ByteSpan(), String("packet").RawByteSpan());
}

TEST_F(RtcTransportTest, Writable) {
  CreateInitializedTransport();
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  EXPECT_CALL(*mock_connection_, Writable(testing::_))
      .WillOnce(
          testing::Invoke([](ScriptPromiseResolver<IDLBoolean>* resolver) {
            resolver->Resolve(true);
          }));

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
      context, CreateRtcTransportConfig(), exception_state, nullptr);

  ScriptPromiseTester tester(script_state,
                             unitialized_transport->writable(script_state));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_FALSE(tester.Value().V8Value()->IsTrue());
}

}  // namespace blink

namespace blink {

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

}  // namespace blink
