// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_stream_create_helper.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/completion_once_callback.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/quic/test_task_runner.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_flags.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/qpack/qpack_decoder.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_stream.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {
class HttpNetworkSession;
class URLRequest;
class WebSocketHttp2HandshakeStream;
class WebSocketHttp3HandshakeStream;
class X509Certificate;
struct WebSocketHandshakeRequestInfo;
struct WebSocketHandshakeResponseInfo;
}  // namespace net

using ::net::test::IsError;
using ::net::test::IsOk;
using ::testing::_;
using ::testing::StrictMock;
using ::testing::TestWithParam;
using ::testing::Values;

namespace net {
namespace {

enum HandshakeStreamType {
  BASIC_HANDSHAKE_STREAM,
  HTTP2_HANDSHAKE_STREAM,
  HTTP3_HANDSHAKE_STREAM
};

// This class encapsulates the details of creating a mock ClientSocketHandle.
class MockClientSocketHandleFactory {
 public:
  MockClientSocketHandleFactory()
      : common_connect_job_params_(
            socket_factory_maker_.factory(),
            /*host_resolver=*/nullptr,
            /*http_auth_cache=*/nullptr,
            /*http_auth_handler_factory=*/nullptr,
            /*spdy_session_pool=*/nullptr,
            /*quic_supported_versions=*/nullptr,
            /*quic_session_pool=*/nullptr,
            /*proxy_delegate=*/nullptr,
            /*http_user_agent_settings=*/nullptr,
            /*ssl_client_context=*/nullptr,
            /*socket_performance_watcher_factory=*/nullptr,
            /*network_quality_estimator=*/nullptr,
            /*net_log=*/nullptr,
            /*websocket_endpoint_lock_manager=*/nullptr,
            /*http_server_properties=*/nullptr,
            /*alpn_protos=*/nullptr,
            /*application_settings=*/nullptr,
            /*ignore_certificate_errors=*/nullptr,
            /*early_data_enabled=*/nullptr),
        pool_(1, 1, &common_connect_job_params_) {}

  MockClientSocketHandleFactory(const MockClientSocketHandleFactory&) = delete;
  MockClientSocketHandleFactory& operator=(
      const MockClientSocketHandleFactory&) = delete;

  // The created socket expects |expect_written| to be written to the socket,
  // and will respond with |return_to_read|. The test will fail if the expected
  // text is not written, or if all the bytes are not read.
  std::unique_ptr<ClientSocketHandle> CreateClientSocketHandle(
      const std::string& expect_written,
      const std::string& return_to_read) {
    socket_factory_maker_.SetExpectations(expect_written, return_to_read);
    auto socket_handle = std::make_unique<ClientSocketHandle>();
    socket_handle->Init(
        ClientSocketPool::GroupId(
            url::SchemeHostPort(url::kHttpScheme, "a", 80),
            PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
            SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false),
        scoped_refptr<ClientSocketPool::SocketParams>(),
        std::nullopt /* proxy_annotation_tag */, MEDIUM, SocketTag(),
        ClientSocketPool::RespectLimits::ENABLED, CompletionOnceCallback(),
        ClientSocketPool::ProxyAuthCallback(), &pool_, NetLogWithSource());
    return socket_handle;
  }

 private:
  WebSocketMockClientSocketFactoryMaker socket_factory_maker_;
  const CommonConnectJobParams common_connect_job_params_;
  MockTransportClientSocketPool pool_;
};

class TestConnectDelegate : public WebSocketStream::ConnectDelegate {
 public:
  ~TestConnectDelegate() override = default;

  void OnCreateRequest(URLRequest* request) override {}
  void OnURLRequestConnected(URLRequest* request,
                             const TransportInfo& info) override {}
  void OnSuccess(
      std::unique_ptr<WebSocketStream> stream,
      std::unique_ptr<WebSocketHandshakeResponseInfo> response) override {}
  void OnFailure(const std::string& failure_message,
                 int net_error,
                 std::optional<int> response_code) override {}
  void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo> request) override {}
  void OnSSLCertificateError(
      std::unique_ptr<WebSocketEventInterface::SSLErrorCallbacks>
          ssl_error_callbacks,
      int net_error,
      const SSLInfo& ssl_info,
      bool fatal) override {}
  int OnAuthRequired(const AuthChallengeInfo& auth_info,
                     scoped_refptr<HttpResponseHeaders> response_headers,
                     const IPEndPoint& host_port_pair,
                     base::OnceCallback<void(const AuthCredentials*)> callback,
                     std::optional<AuthCredentials>* credentials) override {
    *credentials = std::nullopt;
    return OK;
  }
};

class MockWebSocketStreamRequestAPI : public WebSocketStreamRequestAPI {
 public:
  ~MockWebSocketStreamRequestAPI() override = default;

  MOCK_METHOD1(OnBasicHandshakeStreamCreated,
               void(WebSocketBasicHandshakeStream* handshake_stream));
  MOCK_METHOD1(OnHttp2HandshakeStreamCreated,
               void(WebSocketHttp2HandshakeStream* handshake_stream));
  MOCK_METHOD1(OnHttp3HandshakeStreamCreated,
               void(WebSocketHttp3HandshakeStream* handshake_stream));
  MOCK_METHOD3(OnFailure,
               void(const std::string& message,
                    int net_error,
                    std::optional<int> response_code));
};

class WebSocketHandshakeStreamCreateHelperTest
    : public TestWithParam<HandshakeStreamType>,
      public WithTaskEnvironment {
 protected:
  WebSocketHandshakeStreamCreateHelperTest()
      : quic_version_(quic::HandshakeProtocol::PROTOCOL_TLS1_3,
                      quic::QuicTransportVersion::QUIC_VERSION_IETF_RFC_V1),
        mock_quic_data_(quic_version_) {}
  std::unique_ptr<WebSocketStream> CreateAndInitializeStream(
      const std::vector<std::string>& sub_protocols,
      const WebSocketExtraHeaders& extra_request_headers,
      const WebSocketExtraHeaders& extra_response_headers) {
    constexpr char kPath[] = "/";
    constexpr char kOrigin[] = "http://origin.example.org";
    const GURL url("wss://www.example.org/");
    NetLogWithSource net_log;

    WebSocketHandshakeStreamCreateHelper create_helper(
        &connect_delegate_, sub_protocols, &stream_request_);

    switch (GetParam()) {
      case BASIC_HANDSHAKE_STREAM:
        EXPECT_CALL(stream_request_, OnBasicHandshakeStreamCreated(_)).Times(1);
        break;

      case HTTP2_HANDSHAKE_STREAM:
        EXPECT_CALL(stream_request_, OnHttp2HandshakeStreamCreated(_)).Times(1);
        break;

      case HTTP3_HANDSHAKE_STREAM:
        EXPECT_CALL(stream_request_, OnHttp3HandshakeStreamCreated(_)).Times(1);
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }

    EXPECT_CALL(stream_request_, OnFailure(_, _, _)).Times(0);

    HttpRequestInfo request_info;
    request_info.url = url;
    request_info.method = "GET";
    request_info.load_flags = LOAD_DISABLE_CACHE;
    request_info.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

    auto headers = WebSocketCommonTestHeaders();

    switch (GetParam()) {
      case BASIC_HANDSHAKE_STREAM: {
        std::unique_ptr<ClientSocketHandle> socket_handle =
            socket_handle_factory_.CreateClientSocketHandle(
                WebSocketStandardRequest(kPath, "www.example.org",
                                         url::Origin::Create(GURL(kOrigin)),
                                         /*send_additional_request_headers=*/{},
                                         extra_request_headers),
                WebSocketStandardResponse(
                    WebSocketExtraHeadersToString(extra_response_headers)));

        std::unique_ptr<WebSocketHandshakeStreamBase> handshake =
            create_helper.CreateBasicStream(std::move(socket_handle), false,
                                            &websocket_endpoint_lock_manager_);

        // If in future the implementation type returned by CreateBasicStream()
        // changes, this static_cast will be wrong. However, in that case the
        // test will fail and AddressSanitizer should identify the issue.
        static_cast<WebSocketBasicHandshakeStream*>(handshake.get())
            ->SetWebSocketKeyForTesting("dGhlIHNhbXBsZSBub25jZQ==");

        handshake->RegisterRequest(&request_info);
        int rv = handshake->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                             CompletionOnceCallback());
        EXPECT_THAT(rv, IsOk());

        HttpResponseInfo response;
        TestCompletionCallback request_callback;
        rv = handshake->SendRequest(headers, &response,
                                    request_callback.callback());
        EXPECT_THAT(rv, IsOk());

        TestCompletionCallback response_callback;
        rv = handshake->ReadResponseHeaders(response_callback.callback());
        EXPECT_THAT(rv, IsOk());
        EXPECT_EQ(101, response.headers->response_code());
        EXPECT_TRUE(response.headers->HasHeaderValue("Connection", "Upgrade"));
        EXPECT_TRUE(response.headers->HasHeaderValue("Upgrade", "websocket"));
        return handshake->Upgrade();
      }
      case HTTP2_HANDSHAKE_STREAM: {
        SpdyTestUtil spdy_util;
        quiche::HttpHeaderBlock request_header_block = WebSocketHttp2Request(
            kPath, "www.example.org", kOrigin, extra_request_headers);
        spdy::SpdySerializedFrame request_headers(
            spdy_util.ConstructSpdyHeaders(1, std::move(request_header_block),
                                           DEFAULT_PRIORITY, false));
        MockWrite writes[] = {CreateMockWrite(request_headers, 0)};

        quiche::HttpHeaderBlock response_header_block =
            WebSocketHttp2Response(extra_response_headers);
        spdy::SpdySerializedFrame response_headers(
            spdy_util.ConstructSpdyResponseHeaders(
                1, std::move(response_header_block), false));
        MockRead reads[] = {CreateMockRead(response_headers, 1),
                            MockRead(ASYNC, 0, 2)};

        SequencedSocketData data(reads, writes);

        SSLSocketDataProvider ssl(ASYNC, OK);
        ssl.ssl_info.cert =
            ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");

        SpdySessionDependencies session_deps;
        session_deps.socket_factory->AddSocketDataProvider(&data);
        session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

        std::unique_ptr<HttpNetworkSession> http_network_session =
            SpdySessionDependencies::SpdyCreateSession(&session_deps);
        const SpdySessionKey key(
            HostPortPair::FromURL(url), PRIVACY_MODE_DISABLED,
            ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
            /*disable_cert_verification_network_fetches=*/false);
        base::WeakPtr<SpdySession> spdy_session =
            CreateSpdySession(http_network_session.get(), key, net_log);
        std::unique_ptr<WebSocketHandshakeStreamBase> handshake =
            create_helper.CreateHttp2Stream(spdy_session, {} /* dns_aliases */);

        handshake->RegisterRequest(&request_info);
        int rv = handshake->InitializeStream(true, DEFAULT_PRIORITY,
                                             NetLogWithSource(),
                                             CompletionOnceCallback());
        EXPECT_THAT(rv, IsOk());

        HttpResponseInfo response;
        TestCompletionCallback request_callback;
        rv = handshake->SendRequest(headers, &response,
                                    request_callback.callback());
        EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
        rv = request_callback.WaitForResult();
        EXPECT_THAT(rv, IsOk());

        TestCompletionCallback response_callback;
        rv = handshake->ReadResponseHeaders(response_callback.callback());
        EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
        rv = response_callback.WaitForResult();
        EXPECT_THAT(rv, IsOk());

        EXPECT_EQ(200, response.headers->response_code());
        return handshake->Upgrade();
      }
      case HTTP3_HANDSHAKE_STREAM: {
        const quic::QuicStreamId client_data_stream_id(
            quic::QuicUtils::GetFirstBidirectionalStreamId(
                quic_version_.transport_version, quic::Perspective::IS_CLIENT));
        quic::QuicCryptoClientConfig crypto_config(
            quic::test::crypto_test_utils::ProofVerifierForTesting());

        const quic::QuicConnectionId connection_id(
            quic::test::TestConnectionId(2));
        test::QuicTestPacketMaker client_maker(
            quic_version_, connection_id, &clock_, "mail.example.org",
            quic::Perspective::IS_CLIENT,
            /*client_headers_include_h2_stream_dependency_=*/false);
        test::QuicTestPacketMaker server_maker(
            quic_version_, connection_id, &clock_, "mail.example.org",
            quic::Perspective::IS_SERVER,
            /*client_headers_include_h2_stream_dependency_=*/false);
        IPEndPoint peer_addr(IPAddress(192, 0, 2, 23), 443);
        quic::test::MockConnectionIdGenerator connection_id_generator;

        testing::StrictMock<quic::test::MockQuicConnectionVisitor> visitor;
        ProofVerifyDetailsChromium verify_details;
        MockCryptoClientStreamFactory crypto_client_stream_factory;
        TransportSecurityState transport_security_state;
        SSLConfigServiceDefaults ssl_config_service;

        FLAGS_quic_enable_http3_grease_randomness = false;
        clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
        quic::QuicEnableVersion(quic_version_);
        quic::test::MockRandom random_generator{0};

        quiche::HttpHeaderBlock request_header_block = WebSocketHttp2Request(
            kPath, "www.example.org", kOrigin, extra_request_headers);

        int packet_number = 1;
        mock_quic_data_.AddWrite(
            SYNCHRONOUS,
            client_maker.MakeInitialSettingsPacket(packet_number++));

        mock_quic_data_.AddWrite(
            ASYNC,
            client_maker.MakeRequestHeadersPacket(
                packet_number++, client_data_stream_id,
                /*fin=*/false, ConvertRequestPriorityToQuicPriority(LOWEST),
                std::move(request_header_block), nullptr));

        quiche::HttpHeaderBlock response_header_block =
            WebSocketHttp2Response(extra_response_headers);

        mock_quic_data_.AddRead(
            ASYNC, server_maker.MakeResponseHeadersPacket(
                       /*packet_number=*/1, client_data_stream_id,
                       /*fin=*/false, std::move(response_header_block),
                       /*spdy_headers_frame_length=*/nullptr));

        mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

        mock_quic_data_.AddWrite(
            SYNCHRONOUS,
            client_maker.Packet(packet_number++)
                .AddAckFrame(/*first_received=*/1, /*largest_received=*/1,
                             /*smallest_received=*/0)
                .AddStopSendingFrame(client_data_stream_id,
                                     quic::QUIC_STREAM_CANCELLED)
                .AddRstStreamFrame(client_data_stream_id,
                                   quic::QUIC_STREAM_CANCELLED)
                .Build());
        auto socket = std::make_unique<MockUDPClientSocket>(
            mock_quic_data_.InitializeAndGetSequencedSocketData(),
            NetLog::Get());
        socket->Connect(peer_addr);

        scoped_refptr<test::TestTaskRunner> runner =
            base::MakeRefCounted<test::TestTaskRunner>(&clock_);
        auto helper = std::make_unique<QuicChromiumConnectionHelper>(
            &clock_, &random_generator);
        auto alarm_factory =
            std::make_unique<QuicChromiumAlarmFactory>(runner.get(), &clock_);
        // Ownership of 'writer' is passed to 'QuicConnection'.
        QuicChromiumPacketWriter* writer = new QuicChromiumPacketWriter(
            socket.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get());
        quic::QuicConnection* connection = new quic::QuicConnection(
            connection_id, quic::QuicSocketAddress(),
            net::ToQuicSocketAddress(peer_addr), helper.get(),
            alarm_factory.get(), writer, true /* owns_writer */,
            quic::Perspective::IS_CLIENT,
            quic::test::SupportedVersions(quic_version_),
            connection_id_generator);
        connection->set_visitor(&visitor);

        // Load a certificate that is valid for *.example.org
        scoped_refptr<X509Certificate> test_cert(
            ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
        EXPECT_TRUE(test_cert.get());

        verify_details.cert_verify_result.verified_cert = test_cert;
        verify_details.cert_verify_result.is_issued_by_known_root = true;
        crypto_client_stream_factory.AddProofVerifyDetails(&verify_details);

        base::TimeTicks dns_end = base::TimeTicks::Now();
        base::TimeTicks dns_start = dns_end - base::Milliseconds(1);

        session_ = std::make_unique<QuicChromiumClientSession>(
            connection, std::move(socket),
            /*stream_factory=*/nullptr, &crypto_client_stream_factory, &clock_,
            &transport_security_state, &ssl_config_service,
            /*server_info=*/nullptr,
            QuicSessionAliasKey(
                url::SchemeHostPort(),
                QuicSessionKey("mail.example.org", 80, PRIVACY_MODE_DISABLED,
                               ProxyChain::Direct(), SessionUsage::kDestination,
                               SocketTag(), NetworkAnonymizationKey(),
                               SecureDnsPolicy::kAllow,
                               /*require_dns_https_alpn=*/false)),
            /*require_confirmation=*/false,
            /*migrate_session_early_v2=*/false,
            /*migrate_session_on_network_change_v2=*/false,
            /*default_network=*/handles::kInvalidNetworkHandle,
            quic::QuicTime::Delta::FromMilliseconds(
                kDefaultRetransmittableOnWireTimeout.InMilliseconds()),
            /*migrate_idle_session=*/true, /*allow_port_migration=*/false,
            kDefaultIdleSessionMigrationPeriod,
            /*multi_port_probing_interval=*/0, kMaxTimeOnNonDefaultNetwork,
            kMaxMigrationsToNonDefaultNetworkOnWriteError,
            kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
            kQuicYieldAfterPacketsRead,
            quic::QuicTime::Delta::FromMilliseconds(
                kQuicYieldAfterDurationMilliseconds),
            /*cert_verify_flags=*/0, quic::test::DefaultQuicConfig(),
            std::make_unique<TestQuicCryptoClientConfigHandle>(&crypto_config),
            "CONNECTION_UNKNOWN", dns_start, dns_end,
            base::DefaultTickClock::GetInstance(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get(),
            /*socket_performance_watcher=*/nullptr,
            ConnectionEndpointMetadata(), /*report_ecn=*/true,
            /*enable_origin_frame=*/true,
            NetLogWithSource::Make(NetLogSourceType::NONE));

        session_->Initialize();

        // Blackhole QPACK decoder stream instead of constructing mock writes.
        session_->qpack_decoder()->set_qpack_stream_sender_delegate(
            &noop_qpack_stream_sender_delegate_);
        TestCompletionCallback callback;
        EXPECT_THAT(session_->CryptoConnect(callback.callback()), IsOk());
        EXPECT_TRUE(session_->OneRttKeysAvailable());
        std::unique_ptr<QuicChromiumClientSession::Handle> session_handle =
            session_->CreateHandle(
                url::SchemeHostPort(url::kHttpsScheme, "mail.example.org", 80));

        std::unique_ptr<WebSocketHandshakeStreamBase> handshake =
            create_helper.CreateHttp3Stream(std::move(session_handle),
                                            {} /* dns_aliases */);

        handshake->RegisterRequest(&request_info);
        int rv = handshake->InitializeStream(true, DEFAULT_PRIORITY, net_log,
                                             CompletionOnceCallback());
        EXPECT_THAT(rv, IsOk());

        HttpResponseInfo response;
        TestCompletionCallback request_callback;
        rv = handshake->SendRequest(headers, &response,
                                    request_callback.callback());
        EXPECT_THAT(rv, IsOk());

        session_->StartReading();

        TestCompletionCallback response_callback;
        rv = handshake->ReadResponseHeaders(response_callback.callback());
        EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
        rv = response_callback.WaitForResult();
        EXPECT_THAT(rv, IsOk());

        EXPECT_EQ(200, response.headers->response_code());

        return handshake->Upgrade();
      }
      default:
        NOTREACHED_IN_MIGRATION();
        return nullptr;
    }
  }

 private:
  MockClientSocketHandleFactory socket_handle_factory_;
  TestConnectDelegate connect_delegate_;
  StrictMock<MockWebSocketStreamRequestAPI> stream_request_;
  WebSocketEndpointLockManager websocket_endpoint_lock_manager_;

  // For HTTP3_HANDSHAKE_STREAM
  quic::ParsedQuicVersion quic_version_;
  quic::MockClock clock_;
  std::unique_ptr<QuicChromiumClientSession> session_;
  test::MockQuicData mock_quic_data_;
  quic::test::NoopQpackStreamSenderDelegate noop_qpack_stream_sender_delegate_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebSocketHandshakeStreamCreateHelperTest,
                         Values(BASIC_HANDSHAKE_STREAM,
                                HTTP2_HANDSHAKE_STREAM,
                                HTTP3_HANDSHAKE_STREAM));

// Confirm that the basic case works as expected.
TEST_P(WebSocketHandshakeStreamCreateHelperTest, BasicStream) {
  std::unique_ptr<WebSocketStream> stream =
      CreateAndInitializeStream({}, {}, {});
  EXPECT_EQ("", stream->GetExtensions());
  EXPECT_EQ("", stream->GetSubProtocol());
}

// Verify that the sub-protocols are passed through.
TEST_P(WebSocketHandshakeStreamCreateHelperTest, SubProtocols) {
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chat");
  sub_protocols.push_back("superchat");
  std::unique_ptr<WebSocketStream> stream = CreateAndInitializeStream(
      sub_protocols, {{"Sec-WebSocket-Protocol", "chat, superchat"}},
      {{"Sec-WebSocket-Protocol", "superchat"}});
  EXPECT_EQ("superchat", stream->GetSubProtocol());
}

// Verify that extension name is available. Bad extension names are tested in
// websocket_stream_test.cc.
TEST_P(WebSocketHandshakeStreamCreateHelperTest, Extensions) {
  std::unique_ptr<WebSocketStream> stream = CreateAndInitializeStream(
      {}, {}, {{"Sec-WebSocket-Extensions", "permessage-deflate"}});
  EXPECT_EQ("permessage-deflate", stream->GetExtensions());
}

// Verify that extension parameters are available. Bad parameters are tested in
// websocket_stream_test.cc.
TEST_P(WebSocketHandshakeStreamCreateHelperTest, ExtensionParameters) {
  std::unique_ptr<WebSocketStream> stream = CreateAndInitializeStream(
      {}, {},
      {{"Sec-WebSocket-Extensions",
        "permessage-deflate;"
        " client_max_window_bits=14; server_max_window_bits=14;"
        " server_no_context_takeover; client_no_context_takeover"}});

  EXPECT_EQ(
      "permessage-deflate;"
      " client_max_window_bits=14; server_max_window_bits=14;"
      " server_no_context_takeover; client_no_context_takeover",
      stream->GetExtensions());
}

}  // namespace

}  // namespace net
