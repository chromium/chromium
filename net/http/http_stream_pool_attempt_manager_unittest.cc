// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_attempt_manager.h"

#include <list>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/port_util.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/alternative_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_handle.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/http/http_stream_request.h"
#include "net/log/test_net_log.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket_handle.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using ::testing::_;
using ::testing::Optional;

namespace net {

using test::IsError;
using test::IsOk;
using test::MockQuicData;
using test::QuicTestPacketMaker;

using Group = HttpStreamPool::Group;
using AttemptManager = HttpStreamPool::AttemptManager;

namespace {

IPEndPoint MakeIPEndPoint(std::string_view addr, uint16_t port = 80) {
  return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
}

void ValidateConnectTiming(LoadTimingInfo::ConnectTiming& connect_timing) {
  EXPECT_LE(connect_timing.domain_lookup_start,
            connect_timing.domain_lookup_end);
  EXPECT_LE(connect_timing.domain_lookup_end, connect_timing.connect_start);
  EXPECT_LE(connect_timing.connect_start, connect_timing.ssl_start);
  EXPECT_LE(connect_timing.ssl_start, connect_timing.ssl_end);
  // connectEnd should cover TLS handshake.
  EXPECT_LE(connect_timing.ssl_end, connect_timing.connect_end);
}

// A helper to create an HttpStreamKey.
class StreamKeyBuilder {
 public:
  explicit StreamKeyBuilder(std::string_view destination = "http://a.test")
      : destination_(url::SchemeHostPort(GURL(destination))) {}

  StreamKeyBuilder(const StreamKeyBuilder&) = delete;
  StreamKeyBuilder& operator=(const StreamKeyBuilder&) = delete;

  ~StreamKeyBuilder() = default;

  StreamKeyBuilder& from_key(const HttpStreamKey& key) {
    destination_ = key.destination();
    privacy_mode_ = key.privacy_mode();
    secure_dns_policy_ = key.secure_dns_policy();
    disable_cert_network_fetches_ = key.disable_cert_network_fetches();
    return *this;
  }

  StreamKeyBuilder& set_destination(std::string_view destination) {
    set_destination(url::SchemeHostPort(GURL(destination)));
    return *this;
  }

  StreamKeyBuilder& set_destination(url::SchemeHostPort destination) {
    destination_ = std::move(destination);
    return *this;
  }

  StreamKeyBuilder& set_privacy_mode(PrivacyMode privacy_mode) {
    privacy_mode_ = privacy_mode;
    return *this;
  }

  HttpStreamKey Build() const {
    return HttpStreamKey(destination_, privacy_mode_, SocketTag(),
                         NetworkAnonymizationKey(), secure_dns_policy_,
                         disable_cert_network_fetches_);
  }

 private:
  url::SchemeHostPort destination_;
  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  SecureDnsPolicy secure_dns_policy_ = SecureDnsPolicy::kAllow;
  bool disable_cert_network_fetches_ = true;
};

class Preconnector {
 public:
  explicit Preconnector(std::string_view destination) {
    key_builder_.set_destination(destination);
  }

  Preconnector(const Preconnector&) = delete;
  Preconnector& operator=(const Preconnector&) = delete;

  ~Preconnector() = default;

  Preconnector& set_num_streams(size_t num_streams) {
    num_streams_ = num_streams;
    return *this;
  }

  Preconnector& set_quic_version(quic::ParsedQuicVersion quic_version) {
    quic_version_ = quic_version;
    return *this;
  }

  HttpStreamKey GetStreamKey() const { return key_builder_.Build(); }

  int Preconnect(HttpStreamPool& pool) {
    int rv = pool.Preconnect(
        HttpStreamPoolSwitchingInfo(GetStreamKey(), alternative_service_info_,
                                    quic_version_, is_http1_allowed_,
                                    load_flags_, proxy_info_),
        num_streams_,
        base::BindOnce(&Preconnector::OnComplete, base::Unretained(this)));
    if (rv != ERR_IO_PENDING) {
      result_ = rv;
    }
    return rv;
  }

  int WaitForResult() {
    if (result_.has_value()) {
      return *result_;
    }
    base::RunLoop run_loop;
    wait_result_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    CHECK(result_.has_value());
    return *result_;
  }

  std::optional<int> result() const { return result_; }

 private:
  void OnComplete(int rv) {
    result_ = rv;
    if (wait_result_closure_) {
      std::move(wait_result_closure_).Run();
    }
  }

  StreamKeyBuilder key_builder_;

  size_t num_streams_ = 1;

  AlternativeServiceInfo alternative_service_info_;
  quic::ParsedQuicVersion quic_version_ =
      quic::ParsedQuicVersion::Unsupported();
  bool is_http1_allowed_ = true;
  ProxyInfo proxy_info_ = ProxyInfo::Direct();
  int load_flags_ = 0;

  std::optional<int> result_;
  base::OnceClosure wait_result_closure_;
};

// A helper to request an HttpStream. On success, it keeps the provided
// HttpStream. On failure, it keeps error information.
class StreamRequester : public HttpStreamRequest::Delegate {
 public:
  StreamRequester() = default;

  explicit StreamRequester(const HttpStreamKey& key) {
    key_builder_.from_key(key);
  }

  StreamRequester(const StreamRequester&) = delete;
  StreamRequester& operator=(const StreamRequester&) = delete;

  ~StreamRequester() override = default;

  StreamRequester& set_destination(std::string_view destination) {
    key_builder_.set_destination(destination);
    return *this;
  }

  StreamRequester& set_destination(url::SchemeHostPort destination) {
    key_builder_.set_destination(destination);
    return *this;
  }

  StreamRequester& set_priority(RequestPriority priority) {
    priority_ = priority;
    return *this;
  }

  StreamRequester& set_enable_ip_based_pooling(bool enable_ip_based_pooling) {
    enable_ip_based_pooling_ = enable_ip_based_pooling;
    return *this;
  }

  StreamRequester& set_enable_alternative_services(
      bool enable_alternative_services) {
    enable_alternative_services_ = enable_alternative_services;
    return *this;
  }

  StreamRequester& set_is_http1_allowed(bool is_http1_allowed) {
    is_http1_allowed_ = is_http1_allowed;
    return *this;
  }

  StreamRequester& set_load_flags(int load_flags) {
    load_flags_ = load_flags;
    return *this;
  }

  StreamRequester& set_proxy_info(ProxyInfo proxy_info) {
    proxy_info_ = std::move(proxy_info);
    return *this;
  }

  StreamRequester& set_privacy_mode(PrivacyMode privacy_mode) {
    key_builder_.set_privacy_mode(privacy_mode);
    return *this;
  }

  StreamRequester& set_alternative_service_info(
      AlternativeServiceInfo alternative_service_info) {
    alternative_service_info_ = std::move(alternative_service_info);
    return *this;
  }

  StreamRequester& set_quic_version(quic::ParsedQuicVersion quic_version) {
    quic_version_ = quic_version;
    return *this;
  }

  HttpStreamKey GetStreamKey() const { return key_builder_.Build(); }

  HttpStreamRequest* RequestStream(HttpStreamPool& pool) {
    HttpStreamKey stream_key = GetStreamKey();
    request_ = pool.RequestStream(
        this,
        HttpStreamPoolSwitchingInfo(stream_key, alternative_service_info_,
                                    quic_version_, is_http1_allowed_,
                                    load_flags_, proxy_info_),
        priority_, allowed_bad_certs_, enable_ip_based_pooling_,
        enable_alternative_services_, NetLogWithSource());
    return request_.get();
  }

  int WaitForResult() {
    if (result_.has_value()) {
      return *result_;
    }
    base::RunLoop run_loop;
    wait_result_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    CHECK(result_.has_value());
    return *result_;
  }

  void ResetRequest() { request_.reset(); }

  // HttpStreamRequest::Delegate methods:
  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    used_proxy_info_ = used_proxy_info;
    stream_ = std::move(stream);
    SetResult(OK);
  }

  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {
    NOTREACHED();
  }

  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {
    NOTREACHED();
  }

  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_error_info) override {
    net_error_details_ = net_error_details;
    used_proxy_info_ = used_proxy_info;
    resolve_error_info_ = resolve_error_info;
    SetResult(status);
  }

  void OnCertificateError(int status, const SSLInfo& ssl_info) override {
    cert_error_ssl_info_ = ssl_info;
    SetResult(status);
  }

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {
    NOTREACHED();
  }

  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override {
    CHECK(!cert_info_);
    cert_info_ = cert_info;
    SetResult(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  }

  void OnQuicBroken() override {}

  void OnSwitchesToHttpStreamPool(
      HttpStreamPoolSwitchingInfo request_info) override {}

  std::unique_ptr<HttpStream> ReleaseStream() { return std::move(stream_); }

  std::optional<int> result() const { return result_; }

  const NetErrorDetails& net_error_details() const {
    return net_error_details_;
  }

  const ResolveErrorInfo& resolve_error_info() const {
    return resolve_error_info_;
  }

  const SSLInfo& cert_error_ssl_info() const { return cert_error_ssl_info_; }

  scoped_refptr<SSLCertRequestInfo> cert_info() const { return cert_info_; }

  NextProto negotiated_protocol() const {
    return request_->negotiated_protocol();
  }

  const ConnectionAttempts& connection_attempts() const {
    return request_->connection_attempts();
  }

  const ProxyInfo& used_proxy_info() const { return used_proxy_info_; }

 private:
  void SetResult(int rv) {
    result_ = rv;
    if (wait_result_closure_) {
      std::move(wait_result_closure_).Run();
    }
  }

  StreamKeyBuilder key_builder_;

  RequestPriority priority_ = RequestPriority::IDLE;

  std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;

  bool enable_ip_based_pooling_ = true;
  bool enable_alternative_services_ = true;
  bool is_http1_allowed_ = true;
  int load_flags_ = 0;
  ProxyInfo proxy_info_ = ProxyInfo::Direct();
  AlternativeServiceInfo alternative_service_info_;
  quic::ParsedQuicVersion quic_version_ =
      quic::ParsedQuicVersion::Unsupported();

  std::unique_ptr<HttpStreamRequest> request_;

  base::OnceClosure wait_result_closure_;

  std::unique_ptr<HttpStream> stream_;
  std::optional<int> result_;
  NetErrorDetails net_error_details_;
  ResolveErrorInfo resolve_error_info_;
  SSLInfo cert_error_ssl_info_;
  scoped_refptr<SSLCertRequestInfo> cert_info_;
  ProxyInfo used_proxy_info_;
};

constexpr std::string_view kDefaultServerName = "www.example.org";
constexpr std::string_view kDefaultDestination = "https://www.example.org";

}  // namespace

class HttpStreamPoolAttemptManagerTest : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolAttemptManagerTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    feature_list_.InitAndEnableFeature(features::kHappyEyeballsV3);
    InitializeSession();
  }

 protected:
  void InitializeSession() {
    http_network_session_.reset();
    session_deps_.alternate_host_resolver =
        std::make_unique<FakeServiceEndpointResolver>();

    auto quic_context = std::make_unique<MockQuicContext>();
    quic_context->AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
    quic_context->params()->origins_to_force_quic_on =
        origins_to_force_quic_on_;
    session_deps_.quic_context = std::move(quic_context);
    session_deps_.enable_quic = true;

    // Load a certificate that is valid for *.example.org
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    EXPECT_TRUE(test_cert.get());
    verify_details_.cert_verify_result.verified_cert = test_cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    auto mock_crypto_client_stream_factory =
        std::make_unique<MockCryptoClientStreamFactory>();
    mock_crypto_client_stream_factory->AddProofVerifyDetails(&verify_details_);
    mock_crypto_client_stream_factory->set_handshake_mode(
        MockCryptoClientStream::CONFIRM_HANDSHAKE);
    session_deps_.quic_crypto_client_stream_factory =
        std::move(mock_crypto_client_stream_factory);

    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  }

  void DestroyHttpNetworkSession() { http_network_session_.reset(); }

  HttpStreamPool& pool() { return *http_network_session_->http_stream_pool(); }

  FakeServiceEndpointResolver* resolver() {
    return static_cast<FakeServiceEndpointResolver*>(
        session_deps_.alternate_host_resolver.get());
  }

  MockClientSocketFactory* socket_factory() {
    return session_deps_.socket_factory.get();
  }

  SSLConfigService* ssl_config_service() {
    return session_deps_.ssl_config_service.get();
  }

  MockCryptoClientStreamFactory* crypto_client_stream_factory() {
    return static_cast<MockCryptoClientStreamFactory*>(
        session_deps_.quic_crypto_client_stream_factory.get());
  }

  HttpNetworkSession* http_network_session() {
    return http_network_session_.get();
  }

  HttpServerProperties* http_server_properties() {
    return http_network_session_->http_server_properties();
  }

  SpdySessionPool* spdy_session_pool() {
    return http_network_session_->spdy_session_pool();
  }

  QuicSessionPool* quic_session_pool() {
    return http_network_session_->quic_session_pool();
  }

  quic::ParsedQuicVersion quic_version() {
    return quic::ParsedQuicVersion::RFCv1();
  }

  base::WeakPtr<SpdySession> CreateFakeSpdySession(
      const HttpStreamKey& stream_key,
      IPEndPoint peer_addr = IPEndPoint(IPAddress(192, 0, 2, 1), 443)) {
    Group& group = pool().GetOrCreateGroupForTesting(stream_key);
    CHECK(!spdy_session_pool()->HasAvailableSession(group.spdy_session_key(),
                                                    /*is_websocket=*/false));
    auto socket = FakeStreamSocket::CreateForSpdy();
    socket->set_peer_addr(peer_addr);
    auto handle = group.CreateHandle(
        std::move(socket), StreamSocketHandle::SocketReuseType::kUnused,
        LoadTimingInfo::ConnectTiming());

    base::WeakPtr<SpdySession> spdy_session;
    int rv = spdy_session_pool()->CreateAvailableSessionFromSocketHandle(
        group.spdy_session_key(), std::move(handle), NetLogWithSource(),
        &spdy_session);
    CHECK_EQ(rv, OK);
    // See the comment of CreateFakeSpdySession() in spdy_test_util_common.cc.
    spdy_session->SetTimeToBufferSmallWindowUpdates(base::TimeDelta::Max());
    return spdy_session;
  }

  void AddQuicData(std::string_view host = kDefaultServerName) {
    auto client_maker = std::make_unique<QuicTestPacketMaker>(
        quic_version(),
        quic::QuicUtils::CreateRandomConnectionId(
            session_deps_.quic_context->random_generator()),
        session_deps_.quic_context->clock(), std::string(host),
        quic::Perspective::IS_CLIENT);

    auto quic_data = std::make_unique<MockQuicData>(quic_version());

    int packet_number = 1;
    quic_data->AddReadPauseForever();
    quic_data->AddConnect(ASYNC, OK);
    // HTTP/3 SETTINGS are always the first thing sent on a connection.
    quic_data->AddWrite(SYNCHRONOUS, client_maker->MakeInitialSettingsPacket(
                                         /*packet_number=*/packet_number++));
    // Connection close on shutdown.
    quic_data->AddWrite(
        SYNCHRONOUS,
        client_maker->Packet(packet_number++)
            .AddConnectionCloseFrame(quic::QUIC_CONNECTION_CANCELLED,
                                     "net error", quic::NO_IETF_QUIC_ERROR)
            .Build());
    quic_data->AddSocketDataToFactory(socket_factory());

    quic_client_makers_.emplace_back(std::move(client_maker));
    mock_quic_datas_.emplace_back(std::move(quic_data));
  }

  QuicTestPacketMaker* CreateQuicClientPacketMaker(
      std::string_view host = kDefaultServerName) {
    auto client_maker = std::make_unique<QuicTestPacketMaker>(
        quic_version(),
        quic::QuicUtils::CreateRandomConnectionId(
            session_deps_.quic_context->random_generator()),
        session_deps_.quic_context->clock(), std::string(host),
        quic::Perspective::IS_CLIENT);
    QuicTestPacketMaker* raw_client_maker = client_maker.get();
    quic_client_makers_.emplace_back(std::move(client_maker));
    return raw_client_maker;
  }

  std::set<HostPortPair>& origins_to_force_quic_on() {
    return origins_to_force_quic_on_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // For NetLog recording test coverage.
  RecordingNetLogObserver net_log_observer_;

  SpdySessionDependencies session_deps_;

  std::set<HostPortPair> origins_to_force_quic_on_;

  ProofVerifyDetailsChromium verify_details_;
  std::vector<std::unique_ptr<QuicTestPacketMaker>> quic_client_makers_;
  std::vector<std::unique_ptr<MockQuicData>> mock_quic_datas_;

  std::unique_ptr<HttpNetworkSession> http_network_session_;
};

TEST_F(HttpStreamPoolAttemptManagerTest, ResolveEndpointFailedSync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request->set_start_result(ERR_FAILED);
  StreamRequester requester;
  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));

  // Resetting the request should release the corresponding job(s).
  requester.ResetRequest();
  EXPECT_EQ(pool().JobControllerCountForTesting(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ResolveEndpointFailedMultipleRequests) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.RequestStream(pool());

  StreamRequester requester2;
  requester2.RequestStream(pool());

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  RunUntilIdle();

  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_FAILED)));
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_FAILED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, LoadState) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());

  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(HttpStreamPoolAttemptManagerTest, ResolveErrorInfo) {
  ResolveErrorInfo resolve_error_info(ERR_NAME_NOT_RESOLVED);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request->set_resolve_error_info(resolve_error_info);

  StreamRequester requester;
  requester.RequestStream(pool());

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_NAME_NOT_RESOLVED)));
  ASSERT_EQ(requester.resolve_error_info(), resolve_error_info);
  ASSERT_EQ(requester.connection_attempts().size(), 1u);
  EXPECT_EQ(requester.connection_attempts()[0].result, ERR_NAME_NOT_RESOLVED);
}

TEST_F(HttpStreamPoolAttemptManagerTest, DnsAliases) {
  const std::set<std::string> kAliases = {"alias1", "alias2"};
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_aliases(kAliases)
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  EXPECT_THAT(stream->GetDnsAliases(), kAliases);
}

TEST_F(HttpStreamPoolAttemptManagerTest, ConnectTiming) {
  constexpr base::TimeDelta kDnsUpdateDelay = base::Milliseconds(20);
  constexpr base::TimeDelta kDnsFinishDelay = base::Milliseconds(10);
  constexpr base::TimeDelta kTcpDelay = base::Milliseconds(20);
  constexpr base::TimeDelta kTlsDelay = base::Milliseconds(90);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  MockConnectCompleter tcp_connect_completer;
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(&tcp_connect_completer));
  socket_factory()->AddSocketDataProvider(data.get());

  MockConnectCompleter tls_connect_completer;
  auto ssl = std::make_unique<SSLSocketDataProvider>(&tls_connect_completer);
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  FastForwardBy(kDnsUpdateDelay);
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(false)
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(kDnsFinishDelay);
  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointRequestFinished(
      OK);
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(kTcpDelay);
  tcp_connect_completer.Complete(OK);
  RunUntilIdle();
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(kTlsDelay);
  tls_connect_completer.Complete(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();

  // Initialize `stream` to make load timing info available.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  stream->InitializeStream(/*can_send_early=*/false, RequestPriority::IDLE,
                           NetLogWithSource(), base::DoNothing());

  LoadTimingInfo timing_info;
  ASSERT_TRUE(stream->GetLoadTimingInfo(&timing_info));

  LoadTimingInfo::ConnectTiming& connect_timing = timing_info.connect_timing;

  ValidateConnectTiming(connect_timing);

  ASSERT_EQ(
      connect_timing.domain_lookup_end - connect_timing.domain_lookup_start,
      kDnsUpdateDelay);
  ASSERT_EQ(connect_timing.connect_end - connect_timing.connect_start,
            kDnsFinishDelay + kTcpDelay + kTlsDelay);
  ASSERT_EQ(connect_timing.ssl_end - connect_timing.ssl_start, kTlsDelay);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ConnectTimingDnsResolutionNotFinished) {
  constexpr base::TimeDelta kDnsUpdateDelay = base::Milliseconds(30);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("http://a.test").RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());

  FastForwardBy(kDnsUpdateDelay);
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  FastForwardBy(kDnsUpdateDelay);
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();

  // Initialize `stream` to make load timing info available.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  stream->InitializeStream(/*can_send_early=*/false, RequestPriority::IDLE,
                           NetLogWithSource(), base::DoNothing());

  LoadTimingInfo timing_info;
  ASSERT_TRUE(stream->GetLoadTimingInfo(&timing_info));
  ASSERT_EQ(timing_info.connect_timing.domain_lookup_end,
            timing_info.connect_timing.connect_start);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PlainHttpWaitForHttpsRecord) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("http://a.test").RequestStream(pool());

  // Notify there is a resolved IP address. The request should not make any
  // progress since it needs to wait for HTTPS RR.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);

  // Simulate triggering HTTP -> HTTPS upgrade.
  endpoint_request->CallOnServiceEndpointRequestFinished(
      ERR_DNS_NAME_HTTPS_ONLY);
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_DNS_NAME_HTTPS_ONLY)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SetPriority) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester1;
  HttpStreamRequest* request1 =
      requester1.set_priority(RequestPriority::LOW).RequestStream(pool());
  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester1.GetStreamKey())
          .GetAttemptManagerForTesting();
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);
  ASSERT_EQ(manager->GetPriority(), RequestPriority::LOW);

  // Create another request with IDLE priority, which has lower than LOW.
  StreamRequester requester2;
  HttpStreamRequest* request2 =
      requester2.set_priority(RequestPriority::IDLE).RequestStream(pool());
  ASSERT_EQ(manager, pool()
                         .GetOrCreateGroupForTesting(requester2.GetStreamKey())
                         .GetAttemptManagerForTesting());
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);
  ASSERT_EQ(manager->GetPriority(), RequestPriority::LOW);

  // Set the second request's priority to HIGHEST. The corresponding service
  // endpoint request and attempt manager should update their priorities.
  request2->SetPriority(RequestPriority::HIGHEST);
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::HIGHEST);
  ASSERT_EQ(manager->GetPriority(), RequestPriority::HIGHEST);

  // Check `request2` completes first.

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data1.get());

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);
  ASSERT_EQ(request1->GetLoadState(), LOAD_STATE_CONNECTING);
  ASSERT_EQ(request2->GetLoadState(), LOAD_STATE_CONNECTING);

  RunUntilIdle();
  ASSERT_FALSE(request1->completed());
  ASSERT_TRUE(request2->completed());
  ASSERT_EQ(request1->GetLoadState(), LOAD_STATE_CONNECTING);
  ASSERT_EQ(request2->GetLoadState(), LOAD_STATE_IDLE);
  std::unique_ptr<HttpStream> stream = requester2.ReleaseStream();
  ASSERT_TRUE(stream);
}

TEST_F(HttpStreamPoolAttemptManagerTest, TcpFailSync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(requester.connection_attempts().size(), 1u);
  ASSERT_EQ(requester.connection_attempts()[0].result, ERR_FAILED);
}

TEST_F(HttpStreamPoolAttemptManagerTest, TcpFailAsync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(requester.connection_attempts().size(), 1u);
  ASSERT_EQ(requester.connection_attempts()[0].result, ERR_FAILED);
}

TEST_F(HttpStreamPoolAttemptManagerTest, TlsOkAsync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, TcpSyncTlsAsyncOk) {
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, TlsCryptoReadyDelayed) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  HttpStreamRequest* request =
      requester.set_destination("https://a.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  ASSERT_FALSE(requester.result().has_value());
  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_SSL_HANDSHAKE);

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, CertificateError) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const scoped_refptr<X509Certificate> kCert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_DATE_INVALID);
  ssl.ssl_info.cert_status = ERR_CERT_DATE_INVALID;
  ssl.ssl_info.cert = kCert;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  constexpr std::string_view kDestination = "https://a.test";
  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_FALSE(requester1.result().has_value());
  EXPECT_FALSE(requester2.result().has_value());

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_CERT_DATE_INVALID)));
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_CERT_DATE_INVALID)));
  ASSERT_TRUE(
      requester1.cert_error_ssl_info().cert->EqualsIncludingChain(kCert.get()));
  ASSERT_EQ(requester1.connection_attempts().size(), 1u);
  ASSERT_EQ(requester1.connection_attempts()[0].result, ERR_CERT_DATE_INVALID);

  ASSERT_TRUE(
      requester2.cert_error_ssl_info().cert->EqualsIncludingChain(kCert.get()));
  ASSERT_EQ(requester2.connection_attempts().size(), 1u);
  ASSERT_EQ(requester2.connection_attempts()[0].result, ERR_CERT_DATE_INVALID);
}

TEST_F(HttpStreamPoolAttemptManagerTest, NeedsClientAuth) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_FALSE(requester1.result().has_value());
  EXPECT_FALSE(requester2.result().has_value());

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_EQ(requester1.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
  EXPECT_EQ(requester2.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
}

// Tests that after a fatal error (e.g., the server required a client cert),
// following attempt failures are ignored and the existing requests get the
// same fatal error.
TEST_F(HttpStreamPoolAttemptManagerTest, TcpFailAfterNeedsClientAuth) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data1 = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data1.get());
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data2.get());

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_EQ(requester1.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
  EXPECT_EQ(requester2.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
}

TEST_F(HttpStreamPoolAttemptManagerTest, RequestCancelledBeforeAttemptSuccess) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  requester.ResetRequest();
  RunUntilIdle();

  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, OneIPEndPointFailed) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data2.get());

  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v6("2001:db8::1")
                                     .add_v4("192.0.2.1")
                                     .endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, IPEndPointTimedout) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(TcpStreamAttempt::kTcpHandshakeTimeout);
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_TIMED_OUT)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, IPEndPointsSlow) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());

  auto data1 = std::make_unique<SequencedSocketData>();
  // Make the first and the second attempt stalled.
  data1->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());
  // The third attempt succeeds.
  auto data3 = std::make_unique<SequencedSocketData>();
  data3->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data3.get());

  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v6("2001:db8::1")
                                     .add_v6("2001:db8::2")
                                     .add_v4("192.0.2.1")
                                     .endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .GetAttemptManagerForTesting();
  ASSERT_EQ(manager->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(request->completed());

  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_EQ(manager->InFlightAttemptCount(), 2u);
  ASSERT_EQ(manager->PendingJobCount(), 0u);
  ASSERT_FALSE(request->completed());

  // FastForwardBy() executes non-delayed tasks so the request finishes
  // immediately.
  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_TRUE(request->completed());
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       PauseSlowTimerAfterTcpHandshakeForTls) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  MockConnectCompleter tcp_connect_completer1;
  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(&tcp_connect_completer1));
  socket_factory()->AddSocketDataProvider(data1.get());
  // This TLS handshake never finishes.
  auto ssl1 =
      std::make_unique<SSLSocketDataProvider>(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory()->AddSSLSocketDataProvider(ssl1.get());

  MockConnectCompleter tcp_connect_completer2;
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(&tcp_connect_completer2));
  socket_factory()->AddSocketDataProvider(data2.get());
  auto ssl2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(ssl2.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder()
                         .add_v6("2001:db8::1")
                         .add_v4("192.0.2.1")
                         .endpoint())
      .set_crypto_ready(false)
      .CallOnServiceEndpointsUpdated();
  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .GetAttemptManagerForTesting();
  ASSERT_EQ(manager->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(requester.result().has_value());

  // Complete TCP handshake after a delay that is less than the connection
  // attempt delay.
  constexpr base::TimeDelta kTcpDelay = base::Milliseconds(30);
  ASSERT_LT(kTcpDelay, HttpStreamPool::kConnectionAttemptDelay);
  FastForwardBy(kTcpDelay);
  tcp_connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(manager->InFlightAttemptCount(), 1u);

  // Fast-forward to the connection attempt delay. Since the in-flight attempt
  // has completed TCP handshake and is waiting for HTTPS RR, the manager
  // shouldn't start another attempt.
  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_EQ(manager->InFlightAttemptCount(), 1u);

  // Complete DNS resolution fully.
  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointRequestFinished(
      OK);
  ASSERT_EQ(manager->InFlightAttemptCount(), 1u);

  // Fast-forward to the connection attempt delay again. This time the in-flight
  // attempt is still doing TLS handshake, it's treated as slow and the manager
  // should start another attempt.
  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_EQ(manager->InFlightAttemptCount(), 2u);

  // Complete the second attempt. The request should finish successfully.
  tcp_connect_completer2.Complete(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Regression test for crbug.com/368187247. Tests that an idle stream socket
// is reused when an in-flight connection attempt is slow.
TEST_F(HttpStreamPoolAttemptManagerTest,
       SlowTimerFiredAfterIdleSocketAvailable) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination("http://a.test").Build();

  MockConnectCompleter connect_completer;
  SequencedSocketData data;
  data.set_connect_data(MockConnect(&connect_completer));
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester(stream_key);
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Create an active text-based stream and release it to create an idle stream.
  // The idle stream should be reused for the in-flight request.
  Group& group = pool().GetOrCreateGroupForTesting(stream_key);
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::make_unique<FakeStreamSocket>(),
      StreamSocketHandle::SocketReuseType::kReusedIdle,
      LoadTimingInfo::ConnectTiming());
  stream.reset();
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 2u);

  // Fire the slow timer. It should not attempt another connection.
  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 2u);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, FeatureParamStreamLimits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kHappyEyeballsV3,
      {{std::string(HttpStreamPool::kMaxStreamSocketsPerPoolParamName), "2"},
       {std::string(HttpStreamPool::kMaxStreamSocketsPerGroupParamName), "3"}});
  InitializeSession();
  ASSERT_EQ(pool().max_stream_sockets_per_pool(), 2u);
  ASSERT_EQ(pool().max_stream_sockets_per_group(), 2u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReachedGroupLimit) {
  constexpr size_t kMaxPerGroup = 4;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  // Create streams up to the per-group limit for a destination.
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> data_providers;
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    auto requester = std::make_unique<StreamRequester>();
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());

    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
  }

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  Group& group =
      pool().GetOrCreateGroupForTesting(requesters[0]->GetStreamKey());
  AttemptManager* manager = group.GetAttemptManagerForTesting();
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->InFlightAttemptCount(), kMaxPerGroup);
  ASSERT_EQ(manager->PendingJobCount(), 0u);

  // This request should not start an attempt as the group reached its limit.
  StreamRequester stalled_requester;
  HttpStreamRequest* stalled_request = stalled_requester.RequestStream(pool());
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());
  data_providers.emplace_back(std::move(data));

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->InFlightAttemptCount(), kMaxPerGroup);
  ASSERT_EQ(manager->PendingJobCount(), 1u);
  ASSERT_EQ(stalled_request->GetLoadState(),
            LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET);

  // Finish all in-flight attempts successfully.
  RunUntilIdle();
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->InFlightAttemptCount(), 0u);
  ASSERT_EQ(manager->PendingJobCount(), 1u);

  // Release one HttpStream and close it to make non-reusable.
  std::unique_ptr<StreamRequester> released_requester =
      std::move(requesters.back());
  requesters.pop_back();
  std::unique_ptr<HttpStream> released_stream =
      released_requester->ReleaseStream();

  // Need to initialize the HttpStream as HttpBasicStream doesn't disconnect
  // the underlying stream socket when not initialized.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  released_stream->RegisterRequest(&request_info);
  released_stream->InitializeStream(/*can_send_early=*/false,
                                    RequestPriority::IDLE, NetLogWithSource(),
                                    base::DoNothing());

  released_stream->Close(/*not_reusable=*/true);
  released_stream.reset();

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->InFlightAttemptCount(), 1u);
  ASSERT_EQ(manager->PendingJobCount(), 0u);

  RunUntilIdle();

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->InFlightAttemptCount(), 0u);
  ASSERT_EQ(manager->PendingJobCount(), 0u);
  ASSERT_TRUE(stalled_request->completed());
  std::unique_ptr<HttpStream> stream = stalled_requester.ReleaseStream();
  ASSERT_TRUE(stream);
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  const HttpStreamKey key_a(url::SchemeHostPort("http", "a.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  const HttpStreamKey key_b(url::SchemeHostPort("http", "b.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  // Create HttpStreams up to the group limit in group A.
  Group& group_a = pool().GetOrCreateGroupForTesting(key_a);
  std::vector<std::unique_ptr<HttpStream>> streams_a;
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    streams_a.emplace_back(group_a.CreateTextBasedStream(
        std::make_unique<FakeStreamSocket>(),
        StreamSocketHandle::SocketReuseType::kUnused,
        LoadTimingInfo::ConnectTiming()));
  }

  ASSERT_FALSE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());
  ASSERT_TRUE(group_a.ReachedMaxStreamLimit());
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group_a.ActiveStreamSocketCount(), kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  // Create a HttpStream in group B. It should not be blocked because both
  // per-group and per-pool limits are not reached yet.
  StreamRequester requester1(key_b);
  HttpStreamRequest* request1 = requester1.RequestStream(pool());
  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data1.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  ASSERT_TRUE(request1->completed());

  // The pool reached the limit, but it doesn't have any blocked request. Group
  // A reached the group limit. Group B doesn't reach the group limit.
  Group& group_b = pool().GetOrCreateGroupForTesting(key_b);
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());
  ASSERT_TRUE(group_a.ReachedMaxStreamLimit());
  ASSERT_FALSE(group_b.ReachedMaxStreamLimit());

  // Create another HttpStream in group B. It should be blocked because the pool
  // reached limit, event when group B doesn't reach its limit.
  StreamRequester requester2(key_b);
  HttpStreamRequest* request2 = requester2.RequestStream(pool());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data2.get());
  ASSERT_EQ(request2->GetLoadState(),
            LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL);

  RunUntilIdle();
  AttemptManager* manager_b = group_b.GetAttemptManagerForTesting();
  ASSERT_FALSE(request2->completed());
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_TRUE(pool().IsPoolStalled());
  ASSERT_EQ(manager_b->InFlightAttemptCount(), 0u);
  ASSERT_EQ(manager_b->PendingJobCount(), 1u);

  // Release one HttpStream from group A. It should unblock the in-flight
  // request in group B.
  std::unique_ptr<HttpStream> released_stream = std::move(streams_a.back());
  streams_a.pop_back();
  released_stream.reset();
  RunUntilIdle();

  ASSERT_TRUE(request2->completed());
  ASSERT_EQ(manager_b->PendingJobCount(), 0u);
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ReachedPoolLimitHighPriorityGroupFirst) {
  constexpr size_t kMaxPerGroup = 1;
  constexpr size_t kMaxPerPool = 2;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  // Create 4 requests with different destinations and priorities.
  constexpr struct Item {
    std::string_view host;
    std::string_view ip_address;
    RequestPriority priority;
  } items[] = {
      {"a.test", "192.0.2.1", RequestPriority::IDLE},
      {"b.test", "192.0.2.2", RequestPriority::IDLE},
      {"c.test", "192.0.2.3", RequestPriority::LOWEST},
      {"d.test", "192.0.2.4", RequestPriority::HIGHEST},
  };

  std::vector<FakeServiceEndpointRequest*> endpoint_requests;
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> socket_datas;
  for (const auto& [host, ip_address, priority] : items) {
    FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
    endpoint_request->add_endpoint(
        ServiceEndpointBuilder().add_v4(ip_address).endpoint());
    endpoint_requests.emplace_back(endpoint_request);

    auto requester = std::make_unique<StreamRequester>();
    requester->set_destination(url::SchemeHostPort("http", host, 80))
        .set_priority(priority);
    requesters.emplace_back(std::move(requester));

    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    socket_datas.emplace_back(std::move(data));
  }

  // Complete the first two requests to reach the pool's limit.
  for (size_t i = 0; i < kMaxPerPool; ++i) {
    HttpStreamRequest* request = requesters[i]->RequestStream(pool());
    endpoint_requests[i]->CallOnServiceEndpointRequestFinished(OK);
    RunUntilIdle();
    ASSERT_TRUE(request->completed());
  }

  ASSERT_TRUE(pool().ReachedMaxStreamLimit());

  // Start the remaining requests. These requests should be blocked.
  HttpStreamRequest* request_c = requesters[2]->RequestStream(pool());
  endpoint_requests[2]->CallOnServiceEndpointRequestFinished(OK);

  HttpStreamRequest* request_d = requesters[3]->RequestStream(pool());
  endpoint_requests[3]->CallOnServiceEndpointRequestFinished(OK);

  RunUntilIdle();

  ASSERT_FALSE(request_c->completed());
  ASSERT_FALSE(request_d->completed());

  // Release the HttpStream from group A. It should unblock group D, which has
  // higher priority than group C.
  std::unique_ptr<HttpStream> stream_a = requesters[0]->ReleaseStream();
  stream_a.reset();

  RunUntilIdle();

  ASSERT_FALSE(request_c->completed());
  ASSERT_TRUE(request_d->completed());

  // Release the HttpStream from group B. It should unblock group C.
  std::unique_ptr<HttpStream> stream_b = requesters[1]->ReleaseStream();
  stream_b.reset();

  RunUntilIdle();

  ASSERT_TRUE(request_c->completed());
}

// Regression test for crbug.com/368164182. Tests that the per-group limit is
// respected when there is an idle stream socket.
TEST_F(HttpStreamPoolAttemptManagerTest,
       ReachedPerGroupLimitWithIdleStreamSocket) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination("http://a.test").Build();

  Group& group = pool().GetOrCreateGroupForTesting(stream_key);

  // Create an active text-based stream and release it to create an idle stream.
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::make_unique<FakeStreamSocket>(),
      StreamSocketHandle::SocketReuseType::kReusedIdle,
      LoadTimingInfo::ConnectTiming());
  stream.reset();

  // Create requests up to the per-group limit + 1. Active stream counts for the
  // group should not exceed the per-group limit.
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (size_t i = 0; i < pool().max_stream_sockets_per_group() + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));

    auto requester = std::make_unique<StreamRequester>(stream_key);
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());
    ASSERT_FALSE(raw_requester->result().has_value());
    ASSERT_LE(group.ActiveStreamSocketCount(),
              pool().max_stream_sockets_per_group());
  }

  for (const auto& requester : requesters) {
    requester->WaitForResult();
    EXPECT_THAT(requester->result(), Optional(IsOk()));
    // Release the stream to unblock other requests.
    requester->ReleaseStream();
  }
}

TEST_F(HttpStreamPoolAttemptManagerTest, RequestStreamIdleStreamSocket) {
  StreamRequester requester;
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  HttpStreamRequest* request = requester.RequestStream(pool());
  RunUntilIdle();
  ASSERT_TRUE(request->completed());

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

// Tests that the group and pool limits are ignored when requests set
// LOAD_IGNORE_LIMITS.
TEST_F(HttpStreamPoolAttemptManagerTest, IgnoreLimits) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> data_providers;

  for (size_t i = 0; i < kMaxPerPool + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
    auto requester = std::make_unique<StreamRequester>();
    requester->set_load_flags(LOAD_IGNORE_LIMITS).RequestStream(pool());
    requester->WaitForResult();
    EXPECT_THAT(requester->result(), Optional(IsOk()));
    requesters.emplace_back(std::move(requester));
  }
}

TEST_F(HttpStreamPoolAttemptManagerTest, UseIdleStreamSocketAfterRelease) {
  StreamRequester requester;
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());

  // Create HttpStreams up to the group's limit.
  std::vector<std::unique_ptr<HttpStream>> streams;
  for (size_t i = 0; i < pool().max_stream_sockets_per_group(); ++i) {
    std::unique_ptr<HttpStream> http_stream = group.CreateTextBasedStream(
        std::make_unique<FakeStreamSocket>(),
        StreamSocketHandle::SocketReuseType::kUnused,
        LoadTimingInfo::ConnectTiming());
    streams.emplace_back(std::move(http_stream));
  }
  ASSERT_EQ(group.ActiveStreamSocketCount(),
            pool().max_stream_sockets_per_group());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);

  // Request a stream. The request should be blocked.
  resolver()->AddFakeRequest();
  HttpStreamRequest* request = requester.RequestStream(pool());
  RunUntilIdle();
  AttemptManager* manager = group.GetAttemptManagerForTesting();
  ASSERT_FALSE(request->completed());
  ASSERT_EQ(manager->PendingJobCount(), 1u);

  // Release an active HttpStream. The underlying StreamSocket should be used
  // to the pending request.
  std::unique_ptr<HttpStream> released_stream = std::move(streams.back());
  streams.pop_back();

  released_stream.reset();
  requester.WaitForResult();
  ASSERT_TRUE(request->completed());
  ASSERT_EQ(manager->PendingJobCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       CloseIdleStreamAttemptConnectionReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  const HttpStreamKey key_a(url::SchemeHostPort("http", "a.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  const HttpStreamKey key_b(url::SchemeHostPort("http", "b.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  // Add idle streams up to the group's limit in group A.
  Group& group_a = pool().GetOrCreateGroupForTesting(key_a);
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    group_a.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  }
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 2u);
  ASSERT_FALSE(pool().ReachedMaxStreamLimit());

  // Create an HttpStream in group B. The pool should reach its limit.
  Group& group_b = pool().GetOrCreateGroupForTesting(key_b);
  std::unique_ptr<HttpStream> stream1 = group_b.CreateTextBasedStream(
      std::make_unique<FakeStreamSocket>(),
      StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());

  // Request a stream in group B. The request should close an idle stream in
  // group A.
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  ASSERT_TRUE(request->completed());
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ProcessPendingRequestDnsResolutionOngoing) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());

  StreamRequester requester;
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // This should not enter an infinite loop.
  pool().ProcessPendingRequestsInGroups();

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Tests that all in-flight requests and connection attempts are canceled
// when an IP address change event happens.
TEST_F(HttpStreamPoolAttemptManagerTest,
       CancelAttemptAndRequestsOnIPAddressChange) {
  FakeServiceEndpointRequest* endpoint_request1 = resolver()->AddFakeRequest();
  FakeServiceEndpointRequest* endpoint_request2 = resolver()->AddFakeRequest();

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data1.get());

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());

  StreamRequester requester1;
  requester1.set_destination("https://a.test").RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination("https://b.test").RequestStream(pool());

  endpoint_request1->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request1->CallOnServiceEndpointRequestFinished(OK);
  endpoint_request2->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint());
  endpoint_request2->CallOnServiceEndpointRequestFinished(OK);

  AttemptManager* manager1 =
      pool()
          .GetOrCreateGroupForTesting(requester1.GetStreamKey())
          .GetAttemptManagerForTesting();
  AttemptManager* manager2 =
      pool()
          .GetOrCreateGroupForTesting(requester2.GetStreamKey())
          .GetAttemptManagerForTesting();
  ASSERT_EQ(manager1->JobCount(), 1u);
  ASSERT_EQ(manager1->InFlightAttemptCount(), 1u);
  ASSERT_EQ(manager2->JobCount(), 1u);
  ASSERT_EQ(manager2->InFlightAttemptCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();
  ASSERT_EQ(manager1->JobCount(), 0u);
  ASSERT_EQ(manager1->InFlightAttemptCount(), 0u);
  ASSERT_EQ(manager2->JobCount(), 0u);
  ASSERT_EQ(manager2->InFlightAttemptCount(), 0u);
  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
}

// Tests that the network change error is reported even when a different error
// has already happened.
TEST_F(HttpStreamPoolAttemptManagerTest, IPAddressChangeAfterNeedsClientAuth) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();
  EXPECT_THAT(requester1.result(),
              Optional(IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED)));
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SSLConfigChangedCloseIdleStream) {
  StreamRequester requester;
  requester.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  ssl_config_service()->NotifySSLContextConfigChange();
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SSLConfigChangedReleasedStreamGenerationOutdated) {
  StreamRequester requester;
  requester.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::make_unique<FakeStreamSocket>(),
                                  StreamSocketHandle::SocketReuseType::kUnused,
                                  LoadTimingInfo::ConnectTiming());
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  ssl_config_service()->NotifySSLContextConfigChange();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  // Release the HttpStream, the underlying StreamSocket should not be pooled
  // as an idle stream since the generation is different.
  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SSLConfigForServersChanged) {
  // Create idle streams in group A and group B.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  Group& group_a =
      pool().GetOrCreateGroupForTesting(requester_a.GetStreamKey());
  group_a.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 1u);

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");
  Group& group_b =
      pool().GetOrCreateGroupForTesting(requester_b.GetStreamKey());
  group_b.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 1u);

  // Simulate an SSLConfigForServers change event for group A. The idle stream
  // in group A should be gone but the idle stream in group B should remain.
  pool().OnSSLConfigForServersChanged({HostPortPair::FromSchemeHostPort(
      requester_a.GetStreamKey().destination())});
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyAvailableSession) {
  StreamRequester requester;
  requester.set_destination("https://a.test")
      .set_enable_ip_based_pooling(false);

  CreateFakeSpdySession(requester.GetStreamKey());
  requester.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Test that setting the priority for a request that will be served via an
// existing SPDY session doesn't crash the network service.
TEST_F(HttpStreamPoolAttemptManagerTest, ChangePriorityForPooledStreamRequest) {
  StreamRequester requester;
  requester.set_destination("https://a.test");

  CreateFakeSpdySession(requester.GetStreamKey());

  HttpStreamRequest* request = requester.RequestStream(pool());
  request->SetPriority(RequestPriority::HIGHEST);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  // HttpStream{,Request} don't provide a way to get its priority.
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyOk) {
  // Create two requests for the same destination. Once a connection is
  // established and is negotiated to use H2, another connection attempts should
  // be canceled and all requests should receive HttpStreams on top of the
  // SpdySession.

  constexpr size_t kNumRequests = 2;
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  std::vector<std::unique_ptr<SequencedSocketData>> socket_datas;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssls;
  std::vector<std::unique_ptr<StreamRequester>> requesters;

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  for (size_t i = 0; i < kNumRequests; ++i) {
    auto data = std::make_unique<SequencedSocketData>(reads, writes);
    socket_factory()->AddSocketDataProvider(data.get());
    socket_datas.emplace_back(std::move(data));
    auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    ssl->next_proto = NextProto::kProtoHTTP2;
    socket_factory()->AddSSLSocketDataProvider(ssl.get());
    ssls.emplace_back(std::move(ssl));

    auto requester = std::make_unique<StreamRequester>();
    requester->set_destination("https://a.test")
        .set_enable_ip_based_pooling(false)
        .RequestStream(pool());
    requesters.emplace_back(std::move(requester));
  }

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  for (auto& requester : requesters) {
    ASSERT_TRUE(requester->result().has_value());
    EXPECT_THAT(requester->result(), Optional(IsOk()));
  }
  Group& group =
      pool().GetOrCreateGroupForTesting(requesters[0]->GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalConnectingStreamCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyCreateSessionFail) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  // Set an invalid ALPS to make SPDY session creation fail.
  ssl->peer_application_settings = "invalid alps";
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  EXPECT_THAT(requester.result(), Optional(IsError(ERR_HTTP2_PROTOCOL_ERROR)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, RequireHttp11AfterSpdySessionCreated) {
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto h2_data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(h2_data.get());
  auto h2_ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  h2_ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(h2_ssl.get());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination).RequestStream(pool());
  HttpStreamKey stream_key = requester1.GetStreamKey();
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));

  // Disable HTTP/2.
  http_server_properties()->SetHTTP11Required(
      stream_key.destination(), stream_key.network_anonymization_key());
  // At this point, the SPDY session is still available because it becomes
  // unavailable after the next request is made.
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));

  // Request a stream again. The second request fails because the first request
  // is still alive and the corresponding attempt manager is still alive. The
  // existing SPDY session should become unavailable.
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_HTTP_1_1_REQUIRED)));
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       RequireHttp11AfterSpdySessionCreatedRequestDestroyed) {
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto h2_data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(h2_data.get());
  auto h2_ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  h2_ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(h2_ssl.get());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination).RequestStream(pool());
  HttpStreamKey stream_key = requester1.GetStreamKey();
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));

  // Disable HTTP/2.
  http_server_properties()->SetHTTP11Required(
      stream_key.destination(), stream_key.network_anonymization_key());
  // At this point, the SPDY session is still available because it becomes
  // unavailable after the next request is made.
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));

  // Destroy the first request.
  requester1.ResetRequest();

  // Request a stream again. The second request should succeed using HTTP/1.1.
  // The existing SPDY session should become unavailable.
  auto h1_data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(h1_data.get());
  SSLSocketDataProvider h1_ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&h1_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DoNotUseSpdySessionForHttpRequest) {
  constexpr std::string_view kHttpsDestination = "https://www.example.com";
  constexpr std::string_view kHttpDestination = "http://www.example.com";

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto h2_data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(h2_data.get());
  auto h2_ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  h2_ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(h2_ssl.get());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester_https;
  requester_https.set_destination(kHttpsDestination).RequestStream(pool());
  HttpStreamKey stream_key = requester_https.GetStreamKey();
  RunUntilIdle();
  EXPECT_THAT(requester_https.result(), Optional(IsOk()));
  EXPECT_EQ(requester_https.negotiated_protocol(), NextProto::kProtoHTTP2);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));

  // Request a stream for http (not https). The second request should use
  // HTTP/1.1 and should not use the existing SPDY session.
  auto h1_data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(h1_data.get());
  SSLSocketDataProvider h1_ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&h1_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester_http;
  requester_http.set_destination(kHttpDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_http.result(), Optional(IsOk()));
  EXPECT_NE(requester_http.negotiated_protocol(), NextProto::kProtoHTTP2);
}

TEST_F(HttpStreamPoolAttemptManagerTest, CloseIdleSpdySessionWhenPoolStalled) {
  pool().set_max_stream_sockets_per_group_for_testing(1u);
  pool().set_max_stream_sockets_per_pool_for_testing(1u);

  constexpr std::string_view kDestinationA = "https://a.test";
  constexpr std::string_view kDestinationB = "https://b.test";

  // Create an idle SPDY session for `kDestinationA`. This session should be
  // closed when a request is created for `kDestinationB`.
  const HttpStreamKey stream_key_a =
      StreamKeyBuilder().set_destination(kDestinationA).Build();
  CreateFakeSpdySession(stream_key_a);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key_a.ToSpdySessionKey(), /*is_websocket=*/false));

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester_b;
  requester_b.set_destination(kDestinationB).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  EXPECT_EQ(requester_b.negotiated_protocol(), NextProto::kProtoHTTP2);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      requester_b.GetStreamKey().ToSpdySessionKey(), /*is_websocket=*/false));
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key_a.ToSpdySessionKey(), /*is_websocket=*/false));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       PreconnectRequireHttp11AfterSpdySessionCreated) {
  const MockWrite writes[] = {MockWrite(ASYNC, OK, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto h2_data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(h2_data.get());
  auto h2_ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  h2_ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(h2_ssl.get());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector1(kDefaultDestination);
  HttpStreamKey stream_key = preconnector1.GetStreamKey();
  preconnector1.Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector1.result(), Optional(IsOk()));
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));

  // Disable HTTP/2.
  http_server_properties()->SetHTTP11Required(
      stream_key.destination(), stream_key.network_anonymization_key());

  // Preconnect again. The existing SPDY session should become unavailable.

  auto h1_data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(h1_data.get());
  SSLSocketDataProvider h1_ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&h1_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector2(kDefaultDestination);
  int rv = preconnector2.Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_HTTP_1_1_REQUIRED));
  RunUntilIdle();
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 1;
  constexpr size_t kMaxPerPool = 2;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  // Create SPDY sessions up to the pool limit. Initialize streams to make
  // SPDY sessions active.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  base::WeakPtr<SpdySession> spdy_session_a = CreateFakeSpdySession(
      requester_a.GetStreamKey(), MakeIPEndPoint("192.0.2.1"));
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream_a = requester_a.ReleaseStream();
  HttpRequestInfo request_info_a;
  request_info_a.url = GURL("https://a.test");
  request_info_a.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream_a->RegisterRequest(&request_info_a);
  stream_a->InitializeStream(/*can_send_early=*/false, DEFAULT_PRIORITY,
                             NetLogWithSource(), base::DoNothing());

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");
  CreateFakeSpdySession(requester_b.GetStreamKey(),
                        MakeIPEndPoint("192.0.2.2"));
  requester_b.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream_b = requester_b.ReleaseStream();
  HttpRequestInfo request_info_b;
  request_info_b.url = GURL("https://b.test");
  request_info_b.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream_b->RegisterRequest(&request_info_b);
  stream_b->InitializeStream(/*can_send_early=*/false, DEFAULT_PRIORITY,
                             NetLogWithSource(), base::DoNothing());

  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());

  // Request a stream in group C. It should be blocked.
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester_c;
  requester_c.set_destination("https://c.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  Group& group_c =
      pool().GetOrCreateGroupForTesting(requester_c.GetStreamKey());
  ASSERT_EQ(group_c.GetAttemptManagerForTesting()->PendingJobCount(), 1u);
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_TRUE(pool().IsPoolStalled());

  // Close the group A's SPDY session. It should unblock the request in group C.
  spdy_session_a->CloseSessionOnError(ERR_ABORTED,
                                      /*description=*/"for testing");
  RunUntilIdle();
  EXPECT_THAT(requester_c.result(), Optional(IsOk()));
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());

  // Need to close HttpStreams before finishing this test due to the DCHECK in
  // the destructor of SpdyHttpStream.
  // TODO(crbug.com/346835898): Figure out a way not to rely on this behavior,
  // or fix SpdySessionStream somehow.
  stream_a->Close(/*not_reusable=*/true);
  stream_b->Close(/*not_reusable=*/true);
}

// In the following SPDY IP-based pooling tests, we use spdy_pooling.pem that
// has "www.example.org" and "example.test" as alternate names.

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyMatchingIpSessionOk) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionBecomeUnavailableBeforeNotify) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  // Add a SpdySession for www.example.org.
  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  requester_a.WaitForResult();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  // Data for the second request.
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  // Create the second request to example.test. It will finds the matching
  // SPDY session, but the task to use the session runs asynchronously, so it
  // hasn't run yet.
  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());
  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester_b.result().has_value());

  // Close the session before the second request can try to use it.
  spdy_session_pool()->CloseAllSessions();

  // Finish the service endpoint resolution. It should create a new SPDY
  // session.
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  requester_b.WaitForResult();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_TRUE(spdy_session_pool()->FindAvailableSession(
      requester_b.GetStreamKey().ToSpdySessionKey(),
      /*enable_ip_based_pooling=*/true, /*is_websocket=*/false,
      NetLogWithSource()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyPreconnectMatchingIpSession) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector_b("https://example.test");
  preconnector_b.Preconnect(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(preconnector_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionAlreadyHaveSession) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());

  // Call both CallOnServiceEndpointsUpdated() and
  // CallOnServiceEndpointRequestFinished() to check existing sessions twice.
  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointsUpdated()
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionDnsResolutionFinishSynchronously) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .set_start_result(OK);

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());
  ASSERT_FALSE(requester_b.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyMatchingIpSessionDisabled) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("192.0.2.1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test")
      .set_enable_ip_based_pooling(false)
      .RequestStream(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyMatchingIpSessionKeyMismatch) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("192.0.2.1", 443);

  StreamRequester requester_a;
  // Set privacy mode to make SpdySessionKey different.
  requester_a.set_destination("https://www.example.org")
      .set_privacy_mode(PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS);

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionVerifyDomainFailed) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("192.0.2.1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  // Use a destination that is not listed in spdy_pooling.pem.
  StreamRequester requester_b;
  requester_b.set_destination("https://non-alternative.test")
      .RequestStream(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ThrottleAttemptForSpdyBlockSecondAttempt) {
  constexpr std::string_view kDestination = "https://a.test";

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  // Set the destination is known to support HTTP/2.
  HttpStreamKey stream_key = requester1.GetStreamKey();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  // There should be only one in-flight attempt because attempts are throttled.
  Group& group = pool().GetOrCreateGroupForTesting(requester1.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 1u);

  // This should not enter an infinite loop.
  pool().ProcessPendingRequestsInGroups();

  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ThrottleAttemptForSpdyDelayPassedHttp2) {
  constexpr std::string_view kDestination = "https://a.test";

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  // Set the destination is known to support HTTP/2.
  HttpStreamKey stream_key = requester1.GetStreamKey();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockConnectCompleter connect_completer1;
  auto data1 = std::make_unique<SequencedSocketData>(reads, writes);
  data1->set_connect_data(MockConnect(&connect_completer1));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto ssl1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl1->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl1.get());

  MockConnectCompleter connect_completer2;
  auto data2 = std::make_unique<SequencedSocketData>(reads, writes);
  data2->set_connect_data(MockConnect(&connect_completer2));
  socket_factory()->AddSocketDataProvider(data2.get());
  auto ssl2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl2->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl2.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  // There should be only one in-flight attempt because attempts are throttled.
  Group& group = pool().GetOrCreateGroupForTesting(requester1.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 1u);

  FastForwardBy(AttemptManager::kSpdyThrottleDelay);
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 2u);

  connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 0u);

  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ThrottleAttemptForSpdyDelayPassedHttp1) {
  constexpr std::string_view kDestination = "https://a.test";

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  // Set the destination is known to support HTTP/2.
  HttpStreamKey stream_key = requester1.GetStreamKey();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockConnectCompleter connect_completer1;
  auto data1 = std::make_unique<SequencedSocketData>(reads, writes);
  data1->set_connect_data(MockConnect(&connect_completer1));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto ssl1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(ssl1.get());

  MockConnectCompleter connect_completer2;
  auto data2 = std::make_unique<SequencedSocketData>(reads, writes);
  data2->set_connect_data(MockConnect(&connect_completer2));
  socket_factory()->AddSocketDataProvider(data2.get());
  auto ssl2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(ssl2.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  // There should be only one in-flight attempt because attempts are throttled.
  Group& group = pool().GetOrCreateGroupForTesting(requester1.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 1u);

  FastForwardBy(AttemptManager::kSpdyThrottleDelay);
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 2u);

  connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 1u);

  connect_completer2.Complete(OK);
  RunUntilIdle();

  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectSpdySessionAvailable) {
  Preconnector preconnector("https://a.test");
  CreateFakeSpdySession(preconnector.GetStreamKey());

  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsOk());
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectActiveStreamsAvailable) {
  Preconnector preconnector("http://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsOk());
  ASSERT_EQ(group.GetAttemptManagerForTesting(), nullptr);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectFail) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(*preconnector.result(), IsError(ERR_FAILED));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleStreamsHttp1) {
  constexpr size_t kNumStreams = 2;

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (size_t i = 0; i < kNumStreams; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(),
            kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), kNumStreams);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleStreamsHttp2) {
  constexpr size_t kNumStreams = 2;

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector("https://a.test");

  HttpStreamKey stream_key = preconnector.GetStreamKey();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), false));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectRequireHttp1) {
  constexpr size_t kNumStreams = 2;

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector("https://a.test");

  HttpStreamKey stream_key = preconnector.GetStreamKey();
  http_server_properties()->SetHTTP11Required(
      stream_key.destination(), stream_key.network_anonymization_key());

  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssls;
  for (size_t i = 0; i < kNumStreams; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
    auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    ssl->next_protos_expected_in_ssl_config = {kProtoHTTP11};
    socket_factory()->AddSSLSocketDataProvider(ssl.get());
    ssls.emplace_back(std::move(ssl));
  }

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(), 2u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), 2u);
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), false));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleStreamsOkAndFail) {
  constexpr size_t kNumStreams = 2;

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  std::vector<MockConnect> connects = {
      {MockConnect(ASYNC, OK), MockConnect(ASYNC, ERR_FAILED)}};
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (const auto& connect : connects) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(connect);
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(),
            kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleStreamsFailAndOk) {
  constexpr size_t kNumStreams = 2;

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  std::vector<MockConnect> connects = {
      {MockConnect(ASYNC, ERR_FAILED), MockConnect(ASYNC, OK)}};
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (const auto& connect : connects) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(connect);
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.GetAttemptManagerForTesting()->InFlightAttemptCount(),
            kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleRequests) {
  constexpr std::string_view kDestination("http://a.test");

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector1(kDestination);
  Preconnector preconnector2(kDestination);

  MockConnectCompleter completers[2];
  std::vector<MockConnect> connects = {
      {MockConnect(&completers[0]), MockConnect(&completers[1])}};
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (const auto& connect : connects) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(connect);
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  int rv = preconnector1.set_num_streams(1).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = preconnector2.set_num_streams(2).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  ASSERT_FALSE(preconnector1.result().has_value());
  ASSERT_FALSE(preconnector2.result().has_value());

  completers[0].Complete(OK);
  RunUntilIdle();
  ASSERT_TRUE(preconnector1.result().has_value());
  EXPECT_THAT(*preconnector1.result(), IsOk());
  ASSERT_FALSE(preconnector2.result().has_value());

  completers[1].Complete(OK);
  RunUntilIdle();
  EXPECT_THAT(preconnector2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectReachedGroupLimit) {
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  constexpr size_t kNumStreams = 2;

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  EXPECT_THAT(preconnector.result(),
              Optional(IsError(ERR_PRECONNECT_MAX_SOCKET_LIMIT)));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 1;
  constexpr size_t kMaxPerPool = 2;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  constexpr size_t kNumStreams = 2;

  auto key_a = StreamKeyBuilder("http://a.test").Build();
  pool().GetOrCreateGroupForTesting(key_a).CreateTextBasedStream(
      std::make_unique<FakeStreamSocket>(),
      StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  Preconnector preconnector_b("http://b.test");

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  int rv = preconnector_b.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  Group& group_b =
      pool().GetOrCreateGroupForTesting(preconnector_b.GetStreamKey());
  EXPECT_THAT(preconnector_b.result(),
              Optional(IsError(ERR_PRECONNECT_MAX_SOCKET_LIMIT)));
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       RequestStreamAndPreconnectWhileFailing) {
  constexpr std::string_view kDestination = "http://a.test";

  // Add two fake DNS resolutions (one for failing case, another is for success
  // case).
  for (size_t i = 0; i < 2; ++i) {
    FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
    endpoint_request
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .set_start_result(OK);
  }

  auto failed_data = std::make_unique<SequencedSocketData>();
  failed_data->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(failed_data.get());

  auto success_data = std::make_unique<SequencedSocketData>();
  success_data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(success_data.get());

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());

  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_CONNECTION_RESET)));

  // The first request isn't destroyed yet so the failing attempt manager is
  // still alive. A request that comes during a failure also fails.
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_CONNECTION_RESET)));

  // Preconnect fails too.
  Preconnector preconnector1(kDestination);
  EXPECT_THAT(preconnector1.Preconnect(pool()), IsError(ERR_CONNECTION_RESET));

  // Destroy failed requests. This should destroy the failing attempt manager.
  requester1.ResetRequest();
  requester2.ResetRequest();

  // Request a stream again. This time server is happy to accept the connection.
  StreamRequester requester3;
  requester3.set_destination(kDestination).RequestStream(pool());

  RunUntilIdle();
  EXPECT_THAT(requester3.result(), Optional(IsOk()));

  Preconnector preconnector2(kDestination);
  EXPECT_THAT(preconnector2.Preconnect(pool()), IsOk());
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReleaseStreamWhileFailing) {
  constexpr std::string_view kDestination = "http://a.test";

  SequencedSocketData data1;
  data1.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(&data1);

  SequencedSocketData data2;
  data2.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  socket_factory()->AddSocketDataProvider(&data2);

  // Add two fake DNS resolutions (one for success case, another is for failure
  // case).
  for (size_t i = 0; i < 2; ++i) {
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
  }

  // Create an active HttpStream.
  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream1 = requester1.ReleaseStream();
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream1->RegisterRequest(&request_info);
  stream1->InitializeStream(/*can_send_early=*/false, RequestPriority::IDLE,
                            NetLogWithSource(), base::DoNothing());

  // Request the second stream. The request fails. The corresponding manager
  // becomes the failing state.
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_CONNECTION_REFUSED)));

  // Release the HttpStream. The manager should not do anything since it's
  // failing and requests are still alive.
  stream1.reset();

  // Reset the requests. The manager should complete.
  HttpStreamKey stream_key = requester1.GetStreamKey();
  requester1.ResetRequest();
  requester2.ResetRequest();
  ASSERT_FALSE(pool()
                   .GetOrCreateGroupForTesting(stream_key)
                   .GetAttemptManagerForTesting());
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectPriority) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  auto data_a = std::make_unique<SequencedSocketData>();
  data_a->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data_a.get());

  Preconnector preconnector("https://a.test");
  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(pool()
                .GetOrCreateGroupForTesting(preconnector.GetStreamKey())
                .GetAttemptManagerForTesting()
                ->GetPriority(),
            RequestPriority::IDLE);
}

// Tests that when an AttemptManager is failing, it's not treated as stalled.
TEST_F(HttpStreamPoolAttemptManagerTest, FailingIsNotStalled) {
  constexpr std::string_view kDestinationA = "http://a.test";
  constexpr std::string_view kDestinationB = "http://b.test";

  // For destination A. This fails.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  auto data_a = std::make_unique<SequencedSocketData>();
  data_a->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(data_a.get());

  // For destination B. This succeeds.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);
  auto data_b = std::make_unique<SequencedSocketData>();
  data_b->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data_b.get());

  StreamRequester requester_a;
  requester_a.set_destination(kDestinationA).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsError(ERR_CONNECTION_RESET)));

  StreamRequester requester_b;
  requester_b.set_destination(kDestinationB).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));

  // Release the connection for B. It triggers processing pending requests in
  // group/attemt manager for A. The group/attempt manager for A is still alive
  // because we don't release `requester_a` yet. The group/attempt manager
  // should not be treated as stalled because these are failing.
  requester_b.ReleaseStream().reset();
  EXPECT_FALSE(pool()
                   .GetOrCreateGroupForTesting(requester_a.GetStreamKey())
                   .GetAttemptManagerForTesting()
                   ->IsStalledByPoolLimit());
}

// Tests that when an AttemptManager has a SPDY session, it's not treated as
// stalled.
TEST_F(HttpStreamPoolAttemptManagerTest, HavingSpdySessionIsNotStalled) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  EXPECT_FALSE(pool()
                   .GetOrCreateGroupForTesting(requester.GetStreamKey())
                   .GetAttemptManagerForTesting()
                   ->IsStalledByPoolLimit());
}

// Tests that when an AttemptManager has a QUIC session, it's not treated as
// stalled.
TEST_F(HttpStreamPoolAttemptManagerTest, HavingQuicSessionIsNotStalled) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  EXPECT_FALSE(pool()
                   .GetOrCreateGroupForTesting(requester.GetStreamKey())
                   .GetAttemptManagerForTesting()
                   ->IsStalledByPoolLimit());
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReuseTypeUnused) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  StreamRequester requester;
  requester.RequestStream(pool());
  RunUntilIdle();
  ASSERT_THAT(requester.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  ASSERT_FALSE(stream->IsConnectionReused());
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReuseTypeUnusedIdle) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  // Preconnect to put an idle stream to the pool.
  Preconnector preconnector("http://a.test");
  preconnector.Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(pool()
                .GetOrCreateGroupForTesting(preconnector.GetStreamKey())
                .IdleStreamSocketCount(),
            1u);

  StreamRequester requester;
  requester.RequestStream(pool());
  RunUntilIdle();
  ASSERT_THAT(requester.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  ASSERT_TRUE(stream->IsConnectionReused());
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReuseTypeReusedIdle) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  StreamRequester requester1;
  requester1.RequestStream(pool());
  RunUntilIdle();
  ASSERT_THAT(requester1.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream1 = requester1.ReleaseStream();
  ASSERT_FALSE(stream1->IsConnectionReused());

  // Destroy the stream to make it an idle stream.
  stream1.reset();

  StreamRequester requester2;
  requester2.RequestStream(pool());
  RunUntilIdle();
  ASSERT_THAT(requester2.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream2 = requester2.ReleaseStream();
  ASSERT_TRUE(stream2->IsConnectionReused());
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicOk) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  // Set `is_quic_known_to_work_on_current_network` to false to check the flag
  // is updated to true after the QUIC attempt succeeds.
  quic_session_pool()->set_has_quic_ever_worked_on_current_network(false);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  AddQuicData();

  // Make TCP attempts stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  ASSERT_FALSE(requester.result().has_value());

  // Call both update and finish callbacks to make sure we don't attempt twice
  // for a single endpoint.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated()
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_THAT(pool()
                  .GetOrCreateGroupForTesting(requester.GetStreamKey())
                  .GetAttemptManagerForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsOk()));
  EXPECT_TRUE(quic_session_pool()->has_quic_ever_worked_on_current_network());

  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  LoadTimingInfo timing_info;
  ASSERT_TRUE(stream->GetLoadTimingInfo(&timing_info));
  ValidateConnectTiming(timing_info.connect_timing);
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicOkSynchronouslyNoTcpAttempt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  AddQuicData();

  // No TCP data is needed because QUIC session attempt succeeds synchronously.

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());

  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .GetAttemptManagerForTesting();
  ASSERT_EQ(manager->InFlightAttemptCount(), 0u);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicOkDnsAlpn) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  AddQuicData();

  // Make TCP attempts stalled forever.
  SequencedSocketData tcp_data1;
  tcp_data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data1);
  SequencedSocketData tcp_data2;
  tcp_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data2);

  // Create two requests to make sure that one success QUIC session creation
  // completes all on-going requests.
  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder()
                         .add_v4("192.0.2.1")
                         .set_alpns({"h3", "h2"})
                         .endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_THAT(pool()
                  .GetOrCreateGroupForTesting(requester1.GetStreamKey())
                  .GetAttemptManagerForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsOk()));
}

// Tests that QUIC is not attempted when marked broken.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicBroken) {
  AlternativeService alternative_service(kProtoQUIC, "www.example.org", 443);
  http_server_properties()->MarkAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_NE(requester.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicFailBeforeTls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  MockConnectCompleter tls_completer;
  SequencedSocketData tls_data;
  tls_data.set_connect_data(MockConnect(&tls_completer));
  socket_factory()->AddSocketDataProvider(&tls_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  quic_completer.Complete(ERR_CONNECTION_REFUSED);
  // Fast forward to make QUIC attempt fail first.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(pool()
                  .GetOrCreateGroupForTesting(requester.GetStreamKey())
                  .GetAttemptManagerForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsError(ERR_CONNECTION_REFUSED)));
  ASSERT_FALSE(requester.result().has_value());

  tls_completer.Complete(ERR_SOCKET_NOT_CONNECTED);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_SOCKET_NOT_CONNECTED)));

  // QUIC should not be marked as broken because TLS attempt also failed.
  const AlternativeService alternative_service(
      NextProto::kProtoQUIC,
      HostPortPair::FromSchemeHostPort(requester.GetStreamKey().destination()));
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicFailAfterTls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  MockConnectCompleter tls_completer;
  SequencedSocketData tls_data;
  tls_data.set_connect_data(MockConnect(&tls_completer));
  socket_factory()->AddSocketDataProvider(&tls_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  tls_completer.Complete(ERR_SOCKET_NOT_CONNECTED);
  // Fast forward to make TLS attempt fail first.
  FastForwardBy(base::Milliseconds(1));
  ASSERT_FALSE(requester.result().has_value());

  quic_completer.Complete(ERR_CONNECTION_REFUSED);
  requester.WaitForResult();
  EXPECT_THAT(pool()
                  .GetOrCreateGroupForTesting(requester.GetStreamKey())
                  .GetAttemptManagerForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsError(ERR_CONNECTION_REFUSED)));
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_CONNECTION_REFUSED)));

  // QUIC should not be marked as broken because TLS attempt also failed.
  const AlternativeService alternative_service(
      NextProto::kProtoQUIC,
      HostPortPair::FromSchemeHostPort(requester.GetStreamKey().destination()));
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicFailNonBrokenErrors) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  const int kErrors[] = {ERR_NETWORK_CHANGED, ERR_INTERNET_DISCONNECTED};
  for (const int net_error : kErrors) {
    // Reset HttpServerProperties.
    InitializeSession();

    MockQuicData quic_data(quic_version());
    quic_data.AddConnect(ASYNC, net_error);
    quic_data.AddSocketDataToFactory(socket_factory());

    SequencedSocketData tcp_data;
    socket_factory()->AddSocketDataProvider(&tcp_data);
    SSLSocketDataProvider ssl(ASYNC, OK);
    socket_factory()->AddSSLSocketDataProvider(&ssl);

    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);

    StreamRequester requester;
    requester.set_destination(kDefaultDestination)
        .set_quic_version(quic_version())
        .RequestStream(pool());
    requester.WaitForResult();
    EXPECT_THAT(requester.result(), Optional(IsOk()));
    EXPECT_NE(requester.negotiated_protocol(), NextProto::kProtoQUIC);

    // QUIC should not be marked as broken because QUIC attempt failed with
    // a protocol independent error.
    const AlternativeService alternative_service(
        NextProto::kProtoQUIC, HostPortPair::FromSchemeHostPort(
                                   requester.GetStreamKey().destination()));
    EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
        alternative_service, NetworkAnonymizationKey()))
        << ErrorToString(net_error);
  }
}

// Test that NetErrorDetails is populated when a QUIC session is created but
// it fails later.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicNetErrorDetails) {
  // QUIC attempt will pause. When resumed, it will fail.
  MockQuicData quic_data(quic_version());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData tls_data;
  tls_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory()->AddSocketDataProvider(&tls_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  crypto_client_stream_factory()->set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());

  // Fast forward to make TLS attempt fail first.
  FastForwardBy(base::Milliseconds(1));
  quic_data.Resume();
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_QUIC_PROTOCOL_ERROR)));
  EXPECT_EQ(requester.net_error_details().quic_connection_error,
            quic::QUIC_PACKET_READ_ERROR);
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicCanUseExistingSession) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  AddQuicData();

  // Make TCP attempts stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());

  // Invoke the update callback, run tasks, then invoke the finish callback to
  // make sure the finish callback checks the existing session.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  // The previous request created a session. This request should use the
  // existing session.
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));

  EXPECT_THAT(pool()
                  .GetOrCreateGroupForTesting(requester1.GetStreamKey())
                  .GetAttemptManagerForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AlternativeSerivcesDisabled) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());
  RunUntilIdle();

  EXPECT_THAT(requester.result(), Optional(IsOk()));
  ASSERT_FALSE(pool()
                   .GetOrCreateGroupForTesting(requester.GetStreamKey())
                   .GetAttemptManagerForTesting()
                   ->GetQuicTaskResultForTesting()
                   .has_value());
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       AlternativeSerivcesDisabledQuicSessionExists) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);

  // Prerequisite: Create a QUIC session.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  AddQuicData();

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  // Actual test: Request a stream without alternative services.
  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_NE(requester2.negotiated_protocol(), NextProto::kProtoQUIC);
}

// Tests that QUIC attempt fails when there is no known QUIC version and the
// DNS resolution indicates that the endpoint doesn't support QUIC.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicEndpointNotFoundNoDnsAlpn) {
  // Set that QUIC is working on the current network.
  quic_session_pool()->set_has_quic_ever_worked_on_current_network(true);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic::ParsedQuicVersion::Unsupported())
      .RequestStream(pool());
  RunUntilIdle();

  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_THAT(pool()
                  .GetOrCreateGroupForTesting(requester.GetStreamKey())
                  .GetAttemptManagerForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN)));
  // No matching ALPN should not update
  // `is_quic_known_to_work_on_current_network()`.
  EXPECT_TRUE(quic_session_pool()->has_quic_ever_worked_on_current_network());

  // QUIC should not be marked as broken.
  const AlternativeService alternative_service(
      NextProto::kProtoQUIC,
      HostPortPair::FromSchemeHostPort(requester.GetStreamKey().destination()));
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicPreconnect) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  AddQuicData();

  SequencedSocketData tcp_data1;
  tcp_data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data1);
  SequencedSocketData tcp_data2;
  tcp_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data2);

  Preconnector preconnector1(kDefaultDestination);
  preconnector1.set_num_streams(2)
      .set_quic_version(quic_version())
      .Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector1.result(), Optional(IsOk()));

  // This preconnect request should complete immediately because we already have
  // an existing QUIC session.
  Preconnector preconnector2(kDefaultDestination);
  int rv = preconnector2.set_num_streams(1)
               .set_quic_version(quic_version())
               .Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(rv, IsOk());
}

// Tests that two destinations that resolve to the same IP address share the
// same QUIC session if allowed.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicMatchingIpSession) {
  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  FakeServiceEndpointRequest* endpoint_request1 = resolver()->AddFakeRequest();
  endpoint_request1
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request2 = resolver()->AddFakeRequest();
  endpoint_request2
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester2;
  requester2.set_destination(kAltDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  ASSERT_EQ(quic_session_pool()->FindExistingSession(
                requester1.GetStreamKey().ToQuicSessionKey(),
                requester1.GetStreamKey().destination()),
            quic_session_pool()->FindExistingSession(
                requester2.GetStreamKey().ToQuicSessionKey(),
                requester2.GetStreamKey().destination()));
}

// The same as above test, but the ServiceEndpointRequest provides two IP
// addresses separately, the first address does not match the existing session
// and the second address matches the existing session.
TEST_F(HttpStreamPoolAttemptManagerTest,
       QuicMatchingIpSessionOnEndpointsUpdated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  // Make the second QUIC attempt stalled forever.
  SequencedSocketData quic_data2;
  quic_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&quic_data2);

  FakeServiceEndpointRequest* endpoint_request1 = resolver()->AddFakeRequest();
  endpoint_request1
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request2 = resolver()->AddFakeRequest();

  StreamRequester requester2;
  requester2.set_destination(kAltDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  endpoint_request2
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester2.result().has_value());

  endpoint_request2
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_EQ(quic_session_pool()->FindExistingSession(
                requester1.GetStreamKey().ToQuicSessionKey(),
                requester1.GetStreamKey().destination()),
            quic_session_pool()->FindExistingSession(
                requester2.GetStreamKey().ToQuicSessionKey(),
                requester2.GetStreamKey().destination()));
}

// Tests that preconnect completes when there is a QUIC session of which IP
// address matches to the service endpoint resolution of the preconnect.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicPreconnectMatchingIpSession) {
  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  FakeServiceEndpointRequest* endpoint_request1 = resolver()->AddFakeRequest();
  endpoint_request1
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  FakeServiceEndpointRequest* endpoint_request2 = resolver()->AddFakeRequest();
  endpoint_request2
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector2(kAltDestination);
  preconnector2.set_quic_version(quic_version()).Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector2.result(), Optional(IsOk()));
  EXPECT_EQ(quic_session_pool()->FindExistingSession(
                requester1.GetStreamKey().ToQuicSessionKey(),
                requester1.GetStreamKey().destination()),
            quic_session_pool()->FindExistingSession(
                preconnector2.GetStreamKey().ToQuicSessionKey(),
                preconnector2.GetStreamKey().destination()));
}

// Tests that when disabled IP-based pooling, QUIC attempts are also disabled.
// TODO(crbug.com/346835898): Make sure this behavior is what we actually want.
// In production code, we currently disable both IP-based pooling and QUIC at
// the same time.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicMatchingIpSessionDisabled) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_enable_ip_based_pooling(false)
      .RequestStream(pool());
  RunUntilIdle();

  EXPECT_THAT(requester.result(), Optional(IsOk()));
  ASSERT_FALSE(pool()
                   .GetOrCreateGroupForTesting(requester.GetStreamKey())
                   .GetAttemptManagerForTesting()
                   ->GetQuicTaskResultForTesting()
                   .has_value());
}

TEST_F(HttpStreamPoolAttemptManagerTest, DelayStreamAttemptQuicOk) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  AddQuicData();

  // Don't add any TCP data. This makes sure that the following request
  // completes with a QUIC session without attempting TCP-based protocols.

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DelayStreamAttemptQuicFail) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data->AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // QUIC should be marked as broken.
  const AlternativeService alternative_service(
      NextProto::kProtoQUIC,
      HostPortPair::FromSchemeHostPort(requester.GetStreamKey().destination()));
  EXPECT_TRUE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DelayStreamAttemptDelayPassed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data->AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(kDelay);

  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       DelayStreamAttemptDisableAlternativeServicesLater) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data->AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnOk) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  AddQuicData();

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnExistingSession) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  AddQuicData();

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // The first request. It should create a QUIC session.
  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination).RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_EQ(requester1.negotiated_protocol(), NextProto::kProtoQUIC);

  // The second request. The request disables alternative services but the
  // QUIC session should be used because QUIC is forced by the
  // HttpNetworkSession. If the second request doesn't use the existing session
  // this test fails because we call AddQuicData() only once so we only added
  // mock reads and writes for only one QUIC connection.
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_EQ(requester2.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnFail) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data->AddSocketDataToFactory(socket_factory());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_CONNECTION_REFUSED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnPreconnectOk) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  AddQuicData();

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector(kDefaultDestination);
  preconnector.Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnPreconnectFail) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data->AddSocketDataToFactory(socket_factory());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector(kDefaultDestination);
  preconnector.Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_CONNECTION_REFUSED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicSessionGoneBeforeUsing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  QuicTestPacketMaker* client_maker = CreateQuicClientPacketMaker();
  MockQuicData quic_data(quic_version());
  quic_data.AddWrite(SYNCHRONOUS, client_maker->MakeInitialSettingsPacket(
                                      /*packet_number=*/1));
  quic_data.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  quic_data.AddSocketDataToFactory(socket_factory());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // QUIC attempt succeeds since we didn't require confirmation.
  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // Try to initialize `stream`. The underlying socket was already closed so
  // the initialization fails.
  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  int rv =
      stream->InitializeStream(/*can_send_early=*/false, RequestPriority::IDLE,
                               NetLogWithSource(), base::DoNothing());
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_CLOSED));
}

TEST_F(HttpStreamPoolAttemptManagerTest, GetInfoAsValue) {
  // Add an idle stream to a.test and create an in-flight connection attempt for
  // b.test.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester_a.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data_b = std::make_unique<SequencedSocketData>();
  data_b->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data_b.get());

  requester_b.RequestStream(pool());

  base::Value::Dict info = pool().GetInfoAsValue();
  EXPECT_THAT(info.FindInt("idle_socket_count"), Optional(1));
  EXPECT_THAT(info.FindInt("connecting_socket_count"), Optional(1));
  EXPECT_THAT(info.FindInt("max_socket_count"),
              Optional(pool().max_stream_sockets_per_pool()));
  EXPECT_THAT(info.FindInt("max_sockets_per_group"),
              Optional(pool().max_stream_sockets_per_group()));

  base::Value::Dict* groups_info = info.FindDict("groups");
  ASSERT_TRUE(groups_info);

  base::Value::Dict* info_a =
      groups_info->FindDict(requester_a.GetStreamKey().ToString());
  ASSERT_TRUE(info_a);
  EXPECT_THAT(info_a->FindInt("active_socket_count"), Optional(1));
  EXPECT_THAT(info_a->FindInt("idle_socket_count"), Optional(1));

  base::Value::Dict* info_b =
      groups_info->FindDict(requester_b.GetStreamKey().ToString());
  ASSERT_TRUE(info_b);
  EXPECT_THAT(info_b->FindInt("active_socket_count"), Optional(1));
  EXPECT_THAT(info_b->FindInt("idle_socket_count"), Optional(0));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcH2OkOriginFail) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. Negotiate HTTP/2 with the alternative service.
  const MockRead alt_reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockWrite alt_writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  SequencedSocketData alt_data(alt_reads, alt_writes);
  socket_factory()->AddSocketDataProvider(&alt_data);
  SSLSocketDataProvider alt_ssl(ASYNC, OK);
  alt_ssl.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&alt_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. The connection is refused.
  StaticSocketDataProvider origin_data;
  origin_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  socket_factory()->AddSocketDataProvider(&origin_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  requester.ResetRequest();
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcFailOriginOk) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. The connection is reset.
  StaticSocketDataProvider alt_data;
  alt_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(&alt_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. Negotiated HTTP/1.1 with the origin.
  StaticSocketDataProvider origin_data;
  socket_factory()->AddSocketDataProvider(&origin_data);
  SSLSocketDataProvider origin_ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&origin_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  requester.ResetRequest();
  EXPECT_TRUE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcNegotiatedWithH1) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. Negotiated with HTTP/1.1.
  StaticSocketDataProvider alt_data;
  socket_factory()->AddSocketDataProvider(&alt_data);
  SSLSocketDataProvider alt_ssl(ASYNC, OK);
  alt_ssl.next_proto = NextProto::kProtoHTTP11;
  socket_factory()->AddSSLSocketDataProvider(&alt_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. The connection is refused.
  StaticSocketDataProvider origin_data;
  origin_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  socket_factory()->AddSocketDataProvider(&origin_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(),
              Optional(IsError(ERR_ALPN_NEGOTIATION_FAILED)));
  requester.ResetRequest();
  // Both the origin and alternavie service failed, so the alternative service
  // should not be marked broken.
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcCertificateError) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. Certificate is invalid.
  StaticSocketDataProvider alt_data;
  socket_factory()->AddSocketDataProvider(&alt_data);
  SSLSocketDataProvider alt_ssl(ASYNC, ERR_CERT_DATE_INVALID);
  alt_ssl.next_proto = NextProto::kProtoHTTP11;
  socket_factory()->AddSSLSocketDataProvider(&alt_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. The connection is stalled forever.
  StaticSocketDataProvider origin_data;
  origin_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&origin_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_CERT_DATE_INVALID)));
  requester.ResetRequest();
  // The alternavie service failed and origin didn't complete, so the
  // alternative service should not be marked broken.
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcSetPriority) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. The connection is stalled forever.
  StaticSocketDataProvider alt_data;
  alt_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&alt_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. The connection is stalled forever.
  StaticSocketDataProvider origin_data;
  origin_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&origin_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  HttpStreamRequest* request =
      requester.set_priority(RequestPriority::LOW).RequestStream(pool());

  AttemptManager* origin_manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .GetAttemptManagerForTesting();
  ASSERT_TRUE(origin_manager);
  EXPECT_EQ(origin_manager->GetPriority(), RequestPriority::LOW);

  HttpStreamKey alt_stream_key =
      StreamKeyBuilder()
          .set_destination(url::SchemeHostPort(
              url::kHttpsScheme, kAlternative.host(), kAlternative.port()))
          .Build();
  AttemptManager* alt_manager = pool()
                                    .GetOrCreateGroupForTesting(alt_stream_key)
                                    .GetAttemptManagerForTesting();
  ASSERT_TRUE(alt_manager);
  EXPECT_EQ(alt_manager->GetPriority(), RequestPriority::LOW);

  request->SetPriority(RequestPriority::HIGHEST);
  EXPECT_EQ(origin_manager->GetPriority(), RequestPriority::HIGHEST);
  EXPECT_EQ(alt_manager->GetPriority(), RequestPriority::HIGHEST);
}

TEST_F(HttpStreamPoolAttemptManagerTest, FlushWithError) {
  // Add an idle stream to a.test and create an in-flight connection attempt for
  // b.test.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester_a.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data_b = std::make_unique<SequencedSocketData>();
  data_b->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data_b.get());

  requester_b.RequestStream(pool());

  // At this point, there are 2 active streams (one is idle and the other is
  // in-flight).
  EXPECT_EQ(pool().TotalActiveStreamCount(), 2u);

  // Flushing should destroy all active streams and in-flight attempts.
  pool().FlushWithError(ERR_ABORTED, "For testing");
  EXPECT_EQ(pool().TotalActiveStreamCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, UnsafePort) {
  StreamRequester requester;
  requester.set_destination("http://www.example.org:7");

  const url::SchemeHostPort destination =
      requester.GetStreamKey().destination();
  ASSERT_FALSE(
      IsPortAllowedForScheme(destination.port(), destination.scheme()));

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_UNSAFE_PORT)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectUnsafePort) {
  Preconnector preconnector("http://www.example.org:7");

  const url::SchemeHostPort destination =
      preconnector.GetStreamKey().destination();
  ASSERT_FALSE(
      IsPortAllowedForScheme(destination.port(), destination.scheme()));

  preconnector.Preconnect(pool());
  preconnector.WaitForResult();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_UNSAFE_PORT)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DisallowH1) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // Nagotiate to use HTTP/1.1.
  StaticSocketDataProvider data;
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.set_is_http1_allowed(false);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_H2_OR_QUIC_REQUIRED)));
}

// Tests that a bad proxy is reported to a ProxyResolutionService when falling
// back to the direct connection succeeds.
TEST_F(HttpStreamPoolAttemptManagerTest, ReportBadProxyAfterSuccessOnDirect) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  StaticSocketDataProvider data;
  socket_factory()->AddSocketDataProvider(&data);

  // Simulate that we have a bad proxy.
  ProxyInfo proxy_info;
  proxy_info.UsePacString("PROXY badproxy:80; DIRECT");
  proxy_info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource());

  StreamRequester requester;
  requester.set_proxy_info(proxy_info).RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // The ProxyResolutionService should know that the proxy is bad.
  auto proxy_chain = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_HTTP, "badproxy", 80);
  const ProxyRetryInfoMap retry_info_map =
      http_network_session()->proxy_resolution_service()->proxy_retry_info();
  auto it = retry_info_map.find(proxy_chain);
  ASSERT_TRUE(it != retry_info_map.end());
  EXPECT_THAT(it->second.net_error, IsError(ERR_PROXY_CONNECTION_FAILED));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DirectProxyInfoForIpProtection) {
  const auto kIpProtectionDirectChain =
      ProxyChain::ForIpProtection(std::vector<ProxyServer>());
  ProxyInfo proxy_info;
  proxy_info.UseProxyChain(kIpProtectionDirectChain);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  StaticSocketDataProvider data;
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.set_proxy_info(proxy_info).RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.used_proxy_info().ToDebugString(),
            proxy_info.ToDebugString());
}

// Regression test for crbug.com/369744951. Ensures that destroying
// an HttpNetworkSession, which owns an HttpStreamPool, doesn't cause a crash
// when a StreamSocket is returned to the pool in the middle of the destruction.
TEST_F(HttpStreamPoolAttemptManagerTest,
       DestroyHttpNetworkSessionWithSpdySession) {
  // Add a SpdySession. The session will be destroyed when the
  // HttpNetworkSession is being destroyed. The underlying StreamSocket will be
  // released to HttpStreamPool::Group.
  CreateFakeSpdySession(
      StreamKeyBuilder().set_destination("https://a.test").Build());

  // Create a request to a different destination. The request never finishes.
  StreamRequester requester;
  requester.set_destination("https://b.test");
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&data);
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Cancel the request before destructing HttpNetworkSession to avoid a
  // dangling pointer.
  requester.ResetRequest();

  // Destroying HttpNetworkSession should not cause crash.
  DestroyHttpNetworkSession();
}

// Regression test for crbug.com/371894055.
TEST_F(HttpStreamPoolAttemptManagerTest,
       AsyncQuicSessionDestroyRequestBeforeSessionCreation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  // Precondition: Create a QUIC session that can be shared for destinations
  // that are resolved to kCommonEndPoint.
  AddQuicData();

  SequencedSocketData tcp_data1;
  tcp_data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data1);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  // Actual test: Create a request that starts a QuicSessionAttempt, which
  // is later destroyed since there is a matching IP session.

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data2;
  tcp_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data2);

  StreamRequester requester2;
  requester2.set_destination(kAltDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester2.result().has_value());

  // Provide a different IP address to start a QUIC attempt.
  endpoint_request->set_crypto_ready(true)
      .add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester2.result().has_value());

  // Provide kCommonEndPoint so that the corresponding attempt manager cancel
  // the in-flight QUIC attempt and use the existing session.
  endpoint_request->set_crypto_ready(true)
      .add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointsUpdated();

  // Resume the QUIC attempt. This should not detect a dangling pointer.
  quic_completer.Complete(OK);
  requester2.WaitForResult();
}

}  // namespace net
