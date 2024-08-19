// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job.h"

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
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_handle.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/log/test_net_log.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
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

using ::testing::_;
using ::testing::Optional;

namespace net {

using test::IsError;
using test::IsOk;
using test::MockQuicData;
using test::QuicTestPacketMaker;

using Group = HttpStreamPool::Group;
using Job = HttpStreamPool::Job;

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
    return pool.Preconnect(
        GetStreamKey(), num_streams_, quic_version_,
        base::BindOnce(&Preconnector::OnComplete, base::Unretained(this)));
  }

  std::optional<int> result() const { return result_; }

 private:
  void OnComplete(int rv) { result_ = rv; }

  StreamKeyBuilder key_builder_;

  size_t num_streams_ = 1;

  quic::ParsedQuicVersion quic_version_ =
      quic::ParsedQuicVersion::Unsupported();

  std::optional<int> result_;
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

  StreamRequester& set_privacy_mode(PrivacyMode privacy_mode) {
    key_builder_.set_privacy_mode(privacy_mode);
    return *this;
  }

  StreamRequester& set_quic_version(quic::ParsedQuicVersion quic_version) {
    quic_version_ = quic_version;
    return *this;
  }

  HttpStreamKey GetStreamKey() const { return key_builder_.Build(); }

  HttpStreamRequest* RequestStream(HttpStreamPool& pool) {
    HttpStreamKey stream_key = GetStreamKey();
    request_ = pool.RequestStream(this, stream_key, priority_,
                                  allowed_bad_certs_, enable_ip_based_pooling_,
                                  enable_alternative_services_, quic_version_,
                                  NetLogWithSource());
    return request_.get();
  }

  void CancelRequest() { request_.reset(); }

  // HttpStreamRequest::Delegate methods:
  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    stream_ = std::move(stream);
    result_ = OK;
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
    result_ = status;
    net_error_details_ = net_error_details;
    resolve_error_info_ = resolve_error_info;
  }

  void OnCertificateError(int status, const SSLInfo& ssl_info) override {
    result_ = status;
    cert_error_ssl_info_ = ssl_info;
  }

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {
    NOTREACHED();
  }

  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override {
    CHECK(!cert_info_);
    result_ = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    cert_info_ = cert_info;
  }

  void OnQuicBroken() override {}

  void OnSwitchesToHttpStreamPool(
      HttpStreamKey stream_key,
      quic::ParsedQuicVersion quic_version) override {}

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

 private:
  StreamKeyBuilder key_builder_;

  RequestPriority priority_ = RequestPriority::IDLE;

  std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;

  bool enable_ip_based_pooling_ = true;

  bool enable_alternative_services_ = true;

  quic::ParsedQuicVersion quic_version_ =
      quic::ParsedQuicVersion::Unsupported();

  std::unique_ptr<HttpStreamRequest> request_;

  std::unique_ptr<HttpStream> stream_;
  std::optional<int> result_;
  NetErrorDetails net_error_details_;
  ResolveErrorInfo resolve_error_info_;
  SSLInfo cert_error_ssl_info_;
  scoped_refptr<SSLCertRequestInfo> cert_info_;
};

constexpr std::string_view kDefaultServerName = "www.example.org";
constexpr std::string_view kDefaultDestination = "https://www.example.org";

}  // namespace

class HttpStreamPoolJobTest : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolJobTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    feature_list_.InitAndEnableFeature(features::kHappyEyeballsV3);
    session_deps_.alternate_host_resolver =
        std::make_unique<FakeServiceEndpointResolver>();

    auto quic_context = std::make_unique<MockQuicContext>();
    quic_context->AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
    session_deps_.quic_context = std::move(quic_context);

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

 protected:
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

 private:
  base::test::ScopedFeatureList feature_list_;
  // For NetLog recording test coverage.
  RecordingNetLogObserver net_log_observer_;

  SpdySessionDependencies session_deps_;

  ProofVerifyDetailsChromium verify_details_;
  std::vector<std::unique_ptr<QuicTestPacketMaker>> quic_client_makers_;
  std::vector<std::unique_ptr<MockQuicData>> mock_quic_datas_;

  std::unique_ptr<HttpNetworkSession> http_network_session_;
};

TEST_F(HttpStreamPoolJobTest, ResolveEndpointFailedSync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request->set_start_result(ERR_FAILED);
  StreamRequester requester;
  requester.RequestStream(pool());
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));
}

TEST_F(HttpStreamPoolJobTest, ResolveEndpointFailedMultipleRequests) {
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

TEST_F(HttpStreamPoolJobTest, LoadState) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());

  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));

  RunUntilIdle();
  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(HttpStreamPoolJobTest, ResolveErrorInfo) {
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

TEST_F(HttpStreamPoolJobTest, DnsAliases) {
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

TEST_F(HttpStreamPoolJobTest, ConnectTiming) {
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

TEST_F(HttpStreamPoolJobTest, ConnectTimingDnsResolutionNotFinished) {
  constexpr base::TimeDelta kDnsUpdateDelay = base::Milliseconds(30);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("http://a.test").RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());

  FastForwardBy(kDnsUpdateDelay);
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
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

TEST_F(HttpStreamPoolJobTest, SetPriority) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  StreamRequester requester1;
  HttpStreamRequest* request1 =
      requester1.set_priority(RequestPriority::LOW).RequestStream(pool());
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);

  StreamRequester requester2;
  HttpStreamRequest* request2 =
      requester2.set_priority(RequestPriority::IDLE).RequestStream(pool());
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);

  request2->SetPriority(RequestPriority::HIGHEST);
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::HIGHEST);

  // Check `request2` completes first.

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data1.get());

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointsUpdated();
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

TEST_F(HttpStreamPoolJobTest, TcpFailSync) {
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

TEST_F(HttpStreamPoolJobTest, TcpFailAsync) {
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

TEST_F(HttpStreamPoolJobTest, TlsOk) {
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

TEST_F(HttpStreamPoolJobTest, TlsCryptoReadyDelayed) {
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

TEST_F(HttpStreamPoolJobTest, CertificateError) {
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

TEST_F(HttpStreamPoolJobTest, NeedsClientAuth) {
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
TEST_F(HttpStreamPoolJobTest, TcpFailAfterNeedsClientAuth) {
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

TEST_F(HttpStreamPoolJobTest, RequestCancelledBeforeAttemptSuccess) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  requester.CancelRequest();
  RunUntilIdle();

  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolJobTest, OneIPEndPointFailed) {
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

TEST_F(HttpStreamPoolJobTest, IPEndPointTimedout) {
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

TEST_F(HttpStreamPoolJobTest, IPEndPointsSlow) {
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
  Job* job = pool()
                 .GetOrCreateGroupForTesting(requester.GetStreamKey())
                 .GetJobForTesting();
  ASSERT_EQ(job->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(request->completed());

  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_EQ(job->InFlightAttemptCount(), 2u);
  ASSERT_EQ(job->PendingRequestCount(), 0u);
  ASSERT_FALSE(request->completed());

  // FastForwardBy() executes non-delayed tasks so the request finishes
  // immediately.
  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_TRUE(request->completed());
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolJobTest, PauseSlowTimerAfterTcpHandshakeForTls) {
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
  Job* job = pool()
                 .GetOrCreateGroupForTesting(requester.GetStreamKey())
                 .GetJobForTesting();
  ASSERT_EQ(job->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(requester.result().has_value());

  // Complete TCP handshake after a delay that is less than the connection
  // attempt delay.
  constexpr base::TimeDelta kTcpDelay = base::Milliseconds(30);
  ASSERT_LT(kTcpDelay, HttpStreamPool::kConnectionAttemptDelay);
  FastForwardBy(kTcpDelay);
  tcp_connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(job->InFlightAttemptCount(), 1u);

  // Fast-forward to the connection attempt delay. Since the in-flight attempt
  // has completed TCP handshake and is waiting for HTTPS RR, the job shouldn't
  // start another attempt.
  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_EQ(job->InFlightAttemptCount(), 1u);

  // Complete DNS resolution fully.
  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointRequestFinished(
      OK);
  ASSERT_EQ(job->InFlightAttemptCount(), 1u);

  // Fast-forward to the connection attempt delay again. This time the in-flight
  // attempt is still doing TLS handshake, it's treated as slow and the job
  // should start another attempt.
  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_EQ(job->InFlightAttemptCount(), 2u);

  // Complete the second attempt. The request should finish successfully.
  tcp_connect_completer2.Complete(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolJobTest, ReachedGroupLimit) {
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
  Job* job = group.GetJobForTesting();
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), kMaxPerGroup);
  ASSERT_EQ(job->PendingRequestCount(), 0u);

  // This request should not start an attempt as the group reached its limit.
  StreamRequester stalled_requester;
  HttpStreamRequest* stalled_request = stalled_requester.RequestStream(pool());
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());
  data_providers.emplace_back(std::move(data));

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), kMaxPerGroup);
  ASSERT_EQ(job->PendingRequestCount(), 1u);
  ASSERT_EQ(stalled_request->GetLoadState(),
            LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET);

  // Finish all in-flight attempts successfully.
  RunUntilIdle();
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), 0u);
  ASSERT_EQ(job->PendingRequestCount(), 1u);

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
  ASSERT_EQ(job->InFlightAttemptCount(), 1u);
  ASSERT_EQ(job->PendingRequestCount(), 0u);

  RunUntilIdle();

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), 0u);
  ASSERT_EQ(job->PendingRequestCount(), 0u);
  ASSERT_TRUE(stalled_request->completed());
  std::unique_ptr<HttpStream> stream = stalled_requester.ReleaseStream();
  ASSERT_TRUE(stream);
}

TEST_F(HttpStreamPoolJobTest, ReachedPoolLimit) {
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
  Job* job_b = group_b.GetJobForTesting();
  ASSERT_FALSE(request2->completed());
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_TRUE(pool().IsPoolStalled());
  ASSERT_EQ(job_b->InFlightAttemptCount(), 0u);
  ASSERT_EQ(job_b->PendingRequestCount(), 1u);

  // Release one HttpStream from group A. It should unblock the in-flight
  // request in group B.
  std::unique_ptr<HttpStream> released_stream = std::move(streams_a.back());
  streams_a.pop_back();
  released_stream.reset();
  RunUntilIdle();

  ASSERT_TRUE(request2->completed());
  ASSERT_EQ(job_b->PendingRequestCount(), 0u);
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());
}

TEST_F(HttpStreamPoolJobTest, ReachedPoolLimitHighPriorityGroupFirst) {
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

TEST_F(HttpStreamPoolJobTest, RequestStreamIdleStreamSocket) {
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

TEST_F(HttpStreamPoolJobTest, UseIdleStreamSocketAfterRelease) {
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
  Job* job = group.GetJobForTesting();
  ASSERT_FALSE(request->completed());
  ASSERT_EQ(job->PendingRequestCount(), 1u);

  // Release an active HttpStream. The underlying StreamSocket should be used
  // to the pending request.
  std::unique_ptr<HttpStream> released_stream = std::move(streams.back());
  streams.pop_back();

  released_stream.reset();
  ASSERT_TRUE(request->completed());
  ASSERT_EQ(job->PendingRequestCount(), 0u);
}

TEST_F(HttpStreamPoolJobTest,
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

TEST_F(HttpStreamPoolJobTest, ProcessPendingRequestDnsResolutionOngoing) {
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
TEST_F(HttpStreamPoolJobTest, CancelAttemptAndRequestsOnIPAddressChange) {
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

  Job* job1 = pool()
                  .GetOrCreateGroupForTesting(requester1.GetStreamKey())
                  .GetJobForTesting();
  Job* job2 = pool()
                  .GetOrCreateGroupForTesting(requester2.GetStreamKey())
                  .GetJobForTesting();
  ASSERT_EQ(job1->RequestCount(), 1u);
  ASSERT_EQ(job1->InFlightAttemptCount(), 1u);
  ASSERT_EQ(job2->RequestCount(), 1u);
  ASSERT_EQ(job2->InFlightAttemptCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();
  ASSERT_EQ(job1->RequestCount(), 0u);
  ASSERT_EQ(job1->InFlightAttemptCount(), 0u);
  ASSERT_EQ(job2->RequestCount(), 0u);
  ASSERT_EQ(job2->InFlightAttemptCount(), 0u);
  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
}

// Tests that the network change error is reported even when a different error
// has already happened.
TEST_F(HttpStreamPoolJobTest, IPAddressChangeAfterNeedsClientAuth) {
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

TEST_F(HttpStreamPoolJobTest, SSLConfigChangedCloseIdleStream) {
  StreamRequester requester;
  requester.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  ssl_config_service()->NotifySSLContextConfigChange();
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolJobTest,
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

TEST_F(HttpStreamPoolJobTest, SSLConfigForServersChanged) {
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

TEST_F(HttpStreamPoolJobTest, SpdyAvailableSession) {
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
TEST_F(HttpStreamPoolJobTest, ChangePriorityForPooledStreamRequest) {
  StreamRequester requester;
  requester.set_destination("https://a.test");

  CreateFakeSpdySession(requester.GetStreamKey());

  HttpStreamRequest* request = requester.RequestStream(pool());
  request->SetPriority(RequestPriority::HIGHEST);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  // HttpStream{,Request} don't provide a way to get its priority.
}

TEST_F(HttpStreamPoolJobTest, SpdyOk) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalConnectingStreamCount(), 0u);
}

TEST_F(HttpStreamPoolJobTest, SpdyCreateSessionFail) {
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

TEST_F(HttpStreamPoolJobTest, RequireHttp11AfterSpdySessionCreated) {
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
  // is still alive and the corresponding job is still alive. The existing SPDY
  // session should become unavailable.
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_HTTP_1_1_REQUIRED)));
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), /*is_websocket=*/false));
}

TEST_F(HttpStreamPoolJobTest,
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
  requester1.CancelRequest();

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

TEST_F(HttpStreamPoolJobTest, DoNotUseSpdySessionForHttpRequest) {
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

TEST_F(HttpStreamPoolJobTest, CloseIdleSpdySessionWhenPoolStalled) {
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

TEST_F(HttpStreamPoolJobTest, PreconnectRequireHttp11AfterSpdySessionCreated) {
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

TEST_F(HttpStreamPoolJobTest, SpdyReachedPoolLimit) {
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
  ASSERT_EQ(group_c.GetJobForTesting()->PendingRequestCount(), 1u);
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

TEST_F(HttpStreamPoolJobTest, SpdyMatchingIpSessionOk) {
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

TEST_F(HttpStreamPoolJobTest, SpdyMatchingIpSessionAlreadyHaveSession) {
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

TEST_F(HttpStreamPoolJobTest,
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

TEST_F(HttpStreamPoolJobTest, SpdyMatchingIpSessionDisabled) {
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

TEST_F(HttpStreamPoolJobTest, SpdyMatchingIpSessionKeyMismatch) {
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

TEST_F(HttpStreamPoolJobTest, SpdyMatchingIpSessionVerifyDomainFailed) {
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

TEST_F(HttpStreamPoolJobTest, ThrottleAttemptForSpdyBlockSecondAttempt) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 1u);

  // This should not enter an infinite loop.
  pool().ProcessPendingRequestsInGroups();

  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolJobTest, ThrottleAttemptForSpdyDelayPassedHttp2) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 1u);

  FastForwardBy(Job::kSpdyThrottleDelay);
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 2u);

  connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 0u);

  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolJobTest, ThrottleAttemptForSpdyDelayPassedHttp1) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 1u);

  FastForwardBy(Job::kSpdyThrottleDelay);
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 2u);

  connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 1u);

  connect_completer2.Complete(OK);
  RunUntilIdle();

  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolJobTest, PreconnectSpdySessionAvailable) {
  Preconnector preconnector("https://a.test");
  CreateFakeSpdySession(preconnector.GetStreamKey());

  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsOk());
}

TEST_F(HttpStreamPoolJobTest, PreconnectActiveStreamsAvailable) {
  Preconnector preconnector("http://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsOk());
}

TEST_F(HttpStreamPoolJobTest, PreconnectFail) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(*preconnector.result(), IsError(ERR_FAILED));
}

TEST_F(HttpStreamPoolJobTest, PreconnectMultipleStreamsHttp1) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), kNumStreams);
}

TEST_F(HttpStreamPoolJobTest, PreconnectMultipleStreamsHttp2) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), false));
}

TEST_F(HttpStreamPoolJobTest, PreconnectRequireHttp1) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), 2u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), 2u);
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key.ToSpdySessionKey(), false));
}

TEST_F(HttpStreamPoolJobTest, PreconnectMultipleStreamsOkAndFail) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolJobTest, PreconnectMultipleStreamsFailAndOk) {
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
  ASSERT_EQ(group.GetJobForTesting()->InFlightAttemptCount(), kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolJobTest, PreconnectMultipleRequests) {
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

TEST_F(HttpStreamPoolJobTest, PreconnectReachedGroupLimit) {
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

TEST_F(HttpStreamPoolJobTest, PreconnectReachedPoolLimit) {
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

TEST_F(HttpStreamPoolJobTest, RequestStreamAndPreconnectWhileFailing) {
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

  // The first request isn't destroyed yet so the failing job is still alive.
  // A request that comes during a failure also fails.
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_CONNECTION_RESET)));

  // Preconnect fails too.
  Preconnector preconnector1(kDestination);
  EXPECT_THAT(preconnector1.Preconnect(pool()), IsError(ERR_CONNECTION_RESET));

  // Destroy failed requests. This should destroy the failing job.
  requester1.CancelRequest();
  requester2.CancelRequest();

  // Request a stream again. This time server is happy to accept the connection.
  StreamRequester requester3;
  requester3.set_destination(kDestination).RequestStream(pool());

  RunUntilIdle();
  EXPECT_THAT(requester3.result(), Optional(IsOk()));

  Preconnector preconnector2(kDestination);
  EXPECT_THAT(preconnector2.Preconnect(pool()), IsOk());
}

TEST_F(HttpStreamPoolJobTest, ReuseTypeUnused) {
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

TEST_F(HttpStreamPoolJobTest, ReuseTypeUnusedIdle) {
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

TEST_F(HttpStreamPoolJobTest, ReuseTypeReusedIdle) {
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

TEST_F(HttpStreamPoolJobTest, QuicOk) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

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
                  .GetJobForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsOk()));

  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  LoadTimingInfo timing_info;
  ASSERT_TRUE(stream->GetLoadTimingInfo(&timing_info));
  ValidateConnectTiming(timing_info.connect_timing);
}

TEST_F(HttpStreamPoolJobTest, QuicOkDnsAlpn) {
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
                  .GetJobForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsOk()));
}

TEST_F(HttpStreamPoolJobTest, QuicCanUseExistingSession) {
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
                  .GetJobForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsOk()));
}

TEST_F(HttpStreamPoolJobTest, AlternativeSerivcesDisabled) {
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
                   .GetJobForTesting()
                   ->GetQuicTaskResultForTesting()
                   .has_value());
}

// Tests that QUIC attempt fails when there is no known QUIC version and the
// DNS resolution indicates that the endpoint doesn't support QUIC.
TEST_F(HttpStreamPoolJobTest, QuicEndpointNotFoundNoDnsAlpn) {
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
                  .GetJobForTesting()
                  ->GetQuicTaskResultForTesting(),
              Optional(IsError(ERR_FAILED)));
}

TEST_F(HttpStreamPoolJobTest, QuicPreconnect) {
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
TEST_F(HttpStreamPoolJobTest, QuicMatchingIpSession) {
  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  AddQuicData();

  // Make TCP attempts stalled forever.
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
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  ASSERT_EQ(quic_session_pool()->FindExistingSession(
                requester1.GetStreamKey().ToQuicSessionKey(),
                requester1.GetStreamKey().destination()),
            quic_session_pool()->FindExistingSession(
                requester2.GetStreamKey().ToQuicSessionKey(),
                requester2.GetStreamKey().destination()));
}

// Tests that when disabled IP-based pooling, QUIC attempts are also disabled.
// TODO(crbug.com/346835898): Make sure this behavior is what we actually want.
// In production code, we currently disable both IP-based pooling and QUIC at
// the same time.
TEST_F(HttpStreamPoolJobTest, QuicMatchingIpSessionDisabled) {
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
                   .GetJobForTesting()
                   ->GetQuicTaskResultForTesting()
                   .has_value());
}

TEST_F(HttpStreamPoolJobTest, DelayStreamAttemptQuicOk) {
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

TEST_F(HttpStreamPoolJobTest, DelayStreamAttemptQuicFail) {
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
}

TEST_F(HttpStreamPoolJobTest, DelayStreamAttemptDelayPassed) {
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

TEST_F(HttpStreamPoolJobTest,
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

}  // namespace net
