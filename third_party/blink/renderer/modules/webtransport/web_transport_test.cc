// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport.h"

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/http_response_headers.mojom-blink.h"
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
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_byob_reader_read_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_read_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_bidirectional_stream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_close_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_congestion_control.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_hash.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_receive_stream_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_send_stream_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_send_stream_stats.h"
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
#include "third_party/blink/renderer/modules/webtransport/web_transport_receive_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_send_group.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_send_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
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
        Vector<String> application_protocols,
        network::mojom::blink::WebTransportCongestionControl congestion_control,
        std::optional<uint16_t>
            anticipated_concurrent_incoming_unidirectional_streams,
        std::optional<uint16_t>
            anticipated_concurrent_incoming_bidirectional_streams,
        mojo::PendingRemote<network::mojom::blink::WebTransportHandshakeClient>
            handshake_client)
        : url(url),
          fingerprints(std::move(fingerprints)),
          application_protocols(std::move(application_protocols)),
          congestion_control(congestion_control),
          anticipated_concurrent_incoming_unidirectional_streams(
              anticipated_concurrent_incoming_unidirectional_streams),
          anticipated_concurrent_incoming_bidirectional_streams(
              anticipated_concurrent_incoming_bidirectional_streams),
          handshake_client(std::move(handshake_client)) {}

    KURL url;
    Vector<network::mojom::blink::WebTransportCertificateFingerprintPtr>
        fingerprints;
    Vector<String> application_protocols;
    network::mojom::blink::WebTransportCongestionControl congestion_control;
    std::optional<uint16_t>
        anticipated_concurrent_incoming_unidirectional_streams;
    std::optional<uint16_t>
        anticipated_concurrent_incoming_bidirectional_streams;
    mojo::PendingRemote<network::mojom::blink::WebTransportHandshakeClient>
        handshake_client;
  };

  void Connect(
      const KURL& url,
      Vector<network::mojom::blink::WebTransportCertificateFingerprintPtr>
          fingerprints,
      const Vector<String>& application_protocols,
      network::mojom::blink::WebTransportCongestionControl congestion_control,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_unidirectional_streams,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_bidirectional_streams,
      mojo::PendingRemote<network::mojom::blink::WebTransportHandshakeClient>
          handshake_client) override {
    connect_args_.push_back(ConnectArgs(
        url, std::move(fingerprints), application_protocols, congestion_control,
        anticipated_concurrent_incoming_unidirectional_streams,
        anticipated_concurrent_incoming_bidirectional_streams,
        std::move(handshake_client)));
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

  MOCK_METHOD4(
      CreateStream,
      void(mojo::ScopedDataPipeConsumerHandle readable,
           mojo::ScopedDataPipeProducerHandle writable,
           network::mojom::blink::WebTransportStreamPriorityPtr priority,
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
        blink::BindRepeating(&WebTransportTest::BindConnector,
                             weak_ptr_factory_.GetWeakPtr()));
  }

  static WebTransportOptions* EmptyOptions() {
    return MakeGarbageCollected<WebTransportOptions>();
  }

  static WebTransportSendStreamOptions* EmptySendStreamOptions() {
    return MakeGarbageCollected<WebTransportSendStreamOptions>();
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
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200 OK")
            .Build(),
        /*selected_application_protocol=*/String(),
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

  WritableStream* CreateSendStreamSuccessfully(const V8TestingScope& scope,
                                               WebTransport* web_transport) {
    EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
        .WillOnce([this](mojo::ScopedDataPipeConsumerHandle handle, Unused,
                         Unused,
                         base::OnceCallback<void(bool, uint32_t)> callback) {
          send_stream_consumer_handle_ = std::move(handle);
          std::move(callback).Run(true, next_stream_id_++);
        });

    auto* script_state = scope.GetScriptState();
    auto send_stream_promise = web_transport->createUnidirectionalStream(
        script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
    ScriptPromiseTester tester(script_state, send_stream_promise);

    tester.WaitUntilSettled();

    EXPECT_TRUE(tester.IsFulfilled());
    auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                   tester.Value().V8Value());
    EXPECT_TRUE(writable);
    return writable;
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

  ReadableStream* ReadReceiveStream(const V8TestingScope& scope,
                                    WebTransport* web_transport) {
    ReadableStream* streams = web_transport->incomingUnidirectionalStreams();

    v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

    ReadableStream* readable =
        V8ReadableStream::ToWrappable(scope.GetIsolate(), v8value);
    EXPECT_TRUE(readable);

    return readable;
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
  Deque<AcceptUnidirectionalStreamCallback>
      pending_unidirectional_accept_callbacks_;
  Deque<AcceptBidirectionalStreamCallback>
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
      .WillOnce([](base::span<const uint8_t>,
                   MockWebTransport::SendDatagramCallback callback) {
        std::move(callback).Run(true);
      });

  auto* writable = web_transport->datagrams()->writable();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMUint8Array::Create(1);
  *chunk->Data() = 'A';
  auto result =
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
      .WillRepeatedly([](base::span<const uint8_t>,
                         MockWebTransport::SendDatagramCallback callback) {
        std::move(callback).Run(true);
      });

  constexpr uint32_t kMaxBufferedDatagrams = 3;
  web_transport->datagrams()->setOutgoingMaxBufferedDatagrams(
      kMaxBufferedDatagrams);
  auto* writable = web_transport->datagrams()->writable();
  auto* script_state = scope.GetScriptState();
  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromise<IDLUndefined> promise1;
  ScriptPromise<IDLUndefined> promise2;
  ScriptPromise<IDLUndefined> promise3;
  ScriptPromise<IDLUndefined> promise4;

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
  auto result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);

  ConnectSuccessfullyWithoutRunningPendingTasks(web_transport);

  testing::Sequence s;
  EXPECT_CALL(*mock_web_transport_, SendDatagram(ElementsAre('A'), _))
      .WillOnce([](base::span<const uint8_t>,
                   MockWebTransport::SendDatagramCallback callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(*mock_web_transport_, SendDatagram(ElementsAre('N'), _))
      .WillOnce([](base::span<const uint8_t>,
                   MockWebTransport::SendDatagramCallback callback) {
        std::move(callback).Run(true);
      });

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
  auto result =
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
  result.append_range(array->ByteSpan());
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
  auto result = reader->read(script_state, ASSERT_NO_EXCEPTION);
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
  auto result = reader->read(script_state, ASSERT_NO_EXCEPTION);

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
  auto* read_options =
      MakeGarbageCollected<ReadableStreamBYOBReaderReadOptions>();

  auto result =
      reader->read(script_state, view, read_options, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, result);

  const std::array<uint8_t, 1> chunk = {'A'};
  client_remote_->OnDatagramReceived(chunk);

  test::RunPendingTasks();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(GetValueAsVector(script_state, tester.Value()), ElementsAre('A'));
}

TEST_F(WebTransportTest, ReceiveDatagramWithBYOBReaderMinOption) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  auto* readable = web_transport->datagrams()->readable();
  auto* reader =
      readable->GetBYOBReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  // Create a buffer of 4 bytes.
  NotShared<DOMArrayBufferView> view =
      NotShared<DOMUint8Array>(DOMUint8Array::Create(4));

  auto* read_options =
      MakeGarbageCollected<ReadableStreamBYOBReaderReadOptions>();
  read_options->setMin(2);

  // Request to read at least 2 bytes.
  ScriptPromiseTester tester(
      script_state,
      reader->read(script_state, view, read_options, ASSERT_NO_EXCEPTION));

  // Send only 1 byte first. This should not fulfill the read.
  client_remote_->OnDatagramReceived({'A'});
  test::RunPendingTasks();
  // Promise should still be pending.
  EXPECT_FALSE(tester.IsFulfilled());
  EXPECT_FALSE(tester.IsRejected());
  // Send another byte.
  client_remote_->OnDatagramReceived({'B'});
  test::RunPendingTasks();
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(GetValueAsVector(script_state, tester.Value()),
              ElementsAre('A', 'B'));
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
  auto* read_options =
      MakeGarbageCollected<ReadableStreamBYOBReaderReadOptions>();
  read_options->setMin(1);

  auto result =
      reader->read(script_state, view, read_options, ASSERT_NO_EXCEPTION);
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
  auto result1 = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester1(script_state, result1);
  tester1.WaitUntilSettled();
  EXPECT_TRUE(tester1.IsRejected());
}

TEST_F(WebTransportTest, ResettingIncomingMaxBufferedDatagramsWorksAfterClose) {
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

  constexpr uint32_t kNoBufferedDatagrams = 0;
  web_transport->datagrams()->setIncomingMaxBufferedDatagrams(
      kNoBufferedDatagrams);
  auto result = reader->read(script_state, ASSERT_NO_EXCEPTION);

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
  auto result = reader->read(script_state, ASSERT_NO_EXCEPTION);

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
  auto result1 = reader->read(script_state, ASSERT_NO_EXCEPTION);
  auto result2 = reader->read(script_state, ASSERT_NO_EXCEPTION);

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

TEST_F(WebTransportTest, IncomingMaxBufferedDatagramsIsObeyed) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  constexpr uint32_t kMaxBufferedDatagrams = 5;
  web_transport->datagrams()->setIncomingMaxBufferedDatagrams(
      kMaxBufferedDatagrams);

  for (uint32_t i = 0; i < kMaxBufferedDatagrams + 1; ++i) {
    const std::array<uint8_t, 1> chunk = {static_cast<uint8_t>('0' + i)};
    client_remote_->OnDatagramReceived(chunk);
  }

  // Make sure that the calls have run.
  test::RunPendingTasks();

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  for (uint32_t i = 0; i < kMaxBufferedDatagrams; ++i) {
    auto result = reader->read(script_state, ASSERT_NO_EXCEPTION);

    ScriptPromiseTester tester(script_state, result);
    tester.WaitUntilSettled();

    EXPECT_TRUE(tester.IsFulfilled());
    EXPECT_THAT(GetValueAsVector(script_state, tester.Value()),
                ElementsAre('0' + i + 1));
  }
}

TEST_F(WebTransportTest, ResettingMaxBufferedDatagramsClearsQueue) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  constexpr uint32_t kMaxBufferedDatagrams = 5;
  web_transport->datagrams()->setIncomingMaxBufferedDatagrams(
      kMaxBufferedDatagrams);

  // The server sends datagrams before the page starts reading.
  for (uint32_t i = 0; i < kMaxBufferedDatagrams; ++i) {
    const std::array<uint8_t, 1> chunk = {static_cast<uint8_t>('0' + i)};
    client_remote_->OnDatagramReceived(chunk);
  }

  // Process the receive tasks so the datagrams are buffered before the limit
  // changes.
  test::RunPendingTasks();

  constexpr uint32_t kNoBufferedDatagrams = 0;
  // The page asks for a zero-sized queue. The setter clamps that to 1, so the
  // queue is trimmed to one datagram instead of being emptied.
  web_transport->datagrams()->setIncomingMaxBufferedDatagrams(
      kNoBufferedDatagrams);

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  auto result1 = reader->read(script_state, ASSERT_NO_EXCEPTION);
  auto result2 = reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester1(script_state, result1);
  ScriptPromiseTester tester2(script_state, result2);
  tester1.WaitUntilSettled();

  // If trimming kept more than one datagram, this read would complete.
  test::RunPendingTasks();

  // The first read gets the latest datagram retained after trimming.
  EXPECT_TRUE(tester1.IsFulfilled());
  EXPECT_THAT(GetValueAsVector(script_state, tester1.Value()),
              ElementsAre('0' + kMaxBufferedDatagrams - 1));

  // The retained datagram has already been consumed, so this read is still
  // waiting for a future datagram.
  EXPECT_FALSE(tester2.IsFulfilled());
  EXPECT_FALSE(tester2.IsRejected());
}

TEST_F(WebTransportTest,
       ReadIncomingDatagramWorksWithMaxBufferedDatagramsZero) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  constexpr uint32_t kNoBufferedDatagrams = 0;
  web_transport->datagrams()->setIncomingMaxBufferedDatagrams(
      kNoBufferedDatagrams);

  auto* readable = web_transport->datagrams()->readable();
  auto* script_state = scope.GetScriptState();
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  auto result = reader->read(script_state, ASSERT_NO_EXCEPTION);

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

  constexpr uint32_t kMaxBufferedDatagrams = 2;
  web_transport->datagrams()->setIncomingMaxBufferedDatagrams(
      kMaxBufferedDatagrams);

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
  auto result = reader->read(script_state, ASSERT_NO_EXCEPTION);

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

  auto result1 = reader->read(script_state, ASSERT_NO_EXCEPTION);
  auto result2 = reader->read(script_state, ASSERT_NO_EXCEPTION);

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
                           Not(Truly(ValidProducerHandle)), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
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
  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), exception_state);
  EXPECT_TRUE(send_stream_promise.IsEmpty());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kNetworkError),
            exception_state.Code());
}

TEST_F(WebTransportTest, CreateSendStreamFailure) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(false, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
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
  WeakPersistent<WritableStream> send_stream;

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

  WeakPersistent<WritableStream> send_stream;
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
  // We use v8::Persistent instead of ScriptPromise, because
  // ScriptPromise will be broken when CollectAllGarbageForTesting is
  // called.
  v8::Persistent<v8::Promise> close_promise_persistent;

  {
    v8::HandleScope handle_scope(isolate);
    auto close_promise = send_stream->close(script_state, ASSERT_NO_EXCEPTION);
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
        script_state, ScriptPromise<IDLUndefined>::FromV8Promise(
                          isolate, close_promise_persistent.Get(isolate)));
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

  WeakPersistent<WritableStream> send_stream;

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

  WeakPersistent<ReadableStream> receive_stream;
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

  // Eagerly destroy the promise as this test is using manual GC
  // without stack which is incompatible with ScriptValue.
  std::optional<ScriptPromise<IDLUndefined>> cancel_promise;
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

// Tests that when OnIncomingStreamClosed() arrives before the stream is
// created (due to IPC timing), the notification is stored in
// closed_potentially_pending_streams_ and consumed when the stream is
// eventually created. Verifies the fix for crbug.com/358257243.
TEST_F(WebTransportTest, PendingIncomingStreamCloseConsumedOnceStreamExists) {
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  constexpr uint32_t kStreamId = 0;
  EXPECT_FALSE(web_transport->HasPendingClosedStreamForTesting(kStreamId));

  web_transport->OnIncomingStreamClosed(kStreamId, /*fin_received=*/true);
  EXPECT_TRUE(web_transport->HasPendingClosedStreamForTesting(kStreamId));

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();
  ReadableStream* receive_stream = ReadReceiveStream(scope, web_transport);
  ASSERT_TRUE(receive_stream);

  // Creating the ReceiveStream should consume the pending close entry.
  EXPECT_FALSE(web_transport->HasPendingClosedStreamForTesting(kStreamId));

  producer.reset();
  test::RunPendingTasks();
}

// Tests that OnIncomingStreamClosed() notifications for a stream that was
// locally aborted (canceled) before receiving a close notification are properly
// ignored, preventing entries from accumulating in
// closed_potentially_pending_streams_. Part of fix for crbug.com/358257243.
TEST_F(WebTransportTest, CloseIgnoredAfterLocalAbort) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  constexpr uint32_t kStreamId = 0;
  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();
  ReadableStream* receive_stream = ReadReceiveStream(scope, web_transport);
  ASSERT_TRUE(receive_stream);

  // Cancel the stream locally (without receiving OnIncomingStreamClosed first).
  // This triggers ForgetIncomingStream with has_received_close=false,
  // adding kStreamId to recently_forgotten_incoming_stream_ids_.
  auto cancel_promise =
      receive_stream->cancel(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester cancel_tester(script_state, cancel_promise);
  test::RunPendingTasks();
  cancel_tester.WaitUntilSettled();
  EXPECT_TRUE(cancel_tester.IsFulfilled());

  // Now simulate OnIncomingStreamClosed arriving after the local abort.
  // This should be ignored because the stream_id is in the tracking set.
  web_transport->OnIncomingStreamClosed(kStreamId, /*fin_received=*/true);

  // The close should NOT be added to closed_potentially_pending_streams_
  // because the stream was already forgotten and tracked.
  EXPECT_FALSE(web_transport->HasPendingClosedStreamForTesting(kStreamId));
}

TEST_F(WebTransportTest, ReceiveStreamGarbageCollectionRemoteClose) {
  V8TestingScope scope;

  WeakPersistent<ReadableStream> receive_stream;
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

  client_remote_->OnIncomingStreamClosed(/*stream_id=*/0,
                                         /*fin_received=*/false);

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

  WeakPersistent<ReadableStream> receive_stream;
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

  client_remote_->OnIncomingStreamClosed(/*stream_id=*/0,
                                         /*fin_received=*/false);

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
  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
      .WillOnce([&](Unused, Unused, Unused,
                    base::OnceCallback<void(bool, uint32_t)> callback) {
        create_stream_callback = std::move(callback);
      });
  EXPECT_CALL(*mock_web_transport_, Close());

  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
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

  ReadableStream* receive_stream = ReadReceiveStream(scope, web_transport);

  const std::string_view data = "what";
  EXPECT_EQ(producer->WriteAllData(base::as_byte_span(data)), MOJO_RESULT_OK);

  producer.reset();
  web_transport->OnIncomingStreamClosed(/*stream_id=*/0, true);

  auto* reader = receive_stream->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
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

  ReadableStream* receive_stream = ReadReceiveStream(scope, web_transport);

  auto* reader = receive_stream->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state, read_promise);

  web_transport->close(nullptr);

  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  WebTransportError* exception = V8WebTransportError::ToWrappable(
      scope.GetIsolate(), read_tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "WebTransportError");
  EXPECT_EQ(exception->source(), V8WebTransportErrorSource::Enum::kSession);
  EXPECT_EQ(exception->streamErrorCode(), std::nullopt);
}

TEST_F(WebTransportTest, CreateReceiveStreamThenRemoteClose) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();

  ReadableStream* receive_stream = ReadReceiveStream(scope, web_transport);

  auto* reader = receive_stream->GetDefaultReaderForTesting(
      script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester read_tester(script_state, read_promise);

  client_remote_.reset();

  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
  WebTransportError* exception = V8WebTransportError::ToWrappable(
      scope.GetIsolate(), read_tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "WebTransportError");
  EXPECT_EQ(exception->source(), V8WebTransportErrorSource::Enum::kSession);
  EXPECT_EQ(exception->streamErrorCode(), std::nullopt);
}

// BidirectionalStreams are thoroughly tested in bidirectional_stream_test.cc.
// Here we just test the WebTransport APIs.
TEST_F(WebTransportTest, CreateBidirectionalStream) {
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Truly(ValidProducerHandle), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto bidirectional_stream_promise = web_transport->createBidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
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
  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Truly(ValidProducerHandle), _, _))
      .WillOnce([&](mojo::ScopedDataPipeConsumerHandle readable_handle,
                    mojo::ScopedDataPipeProducerHandle writable_handle, Unused,
                    base::OnceCallback<void(bool, uint32_t)> callback) {
        readable = std::move(readable_handle);
        writable = std::move(writable_handle);
        std::move(callback).Run(true, kStreamId);
      });

  auto* script_state = scope.GetScriptState();
  auto bidirectional_stream_promise = web_transport->createBidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
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
  EXPECT_EQ(error->source(), V8WebTransportErrorSource::Enum::kStream);

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
  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Truly(ValidProducerHandle), _, _))
      .WillOnce([&](mojo::ScopedDataPipeConsumerHandle readable_handle,
                    mojo::ScopedDataPipeProducerHandle writable_handle, Unused,
                    base::OnceCallback<void(bool, uint32_t)> callback) {
        readable = std::move(readable_handle);
        writable = std::move(writable_handle);
        std::move(callback).Run(true, kStreamId);
      });

  auto* script_state = scope.GetScriptState();
  auto bidirectional_stream_promise = web_transport->createBidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
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
  EXPECT_EQ(error->source(), V8WebTransportErrorSource::Enum::kStream);

  EXPECT_TRUE(bidirectional_stream->readable()->IsReadable());
}

TEST_F(WebTransportTest, CreateSendGroup) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  auto* group1 = web_transport->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(group1);
  EXPECT_EQ(group1->group_id(), 1u);

  auto* group2 = web_transport->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(group2);
  EXPECT_EQ(group2->group_id(), 2u);

  // Each call should return a distinct object with a unique ID.
  EXPECT_NE(group1, group2);
  EXPECT_NE(group1->group_id(), group2->group_id());
}

TEST_F(WebTransportTest, CreateSendGroupBeforeConnection) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* web_transport = WebTransport::Create(
      scope.GetScriptState(), String("https://example.com/"), EmptyOptions(),
      ASSERT_NO_EXCEPTION);

  // createSendGroup() should work even before the connection is established,
  // since group creation is purely client-side bookkeeping.
  auto* group = web_transport->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(group);
  EXPECT_EQ(group->group_id(), 1u);
}

TEST_F(WebTransportTest, SendGroupGetStatsReturnsZeroedStats) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  auto* group = web_transport->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(group);

  auto* script_state = scope.GetScriptState();
  auto stats_promise = group->getStats(script_state);
  ScriptPromiseTester tester(script_state, stats_promise);

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  // Verify the resolved value is a WebTransportSendStreamStats dictionary
  // with all stats zeroed (stub implementation).
  v8::Local<v8::Value> result = tester.Value().V8Value();
  ASSERT_TRUE(result->IsObject());

  v8::Local<v8::Object> stats_obj = result.As<v8::Object>();
  auto context = script_state->GetContext();
  auto* isolate = script_state->GetIsolate();

  v8::Local<v8::Value> bytes_written;
  ASSERT_TRUE(stats_obj->Get(context, V8AtomicString(isolate, "bytesWritten"))
                  .ToLocal(&bytes_written));
  EXPECT_EQ(bytes_written->IntegerValue(context).ToChecked(), 0);

  v8::Local<v8::Value> bytes_sent;
  ASSERT_TRUE(stats_obj->Get(context, V8AtomicString(isolate, "bytesSent"))
                  .ToLocal(&bytes_sent));
  EXPECT_EQ(bytes_sent->IntegerValue(context).ToChecked(), 0);

  v8::Local<v8::Value> bytes_acknowledged;
  ASSERT_TRUE(
      stats_obj->Get(context, V8AtomicString(isolate, "bytesAcknowledged"))
          .ToLocal(&bytes_acknowledged));
  EXPECT_EQ(bytes_acknowledged->IntegerValue(context).ToChecked(), 0);
}

TEST_F(WebTransportTest, CreateSendGroupAfterClose) {
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  EXPECT_CALL(*mock_web_transport_, Close());

  web_transport->close(nullptr);
  test::RunPendingTasks();

  // createSendGroup() should still succeed after close, since group creation
  // is purely client-side bookkeeping with no network interaction.
  auto* group = web_transport->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(group);
  EXPECT_EQ(group->group_id(), 1u);
}

TEST_F(WebTransportTest, CreateSendGroupOverflow) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  // Simulate being at the limit by setting next_send_group_id_ to max.
  web_transport->SetNextSendGroupIdForTesting(
      std::numeric_limits<uint32_t>::max());

  DummyExceptionStateForTesting exception_state;
  auto* group = web_transport->createSendGroup(exception_state);
  EXPECT_FALSE(group);
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(WebTransportTest, CreateUnidirectionalStreamReturnsSendStream) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Not(Truly(ValidProducerHandle)), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  ASSERT_TRUE(writable);

  // Verify it's a WebTransportSendStream, not a plain SendStream.
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);

  // Default attribute values.
  EXPECT_EQ(send_stream->sendGroup(), nullptr);
  EXPECT_EQ(send_stream->sendOrder(), 0);
}

TEST_F(WebTransportTest, SendStreamSetSendGroup) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Not(Truly(ValidProducerHandle)), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);

  // Create a send group and assign it.
  auto* group = web_transport->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(group);

  send_stream->setSendGroup(group, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(send_stream->sendGroup(), group);

  // Setting to null should also work.
  send_stream->setSendGroup(nullptr, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(send_stream->sendGroup(), nullptr);
}

TEST_F(WebTransportTest, SendStreamSetSendGroupCrossTransportThrows) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport1 =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Not(Truly(ValidProducerHandle)), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto send_stream_promise = web_transport1->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);

  // Create a second (unconnected) WebTransport and a group on it.
  // createSendGroup() works before connection — it's client-side bookkeeping.
  auto* web_transport2 =
      Create(scope, "https://other.example.com", EmptyOptions());
  auto* other_group = web_transport2->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(other_group);

  // Assigning a group from a different transport should throw.
  DummyExceptionStateForTesting exception_state;
  send_stream->setSendGroup(other_group, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  // The group should not have been set.
  EXPECT_EQ(send_stream->sendGroup(), nullptr);
}

TEST_F(WebTransportTest, SendStreamSetSendOrder) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Not(Truly(ValidProducerHandle)), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);

  send_stream->setSendOrder(42);
  EXPECT_EQ(send_stream->sendOrder(), 42);

  send_stream->setSendOrder(-100);
  EXPECT_EQ(send_stream->sendOrder(), -100);
}

TEST_F(WebTransportTest, SendStreamGetStats) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Not(Truly(ValidProducerHandle)), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);

  auto stats_promise = send_stream->getStats(script_state);
  ScriptPromiseTester stats_tester(script_state, stats_promise);

  stats_tester.WaitUntilSettled();
  EXPECT_TRUE(stats_tester.IsFulfilled());

  // Verify the resolved value is a WebTransportSendStreamStats dictionary.
  // Currently a stub that always returns zeroed stats regardless of stream
  // state. TODO(crbug.com/487117768): Replace with real stats from the
  // network service once Mojo plumbing is wired up.
  v8::Local<v8::Value> result = stats_tester.Value().V8Value();
  ASSERT_TRUE(result->IsObject());

  v8::Local<v8::Object> stats_obj = result.As<v8::Object>();
  auto context = script_state->GetContext();
  auto* isolate = script_state->GetIsolate();

  v8::Local<v8::Value> bytes_written;
  ASSERT_TRUE(stats_obj->Get(context, V8AtomicString(isolate, "bytesWritten"))
                  .ToLocal(&bytes_written));
  EXPECT_EQ(bytes_written->IntegerValue(context).ToChecked(), 0);

  v8::Local<v8::Value> bytes_sent;
  ASSERT_TRUE(stats_obj->Get(context, V8AtomicString(isolate, "bytesSent"))
                  .ToLocal(&bytes_sent));
  EXPECT_EQ(bytes_sent->IntegerValue(context).ToChecked(), 0);

  v8::Local<v8::Value> bytes_acknowledged;
  ASSERT_TRUE(
      stats_obj->Get(context, V8AtomicString(isolate, "bytesAcknowledged"))
          .ToLocal(&bytes_acknowledged));
  EXPECT_EQ(bytes_acknowledged->IntegerValue(context).ToChecked(), 0);
}

TEST_F(WebTransportTest, BidirectionalStreamWritableIsSendStream) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Truly(ValidProducerHandle), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto bidirectional_stream_promise = web_transport->createBidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* bidirectional_stream = V8WebTransportBidirectionalStream::ToWrappable(
      scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(bidirectional_stream);

  // Verify the writable side is a WebTransportSendStream.
  auto* writable = bidirectional_stream->writable();
  ASSERT_TRUE(writable);
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);

  // Default attribute values.
  EXPECT_EQ(send_stream->sendGroup(), nullptr);
  EXPECT_EQ(send_stream->sendOrder(), 0);
}

TEST_F(WebTransportTest, CreateSendStreamFlagOffReturnsSendStream) {
  ScopedWebTransportSendGroupForTest scoped_feature(false);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Not(Truly(ValidProducerHandle)), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  ASSERT_TRUE(writable);

  // With the flag off, the result should NOT be a WebTransportSendStream.
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  EXPECT_FALSE(send_stream);
}

TEST_F(WebTransportTest, BidirectionalStreamFlagOffWritableIsNotSendStream) {
  ScopedWebTransportSendGroupForTest scoped_feature(false);
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Truly(ValidProducerHandle), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto bidirectional_stream_promise = web_transport->createBidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  auto* bidirectional_stream = V8WebTransportBidirectionalStream::ToWrappable(
      scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(bidirectional_stream);

  // With the flag off, the writable side should NOT be a
  // WebTransportSendStream.
  auto* writable = bidirectional_stream->writable();
  ASSERT_TRUE(writable);
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  EXPECT_FALSE(send_stream);
}

// Tests for crbug.com/487117768 — sendGroup/sendOrder options at stream
// creation time.

TEST_F(WebTransportTest, CreateUnidirectionalStreamWithSendGroupOption) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* script_state = scope.GetScriptState();

  auto* group = web_transport->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(group);

  auto* options = MakeGarbageCollected<WebTransportSendStreamOptions>();
  options->setSendGroup(group);
  options->setSendOrder(42);

  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
      .WillOnce(
          [](Unused, Unused,
             network::mojom::blink::WebTransportStreamPriorityPtr priority,
             base::OnceCallback<void(bool, uint32_t)> callback) {
            ASSERT_TRUE(priority);
            EXPECT_EQ(priority->send_group_id, std::optional<uint32_t>(1));
            EXPECT_EQ(priority->send_order, 42);
            std::move(callback).Run(true, 0);
          });

  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, options, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  ASSERT_TRUE(writable);
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);
  EXPECT_EQ(send_stream->sendGroup(), group);
  EXPECT_EQ(send_stream->sendOrder(), 42);
}

TEST_F(WebTransportTest, CreateUnidirectionalStreamWithSendOrderOnly) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* script_state = scope.GetScriptState();

  auto* options = MakeGarbageCollected<WebTransportSendStreamOptions>();
  options->setSendOrder(99);

  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
      .WillOnce(
          [](Unused, Unused,
             network::mojom::blink::WebTransportStreamPriorityPtr priority,
             base::OnceCallback<void(bool, uint32_t)> callback) {
            ASSERT_TRUE(priority);
            EXPECT_FALSE(priority->send_group_id.has_value());  // No group.
            EXPECT_EQ(priority->send_order, 99);
            std::move(callback).Run(true, 0);
          });

  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, options, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  ASSERT_TRUE(writable);
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);
  EXPECT_EQ(send_stream->sendGroup(), nullptr);
  EXPECT_EQ(send_stream->sendOrder(), 99);
}

TEST_F(WebTransportTest, CreateBidirectionalStreamWithSendGroupOption) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* script_state = scope.GetScriptState();

  auto* group = web_transport->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(group);

  auto* options = MakeGarbageCollected<WebTransportSendStreamOptions>();
  options->setSendGroup(group);
  options->setSendOrder(7);

  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
      .WillOnce(
          [](Unused, Unused,
             network::mojom::blink::WebTransportStreamPriorityPtr priority,
             base::OnceCallback<void(bool, uint32_t)> callback) {
            ASSERT_TRUE(priority);
            EXPECT_EQ(priority->send_group_id, std::optional<uint32_t>(1));
            EXPECT_EQ(priority->send_order, 7);
            std::move(callback).Run(true, 0);
          });

  auto bidirectional_stream_promise = web_transport->createBidirectionalStream(
      script_state, options, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidirectional_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* bidirectional_stream = V8WebTransportBidirectionalStream::ToWrappable(
      scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(bidirectional_stream);
  auto* send_stream =
      DynamicTo<WebTransportSendStream>(bidirectional_stream->writable());
  ASSERT_TRUE(send_stream);
  EXPECT_EQ(send_stream->sendGroup(), group);
  EXPECT_EQ(send_stream->sendOrder(), 7);
}

TEST_F(WebTransportTest,
       CreateUnidirectionalStreamWithCrossTransportGroupThrows) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport1 =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* script_state = scope.GetScriptState();

  // Create a second (unconnected) WebTransport — only needs createSendGroup(),
  // which is client-side bookkeeping and doesn't require a connection.
  // Using Create() instead of CreateAndConnectSuccessfully() avoids the
  // single-connection DCHECK in the test fixture.
  auto* web_transport2 = Create(scope, "https://example2.com", EmptyOptions());
  auto* other_group = web_transport2->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(other_group);

  auto* options = MakeGarbageCollected<WebTransportSendStreamOptions>();
  options->setSendGroup(other_group);

  // Using a group from a different transport should throw.
  auto& exception_state = scope.GetExceptionState();
  auto send_stream_promise = web_transport1->createUnidirectionalStream(
      script_state, options, exception_state);
  EXPECT_TRUE(send_stream_promise.IsEmpty());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kInvalidStateError),
            exception_state.Code());
}

TEST_F(WebTransportTest, CreateUnidirectionalStreamWithEmptyOptions) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* script_state = scope.GetScriptState();

  // Pass default-constructed options — should behave identically to nullptr.
  auto* options = MakeGarbageCollected<WebTransportSendStreamOptions>();

  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
      .WillOnce(
          [](Unused, Unused,
             network::mojom::blink::WebTransportStreamPriorityPtr priority,
             base::OnceCallback<void(bool, uint32_t)> callback) {
            // Default options: no group, send_order 0 — priority should be
            // null.
            EXPECT_FALSE(priority);
            std::move(callback).Run(true, 0);
          });

  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, options, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  ASSERT_TRUE(writable);
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);
  EXPECT_EQ(send_stream->sendGroup(), nullptr);
  EXPECT_EQ(send_stream->sendOrder(), 0);
}

TEST_F(WebTransportTest, CreateUnidirectionalStreamWithExplicitNullGroup) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* script_state = scope.GetScriptState();

  // Explicitly set sendGroup to null — spec allows this.
  auto* options = MakeGarbageCollected<WebTransportSendStreamOptions>();
  options->setSendGroup(nullptr);
  options->setSendOrder(5);

  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
      .WillOnce(
          [](Unused, Unused,
             network::mojom::blink::WebTransportStreamPriorityPtr priority,
             base::OnceCallback<void(bool, uint32_t)> callback) {
            // Null group + non-zero send_order → priority should be present.
            ASSERT_TRUE(priority);
            EXPECT_FALSE(priority->send_group_id.has_value());  // No group.
            EXPECT_EQ(priority->send_order, 5);
            std::move(callback).Run(true, 0);
          });

  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, options, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  ASSERT_TRUE(writable);
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  ASSERT_TRUE(send_stream);
  EXPECT_EQ(send_stream->sendGroup(), nullptr);
  EXPECT_EQ(send_stream->sendOrder(), 5);
}

TEST_F(WebTransportTest, CreateUnidirectionalStreamWithOptionsFlagOff) {
  // When the WebTransportSendGroup flag is OFF, options should be accepted
  // but sendGroup is not available. The stream is a plain SendStream.
  ScopedWebTransportSendGroupForTest scoped_feature(false);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* script_state = scope.GetScriptState();

  // Pass options with sendOrder — sendGroup is not available when flag is off.
  auto* options = MakeGarbageCollected<WebTransportSendStreamOptions>();
  options->setSendOrder(10);

  EXPECT_CALL(*mock_web_transport_, CreateStream(_, _, _, _))
      .WillOnce(
          [](Unused, Unused,
             network::mojom::blink::WebTransportStreamPriorityPtr priority,
             base::OnceCallback<void(bool, uint32_t)> callback) {
            // send_order is non-zero, so priority should be present even with
            // the SendGroup feature flag off.
            ASSERT_TRUE(priority);
            EXPECT_FALSE(priority->send_group_id.has_value());  // No group.
            EXPECT_EQ(priority->send_order, 10);
            std::move(callback).Run(true, 0);
          });

  auto send_stream_promise = web_transport->createUnidirectionalStream(
      script_state, options, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, send_stream_promise);

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* writable = V8WritableStream::ToWrappable(scope.GetIsolate(),
                                                 tester.Value().V8Value());
  ASSERT_TRUE(writable);
  // When the flag is off, the stream should NOT be a WebTransportSendStream.
  auto* send_stream = DynamicTo<WebTransportSendStream>(writable);
  EXPECT_FALSE(send_stream);
}

TEST_F(WebTransportTest,
       CreateBidirectionalStreamWithCrossTransportGroupThrows) {
  ScopedWebTransportSendGroupForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport1 =
      CreateAndConnectSuccessfully(scope, "https://example.com");
  auto* script_state = scope.GetScriptState();

  // Second transport is unconnected — only needs createSendGroup().
  auto* web_transport2 = Create(scope, "https://example2.com", EmptyOptions());
  auto* other_group = web_transport2->createSendGroup(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(other_group);

  auto* options = MakeGarbageCollected<WebTransportSendStreamOptions>();
  options->setSendGroup(other_group);

  auto& exception_state = scope.GetExceptionState();
  auto bidirectional_stream_promise = web_transport1->createBidirectionalStream(
      script_state, options, exception_state);
  EXPECT_TRUE(bidirectional_stream_promise.IsEmpty());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kInvalidStateError),
            exception_state.Code());
}

TEST_F(WebTransportTest, CongestionControlThroughput) {
  ScopedWebTransportCongestionControlForTest scoped_feature(true);
  V8TestingScope scope;
  AddBinder(scope);

  auto* options = MakeGarbageCollected<WebTransportOptions>();
  options->setCongestionControl(V8WebTransportCongestionControl(
      V8WebTransportCongestionControl::Enum::kThroughput));

  auto* web_transport = WebTransport::Create(scope.GetScriptState(),
                                             String("https://example.com/"),
                                             options, ASSERT_NO_EXCEPTION);
  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());

  // Verify the Mojo IPC carries the correct enum value.
  EXPECT_EQ(args[0].congestion_control,
            network::mojom::blink::WebTransportCongestionControl::kThroughput);

  // Verify the getter returns what was set.
  EXPECT_EQ(web_transport->congestionControl().AsEnum(),
            V8WebTransportCongestionControl::Enum::kThroughput);
}

TEST_F(WebTransportTest, CongestionControlDefaultSendsKDefault) {
  ScopedWebTransportCongestionControlForTest scoped_feature(true);
  V8TestingScope scope;
  AddBinder(scope);

  // No congestionControl option set — should send kDefault over Mojo.
  // Create is called for its side-effect (triggering the Mojo Connect).
  WebTransport::Create(scope.GetScriptState(), String("https://example.com/"),
                       EmptyOptions(), ASSERT_NO_EXCEPTION);
  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  EXPECT_EQ(args[0].congestion_control,
            network::mojom::blink::WebTransportCongestionControl::kDefault);
}

TEST_F(WebTransportTest, CongestionControlFlagOff) {
  ScopedWebTransportCongestionControlForTest scoped_feature(false);
  V8TestingScope scope;
  AddBinder(scope);

  auto* options = MakeGarbageCollected<WebTransportOptions>();
  options->setCongestionControl(V8WebTransportCongestionControl(
      V8WebTransportCongestionControl::Enum::kLowLatency));

  auto* web_transport = WebTransport::Create(scope.GetScriptState(),
                                             String("https://example.com/"),
                                             options, ASSERT_NO_EXCEPTION);
  test::RunPendingTasks();

  // When the flag is off, the getter should return "default" regardless
  // of what was set in the constructor options.
  EXPECT_EQ(web_transport->congestionControl().AsEnum(),
            V8WebTransportCongestionControl::Enum::kDefault);

  // The Mojo IPC should also send kDefault when the flag is off.
  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  EXPECT_EQ(args[0].congestion_control,
            network::mojom::blink::WebTransportCongestionControl::kDefault);
}

// ---- WebTransportReceiveStream (crbug.com/487117768) ---------------------
// These tests cover the receive-side counterpart of the WebTransportSendStream
// tests above. Behavior of the IncomingStream itself is exercised in
// incoming_stream_test.cc; here we just verify the wrapper class chosen by
// the WebTransportReceiveStream runtime flag.

TEST_F(WebTransportTest, IncomingUnidirectionalStreamIsReceiveStream) {
  ScopedWebTransportReceiveStreamForTest scoped_feature(true);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();
  ReadableStream* streams = web_transport->incomingUnidirectionalStreams();
  v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

  ReadableStream* readable =
      V8ReadableStream::ToWrappable(scope.GetIsolate(), v8value);
  ASSERT_TRUE(readable);

  // Verify it's a WebTransportReceiveStream (flag on), not a plain
  // ReceiveStream.
  auto* receive_stream = DynamicTo<WebTransportReceiveStream>(readable);
  ASSERT_TRUE(receive_stream);
  EXPECT_TRUE(receive_stream->GetIncomingStream());

  producer.reset();
  web_transport->OnIncomingStreamClosed(/*stream_id=*/0, true);
}

TEST_F(WebTransportTest, IncomingUnidirectionalStreamFlagOffIsLegacyReceive) {
  ScopedWebTransportReceiveStreamForTest scoped_feature(false);
  V8TestingScope scope;
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();
  ReadableStream* streams = web_transport->incomingUnidirectionalStreams();
  v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);

  ReadableStream* readable =
      V8ReadableStream::ToWrappable(scope.GetIsolate(), v8value);
  ASSERT_TRUE(readable);

  // With the flag off the wrapper should NOT be a WebTransportReceiveStream.
  EXPECT_FALSE(DynamicTo<WebTransportReceiveStream>(readable));

  producer.reset();
  web_transport->OnIncomingStreamClosed(/*stream_id=*/0, true);
}

TEST_F(WebTransportTest, ReceiveStreamGetStatsReturnsZeroedStub) {
  ScopedWebTransportReceiveStreamForTest scoped_feature(true);
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();
  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  mojo::ScopedDataPipeProducerHandle producer = DoAcceptUnidirectionalStream();
  ReadableStream* streams = web_transport->incomingUnidirectionalStreams();
  v8::Local<v8::Value> v8value = ReadValueFromStream(scope, streams);
  auto* readable = V8ReadableStream::ToWrappable(scope.GetIsolate(), v8value);
  auto* receive_stream = DynamicTo<WebTransportReceiveStream>(readable);
  ASSERT_TRUE(receive_stream);

  auto stats_promise = receive_stream->getStats(script_state);
  ScriptPromiseTester stats_tester(script_state, stats_promise);
  stats_tester.WaitUntilSettled();
  EXPECT_TRUE(stats_tester.IsFulfilled());

  // The resolved value is a WebTransportReceiveStreamStats dictionary. Stub
  // returns zeroed stats; TODO(crbug.com/510589920) replace with real Mojo
  // data.
  v8::Local<v8::Value> result = stats_tester.Value().V8Value();
  ASSERT_TRUE(result->IsObject());
  v8::Local<v8::Object> stats_obj = result.As<v8::Object>();
  auto context = script_state->GetContext();
  auto* isolate = script_state->GetIsolate();

  v8::Local<v8::Value> bytes_received;
  ASSERT_TRUE(stats_obj->Get(context, V8AtomicString(isolate, "bytesReceived"))
                  .ToLocal(&bytes_received));
  EXPECT_EQ(bytes_received->IntegerValue(context).ToChecked(), 0);

  v8::Local<v8::Value> bytes_read;
  ASSERT_TRUE(stats_obj->Get(context, V8AtomicString(isolate, "bytesRead"))
                  .ToLocal(&bytes_read));
  EXPECT_EQ(bytes_read->IntegerValue(context).ToChecked(), 0);

  producer.reset();
  web_transport->OnIncomingStreamClosed(/*stream_id=*/0, true);
}

TEST_F(WebTransportTest, BidirectionalStreamReadableIsReceiveStream) {
  ScopedWebTransportReceiveStreamForTest scoped_feature(true);
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Truly(ValidProducerHandle), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto bidi_promise = web_transport->createBidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidi_promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* bidi = V8WebTransportBidirectionalStream::ToWrappable(
      scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(bidi);

  auto* readable = bidi->readable();
  ASSERT_TRUE(readable);
  auto* receive_stream = DynamicTo<WebTransportReceiveStream>(readable);
  ASSERT_TRUE(receive_stream);
}

TEST_F(WebTransportTest, BidirectionalStreamFlagOffReadableIsLegacyReceive) {
  ScopedWebTransportReceiveStreamForTest scoped_feature(false);
  V8TestingScope scope;

  auto* web_transport =
      CreateAndConnectSuccessfully(scope, "https://example.com");

  EXPECT_CALL(*mock_web_transport_,
              CreateStream(Truly(ValidConsumerHandle),
                           Truly(ValidProducerHandle), _, _))
      .WillOnce([](Unused, Unused, Unused,
                   base::OnceCallback<void(bool, uint32_t)> callback) {
        std::move(callback).Run(true, 0);
      });

  auto* script_state = scope.GetScriptState();
  auto bidi_promise = web_transport->createBidirectionalStream(
      script_state, EmptySendStreamOptions(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state, bidi_promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* bidi = V8WebTransportBidirectionalStream::ToWrappable(
      scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(bidi);
  EXPECT_FALSE(DynamicTo<WebTransportReceiveStream>(bidi->readable()));
}

// Regression test for the nullptr guard in web_transport_receive_stream.cc's
// ForgetStream callback. The callback is bound with
// WrapWeakPersistent(web_transport), so if WebTransport is garbage-collected
// before the IncomingStream fires on_abort_, ForgetStream sees `transport ==
// nullptr` and must return early instead of dereferencing.
//
// To exercise the path we must avoid the natural accept flow (which registers
// the stream in WebTransport's incoming_stream_map_ — a strong ref that keeps
// WebTransport alive transitively). Instead we construct
// WebTransportReceiveStream directly so the stream is NOT in the map, then
// break the mojo connection (Cleanup() iterates the map and would reset
// on_abort_ on entries it finds — ours isn't there, so on_abort_ survives),
// GC WebTransport, then trigger AbortAndReset via OnIncomingStreamClosed +
// pipe close. Removing the guard makes this test crash with a nullptr deref
// in WebTransport::ForgetIncomingStream.
TEST_F(WebTransportTest,
       WebTransportReceiveStreamForgetStreamSurvivesWebTransportGC) {
  ScopedWebTransportReceiveStreamForTest scoped_feature(true);

  V8TestingScope scope;

  // Build the data pipe ourselves so we own the producer end and can close it
  // later to drive HandlePipeClosed().
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = 0;
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(&options, producer, consumer), MOJO_RESULT_OK);

  WeakPersistent<WebTransport> web_transport;
  Persistent<WebTransportReceiveStream> receive_stream;

  {
    v8::HandleScope handle_scope(scope.GetIsolate());

    auto* wt = CreateAndConnectSuccessfully(scope, "https://example.com");
    web_transport = wt;

    // Construct WebTransportReceiveStream directly. Intentionally does NOT
    // call wt->incoming_stream_map_.insert(...) — so WebTransport has no
    // strong reference to our stream. The constructor binds ForgetStream
    // with WrapWeakPersistent(wt), exactly mirroring production.
    auto* s = MakeGarbageCollected<WebTransportReceiveStream>(
        scope.GetScriptState(), wt, /*stream_id=*/0, std::move(consumer));
    s->Init(ASSERT_NO_EXCEPTION);
    receive_stream = s;
  }

  // Break the mojo connection so WebTransport becomes collectable. Cleanup()
  // iterates incoming_stream_map_ and calls Error() (which resets on_abort_)
  // on each entry — but our stream isn't in the map, so its callback
  // survives intact.
  client_remote_.reset();
  test::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(web_transport)
      << "WebTransport should be GC'd; test scenario not exercised";
  ASSERT_TRUE(receive_stream);

  // Both signals are required to reach ProcessClose() → CloseAbortAndReset()
  // → AbortAndReset() → on_abort_.Run().
  receive_stream->GetIncomingStream()->OnIncomingStreamClosed(
      /*fin_received=*/true);
  producer.reset();
  test::RunPendingTasks();

  // If we reach here without crashing, the nullptr guard worked. Without it,
  // transport->ForgetIncomingStream(...) would SEGV on the null pointer.
  EXPECT_TRUE(receive_stream);
}

}  // namespace

}  // namespace blink
