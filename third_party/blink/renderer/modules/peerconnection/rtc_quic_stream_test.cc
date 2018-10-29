// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the RTCQuicStream Blink bindings, QuicStreamProxy and
// QuicStreamHost by mocking out the underlying P2PQuicTransport.
// Everything is run on a single thread but with separate TestSimpleTaskRunners
// for the main thread / worker thread.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport_test.h"

namespace blink {
namespace {

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;

}  // namespace

class RTCQuicStreamTest : public RTCQuicTransportTest {};

// Test that RTCQuicTransport.createStream causes CreateStream to be called on
// the underlying transport and a P2PQuicStream::Delegate to be set on the
// underlying P2PQuicStream.
TEST_F(RTCQuicStreamTest, RTCQuicTransportCreateStreamCallsUnderlying) {
  V8TestingScope scope;

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, SetDelegate(_))
      .WillOnce(Invoke(
          [](P2PQuicStream::Delegate* delegate) { EXPECT_TRUE(delegate); }));

  auto p2p_quic_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*p2p_quic_transport, CreateStream())
      .WillOnce(Return(p2p_quic_stream.get()));

  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(p2p_quic_transport));
  Persistent<RTCQuicStream> quic_stream =
      quic_transport->createStream(ASSERT_NO_EXCEPTION);

  RunUntilIdle();
}

// Test that calling OnStream on the P2PQuicTransport delegate causes a
// quicstream event to fire with a new RTCQuicStream.
TEST_F(RTCQuicStreamTest, NewRemoteStreamFiresEvent) {
  V8TestingScope scope;

  P2PQuicTransport::Delegate* transport_delegate = nullptr;
  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, &transport_delegate);

  Persistent<MockEventListener> quic_stream_listener =
      CreateMockEventListener();
  EXPECT_CALL(*quic_stream_listener, handleEvent(_, _))
      .WillOnce(Invoke([](ExecutionContext*, Event* event) {
        auto* stream_event = static_cast<RTCQuicStreamEvent*>(event);
        EXPECT_NE(nullptr, stream_event->stream());
      }));
  quic_transport->addEventListener(EventTypeNames::quicstream,
                                   quic_stream_listener);

  ASSERT_TRUE(transport_delegate);

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, SetDelegate(_))
      .WillOnce(Invoke(
          [](P2PQuicStream::Delegate* delegate) { EXPECT_TRUE(delegate); }));
  transport_delegate->OnStream(p2p_quic_stream.get());

  RunUntilIdle();
}

// Test that calling reset() calls Reset() on the underlying P2PQuicStream.
TEST_F(RTCQuicStreamTest, ResetCallsUnderlying) {
  V8TestingScope scope;

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, Reset()).Times(1);

  auto p2p_quic_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*p2p_quic_transport, CreateStream())
      .WillOnce(Return(p2p_quic_stream.get()));

  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(p2p_quic_transport));
  Persistent<RTCQuicStream> stream =
      quic_transport->createStream(ASSERT_NO_EXCEPTION);

  stream->reset();

  RunUntilIdle();
}

// Test that calling OnRemoteReset() on the P2PQuicStream delegate fires a
// statechange event to 'closed'.
TEST_F(RTCQuicStreamTest, OnRemoteResetFiresStateChangeToClosed) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, SetDelegate(_))
      .WillOnce(SaveArg<0>(&stream_delegate));

  auto p2p_quic_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*p2p_quic_transport, CreateStream())
      .WillOnce(Return(p2p_quic_stream.get()));

  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(p2p_quic_transport));
  Persistent<RTCQuicStream> quic_stream =
      quic_transport->createStream(ASSERT_NO_EXCEPTION);

  Persistent<MockEventListener> state_change_listener =
      CreateMockEventListener();
  EXPECT_CALL(*state_change_listener, handleEvent(_, _))
      .WillOnce(InvokeWithoutArgs(
          [quic_stream]() { EXPECT_EQ("closed", quic_stream->state()); }));
  quic_stream->addEventListener(EventTypeNames::statechange,
                                state_change_listener);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnRemoteReset();

  RunUntilIdle();
}

// Test that a pending OnRemoteReset() is ignored if reset() is called first.
TEST_F(RTCQuicStreamTest, PendingOnRemoteResetIgnoredAfterReset) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, SetDelegate(_))
      .WillOnce(SaveArg<0>(&stream_delegate));

  auto p2p_quic_transport = std::make_unique<MockP2PQuicTransport>();
  EXPECT_CALL(*p2p_quic_transport, CreateStream())
      .WillOnce(Return(p2p_quic_stream.get()));

  Persistent<RTCQuicTransport> quic_transport =
      CreateConnectedQuicTransport(scope, std::move(p2p_quic_transport));
  Persistent<RTCQuicStream> quic_stream =
      quic_transport->createStream(ASSERT_NO_EXCEPTION);

  // Expect no statechange event since reset() will already transition the state
  // to 'closed'.
  Persistent<MockEventListener> state_change_listener =
      CreateMockEventListener();
  EXPECT_CALL(*state_change_listener, handleEvent(_, _)).Times(0);
  quic_stream->addEventListener(EventTypeNames::statechange,
                                state_change_listener);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnRemoteReset();
  quic_stream->reset();
  EXPECT_EQ("closed", quic_stream->state());

  RunUntilIdle();

  EXPECT_EQ("closed", quic_stream->state());
}

}  // namespace blink
