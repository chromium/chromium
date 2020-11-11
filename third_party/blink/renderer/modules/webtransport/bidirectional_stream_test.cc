// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/bidirectional_stream.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/quic_transport.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bidirectional_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_quic_transport_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"
#include "third_party/blink/renderer/modules/webtransport/test_utils.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// These tests only ever create one stream at a time, so use a hardcoded stream
// id.
constexpr uint32_t kDefaultStreamId = 0;

// BidirectionalStream depends on blink::QuicTransport. Rather than virtualise
// blink::QuicTransport for these tests, we use a stub implementation of
// network::mojom::blink::QuicTransport to get the behaviour we want. This class
// only supports the creation of one BidirectionalStream at a time for
// simplicity.
class StubQuicTransport : public network::mojom::blink::QuicTransport {
 public:
  explicit StubQuicTransport(
      mojo::PendingReceiver<network::mojom::blink::QuicTransport>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  // Functions used by tests to inspect and manipulate the object.

  // Data written to the |writable| side of the bidirectional stream can be read
  // from this handle.
  mojo::ScopedDataPipeConsumerHandle& OutputConsumer() {
    return output_consumer_;
  }

  // Data written to this handle will appear on the |readable| side of the
  // bidirectional stream.
  mojo::ScopedDataPipeProducerHandle& InputProducer() {
    return input_producer_;
  }

  bool WasSendFinCalled() const { return was_send_fin_called_; }
  bool WasAbortStreamCalled() const { return was_abort_stream_called_; }

  // Responds to an earlier call to AcceptBidirectionalStream with a new stream
  // as if it was created by the remote server. The remote handles can be
  // accessed via OutputConsumer() and InputConsumer() as with locally-created
  // streams.
  void CreateRemote() {
    ASSERT_TRUE(accept_callback_);
    mojo::ScopedDataPipeProducerHandle output_producer;
    mojo::ScopedDataPipeConsumerHandle output_consumer;

    ASSERT_TRUE(
        CreateDataPipeForWebTransportTests(&output_producer, &output_consumer));
    output_consumer_ = std::move(output_consumer);

    mojo::ScopedDataPipeProducerHandle input_producer;
    mojo::ScopedDataPipeConsumerHandle input_consumer;

    ASSERT_TRUE(
        CreateDataPipeForWebTransportTests(&input_producer, &input_consumer));
    input_producer_ = std::move(input_producer);

    std::move(accept_callback_)
        .Run(kDefaultStreamId, std::move(input_consumer),
             std::move(output_producer));

    // This prevents redundant calls to AcceptBidirectionalStream() by ensuring
    // the call to Enqueue() happens before the next call to pull().
    test::RunPendingTasks();
  }

  // Implementation of QuicTransport.
  void SendDatagram(base::span<const uint8_t> data,
                    base::OnceCallback<void(bool)>) override {
    NOTREACHED();
  }

  void CreateStream(
      mojo::ScopedDataPipeConsumerHandle output_consumer,
      mojo::ScopedDataPipeProducerHandle input_producer,
      base::OnceCallback<void(bool, uint32_t)> callback) override {
    EXPECT_TRUE(output_consumer.is_valid());
    EXPECT_FALSE(output_consumer_.is_valid());
    output_consumer_ = std::move(output_consumer);

    EXPECT_TRUE(input_producer.is_valid());
    EXPECT_FALSE(input_producer_.is_valid());
    input_producer_ = std::move(input_producer);

    std::move(callback).Run(true, kDefaultStreamId);
  }

  void AcceptBidirectionalStream(
      base::OnceCallback<void(uint32_t,
                              mojo::ScopedDataPipeConsumerHandle,
                              mojo::ScopedDataPipeProducerHandle)> callback)
      override {
    DCHECK(!accept_callback_);
    accept_callback_ = std::move(callback);
  }

  void AcceptUnidirectionalStream(
      base::OnceCallback<void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>
          callback) override {
    DCHECK(!ignored_unidirectional_stream_callback_);
    // This method is always called. We have to retain the callback to avoid an
    // error about early destruction, but never call it.
    ignored_unidirectional_stream_callback_ = std::move(callback);
  }

  void SendFin(uint32_t stream_id) override {
    EXPECT_EQ(stream_id, kDefaultStreamId);
    was_send_fin_called_ = true;
  }

  void AbortStream(uint32_t stream_id, uint64_t code) override {
    EXPECT_EQ(stream_id, kDefaultStreamId);
    was_abort_stream_called_ = true;
  }

 private:
  base::OnceCallback<void(uint32_t,
                          mojo::ScopedDataPipeConsumerHandle,
                          mojo::ScopedDataPipeProducerHandle)>
      accept_callback_;
  base::OnceCallback<void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>
      ignored_unidirectional_stream_callback_;
  mojo::Receiver<network::mojom::blink::QuicTransport> receiver_;
  mojo::ScopedDataPipeConsumerHandle output_consumer_;
  mojo::ScopedDataPipeProducerHandle input_producer_;
  bool was_send_fin_called_ = false;
  bool was_abort_stream_called_ = false;
};

// This class sets up a connected blink::QuicTransport object using a
// StubQuicTransport and provides access to both.
class ScopedQuicTransport : public mojom::blink::QuicTransportConnector {
  STACK_ALLOCATED();

 public:
  // For convenience, this class does work in the constructor. This is okay
  // because it is only used for testing.
  explicit ScopedQuicTransport(const V8TestingScope& scope)
      : browser_interface_broker_(
            &scope.GetExecutionContext()->GetBrowserInterfaceBroker()) {
    browser_interface_broker_->SetBinderForTesting(
        mojom::blink::QuicTransportConnector::Name_,
        base::BindRepeating(&ScopedQuicTransport::BindConnector,
                            weak_ptr_factory_.GetWeakPtr()));
    quic_transport_ = QuicTransport::Create(
        scope.GetScriptState(), "quic-transport://example.com/",
        MakeGarbageCollected<QuicTransportOptions>(), ASSERT_NO_EXCEPTION);

    test::RunPendingTasks();
  }

  ~ScopedQuicTransport() override {
    browser_interface_broker_->SetBinderForTesting(
        mojom::blink::QuicTransportConnector::Name_, {});
  }

  QuicTransport* GetQuicTransport() const { return quic_transport_; }
  StubQuicTransport* Stub() const { return stub_.get(); }

  void ResetStub() { stub_.reset(); }

  BidirectionalStream* CreateBidirectionalStream(const V8TestingScope& scope) {
    auto* script_state = scope.GetScriptState();
    ScriptPromise bidirectional_stream_promise =
        quic_transport_->createBidirectionalStream(script_state,
                                                   ASSERT_NO_EXCEPTION);
    ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

    tester.WaitUntilSettled();

    EXPECT_TRUE(tester.IsFulfilled());
    auto* bidirectional_stream = V8BidirectionalStream::ToImplWithTypeCheck(
        scope.GetIsolate(), tester.Value().V8Value());
    EXPECT_TRUE(bidirectional_stream);
    return bidirectional_stream;
  }

  BidirectionalStream* RemoteCreateBidirectionalStream(
      const V8TestingScope& scope) {
    stub_->CreateRemote();
    ReadableStream* streams = quic_transport_->receiveBidirectionalStreams();

    v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

    BidirectionalStream* bidirectional_stream =
        V8BidirectionalStream::ToImplWithTypeCheck(scope.GetIsolate(), v8value);
    EXPECT_TRUE(bidirectional_stream);

    return bidirectional_stream;
  }

  // Implementation of mojom::blink::QuicTransportConnector.
  void Connect(
      const KURL&,
      Vector<network::mojom::blink::QuicTransportCertificateFingerprintPtr>
          fingerprints,
      mojo::PendingRemote<network::mojom::blink::QuicTransportHandshakeClient>
          pending_handshake_client) override {
    mojo::Remote<network::mojom::blink::QuicTransportHandshakeClient>
        handshake_client(std::move(pending_handshake_client));

    mojo::PendingRemote<network::mojom::blink::QuicTransport>
        quic_transport_to_pass;
    mojo::PendingRemote<network::mojom::blink::QuicTransportClient>
        client_remote;

    stub_ = std::make_unique<StubQuicTransport>(
        quic_transport_to_pass.InitWithNewPipeAndPassReceiver());

    handshake_client->OnConnectionEstablished(
        std::move(quic_transport_to_pass),
        client_remote.InitWithNewPipeAndPassReceiver());
    client_remote_.Bind(std::move(client_remote));
  }

 private:
  void BindConnector(mojo::ScopedMessagePipeHandle handle) {
    connector_receiver_.Bind(
        mojo::PendingReceiver<mojom::blink::QuicTransportConnector>(
            std::move(handle)));
  }

  // |browser_interface_broker_| is cached here because we need to use it in the
  // destructor. This means ScopedQuicTransport must always be destroyed before
  // the V8TestingScope object that owns the BrowserInterfaceBrokerProxy.
  BrowserInterfaceBrokerProxy* browser_interface_broker_;
  QuicTransport* quic_transport_;
  std::unique_ptr<StubQuicTransport> stub_;
  mojo::Remote<network::mojom::blink::QuicTransportClient> client_remote_;
  mojo::Receiver<mojom::blink::QuicTransportConnector> connector_receiver_{
      this};

  base::WeakPtrFactory<ScopedQuicTransport> weak_ptr_factory_{this};
};

// This test fragment is common to CreateLocallyAndWrite and
// CreateRemotelyAndWrite.
void TestWrite(const V8TestingScope& scope,
               ScopedQuicTransport* scoped_quic_transport,
               BidirectionalStream* bidirectional_stream) {
  auto* script_state = scope.GetScriptState();
  auto* writer = bidirectional_stream->writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().IsUndefined());

  mojo::ScopedDataPipeConsumerHandle& output_consumer =
      scoped_quic_transport->Stub()->OutputConsumer();
  const void* buffer = nullptr;
  uint32_t buffer_num_bytes = 0;
  MojoResult mojo_result = output_consumer->BeginReadData(
      &buffer, &buffer_num_bytes, MOJO_BEGIN_READ_DATA_FLAG_NONE);

  ASSERT_EQ(mojo_result, MOJO_RESULT_OK);
  EXPECT_EQ(buffer_num_bytes, 1u);
  EXPECT_EQ(reinterpret_cast<const char*>(buffer)[0], 'A');

  output_consumer->EndReadData(buffer_num_bytes);
}

TEST(BidirectionalStreamTest, CreateLocallyAndWrite) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  TestWrite(scope, &scoped_quic_transport, bidirectional_stream);
}

TEST(BidirectionalStreamTest, CreateRemotelyAndWrite) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.RemoteCreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  TestWrite(scope, &scoped_quic_transport, bidirectional_stream);
}

// This test fragment is common to CreateLocallyAndRead and
// CreateRemotelyAndRead.
void TestRead(const V8TestingScope& scope,
              ScopedQuicTransport* scoped_quic_transport,
              BidirectionalStream* bidirectional_stream) {
  mojo::ScopedDataPipeProducerHandle& input_producer =
      scoped_quic_transport->Stub()->InputProducer();
  constexpr char input[] = {'B'};
  uint32_t input_num_bytes = sizeof(input);
  MojoResult mojo_result = input_producer->WriteData(
      input, &input_num_bytes, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);

  ASSERT_EQ(mojo_result, MOJO_RESULT_OK);

  v8::Local<v8::Value> v8array =
      ReadValueFromStream(scope, bidirectional_stream->readable());
  DOMUint8Array* u8array =
      V8Uint8Array::ToImplWithTypeCheck(scope.GetIsolate(), v8array);
  ASSERT_TRUE(u8array);

  ASSERT_EQ(u8array->byteLengthAsSizeT(), 1u);
  EXPECT_EQ(reinterpret_cast<char*>(u8array->Data())[0], 'B');
}

TEST(BidirectionalStreamTest, CreateLocallyAndRead) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  TestRead(scope, &scoped_quic_transport, bidirectional_stream);
}

TEST(BidirectionalStreamTest, CreateRemotelyAndRead) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.RemoteCreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  TestRead(scope, &scoped_quic_transport, bidirectional_stream);
}

TEST(BidirectionalStreamTest, IncomingStreamCleanClose) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  scoped_quic_transport.GetQuicTransport()->OnIncomingStreamClosed(
      kDefaultStreamId, true);
  scoped_quic_transport.Stub()->InputProducer().reset();

  auto* script_state = scope.GetScriptState();
  auto* reader = bidirectional_stream->readable()->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);

  ScriptPromise read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester read_tester(script_state, read_promise);
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  v8::Local<v8::Value> result = read_tester.Value().V8Value();
  DCHECK(result->IsObject());
  v8::Local<v8::Value> v8value;
  bool done = false;
  EXPECT_TRUE(
      V8UnpackIteratorResult(script_state, result.As<v8::Object>(), &done)
          .ToLocal(&v8value));
  EXPECT_TRUE(done);

  ScriptPromiseTester tester(scope.GetScriptState(),
                             bidirectional_stream->writingAborted());
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST(BidirectionalStreamTest, IncomingStreamAbort) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  bidirectional_stream->abortReading(nullptr);

  ScriptPromiseTester tester(scope.GetScriptState(),
                             bidirectional_stream->writingAborted());
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST(BidirectionalStreamTest, OutgoingStreamAbort) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  bidirectional_stream->abortWriting(nullptr);

  ScriptPromiseTester tester(scope.GetScriptState(),
                             bidirectional_stream->readingAborted());
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  const auto* const stub = scoped_quic_transport.Stub();
  EXPECT_FALSE(stub->WasSendFinCalled());
  EXPECT_TRUE(stub->WasAbortStreamCalled());
}

TEST(BidirectionalStreamTest, OutgoingStreamCleanClose) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  auto* script_state = scope.GetScriptState();
  ScriptPromise close_promise = bidirectional_stream->writable()->close(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester close_tester(script_state, close_promise);
  close_tester.WaitUntilSettled();
  EXPECT_TRUE(close_tester.IsFulfilled());

  // The incoming side is closed by the network service.
  scoped_quic_transport.GetQuicTransport()->OnIncomingStreamClosed(
      kDefaultStreamId, false);
  scoped_quic_transport.Stub()->InputProducer().reset();

  ScriptPromiseTester tester(script_state,
                             bidirectional_stream->readingAborted());
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  const auto* const stub = scoped_quic_transport.Stub();
  EXPECT_TRUE(stub->WasSendFinCalled());
  EXPECT_FALSE(stub->WasAbortStreamCalled());
}

TEST(BidirectionalStreamTest, AbortBothOutgoingFirst) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  bidirectional_stream->abortWriting(nullptr);
  bidirectional_stream->abortReading(nullptr);

  ScriptPromiseTester reading_tester(scope.GetScriptState(),
                                     bidirectional_stream->readingAborted());
  reading_tester.WaitUntilSettled();
  EXPECT_TRUE(reading_tester.IsFulfilled());

  ScriptPromiseTester writing_tester(scope.GetScriptState(),
                                     bidirectional_stream->writingAborted());
  writing_tester.WaitUntilSettled();
  EXPECT_TRUE(writing_tester.IsFulfilled());
}

TEST(BidirectionalStreamTest, AbortBothIncomingFirst) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  bidirectional_stream->abortReading(nullptr);
  bidirectional_stream->abortWriting(nullptr);

  ScriptPromiseTester reading_tester(scope.GetScriptState(),
                                     bidirectional_stream->readingAborted());
  reading_tester.WaitUntilSettled();
  EXPECT_TRUE(reading_tester.IsFulfilled());

  ScriptPromiseTester writing_tester(scope.GetScriptState(),
                                     bidirectional_stream->writingAborted());
  writing_tester.WaitUntilSettled();
  EXPECT_TRUE(writing_tester.IsFulfilled());
}

TEST(BidirectionalStreamTest, CloseOutgoingThenAbortIncoming) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  // 1. Close outgoing.
  auto* script_state = scope.GetScriptState();
  ScriptPromise close_promise = bidirectional_stream->writable()->close(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester close_tester(script_state, close_promise);
  close_tester.WaitUntilSettled();
  EXPECT_TRUE(close_tester.IsFulfilled());

  // 2. Abort incoming.
  bidirectional_stream->abortReading(nullptr);

  // 3. The network service closes the incoming data pipe as a result of 1.
  scoped_quic_transport.GetQuicTransport()->OnIncomingStreamClosed(
      kDefaultStreamId, false);
  scoped_quic_transport.Stub()->InputProducer().reset();

  ScriptPromiseTester tester(script_state,
                             bidirectional_stream->readingAborted());
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST(BidirectionalStreamTest, AbortIncomingThenCloseOutgoing) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  // 1. Abort incoming.
  bidirectional_stream->abortReading(nullptr);

  // 2. Close outgoing. It should already have been closed when we aborted
  // reading, so this should be a no-op.
  auto* script_state = scope.GetScriptState();
  ScriptPromise close_promise = bidirectional_stream->writable()->close(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester close_tester(script_state, close_promise);
  close_tester.WaitUntilSettled();
  EXPECT_TRUE(close_tester.IsRejected());

  ScriptPromiseTester tester(script_state,
                             bidirectional_stream->writingAborted());
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST(BidirectionalStreamTest, CloseQuicTransport) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  scoped_quic_transport.GetQuicTransport()->close(nullptr);

  ScriptPromiseTester reading_tester(scope.GetScriptState(),
                                     bidirectional_stream->readingAborted());
  reading_tester.WaitUntilSettled();
  EXPECT_TRUE(reading_tester.IsFulfilled());

  ScriptPromiseTester writing_tester(scope.GetScriptState(),
                                     bidirectional_stream->writingAborted());
  writing_tester.WaitUntilSettled();
  EXPECT_TRUE(writing_tester.IsFulfilled());
}

TEST(BidirectionalStreamTest, RemoteDropQuicTransport) {
  V8TestingScope scope;
  ScopedQuicTransport scoped_quic_transport(scope);
  auto* bidirectional_stream =
      scoped_quic_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  scoped_quic_transport.ResetStub();

  ScriptPromiseTester reading_tester(scope.GetScriptState(),
                                     bidirectional_stream->readingAborted());
  reading_tester.WaitUntilSettled();
  EXPECT_TRUE(reading_tester.IsFulfilled());

  ScriptPromiseTester writing_tester(scope.GetScriptState(),
                                     bidirectional_stream->writingAborted());
  writing_tester.WaitUntilSettled();
  EXPECT_TRUE(writing_tester.IsFulfilled());
}

}  // namespace

}  // namespace blink
