// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport.h"

#include <array>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/web_transport.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_bidirectional_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_close_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_hash.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_options.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/bidirectional_stream.h"
#include "third_party/blink/renderer/modules/webtransport/datagram_duplex_stream.h"
#include "third_party/blink/renderer/modules/webtransport/receive_stream.h"
#include "third_party/blink/renderer/modules/webtransport/send_stream.h"
#include "third_party/blink/renderer/modules/webtransport/test_utils.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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

class WebTransportConnector final : public mojom::blink::WebTransportConnector {
 public:
  struct ConnectArgs {
    ConnectArgs(
        const KURL& url,
        Vector<network::mojom::blink::WebTransportCertificateFingerprintPtr>
            fingerprints,
        mojo::PendingRemote<network::mojom::blink::WebTransportHandshakeClient>
            handshake_client)
        : url(url),
          fingerprints(std::move(fingerprints)),
          handshake_client(std::move(handshake_client)) {}

    KURL url;
    Vector<network::mojom::blink::WebTransportCertificateFingerprintPtr>
        fingerprints;
    mojo::PendingRemote<network::mojom::blink::WebTransportHandshakeClient>
        handshake_client;
  };

  void Connect(
      const KURL& url,
      Vector<network::mojom::blink::WebTransportCertificateFingerprintPtr>
          fingerprints,
      mojo::PendingRemote<network::mojom::blink::WebTransportHandshakeClient>
          handshake_client) override {
    connect_args_.push_back(
        ConnectArgs(url, std::move(fingerprints), std::move(handshake_client)));
  }

  Vector<ConnectArgs> TakeConnectArgs() { return std::move(connect_args_); }

  void Bind(
      mojo::PendingReceiver<mojom::blink::WebTransportConnector> receiver) {
    receiver_set_.Add(this, std::move(receiver));
  }

 private:
  mojo::ReceiverSet<mojom::blink::WebTransportConnector> receiver_set_;
  Vector<ConnectArgs> connect_args_;
};

class MockWebTransport : public network::mojom::blink::WebTransport {
 public:
  explicit MockWebTransport(
      mojo::PendingReceiver<network::mojom::blink::WebTransport>
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
  MOCK_METHOD1(GetStats, void(GetStatsCallback));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD2(Close, void(uint32_t, String));

  void Close(
      network::mojom::blink::WebTransportCloseInfoPtr close_info) override {
    if (!close_info) {
      Close();
      return;
    }
    Close(close_info->code, close_info->reason);
  }

  void SendFin(uint32_t stream_id) override {}
  void AbortStream(uint32_t stream_id, uint8_t code) override {}
  void StopSending(uint32_t stream_id, uint8_t code) override {}

 private:
  mojo::Receiver<network::mojom::blink::WebTransport> receiver_;
};

class WebTransportTest : public ::testing::Test {
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
        mojom::blink::WebTransportConnector::Name_,
        WTF::BindRepeating(&WebTransportTest::BindConnector,
                  weak_ptr_factory_.GetWeakPtr()));
  }

  static WebTransportOptions* EmptyOptions() {
    return MakeGarbageCollected<WebTransportOptions>();
  }

  // Creates a WebTransport object with the given |url|.
  WebTransport* Create(const V8TestingScope& scope,
                       const String& url,
                       WebTransportOptions* options) {
    AddBinder(scope);
    return WebTransport::Create(scope.GetScriptState(), url, options,
                                ASSERT_NO_EXCEPTION);
  }

  // Connects a WebTransport object. Runs the event loop.
  void ConnectSuccessfully(
      WebTransport* web_transport,
      base::TimeDelta expected_outgoing_datagram_expiration_duration =
          base::TimeDelta()) {
    ConnectSuccessfullyWithoutRunningPendingTasks(
        web_transport, expected_outgoing_datagram_expiration_duration);
    test::RunPendingTasks();
  }

  void ConnectSuccessfullyWithoutRunningPendingTasks(
      WebTransport* web_transport,
      base::TimeDelta expected_outgoing_datagram_expiration_duration =
          base::TimeDelta()) {
    DCHECK(!mock_web_transport_) << "Only one connection supported, sorry";

    test::RunPendingTasks();

    auto args = connector_.TakeConnectArgs();
    if (args.size() != 1u) {
      ADD_FAILURE() << "args.size() should be 1, but is " << args.size();
      return;
    }

    mojo::Remote<network::mojom::blink::WebTransportHandshakeClient>
        handshake_client(std::move(args[0].handshake_client));

    mojo::PendingRemote<network::mojom::blink::WebTransport>
        web_transport_to_pass;
    mojo::PendingRemote<network::mojom::blink::WebTransportClient>
        client_remote;

    mock_web_transport_ = std::make_unique<StrictMock<MockWebTransport>>(
        web_transport_to_pass.InitWithNewPipeAndPassReceiver());

    // These are called on every connection, so expect them in every test.
    EXPECT_CALL(*mock_web_transport_, AcceptUnidirectionalStream(_))
        .WillRepeatedly([this](AcceptUnidirectionalStreamCallback callback) {
          pending_unidirectional_accept_callbacks_.push_back(
              std::move(callback));
        });

    EXPECT_CALL(*mock_web_transport_, AcceptBidirectionalStream(_))
        .WillRepeatedly([this](AcceptBidirectionalStreamCallback callback) {
          pending_bidirectional_accept_callbacks_.push_back(
              std::move(callback));
        });

    if (expected_outgoing_datagram_expiration_duration != base::TimeDelta()) {
      EXPECT_CALL(*mock_web_transport_,
                  SetOutgoingDatagramExpirationDuration(
                      expected_outgoing_datagram_expiration_duration));
    }

    handshake_client->OnConnectionEstablished(
        std::move(web_transport_to_pass),
        client_remote.InitWithNewPipeAndPassReceiver(),
        network::mojom::blink::HttpResponseHeaders::New(),
        network::mojom::blink::WebTransportStats::New());
    client_remote_.Bind(std::move(client_remote));
  }

  // Creates, connects and returns a WebTransport object with the given |url|.
  // Runs the event loop.
  WebTransport* CreateAndConnectSuccessfully(
      const V8TestingScope& scope,
      const String& url,
      WebTransportOptions* options = EmptyOptions()) {
    auto* web_transport = Create(scope, url, options);
    ConnectSuccessfully(web_transport);
    return web_transport;
  }

  SendStream* CreateSendStreamSuccessfully(const V8TestingScope& scope,
                                           WebTransport* web_transport) {
    EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _))
        .WillOnce([this](mojo::ScopedDataPipeConsumerHandle handle, Unused,
                         base::OnceCallback<void(bool, uint32_t)> callback) {
          send_stream_consumer_handle_ = std::move(handle);
          std::move(callback).Run(true, next_stream_id_++);
        });

    auto* script_state = scope.GetScriptState();
    ScriptPromiseUntyped send_stream_promise =
        web_transport->createUnidirectionalStream(script_state,
                                                  ASSERT_NO_EXCEPTION);
    ScriptPromiseTester tester(script_state, send_stream_promise);

    tester.WaitUntilSettled();

    EXPECT_TRUE(tester.IsFulfilled());
    auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                   tester.Value().V8Value());
    EXPECT_TRUE(writable);
    return static_cast<SendStream*>(writable);
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
                                   WebTransport* web_transport) {
    ReadableStream* streams = web_transport->incomingUnidirectionalStreams();

    v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

    ReadableStream* readable =
        V8ReadableStream::ToWrappable(scope.GetIsolate(), v8value);
    EXPECT_TRUE(readable);

    return static_cast<ReceiveStream*>(readable);
  }

  void BindConnector(mojo::ScopedMessagePipeHandle handle) {
    connector_.Bind(mojo::PendingReceiver<mojom::blink::WebTransportConnector>(
        std::move(handle)));
  }

  void TearDown() override {
    if (!interface_broker_)
      return;
    interface_broker_->SetBinderForTesting(
        mojom::blink::WebTransportConnector::Name_, {});
  }

  raw_ptr<const BrowserInterfaceBrokerProxy, DanglingUntriaged>
      interface_broker_ = nullptr;
  WTF::Deque<AcceptUnidirectionalStreamCallback>
      pending_unidirectional_accept_callbacks_;
  WTF::Deque<AcceptBidirectionalStreamCallback>
      pending_bidirectional_accept_callbacks_;
  test::TaskEnvironment task_environment_;
  WebTransportConnector connector_;
  std::unique_ptr<MockWebTransport> mock_web_transport_;
  mojo::Remote<network::mojom::blink::WebTransportClient> client_remote_;
  uint32_t next_stream_id_ = 0;
  mojo::ScopedDataPipeConsumerHandle send_stream_consumer_handle_;

  base::WeakPtrFactory<WebTransportTest> weak_ptr_factory_{this};
};

TEST_F(WebTransportTest, FailWithNullURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  WebTransport::Create(scope.GetScriptState(), String(), EmptyOptions(),
                       exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
}

TEST_F(WebTransportTest, FailWithEmptyURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  WebTransport::Create(scope.GetScriptState(), String(""), EmptyOptions(),
                       exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL '' is invalid.", exception_state.Message());
}

TEST_F(WebTransportTest, FailWithNoScheme) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  WebTransport::Create(scope.GetScriptState(), String("no-scheme"),
                       EmptyOptions(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL 'no-scheme' is invalid.", exception_state.Message());
}

TEST_F(WebTransportTest, FailWithHttpsURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  WebTransport::Create(scope.GetScriptState(), String("http://example.com/"),
                       EmptyOptions(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL's scheme must be 'https'. 'http' is not allowed.",
            exception_state.Message());
}

TEST_F(WebTransportTest, FailWithNoHost) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  WebTransport::Create(scope.GetScriptState(), String("https:///"),
                       EmptyOptions(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL 'https:///' is invalid.", exception_state.Message());
}

TEST_F(WebTransportTest, FailWithURLFragment) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  WebTransport::Create(scope.GetScriptState(),
                       String("https://example.com/#failing"), EmptyOptions(),
                       exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ(
      "The URL contains a fragment identifier ('#failing'). Fragment "
      "identifiers are not allowed in WebTransport URLs.",
      exception_state.Message());
}

TEST_F(WebTransportTest, FailByCSP) {
  V8TestingScope scope;
  scope.GetExecutionContext()
      ->GetContentSecurityPolicyForCurrentWorld()
      ->AddPolicies(ParseContentSecurityPolicies(
          "connect-src 'none'",
          network::mojom::ContentSecurityPolicyType::kEnforce,
          network::mojom::ContentSecurityPolicySource::kHTTP,
          *(scope.GetExecutionContext()->GetSecurityOrigin())));
  auto* web_transport = WebTransport::Create(
      scope.GetScriptState(), String("https://example.com/"), EmptyOptions(),
      ASSERT_NO_EXCEPTION);
  ScriptPromiseTester ready_tester(
      scope.GetScriptState(), web_transport->ready(scope.GetScriptState()));
  ScriptPromiseTester closed_tester(
      scope.GetScriptState(), web_transport->closed(scope.GetScriptState()));

  test::RunPendingTasks();

  EXPECT_FALSE(web_transport->HasPendingActivity());
  EXPECT_TRUE(ready_tester.IsRejected());
  EXPECT_TRUE(closed_tester.IsRejected());
}

TEST_F(WebTransportTest, PassCSP) {
  V8TestingScope scope;
  // This doesn't work without the https:// prefix, even thought it should
  // according to
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Security-Policy/connect-src.
  scope.GetExecutionContext()
      ->GetContentSecurityPolicyForCurrentWorld()
      ->AddPolicies(ParseContentSecurityPolicies(
          "connect-src https://example.com",
          network::mojom::ContentSecurityPolicyType::kEnforce,
          network::mojom::ContentSecurityPolicySource::kHTTP,
          *(scope.GetExecutionContext()->GetSecurityOrigin())));
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com/");
  ScriptPromiseTester ready_tester(
      scope.GetScriptState(), web_transport->ready(scope.GetScriptState()));

  EXPECT_TRUE(web_transport->HasPendingActivity());

  ready_tester.WaitUntilSettled();
  EXPECT_TRUE(ready_tester.IsFulfilled());
}

TEST_F(WebTransportTest, SendConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* web_transport = WebTransport::Create(
      scope.GetScriptState(), String("https://example.com/"), EmptyOptions(),
      ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  EXPECT_EQ(KURL("https://example.com/"), args[0].url);
  EXPECT_TRUE(args[0].fingerprints.empty());
  EXPECT_TRUE(web_transport->HasPendingActivity());
}

TEST_F(WebTransportTest, SuccessfulConnect) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  ScriptPromiseTester ready_tester(
      scope.GetScriptState(), web_transport->ready(scope.GetScriptState()));

  EXPECT_TRUE(web_transport->HasPendingActivity());

  ready_tester.WaitUntilSettled();
  EXPECT_TRUE(ready_tester.IsFulfilled());
}

TEST_F(WebTransportTest, FailedConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* web_transport = WebTransport::Create(
      scope.GetScriptState(), String("https://example.com/"), EmptyOptions(),
      ASSERT_NO_EXCEPTION);
  ScriptPromiseTester ready_tester(
      scope.GetScriptState(), web_transport->ready(scope.GetScriptState()));
  ScriptPromiseTester closed_tester(
      scope.GetScriptState(), web_transport->closed(scope.GetScriptState()));

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());

  mojo::Remote<network::mojom::blink::WebTransportHandshakeClient>
      handshake_client(std::move(args[0].handshake_client));

  handshake_client->OnHandshakeFailed(nullptr);

  test::RunPendingTasks();
  EXPECT_FALSE(web_transport->HasPendingActivity());
  EXPECT_TRUE(ready_tester.IsRejected());
  EXPECT_TRUE(closed_tester.IsRejected());
}

TEST_F(WebTransportTest, SendConnectWithFingerprint) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* hash = MakeGarbageCollected<WebTransportHash>();
  hash->setAlgorithm("sha-256");
  constexpr uint8_t kPattern[] = {
      0xED, 0x3D, 0xD7, 0xC3, 0x67, 0x10, 0x94, 0x68, 0xD1, 0xDC, 0xD1,
      0x26, 0x5C, 0xB2, 0x74, 0xD7, 0x1C, 0xA2, 0x63, 0x3E, 0x94, 0x94,
      0xC0, 0x84, 0x39, 0xD6, 0x64, 0xFA, 0x08, 0xB9, 0x77, 0x37,
  };
  DOMUint8Array* hashValue = DOMUint8Array::Create(kPattern);
  hash->setValue(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      NotShared<DOMUint8Array>(hashValue)));
  auto* options = MakeGarbageCollected<WebTransportOptions>();
  options->setServerCertificateHashes({hash});
  WebTransport::Create(scope.GetScriptState(), String("https://example.com/"),
                       options, ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  ASSERT_EQ(1u, args[0].fingerprints.size());
  EXPECT_EQ(args[0].fingerprints[0]->algorithm, "sha-256");
  EXPECT_EQ(args[0].fingerprints[0]->fingerprint,
            "ED:3D:D7:C3:67:10:94:68:D1:DC:D1:26:5C:B2:74:D7:1C:A2:63:3E:94:94:"
            "C0:84:39:D6:64:FA:08:B9:77:37");
}

TEST_F(WebTransportTest, SendConnectWithArrayBufferHash) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* hash = MakeGarbageCollected<WebTransportHash>();
  hash->setAlgorithm("sha-256");
  constexpr uint8_t kPattern[] = {0x28, 0x24, 0xa8, 0xa2};
  DOMArrayBuffer* hashValue = DOMArrayBuffer::Create(kPattern);
  hash->setValue(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(hashValue));
  auto* options = MakeGarbageCollected<WebTransportOptions>();
  options->setServerCertificateHashes({hash});
  WebTransport::Create(scope.GetScriptState(), String("https://example.com/"),
                       options, ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  ASSERT_EQ(1u, args[0].fingerprints.size());
  EXPECT_EQ(args[0].fingerprints[0]->algorithm, "sha-256");
  EXPECT_EQ(args[0].fingerprints[0]->fingerprint, "28:24:A8:A2");
}

TEST_F(WebTransportTest, SendConnectWithOffsetArrayBufferViewHash) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* hash = MakeGarbageCollected<WebTransportHash>();
  hash->setAlgorithm("sha-256");
  constexpr uint8_t kPattern[6] = {0x28, 0x24, 0xa8, 0xa2, 0x44, 0xee};
  DOMArrayBuffer* buffer = DOMArrayBuffer::Create(kPattern);
  DOMUint8Array* view = DOMUint8Array::Create(buffer, 2, 3);
  hash->setValue(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      NotShared<DOMUint8Array>(view)));
  auto* options = MakeGarbageCollected<WebTransportOptions>();
  options->setServerCertificateHashes({hash});
  WebTransport::Create(scope.GetScriptState(), String("https://example.com/"),
                       options, ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  ASSERT_EQ(1u, args[0].fingerprints.size());
  EXPECT_EQ(args[0].fingerprints[0]->algorithm, "sha-256");
  EXPECT_EQ(args[0].fingerprints[0]->fingerprint, "A8:A2:44");
}

// Regression test for https://crbug.com/1242185.
TEST_F(WebTransportTest, SendConnectWithInvalidFingerprint) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* hash = MakeGarbageCollected<WebTransportHash>();
  // "algorithm" is unset.
  constexpr uint8_t kPattern[] = {
      0xED, 0x3D, 0xD7, 0xC3, 0x67, 0x10, 0x94, 0x68, 0xD1, 0xDC, 0xD1,
      0x26, 0x5C, 0xB2, 0x74, 0xD7, 0x1C, 0xA2, 0x63, 0x3E, 0x94, 0x94,
      0xC0, 0x84, 0x39, 0xD6, 0x64, 0xFA, 0x08, 0xB9, 0x77, 0x37,
  };
  DOMUint8Array* hashValue = DOMUint8Array::Create(kPattern);
  hash->setValue(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      NotShared<DOMUint8Array>(hashValue)));
  auto* options = MakeGarbageCollected<WebTransportOptions>();
  options->setServerCertificateHashes({hash});
  WebTransport::Create(scope.GetScriptState(), String("https://example.com/"),
                       options, ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  ASSERT_EQ(0u, args[0].fingerprints.size());
}

TEST_F(WebTransportTest, CloseDuringConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* web_transport = WebTransport::Create(
      scope.GetScriptState(), String("https://example.com/"), EmptyOptions(),
      ASSERT_NO_EXCEPTION);
  ScriptPromiseTester ready_tester(
      scope.GetScriptState(), web_transport->ready(scope.GetScriptState()));
  ScriptPromiseTester closed_tester(
      scope.GetScriptState(), web_transport->closed(scope.GetScriptState()));

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());

  web_transport->close(nullptr);

  test::RunPendingTasks();

  EXPECT_FALSE(web_transport->HasPendingActivity());
  EXPECT_TRUE(ready_tester.IsRejected());
  EXPECT_TRUE(closed_tester.IsRejected());
}

TEST_F(WebTransportTest, CloseAfterConnection) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  EXPECT_CALL(*mock_web_transport_, Close(42, String("because")));

  ScriptPromiseTester ready_tester(
      scope.GetScriptState(), web_transport->ready(scope.GetScriptState()));
  ScriptPromiseTester closed_tester(
      scope.GetScriptState(), web_transport->closed(scope.GetScriptState()));

  WebTransportCloseInfo* close_info =
      MakeGarbageCollected<WebTransportCloseInfo>();
  close_info->setCloseCode(42);
  close_info->setReason("because");
  web_transport->close(close_info);

  test::RunPendingTasks();

  EXPECT_FALSE(web_transport->HasPendingActivity());
  EXPECT_TRUE(ready_tester.IsFulfilled());
  EXPECT_TRUE(closed_tester.IsFulfilled());

  // Calling close again does nothing.
  web_transport->close(nullptr);
}

TEST_F(WebTransportTest, CloseWithNull) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_, Close());

  ScriptPromiseTester ready_tester(
      scope.GetScriptState(), web_transport->ready(scope.GetScriptState()));
  ScriptPromiseTester closed_tester(
      scope.GetScriptState(), web_transport->closed(scope.GetScriptState()));

  web_transport->close(nullptr);

  test::RunPendingTasks();

  EXPECT_FALSE(web_transport->HasPendingActivity());
  EXPECT_TRUE(ready_tester.IsFulfilled());
  EXPECT_TRUE(closed_tester.IsFulfilled());

  // TODO(yhirano): Make sure Close() is called.
}

TEST_F(WebTransportTest, CloseWithReasonOnly) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_, Close(0, String("because")));

  ScriptPromiseTester ready_tester(
      scope.GetScriptState(), web_transport->ready(scope.GetScriptState()));
  ScriptPromiseTester closed_tester(
      scope.GetScriptState(), web_transport->closed(scope.GetScriptState()));

  WebTransportCloseInfo* close_info =
      MakeGarbageCollected<WebTransportCloseInfo>();
  close_info->setReason("because");
  web_transport->close(close_info);

  test::RunPendingTasks();
}

// A live connection will be kept alive even if there is no explicit reference.
// When the underlying connection is shut down, the connection will be swept.
TEST_F(WebTransportTest, GarbageCollection) {
  V8TestingScope scope;

  WeakPersistent<WebTransport> web_transport;

  auto* isolate = scope.GetIsolate();

  {
    // The streams created when creating a WebTransport create some v8 handles.
    // To ensure these are collected, we need to create a handle scope. This is
    // not a problem for garbage collection in normal operation.
    v8::HandleScope handle_scope(isolate);
    web_transport = CreateAndConnectSuccessfully(scope, "https://example.com");
    EXPECT_CALL(*mock_web_transport_, Close());
  }

  // Pretend the stack is empty. This will avoid accidentally treating any
  // copies of the |web_transport| pointer as references.
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_TRUE(web_transport);

  {
    v8::HandleScope handle_scope(isolate);
    web_transport->close(nullptr);
  }

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(web_transport);
}

TEST_F(WebTransportTest, GarbageCollectMojoConnectionError) {
  V8TestingScope scope;

  WeakPersistent<WebTransport> web_transport;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());
    web_transport = CreateAndConnectSuccessfully(scope, "https://example.com");
  }

  ScriptPromiseTester closed_tester(
      scope.GetScriptState(), web_transport->closed(scope.GetScriptState()));

  // Closing the server-side of the pipe causes a mojo connection error.
  client_remote_.reset();

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(web_transport);
  EXPECT_TRUE(closed_tester.IsRejected());
}

TEST_F(WebTransportTest, SendDatagram) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_, SendDatagram(ElementsAre('A'), _))
      .WillOnce(Invoke([](base::span<const uint8_t>,
                          MockWebTransport::SendDatagramCallback callback) {
        std::move(callback).Run(true);
      }));

  auto* writable = web_transport->datagrams()->writable();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().IsUndefined());
}

// TODO(yhirano): Move this to datagram_duplex_stream_test.cc.
TEST_F(WebTransportTest, BackpressureForOutgoingDatagrams) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_, SendDatagram(_, _))
      .Times(4)
      .WillRepeatedly(
          Invoke([](base::span<const uint8_t>,
                    MockWebTransport::SendDatagramCallback callback) {
            std::move(callback).Run(true);
          }));

  web_transport->datagrams()->setOutgoingHighWaterMark(3);
  auto* writable = web_transport->datagrams()->writable();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseUntyped promise1;
  ScriptPromiseUntyped promise2;
  ScriptPromiseUntyped promise3;
  ScriptPromiseUntyped promise4;

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
  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ(promise1.V8Promise()->State(), v8::Promise::kFulfilled);
  EXPECT_EQ(promise2.V8Promise()->State(), v8::Promise::kFulfilled);
  EXPECT_EQ(promise3.V8Promise()->State(), v8::Promise::kPending);
  EXPECT_EQ(promise4.V8Promise()->State(), v8::Promise::kPending);

  // The rest are resolved by the callback.
  test::RunPendingTasks();
  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ(promise3.V8Promise()->State(), v8::Promise::kFulfilled);
  EXPECT_EQ(promise4.V8Promise()->State(), v8::Promise::kFulfilled);
}

TEST_F(WebTransportTest, SendDatagramBeforeConnect) {
  V8TestingScope scope;
  auto* web_transport = Create(scope, "https://example.com", EmptyOptions());

  auto* writable = web_transport->datagrams()->writable();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);

  ConnectSuccessfullyWithoutRunningPendingTasks(web_transport);

  testing::Sequence s;
  EXPECT_CALL(*mock_web_transport_, SendDatagram(ElementsAre('A'), _))
      .WillOnce(Invoke([](base::span<const uint8_t>,
                          MockWebTransport::SendDatagramCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*mock_web_transport_, SendDatagram(ElementsAre('N'), _))
      .WillOnce(Invoke([](base::span<const uint8_t>,
                          MockWebTransport::SendDatagramCallback callback) {
        std::move(callback).Run(true);
      }));

  test::RunPendingTasks();
  *chunk->Data() = 'N';
  result = writer->write(script_state, ScriptValue::From(script_state, chunk),
                         ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().IsUndefined());
}

TEST_F(WebTransportTest, SendDatagramAfterClose) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  EXPECT_CALL(*mock_web_transport_, Close());

  web_transport->close(nullptr);
  test::RunPendingTasks();

  auto* writable = web_transport->datagrams()->writable();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);

  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  ScriptPromiseUntyped result =
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
  if (!V8UnpackIterationResult(script_state,
                               iterator_result.V8Value().As<v8::Object>(),
                               &value, &done)) {
    ADD_FAILURE() << "unable to unpack iterator_result";
    return {};
  }

  EXPECT_FALSE(done);
  DummyExceptionStateForTesting exception_state;
  auto array = NativeValueTraits<NotShared<DOMUint8Array>>::NativeValue(
      script_state->GetIsolate(), value, exception_state);
  if (!array) {
    ADD_FAILURE() << "value was not a Uint8Array";
    return {};
  }

  Vector<uint8_t> result;
  result.Append(array->Data(), base::checked_cast<wtf_size_t>(array->length()));
  return result;
}

TEST_F(WebTransportTest, ReceiveDatagramBeforeRead) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  const std::array<uint8_t, 1> chunk = {'A'};
  client_remote_->OnDatagramReceived(chunk);

  test::RunPendingTasks();

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped result = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester.Value()), ElementsAre('A'));
}

TEST_F(WebTransportTest, ReceiveDatagramDuringRead) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped result = reader->read(script_state, ASSERT_NO_EXCEPTION);

  const std::array<uint8_t, 1> chunk = {'A'};
  client_remote_->OnDatagramReceived(chunk);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester.Value()), ElementsAre('A'));
}

TEST_F(WebTransportTest, ReceiveDatagramWithBYOBReader) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetBYOBReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  NotShared<DOMArrayBufferView> view =
      NotShared<DOMUint8Array>(DOMUint8Array::Create(1));
  ScriptPromiseUntyped result =
      reader->read(script_state, view, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);

  const std::array<uint8_t, 1> chunk = {'A'};
  client_remote_->OnDatagramReceived(chunk);

  test::RunPendingTasks();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(GetValueAsVector(script_state, tester.Value()), ElementsAre('A'));
}

bool IsRangeError(ScriptState* script_state,
                  ScriptValue value,
                  const String& message) {
  v8::Local<v8::Object> object;
  if (!value.V8Value()->ToObject(script_state->GetContext()).ToLocal(&object)) {
    return false;
  }
  if (!object->IsNativeError())
    return false;

  const auto& Has = [script_state, object](const String& key,
                                           const String& value) -> bool {
    v8::Local<v8::Value> actual;
    return object
               ->Get(script_state->GetContext(),
                     V8AtomicString(script_state->GetIsolate(), key))
               .ToLocal(&actual) &&
           ToCoreStringWithUndefinedOrNullCheck(script_state->GetIsolate(),
                                                actual) == value;
  };

  return Has("name", "RangeError") && Has("message", message);
}

TEST_F(WebTransportTest, ReceiveDatagramWithoutEnoughBuffer) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetBYOBReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  NotShared<DOMArrayBufferView> view =
      NotShared<DOMUint8Array>(DOMUint8Array::Create(1));
  ScriptPromiseUntyped result =
      reader->read(script_state, view, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);

  const std::array<uint8_t, 3> chunk = {'A', 'B', 'C'};
  client_remote_->OnDatagramReceived(chunk);

  test::RunPendingTasks();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  EXPECT_TRUE(IsRangeError(script_state, tester.Value(),
                           "supplied view is not large enough."));
}

TEST_F(WebTransportTest, CancelDatagramReadableWorks) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* readable = web_transport->datagrams()->readable();

  // This datagram should be discarded.
  const std::array<uint8_t, 1> chunk1 = {'A'};
  client_remote_->OnDatagramReceived(chunk1);

  test::RunPendingTasks();

  readable->cancel(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  // This datagram should also be discarded.
  const std::array<uint8_t, 1> chunk2 = {'B'};
  client_remote_->OnDatagramReceived(chunk2);

  test::RunPendingTasks();
}

TEST_F(WebTransportTest, DatagramsShouldBeErroredAfterClose) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  EXPECT_CALL(*mock_web_transport_, Close());

  const std::array<uint8_t, 1> chunk1 = {'A'};
  client_remote_->OnDatagramReceived(chunk1);

  test::RunPendingTasks();

  web_transport->close(nullptr);

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped result1 =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester1(script_state, result1);
  tester1.WaitUntilSettled();
  EXPECT_TRUE(tester1.IsRejected());
}

TEST_F(WebTransportTest, ResettingIncomingHighWaterMarkWorksAfterClose) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  EXPECT_CALL(*mock_web_transport_, Close());

  const std::array<uint8_t, 1> chunk1 = {'A'};
  client_remote_->OnDatagramReceived(chunk1);

  test::RunPendingTasks();

  web_transport->close(nullptr);

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  web_transport->datagrams()->setIncomingHighWaterMark(0);
  ScriptPromiseUntyped result = reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(WebTransportTest, TransportErrorErrorsReadableStream) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  // This datagram should be discarded.
  const std::array<uint8_t, 1> chunk1 = {'A'};
  client_remote_->OnDatagramReceived(chunk1);

  test::RunPendingTasks();

  // Cause a transport error.
  client_remote_.reset();

  test::RunPendingTasks();

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped result = reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(WebTransportTest, DatagramsAreDropped) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  // Chunk 'A' gets placed in the source queue.
  const std::array<uint8_t, 1> chunk1 = {'A'};
  client_remote_->OnDatagramReceived(chunk1);

  // Chunk 'B' replaces chunk 'A'.
  const std::array<uint8_t, 1> chunk2 = {'B'};
  client_remote_->OnDatagramReceived(chunk2);

  // Make sure that the calls have run.
  test::RunPendingTasks();

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped result1 =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped result2 =
      reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester1(script_state, result1);
  ScriptPromiseTester tester2(script_state, result2);
  tester1.WaitUntilSettled();
  EXPECT_TRUE(tester1.IsFulfilled());
  EXPECT_FALSE(tester2.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester1.Value()),
              ElementsAre('B'));

  // Chunk 'C' fulfills the pending read.
  const std::array<uint8_t, 1> chunk3 = {'C'};
  client_remote_->OnDatagramReceived(chunk3);

  tester2.WaitUntilSettled();
  EXPECT_TRUE(tester2.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester2.Value()),
              ElementsAre('C'));
}

TEST_F(WebTransportTest, IncomingHighWaterMarkIsObeyed) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  constexpr int32_t kHighWaterMark = 5;
  web_transport->datagrams()->setIncomingHighWaterMark(kHighWaterMark);

  for (int i = 0; i < kHighWaterMark + 1; ++i) {
    const std::array<uint8_t, 1> chunk = {static_cast<uint8_t>('0' + i)};
    client_remote_->OnDatagramReceived(chunk);
  }

  // Make sure that the calls have run.
  test::RunPendingTasks();

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  for (int i = 0; i < kHighWaterMark; ++i) {
    ScriptPromiseUntyped result =
        reader->read(script_state, ASSERT_NO_EXCEPTION);

    ScriptPromiseTester tester(script_state, result);
    tester.WaitUntilSettled();

    EXPECT_TRUE(tester.IsFulfilled());
    EXPECT_THAT(GetValueAsVector(script_state, tester.Value()),
                ElementsAre('0' + i + 1));
  }
}

TEST_F(WebTransportTest, ResettingHighWaterMarkClearsQueue) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  constexpr int32_t kHighWaterMark = 5;
  web_transport->datagrams()->setIncomingHighWaterMark(kHighWaterMark);

  for (int i = 0; i < kHighWaterMark; ++i) {
    const std::array<uint8_t, 1> chunk = {'A'};
    client_remote_->OnDatagramReceived(chunk);
  }

  // Make sure that the calls have run.
  test::RunPendingTasks();

  web_transport->datagrams()->setIncomingHighWaterMark(0);

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseUntyped result = reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);

  // Give the promise an opportunity to settle.
  test::RunPendingTasks();

  // The queue should be empty, so read() should not have completed.
  EXPECT_FALSE(tester.IsFulfilled());
  EXPECT_FALSE(tester.IsRejected());
}

TEST_F(WebTransportTest, ReadIncomingDatagramWorksWithHighWaterMarkZero) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  web_transport->datagrams()->setIncomingHighWaterMark(0);

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped result = reader->read(script_state, ASSERT_NO_EXCEPTION);

  const std::array<uint8_t, 1> chunk = {'A'};
  client_remote_->OnDatagramReceived(chunk);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester.Value()), ElementsAre('A'));
}

// We only do an extremely basic test for incomingMaxAge as overriding
// base::TimeTicks::Now() doesn't work well in Blink and passing in a mock clock
// would add a lot of complexity for little benefit.
TEST_F(WebTransportTest, IncomingMaxAgeIsObeyed) {
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  web_transport->datagrams()->setIncomingHighWaterMark(2);

  const std::array<uint8_t, 1> chunk1 = {'A'};
  client_remote_->OnDatagramReceived(chunk1);

  const std::array<uint8_t, 1> chunk2 = {'B'};
  client_remote_->OnDatagramReceived(chunk2);

  test::RunPendingTasks();

  constexpr base::TimeDelta kMaxAge = base::Microseconds(1);
  web_transport->datagrams()->setIncomingMaxAge(kMaxAge.InMillisecondsF());

  test::RunDelayedTasks(kMaxAge);

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  // The queue should be empty so the read should not complete.
  ScriptPromiseUntyped result = reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);

  test::RunPendingTasks();

  EXPECT_FALSE(tester.IsFulfilled());
  EXPECT_FALSE(tester.IsRejected());
}

// This is a regression test for https://crbug.com/1246335
TEST_F(WebTransportTest, TwoSimultaneousReadsWork) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseUntyped result1 =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped result2 =
      reader->read(script_state, ASSERT_NO_EXCEPTION);

  const std::array<uint8_t, 1> chunk1 = {'A'};
  client_remote_->OnDatagramReceived(chunk1);

  const std::array<uint8_t, 1> chunk2 = {'B'};
  client_remote_->OnDatagramReceived(chunk2);

  ScriptPromiseTester tester1(script_state, result1);
  tester1.WaitUntilSettled();
  EXPECT_TRUE(tester1.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester1.Value()),
              ElementsAre('A'));

  ScriptPromiseTester tester2(script_state, result2);
  tester2.WaitUntilSettled();
  EXPECT_TRUE(tester2.IsFulfilled());

  EXPECT_THAT(GetValueAsVector(script_state, tester2.Value()),
              ElementsAre('B'));
}

bool ValidProducerHandle(const mojo::ScopedDataPipeProducerHandle& handle) {
  return handle.is_valid();
}

bool ValidConsumerHandle(const mojo::ScopedDataPipeConsumerHandle& handle) {
  return handle.is_valid();
}

TEST_F(WebTransportTest, CreateSendStream) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Not(Truly(ValidProducerHandle)), _))
      .WillOnce([](Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped send_stream_promise =
      web_transport->createUnidirectionalStream(script_state,
                                                ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  EXPECT_TRUE(writable);
}

TEST_F(WebTransportTest, CreateSendStreamBeforeConnect) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* web_transport = WebTransport::Create(
      script_state, "https://example.com", EmptyOptions(), ASSERT_NO_EXCEPTION);
  auto& exception_state = scope.GetExceptionState();
  ScriptPromiseUntyped send_stream_promise =
      web_transport->createUnidirectionalStream(script_state, exception_state);
  EXPECT_TRUE(send_stream_promise.IsEmpty());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kNetworkError),
            exception_state.Code());
}

TEST_F(WebTransportTest, CreateSendStreamFailure) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _))
      .WillOnce([](Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(false, 0);
      });

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped send_stream_promise =
      web_transport->createUnidirectionalStream(script_state,
                                                ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
  DOMException* exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  EXPECT_EQ(exception->name(), "NetworkError");
  EXPECT_EQ(exception->message(), "Failed to create send stream.");
}

// Every active stream is kept alive by the WebTransport object.
TEST_F(WebTransportTest, SendStreamGarbageCollection) {
  V8TestingScope scope;

  WeakPersistent<WebTransport> web_transport;
  WeakPersistent<SendStream> send_stream;

  auto* isolate = scope.GetIsolate();

  {
    // The streams created when creating a WebTransport or SendStream create
    // some v8 handles. To ensure these are collected, we need to create a
    // handle scope. This is not a problem for garbage collection in normal
    // operation.
    v8::HandleScope handle_scope(isolate);

    web_transport = CreateAndConnectSuccessfully(scope, "https://example.com");
    EXPECT_CALL(*mock_web_transport_, Close());
    send_stream = CreateSendStreamSuccessfully(scope, web_transport);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_TRUE(web_transport);
  EXPECT_TRUE(send_stream);

  {
    v8::HandleScope handle_scope(isolate);
    web_transport->close(nullptr);
  }

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(web_transport);
  EXPECT_FALSE(send_stream);
}

// A live stream will be kept alive even if there is no explicit reference.
// When the underlying connection is shut down, the connection will be swept.
TEST_F(WebTransportTest, SendStreamGarbageCollectionLocalClose) {
  V8TestingScope scope;

  WeakPersistent<SendStream> send_stream;
  WeakPersistent<WebTransport> web_transport;

  {
    // The writable stream created when creating a SendStream creates some
    // v8 handles. To ensure these are collected, we need to create a handle
    // scope. This is not a problem for garbage collection in normal operation.
    v8::HandleScope handle_scope(scope.GetIsolate());

    web_transport = CreateAndConnectSuccessfully(scope, "https://example.com");
    send_stream = CreateSendStreamSuccessfully(scope, web_transport);
  }

  // Pretend the stack is empty. This will avoid accidentally treating any
  // copies of the |send_stream| pointer as references.
  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(send_stream);

  auto* script_state = scope.GetScriptState();
  auto* isolate = scope.GetIsolate();
  // We use v8::Persistent instead of ScriptPromiseUntyped, because
  // ScriptPromiseUntyped will be broken when CollectAllGarbageForTesting is
  // called.
  v8::Persistent<v8::Promise> close_promise_persistent;

  {
    v8::HandleScope handle_scope(isolate);
    ScriptPromiseUntyped close_promise =
        send_stream->close(script_state, ASSERT_NO_EXCEPTION);
    close_promise_persistent.Reset(isolate, close_promise.V8Promise());
  }

  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();

  // The WebTransport object is alive because it's connected.
  ASSERT_TRUE(web_transport);

  // The SendStream object has not been collected yet, because it remains
  // referenced by |web_transport| until OnOutgoingStreamClosed is called.
  EXPECT_TRUE(send_stream);

  web_transport->OnOutgoingStreamClosed(/*stream_id=*/0);

  {
    v8::HandleScope handle_scope(isolate);
    ScriptPromiseTester tester(
        script_state,
        ScriptPromiseUntyped(isolate, close_promise_persistent.Get(isolate)));
    close_promise_persistent.Reset();
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_TRUE(web_transport);
  EXPECT_FALSE(send_stream);
}

TEST_F(WebTransportTest, SendStreamGarbageCollectionRemoteClose) {
  V8TestingScope scope;

  WeakPersistent<SendStream> send_stream;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* web_transport =
        CreateAndConnectSuccessfully(scope, "https://example.com");
    send_stream = CreateSendStreamSuccessfully(scope, web_transport);
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
TEST_F(WebTransportTest, ReceiveStreamGarbageCollectionCancel) {
  V8TestingScope scope;

  WeakPersistent<ReceiveStream> receive_stream;
  mojo::ScopedDataPipeProducerHandle producer;

  {
    // The readable stream created when creating a ReceiveStream creates some
    // v8 handles. To ensure these are collected, we need to create a handle
    // scope. This is not a problem for garbage collection in normal operation.
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* web_transport =
        CreateAndConnectSuccessfully(scope, "https://example.com");

    producer = DoAcceptUnidirectionalStream();
    receive_stream = ReadReceiveStream(scope, web_transport);
  }

  // Pretend the stack is empty. This will avoid accidentally treating any
  // copies of the |receive_stream| pointer as references.
  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  auto* script_state = scope.GetScriptState();

  // Eagerly destroy the ScriptPromiseUntyped as this test is using manual GC
  // without stack which is incompatible with ScriptValue.
  std::optional<ScriptPromiseUntyped> cancel_promise;
  {
    // Cancelling also creates v8 handles, so we need a new handle scope as
    // above.
    v8::HandleScope handle_scope(scope.GetIsolate());
    cancel_promise.emplace(
        receive_stream->cancel(script_state, ASSERT_NO_EXCEPTION));
  }

  ScriptPromiseTester tester(script_state, cancel_promise.value());
  cancel_promise.reset();
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(receive_stream);
}

TEST_F(WebTransportTest, ReceiveStreamGarbageCollectionRemoteClose) {
  V8TestingScope scope;

  WeakPersistent<ReceiveStream> receive_stream;
  mojo::ScopedDataPipeProducerHandle producer;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* web_transport =
        CreateAndConnectSuccessfully(scope, "https://example.com");
    producer = DoAcceptUnidirectionalStream();
    receive_stream = ReadReceiveStream(scope, web_transport);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  // Close the other end of the pipe.
  producer.reset();

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  receive_stream->GetIncomingStream()->OnIncomingStreamClosed(false);

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(receive_stream);
}

// This is the same test as ReceiveStreamGarbageCollectionRemoteClose, except
// that the order of the data pipe being reset and the OnIncomingStreamClosed
// message is reversed. It is important that the object is not collected until
// both events have happened.
TEST_F(WebTransportTest, ReceiveStreamGarbageCollectionRemoteCloseReverse) {
  V8TestingScope scope;

  WeakPersistent<ReceiveStream> receive_stream;
  mojo::ScopedDataPipeProducerHandle producer;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* web_transport =
        CreateAndConnectSuccessfully(scope, "https://example.com");

    producer = DoAcceptUnidirectionalStream();
    receive_stream = ReadReceiveStream(scope, web_transport);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  receive_stream->GetIncomingStream()->OnIncomingStreamClosed(false);

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  ASSERT_TRUE(receive_stream);

  producer.reset();

  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(receive_stream);
}

TEST_F(WebTransportTest, CreateSendStreamAbortedByClose) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  base::OnceCallback<void(bool, uint32_t)> create_stream_callback;
  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _))
      .WillOnce([&](Unused, Unused,
                    base::OnceCallback<void(bool, uint32_t)> callback) {
        create_stream_callback = std::move(callback);
      });
  EXPECT_CALL(*mock_web_transport_, Close());

  ScriptPromiseUntyped send_stream_promise =
      web_transport->createUnidirectionalStream(script_state,
                                                ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  test::RunPendingTasks();

  web_transport->close(nullptr);
  std::move(create_stream_callback).Run(true, 0);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
}

// ReceiveStream functionality is thoroughly tested in incoming_stream_test.cc.
// This test just verifies that the creation is done correctly.
TEST_F(WebTransportTest, CreateReceiveStream) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();

  ReceiveStream* receive_stream = ReadReceiveStream(scope, web_transport);

  const std::string_view data = "what";
  EXPECT_EQ(producer->WriteAllData(base::as_byte_span(data)), MOJO_RESULT_OK);

  producer.reset();
  web_transport->OnIncomingStreamClosed(/*stream_id=*/0, true);

  auto* reader = receive_stream->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped read_promise =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state, read_promise);
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());
  auto read_result = read_tester.Value().V8Value();
  ASSERT_TRUE(read_result->IsObject());
  v8::Local<v8::Value> value;
  bool done = false;
  ASSERT_TRUE(V8UnpackIterationResult(
      script_state, read_result.As<v8::Object>(), &value, &done));
  NotShared<DOMUint8Array> u8array =
      NativeValueTraits<NotShared<DOMUint8Array>>::NativeValue(
          scope.GetIsolate(), value, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(u8array);
  EXPECT_THAT(u8array->ByteSpan(), ElementsAre('w', 'h', 'a', 't'));
}

TEST_F(WebTransportTest, CreateReceiveStreamThenClose) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_, Close());

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();

  ReceiveStream* receive_stream = ReadReceiveStream(scope, web_transport);

  auto* reader = receive_stream->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped read_promise =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state, read_promise);

  web_transport->close(nullptr);

  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  WebTransportError* exception = V8WebTransportError::ToWrappable(
      scope.GetIsolate(), read_tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "WebTransportError");
  EXPECT_EQ(exception->source(), "session");
  EXPECT_EQ(exception->streamErrorCode(), std::nullopt);
}

TEST_F(WebTransportTest, CreateReceiveStreamThenRemoteClose) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();

  ReceiveStream* receive_stream = ReadReceiveStream(scope, web_transport);

  auto* reader = receive_stream->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseUntyped read_promise =
      reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state, read_promise);

  client_remote_.reset();

  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  WebTransportError* exception = V8WebTransportError::ToWrappable(
      scope.GetIsolate(), read_tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "WebTransportError");
  EXPECT_EQ(exception->source(), "session");
  EXPECT_EQ(exception->streamErrorCode(), std::nullopt);
}

// BidirectionalStreams are thoroughly tested in bidirectional_stream_test.cc.
// Here we just test the WebTransport APIs.
TEST_F(WebTransportTest, CreateBidirectionalStream) {
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_, CreateStream(Truly(ValidConsumerHandle),
                                                 Truly(ValidProducerHandle), _))
      .WillOnce([](Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped bidirectional_stream_promise =
      web_transport->createBidirectionalStream(script_state,
                                               ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* bidirectional_stream = V8WebTransportBidirectionalStream::ToWrappable(
      scope.GetIsolate(), tester.Value().V8Value());
  EXPECT_TRUE(bidirectional_stream);
}

TEST_F(WebTransportTest, ReceiveBidirectionalStream) {
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

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

  ReadableStream* streams = web_transport->incomingBidirectionalStreams();

  v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

  BidirectionalStream* bidirectional_stream =
      V8WebTransportBidirectionalStream::ToWrappable(scope.GetIsolate(),
                                                     v8value);
  EXPECT_TRUE(bidirectional_stream);
}

TEST_F(WebTransportTest, SetDatagramWritableQueueExpirationDuration) {
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  constexpr double kDuration = 40;
  constexpr base::TimeDelta kDurationDelta = base::Milliseconds(kDuration);
  EXPECT_CALL(*mock_web_transport_,
              SetOutgoingDatagramExpirationDuration(kDurationDelta));

  web_transport->setDatagramWritableQueueExpirationDuration(kDuration);

  test::RunPendingTasks();
}

// Regression test for https://crbug.com/1241489.
TEST_F(WebTransportTest, SetOutgoingMaxAgeBeforeConnectComplete) {
  V8TestingScope scope;

  auto* web_transport = Create(scope, "https://example.com/", EmptyOptions());

  constexpr double kDuration = 1000;
  constexpr base::TimeDelta kDurationDelta = base::Milliseconds(kDuration);

  web_transport->datagrams()->setOutgoingMaxAge(kDuration);

  ConnectSuccessfully(web_transport, kDurationDelta);
}

TEST_F(WebTransportTest, OnClosed) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  auto* script_state = scope.GetScriptState();
  ScriptPromiseTester tester(script_state,
                             web_transport->closed(scope.GetScriptState()));

  web_transport->OnClosed(
      network::mojom::blink::WebTransportCloseInfo::New(99, "reason"),
      network::mojom::blink::WebTransportStats::New());

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  ScriptValue value = tester.Value();
  ASSERT_FALSE(value.IsEmpty());
  ASSERT_TRUE(value.IsObject());
  WebTransportCloseInfo* close_info = WebTransportCloseInfo::Create(
      isolate, value.V8Value(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(close_info->hasCloseCode());
  EXPECT_TRUE(close_info->hasReason());
  EXPECT_EQ(close_info->closeCode(), 99u);
  EXPECT_EQ(close_info->reason(), "reason");
}

// Regression test for https://crbug.com/347710668.
TEST_F(WebTransportTest, ClosedAccessorCalledAfterOnClosed) {
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  web_transport->OnClosed(
      network::mojom::blink::WebTransportCloseInfo::New(99, "reason"),
      network::mojom::blink::WebTransportStats::New());

  // If this doesn't crash then the test passed.
  EXPECT_FALSE(web_transport->closed(scope.GetScriptState()).IsEmpty());
}

TEST_F(WebTransportTest, OnClosedWithNull) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  auto* script_state = scope.GetScriptState();
  ScriptPromiseTester tester(script_state,
                             web_transport->closed(scope.GetScriptState()));

  web_transport->OnClosed(nullptr,
                          network::mojom::blink::WebTransportStats::New());

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  ScriptValue value = tester.Value();
  ASSERT_FALSE(value.IsEmpty());
  ASSERT_TRUE(value.IsObject());
  WebTransportCloseInfo* close_info = WebTransportCloseInfo::Create(
      isolate, value.V8Value(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(close_info->hasCloseCode());
  EXPECT_TRUE(close_info->hasReason());
}

TEST_F(WebTransportTest, ReceivedResetStream) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  constexpr uint32_t kStreamId = 99;
  constexpr uint32_t kCode = 0xffffffff;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::ScopedDataPipeProducerHandle writable;
  EXPECT_CALL(*mock_web_transport_, CreateStream(Truly(ValidConsumerHandle),
                                                 Truly(ValidProducerHandle), _))
      .WillOnce([&](mojo::ScopedDataPipeConsumerHandle readable_handle,
                    mojo::ScopedDataPipeProducerHandle writable_handle,
                    base::OnceCallback<void(bool, uint32_t)> callback) {
        readable = std::move(readable_handle);
        writable = std::move(writable_handle);
        std::move(callback).Run(true, kStreamId);
      });

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped bidirectional_stream_promise =
      web_transport->createBidirectionalStream(script_state,
                                               ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* bidirectional_stream = V8WebTransportBidirectionalStream::ToWrappable(
      scope.GetIsolate(), tester.Value().V8Value());
  EXPECT_TRUE(bidirectional_stream);

  web_transport->OnReceivedResetStream(kStreamId, kCode);

  ASSERT_TRUE(bidirectional_stream->readable()->IsErrored());
  v8::Local<v8::Value> error_value =
      bidirectional_stream->readable()->GetStoredError(isolate);
  WebTransportError* error =
      V8WebTransportError::ToWrappable(scope.GetIsolate(), error_value);
  ASSERT_TRUE(error);

  EXPECT_EQ(error->streamErrorCode(), kCode);
  EXPECT_EQ(error->source(), "stream");

  EXPECT_TRUE(bidirectional_stream->writable()->IsWritable());
}

TEST_F(WebTransportTest, ReceivedStopSending) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  constexpr uint32_t kStreamId = 51;
  constexpr uint32_t kCode = 255;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::ScopedDataPipeProducerHandle writable;
  EXPECT_CALL(*mock_web_transport_, CreateStream(Truly(ValidConsumerHandle),
                                                 Truly(ValidProducerHandle), _))
      .WillOnce([&](mojo::ScopedDataPipeConsumerHandle readable_handle,
                    mojo::ScopedDataPipeProducerHandle writable_handle,
                    base::OnceCallback<void(bool, uint32_t)> callback) {
        readable = std::move(readable_handle);
        writable = std::move(writable_handle);
        std::move(callback).Run(true, kStreamId);
      });

  auto* script_state = scope.GetScriptState();
  ScriptPromiseUntyped bidirectional_stream_promise =
      web_transport->createBidirectionalStream(script_state,
                                               ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* bidirectional_stream = V8WebTransportBidirectionalStream::ToWrappable(
      scope.GetIsolate(), tester.Value().V8Value());
  EXPECT_TRUE(bidirectional_stream);

  web_transport->OnReceivedStopSending(kStreamId, kCode);

  ASSERT_TRUE(bidirectional_stream->writable()->IsErrored());
  v8::Local<v8::Value> error_value =
      bidirectional_stream->writable()->GetStoredError(isolate);
  WebTransportError* error =
      V8WebTransportError::ToWrappable(scope.GetIsolate(), error_value);
  ASSERT_TRUE(error);

  EXPECT_EQ(error->streamErrorCode(), kCode);
  EXPECT_EQ(error->source(), "stream");

  EXPECT_TRUE(bidirectional_stream->readable()->IsReadable());
}

}  // namespace

}  // namespace blink
