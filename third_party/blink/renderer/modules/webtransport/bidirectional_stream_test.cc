// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/bidirectional_stream.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/web_transport.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_bidirectional_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/test_utils.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// These tests only ever create one stream at a time, so use a hardcoded stream
// id.
constexpr uint32_t kDefaultStreamId = 0;

// BidirectionalStream depends on blink::WebTransport. Rather than virtualise
// blink::WebTransport for these tests, we use a stub implementation of
// network::mojom::blink::WebTransport to get the behaviour we want. This class
// only supports the creation of one BidirectionalStream at a time for
// simplicity.
class StubWebTransport : public network::mojom::blink::WebTransport {
 public:
  explicit StubWebTransport(
      mojo::PendingReceiver<network::mojom::blink::WebTransport>
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

  // Implementation of WebTransport.
  void SendDatagram(base::span<const uint8_t> data,
                    base::OnceCallback<void(bool)>) override {
    NOTREACHED_IN_MIGRATION();
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

  void AbortStream(uint32_t stream_id, uint8_t code) override {
    EXPECT_EQ(stream_id, kDefaultStreamId);
    was_abort_stream_called_ = true;
  }

  void StopSending(uint32_t stream_id, uint8_t code) override {
    // TODO(ricea): Record that this was called when a test needs it.
  }

  void SetOutgoingDatagramExpirationDuration(base::TimeDelta) override {}

  void GetStats(GetStatsCallback callback) override {
    std::move(callback).Run(nullptr);
  }

  void Close(network::mojom::blink::WebTransportCloseInfoPtr) override {}

 private:
  base::OnceCallback<void(uint32_t,
                          mojo::ScopedDataPipeConsumerHandle,
                          mojo::ScopedDataPipeProducerHandle)>
      accept_callback_;
  base::OnceCallback<void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>
      ignored_unidirectional_stream_callback_;
  mojo::Receiver<network::mojom::blink::WebTransport> receiver_;
  mojo::ScopedDataPipeConsumerHandle output_consumer_;
  mojo::ScopedDataPipeProducerHandle input_producer_;
  bool was_send_fin_called_ = false;
  bool was_abort_stream_called_ = false;
};

// This class sets up a connected blink::WebTransport object using a
// StubWebTransport and provides access to both.
class ScopedWebTransport {
  STACK_ALLOCATED();

 public:
  // This constructor runs the event loop.
  explicit ScopedWebTransport(const V8TestingScope& scope) {
    creator_.Init(scope.GetScriptState(),
                  WTF::BindRepeating(&ScopedWebTransport::CreateStub,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  WebTransport* GetWebTransport() const { return creator_.GetWebTransport(); }
  StubWebTransport* Stub() const { return stub_.get(); }

  void ResetCreator() { creator_.Reset(); }
  void ResetStub() { stub_.reset(); }

  BidirectionalStream* CreateBidirectionalStream(const V8TestingScope& scope) {
    auto* script_state = scope.GetScriptState();
    ScriptPromiseUntyped bidirectional_stream_promise =
        GetWebTransport()->createBidirectionalStream(script_state,
                                                     ASSERT_NO_EXCEPTION);
    ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

    tester.WaitUntilSettled();

    EXPECT_TRUE(tester.IsFulfilled());
    auto* bidirectional_stream = V8WebTransportBidirectionalStream::ToWrappable(
        scope.GetIsolate(), tester.Value().V8Value());
    EXPECT_TRUE(bidirectional_stream);
    return bidirectional_stream;
  }

  BidirectionalStream* RemoteCreateBidirectionalStream(
      const V8TestingScope& scope) {
    stub_->CreateRemote();
    ReadableStream* streams = GetWebTransport()->incomingBidirectionalStreams();

    v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

    BidirectionalStream* bidirectional_stream =
        V8WebTransportBidirectionalStream::ToWrappable(scope.GetIsolate(),
                                                       v8value);
    EXPECT_TRUE(bidirectional_stream);

    return bidirectional_stream;
  }

 private:
  void CreateStub(mojo::PendingRemote<network::mojom::blink::WebTransport>&
                      web_transport_to_pass) {
    stub_ = std::make_unique<StubWebTransport>(
        web_transport_to_pass.InitWithNewPipeAndPassReceiver());
  }

  TestWebTransportCreator creator_;
  std::unique_ptr<StubWebTransport> stub_;

  base::WeakPtrFactory<ScopedWebTransport> weak_ptr_factory_{this};
};

// This test fragment is common to CreateLocallyAndWrite and
// CreateRemotelyAndWrite.
void TestWrite(const V8TestingScope& scope,
               ScopedWebTransport* scoped_web_transport,
               BidirectionalStream* bidirectional_stream) {
  auto* script_state = scope.GetScriptState();
  auto* writer = bidirectional_stream->writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().IsUndefined());

  mojo::ScopedDataPipeConsumerHandle& output_consumer =
      scoped_web_transport->Stub()->OutputConsumer();
  base::span<const uint8_t> buffer;
  MojoResult mojo_result =
      output_consumer->BeginReadData(MOJO_BEGIN_READ_DATA_FLAG_NONE, buffer);

  ASSERT_EQ(mojo_result, MOJO_RESULT_OK);
  EXPECT_EQ(buffer.size(), 1u);
  EXPECT_EQ(base::as_string_view(buffer), "A");

  output_consumer->EndReadData(buffer.size());
}

TEST(BidirectionalStreamTest, CreateLocallyAndWrite) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  TestWrite(scope, &scoped_web_transport, bidirectional_stream);
}

TEST(BidirectionalStreamTest, CreateRemotelyAndWrite) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.RemoteCreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  TestWrite(scope, &scoped_web_transport, bidirectional_stream);
}

// This test fragment is common to CreateLocallyAndRead and
// CreateRemotelyAndRead.
void TestRead(V8TestingScope& scope,
              ScopedWebTransport* scoped_web_transport,
              BidirectionalStream* bidirectional_stream) {
  mojo::ScopedDataPipeProducerHandle& input_producer =
      scoped_web_transport->Stub()->InputProducer();
  MojoResult mojo_result =
      input_producer->WriteAllData(base::as_byte_span(std::string_view("B")));

  ASSERT_EQ(mojo_result, MOJO_RESULT_OK);

  v8::Local<v8::Value> v8array =
      ReadValueFromStream(scope, bidirectional_stream->readable());
  NotShared<DOMUint8Array> u8array =
      NativeValueTraits<NotShared<DOMUint8Array>>::NativeValue(
          scope.GetIsolate(), v8array, scope.GetExceptionState());
  ASSERT_TRUE(u8array);

  ASSERT_EQ(u8array->byteLength(), 1u);
  EXPECT_EQ(reinterpret_cast<char*>(u8array->Data())[0], 'B');
}

TEST(BidirectionalStreamTest, CreateLocallyAndRead) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  TestRead(scope, &scoped_web_transport, bidirectional_stream);
}

TEST(BidirectionalStreamTest, CreateRemotelyAndRead) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.RemoteCreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  TestRead(scope, &scoped_web_transport, bidirectional_stream);
}

TEST(BidirectionalStreamTest, IncomingStreamCleanClose) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  scoped_web_transport.GetWebTransport()->OnIncomingStreamClosed(
      kDefaultStreamId, true);
  scoped_web_transport.Stub()->InputProducer().reset();

  auto* script_state = scope.GetScriptState();
  auto* reader = bidirectional_stream->readable()->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseUntyped read_promise =
      reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester read_tester(script_state, read_promise);
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  v8::Local<v8::Value> result = read_tester.Value().V8Value();
  DCHECK(result->IsObject());
  v8::Local<v8::Value> v8value;
  bool done = false;
  EXPECT_TRUE(V8UnpackIterationResult(script_state, result.As<v8::Object>(),
                                      &v8value, &done));
  EXPECT_TRUE(done);
}

TEST(BidirectionalStreamTest, OutgoingStreamCleanClose) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped close_promise = bidirectional_stream->writable()->close(
      script_state, ASSERT_NO_EXCEPTION);

  scoped_web_transport.GetWebTransport()->OnOutgoingStreamClosed(
      kDefaultStreamId);

  ScriptPromiseTester close_tester(script_state, close_promise);
  close_tester.WaitUntilSettled();
  EXPECT_TRUE(close_tester.IsFulfilled());

  // The incoming side is closed by the network service.
  scoped_web_transport.GetWebTransport()->OnIncomingStreamClosed(
      kDefaultStreamId, false);
  scoped_web_transport.Stub()->InputProducer().reset();

  const auto* const stub = scoped_web_transport.Stub();
  EXPECT_TRUE(stub->WasSendFinCalled());
  EXPECT_FALSE(stub->WasAbortStreamCalled());
}

TEST(BidirectionalStreamTest, CloseWebTransport) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  scoped_web_transport.GetWebTransport()->close(nullptr);

  EXPECT_TRUE(bidirectional_stream->readable()->IsErrored());
  EXPECT_TRUE(bidirectional_stream->writable()->IsErrored());
}

TEST(BidirectionalStreamTest, RemoteDropWebTransport) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  scoped_web_transport.ResetCreator();

  test::RunPendingTasks();

  EXPECT_TRUE(bidirectional_stream->readable()->IsErrored());
  EXPECT_TRUE(bidirectional_stream->writable()->IsErrored());
}

TEST(BidirectionalStreamTest, WriteAfterCancellingIncoming) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped cancel_promise =
      bidirectional_stream->readable()->cancel(script_state,
                                               ASSERT_NO_EXCEPTION);
  ScriptPromiseTester cancel_tester(script_state, cancel_promise);
  cancel_tester.WaitUntilSettled();
  EXPECT_TRUE(cancel_tester.IsFulfilled());

  TestWrite(scope, &scoped_web_transport, bidirectional_stream);

  scoped_web_transport.ResetStub();
  test::RunPendingTasks();
}

TEST(BidirectionalStreamTest, WriteAfterIncomingClosed) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  scoped_web_transport.GetWebTransport()->OnIncomingStreamClosed(
      kDefaultStreamId, true);
  scoped_web_transport.Stub()->InputProducer().reset();

  test::RunPendingTasks();

  TestWrite(scope, &scoped_web_transport, bidirectional_stream);

  scoped_web_transport.ResetStub();
  test::RunPendingTasks();
}

TEST(BidirectionalStreamTest, ReadAfterClosingOutgoing) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped close_promise = bidirectional_stream->writable()->close(
      script_state, ASSERT_NO_EXCEPTION);

  scoped_web_transport.GetWebTransport()->OnOutgoingStreamClosed(
      kDefaultStreamId);

  ScriptPromiseTester close_tester(script_state, close_promise);
  close_tester.WaitUntilSettled();
  EXPECT_TRUE(close_tester.IsFulfilled());

  TestRead(scope, &scoped_web_transport, bidirectional_stream);
}

TEST(BidirectionalStreamTest, ReadAfterAbortingOutgoing) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped abort_promise = bidirectional_stream->writable()->abort(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester abort_tester(script_state, abort_promise);
  abort_tester.WaitUntilSettled();
  EXPECT_TRUE(abort_tester.IsFulfilled());

  TestRead(scope, &scoped_web_transport, bidirectional_stream);
}

TEST(BidirectionalStreamTest, ReadAfterOutgoingAborted) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedWebTransport scoped_web_transport(scope);
  auto* bidirectional_stream =
      scoped_web_transport.CreateBidirectionalStream(scope);
  ASSERT_TRUE(bidirectional_stream);

  scoped_web_transport.Stub()->OutputConsumer().reset();
  test::RunPendingTasks();

  TestRead(scope, &scoped_web_transport, bidirectional_stream);
}

}  // namespace

}  // namespace blink
