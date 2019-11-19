// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the RTCQuicStream Blink bindings, QuicStreamProxy and
// QuicStreamHost by mocking out the underlying P2PQuicTransport.
// Everything is run on a single thread but with separate TestSimpleTaskRunners
// for the main thread / worker thread.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport_test.h"

namespace blink {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;

RTCQuicStreamWriteParameters* CreateWriteParametersWithData(
    const Vector<uint8_t>& data) {
  RTCQuicStreamWriteParameters* write_parameters =
      RTCQuicStreamWriteParameters::Create();
  write_parameters->setData(NotShared<DOMUint8Array>(
      DOMUint8Array::Create(data.data(), data.size())));
  write_parameters->setFinish(false);
  return write_parameters;
}

RTCQuicStreamWriteParameters* CreateWriteParametersWithDataOfLength(
    uint32_t length,
    bool finish = false) {
  RTCQuicStreamWriteParameters* write_parameters =
      RTCQuicStreamWriteParameters::Create();
  write_parameters->setData(
      NotShared<DOMUint8Array>(DOMUint8Array::Create(length)));
  write_parameters->setFinish(finish);
  return write_parameters;
}

RTCQuicStreamWriteParameters* CreateWriteParametersWithoutData(bool finish) {
  RTCQuicStreamWriteParameters* write_parameters =
      RTCQuicStreamWriteParameters::Create();
  write_parameters->setFinish(finish);
  return write_parameters;
}

}  // namespace

class RTCQuicStreamTest : public RTCQuicTransportTest {
 public:
  RTCQuicStream* CreateQuicStream(V8TestingScope& scope,
                                  MockP2PQuicStream* mock_stream) {
    auto p2p_quic_transport = std::make_unique<MockP2PQuicTransport>();
    EXPECT_CALL(*p2p_quic_transport, CreateStream())
        .WillOnce(Return(mock_stream));
    Persistent<RTCQuicTransport> quic_transport =
        CreateConnectedQuicTransport(scope, std::move(p2p_quic_transport));
    return quic_transport->createStream(ASSERT_NO_EXCEPTION);
  }
};

// Test that RTCQuicTransport.createStream causes CreateStream to be called on
// the underlying transport and a P2PQuicStream::Delegate to be set on the
// underlying P2PQuicStream.
TEST_F(RTCQuicStreamTest, RTCQuicTransportCreateStreamCallsUnderlying) {
  V8TestingScope scope;

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, SetDelegate(_))
      .WillOnce(testing::Invoke(
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
  EXPECT_CALL(*quic_stream_listener, Invoke(_, _))
      .WillOnce(testing::Invoke([](ExecutionContext*, Event* event) {
        auto* stream_event = static_cast<RTCQuicStreamEvent*>(event);
        EXPECT_NE(nullptr, stream_event->stream());
      }));
  quic_transport->addEventListener(event_type_names::kQuicstream,
                                   quic_stream_listener);

  ASSERT_TRUE(transport_delegate);

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, SetDelegate(_))
      .WillOnce(testing::Invoke(
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
  EXPECT_CALL(*state_change_listener, Invoke(_, _))
      .WillOnce(InvokeWithoutArgs(
          [quic_stream]() { EXPECT_EQ("closed", quic_stream->state()); }));
  quic_stream->addEventListener(event_type_names::kStatechange,
                                state_change_listener);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

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
  EXPECT_CALL(*state_change_listener, Invoke(_, _)).Times(0);
  quic_stream->addEventListener(event_type_names::kStatechange,
                                state_change_listener);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream_delegate->OnRemoteReset();
  quic_stream->reset();
  EXPECT_EQ("closed", quic_stream->state());

  RunUntilIdle();

  EXPECT_EQ("closed", quic_stream->state());
}

// The following group tests write(), writeBufferedAmount(), and
// maxWriteBufferdAmount().

// Test that write() adds to writeBufferedAmount().
TEST_F(RTCQuicStreamTest, WriteAddsToWriteBufferedAmount) {
  V8TestingScope scope;

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  stream->write(CreateWriteParametersWithData({1, 2}), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(2u, stream->writeBufferedAmount());

  stream->write(CreateWriteParametersWithData({3, 4, 5}), ASSERT_NO_EXCEPTION);
  EXPECT_EQ(5u, stream->writeBufferedAmount());

  RunUntilIdle();
}

// Test that write() calls WriteData() on the underlying P2PQuicStream.
TEST_F(RTCQuicStreamTest, WriteCallsWriteData) {
  V8TestingScope scope;

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, WriteData(_, _))
      .WillOnce(testing::Invoke([](Vector<uint8_t> data, bool fin) {
        EXPECT_THAT(data, ElementsAre(1, 2, 3, 4));
        EXPECT_FALSE(fin);
      }));

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(CreateWriteParametersWithData({1, 2, 3, 4}),
                ASSERT_NO_EXCEPTION);

  RunUntilIdle();
}

// Test that write() with no data throws a NotSupportedError and does not post a
// WriteData() to the underlying P2PQuicStream.
TEST_F(RTCQuicStreamTest, WriteWithoutDataThrowsNotSupportedError) {
  V8TestingScope scope;

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, WriteData(_, _)).Times(0);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(CreateWriteParametersWithoutData(/*finish=*/false),
                scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kNotSupportedError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());

  RunUntilIdle();
}

// Test that writing a finish without data calls WriteData() on the underlying
// P2PQuicStream.
TEST_F(RTCQuicStreamTest, WriteFinishWithoutDataCallsWriteData) {
  V8TestingScope scope;

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();
  EXPECT_CALL(*p2p_quic_stream, WriteData(_, _))
      .WillOnce(testing::Invoke([](Vector<uint8_t> data, bool fin) {
        EXPECT_THAT(data, ElementsAre());
        EXPECT_TRUE(fin);
      }));

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(CreateWriteParametersWithoutData(/*finish=*/true),
                ASSERT_NO_EXCEPTION);

  RunUntilIdle();
}

// Test that writeBufferedAmount is decreased when receiving OnWriteDataConsumed
// from the underlying P2PQuicStream.
TEST_F(RTCQuicStreamTest, OnWriteDataConsumedSubtractsFromWriteBufferedAmount) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(CreateWriteParametersWithDataOfLength(4), ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnWriteDataConsumed(4);

  RunUntilIdle();

  EXPECT_EQ(0u, stream->writeBufferedAmount());
}

// Test that writeBufferedAmount is set to 0 if the stream was reset by the
// remote peer.
TEST_F(RTCQuicStreamTest, OnRemoteResetSetsWriteBufferedAmountToZero) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(CreateWriteParametersWithDataOfLength(4), ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream_delegate->OnRemoteReset();

  RunUntilIdle();

  EXPECT_EQ(0u, stream->writeBufferedAmount());

  RunUntilIdle();
}

// Test that writeBufferedAmount is set to 0 if the stream calls write() with
// a finish, followed by receiving a finish from the remote side, and reading
// it out.
//
// TODO(https://crbug.com/874296): It doesn't really make sense the write buffer
// gets cleared in this case. Consider changing this.
TEST_F(RTCQuicStreamTest,
       FinishThenReceiveFinishSetsWriteBufferedAmountToZero) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(CreateWriteParametersWithDataOfLength(4, /*finish=*/true),
                ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream_delegate->OnDataReceived({}, /*fin=*/true);

  RunUntilIdle();

  EXPECT_EQ(4u, stream->writeBufferedAmount());
  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(10));
  EXPECT_TRUE(stream->readInto(read_buffer, ASSERT_NO_EXCEPTION)->finished());
  EXPECT_EQ(0u, stream->writeBufferedAmount());

  RunUntilIdle();
}

// Test that write throws an InvalidStateError if the stream was reset by the
// remote peer.
TEST_F(RTCQuicStreamTest, WriteThrowsIfRemoteReset) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream_delegate->OnRemoteReset();

  RunUntilIdle();

  stream->write(CreateWriteParametersWithDataOfLength(1),
                scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());

  RunUntilIdle();
}

// The following group tests waitForWriteBufferedAmountBelow().

// Test that a waitForWriteBufferedAmountBelow() promise resolves once
// OnWriteDataConsumed() frees up enough write buffer space.
TEST_F(RTCQuicStreamTest, WaitForWriteBufferedAmountBelowResolves) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  stream->write(
      CreateWriteParametersWithDataOfLength(stream->maxWriteBufferedAmount()),
      ASSERT_NO_EXCEPTION);

  ScriptPromise promise = stream->waitForWriteBufferedAmountBelow(
      scope.GetScriptState(), stream->maxWriteBufferedAmount() - 1,
      ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnWriteDataConsumed(1);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that a waitForWriteBufferedAmount() promise does not resolve until
// OnWriteDataConsumed() frees up exactly the threshold amount.
TEST_F(RTCQuicStreamTest,
       WaitForWriteBufferedAmountBelowDoesNotResolveUntilExceedsThreshold) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(
      CreateWriteParametersWithDataOfLength(stream->maxWriteBufferedAmount()),
      ASSERT_NO_EXCEPTION);

  ScriptPromise promise = stream->waitForWriteBufferedAmountBelow(
      scope.GetScriptState(), stream->maxWriteBufferedAmount() - 10,
      ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());

  RunUntilIdle();

  // Post OnWriteDataConsumed(9) -- this should not resolve the promise since
  // we're waiting for 10 bytes to be available.
  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnWriteDataConsumed(9);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());

  // Post OnWriteDataConsumed(1) -- this should resolve the promise since now we
  // have 9 + 1 = 10 bytes available.
  stream_delegate->OnWriteDataConsumed(1);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that if two waitForWriteBufferedAmount() promises are waiting on
// different thresholds, OnWriteDataConsumed() which satisfies the first
// threshold but not the second will only resolve the first promise. Once
// OnWriteDataConsumed() is received again past the second threshold then the
// second promise will be resolved.
TEST_F(RTCQuicStreamTest,
       TwoWaitForWriteBufferedAmountBelowPromisesResolveInSequence) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(
      CreateWriteParametersWithDataOfLength(stream->maxWriteBufferedAmount()),
      ASSERT_NO_EXCEPTION);

  ScriptPromise promise_10 = stream->waitForWriteBufferedAmountBelow(
      scope.GetScriptState(), stream->maxWriteBufferedAmount() - 10,
      ASSERT_NO_EXCEPTION);
  ScriptPromise promise_90 = stream->waitForWriteBufferedAmountBelow(
      scope.GetScriptState(), stream->maxWriteBufferedAmount() - 90,
      ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnWriteDataConsumed(10);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_10.V8Value().As<v8::Promise>()->State());
  EXPECT_EQ(v8::Promise::kPending,
            promise_90.V8Value().As<v8::Promise>()->State());

  stream_delegate->OnWriteDataConsumed(80);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_90.V8Value().As<v8::Promise>()->State());
}

// Test that if two waitForWriteBufferedAmount() promises are waiting on
// different thresholds and a single OnWriteDataConsumed() is received such that
// the buffered amount is below both thresholds then both promises are resolved.
TEST_F(RTCQuicStreamTest,
       TwoWaitForWriteBufferedAmountBelowPromisesResolveTogether) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(
      CreateWriteParametersWithDataOfLength(stream->maxWriteBufferedAmount()),
      ASSERT_NO_EXCEPTION);

  ScriptPromise promise_10 = stream->waitForWriteBufferedAmountBelow(
      scope.GetScriptState(), stream->maxWriteBufferedAmount() - 10,
      ASSERT_NO_EXCEPTION);
  ScriptPromise promise_90 = stream->waitForWriteBufferedAmountBelow(
      scope.GetScriptState(), stream->maxWriteBufferedAmount() - 90,
      ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnWriteDataConsumed(90);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_10.V8Value().As<v8::Promise>()->State());
  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_90.V8Value().As<v8::Promise>()->State());
}

// Test that when receiving OnRemoteReset() the waitForWriteBufferedAmountBelow
// Promise will be rejected.
TEST_F(RTCQuicStreamTest,
       WaitForWriteBufferedAmountBelowPromisesRejectedOnRemoteReset) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(
      CreateWriteParametersWithDataOfLength(stream->maxWriteBufferedAmount()),
      ASSERT_NO_EXCEPTION);

  ScriptPromise promise = stream->waitForWriteBufferedAmountBelow(
      scope.GetScriptState(), 0, ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream_delegate->OnRemoteReset();

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kRejected,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that there is no crash when the ExecutionContext is being destroyed and
// there are pending waitForWriteBufferedAmountBelow() promises. If the
// RTCQuicStream attempts to resolve the promise in ContextDestroyed, it will
// likely crash since the v8::Isolate is being torn down.
TEST_F(
    RTCQuicStreamTest,
    NoCrashIfPendingWaitForWriteBufferedAmountBelowPromisesOnContextDestroyed) {
  V8TestingScope scope;

  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>();

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  stream->write(
      CreateWriteParametersWithDataOfLength(stream->maxWriteBufferedAmount()),
      ASSERT_NO_EXCEPTION);

  stream->waitForWriteBufferedAmountBelow(scope.GetScriptState(), 0,
                                          ASSERT_NO_EXCEPTION);

  RunUntilIdle();
}

// The following group tests readInto(), readBufferedAmount(), and
// maxReadBufferedAmount().

static base::span<const uint8_t> GetSpan(NotShared<DOMUint8Array> data) {
  return base::make_span(data.View()->Data(), data.View()->length());
}

// Test that readInto() with an empty data buffer succeeds but does not post
// MarkReceivedDataConsumed() to the underlying P2PQuicStream.
TEST_F(RTCQuicStreamTest, ReadIntoEmptyDoesNotPostMarkReceivedDataConsumed) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  EXPECT_CALL(*p2p_quic_stream, MarkReceivedDataConsumed(_)).Times(0);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  EXPECT_EQ(0u, stream->readBufferedAmount());

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(0));
  RTCQuicStreamReadResult* result =
      stream->readInto(read_buffer, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(0u, result->amount());
  EXPECT_FALSE(result->finished());
  EXPECT_EQ(0u, stream->readBufferedAmount());

  RunUntilIdle();
}

// Test that data delived via OnDataReceived() increases readBufferedAmount.
TEST_F(RTCQuicStreamTest, OnDataReceivedIncreasesReadBufferedAmount) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/false);

  RunUntilIdle();

  EXPECT_EQ(3u, stream->readBufferedAmount());
}

// Test that readInto() reads out data received from OnDataReceived() and
// decreases readBufferedAmount.
TEST_F(RTCQuicStreamTest, ReadIntoReadsDataFromOnDataReceived) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/false);

  RunUntilIdle();

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(5));
  RTCQuicStreamReadResult* result =
      stream->readInto(read_buffer, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(3u, result->amount());
  EXPECT_FALSE(result->finished());
  EXPECT_THAT(GetSpan(read_buffer), ElementsAre(1, 2, 3, 0, 0));
  EXPECT_EQ(0u, stream->readBufferedAmount());

  RunUntilIdle();
}

// Test that readInto() posts MarkReceivedDataConsumed() with the amount of
// bytes read to the underlying P2PQuicStream.
TEST_F(RTCQuicStreamTest, ReadIntoPostsMarkReceivedDataConsumed) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  EXPECT_CALL(*p2p_quic_stream, MarkReceivedDataConsumed(3)).Times(1);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/false);

  RunUntilIdle();

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(5));
  RTCQuicStreamReadResult* result =
      stream->readInto(read_buffer, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(3u, result->amount());
  EXPECT_FALSE(result->finished());

  RunUntilIdle();
}

// Test that readInto() returns {finished: true} if OnDataReceived() has
// indicated the stream is finished without receiving any data.
TEST_F(RTCQuicStreamTest, ReadIntoReadsBareFin) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({}, /*fin=*/true);

  RunUntilIdle();

  NotShared<DOMUint8Array> empty_read_buffer(DOMUint8Array::Create(0));
  RTCQuicStreamReadResult* result =
      stream->readInto(empty_read_buffer, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(0u, result->amount());
  EXPECT_TRUE(result->finished());

  RunUntilIdle();
}

// Test that readInto() indicates finished once all buffered received data has
// been read.
TEST_F(RTCQuicStreamTest, ReadIntoReadsBufferedDataAndFinish) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/true);

  RunUntilIdle();

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(3));
  RTCQuicStreamReadResult* result =
      stream->readInto(read_buffer, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(3u, result->amount());
  EXPECT_TRUE(result->finished());
  EXPECT_THAT(GetSpan(read_buffer), ElementsAre(1, 2, 3));

  RunUntilIdle();
}

// Test that readInto() does not indicate finished until all buffered data has
// been read out, even if the finish has already been received.
TEST_F(RTCQuicStreamTest, ReadIntoReadsPartialDataBeforeFin) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/true);

  RunUntilIdle();

  {
    NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(2));
    RTCQuicStreamReadResult* result =
        stream->readInto(read_buffer, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(2u, result->amount());
    EXPECT_FALSE(result->finished());
    EXPECT_THAT(GetSpan(read_buffer), ElementsAre(1, 2));
  }

  {
    NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(2));
    RTCQuicStreamReadResult* result =
        stream->readInto(read_buffer, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(1u, result->amount());
    EXPECT_TRUE(result->finished());
    EXPECT_THAT(GetSpan(read_buffer), ElementsAre(3, 0));
  }

  RunUntilIdle();
}

// Test that readInto() does not post MarkReceivedDataConsumed() to the
// underlying P2PQuicStream once the finish flag has been delivered in
// OnDataReceived().
TEST_F(RTCQuicStreamTest,
       ReadIntoDoesNotPostMarkReceivedDataConsumedOnceFinReceived) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  EXPECT_CALL(*p2p_quic_stream, MarkReceivedDataConsumed(_)).Times(0);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/true);

  RunUntilIdle();

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(2));

  RTCQuicStreamReadResult* result =
      stream->readInto(read_buffer, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(2u, result->amount());
  EXPECT_FALSE(result->finished());

  result = stream->readInto(read_buffer, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(1u, result->amount());
  EXPECT_TRUE(result->finished());

  RunUntilIdle();
}

// Test that readInto() throws an InvalidStateError if the finish flag has
// already been read out.
TEST_F(RTCQuicStreamTest, ReadIntoThrowsIfFinishAlreadyRead) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({}, /*fin=*/true);

  RunUntilIdle();

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(2));
  EXPECT_TRUE(stream->readInto(read_buffer, ASSERT_NO_EXCEPTION)->finished());

  stream->readInto(read_buffer, scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

// Test that readInto() throws an InvalidStateError if the stream is closed.
TEST_F(RTCQuicStreamTest, ReadIntoThrowsIfClosed) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);

  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream->reset();

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(2));
  stream->readInto(read_buffer, scope.GetExceptionState());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());

  RunUntilIdle();
}

// The following group tests waitForReadable().

// Test that a waitForReadable() promise resolves once OnDataReceived() delivers
// enough data.
TEST_F(RTCQuicStreamTest, WaitForReadableResolves) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  ScriptPromise promise =
      stream->waitForReadable(scope.GetScriptState(), 3, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/false);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that a waitForReadable() promise resolves immediately if sufficient data
// is already in the receive buffer.
TEST_F(RTCQuicStreamTest, WaitForReadableResolveImmediately) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/false);

  RunUntilIdle();

  ScriptPromise promise =
      stream->waitForReadable(scope.GetScriptState(), 3, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());

  RunUntilIdle();
}

// Test that a waitForReadable() promise resolves immediately if finish has
// been received, but not yet read out.
TEST_F(RTCQuicStreamTest,
       WaitForReadableResolveImmediatelyAfterFinishReceived) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({}, /*fin=*/true);

  RunUntilIdle();

  ScriptPromise promise =
      stream->waitForReadable(scope.GetScriptState(), 10, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());

  RunUntilIdle();
}

// Test that a waitForReadable() promise does not resolve until OnDataReceived()
// delivers at least the readable amount.
TEST_F(RTCQuicStreamTest, WaitForReadableDoesNotResolveUntilExceedsThreshold) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  ScriptPromise promise =
      stream->waitForReadable(scope.GetScriptState(), 5, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/false);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kPending,
            promise.V8Value().As<v8::Promise>()->State());

  stream_delegate->OnDataReceived({4, 5}, /*fin=*/false);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());
}

// Test that if two waitForReadable() promises are waiting on different readable
// amounts, OnDataReceived() which satisfies the first readable amount but not
// the second will only resolve the first promise. Once OnDataReceived() is
// received again with readable amount satisfying the second promise then it
// will be resolved.
TEST_F(RTCQuicStreamTest, TwoWaitForReadablePromisesResolveInSequence) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  ScriptPromise promise_3 =
      stream->waitForReadable(scope.GetScriptState(), 3, ASSERT_NO_EXCEPTION);
  ScriptPromise promise_5 =
      stream->waitForReadable(scope.GetScriptState(), 5, ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3}, /*fin=*/false);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_3.V8Value().As<v8::Promise>()->State());
  EXPECT_EQ(v8::Promise::kPending,
            promise_5.V8Value().As<v8::Promise>()->State());

  stream_delegate->OnDataReceived({4, 5}, /*fin=*/false);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_5.V8Value().As<v8::Promise>()->State());
}

// Test that if two waitForReadable() promises are waiting on different
// thresholds and a single OnDataReceived() is received such that the readable
// amount satisfies both promises then they are both resolved.
TEST_F(RTCQuicStreamTest, TwoWaitForReadablePromisesResolveTogether) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  ScriptPromise promise_3 =
      stream->waitForReadable(scope.GetScriptState(), 3, ASSERT_NO_EXCEPTION);
  ScriptPromise promise_5 =
      stream->waitForReadable(scope.GetScriptState(), 5, ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({1, 2, 3, 4, 5}, /*fin=*/false);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_3.V8Value().As<v8::Promise>()->State());
  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_5.V8Value().As<v8::Promise>()->State());
}

// Test that a remote finish immediately resolves all pending waitForReadable()
// promises.
TEST_F(RTCQuicStreamTest, RemoteFinishResolvesPendingWaitForReadablePromises) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  ScriptPromise promise_3 =
      stream->waitForReadable(scope.GetScriptState(), 3, ASSERT_NO_EXCEPTION);
  ScriptPromise promise_5 =
      stream->waitForReadable(scope.GetScriptState(), 5, ASSERT_NO_EXCEPTION);

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({}, /*fin=*/true);

  RunUntilIdle();

  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_3.V8Value().As<v8::Promise>()->State());
  EXPECT_EQ(v8::Promise::kFulfilled,
            promise_5.V8Value().As<v8::Promise>()->State());
}

// Test that calling waitForReadable() resolves immediately if the finish has
// been received via OnDataReceived() but not yet read out via readInto().
// Note: If the finish has been read out via readInto(), waitForReadable() will
// throw an exception since the stream is no longer readable.
TEST_F(RTCQuicStreamTest, WaitForReadableResolvesImmediatelyIfRemoteFinished) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({}, /*fin=*/true);

  RunUntilIdle();

  ScriptPromise promise =
      stream->waitForReadable(scope.GetScriptState(), 5, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(v8::Promise::kFulfilled,
            promise.V8Value().As<v8::Promise>()->State());
}

// The following group tests state transitions with reset(), write() with a
// finish, remote reset() and remote write() with a finish.

// Test that a OnRemoteReset() immediately transitions the state to 'closed'.
TEST_F(RTCQuicStreamTest, OnRemoteResetTransitionsToClosed) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  EXPECT_EQ("open", stream->state());
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream_delegate->OnRemoteReset();

  RunUntilIdle();

  EXPECT_EQ("closed", stream->state());

  RunUntilIdle();
}

// Test that writing a finish after reading out a remote finish transitions
// state to 'closed'.
TEST_F(RTCQuicStreamTest, FinishAfterReadingRemoteFinishTransitionsToClosed) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  EXPECT_CALL(*p2p_quic_stream, WriteData(_, true)).Times(1);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  stream_delegate->OnDataReceived({}, /*fin=*/true);

  RunUntilIdle();

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(10));
  EXPECT_TRUE(stream->readInto(read_buffer, ASSERT_NO_EXCEPTION)->finished());

  EXPECT_EQ("closing", stream->state());
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream->write(CreateWriteParametersWithoutData(/*finish=*/true),
                ASSERT_NO_EXCEPTION);

  EXPECT_EQ("closed", stream->state());

  RunUntilIdle();
}

// Test that reading out a remote finish after writing a finish transitions
// state to 'closed'.
TEST_F(RTCQuicStreamTest, ReadingRemoteFinishAfterFinishTransitionsToClosed) {
  V8TestingScope scope;

  P2PQuicStream::Delegate* stream_delegate = nullptr;
  auto p2p_quic_stream = std::make_unique<MockP2PQuicStream>(&stream_delegate);
  EXPECT_CALL(*p2p_quic_stream, WriteData(_, true)).Times(1);
  Persistent<RTCQuicStream> stream =
      CreateQuicStream(scope, p2p_quic_stream.get());

  stream->write(CreateWriteParametersWithoutData(/*finish=*/true),
                ASSERT_NO_EXCEPTION);

  EXPECT_EQ("closing", stream->state());

  RunUntilIdle();

  ASSERT_TRUE(stream_delegate);
  EXPECT_CALL(*p2p_quic_stream.get(), SetDelegate(nullptr));

  stream_delegate->OnDataReceived({}, /*fin=*/true);

  RunUntilIdle();

  EXPECT_EQ("closing", stream->state());

  NotShared<DOMUint8Array> read_buffer(DOMUint8Array::Create(10));
  EXPECT_TRUE(stream->readInto(read_buffer, ASSERT_NO_EXCEPTION)->finished());

  EXPECT_EQ("closed", stream->state());
}

}  // namespace blink
