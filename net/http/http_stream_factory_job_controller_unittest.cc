// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_job_controller.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/schemeful_site.h"
#include "net/base/session_usage.h"
#include "net/base/test_proxy_delegate.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/alternative_service.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_factory_job.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/http/test_upload_data_stream_not_allow_http1.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/mock_proxy_resolver.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_peer.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::SizeIs;

namespace net::test {

namespace {

const char kServerHostname[] = "www.example.com";
const std::string_view kHttpTestUrls[] = {"http://www.example.com",
                                          "https://www.example.com"};

// The default delay for main job defined in QuicSessionPool::
// GetTimeDelayForWaitingJob().
const int kDefaultDelayMilliSecsForWaitingJob = 300;

// Phases in which errors will happen for HTTP, HTTPS and SOCKS5 tests.
enum class TcpErrorPhase {
  kHostResolution,
  kTcpConnect,
  kProxySslHandshake,
  kTunnelRead,
};

// Defined so it can be printed by Gtest.
template <typename Sink>
void AbslStringify(Sink& sink, TcpErrorPhase e) {
  switch (e) {
    case TcpErrorPhase::kHostResolution:
      sink.Append("HostResolution");
      break;
    case TcpErrorPhase::kTcpConnect:
      sink.Append("TcpConnect");
      break;
    case TcpErrorPhase::kProxySslHandshake:
      sink.Append("ProxySslHandshake");
      break;
    case TcpErrorPhase::kTunnelRead:
      sink.Append("TunnelRead");
  }
}

// Phases in which errors will happen for QUIC tests.
enum class QuicErrorPhase { kHostResolution, kProxySession, kUdpConnect };

// Defined so it can be printed by Gtest.
template <typename Sink>
void AbslStringify(Sink& sink, QuicErrorPhase e) {
  switch (e) {
    case QuicErrorPhase::kHostResolution:
      sink.Append("HostResolution");
      break;
    case QuicErrorPhase::kProxySession:
      sink.Append("ProxySession");
      break;
    case QuicErrorPhase::kUdpConnect:
      sink.Append("UdpConnect");
      break;
  }
}

class FailingProxyResolverFactory : public ProxyResolverFactory {
 public:
  FailingProxyResolverFactory() : ProxyResolverFactory(false) {}

  // ProxyResolverFactory override.
  int CreateProxyResolver(const scoped_refptr<PacFileData>& script_data,
                          std::unique_ptr<ProxyResolver>* result,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override {
    return ERR_PAC_SCRIPT_FAILED;
  }
};

// A subclass of QuicChromiumClientSession that "goes away" right after
// CreateHandle was called.
class MockQuicChromiumClientSession : public QuicChromiumClientSession {
 public:
  using QuicChromiumClientSession::QuicChromiumClientSession;

  std::unique_ptr<QuicChromiumClientSession::Handle> CreateHandle(
      url::SchemeHostPort destination) override {
    auto res = QuicChromiumClientSession::CreateHandle(destination);
    // Make the session go away right after it was created.
    SetGoingAwayForTesting(true);
    return res;
  }
};

// A mock HttpServerProperties::PrefDelegate that never finishes loading, so
// HttpServerProperties::IsInitialized() always returns false.
class MockPrefDelegate : public HttpServerProperties::PrefDelegate {
 public:
  MockPrefDelegate() = default;

  MockPrefDelegate(const MockPrefDelegate&) = delete;
  MockPrefDelegate& operator=(const MockPrefDelegate&) = delete;

  ~MockPrefDelegate() override = default;

  // HttpServerProperties::PrefDelegate implementation:
  const base::Value::Dict& GetServerProperties() const override {
    return empty_dict_;
  }
  void SetServerProperties(base::Value::Dict dict,
                           base::OnceClosure callback) override {}
  void WaitForPrefLoad(base::OnceClosure pref_loaded_callback) override {}

  base::Value::Dict empty_dict_;
};

// A `TestProxyDelegate` which always sets a `ProxyChain` with
// `is_for_ip_protection` set to true on the `ProxyInfo` it receives in
// `OnResolveProxy()`.
class TestProxyDelegateForIpProtection : public TestProxyDelegate {
 public:
  TestProxyDelegateForIpProtection() {
    set_proxy_chain(
        ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
            ProxyServer::SCHEME_HTTPS, "ip-pro", 443)}));
    set_extra_header_name(HttpRequestHeaders::kAuthorization);
  }
  void OnResolveProxy(const GURL& url,
                      const NetworkAnonymizationKey& network_anonymization_key,
                      const std::string& method,
                      const ProxyRetryInfoMap& proxy_retry_info,
                      ProxyInfo* result) override {
    ProxyList proxy_list;
    CHECK(proxy_chain().is_for_ip_protection());
    proxy_list.AddProxyChain(proxy_chain());
    proxy_list.AddProxyChain(ProxyChain::ForIpProtection({}));
    result->UseProxyList(proxy_list);
  }
  net::Error OnTunnelHeadersReceived(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      const net::HttpResponseHeaders& response_headers,
      net::CompletionOnceCallback callback) override {
    if (response_headers.response_code() == 502) {
      return net::ERR_PROXY_UNABLE_TO_CONNECT_TO_DESTINATION;
    }
    return net::OK;
  }
};

}  // anonymous namespace

class HttpStreamFactoryJobPeer {
 public:
  // Returns `num_streams_` of `job`. It should be 0 for non-preconnect Jobs.
  static int GetNumStreams(const HttpStreamFactory::Job* job) {
    return job->num_streams_;
  }

  // Return SpdySessionKey of `job`.
  static const SpdySessionKey GetSpdySessionKey(
      const HttpStreamFactory::Job* job) {
    return job->spdy_session_key_;
  }

  static void SetShouldReconsiderProxy(HttpStreamFactory::Job* job) {
    job->should_reconsider_proxy_ = true;
  }

  static void SetStream(HttpStreamFactory::Job* job,
                        std::unique_ptr<HttpStream> http_stream) {
    job->stream_ = std::move(http_stream);
  }

  static void SetQuicConnectionFailedOnDefaultNetwork(
      HttpStreamFactory::Job* job) {
    job->quic_request_.OnConnectionFailedOnDefaultNetwork();
  }
};

class JobControllerPeer {
 public:
  static bool main_job_is_blocked(
      HttpStreamFactory::JobController* job_controller) {
    return job_controller->main_job_is_blocked_;
  }

  static bool main_job_is_resumed(
      HttpStreamFactory::JobController* job_controller) {
    return job_controller->main_job_is_resumed_;
  }

  static void InitializeProxyInfo(
      HttpStreamFactory::JobController* job_controller) {
    job_controller->proxy_info_.UseDirect();
  }

  static AlternativeServiceInfo GetAlternativeServiceInfoFor(
      HttpStreamFactory::JobController* job_controller,
      const HttpRequestInfo& request_info,
      HttpStreamRequest::Delegate* delegate,
      HttpStreamRequest::StreamType stream_type) {
    return job_controller
        ->GetAdvertisedAltSvcFor(
            HttpStreamFactory::StreamRequestInfo(request_info), delegate,
            stream_type)
        .info;
  }

  static quic::ParsedQuicVersion SelectQuicVersion(
      HttpStreamFactory::JobController* job_controller,
      const quic::ParsedQuicVersionVector& advertised_versions) {
    return job_controller->SelectQuicVersion(advertised_versions);
  }

  static void SetAltJobFailedOnDefaultNetwork(
      HttpStreamFactory::JobController* job_controller) {
    DCHECK(job_controller->alternative_job() != nullptr);
    HttpStreamFactoryJobPeer::SetQuicConnectionFailedOnDefaultNetwork(
        job_controller->alternative_job_.get());
  }
  static void SetDnsAlpnH3JobFailedOnDefaultNetwork(
      HttpStreamFactory::JobController* job_controller) {
    DCHECK(job_controller->dns_alpn_h3_job() != nullptr);
    HttpStreamFactoryJobPeer::SetQuicConnectionFailedOnDefaultNetwork(
        job_controller->dns_alpn_h3_job_.get());
  }
};

class HttpStreamFactoryJobControllerTestBase : public TestWithTaskEnvironment {
 public:
  explicit HttpStreamFactoryJobControllerTestBase(
      bool happy_eyeballs_v3_enabled,
      std::vector<base::test::FeatureRef> enabled_features = {})
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        happy_eyeballs_v3_enabled_(happy_eyeballs_v3_enabled) {
    std::vector<base::test::FeatureRef> disabled_features;
    if (happy_eyeballs_v3_enabled_) {
      enabled_features.emplace_back(features::kHappyEyeballsV3);
    } else {
      disabled_features.emplace_back(features::kHappyEyeballsV3);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    FLAGS_quic_enable_http3_grease_randomness = false;
    CreateSessionDeps();
  }

  void DestroySession() {
    factory_ = nullptr;
    job_controller_ = nullptr;
    session_.reset();
  }

  // Creates / re-creates `session_deps_`, and clears test fixture fields
  // referencing it.
  void CreateSessionDeps() {
    DestroySession();

    session_deps_.proxy_resolution_service->SetProxyDelegate(nullptr);

    session_deps_ = SpdySessionDependencies(
        ConfiguredProxyResolutionService::CreateDirect());
    session_deps_.enable_quic = true;
    session_deps_.host_resolver->set_synchronous_mode(true);
    session_deps_.http_user_agent_settings =
        std::make_unique<StaticHttpUserAgentSettings>("*", "test-ua");
    if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
      auto host_resolver = std::make_unique<FakeServiceEndpointResolver>();
      // This configures the FakeServiceEndpointResolver in much the same way
      // SpdySessionDeps configures the default MockHostResolver.
      host_resolver->ConfigureDefaultResolution()
          .set_start_result(OK)
          .add_endpoint(ServiceEndpointBuilder().add_v4("127.0.2.1").endpoint())
          .set_crypto_ready(true);
      session_deps_.alternate_host_resolver = std::move(host_resolver);
    }
  }

  void SetPreconnect() {
    ASSERT_FALSE(session_deps_.proxy_delegate);
    is_preconnect_ = true;
  }

  void DisableIPBasedPoolingForH2() {
    ASSERT_FALSE(session_deps_.proxy_delegate);
    enable_ip_based_pooling_for_h2_ = false;
  }

  void SetNotDelayMainJobWithAvailableSpdySession() {
    ASSERT_FALSE(session_deps_.proxy_delegate);
    delay_main_job_with_available_spdy_session_ = false;
  }

  void SetEnableAlternativeServices(bool enable_alternative_services) {
    ASSERT_FALSE(session_deps_.proxy_delegate);
    enable_alternative_services_ = enable_alternative_services;
  }

  void SkipCreatingJobController() {
    ASSERT_FALSE(job_controller_);
    create_job_controller_ = false;
  }

  void Initialize(const HttpRequestInfo& request_info) {
    ASSERT_FALSE(session_deps_.proxy_delegate);
    session_deps_.proxy_delegate = std::make_unique<TestProxyDelegate>();

    if (quic_data_) {
      quic_data_->AddSocketDataToFactory(session_deps_.socket_factory.get());
    }
    if (quic_data2_) {
      quic_data2_->AddSocketDataToFactory(session_deps_.socket_factory.get());
    }
    if (tcp_data_) {
      session_deps_.socket_factory->AddSocketDataProvider(tcp_data_.get());
    }
    if (tcp_data2_) {
      session_deps_.socket_factory->AddSocketDataProvider(tcp_data2_.get());
    }

    session_deps_.proxy_resolution_service->SetProxyDelegate(
        session_deps_.proxy_delegate.get());

    session_deps_.net_log = NetLog::Get();
    HttpNetworkSessionParams params =
        SpdySessionDependencies::CreateSessionParams(&session_deps_);
    HttpNetworkSessionContext session_context =
        SpdySessionDependencies::CreateSessionContext(&session_deps_);

    session_context.quic_crypto_client_stream_factory =
        &crypto_client_stream_factory_;
    session_context.http_user_agent_settings = &http_user_agent_settings_;
    session_context.quic_context = &quic_context_;
    session_ = std::make_unique<HttpNetworkSession>(params, session_context);
    factory_ = session_->http_stream_factory();
    if (create_job_controller_) {
      request_delegate_ = std::make_unique<MockHttpStreamRequestDelegate>();
      CHECK(!request_delegate_->IsDone());
      auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
          factory_, request_delegate_.get(), session_.get(), &job_factory_,
          request_info, is_preconnect_, /*is_websocket=*/false,
          enable_ip_based_pooling_for_h2_, enable_alternative_services_,
          delay_main_job_with_available_spdy_session_,
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
      job_controller_ = job_controller.get();
      HttpStreamFactoryPeer::AddJobController(factory_,
                                              std::move(job_controller));
    }
  }

  HttpStreamFactoryJobControllerTestBase(
      const HttpStreamFactoryJobControllerTestBase&) = delete;
  HttpStreamFactoryJobControllerTestBase& operator=(
      const HttpStreamFactoryJobControllerTestBase&) = delete;

  ~HttpStreamFactoryJobControllerTestBase() override {
    if (should_check_data_consumed_) {
      if (quic_data_) {
        EXPECT_TRUE(quic_data_->AllReadDataConsumed());
        EXPECT_TRUE(quic_data_->AllWriteDataConsumed());
      }
      if (quic_data2_) {
        EXPECT_TRUE(quic_data2_->AllReadDataConsumed());
        EXPECT_TRUE(quic_data2_->AllWriteDataConsumed());
      }
      if (tcp_data_) {
        EXPECT_TRUE(tcp_data_->AllReadDataConsumed());
        EXPECT_TRUE(tcp_data_->AllWriteDataConsumed());
      }
      if (tcp_data2_) {
        EXPECT_TRUE(tcp_data2_->AllReadDataConsumed());
        EXPECT_TRUE(tcp_data2_->AllWriteDataConsumed());
      }
    }
  }

  void SetAlternativeService(const HttpRequestInfo& request_info,
                             AlternativeService alternative_service) {
    url::SchemeHostPort server(request_info.url);
    base::Time expiration = base::Time::Now() + base::Days(1);
    if (alternative_service.protocol == NextProto::kProtoQUIC) {
      session_->http_server_properties()->SetQuicAlternativeService(
          server, NetworkAnonymizationKey(), alternative_service, expiration,
          quic_context_.params()->supported_versions);
    } else {
      session_->http_server_properties()->SetHttp2AlternativeService(
          server, NetworkAnonymizationKey(), alternative_service, expiration);
    }
  }

  void VerifyBrokenAlternateProtocolMapping(const HttpRequestInfo& request_info,
                                            bool should_mark_broken) {
    const url::SchemeHostPort server(request_info.url);
    const AlternativeServiceInfoVector alternative_service_info_vector =
        session_->http_server_properties()->GetAlternativeServiceInfos(
            server, NetworkAnonymizationKey());
    EXPECT_EQ(1u, alternative_service_info_vector.size());
    EXPECT_EQ(should_mark_broken,
              session_->http_server_properties()->IsAlternativeServiceBroken(
                  alternative_service_info_vector[0].alternative_service(),
                  NetworkAnonymizationKey()));
  }

  void SetAsyncQuicSession(bool async_quic_session) {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};
    if (happy_eyeballs_v3_enabled_) {
      enabled_features.emplace_back(features::kHappyEyeballsV3);
    } else {
      disabled_features.emplace_back(features::kHappyEyeballsV3);
    }
    if (async_quic_session) {
      enabled_features.emplace_back(features::kAsyncQuicSession);
    } else {
      disabled_features.emplace_back(features::kAsyncQuicSession);
    }
    feature_list_.Reset();
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void TestAltJobSucceedsAfterMainJobFailed(
      bool alt_job_retried_on_non_default_network,
      bool async_quic_session);
  void TestMainJobSucceedsAfterAltJobFailed(
      bool alt_job_retried_on_non_default_network,
      bool async_quic_session);
  void TestMainJobSucceedsAfterIgnoredError(int net_error,
                                            bool async_quic_session,
                                            bool expect_broken = false,
                                            std::string alternate_host = "");
  void TestAltJobSucceedsAfterMainJobSucceeded(
      bool alt_job_retried_on_non_default_network,
      bool async_quic_session);
  void TestOnStreamFailedForBothJobs(
      bool alt_job_retried_on_non_default_network,
      bool async_quic_session);
  void TestAltJobFailsAfterMainJobSucceeded(
      bool alt_job_retried_on_non_default_network,
      bool async_quic_session);
  void TestMainJobSucceedsAfterAltJobSucceeded(
      bool alt_job_retried_on_non_default_network,
      bool async_quic_session);
  void TestMainJobFailsAfterAltJobSucceeded(
      bool alt_job_retried_on_non_default_network,
      bool async_quic_session);
  void TestAltSvcVersionSelection(
      const std::string& alt_svc_header,
      const quic::ParsedQuicVersion& expected_version,
      const quic::ParsedQuicVersionVector& supported_versions);
  void TestResumeMainJobWhenAltJobStalls(bool async_quic_session);
  void TestAltJobSucceedsMainJobDestroyed(bool async_quic_session);
  void TestOrphanedJobCompletesControllerDestroyed(bool async_quic_session);
  void TestDoNotDelayMainJobIfQuicWasRecentlyBroken(bool async_quic_session);
  void TestDelayMainJobAfterRecentlyBrokenQuicWasConfirmed(
      bool async_quic_session);
  void TestDoNotDelayMainJobIfHasAvailableSpdySession(bool async_quic_session);

  bool happy_eyeballs_v3_enabled() const { return happy_eyeballs_v3_enabled_; }

  // Convenience method to create a basic HttpRequestInfo.
  HttpRequestInfo CreateRequestInfo() {
    HttpRequestInfo request_info;
    request_info.url = GURL("https://a.test/");
    request_info.method = "GET";
    return request_info;
  }

  quic::ParsedQuicVersion version_ = DefaultSupportedQuicVersions().front();
  RecordingNetLogObserver net_log_observer_;
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLogSourceType::HTTP_STREAM_JOB_CONTROLLER)};
  TestJobFactory job_factory_;
  std::unique_ptr<MockHttpStreamRequestDelegate> request_delegate_{
      std::make_unique<MockHttpStreamRequestDelegate>()};
  MockQuicContext quic_context_;
  StaticHttpUserAgentSettings http_user_agent_settings_ = {"*", "test-ua"};
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> session_;
  raw_ptr<HttpStreamFactory> factory_ = nullptr;
  raw_ptr<HttpStreamFactory::JobController, AcrossTasksDanglingUntriaged>
      job_controller_ = nullptr;
  std::unique_ptr<HttpStreamRequest> request_;
  std::unique_ptr<SequencedSocketData> tcp_data_;
  std::unique_ptr<SequencedSocketData> tcp_data2_;
  std::unique_ptr<MockQuicData> quic_data_;
  std::unique_ptr<MockQuicData> quic_data2_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  QuicTestPacketMaker client_maker_{version_,
                                    quic::QuicUtils::CreateRandomConnectionId(
                                        quic_context_.random_generator()),
                                    quic_context_.clock(),
                                    kServerHostname,
                                    quic::Perspective::IS_CLIENT,
                                    false};

 protected:
  bool is_preconnect_ = false;
  bool enable_ip_based_pooling_for_h2_ = true;
  bool enable_alternative_services_ = true;
  bool delay_main_job_with_available_spdy_session_ = true;
  bool should_check_data_consumed_ = true;

 private:
  const bool happy_eyeballs_v3_enabled_;
  bool create_job_controller_ = true;

  base::test::ScopedFeatureList feature_list_;
};

class HttpStreamFactoryJobControllerTest
    : public HttpStreamFactoryJobControllerTestBase {
 protected:
  HttpStreamFactoryJobControllerTest()
      : HttpStreamFactoryJobControllerTestBase(
            /*happy_eyeballs_v3_enabled=*/false) {}
};

// Tests that are run with Happy Eyeballs v3 both enabled and disabled.
class HttpStreamFactoryJobControllerDualPathTest
    : public HttpStreamFactoryJobControllerTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  HttpStreamFactoryJobControllerDualPathTest()
      : HttpStreamFactoryJobControllerTestBase(
            /*happy_eyeballs_v3_enabled=*/GetParam()) {
    // Use real jobs to avoid mocking out the Resume() calls that block H1/H2
    // jobs on H3 jobs under certain circumstances, but only for the HEv1 path.
    job_factory_.set_use_real_jobs();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HttpStreamFactoryJobControllerDualPathTest,
                         ::testing::Bool());

// Make sure that a socket will not outlive the network session if the session
// is destroyed before the conneciton completes.
TEST_P(HttpStreamFactoryJobControllerDualPathTest,
       DestroyingSocketPoolDestroysSocket) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.test");
  Initialize(request_info);

  MockHttpStreamRequestDelegate request_delegate;
  auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
      factory_, request_delegate_.get(), session_.get(), &job_factory_,
      request_info, is_preconnect_, /*is_websocket=*/false,
      enable_ip_based_pooling_for_h2_, enable_alternative_services_,
      delay_main_job_with_available_spdy_session_,
      /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
  auto* job_controller_ptr = job_controller.get();
  HttpStreamFactoryPeer::AddJobController(factory_, std::move(job_controller));
  std::unique_ptr<HttpStreamRequest> request = job_controller_ptr->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(tcp_data_->socket());

  request.reset();
  DestroySession();
  // Destroying the session (and the request) should destroy the socket.
  EXPECT_FALSE(tcp_data_->socket());
}

// Tests that a synchronous preconnect completes (succeeded internally). Note
// that even if the preconnecting socket completes synchronously, both the HEv3
// and non-HEv3 paths use PostTask() so the callback is called asynchronously.
TEST_P(HttpStreamFactoryJobControllerDualPathTest, PreconnectSyncOk) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info = CreateRequestInfo();
  SetPreconnect();
  Initialize(request_info);

  base::RunLoop run_loop;
  job_controller_->Preconnect(
      /*num_streams=*/1, run_loop.QuitClosure());
  run_loop.Run();

  if (happy_eyeballs_v3_enabled()) {
    EXPECT_EQ(session_->http_stream_pool()->TotalIdleStreamCount(), 1u);
  } else {
    TransportClientSocketPool* socket_pool =
        reinterpret_cast<TransportClientSocketPool*>(session_->GetSocketPool(
            HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct()));
    EXPECT_EQ(socket_pool->IdleSocketCount(), 1u);
  }
}

// Tests that an asynchronous preconnect completes (succeeded internally).
TEST_P(HttpStreamFactoryJobControllerDualPathTest, PreconnectAsyncOk) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info = CreateRequestInfo();
  SetPreconnect();
  Initialize(request_info);

  base::RunLoop run_loop;
  job_controller_->Preconnect(
      /*num_streams=*/1, run_loop.QuitClosure());
  run_loop.Run();

  if (happy_eyeballs_v3_enabled()) {
    EXPECT_EQ(session_->http_stream_pool()->TotalIdleStreamCount(), 1u);
  } else {
    TransportClientSocketPool* socket_pool =
        reinterpret_cast<TransportClientSocketPool*>(session_->GetSocketPool(
            HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct()));
    EXPECT_EQ(socket_pool->IdleSocketCount(), 1u);
  }
}

// Tests that an asynchronous preconnect (failed internally).
TEST_P(HttpStreamFactoryJobControllerDualPathTest, PreconnectAsyncFail) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));

  HttpRequestInfo request_info = CreateRequestInfo();
  SetPreconnect();
  Initialize(request_info);

  base::RunLoop run_loop;
  job_controller_->Preconnect(
      /*num_streams=*/1, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that destroying session and pools doesn't invoke a preconnect callback.
TEST_P(HttpStreamFactoryJobControllerDualPathTest, PreconnectDestroySession) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info = CreateRequestInfo();
  SetPreconnect();
  Initialize(request_info);

  bool called = false;
  job_controller_->Preconnect(
      /*num_streams=*/1, base::BindLambdaForTesting([&]() { called = false; }));
  DestroySession();
  ASSERT_FALSE(called);
}

// Test the case of preconnecting to an origin with an alt service record, where
// establishing an H3 connection succeeds quickly, and so TCP should not be
// attempted.
TEST_P(HttpStreamFactoryJobControllerDualPathTest,
       PreconnectToHostWithValidAltSvc) {
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data_->AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));

  HttpRequestInfo request_info = CreateRequestInfo();
  SetPreconnect();
  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  base::RunLoop run_loop;
  job_controller_->Preconnect(1, base::OnceClosure());

  // Wait for preconnect to complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // There should be no H1/H2 connection.
  ClientSocketPool::GroupId group_id(server, PRIVACY_MODE_DISABLED,
                                     NetworkAnonymizationKey(),
                                     SecureDnsPolicy::kAllow,
                                     /*disable_cert_network_fetches=*/false);
  TransportClientSocketPool* socket_pool =
      reinterpret_cast<TransportClientSocketPool*>(session_->GetSocketPool(
          HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct()));
  EXPECT_FALSE(socket_pool->HasGroupForTesting(group_id));

  // There should be a QUIC session.
  QuicSessionKey session_key(
      HostPortPair::FromSchemeHostPort(server), PRIVACY_MODE_DISABLED,
      ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*require_dns_https_alpn=*/false,
      /*disable_cert_verification_network_fetches=*/false);
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  EXPECT_TRUE(quic_session_pool->FindExistingSession(session_key, server));
}

// Test the case of preconnecting to an origin with an alt service record, where
// establishing an H3 connection succeeds quickly, and so TCP should not be
// attempted. In this case, the origin is marked as HTTP/1.1 only, but that only
// applies to H2 connections, despite its name, so should have no effect.
TEST_P(HttpStreamFactoryJobControllerDualPathTest,
       PreconnectToHostWithValidAltSvcWithHttp11Only) {
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data_->AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));

  HttpRequestInfo request_info = CreateRequestInfo();
  SetPreconnect();
  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  session_->http_server_properties()->SetHTTP11Required(
      server, NetworkAnonymizationKey());

  base::RunLoop run_loop;
  job_controller_->Preconnect(1, base::OnceClosure());

  // Wait for preconnect to complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // There should be no H1/H2 connection.
  ClientSocketPool::GroupId group_id(server, PRIVACY_MODE_DISABLED,
                                     NetworkAnonymizationKey(),
                                     SecureDnsPolicy::kAllow,
                                     /*disable_cert_network_fetches=*/false);
  TransportClientSocketPool* socket_pool =
      reinterpret_cast<TransportClientSocketPool*>(session_->GetSocketPool(
          HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct()));
  EXPECT_FALSE(socket_pool->HasGroupForTesting(group_id));

  // There should be a QUIC session.
  QuicSessionKey session_key(
      HostPortPair::FromSchemeHostPort(server), PRIVACY_MODE_DISABLED,
      ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*require_dns_https_alpn=*/false,
      /*disable_cert_verification_network_fetches=*/false);
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  EXPECT_TRUE(quic_session_pool->FindExistingSession(session_key, server));
}

// Check the case where the attempt to connect to the alt service destination
// succeeds. The destination is marked as HTTP/1.1 only, but that should not
// affect H3 connections, only H2 ones.
TEST_P(HttpStreamFactoryJobControllerDualPathTest,
       AltSvcAttemptSucceedsWithHttp11Only) {
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data_->AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));

  // In the HEv3 case, UDP and TCP connections are made at the same time, so
  // need to create mock TCP connections. In the non-HEv3 case, UDP connections
  // are made immediately, and then post a task to complete connection
  // establishment, and the task to start the TCP job is always posted
  // asynchronously. As a result, the main job is then never started.
  if (happy_eyeballs_v3_enabled()) {
    tcp_data_ = std::make_unique<SequencedSocketData>();
    tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  }

  HttpRequestInfo request_info = CreateRequestInfo();
  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  session_->http_server_properties()->SetHTTP11Required(
      server, NetworkAnonymizationKey());

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  // Wait for the request to succeed.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());

  // There should be no H1/H2 connection.
  ClientSocketPool::GroupId group_id(server, PRIVACY_MODE_DISABLED,
                                     NetworkAnonymizationKey(),
                                     SecureDnsPolicy::kAllow,
                                     /*disable_cert_network_fetches=*/false);
  TransportClientSocketPool* socket_pool =
      reinterpret_cast<TransportClientSocketPool*>(session_->GetSocketPool(
          HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct()));
  EXPECT_FALSE(socket_pool->HasGroupForTesting(group_id));

  // There should be a QUIC session.
  QuicSessionKey session_key(
      HostPortPair::FromSchemeHostPort(server), PRIVACY_MODE_DISABLED,
      ProxyChain::Direct(), SessionUsage::kDestination, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*require_dns_https_alpn=*/false,
      /*disable_cert_verification_network_fetches=*/false);
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  EXPECT_TRUE(quic_session_pool->FindExistingSession(session_key, server));
}

// Test the cases where there are no valid ALPNs to use.
TEST_P(HttpStreamFactoryJobControllerDualPathTest, NoValidAlpns) {
  // Reasons to not be able to use H2/H3. The only reason for HTTP/1.1 is an
  // UploadDataStream that doesn't allow HTTP/1, due to not knowing its length.
  enum class NoH2Reason {
    kDisabledGlobally,
    kRequiresHttp11,
  };
  enum class NoH3Reason {
    kDisabledGlobally,
    kNoAltService,
  };
  for (const auto no_h2_reason :
       {NoH2Reason::kDisabledGlobally, NoH2Reason::kRequiresHttp11}) {
    for (const auto no_h3_reason :
         {NoH3Reason::kDisabledGlobally, NoH3Reason::kNoAltService}) {
      CreateSessionDeps();
      SetEnableAlternativeServices(true);

      tcp_data_ = std::make_unique<SequencedSocketData>();
      tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
      SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
      // Despite H1 and H2 both being banned, only H1 being disallowed is
      // reflected in the next proto vector sent to the server. The main reason
      // for this is that H2 may only be disallowed on a per-request basis, due
      // to a chunked upload, while both reasons for disallowing H2 apply
      // globally.
      ssl_data.next_protos_expected_in_ssl_config =
          NextProtoVector{NextProto::kProtoHTTP11};
      session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

      // Can't use switch for H2, because `enable_http2` must be set before
      // creating an HttpNetworkSession, but SetHTTP11Required() ust be called
      // after.
      if (no_h2_reason == NoH2Reason::kDisabledGlobally) {
        session_deps_.enable_http2 = false;
      }

      switch (no_h3_reason) {
        case NoH3Reason::kDisabledGlobally:
          session_deps_.enable_quic = false;
          break;
        case NoH3Reason::kNoAltService:
          SetEnableAlternativeServices(false);
          break;
      }

      UploadDataStreamNotAllowHTTP1 upload("Upload data that does not matter.");
      HttpRequestInfo request_info;
      request_info.method = "GET";
      request_info.url = GURL("https://www.google.com");
      request_info.upload_data_stream = &upload;

      Initialize(request_info);
      AlternativeService alternative_service(NextProto::kProtoQUIC,
                                             request_info.url.host(), 443);
      SetAlternativeService(request_info, alternative_service);

      if (no_h2_reason == NoH2Reason::kRequiresHttp11) {
        session_->http_server_properties()->SetHTTP11Required(
            url::SchemeHostPort(request_info.url), NetworkAnonymizationKey());
      }

      auto request = job_controller_->Start(
          request_delegate_.get(), nullptr, net_log_with_source_,
          HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
      EXPECT_EQ(request_delegate_->WaitForError(), ERR_ALPN_NEGOTIATION_FAILED);
      EXPECT_TRUE(ssl_data.ConnectDataConsumed());
    }
  }
}

// Check the case where a QUIC alt service request has the same destination as a
// `enable_alternative_services=false` request. The latter request should not
// get a QUIC session.
TEST_P(HttpStreamFactoryJobControllerDualPathTest,
       AltServiceHasSameDestinationAsNoQuicRequest) {
  if (happy_eyeballs_v3_enabled()) {
    GTEST_SKIP()
        << "This test currently CHECKs in HEv3 mode, due to merging QUIC-only "
           "and anything-but-quic ALPN lists, to get an empty list.";
  }

  // The alt-service URL for the initial request, and the destination URL for
  // the second request.
  const GURL alt_service_url("https://alt.a.test");

  // Use COLD_START to stall alt job's QUIC connection attempt, which never
  // connects.
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  // The TCP connection attempt of the initial request. It fails to connect. Use
  // a connect completer so can wait until the connection has been attempted
  // before starting the second request, to get a consistent TCP connection
  // order.
  MockConnectCompleter connect_completer1;
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(&connect_completer1));

  // The TCP connection to `alt_service_url`, for the second request.
  tcp_data2_ = std::make_unique<SequencedSocketData>();
  tcp_data2_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data2(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data2);

  // First request.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://a.test");

  Initialize(request_info);
  // Set up the alt service. Must be done after the Initialize() call.
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         alt_service_url.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // Start and run the first request. Its TCP connection attempt fails. It hangs
  // waiting on its alt service connection attempt.
  auto request = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  connect_completer1.WaitForConnectAndComplete(ERR_FAILED);

  // Start the second request to `alt_service_url` directly. The important part
  // is the `enable_alternative_services=false`.
  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = alt_service_url;
  MockHttpStreamRequestDelegate request_delegate2;
  auto owned_job_controller2 =
      std::make_unique<HttpStreamFactory::JobController>(
          factory_, &request_delegate2, session_.get(), &job_factory_,
          request_info2, is_preconnect_, /*is_websocket=*/false,
          enable_ip_based_pooling_for_h2_,
          /*enable_alternative_services=*/false,
          delay_main_job_with_available_spdy_session_,
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
  auto job_controller2 = owned_job_controller2.get();
  HttpStreamFactoryPeer::AddJobController(factory_,
                                          std::move(owned_job_controller2));
  auto request2 =
      job_controller2->Start(&request_delegate2, nullptr, net_log_with_source_,
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  // The second request succeeds, and gets a TCP/IP connection.
  auto stream2 = request_delegate2.WaitForHttpStream();
  EXPECT_TRUE(stream2);
  EXPECT_FALSE(stream2->GetQuicConnectionDetails());

  // The other request is still waiting on the QUIC connection attempt.
  EXPECT_FALSE(request_delegate_->IsDone());
}

TEST_F(HttpStreamFactoryJobControllerTest, ProxyResolutionFailsSync) {
  ProxyConfig proxy_config;
  proxy_config.set_pac_url(GURL("http://fooproxyurl"));
  proxy_config.set_pac_mandatory(true);
  session_deps_.proxy_resolution_service =
      std::make_unique<ConfiguredProxyResolutionService>(
          std::make_unique<ProxyConfigServiceFixed>(ProxyConfigWithAnnotation(
              proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS)),
          std::make_unique<FailingProxyResolverFactory>(),
          /*host_resolver_for_override_rules=*/nullptr, /*net_log=*/nullptr,
          /*quick_check_enabled=*/true);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  // Make sure calling GetLoadState() when before job creation does not crash.
  // Regression test for crbug.com/723920.
  EXPECT_EQ(LOAD_STATE_IDLE, job_controller_->GetLoadState());

  EXPECT_EQ(request_delegate_->WaitForError(),
            ERR_MANDATORY_PROXY_CONFIGURATION_FAILED);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerTest, ProxyResolutionFailsAsync) {
  ProxyConfig proxy_config;
  proxy_config.set_pac_url(GURL("http://fooproxyurl"));
  proxy_config.set_pac_mandatory(true);
  auto proxy_resolver_factory =
      std::make_unique<MockAsyncProxyResolverFactory>(false);
  auto* proxy_resolver_factory_ptr = proxy_resolver_factory.get();
  MockAsyncProxyResolver resolver;
  session_deps_.proxy_resolution_service =
      std::make_unique<ConfiguredProxyResolutionService>(
          std::make_unique<ProxyConfigServiceFixed>(ProxyConfigWithAnnotation(
              proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS)),
          std::move(proxy_resolver_factory),
          /*host_resolver_for_override_rules=*/nullptr, /*net_log=*/nullptr,
          /*quick_check_enabled=*/true);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL,
            job_controller_->GetLoadState());

  proxy_resolver_factory_ptr->pending_requests()[0]->CompleteNowWithForwarder(
      ERR_FAILED, &resolver);
  EXPECT_EQ(request_delegate_->WaitForError(),
            ERR_MANDATORY_PROXY_CONFIGURATION_FAILED);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerTest, NoSupportedProxies) {
  session_deps_.proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                             "myproxy.org", 443)},
          TRAFFIC_ANNOTATION_FOR_TESTS);
  session_deps_.enable_quic = false;
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  EXPECT_EQ(request_delegate_->WaitForError(), ERR_NO_SUPPORTED_PROXIES);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// TODO(crbug.com/365771838): Add tests for non-ip protection nested proxy
// chains if support is enabled for all builds.
class JobControllerReconsiderProxyAfterErrorTest
    : public HttpStreamFactoryJobControllerTestBase {
 public:
  JobControllerReconsiderProxyAfterErrorTest()
      : HttpStreamFactoryJobControllerTestBase(
            /*happy_eyeballs_v3_enabled=*/false) {}
  void Initialize(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service,
      std::unique_ptr<ProxyDelegate> proxy_delegate = nullptr,
      bool using_quic = false) {
    session_deps_.proxy_delegate = std::move(proxy_delegate);
    session_deps_.proxy_resolution_service =
        std::move(proxy_resolution_service);
    session_deps_.proxy_resolution_service->SetProxyDelegate(
        session_deps_.proxy_delegate.get());
    session_deps_.http_user_agent_settings =
        std::make_unique<StaticHttpUserAgentSettings>("*", "test-ua");
    HttpNetworkSessionParams params =
        SpdySessionDependencies::CreateSessionParams(&session_deps_);
    HttpNetworkSessionContext session_context =
        SpdySessionDependencies::CreateSessionContext(&session_deps_);
    if (using_quic) {
      params.enable_quic = true;
      session_context.quic_crypto_client_stream_factory =
          &crypto_client_stream_factory_;
      session_context.quic_context = &quic_context_;
      session_context.quic_context->params()->origins_to_force_quic_on.insert(
          url::SchemeHostPort(GURL("https://www.example.com")));
    }
    session_ = std::make_unique<HttpNetworkSession>(params, session_context);
    factory_ = session_->http_stream_factory();
  }

  std::unique_ptr<MockQuicChromiumClientSession> CreateMockQUICProxySession(
      url::SchemeHostPort server) {
    const IPEndPoint kIpEndPoint = IPEndPoint(IPAddress::IPv4AllZeros(), 0);
    quic::test::MockRandom random{0};
    quic::MockClock clock;
    QuicChromiumConnectionHelper helper(&clock, &random);
    quic::test::MockAlarmFactory alarm_factory;
    quic::test::MockConnectionIdGenerator connection_id_generator;
    TransportSecurityState transport_security_state;
    SSLConfigServiceDefaults ssl_config_service;
    quic::QuicCryptoClientConfig crypto_config(
        quic::test::crypto_test_utils::ProofVerifierForTesting());
    quic::QuicConfig quic_config(quic::test::DefaultQuicConfig());

    std::unique_ptr<DatagramClientSocket> socket =
        session_deps_.socket_factory->CreateDatagramClientSocket(
            DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
    socket->Connect(kIpEndPoint);
    quic::test::MockQuicConnection* connection =
        new quic::test::MockQuicConnection(&helper, &alarm_factory,
                                           quic::Perspective::IS_CLIENT);
    EXPECT_CALL(*connection,
                CloseConnection(quic::QUIC_PEER_GOING_AWAY, "session torn down",
                                quic::ConnectionCloseBehavior::SILENT_CLOSE))
        .Times(1);

    QuicSessionKey session_key(
        server.host(), server.port(), PRIVACY_MODE_DISABLED,
        ProxyChain::ForIpProtection({}, 0), SessionUsage::kProxy, SocketTag(),
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*require_dns_https_alpn=*/false,
        /*disable_cert_verification_network_fetches=*/true);
    auto new_session = std::make_unique<MockQuicChromiumClientSession>(
        connection, std::move(socket), session_->quic_session_pool(),
        &crypto_client_stream_factory_, &clock, &transport_security_state,
        &ssl_config_service,
        base::WrapUnique(static_cast<QuicServerInfo*>(nullptr)),
        QuicSessionAliasKey(server, session_key),
        /*require_confirmation=*/false,
        /*migrate_session_early_v2=*/false,
        /*migrate_session_on_network_change_v2=*/false, kDefaultNetworkForTests,
        quic::QuicTime::Delta::FromMilliseconds(
            kDefaultRetransmittableOnWireTimeout.InMilliseconds()),
        /*migrate_idle_session=*/false, /*allow_port_migration_=*/false,
        kDefaultIdleSessionMigrationPeriod,
        /*multi_port_probing_interval=*/0, kMaxTimeOnNonDefaultNetwork,
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        /*cert_verify_flags=*/0, quic_config,
        std::make_unique<TestQuicCryptoClientConfigHandle>(&crypto_config),
        "CONNECTION_UNKNOWN", base::TimeTicks::Now(), base::TimeTicks::Now(),
        base::DefaultTickClock::GetInstance(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*socket_performance_watcher=*/nullptr, ConnectionEndpointMetadata(),
        /*enable_origin_frame=*/true,
        /*allow_server_preferred_address=*/true,
        MultiplexedSessionCreationInitiator::kUnknown,
        NetLogWithSource::Make(NetLogSourceType::NONE));
    mock_proxy_sessions_.emplace_back(new_session.get());

    quic::test::NoopQpackStreamSenderDelegate
        noop_qpack_stream_sender_delegate_;
    mock_proxy_sessions_.back()->Initialize();
    mock_proxy_sessions_.back()
        ->qpack_decoder()
        ->set_qpack_stream_sender_delegate(&noop_qpack_stream_sender_delegate_);
    mock_proxy_sessions_.back()->StartReading();

    return new_session;
  }

  std::unique_ptr<HttpStreamRequest> CreateJobController(
      const HttpRequestInfo& request_info) {
    // Create a new delegate unconditionally, as some tests call this helper
    // more than once.
    request_delegate_ = std::make_unique<MockHttpStreamRequestDelegate>();

    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, request_delegate_.get(), session_.get(),
        &default_job_factory_, request_info, is_preconnect_,
        /*is_websocket=*/false, enable_ip_based_pooling_for_h2_,
        enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    auto* job_controller_ptr = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
    return job_controller_ptr->Start(
        request_delegate_.get(), nullptr, net_log_with_source_,
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  }

 protected:
  std::vector<raw_ptr<MockQuicChromiumClientSession>> mock_proxy_sessions_;

 private:
  // Use real Jobs so that Job::Resume() is not mocked out. When main job is
  // resumed it will use mock socket data.
  HttpStreamFactory::JobFactory default_job_factory_;
};

// Test case struct for proxies that use TCP i.e. HTTP, HTTPS, and SOCKS5.
struct TcpProxyTestCase {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, TcpProxyTestCase test_case) {
    sink.Append(testing::PrintToString(test_case.phase));
    sink.Append("_");
    sink.Append(ErrorToShortString(test_case.error));
  }

  // Each test case simulates a connection attempt through a proxy that fails
  // twice, followed by two connection attempts that succeed. For most cases,
  // this is done by having a connection attempt to the first proxy fail,
  // triggering fallback to a second proxy, which also fails, and then fallback
  // to the final (DIRECT) proxy option. However, SslConnectJobs have their own
  // try logic in certain cases. This value is true for those cases, in which
  // case there are two connection attempts to the first proxy, and then the
  // requests fall back to the second (DIRECT) proxy.
  //
  // This only applies to HTTPS tests.
  bool TriggersSslConnectJobRetryLogic() const {
    if (phase == TcpErrorPhase::kProxySslHandshake &&
        error == ERR_SSL_PROTOCOL_ERROR) {
      return true;
    }
    return false;
  }

  TcpErrorPhase phase;
  Error error;
};

std::string PrintHttpProxyTestName(
    const testing::TestParamInfo<
        std::tuple<TcpProxyTestCase, std::string_view>>& info) {
  return ::testing::PrintToString(std::get<0>(info.param)) + "_" +
         GURL(std::get<1>(info.param)).GetScheme();
}

constexpr TcpProxyTestCase kHttpProxyTestCases[] = {
    // These largely correspond to the list of errors in
    // CanFalloverToNextProxy() which can occur with an HTTP proxy.
    //
    // We omit `ERR_CONNECTION_CLOSED` because it is largely unreachable. The
    // HTTP/1.1 parser maps it to `ERR_EMPTY_RESPONSE` or
    // `ERR_RESPONSE_HEADERS_TRUNCATED` in most cases.
    //
    // TODO(davidben): Is omitting `ERR_EMPTY_RESPONSE` a bug in proxy error
    // handling?
    {TcpErrorPhase::kHostResolution, ERR_NAME_NOT_RESOLVED},
    {TcpErrorPhase::kTcpConnect, ERR_ADDRESS_UNREACHABLE},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_TIMED_OUT},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_RESET},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_ABORTED},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_REFUSED},
    {TcpErrorPhase::kTunnelRead, ERR_TIMED_OUT},
    {TcpErrorPhase::kTunnelRead, ERR_SSL_PROTOCOL_ERROR},
};

class JobControllerReconsiderProxyAfterErrorHttpProxyTest
    : public JobControllerReconsiderProxyAfterErrorTest,
      public ::testing::WithParamInterface<
          std::tuple<TcpProxyTestCase, std::string_view>> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    JobControllerReconsiderProxyAfterErrorHttpProxyTest,
    ::testing::Combine(::testing::ValuesIn(kHttpProxyTestCases),
                       ::testing::ValuesIn(kHttpTestUrls)),
    &PrintHttpProxyTestName);

// Test proxy fallback logic in the case connecting through an HTTP proxy.
//
// TODO(eroman): The testing should be expanded to test cases where proxy
//               fallback is NOT supposed to occur.
TEST_P(JobControllerReconsiderProxyAfterErrorHttpProxyTest, Test) {
  const auto [phase, error] = std::get<0>(GetParam());
  const GURL dest_url(std::get<1>(GetParam()));

  CreateSessionDeps();
  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "PROXY badproxy:99; PROXY badfallbackproxy:98; DIRECT",
          TRAFFIC_ANNOTATION_FOR_TESTS);
  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();
  test_proxy_delegate->set_extra_header_name("Foo");

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  static constexpr char kBadProxyTunnelRequest[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: badproxy:99\r\n\r\n";
  static constexpr char kBadFallbackProxyTunnelRequest[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: badfallbackproxy:98\r\n\r\n";
  const MockWrite kBadProxyTunnelWrites[] = {{ASYNC, kBadProxyTunnelRequest}};
  const MockWrite kBadFallbackProxyTunnelWrites[] = {
      {ASYNC, kBadFallbackProxyTunnelRequest}};
  std::vector<MockRead> reads;

  // Generate identical errors for both the main proxy and the fallback proxy.
  // No alternative job is created for either, so only need one data provider
  // for each, when the request makes it to the socket layer.
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job;
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job2;
  switch (phase) {
    case TcpErrorPhase::kProxySslHandshake:
      NOTREACHED() << "No SSL handshake for HTTP connections.";
    case TcpErrorPhase::kHostResolution:
      // Only ERR_NAME_NOT_RESOLVED can be returned by the mock host resolver.
      DCHECK_EQ(ERR_NAME_NOT_RESOLVED, error);
      session_deps_.host_resolver->rules()->AddSimulatedFailure("badproxy");
      session_deps_.host_resolver->rules()->AddSimulatedFailure(
          "badfallbackproxy");
      break;
    case TcpErrorPhase::kTcpConnect:
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>();
      socket_data_proxy_main_job->set_connect_data(MockConnect(ASYNC, error));
      socket_data_proxy_main_job2 =
          std::make_unique<StaticSocketDataProvider>();
      socket_data_proxy_main_job2->set_connect_data(MockConnect(ASYNC, error));
      break;
    case TcpErrorPhase::kTunnelRead:
      if (dest_url.SchemeIs(url::kHttpScheme)) {
        GTEST_SKIP() << "Tunnels aren't established for HTTP destinations.";
      }
      reads.emplace_back(ASYNC, error);
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>(
          reads, kBadProxyTunnelWrites);
      socket_data_proxy_main_job2 = std::make_unique<StaticSocketDataProvider>(
          reads, kBadFallbackProxyTunnelWrites);
      break;
  }

  if (socket_data_proxy_main_job) {
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job.get());
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job2.get());
  }

  // After both proxies fail, the request should fall back to using DIRECT, and
  // succeed.
  SSLSocketDataProvider ssl_data_first_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);
  // Only used in the HTTPS destination case, but harmless in the HTTP case.
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_first_request);

  // Second request should use DIRECT, skipping the bad proxies, and succeed.
  SSLSocketDataProvider ssl_data_second_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_second_request;
  socket_data_direct_second_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_second_request);
  // Only used in the HTTPS destination case, but harmless in the HTTP case.
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_second_request);

  // Now request a stream. It should succeed using the DIRECT fallback proxy
  // option.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = dest_url;
  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate));

  // Start two requests. The first request should consume data from
  // `socket_data_proxy_main_job` and `socket_data_direct_first_request`.
  // The second request should consume data from
  // `socket_data_direct_second_request`.

  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<HttpStreamRequest> request =
        CreateJobController(request_info);
    ASSERT_TRUE(request_delegate_->WaitForHttpStream());

    // Verify that request was fetched without proxy.
    EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());

    // The proxies that failed should now be known to the proxy service as bad.
    const ProxyRetryInfoMap& retry_info =
        session_->proxy_resolution_service()->proxy_retry_info();
    ASSERT_THAT(retry_info, SizeIs(2));
    EXPECT_THAT(retry_info, Contains(Key(ProxyUriToProxyChain(
                                "badproxy:99", ProxyServer::SCHEME_HTTP))));
    EXPECT_THAT(retry_info,
                Contains(Key(ProxyUriToProxyChain("badfallbackproxy:98",
                                                  ProxyServer::SCHEME_HTTP))));

    // The idle socket should have been added back to the socket pool. Close it,
    // so the next loop iteration creates a new socket instead of reusing the
    // idle one.
    auto* socket_pool = session_->GetSocketPool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());
    EXPECT_EQ(1, socket_pool->IdleSocketCount());
    socket_pool->CloseIdleSockets("Close socket reason");
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

using JobControllerReconsiderProxyAfterErrorHttpsProxyTest =
    JobControllerReconsiderProxyAfterErrorHttpProxyTest;

constexpr TcpProxyTestCase kHttpsProxyTestCases[] = {
    // These largely correspond to the list of errors in
    // CanFalloverToNextProxy() which can occur with an HTTPS proxy.
    //
    // We omit `ERR_CONNECTION_CLOSED` because it is largely unreachable. The
    // HTTP/1.1 parser maps it to `ERR_EMPTY_RESPONSE` or
    // `ERR_RESPONSE_HEADERS_TRUNCATED` in most cases.
    //
    // TODO(davidben): Is omitting `ERR_EMPTY_RESPONSE` a bug in proxy error
    // handling?
    {TcpErrorPhase::kHostResolution, ERR_NAME_NOT_RESOLVED},
    {TcpErrorPhase::kTcpConnect, ERR_ADDRESS_UNREACHABLE},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_TIMED_OUT},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_RESET},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_ABORTED},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_REFUSED},
    {TcpErrorPhase::kProxySslHandshake, ERR_CERT_COMMON_NAME_INVALID},
    {TcpErrorPhase::kProxySslHandshake, ERR_SSL_PROTOCOL_ERROR},
    {TcpErrorPhase::kTunnelRead, ERR_TIMED_OUT},
    {TcpErrorPhase::kTunnelRead, ERR_SSL_PROTOCOL_ERROR},
};

INSTANTIATE_TEST_SUITE_P(
    ,
    JobControllerReconsiderProxyAfterErrorHttpsProxyTest,
    ::testing::Combine(::testing::ValuesIn(kHttpsProxyTestCases),
                       ::testing::ValuesIn(kHttpTestUrls)),
    &PrintHttpProxyTestName);

// Test proxy fallback logic in the case connecting through an HTTPS proxy.
TEST_P(JobControllerReconsiderProxyAfterErrorHttpsProxyTest, Test) {
  const auto [phase, error] = std::get<0>(GetParam());
  bool triggers_ssl_connect_job_retry_logic =
      std::get<0>(GetParam()).TriggersSslConnectJobRetryLogic();
  const GURL dest_url(std::get<1>(GetParam()));

  CreateSessionDeps();

  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS badproxy:99; HTTPS badfallbackproxy:98; DIRECT",
          TRAFFIC_ANNOTATION_FOR_TESTS);
  if (triggers_ssl_connect_job_retry_logic) {
    proxy_resolution_service =
        ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
            "HTTPS badproxy:99; DIRECT", TRAFFIC_ANNOTATION_FOR_TESTS);
  }
  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();
  test_proxy_delegate->set_extra_header_name("Foo");

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  static constexpr char kBadProxyTunnelRequest[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://badproxy:99\r\n\r\n";
  static constexpr char kBadFallbackProxyTunnelRequest[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://badfallbackproxy:98\r\n\r\n";
  const MockWrite kBadProxyTunnelWrites[] = {{ASYNC, kBadProxyTunnelRequest}};
  const MockWrite kBadFallbackProxyTunnelWrites[] = {
      {ASYNC, kBadFallbackProxyTunnelRequest}};
  std::vector<MockRead> reads;

  // Generate identical errors for both the main proxy and the fallback proxy.
  // No alternative job is created for either, so only need one data provider
  // for each, when the request makes it to the socket layer.
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job;
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job2;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job2;
  switch (phase) {
    case TcpErrorPhase::kHostResolution:
      // Only ERR_NAME_NOT_RESOLVED can be returned by the mock host resolver.
      DCHECK_EQ(ERR_NAME_NOT_RESOLVED, error);
      session_deps_.host_resolver->rules()->AddSimulatedFailure("badproxy");
      session_deps_.host_resolver->rules()->AddSimulatedFailure(
          "badfallbackproxy");
      break;
    case TcpErrorPhase::kTcpConnect:
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>();
      socket_data_proxy_main_job->set_connect_data(MockConnect(ASYNC, error));
      socket_data_proxy_main_job2 =
          std::make_unique<StaticSocketDataProvider>();
      socket_data_proxy_main_job2->set_connect_data(MockConnect(ASYNC, error));
      break;
    case TcpErrorPhase::kProxySslHandshake:
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>();
      ssl_data_proxy_main_job =
          std::make_unique<SSLSocketDataProvider>(ASYNC, error);
      socket_data_proxy_main_job2 =
          std::make_unique<StaticSocketDataProvider>();
      ssl_data_proxy_main_job2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, error);
      break;
    case TcpErrorPhase::kTunnelRead:
      if (dest_url.SchemeIs(url::kHttpScheme)) {
        GTEST_SKIP() << "Tunnels aren't established for HTTP destinations.";
      }
      reads.emplace_back(ASYNC, error);
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>(
          reads, kBadProxyTunnelWrites);
      ssl_data_proxy_main_job =
          std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      socket_data_proxy_main_job2 = std::make_unique<StaticSocketDataProvider>(
          reads, triggers_ssl_connect_job_retry_logic
                     ? kBadProxyTunnelWrites
                     : kBadFallbackProxyTunnelWrites);
      ssl_data_proxy_main_job2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      break;
  }

  if (socket_data_proxy_main_job) {
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job.get());
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job2.get());
  }
  if (ssl_data_proxy_main_job) {
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        ssl_data_proxy_main_job.get());
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        ssl_data_proxy_main_job2.get());
  }

  // After both proxies fail, the request should fall back to using DIRECT, and
  // succeed.
  SSLSocketDataProvider ssl_data_first_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);
  // Only used in the HTTPS destination case, but harmless in the HTTP case.
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_first_request);

  // Second request should use DIRECT, skipping the bad proxies, and succeed.
  SSLSocketDataProvider ssl_data_second_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_second_request;
  socket_data_direct_second_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_second_request);
  // Only used in the HTTPS destination case, but harmless in the HTTP case.
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_second_request);

  // Now request a stream. It should succeed using the DIRECT fallback proxy
  // option.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = dest_url;

  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate));

  // Start two requests. The first request should consume data from
  // `socket_data_proxy_main_job` and `socket_data_direct_first_request`.
  // The second request should consume data from
  // `socket_data_direct_second_request`.

  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<HttpStreamRequest> request =
        CreateJobController(request_info);
    ASSERT_TRUE(request_delegate_->WaitForHttpStream());

    // Verify that request was fetched without proxy.
    EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());

    // The proxies that failed should now be known to the proxy service as bad.
    const ProxyRetryInfoMap& retry_info =
        session_->proxy_resolution_service()->proxy_retry_info();
    if (!triggers_ssl_connect_job_retry_logic) {
      ASSERT_THAT(retry_info, SizeIs(2));
      EXPECT_THAT(retry_info,
                  Contains(Key(ProxyUriToProxyChain(
                      "https://badproxy:99", ProxyServer::SCHEME_HTTP))));
      EXPECT_THAT(
          retry_info,
          Contains(Key(ProxyUriToProxyChain("https://badfallbackproxy:98",
                                            ProxyServer::SCHEME_HTTP))));
    } else {
      ASSERT_THAT(retry_info, SizeIs(1));
      EXPECT_THAT(retry_info,
                  Contains(Key(ProxyUriToProxyChain(
                      "https://badproxy:99", ProxyServer::SCHEME_HTTP))));
    }

    // The idle socket should have been added back to the socket pool. Close it,
    // so the next loop iteration creates a new socket instead of reusing the
    // idle one.
    auto* socket_pool = session_->GetSocketPool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());
    EXPECT_EQ(1, socket_pool->IdleSocketCount());
    socket_pool->CloseIdleSockets("Close socket reason");
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Same as JobControllerReconsiderProxyAfterErrorHttpsProxyTest.Test, but this
// confirms that HttpStreamFactory::Job::ReconsiderProxyAfterError is correctly
// passing ProxyDelegate to CanFalloverToNextProxy. This is achieved by checking
// TestProxyDelegate::can_fallover_to_next_proxy_override_count.
TEST_P(JobControllerReconsiderProxyAfterErrorHttpsProxyTest,
       TestCanFalloverToNextProxyReceivesProxyDelegate) {
  const auto [phase, error] = std::get<0>(GetParam());
  bool triggers_ssl_connect_job_retry_logic =
      std::get<0>(GetParam()).TriggersSslConnectJobRetryLogic();
  const GURL dest_url(std::get<1>(GetParam()));

  CreateSessionDeps();

  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS badproxy:99; HTTPS badfallbackproxy:98; DIRECT",
          TRAFFIC_ANNOTATION_FOR_TESTS);
  if (triggers_ssl_connect_job_retry_logic) {
    proxy_resolution_service =
        ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
            "HTTPS badproxy:99; DIRECT", TRAFFIC_ANNOTATION_FOR_TESTS);
  }
  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();
  test_proxy_delegate->set_extra_header_name("Foo");

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  static constexpr char kBadProxyTunnelRequest[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://badproxy:99\r\n\r\n";
  static constexpr char kBadFallbackProxyTunnelRequest[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://badfallbackproxy:98\r\n\r\n";
  const MockWrite kBadProxyTunnelWrites[] = {{ASYNC, kBadProxyTunnelRequest}};
  const MockWrite kBadFallbackProxyTunnelWrites[] = {
      {ASYNC, kBadFallbackProxyTunnelRequest}};
  std::vector<MockRead> reads;

  // Generate identical errors for both the main proxy and the fallback proxy.
  // No alternative job is created for either, so only need one data provider
  // for each, when the request makes it to the socket layer.
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job;
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job2;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job2;
  switch (phase) {
    case TcpErrorPhase::kHostResolution:
      // Only ERR_NAME_NOT_RESOLVED can be returned by the mock host resolver.
      DCHECK_EQ(ERR_NAME_NOT_RESOLVED, error);
      session_deps_.host_resolver->rules()->AddSimulatedFailure("badproxy");
      session_deps_.host_resolver->rules()->AddSimulatedFailure(
          "badfallbackproxy");
      break;
    case TcpErrorPhase::kTcpConnect:
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>();
      socket_data_proxy_main_job->set_connect_data(MockConnect(ASYNC, error));
      socket_data_proxy_main_job2 =
          std::make_unique<StaticSocketDataProvider>();
      socket_data_proxy_main_job2->set_connect_data(MockConnect(ASYNC, error));
      break;
    case TcpErrorPhase::kProxySslHandshake:
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>();
      ssl_data_proxy_main_job =
          std::make_unique<SSLSocketDataProvider>(ASYNC, error);
      socket_data_proxy_main_job2 =
          std::make_unique<StaticSocketDataProvider>();
      ssl_data_proxy_main_job2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, error);
      break;
    case TcpErrorPhase::kTunnelRead:
      if (dest_url.SchemeIs(url::kHttpScheme)) {
        GTEST_SKIP() << "Tunnels aren't established for HTTP destinations.";
      }
      reads.emplace_back(ASYNC, error);
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>(
          reads, kBadProxyTunnelWrites);
      ssl_data_proxy_main_job =
          std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      socket_data_proxy_main_job2 = std::make_unique<StaticSocketDataProvider>(
          reads, triggers_ssl_connect_job_retry_logic
                     ? kBadProxyTunnelWrites
                     : kBadFallbackProxyTunnelWrites);
      ssl_data_proxy_main_job2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      break;
  }

  if (socket_data_proxy_main_job) {
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job.get());
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job2.get());
  }
  if (ssl_data_proxy_main_job) {
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        ssl_data_proxy_main_job.get());
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        ssl_data_proxy_main_job2.get());
  }

  // After both proxies fail, the request should fall back to using DIRECT, and
  // succeed.
  SSLSocketDataProvider ssl_data_first_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);
  // Only used in the HTTPS destination case, but harmless in the HTTP case.
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_first_request);

  // Second request should use DIRECT, skipping the bad proxies, and succeed.
  SSLSocketDataProvider ssl_data_second_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_second_request;
  socket_data_direct_second_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_second_request);
  // Only used in the HTTPS destination case, but harmless in the HTTP case.
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_second_request);

  // Now request a stream. It should succeed using the DIRECT fallback proxy
  // option.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = dest_url;

  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate));

  // Start two requests. The first request should consume data from
  // `socket_data_proxy_main_job` and `socket_data_direct_first_request`.
  // The second request should consume data from
  // `socket_data_direct_second_request`.

  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<HttpStreamRequest> request =
        CreateJobController(request_info);
    ASSERT_TRUE(request_delegate_->WaitForHttpStream());

    // Verify that request was fetched without proxy.
    EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());

    // The proxies that failed should now be known to the proxy service as bad.
    const ProxyRetryInfoMap& retry_info =
        session_->proxy_resolution_service()->proxy_retry_info();
    if (!triggers_ssl_connect_job_retry_logic) {
      EXPECT_THAT(retry_info,
                  ::testing::UnorderedElementsAre(

                      Key(ProxyUriToProxyChain(

                          "https://badproxy:99", ProxyServer::SCHEME_HTTP)),

                      Key(ProxyUriToProxyChain("https://badfallbackproxy:98",
                                               ProxyServer::SCHEME_HTTP))));
    } else {
      ASSERT_THAT(retry_info, SizeIs(1));
      EXPECT_THAT(retry_info,
                  Contains(Key(ProxyUriToProxyChain(
                      "https://badproxy:99", ProxyServer::SCHEME_HTTP))));
    }

    // The idle socket should have been added back to the socket pool. Close it,
    // so the next loop iteration creates a new socket instead of reusing the
    // idle one.
    auto* socket_pool = session_->GetSocketPool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());
    EXPECT_EQ(socket_pool->IdleSocketCount(), 1);
    socket_pool->CloseIdleSockets("Close socket reason");
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  auto* proxy_delegate =
      static_cast<TestProxyDelegate*>(session_deps_.proxy_delegate.get());

  // When this is set we fallback directly to DIRECT, hence we expect one fewer
  // call to CanFalloverToNextProxy. See ConfiguredProxyResolutionService
  // creation above and TriggersSslConnectJobRetryLogic()'s documentation.
  if (triggers_ssl_connect_job_retry_logic) {
    EXPECT_EQ(proxy_delegate->on_can_fallover_to_next_proxy_override_count(),
              1);
  } else {
    EXPECT_EQ(proxy_delegate->on_can_fallover_to_next_proxy_override_count(),
              2);
  }
}

constexpr TcpProxyTestCase kFirstNestedHttpsProxyTestCases[] = {
    // These largely correspond to the list of errors in
    // CanFalloverToNextProxy() which can occur with an HTTPS proxy.
    //
    // We omit `ERR_CONNECTION_CLOSED` because it is largely unreachable. The
    // HTTP/1.1 parser maps it to `ERR_EMPTY_RESPONSE` or
    // `ERR_RESPONSE_HEADERS_TRUNCATED` in most cases.
    //
    // TODO(davidben): Is omitting `ERR_EMPTY_RESPONSE` a bug in proxy error
    // handling?
    {TcpErrorPhase::kHostResolution, ERR_NAME_NOT_RESOLVED},
    {TcpErrorPhase::kTcpConnect, ERR_ADDRESS_UNREACHABLE},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_TIMED_OUT},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_RESET},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_ABORTED},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_REFUSED},
    {TcpErrorPhase::kProxySslHandshake, ERR_CERT_COMMON_NAME_INVALID},
    {TcpErrorPhase::kProxySslHandshake, ERR_SSL_PROTOCOL_ERROR},
    {TcpErrorPhase::kTunnelRead, ERR_TIMED_OUT},
    {TcpErrorPhase::kTunnelRead, ERR_SSL_PROTOCOL_ERROR},
};

using JobControllerReconsiderProxyAfterErrorFirstNestedHttpsProxyTest =
    JobControllerReconsiderProxyAfterErrorHttpProxyTest;

INSTANTIATE_TEST_SUITE_P(
    ,
    JobControllerReconsiderProxyAfterErrorFirstNestedHttpsProxyTest,
    ::testing::Combine(::testing::ValuesIn(kFirstNestedHttpsProxyTestCases),
                       ::testing::ValuesIn(kHttpTestUrls)),
    &PrintHttpProxyTestName);

// Same as above but using a multi-proxy chain, with errors encountered by the
// first proxy server in the chain.
TEST_P(JobControllerReconsiderProxyAfterErrorFirstNestedHttpsProxyTest, Test) {
  const auto [phase, error] = std::get<0>(GetParam());
  const GURL dest_url(std::get<1>(GetParam()));
  bool triggers_ssl_connect_job_retry_logic =
      std::get<0>(GetParam()).TriggersSslConnectJobRetryLogic();

  const ProxyServer kGoodProxyServer{ProxyServer::SCHEME_HTTPS,
                                     HostPortPair("goodproxyserver", 100)};
  const ProxyServer kBadProxyServer1{ProxyServer::SCHEME_HTTPS,
                                     HostPortPair("badproxyserver", 99)};
  const ProxyServer kBadProxyServer2{
      ProxyServer::SCHEME_HTTPS, HostPortPair("badfallbackproxyserver", 98)};
  const ProxyChain kNestedProxyChain1 =
      ProxyChain::ForIpProtection({{kBadProxyServer1, kGoodProxyServer}});
  const ProxyChain kNestedProxyChain2 =
      ProxyChain::ForIpProtection({{kBadProxyServer2, kGoodProxyServer}});
  const ProxyChain kDirectIpProtectionProxyChain =
      ProxyChain::ForIpProtection({});

  CreateSessionDeps();

  ProxyList proxy_list;
  proxy_list.AddProxyChain(kNestedProxyChain1);
  proxy_list.AddProxyChain(kNestedProxyChain2);
  proxy_list.AddProxyChain(kDirectIpProtectionProxyChain);
  ProxyConfig proxy_config = ProxyConfig::CreateForTesting(proxy_list);

  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedForTest(
          ProxyConfigWithAnnotation(proxy_config,
                                    TRAFFIC_ANNOTATION_FOR_TESTS));

  if (triggers_ssl_connect_job_retry_logic) {
    proxy_list.Clear();
    proxy_list.AddProxyChain(kNestedProxyChain1);
    proxy_list.AddProxyChain(kDirectIpProtectionProxyChain);
    ProxyConfig proxy_config2 = ProxyConfig::CreateForTesting(proxy_list);

    proxy_resolution_service =
        ConfiguredProxyResolutionService::CreateFixedForTest(
            ProxyConfigWithAnnotation(proxy_config2,
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  }
  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();
  test_proxy_delegate->set_extra_header_name("Foo");

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  static constexpr char kBadProxyServer1TunnelRequest[] =
      "CONNECT goodproxyserver:100 HTTP/1.1\r\n"
      "Host: goodproxyserver:100\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://badproxyserver:99\r\n\r\n";
  static constexpr char kBadProxyServer2TunnelRequest[] =
      "CONNECT goodproxyserver:100 HTTP/1.1\r\n"
      "Host: goodproxyserver:100\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://badfallbackproxyserver:98\r\n\r\n";
  const MockWrite kBadProxyServer1TunnelWrites[] = {
      MockWrite(ASYNC, 0, kBadProxyServer1TunnelRequest)};
  const MockWrite kBadProxyServer2TunnelWrites[] = {
      MockWrite(ASYNC, 0, kBadProxyServer2TunnelRequest)};
  std::vector<MockRead> reads;

  // Generate identical errors for the first proxy server in both the main proxy
  // chain and the fallback proxy chain. No alternative job is created for
  // either, so only need one data provider for each, when the request makes it
  // to the socket layer.
  std::unique_ptr<SequencedSocketData> socket_data_proxy_main_job;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job;
  std::unique_ptr<SequencedSocketData> socket_data_proxy_main_job2;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job2;
  switch (phase) {
    case TcpErrorPhase::kHostResolution:
      // Only ERR_NAME_NOT_RESOLVED can be returned by the mock host
      // resolver.
      DCHECK_EQ(ERR_NAME_NOT_RESOLVED, error);
      session_deps_.host_resolver->rules()->AddSimulatedFailure(
          "badproxyserver");
      session_deps_.host_resolver->rules()->AddSimulatedFailure(
          "badfallbackproxyserver");
      break;
    case TcpErrorPhase::kTcpConnect:
      socket_data_proxy_main_job = std::make_unique<SequencedSocketData>();
      socket_data_proxy_main_job->set_connect_data(MockConnect(ASYNC, error));
      socket_data_proxy_main_job2 = std::make_unique<SequencedSocketData>();
      socket_data_proxy_main_job2->set_connect_data(MockConnect(ASYNC, error));
      break;
    case TcpErrorPhase::kProxySslHandshake:
      socket_data_proxy_main_job = std::make_unique<SequencedSocketData>();
      ssl_data_proxy_main_job =
          std::make_unique<SSLSocketDataProvider>(ASYNC, error);
      socket_data_proxy_main_job2 = std::make_unique<SequencedSocketData>();
      ssl_data_proxy_main_job2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, error);
      break;
    case TcpErrorPhase::kTunnelRead:
      // Note: Unlike for single-proxy chains, tunnels are established for HTTP
      // destinations when multi-proxy chains are in use, so simulate tunnel
      // read failures in all cases.
      reads.emplace_back(ASYNC, error, 1);
      socket_data_proxy_main_job = std::make_unique<SequencedSocketData>(
          reads, kBadProxyServer1TunnelWrites);
      ssl_data_proxy_main_job =
          std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      socket_data_proxy_main_job2 = std::make_unique<SequencedSocketData>(
          reads, kBadProxyServer2TunnelWrites);
      ssl_data_proxy_main_job2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      break;
  }

  if (socket_data_proxy_main_job) {
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job.get());
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job2.get());
  }
  if (ssl_data_proxy_main_job) {
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        ssl_data_proxy_main_job.get());
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        ssl_data_proxy_main_job2.get());
  }

  // After both proxy chains fail, the request should fall back to using DIRECT,
  // and succeed.
  SSLSocketDataProvider ssl_data_first_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);
  // Only used in the HTTPS destination case, but harmless in the HTTP case.
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_first_request);

  // Second request should use DIRECT, skipping the bad proxies, and succeed.
  SSLSocketDataProvider ssl_data_second_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_second_request;
  socket_data_direct_second_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_second_request);
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_second_request);

  // Now request a stream. It should succeed using the DIRECT fallback proxy
  // option.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = dest_url;

  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate));

  // Start two requests. The first request should consume data from
  // `socket_data_proxy_main_job` and `socket_data_direct_first_request`.
  // The second request should consume data from
  // `socket_data_direct_second_request`.

  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<HttpStreamRequest> request =
        CreateJobController(request_info);
    ASSERT_TRUE(request_delegate_->WaitForHttpStream());

    // Verify that request was fetched without proxy.
    EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());

    // The proxies that failed should now be known to the proxy service as bad.
    const ProxyRetryInfoMap& retry_info =
        session_->proxy_resolution_service()->proxy_retry_info();
    if (!triggers_ssl_connect_job_retry_logic) {
      ASSERT_THAT(retry_info, SizeIs(2));
      EXPECT_THAT(retry_info, Contains(Key(kNestedProxyChain1)));
      EXPECT_THAT(retry_info, Contains(Key(kNestedProxyChain2)));
    } else {
      ASSERT_THAT(retry_info, SizeIs(1));
      EXPECT_THAT(retry_info, Contains(Key(kNestedProxyChain1)));
    }

    // The idle socket should have been added back to the socket pool. Close it,
    // so the next loop iteration creates a new socket instead of reusing the
    // idle one.
    auto* socket_pool = session_->GetSocketPool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, kDirectIpProtectionProxyChain);
    EXPECT_EQ(1, socket_pool->IdleSocketCount());
    socket_pool->CloseIdleSockets("Close socket reason");
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

constexpr TcpProxyTestCase kSecondNestedHttpsProxyTestCases[] = {
    // These largely correspond to the list of errors in
    // CanFalloverToNextProxy() which can occur with an HTTPS proxy.
    //
    // We omit `ERR_CONNECTION_CLOSED` because it is largely unreachable. The
    // HTTP/1.1 parser maps it to `ERR_EMPTY_RESPONSE` or
    // `ERR_RESPONSE_HEADERS_TRUNCATED` in most cases.
    //
    // TODO(davidben): Is omitting `ERR_EMPTY_RESPONSE` a bug in proxy error
    // handling?
    {TcpErrorPhase::kProxySslHandshake, ERR_CERT_COMMON_NAME_INVALID},
    {TcpErrorPhase::kProxySslHandshake, ERR_SSL_PROTOCOL_ERROR},
    {TcpErrorPhase::kTunnelRead, ERR_TIMED_OUT},
    {TcpErrorPhase::kTunnelRead, ERR_SSL_PROTOCOL_ERROR},
};

using JobControllerReconsiderProxyAfterErrorSecondNestedHttpsProxyTest =
    JobControllerReconsiderProxyAfterErrorHttpProxyTest;

INSTANTIATE_TEST_SUITE_P(
    ,
    JobControllerReconsiderProxyAfterErrorSecondNestedHttpsProxyTest,
    ::testing::Combine(::testing::ValuesIn(kSecondNestedHttpsProxyTestCases),
                       ::testing::ValuesIn(kHttpTestUrls)),
    &PrintHttpProxyTestName);

// Same as above but using a multi-proxy chain, with errors encountered by the
// second proxy server in the chain.
TEST_P(JobControllerReconsiderProxyAfterErrorSecondNestedHttpsProxyTest, Test) {
  const auto [phase, error] = std::get<0>(GetParam());
  const bool triggers_ssl_connect_job_retry_logic =
      std::get<0>(GetParam()).TriggersSslConnectJobRetryLogic();
  const GURL dest_url(std::get<1>(GetParam()));

  const ProxyServer kGoodProxyServer{ProxyServer::SCHEME_HTTPS,
                                     HostPortPair("goodproxyserver", 100)};
  const ProxyServer kBadProxyServer1{ProxyServer::SCHEME_HTTPS,
                                     HostPortPair("badproxyserver", 99)};
  const ProxyServer kBadProxyServer2{
      ProxyServer::SCHEME_HTTPS, HostPortPair("badfallbackproxyserver", 98)};
  const ProxyChain kNestedProxyChain1 =
      ProxyChain::ForIpProtection({{kGoodProxyServer, kBadProxyServer1}});
  const ProxyChain kNestedProxyChain2 =
      ProxyChain::ForIpProtection({{kGoodProxyServer, kBadProxyServer2}});
  const ProxyChain kDirectIpProtectionProxyChain =
      ProxyChain::ForIpProtection({});

  CreateSessionDeps();

  ProxyList proxy_list;
  proxy_list.AddProxyChain(kNestedProxyChain1);
  proxy_list.AddProxyChain(kNestedProxyChain2);
  proxy_list.AddProxyChain(kDirectIpProtectionProxyChain);
  ProxyConfig proxy_config = ProxyConfig::CreateForTesting(proxy_list);

  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedForTest(
          ProxyConfigWithAnnotation(proxy_config,
                                    TRAFFIC_ANNOTATION_FOR_TESTS));

  if (triggers_ssl_connect_job_retry_logic) {
    proxy_list.Clear();
    proxy_list.AddProxyChain(kNestedProxyChain1);
    proxy_list.AddProxyChain(kDirectIpProtectionProxyChain);
    ProxyConfig proxy_config2 = ProxyConfig::CreateForTesting(proxy_list);

    proxy_resolution_service =
        ConfiguredProxyResolutionService::CreateFixedForTest(
            ProxyConfigWithAnnotation(proxy_config2,
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  }
  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();
  test_proxy_delegate->set_extra_header_name("Foo");

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  static constexpr char kBadProxyServer1TunnelRequest[] =
      "CONNECT badproxyserver:99 HTTP/1.1\r\n"
      "Host: badproxyserver:99\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://goodproxyserver:100\r\n\r\n";
  static constexpr char kBadProxyServer2TunnelRequest[] =
      "CONNECT badfallbackproxyserver:98 HTTP/1.1\r\n"
      "Host: badfallbackproxyserver:98\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://goodproxyserver:100\r\n\r\n";
  const std::string kBadProxyServer1EndpointTunnelRequest = base::StringPrintf(
      "CONNECT %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://badproxyserver:99\r\n\r\n",
      HostPortPair::FromURL(dest_url).ToString().c_str(),
      HostPortPair::FromURL(dest_url).ToString().c_str());
  const std::string kBadProxyServer2EndpointTunnelRequest = base::StringPrintf(
      "CONNECT %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Foo: https://badfallbackproxyserver:98\r\n\r\n",
      HostPortPair::FromURL(dest_url).ToString().c_str(),
      HostPortPair::FromURL(dest_url).ToString().c_str());
  const MockWrite kNestedProxyChain1TunnelWrites[] = {
      {ASYNC, kBadProxyServer1TunnelRequest},
      {ASYNC, kBadProxyServer1EndpointTunnelRequest}};
  const MockWrite kNestedProxyChain2TunnelWrites[] = {
      {ASYNC, kBadProxyServer2TunnelRequest},
      {ASYNC, kBadProxyServer2EndpointTunnelRequest}};

  std::vector<MockRead> reads = {
      MockRead(ASYNC, 1, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  // Generate identical errors for the second proxy server in both the main
  // proxy chain and the fallback proxy chain. No alternative job is created for
  // either, so only need one data provider for each, when the request makes it
  // to the socket layer.
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job_server1;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job_server2;
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job2;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job2_server1;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job2_server2;

  ssl_data_proxy_main_job_server1 =
      std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl_data_proxy_main_job2_server1 =
      std::make_unique<SSLSocketDataProvider>(ASYNC, OK);

  switch (phase) {
    case TcpErrorPhase::kProxySslHandshake:
      ssl_data_proxy_main_job_server2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, error);
      ssl_data_proxy_main_job2_server2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, error);
      break;
    case TcpErrorPhase::kTunnelRead:
      // Note: Unlike for single-proxy chains, tunnels are established for HTTP
      // destinations when multi-proxy chains are in use, so simulate tunnel
      // read failures in all cases.
      reads.emplace_back(ASYNC, error);
      ssl_data_proxy_main_job_server2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      ssl_data_proxy_main_job2_server2 =
          std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
      break;
    case TcpErrorPhase::kHostResolution:
    case TcpErrorPhase::kTcpConnect:
      NOTREACHED()
          << "Skip the kHostResolution and kTcpConnect cases for this test"
          << "since those only make sense for connections to the first proxy "
             "server.";
  }
  socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>(
      reads, kNestedProxyChain1TunnelWrites);
  socket_data_proxy_main_job2 = std::make_unique<StaticSocketDataProvider>(
      reads, triggers_ssl_connect_job_retry_logic
                 ? kNestedProxyChain1TunnelWrites
                 : kNestedProxyChain2TunnelWrites);

  session_deps_.socket_factory->AddSocketDataProvider(
      socket_data_proxy_main_job.get());
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      ssl_data_proxy_main_job_server1.get());
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      ssl_data_proxy_main_job_server2.get());

  session_deps_.socket_factory->AddSocketDataProvider(
      socket_data_proxy_main_job2.get());
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      ssl_data_proxy_main_job2_server1.get());
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      ssl_data_proxy_main_job2_server2.get());

  // After both proxy chains fail, the request should fall back to using DIRECT,
  // and succeed.
  SSLSocketDataProvider ssl_data_first_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_first_request);

  // Second request should use DIRECT, skipping the bad proxies, and succeed.
  SSLSocketDataProvider ssl_data_second_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_second_request;
  socket_data_direct_second_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_second_request);
  // Only used in the HTTPS destination case, but harmless in the HTTP case.
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_second_request);

  // Now request a stream. It should succeed using the DIRECT fallback proxy
  // option.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = dest_url;

  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate));

  // Start two requests. The first request should consume data from
  // `socket_data_proxy_main_job` and `socket_data_direct_first_request`.
  // The second request should consume data from
  // `socket_data_direct_second_request`.

  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<HttpStreamRequest> request =
        CreateJobController(request_info);
    ASSERT_TRUE(request_delegate_->WaitForHttpStream());

    // Verify that request was fetched without proxy.
    EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());

    // The proxies that failed should now be known to the proxy service as bad.
    const ProxyRetryInfoMap& retry_info =
        session_->proxy_resolution_service()->proxy_retry_info();
    if (!triggers_ssl_connect_job_retry_logic) {
      ASSERT_THAT(retry_info, SizeIs(2));
      EXPECT_THAT(retry_info, Contains(Key(kNestedProxyChain1)));
      EXPECT_THAT(retry_info, Contains(Key(kNestedProxyChain2)));
    } else {
      ASSERT_THAT(retry_info, SizeIs(1));
      EXPECT_THAT(retry_info, Contains(Key(kNestedProxyChain1)));
    }

    // The idle socket should have been added back to the socket pool. Close it,
    // so the next loop iteration creates a new socket instead of reusing the
    // idle one.
    auto* socket_pool = session_->GetSocketPool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, kDirectIpProtectionProxyChain);
    EXPECT_EQ(1, socket_pool->IdleSocketCount());
    socket_pool->CloseIdleSockets("Close socket reason");
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Test proxy fallback logic for an IP Protection request.
TEST_F(JobControllerReconsiderProxyAfterErrorTest,
       FallbackOnTunnelConnectionFailedForIpProtection) {
  GURL dest_url = GURL("https://www.example.com");

  CreateSessionDeps();

  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "https://not-used:70", TRAFFIC_ANNOTATION_FOR_TESTS);
  auto test_proxy_delegate =
      std::make_unique<TestProxyDelegateForIpProtection>();

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  static constexpr char kTunnelRequest[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Authorization: https://ip-pro:443\r\n\r\n";
  const MockWrite kTunnelWrites[] = {{ASYNC, kTunnelRequest}};
  std::vector<MockRead> reads;

  // Generate errors for the first proxy server.
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_proxy_main_job;
  reads.emplace_back(ASYNC, ERR_TUNNEL_CONNECTION_FAILED);
  socket_data_proxy_main_job =
      std::make_unique<StaticSocketDataProvider>(reads, kTunnelWrites);
  ssl_data_proxy_main_job = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);

  session_deps_.socket_factory->AddSocketDataProvider(
      socket_data_proxy_main_job.get());
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      ssl_data_proxy_main_job.get());

  // After proxying fails, the request should fall back to using DIRECT, and
  // succeed.
  SSLSocketDataProvider ssl_data_first_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_first_request);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = dest_url;

  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate));

  std::unique_ptr<HttpStreamRequest> request =
      CreateJobController(request_info);
  ASSERT_TRUE(request_delegate_->WaitForHttpStream());

  // Verify that request was fetched without proxy.
  EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());
}

// Test that if the proxy delegate returns
// net::ERR_PROXY_UNABLE_TO_CONNECT_TO_DESTINATION in its
// OnTunnelHeadersReceived method, the request fails but the proxy is not marked
// as bad.
TEST_F(JobControllerReconsiderProxyAfterErrorTest,
       NoFallbackOnProxyUnableToConnectToDestination) {
  GURL dest_url = GURL("https://www.example.com");

  CreateSessionDeps();

  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "https://not-used:70", TRAFFIC_ANNOTATION_FOR_TESTS);
  auto test_proxy_delegate =
      std::make_unique<TestProxyDelegateForIpProtection>();

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  static constexpr char kProxyTunnelRequest[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "Authorization: https://ip-pro:443\r\n\r\n";

  MockWrite writes[] = {{ASYNC, kProxyTunnelRequest}};
  MockRead reads[] = {
      {ASYNC, "HTTP/1.1 502 Bad Gateway\r\n\r\n"},
  };
  StaticSocketDataProvider socket_data(reads, writes);
  session_deps_.socket_factory->AddSocketDataProvider(&socket_data);
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = dest_url;
  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate));

  std::unique_ptr<HttpStreamRequest> request =
      CreateJobController(request_info);
  EXPECT_EQ(request_delegate_->WaitForError(),
            ERR_PROXY_UNABLE_TO_CONNECT_TO_DESTINATION);

  // Verify that the proxy is not marked as bad.
  const ProxyRetryInfoMap& retry_info =
      session_->proxy_resolution_service()->proxy_retry_info();
  EXPECT_TRUE(retry_info.empty());
}

constexpr TcpProxyTestCase kSocks5ProxyTestCases[] = {
    // These largely correspond to the list of errors in
    // CanFalloverToNextProxy() which can occur with an HTTPS proxy.
    //
    // Unlike HTTP/HTTPS proxies, SOCKS proxies are retried in response to
    // `ERR_CONNECTION_CLOSED`.
    {TcpErrorPhase::kHostResolution, ERR_NAME_NOT_RESOLVED},
    {TcpErrorPhase::kTcpConnect, ERR_ADDRESS_UNREACHABLE},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_TIMED_OUT},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_RESET},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_ABORTED},
    {TcpErrorPhase::kTcpConnect, ERR_CONNECTION_REFUSED},
    {TcpErrorPhase::kTunnelRead, ERR_TIMED_OUT},
    {TcpErrorPhase::kTunnelRead, ERR_CONNECTION_CLOSED},
};

class JobControllerReconsiderProxyAfterErrorSocks5ProxyTest
    : public JobControllerReconsiderProxyAfterErrorTest,
      public ::testing::WithParamInterface<TcpProxyTestCase> {};

INSTANTIATE_TEST_SUITE_P(,
                         JobControllerReconsiderProxyAfterErrorSocks5ProxyTest,
                         ::testing::ValuesIn(kSocks5ProxyTestCases),
                         ::testing::PrintToStringParamName());

// Test proxy fallback logic in the case connecting through socks5 proxy.
TEST_P(JobControllerReconsiderProxyAfterErrorSocks5ProxyTest, Test) {
  const auto [phase, error] = GetParam();
  // "host" on port 80 matches the kSOCK5GreetRequest.
  const GURL kDestUrl = GURL("http://host:80/");

  CreateSessionDeps();

  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "SOCKS5 badproxy:99; SOCKS5 badfallbackproxy:98; DIRECT",
          TRAFFIC_ANNOTATION_FOR_TESTS);
  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());
  const MockWrite kTunnelWrites[] = {{ASYNC, kSOCKS5GreetRequest}};
  std::vector<MockRead> reads;

  // Generate identical errors for both the main proxy and the fallback proxy.
  // No alternative job is created for either, so only need one data provider
  // for each, when the request makes it to the socket layer.
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job;
  std::unique_ptr<StaticSocketDataProvider> socket_data_proxy_main_job2;
  switch (phase) {
    case TcpErrorPhase::kHostResolution:
      // Only ERR_NAME_NOT_RESOLVED can be returned by the mock host resolver.
      DCHECK_EQ(ERR_NAME_NOT_RESOLVED, error);
      session_deps_.host_resolver->rules()->AddSimulatedFailure("badproxy");
      session_deps_.host_resolver->rules()->AddSimulatedFailure(
          "badfallbackproxy");
      break;
    case TcpErrorPhase::kTcpConnect:
      socket_data_proxy_main_job = std::make_unique<StaticSocketDataProvider>();
      socket_data_proxy_main_job->set_connect_data(MockConnect(ASYNC, error));
      socket_data_proxy_main_job2 =
          std::make_unique<StaticSocketDataProvider>();
      socket_data_proxy_main_job2->set_connect_data(MockConnect(ASYNC, error));
      break;
    case TcpErrorPhase::kTunnelRead:
      reads.emplace_back(ASYNC, error);
      socket_data_proxy_main_job =
          std::make_unique<StaticSocketDataProvider>(reads, kTunnelWrites);
      socket_data_proxy_main_job2 =
          std::make_unique<StaticSocketDataProvider>(reads, kTunnelWrites);
      break;
    case TcpErrorPhase::kProxySslHandshake:
      NOTREACHED() << "Not possible using SOCKS5.";
  }

  if (socket_data_proxy_main_job) {
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job.get());
    session_deps_.socket_factory->AddSocketDataProvider(
        socket_data_proxy_main_job2.get());
  }

  // After both proxies fail, the request should fall back to using DIRECT, and
  // succeed.
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);

  // Second request should use DIRECT, skipping the bad proxies, and succeed.
  StaticSocketDataProvider socket_data_direct_second_request;
  socket_data_direct_second_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_second_request);

  // Now request a stream. It should succeed using the DIRECT fallback proxy
  // option.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = kDestUrl;

  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate));

  // Start two requests. The first request should consume data from
  // `socket_data_proxy_main_job` and `socket_data_direct_first_request`. The
  // second request should consume data from
  // `socket_data_direct_second_request`.

  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<HttpStreamRequest> request =
        CreateJobController(request_info);
    ASSERT_TRUE(request_delegate_->WaitForHttpStream());

    // Verify that request was fetched without proxy.
    EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());

    // The proxies that failed should now be known to the proxy service as bad.
    const ProxyRetryInfoMap& retry_info =
        session_->proxy_resolution_service()->proxy_retry_info();
    ASSERT_THAT(retry_info, SizeIs(2));
    EXPECT_THAT(retry_info,
                Contains(Key(ProxyUriToProxyChain(
                    "socks5://badproxy:99", ProxyServer::SCHEME_SOCKS5))));
    EXPECT_THAT(
        retry_info,
        Contains(Key(ProxyUriToProxyChain("socks5://badfallbackproxy:98",
                                          ProxyServer::SCHEME_SOCKS5))));

    // The idle socket should have been added back to the socket pool. Close it,
    // so the next loop iteration creates a new socket instead of reusing the
    // idle one.
    auto* socket_pool = session_->GetSocketPool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());
    EXPECT_EQ(1, socket_pool->IdleSocketCount());
    socket_pool->CloseIdleSockets("Close socket reason");
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Tests that ERR_MSG_TOO_BIG is retryable for QUIC proxy.
TEST_F(JobControllerReconsiderProxyAfterErrorTest, ReconsiderErrMsgTooBig) {
  auto quic_proxy_chain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "bad", 99)});
  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {quic_proxy_chain, ProxyChain::ForIpProtection({})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  // Mock data for the QUIC proxy socket.
  StaticSocketDataProvider quic_proxy_socket;
  quic_proxy_socket.set_connect_data(MockConnect(ASYNC, ERR_MSG_TOO_BIG));
  session_deps_.socket_factory->AddSocketDataProvider(&quic_proxy_socket);

  // Mock data for DIRECT.
  StaticSocketDataProvider socket_data_direct;
  socket_data_direct.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(&socket_data_direct);

  // Now request a stream. It should fall back to DIRECT on ERR_MSG_TOO_BIG.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");

  Initialize(std::move(proxy_resolution_service));

  std::unique_ptr<HttpStreamRequest> request =
      CreateJobController(request_info);
  ASSERT_TRUE(request_delegate_->WaitForHttpStream());

  EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());
  const ProxyRetryInfoMap& retry_info =
      session_->proxy_resolution_service()->proxy_retry_info();
  EXPECT_THAT(retry_info, SizeIs(1));
  EXPECT_THAT(retry_info, Contains(Key(quic_proxy_chain)));

  request.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

constexpr struct QuicProxyTestCase {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const QuicProxyTestCase& test_case) {
    sink.Append(testing::PrintToString(test_case.phase));
    sink.Append("_");
    sink.Append(ErrorToShortString(test_case.error));
  }

  QuicErrorPhase phase;
  Error error;
} kQuicProxyTestCases[] = {
    {QuicErrorPhase::kHostResolution, ERR_NAME_NOT_RESOLVED},
    // Test that proxy session gets activated but then failed before requesting
    // the stream. The error is determined by
    // QuicChromiumClientSession::Handle::RequestStream.
    {QuicErrorPhase::kProxySession, ERR_CONNECTION_CLOSED},
    {QuicErrorPhase::kUdpConnect, ERR_ADDRESS_UNREACHABLE},
    {QuicErrorPhase::kUdpConnect, ERR_CONNECTION_TIMED_OUT},
    {QuicErrorPhase::kUdpConnect, ERR_CONNECTION_RESET},
    {QuicErrorPhase::kUdpConnect, ERR_CONNECTION_ABORTED},
    {QuicErrorPhase::kUdpConnect, ERR_CONNECTION_REFUSED},
    {QuicErrorPhase::kUdpConnect, ERR_QUIC_PROTOCOL_ERROR},
    {QuicErrorPhase::kUdpConnect, ERR_QUIC_HANDSHAKE_FAILED},
    {QuicErrorPhase::kUdpConnect, ERR_MSG_TOO_BIG},
};

class JobControllerReconsiderProxyAfterErrorQuicProxyTest
    : public JobControllerReconsiderProxyAfterErrorTest,
      public ::testing::WithParamInterface<QuicProxyTestCase> {};

INSTANTIATE_TEST_SUITE_P(,
                         JobControllerReconsiderProxyAfterErrorQuicProxyTest,
                         ::testing::ValuesIn(kQuicProxyTestCases),
                         ::testing::PrintToStringParamName());

// Test proxy fallback logic in the case connecting through a QUIC proxy.
TEST_P(JobControllerReconsiderProxyAfterErrorQuicProxyTest, Test) {
  const auto [phase, error] = GetParam();
  // To use Quic proxy the destination must be HTTPS.
  GURL dest_url("https://www.example.com");

  url::SchemeHostPort proxy_server(url::kHttpsScheme, "badproxy", 99);
  url::SchemeHostPort proxy_server2(url::kHttpsScheme, "badfallbackproxy", 98);

  CreateSessionDeps();

  auto quic_proxy_chain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, proxy_server.host(), proxy_server.port())});
  auto quic_proxy_chain2 =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, proxy_server2.host(),
          proxy_server2.port())});
  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {quic_proxy_chain, quic_proxy_chain2, ProxyChain::Direct()},
          TRAFFIC_ANNOTATION_FOR_TESTS);
  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  // Generate identical errors for both the main proxy and the fallback proxy.
  // No alternative job is created for either, so only need one data provider
  // for each, when the request makes it to the socket layer.
  std::unique_ptr<StaticSocketDataProvider> quic_proxy_socket_main_job;
  std::unique_ptr<StaticSocketDataProvider> quic_proxy_socket_main_job2;
  switch (phase) {
    case QuicErrorPhase::kHostResolution:
      // Only ERR_NAME_NOT_RESOLVED can be returned by the mock host resolver.
      DCHECK_EQ(ERR_NAME_NOT_RESOLVED, GetParam().error);
      session_deps_.host_resolver->rules()->AddSimulatedFailure("badproxy");
      session_deps_.host_resolver->rules()->AddSimulatedFailure(
          "badfallbackproxy");
      break;
    case QuicErrorPhase::kProxySession:
      quic_proxy_socket_main_job = std::make_unique<StaticSocketDataProvider>();
      quic_proxy_socket_main_job->set_connect_data(MockConnect(ASYNC, OK));
      quic_proxy_socket_main_job2 =
          std::make_unique<StaticSocketDataProvider>();
      quic_proxy_socket_main_job2->set_connect_data(MockConnect(ASYNC, OK));
      break;
    case QuicErrorPhase::kUdpConnect:
      quic_proxy_socket_main_job = std::make_unique<StaticSocketDataProvider>();
      quic_proxy_socket_main_job->set_connect_data(MockConnect(ASYNC, error));
      quic_proxy_socket_main_job2 =
          std::make_unique<StaticSocketDataProvider>();
      quic_proxy_socket_main_job2->set_connect_data(MockConnect(ASYNC, error));
      break;
  }

  // Mock data for the QUIC proxy socket.
  if (quic_proxy_socket_main_job) {
    session_deps_.socket_factory->AddSocketDataProvider(
        quic_proxy_socket_main_job.get());
    session_deps_.socket_factory->AddSocketDataProvider(
        quic_proxy_socket_main_job2.get());
  }

  SSLSocketDataProvider ssl_data_first_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_first_request);

  // Second request should use DIRECT, skipping the bad proxies, and succeed.
  SSLSocketDataProvider ssl_data_second_request(ASYNC, OK);
  StaticSocketDataProvider socket_data_direct_second_request;
  socket_data_direct_second_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_second_request);
  session_deps_.socket_factory->AddSSLSocketDataProvider(
      &ssl_data_second_request);

  // Now request a stream. It should succeed using the DIRECT fallback proxy
  // option.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = dest_url;
  Initialize(std::move(proxy_resolution_service),
             std::move(test_proxy_delegate),
             /*using_quic=*/true);
  if (phase == QuicErrorPhase::kProxySession) {
    session_->quic_session_pool()->ActivateSessionForTesting(
        CreateMockQUICProxySession(proxy_server));
    session_->quic_session_pool()->ActivateSessionForTesting(
        CreateMockQUICProxySession(proxy_server2));
    ASSERT_EQ(mock_proxy_sessions_.size(), 2u);
  }

  // Start two requests. The first request should consume data from
  // `socket_data_proxy_main_job` and `socket_data_direct_first_request`.
  // The second request should consume data from
  // `socket_data_direct_second_request`.
  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<HttpStreamRequest> request =
        CreateJobController(request_info);
    RunUntilIdle();
    // TODO(crbug.com/336318587): Verify the session key.
    crypto_client_stream_factory_.last_stream()
        ->NotifySessionOneRttKeyAvailable();
    ASSERT_TRUE(request_delegate_->WaitForHttpStream());
    EXPECT_TRUE(request_delegate_->used_proxy_info().is_direct());

    // The proxies that failed should now be known to the proxy service as bad.
    const ProxyRetryInfoMap& retry_info =
        session_->proxy_resolution_service()->proxy_retry_info();
    ASSERT_THAT(retry_info, SizeIs(2));
    EXPECT_THAT(retry_info, Contains(Key(quic_proxy_chain)));
    EXPECT_THAT(retry_info, Contains(Key(quic_proxy_chain2)));

    // Quic connection does not create socket. So only check the sessions, and
    // close them. So that the next loop iteration won't reuse them.
    QuicSessionPool* quic_session_pool = session_->quic_session_pool();
    // Mock sessions must be removed from the vector before the session pool
    // destroys them to avoid dangling pointers.
    while (!mock_proxy_sessions_.empty()) {
      MockQuicChromiumClientSession* session = mock_proxy_sessions_.back();
      mock_proxy_sessions_.pop_back();
      quic_session_pool->DeactivateSessionForTesting(session);
    }
    EXPECT_EQ(1, quic_session_pool->CountActiveSessions());
    quic_session_pool->CloseAllSessions(OK, quic::QUIC_NO_ERROR);
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Same as test above except that this is testing the retry behavior for
// non-QUIC proxy on ERR_MSG_TOO_BIG.
TEST_F(JobControllerReconsiderProxyAfterErrorTest,
       DoNotReconsiderErrMsgTooBig) {
  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS badproxy:99; DIRECT", TRAFFIC_ANNOTATION_FOR_TESTS);

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  // Mock data for the HTTPS proxy socket.
  static constexpr char kHttpConnect[] =
      "CONNECT www.example.com:443 HTTP/1.1\r\n"
      "Host: www.example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n\r\n";
  const MockWrite kWrites[] = {{ASYNC, kHttpConnect}};
  const MockRead kReads[] = {{ASYNC, ERR_MSG_TOO_BIG}};
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  StaticSocketDataProvider https_proxy_socket(kReads, kWrites);
  https_proxy_socket.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(&https_proxy_socket);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  // Now request a stream. It should not fallback to DIRECT on ERR_MSG_TOO_BIG.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");

  Initialize(std::move(proxy_resolution_service));

  std::unique_ptr<HttpStreamRequest> request =
      CreateJobController(request_info);
  EXPECT_EQ(request_delegate_->WaitForError(), ERR_MSG_TOO_BIG);

  const ProxyRetryInfoMap& retry_info =
      session_->proxy_resolution_service()->proxy_retry_info();
  EXPECT_THAT(retry_info, SizeIs(0));

  request.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerTest, OnStreamFailedWithNoAlternativeJob) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, ERR_FAILED));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  // There's no other alternative job. Thus when stream failed, it should
  // notify Request of the stream failure.
  EXPECT_EQ(request_delegate_->WaitForError(), ERR_FAILED);
}

TEST_F(HttpStreamFactoryJobControllerTest, OnStreamReadyWithNoAlternativeJob) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  // There's no other alternative job. Thus when a stream is ready, it should
  // notify Request.
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
}

// Test we cancel Jobs correctly when the Request is explicitly canceled
// before any Job is bound to Request.
TEST_F(HttpStreamFactoryJobControllerTest, CancelJobsBeforeBinding) {
  // Use COLD_START to make the alt job pending.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  // Reset the Request will cancel all the Jobs since there's no Job determined
  // to serve Request yet and JobController will notify the factory to delete
  // itself upon completion.
  request_.reset();
  // QuicSessionPool::Job::Request will not complete since the Jobs are
  // canceled, so there is no need to check if all read data was consumed.
  should_check_data_consumed_ = false;
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Test that the controller does not create alternative job when the advertised
// versions in AlternativeServiceInfo do not contain any version that is
// supported.
TEST_F(HttpStreamFactoryJobControllerTest,
       DoNotCreateAltJobIfQuicVersionsUnsupported) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      {quic::ParsedQuicVersion::Unsupported()});

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  request_.reset();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

void HttpStreamFactoryJobControllerTestBase::
    TestDoNotDelayMainJobIfQuicWasRecentlyBroken(bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      quic_context_.params()->supported_versions);

  // Enable QUIC but mark the alternative service as recently broken.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);
  session_->http_server_properties()->MarkAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey());

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // The main job shouldn't have any delay since QUIC was recently broken. Main
  // job should still be blocked as alt job has not succeeded or failed at least
  // once yet.
  EXPECT_EQ(job_controller_->get_main_job_wait_time_for_tests(),
            base::TimeDelta());
  if (async_quic_session) {
    EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));
  } else {
    EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  }
  // Make `alternative_job` succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);
  auto* http_stream_ptr = http_stream.get();

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job());

  EXPECT_EQ(request_delegate_->WaitForHttpStream().get(), http_stream_ptr);

  // Check that alternative job is bound while main job is destroyed.
  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  request_.reset();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read.
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpStreamFactoryJobControllerTest,
       DoNotDelayMainJobIfQuicWasRecentlyBroken) {
  TestDoNotDelayMainJobIfQuicWasRecentlyBroken(false);
}

TEST_F(HttpStreamFactoryJobControllerTest,
       DoNotDelayMainJobIfQuicWasRecentlyBrokenAsyncQuicSession) {
  TestDoNotDelayMainJobIfQuicWasRecentlyBroken(true);
}

void HttpStreamFactoryJobControllerTestBase::
    TestDelayMainJobAfterRecentlyBrokenQuicWasConfirmed(
        bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  base::Time expiration = base::Time::Now() + base::Days(1);
  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      quic_context_.params()->supported_versions);

  // Enable QUIC but mark the alternative service as recently broken.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);
  session_->http_server_properties()->MarkAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey());

  // Confirm the alt service.
  session_->http_server_properties()->ConfirmAlternativeService(
      alternative_service, NetworkAnonymizationKey());

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // The main job should wait and it should still be blocked because the new
  // QUIC session hasn't been created yet. The wait time should be greater than
  // 0.
  EXPECT_TRUE(job_controller_->ShouldWait(
      const_cast<HttpStreamFactory::Job*>(job_controller_->main_job())));
  if (async_quic_session) {
    EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));
  } else {
    EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  }
  EXPECT_GE(job_controller_->get_main_job_wait_time_for_tests(),
            base::TimeDelta());

  // Make `alternative_job` succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);
  auto* http_stream_ptr = http_stream.get();

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job());

  EXPECT_EQ(request_delegate_->WaitForHttpStream().get(), http_stream_ptr);

  // Check that alternative job is bound while main job is destroyed.
  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  request_.reset();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read.
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpStreamFactoryJobControllerTest,
       DelayMainJobAfterRecentlyBrokenQuicWasConfirmed) {
  TestDelayMainJobAfterRecentlyBrokenQuicWasConfirmed(false);
}

TEST_F(HttpStreamFactoryJobControllerTest,
       DelayMainJobAfterRecentlyBrokenQuicWasConfirmedAsyncQuicSession) {
  TestDelayMainJobAfterRecentlyBrokenQuicWasConfirmed(true);
}

void HttpStreamFactoryJobControllerTestBase::TestOnStreamFailedForBothJobs(
    bool alt_job_retried_on_non_default_network,
    bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddConnect(ASYNC, ERR_FAILED);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, ERR_FAILED));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  if (async_quic_session) {
    EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1).WillOnce([this]() {
      job_factory_.main_job()->DoResume();
    });
  }
  // The failure of second Job should be reported to Request as there's no more
  // pending Job to serve the Request.
  EXPECT_EQ(request_delegate_->WaitForError(), ERR_FAILED);

  VerifyBrokenAlternateProtocolMapping(request_info, false);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// This test verifies that the alternative service is not marked broken if both
// jobs fail, and the alternative job is not retried on the alternate network.
TEST_F(HttpStreamFactoryJobControllerTest,
       OnStreamFailedForBothJobsWithoutQuicRetry) {
  TestOnStreamFailedForBothJobs(false, false);
}

// This test verifies that the alternative service is not marked broken if both
// jobs fail, and the alternative job is retried on the alternate network.
TEST_F(HttpStreamFactoryJobControllerTest,
       OnStreamFailedForBothJobsWithQuicRetriedOnAlternateNetwork) {
  TestOnStreamFailedForBothJobs(true, false);
}

// This test verifies that the alternative service is not marked broken if both
// jobs fail, and the alternative job is not retried on the alternate network.
// This test uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       OnStreamFailedForBothJobsWithoutQuicRetryAsyncQuicSession) {
  TestOnStreamFailedForBothJobs(false, true);
}

// This test verifies that the alternative service is not marked broken if both
// jobs fail, and the alternative job is retried on the alternate network. This
// test uses asynchronous QUIC session creation.
TEST_F(
    HttpStreamFactoryJobControllerTest,
    OnStreamFailedForBothJobsWithQuicRetriedOnAlternateNetworkAsyncQuicSession) {
  TestOnStreamFailedForBothJobs(true, true);
}

void HttpStreamFactoryJobControllerTestBase::
    TestAltJobFailsAfterMainJobSucceeded(
        bool alt_job_retried_on_non_default_network,
        bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(ASYNC, ERR_FAILED);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  if (async_quic_session) {
    EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1).WillOnce([this]() {
      job_factory_.main_job()->DoResume();
    });
  }
  // Main job succeeds, starts serving Request and it should report status
  // to Request. The alternative job will mark the main job complete and gets
  // orphaned.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());

  // Reset the request as it's been successfully served.
  request_.reset();
  base::RunLoop().RunUntilIdle();
  VerifyBrokenAlternateProtocolMapping(request_info, true);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // Verify the brokenness is not cleared when the default network changes.
  session_->http_server_properties()->OnDefaultNetworkChanged();
  VerifyBrokenAlternateProtocolMapping(request_info, true);
}

// This test verifies that the alternative service is marked broken when the
// alternative job fails on default after the main job succeeded.  The
// brokenness should not be cleared when the default network changes.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobFailsOnDefaultNetworkAfterMainJobSucceeded) {
  TestAltJobFailsAfterMainJobSucceeded(false, false);
}

// This test verifies that the alternative service is marked broken when the
// alternative job fails on both networks after the main job succeeded.  The
// brokenness should not be cleared when the default network changes.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobFailsOnBothNetworksAfterMainJobSucceeded) {
  TestAltJobFailsAfterMainJobSucceeded(true, false);
}

// This test verifies that the alternative service is marked broken when the
// alternative job fails on default after the main job succeeded. The
// brokenness should not be cleared when the default network changes. This test
// uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobFailsOnDefaultNetworkAfterMainJobSucceededAsyncQuicSession) {
  TestAltJobFailsAfterMainJobSucceeded(false, true);
}

// This test verifies that the alternative service is marked broken when the
// alternative job fails on both networks after the main job succeeded.  The
// brokenness should not be cleared when the default network changes. This test
// uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobFailsOnBothNetworksAfterMainJobSucceededAsyncQuicSession) {
  TestAltJobFailsAfterMainJobSucceeded(true, true);
}

void HttpStreamFactoryJobControllerTestBase::TestAltJobSucceedsMainJobDestroyed(
    bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Use cold start and complete alt job manually.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);
  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  if (async_quic_session) {
    EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));
  } else {
    EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  }
  // Make `alternative_job` succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);
  auto* http_stream_ptr = http_stream.get();

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job());

  EXPECT_EQ(request_delegate_->WaitForHttpStream().get(), http_stream_ptr);

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  request_.reset();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read.
  base::RunLoop().RunUntilIdle();
}

// Tests that when alt job succeeds, main job is destroyed.
TEST_F(HttpStreamFactoryJobControllerTest, AltJobSucceedsMainJobDestroyed) {
  TestAltJobSucceedsMainJobDestroyed(false);
}

// Tests that when alt job succeeds, main job is destroyed.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsMainJobDestroyedAsyncQuicSession) {
  TestAltJobSucceedsMainJobDestroyed(true);
}

// Tests that if alt job succeeds and main job is blocked, main job should be
// cancelled immediately. `request_` completion will clean up the JobController.
// Regression test for crbug.com/678768.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsMainJobBlockedControllerDestroyed) {
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data_->AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);
  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));

  // `alternative_job` succeeds and should report status to `request_delegate_`.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Invoke OnRequestComplete() which should delete `job_controller_` from
  // `factory_`.
  request_.reset();
  // base::RunLoop().RunUntilIdle();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  // This fails without the fix for crbug.com/678768.
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerTest,
       SpdySessionKeyHasOriginHostPortPair) {
  session_deps_.enable_http2_alternative_service = true;

  const char origin_host[] = "www.example.org";
  const uint16_t origin_port = 443;
  const char alternative_host[] = "mail.example.org";
  const uint16_t alternative_port = 123;

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url =
      GURL(base::StringPrintf("https://%s:%u", origin_host, origin_port));
  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                         alternative_host, alternative_port);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  HostPortPair main_host_port_pair =
      HttpStreamFactoryJobPeer::GetSpdySessionKey(job_controller_->main_job())
          .host_port_pair();
  EXPECT_EQ(origin_host, main_host_port_pair.host());
  EXPECT_EQ(origin_port, main_host_port_pair.port());

  HostPortPair alternative_host_port_pair =
      HttpStreamFactoryJobPeer::GetSpdySessionKey(
          job_controller_->alternative_job())
          .host_port_pair();
  EXPECT_EQ(origin_host, alternative_host_port_pair.host());
  EXPECT_EQ(origin_port, alternative_host_port_pair.port());
}

// Regression test for crbug.com/395919017.
// Test that a Job calls HttpServerProperties::SetSupportsSpdy() for an IPv6
// host appropriately.
TEST_F(HttpStreamFactoryJobControllerTest, SupportsSpdyIPv6Destination) {
  // Make sure there is only one socket connect.
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  tcp_data_ = std::make_unique<SequencedSocketData>(reads, writes);
  // connect needs to be async, so the H2 session isn't created immediately.
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://[2001:db8::1]");
  Initialize(request_info);

  // Start a request.
  std::unique_ptr<HttpStreamRequest> stream_request = job_controller_->Start(
      request_delegate_.get(), nullptr /* websocket_handshake_create_helper */,
      NetLogWithSource(), HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  // Wait for an H2 session creation.
  FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());

  stream_request.reset();

  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // Check that the SpdySession was created.
  base::WeakPtr<SpdySession> spdy_session =
      session_->spdy_session_pool()->FindAvailableSession(
          SpdySessionKey(HostPortPair::FromURL(request_info.url),
                         request_info.privacy_mode, ProxyChain::Direct(),
                         SessionUsage::kDestination, request_info.socket_tag,
                         request_info.network_anonymization_key,
                         request_info.secure_dns_policy,
                         /*disable_cert_verification_network_fetches=*/false),
          /*enable_ip_based_pooling_for_h2=*/false, /*is_websocket=*/false,
          NetLogWithSource());
  EXPECT_TRUE(spdy_session);

  EXPECT_TRUE(session_->http_server_properties()->GetSupportsSpdy(
      url::SchemeHostPort(request_info.url),
      request_info.network_anonymization_key));
}

void HttpStreamFactoryJobControllerTestBase::
    TestOrphanedJobCompletesControllerDestroyed(bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Use cold start and complete alt job manually.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (async_quic_session) {
    EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1).WillOnce([this]() {
      job_factory_.main_job()->DoResume();
    });
  }

  // Complete main job now.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());

  // Invoke OnRequestComplete() which should not delete `job_controller_` from
  // `factory_` because alt job is yet to finish.
  request_.reset();
  ASSERT_FALSE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Make `alternative_job` succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);
  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  // This should not call request_delegate_::OnStreamReady.
  job_controller_->OnStreamReady(job_factory_.alternative_job());
  // Make sure that controller does not leak.
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Tests that if an orphaned job completes after `request_` is gone,
// JobController will be cleaned up.
TEST_F(HttpStreamFactoryJobControllerTest,
       OrphanedJobCompletesControllerDestroyed) {
  TestOrphanedJobCompletesControllerDestroyed(false);
}

// Tests that if an orphaned job completes after `request_` is gone,
// JobController will be cleaned up.
TEST_F(HttpStreamFactoryJobControllerTest,
       OrphanedJobCompletesControllerDestroyedAsyncQuicSession) {
  TestOrphanedJobCompletesControllerDestroyed(true);
}

void HttpStreamFactoryJobControllerTestBase::
    TestAltJobSucceedsAfterMainJobFailed(
        bool alt_job_retried_on_non_default_network,
        bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Use cold start and complete alt job manually.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  // One failed TCP connect.
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, ERR_FAILED));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retried on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  // Make `alternative_job` succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);
  if (async_quic_session) {
    base::RunLoop run_loop;
    EXPECT_CALL(*job_factory_.main_job(), Resume())
        .Times(1)
        .WillOnce([&run_loop, this]() {
          run_loop.Quit();
          job_factory_.main_job()->DoResume();
        });
    run_loop.Run();
  }
  auto* http_stream_ptr = http_stream.get();

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job());
  // `alternative_job` succeeds and should report status to Request.
  EXPECT_EQ(request_delegate_->WaitForHttpStream().get(), http_stream_ptr);

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read. Marking as broken also occurs asynchronously.
  base::RunLoop().RunUntilIdle();

  VerifyBrokenAlternateProtocolMapping(request_info, false);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// This test verifies that the alternative service is not mark broken if the
// alternative job succeeds on the default network after the main job failed.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnDefaultNetworkAfterMainJobFailed) {
  TestAltJobSucceedsAfterMainJobFailed(false, false);
}

// This test verifies that the alternative service is not mark broken if the
// alternative job succeeds on the alternate network after the main job failed.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnAlternateNetworkAfterMainJobFailed) {
  TestAltJobSucceedsAfterMainJobFailed(true, false);
}

// This test verifies that the alternative service is not mark broken if the
// alternative job succeeds on the default network after the main job failed.
// This test uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnDefaultNetworkAfterMainJobFailedAsyncQuicSession) {
  TestAltJobSucceedsAfterMainJobFailed(false, true);
}

// This test verifies that the alternative service is not mark broken if the
// alternative job succeeds on the alternate network after the main job failed.
// This test uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnAlternateNetworkAfterMainJobFailedAsyncQuicSession) {
  TestAltJobSucceedsAfterMainJobFailed(true, true);
}

void HttpStreamFactoryJobControllerTestBase::
    TestAltJobSucceedsAfterMainJobSucceeded(
        bool alt_job_retried_on_non_default_network,
        bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Use cold start and complete alt job manually.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (async_quic_session) {
    EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1).WillOnce([this]() {
      job_factory_.main_job()->DoResume();
    });
  }

  // Wait for `main_job` succeed and status to be reported to Request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
  VerifyBrokenAlternateProtocolMapping(request_info, false);

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  // Make `alternative_job` succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job());

  request_.reset();
  // If alt job was retried on the alternate network, the alternative service
  // should be marked broken until the default network changes.
  VerifyBrokenAlternateProtocolMapping(request_info,
                                       alt_job_retried_on_non_default_network);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  if (alt_job_retried_on_non_default_network) {
    // Verify the brokenness is cleared when the default network changes.
    session_->http_server_properties()->OnDefaultNetworkChanged();
    VerifyBrokenAlternateProtocolMapping(request_info, false);
  }

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read.
  base::RunLoop().RunUntilIdle();
}

// This test verifies that the alternative service is not marked broken if the
// alternative job succeeds on the default network after the main job succeeded.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnDefaultNetworkAfterMainJobSucceeded) {
  TestAltJobSucceedsAfterMainJobSucceeded(false, false);
}

// This test verifies that the alternative service is marked broken until the
// default network changes if the alternative job succeeds on the non-default
// network, which failed on the default network previously, after the main job
// succeeded.  The brokenness should be cleared when the default network
// changes.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnAlternateNetworkAfterMainJobSucceeded) {
  TestAltJobSucceedsAfterMainJobSucceeded(true, false);
}

// This test verifies that the alternative service is not marked broken if the
// alternative job succeeds on the default network after the main job succeeded.
// This test uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnDefaultNetworkAfterMainJobSucceededAsyncQuicSession) {
  TestAltJobSucceedsAfterMainJobSucceeded(false, true);
}

// This test verifies that the alternative service is marked broken until the
// default network changes if the alternative job succeeds on the non-default
// network, which failed on the default network previously, after the main job
// succeeded.  The brokenness should be cleared when the default network
// changes. This test uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnAlternateNetworkAfterMainJobSucceededAsyncQuicSession) {
  TestAltJobSucceedsAfterMainJobSucceeded(true, true);
}

void HttpStreamFactoryJobControllerTestBase::
    TestMainJobSucceedsAfterAltJobSucceeded(
        bool alt_job_retried_on_non_default_network,
        bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Use cold start and complete alt job manually.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }
  // Make `alternative_job` succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);
  if (async_quic_session) {
    base::RunLoop run_loop;
    EXPECT_CALL(*job_factory_.main_job(), Resume())
        .Times(1)
        .WillOnce([&run_loop, this]() {
          run_loop.Quit();
          job_factory_.main_job()->DoResume();
        });
    run_loop.Run();
  }
  auto* http_stream_ptr = http_stream.get();

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job());

  // Wait for the main job succeed.
  EXPECT_EQ(request_delegate_->WaitForHttpStream().get(), http_stream_ptr);
  request_.reset();

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read. Marking as broken also occurs asynchronously.
  base::RunLoop().RunUntilIdle();

  // If alt job was retried on the alternate network, the alternative service
  // should be marked broken until the default network changes.
  VerifyBrokenAlternateProtocolMapping(request_info,
                                       alt_job_retried_on_non_default_network);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  if (alt_job_retried_on_non_default_network) {
    // Verify the brokenness is cleared when the default network changes.
    session_->http_server_properties()->OnDefaultNetworkChanged();
    VerifyBrokenAlternateProtocolMapping(request_info, false);
  }
}

// This test verifies that the alternative service is not marked broken if the
// main job succeeds after the alternative job succeeded on the default network.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobSucceededOnDefaultNetwork) {
  TestMainJobSucceedsAfterAltJobSucceeded(false, false);
}

// This test verifies that the alternative service is marked broken until the
// default network changes if the main job succeeds after the alternative job
// succeeded on the non-default network, i.e., failed on the default network
// previously.  The brokenness should be cleared when the default network
// changes.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobSucceededOnAlternateNetwork) {
  TestMainJobSucceedsAfterAltJobSucceeded(true, false);
}

// This test verifies that the alternative service is not marked broken if the
// main job succeeds after the alternative job succeeded on the default network.
// This test uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobSucceededOnDefaultNetworkAsyncQuicSession) {
  TestMainJobSucceedsAfterAltJobSucceeded(false, true);
}

// This test verifies that the alternative service is marked broken until the
// default network changes if the main job succeeds after the alternative job
// succeeded on the non-default network, i.e., failed on the default network
// previously.  The brokenness should be cleared when the default network
// changes. This test uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobSucceededOnAlternateNetworkAsyncQuicSession) {
  TestMainJobSucceedsAfterAltJobSucceeded(true, true);
}

void HttpStreamFactoryJobControllerTestBase::
    TestMainJobFailsAfterAltJobSucceeded(
        bool alt_job_retried_on_non_default_network,
        bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  // Use cold start and complete alt job manually.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, ERR_FAILED));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }
  // Make `alternative_job` succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);
  if (async_quic_session) {
    base::RunLoop run_loop;
    EXPECT_CALL(*job_factory_.main_job(), Resume())
        .Times(1)
        .WillOnce([&run_loop, this]() {
          run_loop.Quit();
          job_factory_.main_job()->DoResume();
        });
    run_loop.Run();
  }
  auto* http_stream_ptr = http_stream.get();

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job());

  EXPECT_EQ(request_delegate_->WaitForHttpStream().get(), http_stream_ptr);

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read. Marking as broken also occurs asynchronously.
  base::RunLoop().RunUntilIdle();

  VerifyBrokenAlternateProtocolMapping(request_info, false);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// This test verifies that the alternative service is not marked broken if the
// main job fails after the alternative job succeeded on the default network.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobFailsAfterAltJobSucceededOnDefaultNetwork) {
  TestMainJobFailsAfterAltJobSucceeded(false, false);
}

// This test verifies that the alternative service is not marked broken if the
// main job fails after the alternative job succeeded on the non-default
// network, i.e., failed on the default network previously.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobFailsAfterAltJobSucceededOnAlternateNetwork) {
  TestMainJobFailsAfterAltJobSucceeded(true, false);
}

// This test verifies that the alternative service is not marked broken if the
// main job fails after the alternative job succeeded on the default network.
// This test uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobFailsAfterAltJobSucceededOnDefaultNetworkAsyncQuicSession) {
  TestMainJobFailsAfterAltJobSucceeded(false, true);
}

// This test verifies that the alternative service is not marked broken if the
// main job fails after the alternative job succeeded on the non-default
// network, i.e., failed on the default network previously. This test uses
// asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobFailsAfterAltJobSucceededOnAlternateNetworkAsyncQuicSession) {
  TestMainJobFailsAfterAltJobSucceeded(true, true);
}

void HttpStreamFactoryJobControllerTestBase::
    TestMainJobSucceedsAfterAltJobFailed(
        bool alt_job_retried_on_non_default_network,
        bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddConnect(SYNCHRONOUS, ERR_FAILED);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  base::HistogramTester histogram_tester;
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // `alternative_job` fails but should not report status to Request.
  if (async_quic_session) {
    EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1).WillOnce([this]() {
      job_factory_.main_job()->DoResume();
    });
  }

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  // `main_job` succeeds and should report status to Request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream().get());

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read. Marking as broken also occurs asynchronously.
  base::RunLoop().RunUntilIdle();

  request_.reset();
  // Verify that the alternate protocol is marked as broken.
  VerifyBrokenAlternateProtocolMapping(request_info, true);
  histogram_tester.ExpectUniqueSample("Net.AlternateServiceFailed", -ERR_FAILED,
                                      1);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  // Verify the brokenness is not cleared when the default network changes.
  session_->http_server_properties()->OnDefaultNetworkChanged();
  VerifyBrokenAlternateProtocolMapping(request_info, true);
}

// This test verifies that the alternative service will be marked broken when
// the alternative job fails on the default network and main job succeeds later.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobFailedOnDefaultNetwork) {
  TestMainJobSucceedsAfterAltJobFailed(false, false);
}

// This test verifies that the alternative service will be marked broken when
// the alternative job fails on both default and alternate networks and main job
// succeeds later.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobFailedOnBothNetworks) {
  TestMainJobSucceedsAfterAltJobFailed(true, false);
}

// This test verifies that the alternative service will be marked broken when
// the alternative job fails on the default network and main job succeeds later.
// This test uses asynchronous Quic session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobFailedOnDefaultNetworkAsyncQuicSession) {
  TestMainJobSucceedsAfterAltJobFailed(false, true);
}

// This test verifies that the alternative service will be marked broken when
// the alternative job fails on both default and alternate networks and main job
// succeeds later. This test uses asynchronous Quic session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobFailedOnBothNetworksAsyncQuicSession) {
  TestMainJobSucceedsAfterAltJobFailed(true, true);
}

void HttpStreamFactoryJobControllerTestBase::
    TestMainJobSucceedsAfterIgnoredError(int net_error,
                                         bool async_quic_session,
                                         bool expect_broken,
                                         std::string alternate_host) {
  SetAsyncQuicSession(async_quic_session);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddConnect(SYNCHRONOUS, net_error);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  base::HistogramTester histogram_tester;

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  if (alternate_host.empty()) {
    alternate_host = server.host();
  }
  AlternativeService alternative_service(NextProto::kProtoQUIC, alternate_host,
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // `alternative_job` fails but should not report status to Request.
  if (async_quic_session) {
    EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1).WillOnce([this]() {
      job_factory_.main_job()->DoResume();
    });
  }
  // `main_job` succeeds and should report status to Request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream().get());

  // Make sure all data that is doing to be read has been read - QUIC, in
  // particular, reads data asynchronously, which the test fixture expects to be
  // read. Marking as broken also occurs asynchronously.
  base::RunLoop().RunUntilIdle();

  request_.reset();

  // Verify that the alternate protocol is not marked as broken.
  VerifyBrokenAlternateProtocolMapping(request_info, expect_broken);
  if (expect_broken) {
    histogram_tester.ExpectUniqueSample("Net.AlternateServiceFailed",
                                        -net_error, 1);
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Verifies that if the alternative job fails due to a connection change event,
// then the alternative service is not marked as broken.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterConnectionChanged) {
  TestMainJobSucceedsAfterIgnoredError(ERR_NETWORK_CHANGED, false);
}

// Verifies that if the alternative job fails due to a disconnected network,
// then the alternative service is not marked as broken.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterInternetDisconnected) {
  TestMainJobSucceedsAfterIgnoredError(ERR_INTERNET_DISCONNECTED, false);
}

// Verifies that if the alternative job fails due to a connection change event,
// then the alternative service is not marked as broken. This test uses
// asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterConnectionChangedAsyncQuicSession) {
  TestMainJobSucceedsAfterIgnoredError(ERR_NETWORK_CHANGED, true);
}

// Verifies that if the alternative job fails due to a disconnected network,
// then the alternative service is not marked as broken. This test uses
// asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterInternetDisconnectedAsyncQuicSession) {
  TestMainJobSucceedsAfterIgnoredError(ERR_INTERNET_DISCONNECTED, true);
}

// Verifies that if the alternative job fails due to a DNS failure,
// then the alternative service is not marked as broken.
TEST_F(HttpStreamFactoryJobControllerTest, MainJobSucceedsAfterDnsFailure) {
  TestMainJobSucceedsAfterIgnoredError(ERR_NAME_NOT_RESOLVED, false);
}

// Verifies that if the alternative job fails due to a DNS failure,
// then the alternative service is not marked as broken. This test uses
// asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterDnsFailureAsyncQuicSession) {
  TestMainJobSucceedsAfterIgnoredError(ERR_NAME_NOT_RESOLVED, true);
}

// Verifies that if the alternative job fails due to a DNS failure on a
// different name, then the alternative service is marked as broken.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterDnsFailureWithAlternateName) {
  TestMainJobSucceedsAfterIgnoredError(ERR_NAME_NOT_RESOLVED, false, true,
                                       "alternate.google.com");
}

// Verifies that if the alternative job fails due to a DNS failure on a
// different name, then the alternative service is marked as broken. This test
// uses asynchronous QUIC session creation.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterDnsFailureWithAlternateNameAsyncQuicSession) {
  TestMainJobSucceedsAfterIgnoredError(ERR_NAME_NOT_RESOLVED, true, true,
                                       "alternate.google.com");
}

// Regression test for crbug/621069.
// Get load state after main job fails and before alternative job succeeds.
TEST_F(HttpStreamFactoryJobControllerTest, GetLoadStateAfterMainJobFailed) {
  // Use COLD_START to complete alt job manually.
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, ERR_FAILED));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // `main_job` fails but should not report status to Request.
  base::RunLoop().RunUntilIdle();

  // Controller should use alternative job to get load state.
  job_controller_->GetLoadState();

  // `alternative_job` succeeds and should report status to Request.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false);
  auto* http_stream_ptr = http_stream.get();

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job());

  EXPECT_EQ(request_delegate_->WaitForHttpStream().get(), http_stream_ptr);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

void HttpStreamFactoryJobControllerTestBase::TestResumeMainJobWhenAltJobStalls(
    bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);
  // Use COLD_START to stall alt job.
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  if (async_quic_session) {
    EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1).WillOnce([this]() {
      job_factory_.main_job()->DoResume();
    });
  }
  // Alt job is stalled and main job should complete successfully.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream().get());
}

TEST_F(HttpStreamFactoryJobControllerTest, ResumeMainJobWhenAltJobStalls) {
  TestResumeMainJobWhenAltJobStalls(false);
}

TEST_F(HttpStreamFactoryJobControllerTest,
       ResumeMainJobWhenAltJobStallsAsyncQuicSession) {
  TestResumeMainJobWhenAltJobStalls(true);
}

TEST_F(HttpStreamFactoryJobControllerTest, InvalidPortForQuic) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  // Using a restricted port 101 for QUIC should fail and the alternative job
  // should post OnStreamFailedCall on the controller to resume the main job.
  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         101);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_factory_.main_job()->is_waiting());

  // Wait until OnStreamFailedCallback is executed on the alternative job.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  base::RunLoop().RunUntilIdle();
}

// Verifies that the main job is not resumed until after the alt job completes
// host resolution.
TEST_F(HttpStreamFactoryJobControllerTest, HostResolutionHang) {
  auto hanging_resolver = std::make_unique<MockHostResolver>();
  hanging_resolver->set_ondemand_mode(true);
  hanging_resolver->rules()->AddRule("www.google.com", "1.2.3.4");
  session_deps_.host_resolver = std::move(hanging_resolver);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data(version_);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);
  ServerNetworkStats stats1;
  stats1.srtt = base::Microseconds(10);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")),
      NetworkAnonymizationKey(), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));

  // Since the alt job has not finished host resolution, there should be no
  // delayed task posted to resume the main job.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::Microseconds(50));
  EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));

  // Allow alt job host resolution to complete.
  session_deps_.host_resolver->ResolveAllPending();

  // Task to resume main job in 15 microseconds should be posted.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::Microseconds(14));
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  FastForwardBy(base::Microseconds(1));

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Unpause mock quic data.
  // `alternative_job` will fail, but its failure should not be reported to
  // Request.
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  EXPECT_TRUE(JobControllerPeer::main_job_is_resumed(job_controller_));
  // OnStreamFailed will post a task to resume the main job immediately but
  // won't call Resume() on the main job since it's been resumed already.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  quic_data.Resume();
  FastForwardUntilNoTasksRemain();

  // Since the main job is hung, the request should still be pending.
  EXPECT_FALSE(request_delegate_->IsDone());

  // Alt job should be cleaned up
  EXPECT_FALSE(job_controller_->alternative_job());
}

// Regression test for crbug.com/789560.
TEST_F(HttpStreamFactoryJobControllerTest, ResumeMainJobLaterCanceled) {
  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateDirect();
  ConfiguredProxyResolutionService* proxy_resolution_service_raw =
      proxy_resolution_service.get();
  session_deps_.proxy_resolution_service = std::move(proxy_resolution_service);

  // Using hanging resolver will cause the alternative job to hang indefinitely.
  session_deps_.alternate_host_resolver =
      std::make_unique<HangingHostResolver>();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // Enable delayed TCP and set time delay for waiting job.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);
  ServerNetworkStats stats1;
  stats1.srtt = base::Microseconds(10);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")),
      NetworkAnonymizationKey(), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());

  base::RunLoop run_loop;
  // The main job should be resumed without delay when alt job fails.
  EXPECT_CALL(*job_factory_.main_job(), Resume())
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });
  job_controller_->OnStreamFailed(job_factory_.alternative_job(),
                                  ERR_QUIC_PROTOCOL_ERROR);
  FastForwardBy(base::Microseconds(0));
  run_loop.Run();
  EXPECT_FALSE(job_controller_->alternative_job());

  // Calling ForceReloadProxyConfig will cause the proxy configuration to
  // change. It will still be the direct connection but the configuration
  // version will be bumped. That is enough for the job controller to restart
  // the jobs.
  proxy_resolution_service_raw->ForceReloadProxyConfig();
  HttpStreamFactoryJobPeer::SetShouldReconsiderProxy(job_factory_.main_job());
  // Now the alt service is marked as broken (e.g. through a different request),
  // so only non-alt job is restarted.
  session_->http_server_properties()->MarkAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey());

  job_controller_->OnStreamFailed(job_factory_.main_job(), ERR_FAILED);
  // Jobs are restarted.
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  // There shouldn't be any ResumeMainJobLater() delayed tasks.
  // This EXPECT_CALL will fail before crbug.com/789560 fix.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::Microseconds(15));

  EXPECT_TRUE(job_controller_->main_job());
  request_.reset();
}

// Test that main job is blocked for kMaxDelayTimeForMainJob(3s) if
// http_server_properties cached an inappropriate large srtt for the server,
// which would potentially delay the main job for a extremely long time in
// delayed tcp case.
TEST_F(HttpStreamFactoryJobControllerTest, DelayedTCPWithLargeSrtt) {
  // The max delay time should be in sync with .cc file.
  base::TimeDelta kMaxDelayTimeForMainJob = base::Seconds(3);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data(version_);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);
  ServerNetworkStats stats1;
  stats1.srtt = base::Seconds(100);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")),
      NetworkAnonymizationKey(), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  base::RunLoop().RunUntilIdle();
  // Main job is not blocked but hasn't resumed yet; it should resume in 3s.
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  EXPECT_FALSE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Task to resume main job in 3 seconds should be posted.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(kMaxDelayTimeForMainJob - base::Microseconds(1));
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  FastForwardBy(base::Microseconds(1));

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Unpause mock quic data and run all remaining tasks. Alt-job  should fail
  // and be cleaned up.
  quic_data.Resume();
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(job_controller_->alternative_job());
}

// TODO(crbug.com/40649375): Disabled because the pending task count does
//                                  not match expectations.
TEST_F(HttpStreamFactoryJobControllerTest,
       DISABLED_ResumeMainJobImmediatelyOnStreamFailed) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data(version_);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);
  ServerNetworkStats stats1;
  stats1.srtt = base::Microseconds(10);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")),
      NetworkAnonymizationKey(), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  // Main job is not blocked but hasn't resumed yet; it's scheduled to resume
  // in 15us.
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  EXPECT_FALSE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Task to resume main job in 15us should be posted.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());

  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::Microseconds(1));

  // Now unpause the mock quic data to fail the alt job. This should immediately
  // resume the main job.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  quic_data.Resume();
  FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Verify there is another task to resume main job with delay but should
  // not call Resume() on the main job as main job has been resumed.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::Microseconds(15));

  FastForwardUntilNoTasksRemain();
}

// When preconnect to a H2 supported server, only 1 connection is opened.
TEST_F(HttpStreamFactoryJobControllerTest,
       PreconnectMultipleStreamsToH2Server) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SetPreconnect();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");
  Initialize(request_info);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, NetworkAnonymizationKey(), true);

  job_controller_->Preconnect(/*num_streams=*/5, base::OnceClosure());
  // Only one job is started.
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_EQ(HttpStreamFactory::PRECONNECT,
            job_controller_->main_job()->job_type());
  // There is only 1 connect even though multiple streams were requested.
  EXPECT_EQ(
      1, HttpStreamFactoryJobPeer::GetNumStreams(job_controller_->main_job()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Check that the logic to only preconnect a single socket to servers with H2
// support respects NetworkIsolationKeys.
TEST_F(HttpStreamFactoryJobControllerTest,
       PreconnectMultipleStreamsToH2ServerWithNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  // It's not strictly necessary to enable
  // `kPartitionConnectionsByNetworkIsolationKey`, but the second phase of the
  // test would only make 4 connections, reusing the first connection, without
  // it.
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Need to re-create HttpServerProperties after enabling the field trial,
  // since it caches the field trial value on construction.
  session_deps_.http_server_properties =
      std::make_unique<HttpServerProperties>();

  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SetPreconnect();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");
  request_info.network_isolation_key = kNetworkIsolationKey1;
  request_info.network_anonymization_key = kNetworkAnonymizationKey1;
  Initialize(request_info);

  // Sets server support HTTP/2, using kNetworkIsolationKey.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, kNetworkAnonymizationKey1, true);

  job_controller_->Preconnect(/*num_streams=*/5, base::OnceClosure());
  // Only one job is started.
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_EQ(HttpStreamFactory::PRECONNECT,
            job_controller_->main_job()->job_type());
  // There is only 1 connect even though multiple streams were requested.
  EXPECT_EQ(
      1, HttpStreamFactoryJobPeer::GetNumStreams(job_controller_->main_job()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // Now try using two different NetworkIsolationKeys, one empty, one not, and
  // make sure that 5 sockets are preconnected with each one.
  std::vector<std::unique_ptr<SequencedSocketData>> socket_data;
  for (auto other_network_isolation_key :
       {NetworkIsolationKey(), kNetworkIsolationKey2}) {
    for (int i = 0; i < 5; ++i) {
      socket_data.emplace_back(std::make_unique<SequencedSocketData>(
          MockConnect(ASYNC, OK), base::span<const MockRead>(),
          base::span<const MockWrite>()));
      session_deps_.socket_factory->AddSocketDataProvider(
          socket_data.back().get());
    }

    request_info.network_isolation_key = other_network_isolation_key;
    request_info.network_anonymization_key =
        NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
            other_network_isolation_key);
    MockHttpStreamRequestDelegate request_delegate;
    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, &request_delegate, session_.get(), &job_factory_,
        request_info, is_preconnect_, /*is_websocket=*/false,
        enable_ip_based_pooling_for_h2_, enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    auto* job_controller_ptr = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
    job_controller_ptr->Preconnect(/*num_streams=*/5, base::OnceClosure());
    // Five jobs should be started.
    EXPECT_TRUE(job_controller_ptr->main_job());
    EXPECT_FALSE(job_controller_ptr->alternative_job());
    EXPECT_EQ(HttpStreamFactory::PRECONNECT,
              job_controller_ptr->main_job()->job_type());
    EXPECT_EQ(5, HttpStreamFactoryJobPeer::GetNumStreams(
                     job_controller_ptr->main_job()));

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  }
}

void HttpStreamFactoryJobControllerTestBase::
    TestDoNotDelayMainJobIfHasAvailableSpdySession(bool async_quic_session) {
  SetAsyncQuicSession(async_quic_session);

  SetNotDelayMainJobWithAvailableSpdySession();
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  // Put a SpdySession in the pool.
  HostPortPair host_port_pair("www.google.com", 443);
  SpdySessionKey key(host_port_pair, PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(), SessionUsage::kDestination,
                     SocketTag(), NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  std::ignore = CreateFakeSpdySession(session_->spdy_session_pool(), key);

  // Handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data(version_);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);
  ServerNetworkStats stats1;
  stats1.srtt = base::Milliseconds(100);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")),
      NetworkAnonymizationKey(), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  // The main job shouldn't have any delay since request can be sent on
  // available SPDY session. When QUIC session creation is async, the main job
  // should still be blocked as alt job has not succeeded or failed at least
  // once yet. Otherwise the main job should not be blocked
  EXPECT_EQ(job_controller_->get_main_job_wait_time_for_tests(),
            base::TimeDelta());
  if (async_quic_session) {
    EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));
    // The main job should have a SPDY session available.
    EXPECT_TRUE(job_controller_->main_job()->HasAvailableSpdySession());
    // Wait for QUIC session creation attempt to resume and unblock the main
    // job.
    FastForwardBy(base::Milliseconds(1));
    // Main job should still have no delay and should be unblocked now.
    EXPECT_EQ(job_controller_->get_main_job_wait_time_for_tests(),
              base::TimeDelta());
    EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  } else {
    EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
    EXPECT_TRUE(job_controller_->main_job()->HasAvailableSpdySession());
  }
}

TEST_F(HttpStreamFactoryJobControllerTest,
       DoNotDelayMainJobIfHasAvailableSpdySession) {
  TestDoNotDelayMainJobIfHasAvailableSpdySession(false);
}

TEST_F(HttpStreamFactoryJobControllerTest,
       DoNotDelayMainJobIfHasAvailableSpdySessionAsyncQuicSession) {
  TestDoNotDelayMainJobIfHasAvailableSpdySession(true);
}

// Check the case that while a preconnect is waiting in the H2 request queue,
// and a SPDY session appears, the job completes successfully.
TEST_F(HttpStreamFactoryJobControllerTest, SpdySessionInterruptsPreconnect) {
  // Make sure there is only one socket connect.
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  tcp_data_ = std::make_unique<SequencedSocketData>(reads, writes);
  // connect needs to be async, so the H2 session isn't created immediately.
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  Initialize(request_info);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, NetworkAnonymizationKey(), true);

  // Start a non-preconnect request.
  std::unique_ptr<HttpStreamRequest> stream_request = job_controller_->Start(
      request_delegate_.get(), nullptr /* websocket_handshake_create_helper */,
      NetLogWithSource(), HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  // Create and start a preconnect request, which should start watching the
  // SpdySessionPool.
  MockHttpStreamRequestDelegate preconnect_request_delegate;
  auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
      factory_, &preconnect_request_delegate, session_.get(), &job_factory_,
      request_info, /*is_preconnect=*/true, /*is_websocket=*/false,
      enable_ip_based_pooling_for_h2_, enable_alternative_services_,
      delay_main_job_with_available_spdy_session_,
      /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
  auto* job_controller_ptr = job_controller.get();
  HttpStreamFactoryPeer::AddJobController(factory_, std::move(job_controller));
  job_controller_ptr->Preconnect(1, base::OnceClosure());
  EXPECT_TRUE(job_controller_ptr->main_job());
  EXPECT_FALSE(job_controller_ptr->alternative_job());

  // The non-preconnect request should create an H2 session, which the
  // preconnect then sees, and the preconnect request should complete and be
  // torn down without ever requesting a socket. If it did request a socket, the
  // test would fail since the mock socket factory would see an unexpected
  // socket request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
  // Let any pending work related to the preconnect request complete, to make
  // sure it doesn't create a second request.
  base::RunLoop().RunUntilIdle();

  stream_request.reset();

  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // Sanity check - make sure the SpdySession was created.
  base::WeakPtr<SpdySession> spdy_session =
      session_->spdy_session_pool()->FindAvailableSession(
          SpdySessionKey(HostPortPair::FromURL(request_info.url),
                         request_info.privacy_mode, ProxyChain::Direct(),
                         SessionUsage::kDestination, request_info.socket_tag,
                         request_info.network_anonymization_key,
                         request_info.secure_dns_policy,
                         /*disable_cert_verification_network_fetches=*/false),
          false /* enable_ip_based_pooling_for_h2 */, /*is_websocket=*/false,
          NetLogWithSource());
  EXPECT_TRUE(spdy_session);
}

// This test verifies that a preconnect job doesn't block subsequent requests
// which can use an existing IP based pooled SpdySession.
// This test uses "wildcard.pem" to support IpBasedPooling for *.example.org,
// and starts 3 requests:
//   [1] Normal non-preconnect request to www.example.org.
//   [2] Preconnect request to other.example.org. The connection is paused until
//       OnConnectComplete() is called in the end of the test.
//   [3] Normal non-preconnect request to other.example.org. This request must
//       succeed even while the preconnect request [2] is paused.
TEST_F(HttpStreamFactoryJobControllerTest,
       PreconnectJobDoesntBlockIpBasedPooling) {
  // Make sure that both "www.example.org" and "other.example.org" are pointing
  // to the same IP address.
  session_deps_.host_resolver->rules()->AddRule(
      "www.example.org", IPAddress::IPv4Localhost().ToString());
  session_deps_.host_resolver->rules()->AddRule(
      "other.example.org", IPAddress::IPv4Localhost().ToString());
  // Make `host_resolver` asynchronous to simulate the issue of
  // crbug.com/1320608.
  session_deps_.host_resolver->set_synchronous_mode(false);

  // This is used for the non-preconnect requests [1] and [3].
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  SequencedSocketData first_socket(reads, writes);
  first_socket.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(&first_socket);

  // This is used for the non-preconnect requests.
  SSLSocketDataProvider ssl_data1(ASYNC, OK);
  ssl_data1.next_proto = NextProto::kProtoHTTP2;
  // "wildcard.pem" supports "*.example.org".
  ssl_data1.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data1);

  // This is used for the preconnect request.
  SequencedSocketData second_socket;
  // The connection is paused. And it will be completed with
  // ERR_CONNECTION_FAILED.
  second_socket.set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  session_deps_.socket_factory->AddSocketDataProvider(&second_socket);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.org");
  Initialize(request_info);

  // Start a non-preconnect request [1].
  {
    std::unique_ptr<HttpStreamRequest> stream_request = job_controller_->Start(
        request_delegate_.get(),
        /*websocket_handshake_stream_create_helper=*/nullptr,
        NetLogWithSource(), HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1).WillOnce([this]() {
      job_factory_.main_job()->DoResume();
    });
    EXPECT_TRUE(request_delegate_->WaitForHttpStream());
  }

  // Sanity check - make sure the SpdySession was created.
  {
    base::WeakPtr<SpdySession> spdy_session =
        session_->spdy_session_pool()->FindAvailableSession(
            SpdySessionKey(HostPortPair::FromURL(request_info.url),
                           request_info.privacy_mode, ProxyChain::Direct(),
                           SessionUsage::kDestination, request_info.socket_tag,
                           request_info.network_anonymization_key,
                           request_info.secure_dns_policy,
                           /*disable_cert_verification_network_fetches=*/false),
            /*enable_ip_based_pooling_for_h2=*/false, /*is_websocket=*/false,
            NetLogWithSource());
    EXPECT_TRUE(spdy_session);
  }

  HttpRequestInfo other_request_info;
  other_request_info.method = "GET";
  other_request_info.url = GURL("https://other.example.org");

  // Create and start a preconnect request [2].
  MockHttpStreamRequestDelegate preconnect_request_delegate;
  auto preconnect_job_controller =
      std::make_unique<HttpStreamFactory::JobController>(
          factory_, &preconnect_request_delegate, session_.get(), &job_factory_,
          other_request_info, /*is_preconnect=*/true,
          /*is_websocket=*/false, /*enable_ip_based_pooling_for_h2=*/true,
          enable_alternative_services_,
          delay_main_job_with_available_spdy_session_,
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
  auto* preconnect_job_controller_ptr = preconnect_job_controller.get();
  HttpStreamFactoryPeer::AddJobController(factory_,
                                          std::move(preconnect_job_controller));
  preconnect_job_controller_ptr->Preconnect(1, base::OnceClosure());
  base::RunLoop().RunUntilIdle();

  // The SpdySession is available for IP based pooling when the host resolution
  // has finished.
  {
    const SpdySessionKey spdy_session_key = SpdySessionKey(
        HostPortPair::FromURL(other_request_info.url),
        other_request_info.privacy_mode, ProxyChain::Direct(),
        SessionUsage::kDestination, other_request_info.socket_tag,
        other_request_info.network_anonymization_key,
        other_request_info.secure_dns_policy,
        /*disable_cert_verification_network_fetches=*/false);
    EXPECT_FALSE(session_->spdy_session_pool()->FindAvailableSession(
        spdy_session_key, /*enable_ip_based_pooling_for_h2=*/false,
        /*is_websocket=*/false, NetLogWithSource()));
    EXPECT_TRUE(session_->spdy_session_pool()->FindAvailableSession(
        spdy_session_key, /*enable_ip_based_pooling_for_h2=*/true,
        /*is_websocket=*/false, NetLogWithSource()));
  }

  // Create and start a second non-preconnect request [3].
  {
    MockHttpStreamRequestDelegate request_delegate;
    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, &request_delegate, session_.get(), &job_factory_,
        other_request_info, /*is_preconnect=*/false,
        /*is_websocket=*/false, /*enable_ip_based_pooling_for_h2=*/true,
        enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    auto* job_controller_ptr = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
    std::unique_ptr<HttpStreamRequest> second_stream_request =
        job_controller_ptr->Start(
            &request_delegate,
            /*websocket_handshake_stream_create_helper=*/nullptr,
            NetLogWithSource(), HttpStreamRequest::HTTP_STREAM,
            DEFAULT_PRIORITY);

    EXPECT_TRUE(request_delegate.WaitForHttpStream());
    second_stream_request.reset();
  }

  second_socket.socket()->OnConnectComplete(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  EXPECT_TRUE(first_socket.AllReadDataConsumed());
  EXPECT_TRUE(first_socket.AllWriteDataConsumed());
}

class JobControllerLimitMultipleH2Requests
    : public HttpStreamFactoryJobControllerTestBase {
 protected:
  JobControllerLimitMultipleH2Requests()
      : HttpStreamFactoryJobControllerTestBase(
            /*happy_eyeballs_v3_enabled=*/false) {}
  const int kNumRequests = 5;
  void SetUp() override { SkipCreatingJobController(); }
};

TEST_F(JobControllerLimitMultipleH2Requests, MultipleRequests) {
  // Make sure there is only one socket connect.
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  tcp_data_ =
      std::make_unique<SequencedSocketData>(reads, base::span<MockWrite>());
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  Initialize(request_info);
  SpdySessionPoolPeer pool_peer(session_->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, NetworkAnonymizationKey(), true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  for (int i = 0; i < kNumRequests; ++i) {
    request_delegates.emplace_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, request_delegates[i].get(), session_.get(), &job_factory_,
        request_info, is_preconnect_, /*is_websocket=*/false,
        enable_ip_based_pooling_for_h2_, enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    auto* job_controller_ptr = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
    auto request = job_controller_ptr->Start(
        request_delegates[i].get(), nullptr, net_log_with_source_,
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_TRUE(job_controller_ptr->main_job());
    EXPECT_FALSE(job_controller_ptr->alternative_job());
    requests.push_back(std::move(request));
  }

  for (int i = 0; i < kNumRequests; ++i) {
    EXPECT_TRUE(request_delegates[i]->WaitForHttpStream());
  }

  requests.clear();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  auto entries = net_log_observer_.GetEntries();
  size_t log_position = 0;
  for (int i = 0; i < kNumRequests - 1; ++i) {
    log_position = ExpectLogContainsSomewhereAfter(
        entries, log_position, NetLogEventType::HTTP_STREAM_JOB_THROTTLED,
        NetLogEventPhase::NONE);
  }
}

// Check that throttling simultaneous requests to a single H2 server respects
// NetworkIsolationKeys.
TEST_F(JobControllerLimitMultipleH2Requests,
       MultipleRequestsNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Need to re-create HttpServerProperties after enabling the field trial,
  // since it caches the field trial value on construction.
  session_deps_.http_server_properties =
      std::make_unique<HttpServerProperties>();

  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey2(kSite2, kSite2);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);

  tcp_data_ = std::make_unique<SequencedSocketData>(
      MockConnect(SYNCHRONOUS, ERR_IO_PENDING), base::span<MockRead>(),
      base::span<MockWrite>());
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  Initialize(request_info);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, kNetworkAnonymizationKey1, true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  std::vector<std::unique_ptr<SequencedSocketData>> socket_data;
  for (int i = 0; i < kNumRequests; ++i) {
    // Shouldn't matter whether requests are interleaved by NetworkIsolationKey
    // or not.
    for (const auto& network_isolation_key :
         {NetworkIsolationKey(), kNetworkIsolationKey1,
          kNetworkIsolationKey2}) {
      request_info.network_isolation_key = network_isolation_key;
      request_info.network_anonymization_key =
          NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
              network_isolation_key);
      // For kNetworkIsolationKey1, all requests but the first will be
      // throttled.
      if (i == 0 || network_isolation_key != kNetworkIsolationKey1) {
        socket_data.emplace_back(std::make_unique<SequencedSocketData>(
            MockConnect(ASYNC, OK), base::span<const MockRead>(),
            base::span<const MockWrite>()));
        session_deps_.socket_factory->AddSocketDataProvider(
            socket_data.back().get());
      }
      request_delegates.emplace_back(
          std::make_unique<MockHttpStreamRequestDelegate>());
      auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
          factory_, request_delegates[i].get(), session_.get(), &job_factory_,
          request_info, is_preconnect_, /*is_websocket=*/false,
          enable_ip_based_pooling_for_h2_, enable_alternative_services_,
          delay_main_job_with_available_spdy_session_,
          /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
      auto* job_controller_ptr = job_controller.get();
      HttpStreamFactoryPeer::AddJobController(factory_,
                                              std::move(job_controller));
      auto request = job_controller_ptr->Start(
          request_delegates[i].get(), nullptr, net_log_with_source_,
          HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
      EXPECT_TRUE(job_controller_ptr->main_job());
      EXPECT_FALSE(job_controller_ptr->alternative_job());
      requests.push_back(std::move(request));
    }
  }
  TransportClientSocketPool* socket_pool =
      reinterpret_cast<TransportClientSocketPool*>(session_->GetSocketPool(
          HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct()));
  ClientSocketPool::GroupId group_id0(
      url::SchemeHostPort(request_info.url), request_info.privacy_mode,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false);
  ClientSocketPool::GroupId group_id1(
      url::SchemeHostPort(request_info.url), request_info.privacy_mode,
      kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false);
  ClientSocketPool::GroupId group_id2(
      url::SchemeHostPort(request_info.url), request_info.privacy_mode,
      kNetworkAnonymizationKey2, SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false);
  EXPECT_EQ(static_cast<uint32_t>(kNumRequests),
            socket_pool->NumConnectJobsInGroupForTesting(group_id0));
  EXPECT_EQ(1u, socket_pool->NumConnectJobsInGroupForTesting(group_id1));
  EXPECT_EQ(static_cast<uint32_t>(kNumRequests),
            socket_pool->NumConnectJobsInGroupForTesting(group_id2));
}

TEST_F(JobControllerLimitMultipleH2Requests, MultipleRequestsFirstRequestHang) {
  // First socket connect hang.
  SequencedSocketData hangdata;
  hangdata.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  session_deps_.socket_factory->AddSocketDataProvider(&hangdata);
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  std::list<SequencedSocketData> socket_data;
  std::list<SSLSocketDataProvider> ssl_socket_data;
  // kNumRequests - 1 will resume themselves after a delay. There will be
  // kNumRequests - 1 sockets opened.
  for (int i = 0; i < kNumRequests - 1; i++) {
    // Only the first one needs a MockRead because subsequent sockets are
    // not used to establish a SpdySession.
    if (i == 0) {
      socket_data.emplace_back(reads, base::span<MockWrite>());
    } else {
      socket_data.emplace_back();
    }
    socket_data.back().set_connect_data(MockConnect(ASYNC, OK));
    session_deps_.socket_factory->AddSocketDataProvider(&socket_data.back());
    ssl_socket_data.emplace_back(ASYNC, OK);
    ssl_socket_data.back().next_proto = NextProto::kProtoHTTP2;
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        &ssl_socket_data.back());
  }
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  Initialize(request_info);
  SpdySessionPoolPeer pool_peer(session_->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, NetworkAnonymizationKey(), true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  for (int i = 0; i < kNumRequests; ++i) {
    request_delegates.push_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, request_delegates[i].get(), session_.get(), &job_factory_,
        request_info, is_preconnect_, /*is_websocket=*/false,
        enable_ip_based_pooling_for_h2_, enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    auto* job_controller_ptr = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
    auto request = job_controller_ptr->Start(
        request_delegates[i].get(), nullptr, net_log_with_source_,
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_TRUE(job_controller_ptr->main_job());
    EXPECT_FALSE(job_controller_ptr->alternative_job());
    requests.push_back(std::move(request));
  }

  EXPECT_GT(GetPendingMainThreadTaskCount(), 0u);
  FastForwardBy(base::Milliseconds(HttpStreamFactory::Job::kHTTP2ThrottleMs));

  for (int i = 0; i < kNumRequests; ++i) {
    EXPECT_TRUE(request_delegates[i]->WaitForHttpStream());
  }

  EXPECT_FALSE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  requests.clear();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  EXPECT_TRUE(hangdata.AllReadDataConsumed());
  for (const auto& data : socket_data) {
    EXPECT_TRUE(data.AllReadDataConsumed());
    EXPECT_TRUE(data.AllWriteDataConsumed());
  }
}

TEST_F(JobControllerLimitMultipleH2Requests,
       MultipleRequestsFirstRequestCanceled) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  SequencedSocketData first_socket(reads, base::span<MockWrite>());
  first_socket.set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider first_ssl_data(ASYNC, OK);
  first_ssl_data.next_proto = NextProto::kProtoHTTP2;
  session_deps_.socket_factory->AddSocketDataProvider(&first_socket);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&first_ssl_data);
  std::list<SequencedSocketData> socket_data;
  std::list<SSLSocketDataProvider> ssl_socket_data;
  // kNumRequests - 1 will be resumed when the first request is canceled.
  for (int i = 0; i < kNumRequests - 1; i++) {
    socket_data.emplace_back();
    socket_data.back().set_connect_data(MockConnect(ASYNC, OK));
    session_deps_.socket_factory->AddSocketDataProvider(&socket_data.back());
    ssl_socket_data.emplace_back(ASYNC, OK);
    ssl_socket_data.back().next_proto = NextProto::kProtoHTTP2;
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        &ssl_socket_data.back());
  }

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  Initialize(request_info);
  SpdySessionPoolPeer pool_peer(session_->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, NetworkAnonymizationKey(), true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  for (int i = 0; i < kNumRequests; ++i) {
    request_delegates.emplace_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, request_delegates[i].get(), session_.get(), &job_factory_,
        request_info, is_preconnect_, /*is_websocket=*/false,
        enable_ip_based_pooling_for_h2_, enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    auto* job_controller_ptr = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
    auto request = job_controller_ptr->Start(
        request_delegates[i].get(), nullptr, net_log_with_source_,
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_TRUE(job_controller_ptr->main_job());
    EXPECT_FALSE(job_controller_ptr->alternative_job());
    requests.push_back(std::move(request));
  }
  // Cancel the first one.
  requests[0].reset();

  for (int i = 1; i < kNumRequests; ++i) {
    EXPECT_TRUE(request_delegates[i]->WaitForHttpStream());
  }

  EXPECT_FALSE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  requests.clear();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  EXPECT_TRUE(first_socket.AllReadDataConsumed());
  for (const auto& data : socket_data) {
    EXPECT_TRUE(data.AllReadDataConsumed());
    EXPECT_TRUE(data.AllWriteDataConsumed());
  }
}

TEST_F(JobControllerLimitMultipleH2Requests, MultiplePreconnects) {
  // Make sure there is only one socket connect.
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  SetPreconnect();
  Initialize(request_info);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, NetworkAnonymizationKey(), true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  for (int i = 0; i < kNumRequests; ++i) {
    request_delegates.emplace_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, request_delegates[i].get(), session_.get(), &job_factory_,
        request_info, is_preconnect_, /*is_websocket=*/false,
        enable_ip_based_pooling_for_h2_, enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    auto* job_controller_ptr = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
    job_controller_ptr->Preconnect(1, base::OnceClosure());
    EXPECT_TRUE(job_controller_ptr->main_job());
    EXPECT_FALSE(job_controller_ptr->alternative_job());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(JobControllerLimitMultipleH2Requests, H1NegotiatedForFirstRequest) {
  // First socket is an HTTP/1.1 socket.
  SequencedSocketData first_socket;
  first_socket.set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSocketDataProvider(&first_socket);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  // Second socket is an HTTP/2 socket.
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  SequencedSocketData second_socket(reads, base::span<MockWrite>());
  second_socket.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(&second_socket);
  SSLSocketDataProvider second_ssl_data(ASYNC, OK);
  second_ssl_data.next_proto = NextProto::kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&second_ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  Initialize(request_info);
  SpdySessionPoolPeer pool_peer(session_->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(
      server, NetworkAnonymizationKey(), true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  for (int i = 0; i < 2; ++i) {
    request_delegates.emplace_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, request_delegates[i].get(), session_.get(), &job_factory_,
        request_info, is_preconnect_, /*is_websocket=*/false,
        enable_ip_based_pooling_for_h2_, enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    auto* job_controller_ptr = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
    auto request = job_controller_ptr->Start(
        request_delegates[i].get(), nullptr, net_log_with_source_,
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_TRUE(job_controller_ptr->main_job());
    EXPECT_FALSE(job_controller_ptr->alternative_job());
    requests.push_back(std::move(request));
  }

  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(request_delegates[i]->WaitForHttpStream());
  }

  EXPECT_FALSE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  requests.clear();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  EXPECT_TRUE(first_socket.AllReadDataConsumed());
  EXPECT_FALSE(second_socket.AllReadDataConsumed());
}

// Tests that HTTP/2 throttling logic only applies to non-QUIC jobs.
TEST_F(JobControllerLimitMultipleH2Requests, QuicJobNotThrottled) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  quic_data_ = std::make_unique<MockQuicData>(version_);
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  tcp_data_ =
      std::make_unique<SequencedSocketData>(reads, base::span<MockWrite>());

  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  SpdySessionPoolPeer pool_peer(session_->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);

  url::SchemeHostPort server(request_info.url);
  // Sets server supports QUIC.
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  // Sets server support HTTP/2.
  session_->http_server_properties()->SetSupportsSpdy(
      server, NetworkAnonymizationKey(), true);

  // Use default job factory so that Resume() is not mocked out.
  HttpStreamFactory::JobFactory default_job_factory;
  auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
      factory_, request_delegate_.get(), session_.get(), &default_job_factory,
      request_info, is_preconnect_, /*is_websocket=*/false,
      enable_ip_based_pooling_for_h2_, enable_alternative_services_,
      delay_main_job_with_available_spdy_session_,
      /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
  auto* job_controller_ptr = job_controller.get();
  HttpStreamFactoryPeer::AddJobController(factory_, std::move(job_controller));
  request_ = job_controller_ptr->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_ptr->main_job());
  EXPECT_TRUE(job_controller_ptr->alternative_job());
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
  auto entries = net_log_observer_.GetEntries();
  for (const auto& entry : entries) {
    ASSERT_NE(NetLogEventType::HTTP_STREAM_JOB_THROTTLED, entry.type);
  }
}

class HttpStreamFactoryJobControllerMisdirectedRequestRetry
    : public HttpStreamFactoryJobControllerTestBase,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool>> {
 public:
  HttpStreamFactoryJobControllerMisdirectedRequestRetry()
      : HttpStreamFactoryJobControllerTestBase(
            /*happy_eyeballs_v3_enabled=*/false) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         HttpStreamFactoryJobControllerMisdirectedRequestRetry,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST_P(HttpStreamFactoryJobControllerMisdirectedRequestRetry,
       DisableIPBasedPoolingForH2AndAlternativeServices) {
  const bool enable_ip_based_pooling_for_h2 = ::testing::get<0>(GetParam());
  const bool enable_alternative_services = ::testing::get<1>(GetParam());
  if (enable_alternative_services) {
    quic_data_ = std::make_unique<MockQuicData>(version_);
    quic_data_->AddConnect(SYNCHRONOUS, OK);
    quic_data_->AddWrite(SYNCHRONOUS,
                         client_maker_.MakeInitialSettingsPacket(1));
    quic_data_->AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  }
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  if (!enable_ip_based_pooling_for_h2) {
    DisableIPBasedPoolingForH2();
  }
  SetEnableAlternativeServices(enable_alternative_services);

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  request_ = job_controller_->Start(
      request_delegate_.get(), nullptr, net_log_with_source_,
      HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  if (enable_alternative_services) {
    EXPECT_TRUE(job_controller_->alternative_job());
  } else {
    EXPECT_FALSE(job_controller_->alternative_job());
  }

  // `main_job` succeeds and should report status to Request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
}

class HttpStreamFactoryJobControllerPreconnectTest
    : public HttpStreamFactoryJobControllerTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  HttpStreamFactoryJobControllerPreconnectTest()
      : HttpStreamFactoryJobControllerTestBase(
            /*happy_eyeballs_v3_enabled=*/false) {}

  void SetUp() override {
    if (!GetParam()) {
      scoped_feature_list_.InitFromCommandLine(std::string(),
                                               "LimitEarlyPreconnects");
    }
  }

  void Initialize() {
    session_deps_.http_server_properties =
        std::make_unique<HttpServerProperties>(
            std::make_unique<MockPrefDelegate>(), nullptr /* net_log */);
    session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    factory_ = session_->http_stream_factory();
    request_info_.method = "GET";
    request_info_.url = GURL("https://www.example.com");
    auto job_controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, request_delegate_.get(), session_.get(), &job_factory_,
        request_info_, /* is_preconnect = */ true,
        /* is_websocket = */ false,
        /* enable_ip_based_pooling_for_h2 = */ true,
        /* enable_alternative_services = */ true,
        /* delay_main_job_with_available_spdy_session = */ true,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    job_controller_ = job_controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_,
                                            std::move(job_controller));
  }

 protected:
  void Preconnect(int num_streams) {
    job_controller_->Preconnect(num_streams, base::OnceClosure());
    // Only one job is started.
    EXPECT_TRUE(job_controller_->main_job());
    EXPECT_FALSE(job_controller_->alternative_job());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  HttpRequestInfo request_info_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HttpStreamFactoryJobControllerPreconnectTest,
                         ::testing::Bool());

TEST_P(HttpStreamFactoryJobControllerPreconnectTest, LimitEarlyPreconnects) {
  std::list<SequencedSocketData> providers;
  std::list<SSLSocketDataProvider> ssl_providers;
  const int kNumPreconects = 5;
  MockRead reads[] = {MockRead(ASYNC, OK)};
  // If experiment is not enabled, there are 5 socket connects.
  const size_t actual_num_connects = GetParam() ? 1 : kNumPreconects;
  for (size_t i = 0; i < actual_num_connects; ++i) {
    providers.emplace_back(reads, base::span<MockWrite>());
    session_deps_.socket_factory->AddSocketDataProvider(&providers.back());
    ssl_providers.emplace_back(ASYNC, OK);
    session_deps_.socket_factory->AddSSLSocketDataProvider(
        &ssl_providers.back());
  }
  Initialize();
  Preconnect(kNumPreconects);
  // If experiment is enabled, only 1 stream is requested.
  EXPECT_EQ((int)actual_num_connects, HttpStreamFactoryJobPeer::GetNumStreams(
                                          job_controller_->main_job()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Test that GetAlternativeServiceInfoFor will include a list of advertised
// versions, which contains a version that is supported. Returns an empty list
// if advertised versions are missing in HttpServerProperties.
TEST_F(HttpStreamFactoryJobControllerTest, GetAlternativeServiceInfoFor) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  base::Time expiration = base::Time::Now() + base::Days(1);

  // Set alternative service with no advertised version.
  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      quic::ParsedQuicVersionVector());

  // Simulate proxy resolution succeeding, after which
  // GetAlternativeServiceInfoFor can be called.
  JobControllerPeer::InitializeProxyInfo(job_controller_);

  AlternativeServiceInfo alt_svc_info =
      JobControllerPeer::GetAlternativeServiceInfoFor(
          job_controller_, request_info, request_delegate_.get(),
          HttpStreamRequest::HTTP_STREAM);
  // Verify that JobController get an empty list of supported QUIC versions.
  EXPECT_TRUE(alt_svc_info.advertised_versions().empty());

  // Set alternative service for the same server with the same list of versions
  // that is supported.
  quic::ParsedQuicVersionVector supported_versions =
      quic_context_.params()->supported_versions;
  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      supported_versions);

  alt_svc_info = JobControllerPeer::GetAlternativeServiceInfoFor(
      job_controller_, request_info, request_delegate_.get(),
      HttpStreamRequest::HTTP_STREAM);
  std::sort(
      supported_versions.begin(), supported_versions.end(),
      [](const quic::ParsedQuicVersion& a, const quic::ParsedQuicVersion& b) {
        return a.transport_version < b.transport_version;
      });
  quic::ParsedQuicVersionVector advertised_versions =
      alt_svc_info.advertised_versions();
  std::sort(
      advertised_versions.begin(), advertised_versions.end(),
      [](const quic::ParsedQuicVersion& a, const quic::ParsedQuicVersion& b) {
        return a.transport_version < b.transport_version;
      });
  EXPECT_EQ(supported_versions, advertised_versions);

  quic::ParsedQuicVersion unsupported_version_1 =
      quic::ParsedQuicVersion::Unsupported();
  quic::ParsedQuicVersion unsupported_version_2 =
      quic::ParsedQuicVersion::Unsupported();
  for (const quic::ParsedQuicVersion& version : quic::AllSupportedVersions()) {
    if (base::Contains(supported_versions, version)) {
      continue;
    }
    if (unsupported_version_1 == quic::ParsedQuicVersion::Unsupported()) {
      unsupported_version_1 = version;
      continue;
    }
    unsupported_version_2 = version;
    break;
  }

  // Set alternative service for the same server with two QUIC versions:
  // - one unsupported version: `unsupported_version_1`,
  // - one supported version:
  // quic_context_.params()->supported_versions[0].
  quic::ParsedQuicVersionVector mixed_quic_versions = {
      unsupported_version_1, quic_context_.params()->supported_versions[0]};
  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      mixed_quic_versions);

  alt_svc_info = JobControllerPeer::GetAlternativeServiceInfoFor(
      job_controller_, request_info, request_delegate_.get(),
      HttpStreamRequest::HTTP_STREAM);
  EXPECT_EQ(2u, alt_svc_info.advertised_versions().size());
  // Verify that JobController returns the list of versions specified in set.
  EXPECT_EQ(mixed_quic_versions, alt_svc_info.advertised_versions());

  // Set alternative service for the same server with two unsupported QUIC
  // versions: `unsupported_version_1`, `unsupported_version_2`.
  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(), alternative_service, expiration,
      {unsupported_version_1, unsupported_version_2});

  alt_svc_info = JobControllerPeer::GetAlternativeServiceInfoFor(
      job_controller_, request_info, request_delegate_.get(),
      HttpStreamRequest::HTTP_STREAM);
  // Verify that JobController returns no valid alternative service.
  EXPECT_EQ(NextProto::kProtoUnknown,
            alt_svc_info.alternative_service().protocol);
  EXPECT_EQ(0u, alt_svc_info.advertised_versions().size());
}

void HttpStreamFactoryJobControllerTestBase::TestAltSvcVersionSelection(
    const std::string& alt_svc_header,
    const quic::ParsedQuicVersion& expected_version,
    const quic::ParsedQuicVersionVector& supported_versions) {
  quic_context_.params()->supported_versions = supported_versions;
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://example.com");
  NetworkIsolationKey network_isolation_key(
      SchemefulSite(GURL("https://example.com")),
      SchemefulSite(GURL("https://example.com")));
  auto network_anonymization_key = NetworkAnonymizationKey::CreateSameSite(
      SchemefulSite(GURL("https://example.com")));
  request_info.network_isolation_key = network_isolation_key;
  request_info.network_anonymization_key = network_anonymization_key;

  Initialize(request_info);
  url::SchemeHostPort origin(request_info.url);
  auto headers = base::MakeRefCounted<HttpResponseHeaders>("");
  headers->AddHeader("alt-svc", alt_svc_header);
  session_->http_stream_factory()->ProcessAlternativeServices(
      session_.get(), network_anonymization_key, headers.get(), origin);
  // Simulate proxy resolution succeeding, after which
  // GetAlternativeServiceInfoFor can be called.
  JobControllerPeer::InitializeProxyInfo(job_controller_);
  AlternativeServiceInfo alt_svc_info =
      JobControllerPeer::GetAlternativeServiceInfoFor(
          job_controller_, request_info, request_delegate_.get(),
          HttpStreamRequest::HTTP_STREAM);
  quic::ParsedQuicVersionVector advertised_versions =
      alt_svc_info.advertised_versions();
  quic::ParsedQuicVersion selected_version =
      JobControllerPeer::SelectQuicVersion(job_controller_,
                                           advertised_versions);
  EXPECT_EQ(expected_version, selected_version)
      << alt_svc_info.ToString() << " "
      << quic::ParsedQuicVersionVectorToString(advertised_versions);
}

TEST_F(HttpStreamFactoryJobControllerTest,
       AltSvcVersionSelectionFindsFirstMatch) {
  TestAltSvcVersionSelection(
      "h3-Q050=\":443\"; ma=2592000,"
      "h3-Q049=\":443\"; ma=2592000,"
      "h3-Q048=\":443\"; ma=2592000,"
      "h3-Q046=\":443\"; ma=2592000,",
      quic::ParsedQuicVersion::Q046(), quic::AllSupportedVersions());
}

TEST_F(HttpStreamFactoryJobControllerTest,
       AltSvcVersionSelectionFindsFirstMatchInverse) {
  TestAltSvcVersionSelection(
      "h3-Q046=\":443\"; ma=2592000,"
      "h3-Q048=\":443\"; ma=2592000,"
      "h3-Q049=\":443\"; ma=2592000,",
      quic::ParsedQuicVersion::Q046(), quic::AllSupportedVersions());
}

TEST_F(HttpStreamFactoryJobControllerTest,
       AltSvcVersionSelectionWithInverseOrderingNewFormat) {
  // Server prefers Q046 but client prefers Q050.
  TestAltSvcVersionSelection(
      "h3-Q046=\":443\"; ma=2592000,"
      "h3-Q050=\":443\"; ma=2592000",
      quic::ParsedQuicVersion::Q046(),
      quic::ParsedQuicVersionVector{quic::ParsedQuicVersion::Q046()});
}

// Tests that if HttpNetworkSession has a non-empty QUIC host allowlist,
// then GetAlternativeServiceFor() will not return any QUIC alternative service
// that's not on the allowlist.
TEST_F(HttpStreamFactoryJobControllerTest, QuicHostAllowlist) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // Set HttpNetworkSession's QUIC host allowlist to only have www.example.com
  HttpNetworkSessionPeer session_peer(session_.get());
  session_peer.params()->quic_host_allowlist.insert("www.example.com");
  quic_context_.params()->allow_remote_alt_svc = true;

  // Set alternative service for www.google.com to be www.example.com over QUIC.
  url::SchemeHostPort server(request_info.url);
  base::Time expiration = base::Time::Now() + base::Days(1);
  quic::ParsedQuicVersionVector supported_versions =
      quic_context_.params()->supported_versions;
  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(),
      AlternativeService(NextProto::kProtoQUIC, "www.example.com", 443),
      expiration, supported_versions);

  // Simulate proxy resolution succeeding, after which
  // GetAlternativeServiceInfoFor can be called.
  JobControllerPeer::InitializeProxyInfo(job_controller_);

  AlternativeServiceInfo alt_svc_info =
      JobControllerPeer::GetAlternativeServiceInfoFor(
          job_controller_, request_info, request_delegate_.get(),
          HttpStreamRequest::HTTP_STREAM);

  std::sort(
      supported_versions.begin(), supported_versions.end(),
      [](const quic::ParsedQuicVersion& a, const quic::ParsedQuicVersion& b) {
        return a.transport_version < b.transport_version;
      });
  quic::ParsedQuicVersionVector advertised_versions =
      alt_svc_info.advertised_versions();
  std::sort(
      advertised_versions.begin(), advertised_versions.end(),
      [](const quic::ParsedQuicVersion& a, const quic::ParsedQuicVersion& b) {
        return a.transport_version < b.transport_version;
      });
  EXPECT_EQ(NextProto::kProtoQUIC, alt_svc_info.alternative_service().protocol);
  EXPECT_EQ(supported_versions, advertised_versions);

  session_->http_server_properties()->SetQuicAlternativeService(
      server, NetworkAnonymizationKey(),
      AlternativeService(NextProto::kProtoQUIC, "www.example.org", 443),
      expiration, supported_versions);

  alt_svc_info = JobControllerPeer::GetAlternativeServiceInfoFor(
      job_controller_, request_info, request_delegate_.get(),
      HttpStreamRequest::HTTP_STREAM);

  EXPECT_EQ(NextProto::kProtoUnknown,
            alt_svc_info.alternative_service().protocol);
  EXPECT_EQ(0u, alt_svc_info.advertised_versions().size());
}

// Tests specific to UseDnsHttpsAlpn feature.
class HttpStreamFactoryJobControllerDnsHttpsAlpnTest
    : public HttpStreamFactoryJobControllerTestBase {
 protected:
  explicit HttpStreamFactoryJobControllerDnsHttpsAlpnTest(
      std::vector<base::test::FeatureRef> enabled_features = {})
      : HttpStreamFactoryJobControllerTestBase(
            /*happy_eyeballs_v3_enabled=*/false,
            std::move(enabled_features)) {}

  void SetUp() override { SkipCreatingJobController(); }

  void EnableOndemandHostResolver() {
    session_deps_.host_resolver->set_synchronous_mode(false);
    session_deps_.host_resolver->set_ondemand_mode(true);
  }

  HttpRequestInfo CreateTestHttpRequestInfo() {
    HttpRequestInfo request_info;
    request_info.method = "GET";
    request_info.url = GURL("https://www.example.org");
    return request_info;
  }

  void RegisterMockHttpsRecord() {
    HostResolverEndpointResult endpoint_result1;
    endpoint_result1.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
    endpoint_result1.metadata.supported_protocol_alpns = {
        quic::AlpnForVersion(version_)};

    HostResolverEndpointResult endpoint_result2;
    endpoint_result2.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};

    std::vector<HostResolverEndpointResult> endpoints;
    endpoints.push_back(endpoint_result1);
    endpoints.push_back(endpoint_result2);
    session_deps_.host_resolver->rules()->AddRule(
        "www.example.org",
        MockHostResolverBase::RuleResolver::RuleResult(
            std::move(endpoints),
            /*aliases=*/std::set<std::string>{"www.example.org"}));
  }

  void CreateJobController(const HttpRequestInfo& request_info) {
    CreateJobControllerImpl(&job_controller_, request_delegate_.get(),
                            request_info);
  }

  std::unique_ptr<HttpStreamRequest> CreateJobControllerAndStart(
      const HttpRequestInfo& request_info) {
    return CreateJobControllerAndStartImpl(
        &job_controller_, request_delegate_.get(), request_info);
  }

  std::unique_ptr<HttpStreamRequest> CreateSecondJobControllerAndStart(
      const HttpRequestInfo& request_info) {
    return CreateJobControllerAndStartImpl(&job_controller2_,
                                           &request_delegate2_, request_info);
  }

  void PrepareForMainJob() { PrepareForMainJobImpl(&tcp_data_, &ssl_data_); }
  void PrepareForSecondMainJob() {
    PrepareForMainJobImpl(&tcp_data2_, &ssl_data2_);
  }

  void PrepareForFirstQuicJob() { PrepareForQuicJobImpl(&quic_data_); }
  void PrepareForSecondQuicJob() { PrepareForQuicJobImpl(&quic_data2_); }

  void PrepareForFirstQuicJobFailure() {
    PrepareForQuicJobFailureImpl(&quic_data_);
  }
  void PrepareForSecondQuicJobFailure() {
    PrepareForQuicJobFailureImpl(&quic_data2_);
  }

  void MakeMainJobSucceed(bool expect_stream_ready) {
    MakeMainJobSucceedImpl(*request_delegate_, tcp_data_.get(),
                           expect_stream_ready);
  }

  void MakeSecondMainJobSucceed(bool expect_stream_ready) {
    MakeMainJobSucceedImpl(request_delegate2_, tcp_data2_.get(),
                           expect_stream_ready);
  }

  void MakeQuicJobSucceed(size_t index, bool expect_stream_ready) {
    base::RunLoop().RunUntilIdle();
    ASSERT_GT(crypto_client_stream_factory_.streams().size(), index);
    MockCryptoClientStream* stream =
        crypto_client_stream_factory_.streams()[index].get();
    ASSERT_TRUE(stream);

    if (expect_stream_ready) {
      stream->NotifySessionOneRttKeyAvailable();
      EXPECT_TRUE(request_delegate_->WaitForHttpStream());
    } else {
      bool is_done = request_delegate_->IsDone();
      stream->NotifySessionOneRttKeyAvailable();
      base::RunLoop().RunUntilIdle();
      // Done state should not change.
      EXPECT_EQ(is_done, request_delegate_->IsDone());
    }
  }

  void CheckJobsStatus(bool main_job_exists,
                       bool alternative_job_exists,
                       bool dns_alpn_h3_job_exists,
                       const std::string& scoped_trace_message = "") {
    CheckJobsStatusImpl(job_controller_.get(), main_job_exists,
                        alternative_job_exists, dns_alpn_h3_job_exists,
                        scoped_trace_message);
  }

  void CheckSecondJobsStatus(bool main_job_exists,
                             bool alternative_job_exists,
                             bool dns_alpn_h3_job_exists,
                             const std::string& scoped_trace_message = "") {
    CheckJobsStatusImpl(job_controller2_.get(), main_job_exists,
                        alternative_job_exists, dns_alpn_h3_job_exists,
                        scoped_trace_message);
  }

  std::unique_ptr<QuicHttpStream> ConnectQuicHttpStream(
      bool alt_destination,
      bool require_dns_https_alpn) {
    NetErrorDetails net_error_details;
    QuicSessionRequest quic_request(session_->quic_session_pool());
    url::SchemeHostPort scheme_host_port(
        url::kHttpsScheme,
        alt_destination ? "alt.example.org" : "www.example.org", 443);
    std::optional<int> quic_request_result;

    CHECK_EQ(ERR_IO_PENDING,
             quic_request.Request(
                 scheme_host_port,
                 require_dns_https_alpn ? quic::ParsedQuicVersion::Unsupported()
                                        : version_,
                 ProxyChain::Direct(), TRAFFIC_ANNOTATION_FOR_TESTS,
                 /*http_user_agent_settings=*/nullptr,
                 SessionUsage::kDestination, PRIVACY_MODE_DISABLED,
                 DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
                 SecureDnsPolicy::kAllow, require_dns_https_alpn,
                 /*cert_verify_flags=*/0, GURL("https://www.example.org/"),
                 net_log_with_source_, &net_error_details,
                 MultiplexedSessionCreationInitiator::kUnknown, std::nullopt,
                 base::BindLambdaForTesting([&](int result) {}),
                 base::BindLambdaForTesting([&quic_request_result](int result) {
                   quic_request_result = result;
                 })));
    base::RunLoop().RunUntilIdle();
    CHECK_EQ(1u, crypto_client_stream_factory_.streams().size());
    CHECK(crypto_client_stream_factory_.streams()[0]);
    crypto_client_stream_factory_.streams()[0]
        ->NotifySessionOneRttKeyAvailable();
    base::RunLoop().RunUntilIdle();
    CHECK(quic_request_result);
    CHECK_EQ(OK, *quic_request_result);

    std::unique_ptr<QuicChromiumClientSession::Handle> session =
        quic_request.ReleaseSessionHandle();
    std::set<std::string> dns_aliases =
        session->GetDnsAliasesForSessionKey(quic_request.session_key());
    auto stream = std::make_unique<QuicHttpStream>(std::move(session),
                                                   std::move(dns_aliases));
    return stream;
  }

  bool IsAlternativeServiceBroken(GURL& url) {
    return session_->http_server_properties()->IsAlternativeServiceBroken(
        AlternativeService(NextProto::kProtoQUIC, HostPortPair::FromURL(url)),
        NetworkAnonymizationKey());
  }

  raw_ptr<HttpStreamFactory::JobController, AcrossTasksDanglingUntriaged>
      job_controller2_ = nullptr;

  MockHttpStreamRequestDelegate request_delegate2_;

 private:
  QuicTestPacketMaker CreateQuicTestPacketMakerForClient() {
    return QuicTestPacketMaker(version_,
                               quic::QuicUtils::CreateRandomConnectionId(
                                   quic_context_.random_generator()),
                               quic_context_.clock(), "www.example.org",
                               quic::Perspective::IS_CLIENT, false);
  }

  void CreateJobControllerImpl(
      raw_ptr<HttpStreamFactory::JobController, AcrossTasksDanglingUntriaged>*
          job_controller,
      MockHttpStreamRequestDelegate* request_delegate,
      const HttpRequestInfo& request_info) {
    auto controller = std::make_unique<HttpStreamFactory::JobController>(
        factory_, request_delegate, session_.get(), &default_job_factory_,
        request_info, is_preconnect_, /*is_websocket=*/false,
        enable_ip_based_pooling_for_h2_, enable_alternative_services_,
        delay_main_job_with_available_spdy_session_,
        /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    *job_controller = controller.get();
    HttpStreamFactoryPeer::AddJobController(factory_, std::move(controller));
  }

  std::unique_ptr<HttpStreamRequest> CreateJobControllerAndStartImpl(
      raw_ptr<HttpStreamFactory::JobController, AcrossTasksDanglingUntriaged>*
          job_controller,
      MockHttpStreamRequestDelegate* request_delegate,
      const HttpRequestInfo& request_info) {
    CreateJobControllerImpl(job_controller, request_delegate, request_info);
    return (*job_controller)
        ->Start(request_delegate, nullptr, net_log_with_source_,
                HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  }

  void PrepareForMainJobImpl(std::unique_ptr<SequencedSocketData>* tcp_data,
                             std::unique_ptr<SSLSocketDataProvider>* ssl_data) {
    *tcp_data = std::make_unique<SequencedSocketData>();
    (*tcp_data)->set_connect_data(
        MockConnect(ASYNC, ERR_IO_PENDING)); /* pause */
    (*ssl_data) = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    session_deps_.socket_factory->AddSSLSocketDataProvider(ssl_data->get());
  }

  void PrepareForQuicJobImpl(std::unique_ptr<MockQuicData>* quic_data) {
    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::COLD_START);
    *quic_data = std::make_unique<MockQuicData>(version_);
    (*quic_data)->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
    (*quic_data)
        ->AddWrite(
            SYNCHRONOUS,
            CreateQuicTestPacketMakerForClient().MakeInitialSettingsPacket(1));
  }

  void PrepareForQuicJobFailureImpl(std::unique_ptr<MockQuicData>* quic_data) {
    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::COLD_START);
    *quic_data = std::make_unique<MockQuicData>(version_);
    (*quic_data)->AddRead(ASYNC, ERR_IO_PENDING);  // Pause
    (*quic_data)->AddRead(ASYNC, ERR_FAILED);
  }

  void MakeMainJobSucceedImpl(MockHttpStreamRequestDelegate& request_delegate,
                              SequencedSocketData* tcp_data,
                              bool expect_stream_ready) {
    if (expect_stream_ready) {
      EXPECT_FALSE(request_delegate.IsDone());
      tcp_data->socket()->OnConnectComplete(MockConnect());
      EXPECT_TRUE(request_delegate.WaitForHttpStream());
    } else {
      bool is_done = request_delegate_->IsDone();
      tcp_data->socket()->OnConnectComplete(MockConnect());
      base::RunLoop().RunUntilIdle();
      // Done state should not change.
      EXPECT_EQ(is_done, request_delegate_->IsDone());
    }
  }

  static void CheckJobsStatusImpl(
      HttpStreamFactory::JobController* job_controller,
      bool main_job_exists,
      bool alternative_job_exists,
      bool dns_alpn_h3_job_exists,
      const std::string& scoped_trace_message) {
    SCOPED_TRACE(scoped_trace_message);
    EXPECT_EQ(main_job_exists, !!job_controller->main_job());
    EXPECT_EQ(alternative_job_exists, !!job_controller->alternative_job());
    EXPECT_EQ(dns_alpn_h3_job_exists, !!job_controller->dns_alpn_h3_job());
  }

  // Use real Jobs so that Job::Resume() is not mocked out. When main job is
  // resumed it will use mock socket data.
  HttpStreamFactory::JobFactory default_job_factory_;

  // Used for man job connection.
  std::unique_ptr<SSLSocketDataProvider> ssl_data_;
  std::unique_ptr<SSLSocketDataProvider> ssl_data2_;
};

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       NoHttpsRecordSyncHostResolve) {
  PrepareForMainJob();
  Initialize(HttpRequestInfo());
  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  // The main job should be synchronously resumed, as host is resolved
  // synchronously.
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  base::RunLoop().RunUntilIdle();

  // `dns_alpn_h3_job` must fail when there is no valid supported alpn. And
  // must be deleted.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false,
                  "DNS ALPN job must be deleted.");

  base::HistogramTester histogram_tester;
  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  // Net.AlternateProtocolUsage records
  // ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON, when only main job exists.
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON,
      1);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       NoHttpsRecordAsyncHostResolveResumeMainWithoutDelay) {
  EnableOndemandHostResolver();
  PrepareForMainJob();
  Initialize(HttpRequestInfo());

  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  // The main job should be resumed quickly after resolving the host.
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());

  // Resolve the host resolve request from `dns_alpn_h3_job`.
  session_deps_.host_resolver->ResolveAllPending();
  base::RunLoop().RunUntilIdle();

  // `dns_alpn_h3_job` must fail when there is no valid supported alpn. And
  // must be deleted.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false,
                  "DNS ALPN job must be deleted.");
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  // The host resolve request from the main job must be resolved using the
  // cached result.
  EXPECT_TRUE(tcp_data_->socket());

  base::HistogramTester histogram_tester;
  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  // Net.AlternateProtocolUsage records
  // ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON, when only main job exists.
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON,
      1);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       NoHttpsRecordAsyncHostResolveResumeMainWithoutDelayQuicWorkedNetwork) {
  EnableOndemandHostResolver();
  PrepareForMainJob();
  Initialize(HttpRequestInfo());

  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);

  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");
  // Main job must be waiting.
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());

  // Resolve the host resolve request from `dns_alpn_h3_job`.
  session_deps_.host_resolver->ResolveAllPending();
  base::RunLoop().RunUntilIdle();

  // `dns_alpn_h3_job` must fail when there is no valid supported alpn. And
  // must be deleted.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false,
                  "DNS ALPN job must be deleted.");
  // The main job should be resumed quickly after resolving the host.
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  // The host resolve request from the main job must be resolved using the
  // cached result.
  EXPECT_TRUE(tcp_data_->socket());

  base::HistogramTester histogram_tester;
  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  // Net.AlternateProtocolUsage records
  // ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON, when only main job exists.
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON,
      1);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       MainJobNoDelayOnQuicNotWorkedNetworkSyncHostResolve) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();
  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");
  // `dns_alpn_h3_job` should not be waiting for dns host
  // resolution as that was resolved synchronously.
  EXPECT_FALSE(job_controller_->dns_alpn_h3_job()
                   ->expect_on_quic_host_resolution_for_tests());

  base::HistogramTester histogram_tester;
  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage",
      ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE, 1);

  // The success of `dns_alpn_h3_job` deletes `main_job`.
  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true, "Main job must be deleted.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       MainJobNoDelayOnQuicNotWorkedNetworkAsyncHostResolve) {
  EnableOndemandHostResolver();
  PrepareForMainJob();
  PrepareForFirstQuicJob();
  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  // `main_job` is blocked until host resolves.
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());

  // Resolve the host resolve request from `dns_alpn_h3_job`.
  session_deps_.host_resolver->ResolveAllPending();
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  base::RunLoop().RunUntilIdle();

  // `main_job` should have been resumed quickly because
  // `is_quic_known_to_work_on_current_network` is false for this test.
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());
  // `dns_alpn_h3_job` must not fail when there is a valid supported alpn.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Both main job and DNS ALPN job must be alive");

  base::HistogramTester histogram_tester;
  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage",
      ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE, 1);

  // The success of `dns_alpn_h3_job` deletes `main_job`.
  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true, "Main job must be deleted.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       MainJobDelayOnQuicWorkedNetwork) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();
  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);

  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");
  base::RunLoop().RunUntilIdle();
  // `dns_alpn_h3_job` must not fail when there is a valid supported alpn.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Both main job and DNS ALPN job must be alive");

  // The main job should be waiting until kDefaultDelayMilliSecsForWaitingJob
  // amount of time has passed.
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  FastForwardBy(base::Milliseconds(kDefaultDelayMilliSecsForWaitingJob - 1));
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  base::HistogramTester histogram_tester;
  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage",
      ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE, 1);

  // The success of `dns_alpn_h3_job` deletes `main_job`.
  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true, "Main job must be deleted.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Test that for a proxied session no DNS APLN H3 job is created (since we don't
// want to perform DNS resolution corresponding to requests that will be
// proxied).
TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       NoDnsAlpnH3JobForProxiedSession) {
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  quic_data_ = std::make_unique<MockQuicData>(quic::ParsedQuicVersion::RFCv1());
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  Initialize(HttpRequestInfo());

  auto proxy_chain =
      ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
          ProxyServer::SCHEME_QUIC, "proxy", 99)});

  auto* test_proxy_delegate =
      static_cast<TestProxyDelegate*>(session_deps_.proxy_delegate.get());
  test_proxy_delegate->set_proxy_chain(proxy_chain);

  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false,
                  "DNS ALPN H3 job must not have been created.");

  base::RunLoop().RunUntilIdle();

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       MainJobSucceedsDnsAlpnH3JobSucceeds) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();
  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());
  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());
  base::RunLoop().RunUntilIdle();

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");
  // `main_job` is not blocked, because the hostname is resolved synchronously
  // and `is_quic_known_to_work_on_current_network` is false for this test.
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  base::HistogramTester histogram_tester;
  // Make `main_job` succeed.
  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE,
      1);

  // The success of `main_job` doesn't delete `dns_alpn_h3_job`.
  EXPECT_TRUE(job_controller_->dns_alpn_h3_job());

  // Make `dns_alpn_h3_job` complete.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/false);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       ActiveSessionAvailableForMainJob) {
  HttpRequestInfo request_info = CreateTestHttpRequestInfo();
  PrepareForFirstQuicJob();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  // Set `is_quic_known_to_work_on_current_network` flag so that
  // the delaying logic of main job would work when the main job is blocked.
  // Note: In this test, we don't need this because the main job is not blocked.
  // But we set here because we want to check that the main job is not blocked.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);

  // Put a SpdySession in the pool.
  SpdySessionKey key(HostPortPair::FromURL(request_info.url),
                     PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  std::ignore = CreateFakeSpdySession(session_->spdy_session_pool(), key);

  request_ = CreateJobControllerAndStart(request_info);
  // `dns_alpn_h3_job` must be created even when an active session is
  // available for `main_job`.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  // Main job must not be waiting because an active session is available.
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  base::HistogramTester histogram_tester;
  // Wait until `main_job` succeeds and status is reported to Request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE,
      1);

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "DNS ALPN job must be alive");

  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/false);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false,
                  "DNS ALPN job must be deleted");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest, MainJobHasActiveSocket) {
  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  PrepareForMainJob();
  PrepareForSecondMainJob();

  PrepareForFirstQuicJobFailure();
  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  // Set `is_quic_known_to_work_on_current_network` flag so that
  // the delaying logic of main job would work when the main job is blocked.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);

  request_ = CreateJobControllerAndStart(request_info);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  FastForwardBy(base::Milliseconds(kDefaultDelayMilliSecsForWaitingJob - 1));
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  auto request2 = CreateSecondJobControllerAndStart(request_info);
  CheckSecondJobsStatus(
      /*main_job_exists=*/true, /*alternative_job_exists=*/false,
      /*dns_alpn_h3_job_exists=*/true,
      "Main job and DNS ALPN job must be created for the second request.");

  // When an active socket is available for the main job, the main job should
  // not be blocked.
  EXPECT_FALSE(job_controller2_->main_job()->is_waiting());

  quic_data_->Resume();
  base::RunLoop().RunUntilIdle();

  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  MakeSecondMainJobSucceed(/*expect_stream_ready=*/true);
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       MainJobHasActiveSocketAltSvcRegistered) {
  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  PrepareForMainJob();
  PrepareForSecondMainJob();

  PrepareForFirstQuicJobFailure();
  PrepareForSecondQuicJobFailure();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  // Set `is_quic_known_to_work_on_current_network` flag so that
  // the delaying logic of main job would work when the main job is blocked.
  QuicSessionPool* quic_session_pool = session_->quic_session_pool();
  quic_session_pool->set_has_quic_ever_worked_on_current_network(true);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "alt.example.org", 443);
  SetAlternativeService(request_info, alternative_service);

  request_ = CreateJobControllerAndStart(request_info);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true,
                  "All types of jobs are created");

  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  FastForwardBy(base::Milliseconds(kDefaultDelayMilliSecsForWaitingJob - 1));
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  auto request2 = CreateSecondJobControllerAndStart(request_info);
  CheckSecondJobsStatus(
      /*main_job_exists=*/true, /*alternative_job_exists=*/true,
      /*dns_alpn_h3_job_exists=*/true,
      "All types of jobs must be created for the second request.");

  // The main job should be waiting until kDefaultDelayMilliSecsForWaitingJob
  // amount of time has passed, when an alternative service was registered,
  // even when an active socket is available for the main job.
  // This is intended to switch to QUIC from TCP for the first connection
  // when the server supports Alt-Svc but doesn't support HTTP DNS records with
  // alpn.
  // Note: When QuicParams.delay_main_job_with_available_spdy_session is false,
  // main job is not blocked.
  EXPECT_TRUE(job_controller2_->main_job()->is_waiting());
  FastForwardBy(base::Milliseconds(kDefaultDelayMilliSecsForWaitingJob - 1));
  EXPECT_TRUE(job_controller2_->main_job()->is_waiting());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(job_controller2_->main_job()->is_waiting());

  quic_data_->Resume();
  quic_data2_->Resume();
  base::RunLoop().RunUntilIdle();

  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  MakeSecondMainJobSucceed(/*expect_stream_ready=*/true);
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       ActiveSessionAvailableForAltSvcJob) {
  PrepareForMainJob();
  RegisterMockHttpsRecord();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  PrepareForFirstQuicJob();

  Initialize(HttpRequestInfo());

  std::unique_ptr<QuicHttpStream> stream =
      ConnectQuicHttpStream(/*alt_destination=*/true,
                            /*require_dns_https_alpn=*/false);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "alt.example.org", 443);
  SetAlternativeService(request_info, alternative_service);

  request_ = CreateJobControllerAndStart(request_info);

  // `dns_alpn_h3_job` must not be created when an active session is
  // available for `alternative_job`.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/false,
                  "Main job and alternative job must be created.");

  base::HistogramTester histogram_tester;
  // Wait until `alternative_job` succeeds and status is reported to Request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
  histogram_tester.ExpectUniqueSample("Net.AlternateProtocolUsage",
                                      ALTERNATE_PROTOCOL_USAGE_NO_RACE, 1);

  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/false,
                  "Main job must be deleted.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       ActiveSessionAvailableForDnsAlpnH3Job) {
  PrepareForFirstQuicJob();
  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  std::unique_ptr<QuicHttpStream> stream =
      ConnectQuicHttpStream(/*alt_destination=*/false,
                            /*require_dns_https_alpn=*/true);
  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and alternative job must not be available.");

  base::HistogramTester histogram_tester;
  // Wait for `dns_alpn_h3_job` to succeed and status to be reported to Request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage",
      ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_WITHOUT_RACE, 1);
  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "DNS alpn H3 job must exist.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       ActiveSessionAvailableForMainJobAndDnsAlpnH3Job) {
  HttpRequestInfo request_info = CreateTestHttpRequestInfo();
  PrepareForFirstQuicJob();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  // Put a SpdySession in the pool.
  SpdySessionKey key(HostPortPair::FromURL(request_info.url),
                     PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*disable_cert_verification_network_fetches=*/false);
  std::ignore = CreateFakeSpdySession(session_->spdy_session_pool(), key);

  std::unique_ptr<QuicHttpStream> stream =
      ConnectQuicHttpStream(/*alt_destination=*/false,
                            /*require_dns_https_alpn=*/true);
  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());

  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job must not be available.");

  base::HistogramTester histogram_tester;
  // Wait for `dns_alpn_h3_job` to succeed and status to be reported to Request.
  EXPECT_TRUE(request_delegate_->WaitForHttpStream());
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage",
      ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_WITHOUT_RACE, 1);

  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "DNS alpn H3 job must exist.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       DoNotStartDnsAlpnH3JobWhenSameHostDefaultPortAltJobCreated) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "www.example.org", 443);
  SetAlternativeService(request_info, alternative_service);

  request_ = CreateJobControllerAndStart(request_info);
  // `dns_alpn_h3_job` must be deleted when a same origin alt service
  // was registered.
  CheckJobsStatus(
      true, true, false,
      "All types of jobs are created, but DNS alpn job must be deleted");

  base::RunLoop().RunUntilIdle();
  base::HistogramTester histogram_tester;
  // Make `main_job` succeed.
  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE,
      1);

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/false,
                  "Alternate job must not be deleted");

  // Make `alternative_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/false);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       AllJobsCreatedMainJobSucceedAltJobSucceedDnsJobSucceed) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();
  PrepareForSecondQuicJob();

  // Use cold start and complete `alternative_job` and `dns_alpn_h3_job`
  // manually.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "alt.example.org", 443);
  SetAlternativeService(request_info, alternative_service);

  request_ = CreateJobControllerAndStart(request_info);
  // `dns_alpn_h3_job` must be created when a different origin alt service
  // was registered.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true,
                  "All types of jobs are created");

  base::HistogramTester histogram_tester;
  base::RunLoop().RunUntilIdle();
  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE,
      1);

  // The success of `main_job` doesn't delete `alternative_job` and
  // `dns_alpn_h3_job`.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true, "Jobs must not be deleted.");

  // Make `alternative_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/false);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Alternate job must be deleted.");

  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(1, /*expect_stream_ready=*/false);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false,
                  "DNS alpn job must be deleted.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       AllJobsCreatedAltJobSucceedDnsJobSucceedMainJobSucceed) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();
  PrepareForSecondQuicJob();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "alt.example.org", 443);
  SetAlternativeService(request_info, alternative_service);

  request_ = CreateJobControllerAndStart(request_info);
  // `dns_alpn_h3_job` must be created when a different origin alt service
  // was registered.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true,
                  "All types of jobs are created");

  base::HistogramTester histogram_tester;
  // Make `alternative_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample("Net.AlternateProtocolUsage",
                                      ALTERNATE_PROTOCOL_USAGE_WON_RACE, 1);

  // The success of `alternative_job` doesn't delete `main_job` and
  // `dns_alpn_h3_job`.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true, "Jobs must not be deleted.");

  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(1, /*expect_stream_ready=*/false);

  // The success of `dns_alpn_h3_job` doesn't delete `main_job` and
  // `alternative_job`.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/false,
                  "DNS alpn job must be deleted.");

  // Make `main_job` succeed.
  MakeMainJobSucceed(/*expect_stream_ready=*/false);

  // `main_job` should be cleared.
  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/false,
                  "Alternate job must be deleted.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       AllJobsCreatedDnsJobSucceedAltJobSucceedMainJobSucceed) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();
  PrepareForSecondQuicJob();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "alt.example.org", 443);
  SetAlternativeService(request_info, alternative_service);

  request_ = CreateJobControllerAndStart(request_info);
  // `dns_alpn_h3_job` must be created when a different origin alt service
  // was registered.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true,
                  "All types of jobs are created");

  base::HistogramTester histogram_tester;
  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(1, /*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage",
      ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE, 1);

  // The success of `dns_alpn_h3_job` doesn't delete `main_job` and
  // `alternative_job`.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true, "Jobs must not be deleted.");

  // Make `alternative_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/false);

  // The success of `alternative_job` doesn't delete `main_job` and
  // `dns_alpn_h3_job`.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Alternate job must be deleted.");

  // Make `main_job` succeed.
  MakeMainJobSucceed(/*expect_stream_ready=*/false);

  // `main_job` should be cleared.
  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true, "Main job must be deleted.");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       DnsJobFailOnDefaultNetworkDnsJobFailMainJobSucceed) {
  PrepareForMainJob();
  PrepareForFirstQuicJobFailure();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());
  request_ = CreateJobControllerAndStart(request_info);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  JobControllerPeer::SetDnsAlpnH3JobFailedOnDefaultNetwork(job_controller_);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true, "Jobs must not be deleted.");

  base::RunLoop().RunUntilIdle();
  base::HistogramTester histogram_tester;
  // Make `dns_alpn_h3_job` fail.
  quic_data_->Resume();
  base::RunLoop().RunUntilIdle();
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false, "DNS alpn job be deleted.");

  // Make `main_job` succeed.
  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  // Net.AlternateProtocolUsage records
  // ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON, when only main job exists.
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON,
      1);

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false,
                  "DNS alpn job must be deleted.");

  request_.reset();
  EXPECT_TRUE(IsAlternativeServiceBroken(request_info.url));
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  histogram_tester.ExpectUniqueSample("Net.AlternateServiceForDnsAlpnH3Failed",
                                      -ERR_QUIC_PROTOCOL_ERROR, 1);

  // Verify the brokenness is not cleared when the default network changes.
  session_->http_server_properties()->OnDefaultNetworkChanged();
  EXPECT_TRUE(IsAlternativeServiceBroken(request_info.url));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       DnsJobFailOnDefaultNetworkMainJobSucceedDnsJobSucceed) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());
  base::HistogramTester histogram_tester;
  request_ = CreateJobControllerAndStart(request_info);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  JobControllerPeer::SetDnsAlpnH3JobFailedOnDefaultNetwork(job_controller_);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true, "Jobs must not be deleted.");
  base::RunLoop().RunUntilIdle();
  // Make `main_job` succeed.
  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage", ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE,
      1);

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "DNS alpn job must not be deleted.");

  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/false);

  request_.reset();
  histogram_tester.ExpectTotalCount("Net.AlternateServiceForDnsAlpnH3Failed",
                                    0);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  EXPECT_TRUE(IsAlternativeServiceBroken(request_info.url));

  // Verify the brokenness is cleared when the default network changes.
  session_->http_server_properties()->OnDefaultNetworkChanged();
  EXPECT_FALSE(IsAlternativeServiceBroken(request_info.url));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       DnsJobSucceedMainJobCanceled) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());
  request_ = CreateJobControllerAndStart(request_info);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  base::HistogramTester histogram_tester;
  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage",
      ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE, 1);

  // Main job is canceled.
  CheckJobsStatus(/*main_job_exists=*/false, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true, "Main job must be deleted");

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       DnsJobFailOnDefaultNetworkDnsJobSucceedMainJobSucceed) {
  PrepareForMainJob();
  PrepareForFirstQuicJob();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());
  request_ = CreateJobControllerAndStart(request_info);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  JobControllerPeer::SetDnsAlpnH3JobFailedOnDefaultNetwork(job_controller_);
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true, "Jobs must not be deleted.");

  base::HistogramTester histogram_tester;
  // Make `dns_alpn_h3_job` succeed.
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/true);
  histogram_tester.ExpectUniqueSample(
      "Net.AlternateProtocolUsage",
      ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE, 1);

  // Main job is not canceled, because `dns_alpn_h3_job` has failed on the
  // default network.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job must not be deleted.");

  // Make `main_job` succeed.
  MakeMainJobSucceed(/*expect_stream_ready=*/false);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest, PreconnectDnsAlpnH3) {
  SetPreconnect();
  PrepareForFirstQuicJob();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();

  Initialize(HttpRequestInfo());
  CreateJobController(request_info);
  job_controller_->Preconnect(/*num_streams=*/5, base::OnceClosure());
  // Only one job is started.
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_EQ(HttpStreamFactory::PRECONNECT_DNS_ALPN_H3,
            job_controller_->main_job()->job_type());

  MakeQuicJobSucceed(0, /*expect_stream_ready=*/false);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       PreconnectAltSvcAvailableActiveSessionAvailable) {
  SetPreconnect();
  PrepareForFirstQuicJob();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  RegisterMockHttpsRecord();
  Initialize(request_info);

  // Register Alt-Svc info.
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, server.host(),
                                         443);
  SetAlternativeService(request_info, alternative_service);

  // Create an active session of require_dns_https_alpn = true.
  std::unique_ptr<QuicHttpStream> stream =
      ConnectQuicHttpStream(/*alt_destination=*/false,
                            /*require_dns_https_alpn=*/true);

  CreateJobController(request_info);
  // Preconnect must succeed using the existing session.
  job_controller_->Preconnect(/*num_streams=*/1, base::OnceClosure());
  ASSERT_TRUE(job_controller_->main_job());
  EXPECT_EQ(HttpStreamFactory::PRECONNECT_DNS_ALPN_H3,
            job_controller_->main_job()->job_type());
  MakeQuicJobSucceed(0, /*expect_stream_ready=*/false);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest, PreconnectNoDnsAlpnH3) {
  EnableOndemandHostResolver();
  PrepareForMainJob();
  SetPreconnect();

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  Initialize(HttpRequestInfo());
  CreateJobController(request_info);
  job_controller_->Preconnect(/*num_streams=*/1, base::OnceClosure());
  // Only one job is started.
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_EQ(HttpStreamFactory::PRECONNECT_DNS_ALPN_H3,
            job_controller_->main_job()->job_type());

  // Resolve the host resolve request from `dns_alpn_h3_job`.
  session_deps_.host_resolver->ResolveAllPending();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HttpStreamFactory::PRECONNECT,
            job_controller_->main_job()->job_type());

  base::RunLoop().RunUntilIdle();

  // Make `main_job` succeed.
  MakeMainJobSucceed(/*expect_stream_ready=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Test that, when an Alt-Svc-based preconnect fails with
// `ERR_DNS_NO_MATCHING_SUPPORTED_ALPN`, the job controller handles it
// correctly. This is a regression test for https://crbug.com/1420202.
//
// In a general HTTPS-RR implementation, this may happen simply because there
// was no A/AAAA route. However, we do not implement HTTPS-RR in full yet (see
// https://crbug.com/1417033), so instead this is only possible in a corner case
// with ECH.
TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       PreconnectAlternateNoDnsAlpn) {
  const char kAlternateHost[] = "alt.example.com";

  EnableOndemandHostResolver();
  PrepareForMainJob();
  SetPreconnect();

  // Register a mock HTTPS record where the HTTPS-RR route is only good for h2,
  // which is incompatible with Alt-Svc. The A/AAAA route would be compatible,
  // but the server supports ECH, so we enable SVCB-reliant mode and reject it.
  // As a result, the alternate job will fail.
  HostResolverEndpointResult endpoint_result1;
  endpoint_result1.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoint_result1.metadata.ech_config_list = {1, 2, 3, 4};
  endpoint_result1.metadata.supported_protocol_alpns = {"h2"};
  HostResolverEndpointResult endpoint_result2;
  endpoint_result2.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  session_deps_.host_resolver->rules()->AddRule(
      kAlternateHost,
      MockHostResolverBase::RuleResolver::RuleResult(
          {endpoint_result1, endpoint_result2}, {kAlternateHost}));

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();
  Initialize(request_info);
  CreateJobController(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC, kAlternateHost,
                                         443);
  SetAlternativeService(request_info, alternative_service);

  job_controller_->Preconnect(/*num_streams=*/1, base::OnceClosure());
  // Only one job is started.
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_EQ(HttpStreamFactory::PRECONNECT,
            job_controller_->main_job()->job_type());

  // Resolve the DNS request.
  session_deps_.host_resolver->ResolveAllPending();
  base::RunLoop().RunUntilIdle();

  // The jobs should have failed. We currently do not try the non-Alt-Svc route
  // in preconnects if Alt-Svc failed.
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       DnsResolutionTimeOverridesFromAlpnH3Job) {
  const base::TimeDelta kDnsDelay = base::Milliseconds(10);
  EnableOndemandHostResolver();
  PrepareForMainJob();
  session_deps_.host_resolver->rules()->AddRule(
      "www.example.org", IPAddress::IPv4Localhost().ToString());
  Initialize(HttpRequestInfo());
  request_ = CreateJobControllerAndStart(CreateTestHttpRequestInfo());
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Main job and DNS ALPN job must be created.");

  // Simulate the delay of DNS resolution.
  FastForwardBy(kDnsDelay);

  // Resolve the host resolve request from `dns_alpn_h3_job`.
  session_deps_.host_resolver->ResolveAllPending();

  // `main_job` must be resumed.
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  // DnsResolutionTimeOverrides must be set.
  EXPECT_EQ(kDnsDelay, request_->dns_resolution_end_time_override() -
                           request_->dns_resolution_start_time_override());

  MakeMainJobSucceed(/*expect_stream_ready=*/true);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       DnsResolutionTimeOverridesFromAlternativeJob) {
  const base::TimeDelta kDnsDelay = base::Milliseconds(10);
  EnableOndemandHostResolver();
  PrepareForMainJob();
  PrepareForFirstQuicJobFailure();
  session_deps_.host_resolver->rules()->AddRule(
      "www.example.org", IPAddress::IPv4Localhost().ToString());
  Initialize(HttpRequestInfo());

  // Register the same destination alternative service.
  HttpRequestInfo request_info = CreateTestHttpRequestInfo();
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "www.example.org", 443);
  SetAlternativeService(request_info, alternative_service);

  request_ = CreateJobControllerAndStart(request_info);

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/false,
                  "Main job and alternative job are created.");

  // Simulate the delay of DNS resolution.
  FastForwardBy(kDnsDelay);

  // Resolve the host resolve request from `alternative_job`.
  session_deps_.host_resolver->ResolveAllPending();

  // `main_job` must be resumed.
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  // DnsResolutionTimeOverrides must be set by the `alternative_job`.
  EXPECT_EQ(kDnsDelay, request_->dns_resolution_end_time_override() -
                           request_->dns_resolution_start_time_override());

  // Make `dns_alpn_h3_job` fail.
  quic_data_->Resume();

  MakeMainJobSucceed(/*expect_stream_ready=*/true);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerDnsHttpsAlpnTest,
       DnsResolutionTimeOverridesNotFromDifferentAlternativeJob) {
  const base::TimeDelta kDnsDelay1 = base::Milliseconds(10);
  const base::TimeDelta kDnsDelay2 = base::Milliseconds(20);

  HttpRequestInfo request_info = CreateTestHttpRequestInfo();

  EnableOndemandHostResolver();
  PrepareForMainJob();
  PrepareForFirstQuicJobFailure();

  session_deps_.host_resolver->rules()->AddRule(
      "www.example.org", IPAddress::IPv4Localhost().ToString());
  session_deps_.host_resolver->rules()->AddRule(
      "alt.example.org", IPAddress::IPv4Localhost().ToString());

  Initialize(HttpRequestInfo());

  // Register a different destination alternative service.
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "alt.example.org", 443);
  SetAlternativeService(request_info, alternative_service);

  request_ = CreateJobControllerAndStart(request_info);

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true,
                  "All types of jobs are created.");

  // `main_job` is blocked until host resolves.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());

  EXPECT_EQ(2u, session_deps_.host_resolver->last_id());
  EXPECT_EQ("alt.example.org", session_deps_.host_resolver->request_host(1));
  EXPECT_EQ("www.example.org", session_deps_.host_resolver->request_host(2));

  // Simulate the delay of DNS resolution.
  FastForwardBy(kDnsDelay1);

  // Resolves the DNS request for "alt.example.org".
  session_deps_.host_resolver->ResolveNow(1);

  // `main_job` must be resumed.
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());

  // DnsResolutionTimeOverrides must not be set for the different destination's
  // alternative job's DNS resolution time.
  EXPECT_TRUE(request_->dns_resolution_end_time_override().is_null());
  EXPECT_TRUE(request_->dns_resolution_start_time_override().is_null());

  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/true,
                  /*dns_alpn_h3_job_exists=*/true,
                  "All types of jobs must alive.");

  // Make `alternative_job` fail.
  quic_data_->Resume();
  base::RunLoop().RunUntilIdle();
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/true,
                  "Alternative job be deleted.");

  // Simulate the delay of DNS resolution.
  FastForwardBy(kDnsDelay2);

  // Resolves the DNS request for "www.example.org".
  session_deps_.host_resolver->ResolveAllPending();
  base::RunLoop().RunUntilIdle();

  // DnsResolutionTimeOverrides must be set.
  EXPECT_EQ(kDnsDelay1 + kDnsDelay2,
            request_->dns_resolution_end_time_override() -
                request_->dns_resolution_start_time_override());

  EXPECT_FALSE(job_controller_->main_job()->is_waiting());
  // `dns_alpn_h3_job` must not fail when there is a valid supported alpn.
  CheckJobsStatus(/*main_job_exists=*/true, /*alternative_job_exists=*/false,
                  /*dns_alpn_h3_job_exists=*/false,
                  "Only main job must be alive");

  MakeMainJobSucceed(/*expect_stream_ready=*/true);

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Tests specific to the HappyEyeballsV3 feature.
// TODO(crbug.com/346835898): Find ways to run more tests with the
// HappyEyeballsV3 feature enabled.
class HttpStreamFactoryJobControllerPoolTest
    : public HttpStreamFactoryJobControllerTestBase {
 public:
  HttpStreamFactoryJobControllerPoolTest()
      : HttpStreamFactoryJobControllerTestBase(
            /*happy_eyeballs_v3_enabled=*/true) {}

  ~HttpStreamFactoryJobControllerPoolTest() override = default;

 protected:
  HttpStreamPool* pool() { return session_->http_stream_pool(); }

  FakeServiceEndpointResolver* resolver() {
    return static_cast<FakeServiceEndpointResolver*>(
        session_deps_.alternate_host_resolver.get());
  }
};

TEST_F(HttpStreamFactoryJobControllerPoolTest, Preconnect) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("127.0.0.1").endpoint())
      .CompleteStartSynchronously(OK);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SetPreconnect();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");
  Initialize(request_info);

  base::RunLoop run_loop;
  job_controller_->Preconnect(
      /*num_streams=*/1, run_loop.QuitClosure());
  run_loop.Run();
  // `job_controller_` is deleted at this point and should not be accessed.
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  ASSERT_EQ(pool()->TotalIdleStreamCount(), 1u);
}

TEST_F(HttpStreamFactoryJobControllerPoolTest, PreconnectSync) {
  SetPreconnect();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");
  Initialize(request_info);

  // Add an idle stream to the pool.
  const HttpStreamKey stream_key(
      url::SchemeHostPort("http", "www.example.com", 80), PRIVACY_MODE_DISABLED,
      SocketTag(), NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false);
  HttpStreamPool::Group& group = pool()->GetOrCreateGroupForTesting(stream_key);
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  // Preconnect should complete immediately as we already have an idle stream.
  base::RunLoop run_loop;
  job_controller_->Preconnect(
      /*num_streams=*/1, run_loop.QuitClosure());
  // `job_controller_` is deleted at this point and should not be accessed.
  ASSERT_EQ(pool()->TotalIdleStreamCount(), 1u);

  run_loop.Run();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

}  // namespace net::test
