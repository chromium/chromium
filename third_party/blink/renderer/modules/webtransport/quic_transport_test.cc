// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"

#include <array>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/quic_transport.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bidirectional_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_quic_transport_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_receive_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_dtls_fingerprint.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_send_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_close_info.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/receive_stream.h"
#include "third_party/blink/renderer/modules/webtransport/send_stream.h"
#include "third_party/blink/renderer/modules/webtransport/test_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::Truly;
using ::testing::Unused;

class QuicTransportConnector final
    : public mojom::blink::QuicTransportConnector {
 public:
  struct ConnectArgs {
    ConnectArgs(
        const KURL& url,
        Vector<network::mojom::blink::QuicTransportCertificateFingerprintPtr>
            fingerprints,
        mojo::PendingRemote<network::mojom::blink::QuicTransportHandshakeClient>
            handshake_client)
        : url(url),
          fingerprints(std::move(fingerprints)),
          handshake_client(std::move(handshake_client)) {}

    KURL url;
    Vector<network::mojom::blink::QuicTransportCertificateFingerprintPtr>
        fingerprints;
    mojo::PendingRemote<network::mojom::blink::QuicTransportHandshakeClient>
        handshake_client;
  };

  void Connect(
      const KURL& url,
      Vector<network::mojom::blink::QuicTransportCertificateFingerprintPtr>
          fingerprints,
      mojo::PendingRemote<network::mojom::blink::QuicTransportHandshakeClient>
          handshake_client) override {
    connect_args_.push_back(
        ConnectArgs(url, std::move(fingerprints), std::move(handshake_client)));
  }

  Vector<ConnectArgs> TakeConnectArgs() { return std::move(connect_args_); }

  void Bind(
      mojo::PendingReceiver<mojom::blink::QuicTransportConnector> receiver) {
    receiver_set_.Add(this, std::move(receiver));
  }

 private:
  mojo::ReceiverSet<mojom::blink::QuicTransportConnector> receiver_set_;
  Vector<ConnectArgs> connect_args_;
};

class MockQuicTransport : public network::mojom::blink::QuicTransport {
 public:
  MockQuicTransport(mojo::PendingReceiver<network::mojom::blink::QuicTransport>
                        pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  MOCK_METHOD2(SendDatagram,
               void(base::span<const uint8_t> data,
                    base::OnceCallback<void(bool)> callback));

  MOCK_METHOD3(CreateStream,
               void(mojo::ScopedDataPipeConsumerHandle readable,
                    mojo::ScopedDataPipeProducerHandle writable,
                    base::OnceCallback<void(bool, uint32_t)> callback));

  MOCK_METHOD1(
      AcceptBidirectionalStream,
      void(base::OnceCallback<void(uint32_t,
                                   mojo::ScopedDataPipeConsumerHandle,
                                   mojo::ScopedDataPipeProducerHandle)>));

  MOCK_METHOD1(AcceptUnidirectionalStream,
               void(base::OnceCallback<
                    void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>));

  MOCK_METHOD1(SetOutgoingDatagramExpirationDuration, void(base::TimeDelta));

  void SendFin(uint32_t stream_id) override {}
  void AbortStream(uint32_t stream_id, uint64_t code) override {}

 private:
  mojo::Receiver<network::mojom::blink::QuicTransport> receiver_;
};

class QuicTransportTest : public ::testing::Test {
 public:
  using AcceptUnidirectionalStreamCallback =
      base::OnceCallback<void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>;
  using AcceptBidirectionalStreamCallback =
      base::OnceCallback<void(uint32_t,
                              mojo::ScopedDataPipeConsumerHandle,
                              mojo::ScopedDataPipeProducerHandle)>;

  void AddBinder(const V8TestingScope& scope) {
    interface_broker_ =
        &scope.GetExecutionContext()->GetBrowserInterfaceBroker();
    interface_broker_->SetBinderForTesting(
        mojom::blink::QuicTransportConnector::Name_,
        base::BindRepeating(&QuicTransportTest::BindConnector,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  static QuicTransportOptions* EmptyOptions() {
    return MakeGarbageCollected<QuicTransportOptions>();
  }

  // Creates a QuicTransport object with the given |url|.
  QuicTransport* Create(const V8TestingScope& scope,
                        const String& url,
                        QuicTransportOptions* options) {
    AddBinder(scope);
    return QuicTransport::Create(scope.GetScriptState(), url, options,
                                 ASSERT_NO_EXCEPTION);
  }

  // Connects a QuicTransport object. Runs the event loop.
  void ConnectSuccessfully(QuicTransport* quic_transport) {
    DCHECK(!mock_quic_transport_) << "Only one connection supported, sorry";

    test::RunPendingTasks();

    auto args = connector_.TakeConnectArgs();
    if (args.size() != 1u) {
      ADD_FAILURE() << "args.size() should be 1, but is " << args.size();
      return;
    }

    mojo::Remote<network::mojom::blink::QuicTransportHandshakeClient>
        handshake_client(std::move(args[0].handshake_client));

    mojo::PendingRemote<network::mojom::blink::QuicTransport>
        quic_transport_to_pass;
    mojo::PendingRemote<network::mojom::blink::QuicTransportClient>
        client_remote;

    mock_quic_transport_ = std::make_unique<StrictMock<MockQuicTransport>>(
        quic_transport_to_pass.InitWithNewPipeAndPassReceiver());

    // These are called on every connection, so expect them in every test.
    EXPECT_CALL(*mock_quic_transport_, AcceptUnidirectionalStream(_))
        .WillRepeatedly([this](AcceptUnidirectionalStreamCallback callback) {
          pending_unidirectional_accept_callbacks_.push_back(
              std::move(callback));
        });

    EXPECT_CALL(*mock_quic_transport_, AcceptBidirectionalStream(_))
        .WillRepeatedly([this](AcceptBidirectionalStreamCallback callback) {
          pending_bidirectional_accept_callbacks_.push_back(
              std::move(callback));
        });

    handshake_client->OnConnectionEstablished(
        std::move(quic_transport_to_pass),
        client_remote.InitWithNewPipeAndPassReceiver());
    client_remote_.Bind(std::move(client_remote));

    test::RunPendingTasks();
  }

  // Creates, connects and returns a QuicTransport object with the given |url|.
  // Runs the event loop.
  QuicTransport* CreateAndConnectSuccessfully(
      const V8TestingScope& scope,
      const String& url,
      QuicTransportOptions* options = EmptyOptions()) {
    auto* quic_transport = Create(scope, url, options);
    ConnectSuccessfully(quic_transport);
    return quic_transport;
  }

  SendStream* CreateSendStreamSuccessfully(const V8TestingScope& scope,
                                           QuicTransport* quic_transport) {
    EXPECT_CALL(*mock_quic_transport_, CreateStream(_, _, _))
        .WillOnce([this](mojo::ScopedDataPipeConsumerHandle handle, Unused,
                         base::OnceCallback<void(bool, uint32_t)> callback) {
          send_stream_consumer_handle_ = std::move(handle);
          std::move(callback).Run(true, next_stream_id_++);
        });

    auto* script_state = scope.GetScriptState();
    ScriptPromise send_stream_promise =
        quic_transport->createSendStream(script_state, ASSERT_NO_EXCEPTION);
    ScriptPromiseTester tester(script_state, send_stream_promise);

    tester.WaitUntilSettled();

    EXPECT_TRUE(tester.IsFulfilled());
    auto* send_stream = V8SendStream::ToImplWithTypeCheck(
        scope.GetIsolate(), tester.Value().V8Value());
    EXPECT_TRUE(send_stream);
    return send_stream;
  }

  mojo::ScopedDataPipeProducerHandle DoAcceptUnidirectionalStream() {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;

    // There's no good way to handle failure to create the pipe, so just
    // continue.
    CreateDataPipeForWebTransportTests(&producer, &consumer);

    std::move(pending_unidirectional_accept_callbacks_.front())
        .Run(next_stream_id_++, std::move(consumer));
    pending_unidirectional_accept_callbacks_.pop_front();

    return producer;
  }

  ReceiveStream* ReadReceiveStream(const V8TestingScope& scope,
                                   QuicTransport* quic_transport) {
    ReadableStream* streams = quic_transport->receiveStreams();

    v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

    ReceiveStream* receive_stream =
        V8ReceiveStream::ToImplWithTypeCheck(scope.GetIsolate(), v8value);
    EXPECT_TRUE(receive_stream);

    return receive_stream;
  }

  void BindConnector(mojo::ScopedMessagePipeHandle handle) {
    connector_.Bind(mojo::PendingReceiver<mojom::blink::QuicTransportConnector>(
        std::move(handle)));
  }

  void TearDown() override {
    if (!interface_broker_)
      return;
    interface_broker_->SetBinderForTesting(
        mojom::blink::QuicTransportConnector::Name_, {});
  }

  BrowserInterfaceBrokerProxy* interface_broker_ = nullptr;
  WTF::Deque<AcceptUnidirectionalStreamCallback>
      pending_unidirectional_accept_callbacks_;
  WTF::Deque<AcceptBidirectionalStreamCallback>
      pending_bidirectional_accept_callbacks_;
  QuicTransportConnector connector_;
  std::unique_ptr<MockQuicTransport> mock_quic_transport_;
  mojo::Remote<network::mojom::blink::QuicTransportClient> client_remote_;
  uint32_t next_stream_id_ = 0;
  mojo::ScopedDataPipeConsumerHandle send_stream_consumer_handle_;

  base::WeakPtrFactory<QuicTransportTest> weak_ptr_factory_{this};
};

TEST_F(QuicTransportTest, FailWithNullURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String(), EmptyOptions(),
                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
}

TEST_F(QuicTransportTest, FailWithEmptyURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String(""), EmptyOptions(),
                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL '' is invalid.", exception_state.Message());
}

TEST_F(QuicTransportTest, FailWithNoScheme) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String("no-scheme"),
                        EmptyOptions(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL 'no-scheme' is invalid.", exception_state.Message());
}

TEST_F(QuicTransportTest, FailWithHttpsURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String("https://example.com/"),
                        EmptyOptions(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ(
      "The URL's scheme must be 'quic-transport'. 'https' is not allowed.",
      exception_state.Message());
}

TEST_F(QuicTransportTest, FailWithNoHost) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String("quic-transport:///"),
                        EmptyOptions(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL 'quic-transport:///' is invalid.",
            exception_state.Message());
}

TEST_F(QuicTransportTest, FailWithURLFragment) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(),
                        String("quic-transport://example.com/#failing"),
                        EmptyOptions(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ(
      "The URL contains a fragment identifier ('#failing'). Fragment "
      "identifiers are not allowed in QuicTransport URLs.",
      exception_state.Message());
}

TEST_F(QuicTransportTest, FailByCSP) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  scope.GetExecutionContext()
      ->GetContentSecurityPolicyForCurrentWorld()
      ->DidReceiveHeader("connect-src 'none'",
                         network::mojom::ContentSecurityPolicyType::kEnforce,
                         network::mojom::ContentSecurityPolicySource::kHTTP);
  QuicTransport::Create(scope.GetScriptState(),
                        String("quic-transport://example.com/"), EmptyOptions(),
                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSecurityError),
            exception_state.Code());
  EXPECT_EQ("Failed to connect to 'quic-transport://example.com/'",
            exception_state.Message());
}

TEST_F(QuicTransportTest, PassCSP) {
  V8TestingScope scope;
  // This doesn't work without the https:// prefix, even thought it should
  // according to
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Security-Policy/connect-src.
  auto& exception_state = scope.GetExceptionState();
  scope.GetExecutionContext()
      ->GetContentSecurityPolicyForCurrentWorld()
      ->DidReceiveHeader("connect-src quic-transport://example.com",
                         network::mojom::ContentSecurityPolicyType::kEnforce,
                         network::mojom::ContentSecurityPolicySource::kHTTP);
  QuicTransport::Create(scope.GetScriptState(),
                        String("quic-transport://example.com/"), EmptyOptions(),
                        exception_state);
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(QuicTransportTest, SendConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* quic_transport = QuicTransport::Create(
      scope.GetScriptState(), String("quic-transport://example.com/"),
      EmptyOptions(), ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  EXPECT_EQ(KURL("quic-transport://example.com/"), args[0].url);
  EXPECT_TRUE(args[0].fingerprints.IsEmpty());
  EXPECT_TRUE(quic_transport->HasPendingActivity());
}

TEST_F(QuicTransportTest, SuccessfulConnect) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
  ScriptPromiseTester ready_tester(scope.GetScriptState(),
                                   quic_transport->ready());

  EXPECT_TRUE(quic_transport->HasPendingActivity());

  ready_tester.WaitUntilSettled();
  EXPECT_TRUE(ready_tester.IsFulfilled());
}

TEST_F(QuicTransportTest, FailedConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* quic_transport = QuicTransport::Create(
      scope.GetScriptState(), String("quic-transport://example.com/"),
      EmptyOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester ready_tester(scope.GetScriptState(),
                                   quic_transport->ready());
  ScriptPromiseTester closed_tester(scope.GetScriptState(),
                                    quic_transport->closed());

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());

  mojo::Remote<network::mojom::blink::QuicTransportHandshakeClient>
      handshake_client(std::move(args[0].handshake_client));

  handshake_client->OnHandshakeFailed(nullptr);

  test::RunPendingTasks();
  EXPECT_FALSE(quic_transport->HasPendingActivity());
  EXPECT_TRUE(ready_tester.IsRejected());
  EXPECT_TRUE(closed_tester.IsRejected());
}

TEST_F(QuicTransportTest, SendConnectWithFingerprint) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* fingerprints = MakeGarbageCollected<RTCDtlsFingerprint>();
  fingerprints->setAlgorithm("sha-256");
  fingerprints->setValue(
      "ED:3D:D7:C3:67:10:94:68:D1:DC:D1:26:5C:B2:74:D7:1C:A2:63:3E:94:94:C0:84:"
      "39:D6:64:FA:08:B9:77:37");
  auto* options = MakeGarbageCollected<QuicTransportOptions>();
  options->setServerCertificateFingerprints({fingerprints});
  QuicTransport::Create(scope.GetScriptState(),
                        String("quic-transport://example.com/"), options,
                        ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  ASSERT_EQ(1u, args[0].fingerprints.size());
  EXPECT_EQ(args[0].fingerprints[0]->algorithm, "sha-256");
  EXPECT_EQ(args[0].fingerprints[0]->fingerprint,
            "ED:3D:D7:C3:67:10:94:68:D1:DC:D1:26:5C:B2:74:D7:1C:A2:63:3E:94:94:"
            "C0:84:39:D6:64:FA:08:B9:77:37");
}

TEST_F(QuicTransportTest, CloseDuringConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* quic_transport = QuicTransport::Create(
      scope.GetScriptState(), String("quic-transport://example.com/"),
      EmptyOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester ready_tester(scope.GetScriptState(),
                                   quic_transport->ready());
  ScriptPromiseTester closed_tester(scope.GetScriptState(),
                                    quic_transport->closed());

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());

  quic_transport->close(nullptr);

  test::RunPendingTasks();

  EXPECT_FALSE(quic_transport->HasPendingActivity());
  EXPECT_TRUE(ready_tester.IsRejected());
  EXPECT_TRUE(closed_tester.IsFulfilled());
}

TEST_F(QuicTransportTest, CloseAfterConnection) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
  ScriptPromiseTester ready_tester(scope.GetScriptState(),
                                   quic_transport->ready());
  ScriptPromiseTester closed_tester(scope.GetScriptState(),
                                    quic_transport->closed());

  WebTransportCloseInfo close_info;
  close_info.setErrorCode(42);
  close_info.setReason("because");
  quic_transport->close(&close_info);

  test::RunPendingTasks();

  // TODO(ricea): Check that the close info is sent through correctly, once we
  // start sending it.

  EXPECT_FALSE(quic_transport->HasPendingActivity());
  EXPECT_TRUE(ready_tester.IsFulfilled());
  EXPECT_TRUE(closed_tester.IsFulfilled());

  // Calling close again does nothing.
  quic_transport->close(nullptr);
}

// A live connection will be kept alive even if there is no explicit reference.
// When the underlying connection is shut down, the connection will be swept.
TEST_F(QuicTransportTest, GarbageCollection) {
  V8TestingScope scope;

  WeakPersistent<QuicTransport> quic_transport;

  {
    // The streams created when creating a QuicTransport create some v8 handles.
    // To ensure these are collected, we need to create a handle scope. This is
    // not a problem for garbage collection in normal operation.
    v8::HandleScope handle_scope(scope.GetIsolate());
    quic_transport =
        CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
  }

  // Pretend the stack is empty. This will avoid accidentally treating any
  // copies of the |quic_transport| pointer as references.
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_TRUE(quic_transport);

  quic_transport->close(nullptr);

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(quic_transport);
}

TEST_F(QuicTransportTest, GarbageCollectMojoConnectionError) {
  V8TestingScope scope;

  WeakPersistent<QuicTransport> quic_transport;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());
    quic_transport =
        CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
  }

  ScriptPromiseTester closed_tester(scope.GetScriptState(),
                                    quic_transport->closed());

  // Closing the server-side of the pipe causes a mojo connection error.
  client_remote_.reset();

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(quic_transport);
  EXPECT_TRUE(closed_tester.IsRejected());
}

TEST_F(QuicTransportTest, SendDatagram) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  EXPECT_CALL(*mock_quic_transport_, SendDatagram(ElementsAre('A'), _))
      .WillOnce(Invoke([](base::span<const uint8_t>,
                          MockQuicTransport::SendDatagramCallback callback) {
        std::move(callback).Run(true);
      }));

  auto* writable = quic_transport->sendDatagrams();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().IsUndefined());
}

TEST_F(QuicTransportTest, BackpressureForOutgoingDatagrams) {
  V8TestingScope scope;
  auto* const options = MakeGarbageCollected<QuicTransportOptions>();
  options->setDatagramWritableHighWaterMark(3);
  auto* quic_transport = CreateAndConnectSuccessfully(
      scope, "quic-transport://example.com", options);

  EXPECT_CALL(*mock_quic_transport_, SendDatagram(_, _))
      .Times(4)
      .WillRepeatedly(
          Invoke([](base::span<const uint8_t>,
                    MockQuicTransport::SendDatagramCallback callback) {
            std::move(callback).Run(true);
          }));

  auto* writable = quic_transport->sendDatagrams();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromise promise1;
  ScriptPromise promise2;
  ScriptPromise promise3;
  ScriptPromise promise4;

  {
    auto* chunk = DOMUint8Array::Create(1);
    *chunk->Data() = 'A';
    promise1 =
        writer->write(script_state, ScriptValue::From(script_state, chunk),
                      ASSERT_NO_EXCEPTION);
  }
  {
    auto* chunk = DOMUint8Array::Create(1);
    *chunk->Data() = 'B';
    promise2 =
        writer->write(script_state, ScriptValue::From(script_state, chunk),
                      ASSERT_NO_EXCEPTION);
  }
  {
    auto* chunk = DOMUint8Array::Create(1);
    *chunk->Data() = 'C';
    promise3 =
        writer->write(script_state, ScriptValue::From(script_state, chunk),
                      ASSERT_NO_EXCEPTION);
  }
  {
    auto* chunk = DOMUint8Array::Create(1);
    *chunk->Data() = 'D';
    promise4 =
        writer->write(script_state, ScriptValue::From(script_state, chunk),
                      ASSERT_NO_EXCEPTION);
  }

  // The first two promises are resolved immediately.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_EQ(promise1.V8Promise()->State(), v8::Promise::kFulfilled);
  EXPECT_EQ(promise2.V8Promise()->State(), v8::Promise::kFulfilled);
  EXPECT_EQ(promise3.V8Promise()->State(), v8::Promise::kPending);
  EXPECT_EQ(promise4.V8Promise()->State(), v8::Promise::kPending);

  // The rest are resolved by the callback.
  test::RunPendingTasks();
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_EQ(promise3.V8Promise()->State(), v8::Promise::kFulfilled);
  EXPECT_EQ(promise4.V8Promise()->State(), v8::Promise::kFulfilled);
}

TEST_F(QuicTransportTest, SendDatagramBeforeConnect) {
  V8TestingScope scope;
  auto* quic_transport =
      Create(scope, "quic-transport://example.com", EmptyOptions());

  auto* writable = quic_transport->sendDatagrams();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);

  ConnectSuccessfully(quic_transport);

  // No datagram is sent.

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().IsUndefined());
}

TEST_F(QuicTransportTest, SendDatagramAfterClose) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  quic_transport->close(nullptr);
  test::RunPendingTasks();

  auto* writable = quic_transport->sendDatagrams();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);

  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);

  // No datagram is sent.

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

Vector<uint8_t> GetValueAsVector(ScriptState* script_state,
                                 ScriptValue iterator_result) {
  bool done = false;
  v8::Local<v8::Value> value;
  if (!V8UnpackIteratorResult(script_state,
                              iterator_result.V8Value().As<v8::Object>(), &done)
           .ToLocal(&value)) {
    ADD_FAILURE() << "unable to unpack iterator_result";
    return {};
  }

  EXPECT_FALSE(done);
  auto* array =
      V8Uint8Array::ToImplWithTypeCheck(script_state->GetIsolate(), value);
  if (!array) {
    ADD_FAILURE() << "value was not a Uint8Array";
    return {};
  }

  Vector<uint8_t> result;
  result.Append(array->Data(), array->length());
  return result;
}

TEST_F(QuicTransportTest, ReceiveDatagramBeforeRead) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  const std::array<uint8_t, 1> chunk = {'A'};
  client_remote_->OnDatagramReceived(chunk);

  test::RunPendingTasks();

  auto* readable = quic_transport->receiveDatagrams();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise result = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester.Value()), ElementsAre('A'));
}

TEST_F(QuicTransportTest, ReceiveDatagramDuringRead) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
  auto* readable = quic_transport->receiveDatagrams();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise result = reader->read(script_state, ASSERT_NO_EXCEPTION);

  const std::array<uint8_t, 1> chunk = {'A'};
  client_remote_->OnDatagramReceived(chunk);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester.Value()), ElementsAre('A'));
}

// This test documents the current behaviour. If you improve the behaviour,
// change the test!
TEST_F(QuicTransportTest, DatagramsAreDropped) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  // Chunk 'A' gets placed in the readable queue.
  const std::array<uint8_t, 1> chunk1 = {'A'};
  client_remote_->OnDatagramReceived(chunk1);

  // Chunk 'B' gets dropped, because there is no space in the readable queue.
  const std::array<uint8_t, 1> chunk2 = {'B'};
  client_remote_->OnDatagramReceived(chunk2);

  // Make sure that the calls have run.
  test::RunPendingTasks();

  auto* readable = quic_transport->receiveDatagrams();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise result1 = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise result2 = reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester1(script_state, result1);
  ScriptPromiseTester tester2(script_state, result2);
  tester1.WaitUntilSettled();
  EXPECT_TRUE(tester1.IsFulfilled());
  EXPECT_FALSE(tester2.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester1.Value()),
              ElementsAre('A'));

  // Chunk 'C' fulfills the pending read.
  const std::array<uint8_t, 1> chunk3 = {'C'};
  client_remote_->OnDatagramReceived(chunk3);

  tester2.WaitUntilSettled();
  EXPECT_TRUE(tester2.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester2.Value()),
              ElementsAre('C'));
}

bool ValidProducerHandle(const mojo::ScopedDataPipeProducerHandle& handle) {
  return handle.is_valid();
}

bool ValidConsumerHandle(const mojo::ScopedDataPipeConsumerHandle& handle) {
  return handle.is_valid();
}

TEST_F(QuicTransportTest, CreateSendStream) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  EXPECT_CALL(*mock_quic_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Not(Truly(ValidProducerHandle)), _))
      .WillOnce([](Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  ScriptPromise send_stream_promise =
      quic_transport->createSendStream(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* send_stream = V8SendStream::ToImplWithTypeCheck(
      scope.GetIsolate(), tester.Value().V8Value());
  EXPECT_TRUE(send_stream);
}

TEST_F(QuicTransportTest, CreateSendStreamBeforeConnect) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* quic_transport =
      QuicTransport::Create(script_state, "quic-transport://example.com",
                            EmptyOptions(), ASSERT_NO_EXCEPTION);
  auto& exception_state = scope.GetExceptionState();
  ScriptPromise send_stream_promise =
      quic_transport->createSendStream(script_state, exception_state);
  EXPECT_TRUE(send_stream_promise.IsEmpty());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kNetworkError),
            exception_state.Code());
}

TEST_F(QuicTransportTest, CreateSendStreamFailure) {
  V8TestingScope scope;
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  EXPECT_CALL(*mock_quic_transport_, CreateStream(_, _, _))
      .WillOnce([](Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(false, 0);
      });

  auto* script_state = scope.GetScriptState();
  ScriptPromise send_stream_promise =
      quic_transport->createSendStream(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
  DOMException* exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), tester.Value().V8Value());
  EXPECT_EQ(exception->name(), "NetworkError");
  EXPECT_EQ(exception->message(), "Failed to create send stream.");
}

// Every active stream is kept alive by the QuicTransport object.
TEST_F(QuicTransportTest, SendStreamGarbageCollection) {
  V8TestingScope scope;

  WeakPersistent<QuicTransport> quic_transport;
  WeakPersistent<SendStream> send_stream;

  {
    // The streams created when creating a QuicTransport or SendStream create
    // some v8 handles. To ensure these are collected, we need to create a
    // handle scope. This is not a problem for garbage collection in normal
    // operation.
    v8::HandleScope handle_scope(scope.GetIsolate());

    quic_transport =
        CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
    send_stream = CreateSendStreamSuccessfully(scope, quic_transport);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_TRUE(quic_transport);
  EXPECT_TRUE(send_stream);

  quic_transport->close(nullptr);

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(quic_transport);
  EXPECT_FALSE(send_stream);
}

// A live stream will be kept alive even if there is no explicit reference.
// When the underlying connection is shut down, the connection will be swept.
TEST_F(QuicTransportTest, SendStreamGarbageCollectionLocalClose) {
  V8TestingScope scope;

  WeakPersistent<SendStream> send_stream;

  {
    // The writable stream created when creating a SendStream creates some
    // v8 handles. To ensure these are collected, we need to create a handle
    // scope. This is not a problem for garbage collection in normal operation.
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* quic_transport =
        CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
    send_stream = CreateSendStreamSuccessfully(scope, quic_transport);
  }

  // Pretend the stack is empty. This will avoid accidentally treating any
  // copies of the |send_stream| pointer as references.
  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(send_stream);

  auto* script_state = scope.GetScriptState();

  ScriptPromise close_promise =
      send_stream->writable()->close(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, close_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(send_stream);
}

TEST_F(QuicTransportTest, SendStreamGarbageCollectionRemoteClose) {
  V8TestingScope scope;

  WeakPersistent<SendStream> send_stream;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* quic_transport =
        CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
    send_stream = CreateSendStreamSuccessfully(scope, quic_transport);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(send_stream);

  // Close the other end of the pipe.
  send_stream_consumer_handle_.reset();

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(send_stream);
}

// A live stream will be kept alive even if there is no explicit reference.
// When the underlying connection is shut down, the connection will be swept.
TEST_F(QuicTransportTest, ReceiveStreamGarbageCollectionCancel) {
  V8TestingScope scope;

  WeakPersistent<ReceiveStream> receive_stream;
  mojo::ScopedDataPipeProducerHandle producer;

  {
    // The readable stream created when creating a ReceiveStream creates some
    // v8 handles. To ensure these are collected, we need to create a handle
    // scope. This is not a problem for garbage collection in normal operation.
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* quic_transport =
        CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

    producer = DoAcceptUnidirectionalStream();
    receive_stream = ReadReceiveStream(scope, quic_transport);
  }

  // Pretend the stack is empty. This will avoid accidentally treating any
  // copies of the |receive_stream| pointer as references.
  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  auto* script_state = scope.GetScriptState();

  ScriptPromise cancel_promise;
  {
    // Cancelling also creates v8 handles, so we need a new handle scope as
    // above.
    v8::HandleScope handle_scope(scope.GetIsolate());
    cancel_promise =
        receive_stream->readable()->cancel(script_state, ASSERT_NO_EXCEPTION);
  }

  ScriptPromiseTester tester(script_state, cancel_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(receive_stream);
}

TEST_F(QuicTransportTest, ReceiveStreamGarbageCollectionRemoteClose) {
  V8TestingScope scope;

  WeakPersistent<ReceiveStream> receive_stream;
  mojo::ScopedDataPipeProducerHandle producer;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* quic_transport =
        CreateAndConnectSuccessfully(scope, "quic-transport://example.com");
    producer = DoAcceptUnidirectionalStream();
    receive_stream = ReadReceiveStream(scope, quic_transport);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  // Close the other end of the pipe.
  producer.reset();

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  receive_stream->OnIncomingStreamClosed(false);

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(receive_stream);
}

// This is the same test as ReceiveStreamGarbageCollectionRemoteClose, except
// that the order of the data pipe being reset and the OnIncomingStreamClosed
// message is reversed. It is important that the object is not collected until
// both events have happened.
TEST_F(QuicTransportTest, ReceiveStreamGarbageCollectionRemoteCloseReverse) {
  V8TestingScope scope;

  WeakPersistent<ReceiveStream> receive_stream;
  mojo::ScopedDataPipeProducerHandle producer;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());

    QuicTransport* quic_transport =
        CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

    producer = DoAcceptUnidirectionalStream();
    receive_stream = ReadReceiveStream(scope, quic_transport);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  receive_stream->OnIncomingStreamClosed(false);

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  producer.reset();

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(receive_stream);
}

TEST_F(QuicTransportTest, CreateSendStreamAbortedByClose) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  base::OnceCallback<void(bool, uint32_t)> create_stream_callback;
  EXPECT_CALL(*mock_quic_transport_, CreateStream(_, _, _))
      .WillOnce([&](Unused, Unused,
                    base::OnceCallback<void(bool, uint32_t)> callback) {
        create_stream_callback = std::move(callback);
      });

  ScriptPromise send_stream_promise =
      quic_transport->createSendStream(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  test::RunPendingTasks();

  quic_transport->close(nullptr);
  std::move(create_stream_callback).Run(true, 0);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
}

// ReceiveStream functionality is thoroughly tested in incoming_stream_test.cc.
// This test just verifies that the creation is done correctly.
TEST_F(QuicTransportTest, CreateReceiveStream) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();

  ReceiveStream* receive_stream = ReadReceiveStream(scope, quic_transport);

  const char data[] = "what";
  uint32_t num_bytes = 4u;

  EXPECT_EQ(
      producer->WriteData(data, &num_bytes, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE),
      MOJO_RESULT_OK);
  EXPECT_EQ(num_bytes, 4u);

  producer.reset();
  quic_transport->OnIncomingStreamClosed(/*stream_id=*/0, true);

  auto* reader = receive_stream->readable()->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state, read_promise);
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());
  auto read_result = read_tester.Value().V8Value();
  ASSERT_TRUE(read_result->IsObject());
  v8::Local<v8::Value> value;
  bool done = false;
  ASSERT_TRUE(
      V8UnpackIteratorResult(script_state, read_result.As<v8::Object>(), &done)
          .ToLocal(&value));
  DOMUint8Array* u8array =
      V8Uint8Array::ToImplWithTypeCheck(scope.GetIsolate(), value);
  ASSERT_TRUE(u8array);
  EXPECT_THAT(base::make_span(static_cast<uint8_t*>(u8array->Data()),
                              u8array->byteLength()),
              ElementsAre('w', 'h', 'a', 't'));
}

TEST_F(QuicTransportTest, CreateReceiveStreamThenClose) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();

  ReceiveStream* receive_stream = ReadReceiveStream(scope, quic_transport);

  auto* reader = receive_stream->readable()->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state, read_promise);

  quic_transport->close(nullptr);

  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  DOMException* exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), read_tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->code(),
            static_cast<uint16_t>(DOMExceptionCode::kNetworkError));

  // TODO(ricea): Fix this message if possible.
  EXPECT_EQ(exception->message(),
            "The stream was aborted by the remote server");
}

TEST_F(QuicTransportTest, CreateReceiveStreamThenRemoteClose) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();

  ReceiveStream* receive_stream = ReadReceiveStream(scope, quic_transport);

  auto* reader = receive_stream->readable()->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromise read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state, read_promise);

  client_remote_.reset();

  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  DOMException* exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), read_tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->code(),
            static_cast<uint16_t>(DOMExceptionCode::kNetworkError));

  // TODO(ricea): Fix this message if possible.
  EXPECT_EQ(exception->message(),
            "The stream was aborted by the remote server");
}

// BidirectionalStreams are thoroughly tested in bidirectional_stream_test.cc.
// Here we just test the QuicTransport APIs.
TEST_F(QuicTransportTest, CreateBidirectionalStream) {
  V8TestingScope scope;

  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  EXPECT_CALL(
      *mock_quic_transport_,
      CreateStream(Truly(ValidConsumerHandle), Truly(ValidProducerHandle), _))
      .WillOnce([](Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  ScriptPromise bidirectional_stream_promise =
      quic_transport->createBidirectionalStream(script_state,
                                                ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* bidirectional_stream = V8BidirectionalStream::ToImplWithTypeCheck(
      scope.GetIsolate(), tester.Value().V8Value());
  EXPECT_TRUE(bidirectional_stream);
}

TEST_F(QuicTransportTest, ReceiveBidirectionalStream) {
  V8TestingScope scope;

  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  mojo::ScopedDataPipeProducerHandle outgoing_producer;
  mojo::ScopedDataPipeConsumerHandle outgoing_consumer;
  ASSERT_TRUE(CreateDataPipeForWebTransportTests(&outgoing_producer,
                                                 &outgoing_consumer));

  mojo::ScopedDataPipeProducerHandle incoming_producer;
  mojo::ScopedDataPipeConsumerHandle incoming_consumer;
  ASSERT_TRUE(CreateDataPipeForWebTransportTests(&incoming_producer,
                                                 &incoming_consumer));

  std::move(pending_bidirectional_accept_callbacks_.front())
      .Run(next_stream_id_++, std::move(incoming_consumer),
           std::move(outgoing_producer));

  ReadableStream* streams = quic_transport->receiveBidirectionalStreams();

  v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

  BidirectionalStream* bidirectional_stream =
      V8BidirectionalStream::ToImplWithTypeCheck(scope.GetIsolate(), v8value);
  EXPECT_TRUE(bidirectional_stream);
}

TEST_F(QuicTransportTest, SetDatagramWritableQueueExpirationDuration) {
  V8TestingScope scope;

  auto* quic_transport =
      CreateAndConnectSuccessfully(scope, "quic-transport://example.com");

  constexpr base::TimeDelta duration = base::TimeDelta::FromMilliseconds(40);
  EXPECT_CALL(*mock_quic_transport_,
              SetOutgoingDatagramExpirationDuration(duration));

  quic_transport->SetDatagramWritableQueueExpirationDuration(duration);

  test::RunPendingTasks();
}

}  // namespace

}  // namespace blink
