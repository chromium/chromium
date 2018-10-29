// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_job_controller.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/platform_thread.h"
#include "net/base/completion_once_callback.h"
#include "net/base/test_proxy_delegate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_factory_job.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/mock_proxy_resolver.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_stream_factory.h"
#include "net/quic/quic_stream_factory_peer.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::SizeIs;

namespace net {

namespace test {

namespace {

const char kServerHostname[] = "www.example.com";

// List of errors for which fallback is expected on an HTTPS proxy.
const int proxy_test_mock_errors[] = {
    ERR_PROXY_CONNECTION_FAILED,
    ERR_NAME_NOT_RESOLVED,
    ERR_ADDRESS_UNREACHABLE,
    ERR_CONNECTION_CLOSED,
    ERR_CONNECTION_TIMED_OUT,
    ERR_CONNECTION_RESET,
    ERR_CONNECTION_REFUSED,
    ERR_CONNECTION_ABORTED,
    ERR_TIMED_OUT,
    ERR_SOCKS_CONNECTION_FAILED,
    ERR_PROXY_CERTIFICATE_INVALID,
    ERR_SSL_PROTOCOL_ERROR,
};

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

class FailingHostResolver : public MockHostResolverBase {
 public:
  FailingHostResolver() : MockHostResolverBase(false /*use_caching*/) {}
  ~FailingHostResolver() override = default;

  int Resolve(const RequestInfo& info,
              RequestPriority priority,
              AddressList* addresses,
              CompletionOnceCallback callback,
              std::unique_ptr<Request>* out_req,
              const NetLogWithSource& net_log) override {
    return ERR_NAME_NOT_RESOLVED;
  }
};

// TODO(xunjieli): This should just use HangingHostResolver from
// mock_host_resolver.h
class HangingResolver : public MockHostResolverBase {
 public:
  HangingResolver() : MockHostResolverBase(false /*use_caching*/) {}
  ~HangingResolver() override = default;

  int Resolve(const RequestInfo& info,
              RequestPriority priority,
              AddressList* addresses,
              CompletionOnceCallback callback,
              std::unique_ptr<Request>* out_req,
              const NetLogWithSource& net_log) override {
    return ERR_IO_PENDING;
  }
};

// A mock HttpServerProperties that always returns false for IsInitialized().
class MockHttpServerProperties : public HttpServerPropertiesImpl {
 public:
  MockHttpServerProperties() = default;
  ~MockHttpServerProperties() override = default;
  bool IsInitialized() const override { return false; }
};

}  // anonymous namespace

class HttpStreamFactoryJobPeer {
 public:
  static void Start(HttpStreamFactory::Job* job,
                    HttpStreamRequest::StreamType stream_type) {
    // Start() is mocked for MockHttpStreamFactoryJob.
    // This is the alternative method to invoke real Start() method on Job.
    job->stream_type_ = stream_type;
    job->StartInternal();
  }

  // Returns |num_streams_| of |job|. It should be 0 for non-preconnect Jobs.
  static int GetNumStreams(const HttpStreamFactory::Job* job) {
    return job->num_streams_;
  }

  // Return SpdySessionKey of |job|.
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

  static AlternativeServiceInfo GetAlternativeServiceInfoFor(
      HttpStreamFactory::JobController* job_controller,
      const HttpRequestInfo& request_info,
      HttpStreamRequest::Delegate* delegate,
      HttpStreamRequest::StreamType stream_type) {
    return job_controller->GetAlternativeServiceInfoFor(request_info, delegate,
                                                        stream_type);
  }

  static void SetAltJobFailedOnDefaultNetwork(
      HttpStreamFactory::JobController* job_controller) {
    DCHECK(job_controller->alternative_job() != nullptr);
    HttpStreamFactoryJobPeer::SetQuicConnectionFailedOnDefaultNetwork(
        job_controller->alternative_job_.get());
  }
};

class HttpStreamFactoryJobControllerTest
    : public TestWithScopedTaskEnvironment {
 public:
  HttpStreamFactoryJobControllerTest()
      : TestWithScopedTaskEnvironment(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    session_deps_.enable_quic = true;
    session_deps_.host_resolver->set_synchronous_mode(true);
  }

  void UseAlternativeProxy() {
    ASSERT_FALSE(test_proxy_delegate_);
    use_alternative_proxy_ = true;
  }

  void SetPreconnect() {
    ASSERT_FALSE(test_proxy_delegate_);
    is_preconnect_ = true;
  }

  void DisableIPBasedPooling() {
    ASSERT_FALSE(test_proxy_delegate_);
    enable_ip_based_pooling_ = false;
  }

  void DisableAlternativeServices() {
    ASSERT_FALSE(test_proxy_delegate_);
    enable_alternative_services_ = false;
  }

  void SkipCreatingJobController() {
    ASSERT_FALSE(job_controller_);
    create_job_controller_ = false;
  }

  void Initialize(const HttpRequestInfo& request_info) {
    ASSERT_FALSE(test_proxy_delegate_);
    test_proxy_delegate_ = std::make_unique<TestProxyDelegate>();
    test_proxy_delegate_->set_alternative_proxy_server(
        ProxyServer::FromPacString("QUIC myproxy.org:443"));
    EXPECT_TRUE(test_proxy_delegate_->alternative_proxy_server().is_quic());

    if (quic_data_)
      quic_data_->AddSocketDataToFactory(session_deps_.socket_factory.get());
    if (tcp_data_)
      session_deps_.socket_factory->AddSocketDataProvider(tcp_data_.get());

    if (use_alternative_proxy_) {
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
          ProxyResolutionService::CreateFixedFromPacResult(
              "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);
      session_deps_.proxy_resolution_service =
          std::move(proxy_resolution_service);
    }

    session_deps_.proxy_resolution_service->SetProxyDelegate(
        test_proxy_delegate_.get());

    session_deps_.net_log = net_log_.bound().net_log();
    HttpNetworkSession::Params params =
        SpdySessionDependencies::CreateSessionParams(&session_deps_);
    HttpNetworkSession::Context session_context =
        SpdySessionDependencies::CreateSessionContext(&session_deps_);

    session_context.quic_crypto_client_stream_factory =
        &crypto_client_stream_factory_;
    session_context.quic_random = &random_generator_;
    session_ = std::make_unique<HttpNetworkSession>(params, session_context);
    factory_ = static_cast<HttpStreamFactory*>(session_->http_stream_factory());
    if (create_job_controller_) {
      job_controller_ = new HttpStreamFactory::JobController(
          factory_, &request_delegate_, session_.get(), &job_factory_,
          request_info, is_preconnect_, false /* is_websocket */,
          enable_ip_based_pooling_, enable_alternative_services_, SSLConfig(),
          SSLConfig());
      HttpStreamFactoryPeer::AddJobController(factory_, job_controller_);
    }
  }

  TestProxyDelegate* test_proxy_delegate() const {
    return test_proxy_delegate_.get();
  }

  ~HttpStreamFactoryJobControllerTest() override {
    if (quic_data_) {
      EXPECT_TRUE(quic_data_->AllReadDataConsumed());
      EXPECT_TRUE(quic_data_->AllWriteDataConsumed());
    }
    if (tcp_data_) {
      EXPECT_TRUE(tcp_data_->AllReadDataConsumed());
      EXPECT_TRUE(tcp_data_->AllWriteDataConsumed());
    }
  }

  void SetAlternativeService(const HttpRequestInfo& request_info,
                             AlternativeService alternative_service) {
    url::SchemeHostPort server(request_info.url);
    base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
    if (alternative_service.protocol == kProtoQUIC) {
      session_->http_server_properties()->SetQuicAlternativeService(
          server, alternative_service, expiration,
          session_->params().quic_supported_versions);
    } else {
      session_->http_server_properties()->SetHttp2AlternativeService(
          server, alternative_service, expiration);
    }
  }

  void VerifyBrokenAlternateProtocolMapping(const HttpRequestInfo& request_info,
                                            bool should_mark_broken) {
    const url::SchemeHostPort server(request_info.url);
    const AlternativeServiceInfoVector alternative_service_info_vector =
        session_->http_server_properties()->GetAlternativeServiceInfos(server);
    EXPECT_EQ(1u, alternative_service_info_vector.size());
    EXPECT_EQ(should_mark_broken,
              session_->http_server_properties()->IsAlternativeServiceBroken(
                  alternative_service_info_vector[0].alternative_service()));
  }

  void TestAltJobSucceedsAfterMainJobFailed(
      bool alt_job_retried_on_non_default_network);
  void TestMainJobSucceedsAfterAltJobFailed(
      bool alt_job_retried_on_non_default_network);
  void TestAltJobSucceedsAfterMainJobSucceeded(
      bool alt_job_retried_on_non_default_network);
  void TestOnStreamFailedForBothJobs(
      bool alt_job_retried_on_non_default_network);
  void TestAltJobFailsAfterMainJobSucceeded(
      bool alt_job_retried_on_non_default_network);
  void TestMainJobSucceedsAfterAltJobSucceeded(
      bool alt_job_retried_on_non_default_network);
  void TestMainJobFailsAfterAltJobSucceeded(
      bool alt_job_retried_on_non_default_network);

  TestJobFactory job_factory_;
  MockHttpStreamRequestDelegate request_delegate_;
  SpdySessionDependencies session_deps_{ProxyResolutionService::CreateDirect()};
  std::unique_ptr<HttpNetworkSession> session_;
  HttpStreamFactory* factory_ = nullptr;
  HttpStreamFactory::JobController* job_controller_ = nullptr;
  std::unique_ptr<HttpStreamRequest> request_;
  std::unique_ptr<SequencedSocketData> tcp_data_;
  std::unique_ptr<MockQuicData> quic_data_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  quic::MockClock clock_;
  quic::test::MockRandom random_generator_{0};
  QuicTestPacketMaker client_maker_{
      HttpNetworkSession::Params().quic_supported_versions[0],
      0,
      &clock_,
      kServerHostname,
      quic::Perspective::IS_CLIENT,
      false};

 protected:
  BoundTestNetLog net_log_;
  bool use_alternative_proxy_ = false;
  bool is_preconnect_ = false;
  bool enable_ip_based_pooling_ = true;
  bool enable_alternative_services_ = true;

 private:
  std::unique_ptr<TestProxyDelegate> test_proxy_delegate_;
  bool create_job_controller_ = true;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamFactoryJobControllerTest);
};

TEST_F(HttpStreamFactoryJobControllerTest, ProxyResolutionFailsSync) {
  ProxyConfig proxy_config;
  proxy_config.set_pac_url(GURL("http://fooproxyurl"));
  proxy_config.set_pac_mandatory(true);
  session_deps_.proxy_resolution_service.reset(new ProxyResolutionService(
      std::make_unique<ProxyConfigServiceFixed>(ProxyConfigWithAnnotation(
          proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS)),
      std::make_unique<FailingProxyResolverFactory>(), nullptr));
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  EXPECT_CALL(request_delegate_,
              OnStreamFailed(ERR_MANDATORY_PROXY_CONFIGURATION_FAILED, _, _))
      .Times(1);
  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  // Make sure calling GetLoadState() when before job creation does not crash.
  // Regression test for crbug.com/723920.
  EXPECT_EQ(LOAD_STATE_IDLE, job_controller_->GetLoadState());

  base::RunLoop().RunUntilIdle();
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerTest, ProxyResolutionFailsAsync) {
  ProxyConfig proxy_config;
  proxy_config.set_pac_url(GURL("http://fooproxyurl"));
  proxy_config.set_pac_mandatory(true);
  MockAsyncProxyResolverFactory* proxy_resolver_factory =
      new MockAsyncProxyResolverFactory(false);
  MockAsyncProxyResolver resolver;
  session_deps_.proxy_resolution_service.reset(new ProxyResolutionService(
      std::make_unique<ProxyConfigServiceFixed>(ProxyConfigWithAnnotation(
          proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS)),
      base::WrapUnique(proxy_resolver_factory), nullptr));
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  EXPECT_EQ(LOAD_STATE_RESOLVING_PROXY_FOR_URL,
            job_controller_->GetLoadState());

  EXPECT_CALL(request_delegate_,
              OnStreamFailed(ERR_MANDATORY_PROXY_CONFIGURATION_FAILED, _, _))
      .Times(1);
  proxy_resolver_factory->pending_requests()[0]->CompleteNowWithForwarder(
      ERR_FAILED, &resolver);
  base::RunLoop().RunUntilIdle();
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerTest, NoSupportedProxies) {
  session_deps_.proxy_resolution_service =
      ProxyResolutionService::CreateFixedFromPacResult(
          "QUIC myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);
  session_deps_.enable_quic = false;
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  EXPECT_CALL(request_delegate_, OnStreamFailed(ERR_NO_SUPPORTED_PROXIES, _, _))
      .Times(1);
  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  base::RunLoop().RunUntilIdle();
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

class JobControllerReconsiderProxyAfterErrorTest
    : public HttpStreamFactoryJobControllerTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, int>> {
 public:
  void Initialize(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
    session_deps_.proxy_resolution_service =
        std::move(proxy_resolution_service);
    session_ = std::make_unique<HttpNetworkSession>(
        SpdySessionDependencies::CreateSessionParams(&session_deps_),
        SpdySessionDependencies::CreateSessionContext(&session_deps_));
    factory_ = session_->http_stream_factory();
  }

  std::unique_ptr<HttpStreamRequest> CreateJobController(
      const HttpRequestInfo& request_info) {
    HttpStreamFactory::JobController* job_controller =
        new HttpStreamFactory::JobController(
            factory_, &request_delegate_, session_.get(), &default_job_factory_,
            request_info, is_preconnect_, false /* is_websocket */,
            enable_ip_based_pooling_, enable_alternative_services_, SSLConfig(),
            SSLConfig());
    HttpStreamFactoryPeer::AddJobController(factory_, job_controller);
    return job_controller->Start(&request_delegate_, nullptr, net_log_.bound(),
                                 HttpStreamRequest::HTTP_STREAM,
                                 DEFAULT_PRIORITY);
  }

 private:
  // Use real Jobs so that Job::Resume() is not mocked out. When main job is
  // resumed it will use mock socket data.
  HttpStreamFactory::JobFactory default_job_factory_;
};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    JobControllerReconsiderProxyAfterErrorTest,
    ::testing::Combine(::testing::Bool(),
                       testing::ValuesIn(proxy_test_mock_errors)));

// TODO(eroman): The testing should be expanded to test cases where proxy
//               fallback is NOT supposed to occur, and also vary across all of
//               the proxy types.
TEST_P(JobControllerReconsiderProxyAfterErrorTest, ReconsiderProxyAfterError) {
  // Use mock proxy client sockets to test the fallback behavior of error codes
  // returned by HttpProxyClientSocketWrapper. Errors returned by transport
  // sockets usually get re-written by the wrapper class. crbug.com/826570.
  session_deps_.socket_factory->UseMockProxyClientSockets();

  const bool set_alternative_proxy_server = ::testing::get<0>(GetParam());
  const int mock_error = ::testing::get<1>(GetParam());
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ProxyResolutionService::CreateFixedFromPacResult(
          "HTTPS badproxy:99; HTTPS badfallbackproxy:98; DIRECT",
          TRAFFIC_ANNOTATION_FOR_TESTS);
  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();
  TestProxyDelegate* test_proxy_delegate_raw = test_proxy_delegate.get();

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty())
      << mock_error;

  // Alternative Proxy job is given preference over the main job, so populate
  // the socket provider first.
  StaticSocketDataProvider socket_data_proxy_alternate_job;
  if (set_alternative_proxy_server) {
    // Mock data for QUIC proxy socket.
    socket_data_proxy_alternate_job.set_connect_data(
        MockConnect(ASYNC, mock_error));
    session_deps_.socket_factory->AddSocketDataProvider(
        &socket_data_proxy_alternate_job);
    test_proxy_delegate->set_alternative_proxy_server(
        ProxyServer::FromPacString("QUIC badproxy:99"));
  }

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ProxyClientSocketDataProvider proxy_data(ASYNC, mock_error);

  StaticSocketDataProvider socket_data_proxy_main_job;
  socket_data_proxy_main_job.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_proxy_main_job);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  session_deps_.socket_factory->AddProxyClientSocketDataProvider(&proxy_data);

  // When retrying the job using the second proxy (badfallback:98),
  // alternative job must not be created. So, socket data for only the
  // main job is needed.
  StaticSocketDataProvider socket_data_proxy_main_job_2;
  socket_data_proxy_main_job_2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_proxy_main_job_2);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  session_deps_.socket_factory->AddProxyClientSocketDataProvider(&proxy_data);

  // First request would use DIRECT, and succeed.
  StaticSocketDataProvider socket_data_direct_first_request;
  socket_data_direct_first_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_first_request);

  // Second request would use DIRECT, and succeed.
  StaticSocketDataProvider socket_data_direct_second_request;
  socket_data_direct_second_request.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(
      &socket_data_direct_second_request);

  // Now request a stream. It should succeed using the DIRECT.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");

  proxy_resolution_service->SetProxyDelegate(test_proxy_delegate.get());
  Initialize(std::move(proxy_resolution_service));
  EXPECT_EQ(set_alternative_proxy_server,
            test_proxy_delegate_raw->alternative_proxy_server().is_quic());

  // Start two requests. The first request should consume data from
  // |socket_data_proxy_main_job|,
  // |socket_data_proxy_alternate_job| and
  // |socket_data_direct_first_request|. The second request should consume
  // data from |socket_data_direct_second_request|.

  for (size_t i = 0; i < 2; ++i) {
    ProxyInfo used_proxy_info;
    EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _))
        .Times(1)
        .WillOnce(::testing::SaveArg<1>(&used_proxy_info));

    std::unique_ptr<HttpStreamRequest> request =
        CreateJobController(request_info);
    base::RunLoop().RunUntilIdle();

    // Verify that request was fetched without proxy.
    EXPECT_TRUE(used_proxy_info.is_direct());

    // The proxies that failed should now be known to the proxy service as
    // bad.
    const ProxyRetryInfoMap& retry_info =
        session_->proxy_resolution_service()->proxy_retry_info();
    EXPECT_THAT(retry_info, SizeIs(set_alternative_proxy_server ? 3 : 2));
    EXPECT_THAT(retry_info, Contains(Key("https://badproxy:99")));
    EXPECT_THAT(retry_info, Contains(Key("https://badfallbackproxy:98")));

    if (set_alternative_proxy_server)
      EXPECT_THAT(retry_info, Contains(Key("quic://badproxy:99")));
  }
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Tests that ERR_MSG_TOO_BIG is retryable for QUIC proxy.
TEST_F(JobControllerReconsiderProxyAfterErrorTest, ReconsiderErrMsgTooBig) {
  session_deps_.socket_factory->UseMockProxyClientSockets();
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ProxyResolutionService::CreateFixedFromPacResult(
          "QUIC badproxy:99; DIRECT", TRAFFIC_ANNOTATION_FOR_TESTS);

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

  // Now request a stream. It should fallback to DIRECT on ERR_MSG_TOO_BIG.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");

  Initialize(std::move(proxy_resolution_service));

  ProxyInfo used_proxy_info;
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _))
      .Times(1)
      .WillOnce(::testing::SaveArg<1>(&used_proxy_info));

  std::unique_ptr<HttpStreamRequest> request =
      CreateJobController(request_info);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(used_proxy_info.is_direct());
  const ProxyRetryInfoMap& retry_info =
      session_->proxy_resolution_service()->proxy_retry_info();
  EXPECT_THAT(retry_info, SizeIs(1));
  EXPECT_THAT(retry_info, Contains(Key("quic://badproxy:99")));

  request.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Same as test above except that this is testing the retry behavior for
// non-QUIC proxy on ERR_MSG_TOO_BIG.
TEST_F(JobControllerReconsiderProxyAfterErrorTest,
       DoNotReconsiderErrMsgTooBig) {
  session_deps_.socket_factory->UseMockProxyClientSockets();
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ProxyResolutionService::CreateFixedFromPacResult(
          "HTTPS badproxy:99; DIRECT", TRAFFIC_ANNOTATION_FOR_TESTS);

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(proxy_resolution_service->proxy_retry_info().empty());

  // Mock data for the HTTPS proxy socket.
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ProxyClientSocketDataProvider proxy_data(ASYNC, ERR_MSG_TOO_BIG);
  StaticSocketDataProvider https_proxy_socket;
  https_proxy_socket.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(&https_proxy_socket);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  session_deps_.socket_factory->AddProxyClientSocketDataProvider(&proxy_data);

  // Now request a stream. It should not fallback to DIRECT on ERR_MSG_TOO_BIG.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");

  Initialize(std::move(proxy_resolution_service));

  ProxyInfo used_proxy_info;
  EXPECT_CALL(request_delegate_, OnStreamFailed(ERR_MSG_TOO_BIG, _, _))
      .Times(1);

  std::unique_ptr<HttpStreamRequest> request =
      CreateJobController(request_info);
  base::RunLoop().RunUntilIdle();

  const ProxyRetryInfoMap& retry_info =
      session_->proxy_resolution_service()->proxy_retry_info();
  EXPECT_THAT(retry_info, SizeIs(0));

  request.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Tests that the main (HTTP) job is started after the alternative
// proxy server job has failed. There are 3 jobs in total that are run
// in the following sequence: alternative proxy server job,
// delayed HTTP job with the first proxy server, HTTP job with
// the second proxy configuration. The result of the last job (OK)
// should be returned to the delegate.
TEST_F(JobControllerReconsiderProxyAfterErrorTest,
       SecondMainJobIsStartedAfterAltProxyServerJobFailed) {
  // Configure the proxies and initialize the test.
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ProxyResolutionService::CreateFixedFromPacResult(
          "HTTPS myproxy.org:443; DIRECT", TRAFFIC_ANNOTATION_FOR_TESTS);

  auto test_proxy_delegate = std::make_unique<TestProxyDelegate>();
  test_proxy_delegate->set_alternative_proxy_server(
      ProxyServer::FromPacString("QUIC myproxy.org:443"));

  proxy_resolution_service->SetProxyDelegate(test_proxy_delegate.get());
  Initialize(std::move(proxy_resolution_service));

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromSeconds(100);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("http://www.example.com")), stats1);

  // Prepare the mocked data.
  MockQuicData quic_data;
  quic_data.AddRead(ASYNC, ERR_QUIC_PROTOCOL_ERROR);
  quic_data.AddWrite(ASYNC, OK);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  StaticSocketDataProvider tcp_data_1;
  tcp_data_1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  session_deps_.socket_factory->AddSocketDataProvider(&tcp_data_1);

  StaticSocketDataProvider tcp_data_2;
  tcp_data_2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  session_deps_.socket_factory->AddSocketDataProvider(&tcp_data_2);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  // Create a request.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.example.com");
  AlternativeService alternative_service(kProtoQUIC, "www.example.com", 80);
  SetAlternativeService(request_info, alternative_service);

  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _)).Times(1);
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);

  // Create the job controller.
  std::unique_ptr<HttpStreamRequest> request =
      CreateJobController(request_info);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(tcp_data_1.AllReadDataConsumed());
  EXPECT_TRUE(tcp_data_1.AllWriteDataConsumed());
  EXPECT_TRUE(tcp_data_2.AllReadDataConsumed());
  EXPECT_TRUE(tcp_data_2.AllWriteDataConsumed());
}

TEST_F(HttpStreamFactoryJobControllerTest, OnStreamFailedWithNoAlternativeJob) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, ERR_FAILED));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  // There's no other alternative job. Thus when stream failed, it should
  // notify Request of the stream failure.
  EXPECT_CALL(request_delegate_, OnStreamFailed(ERR_FAILED, _, _)).Times(1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpStreamFactoryJobControllerTest, OnStreamReadyWithNoAlternativeJob) {
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");

  Initialize(request_info);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  // There's no other alternative job. Thus when a stream is ready, it should
  // notify Request.
  EXPECT_TRUE(job_controller_->main_job());

  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));
  base::RunLoop().RunUntilIdle();
}

// Test we cancel Jobs correctly when the Request is explicitly canceled
// before any Job is bound to Request.
TEST_F(HttpStreamFactoryJobControllerTest, CancelJobsBeforeBinding) {
  // Use COLD_START to make the alt job pending.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddRead(SYNCHRONOUS, OK);

  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Reset the Request will cancel all the Jobs since there's no Job determined
  // to serve Request yet and JobController will notify the factory to delete
  // itself upon completion.
  request_.reset();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  session_->http_server_properties()->SetQuicAlternativeService(
      server, alternative_service, expiration,
      {quic::QUIC_VERSION_UNSUPPORTED});

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  request_.reset();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

void HttpStreamFactoryJobControllerTest::TestOnStreamFailedForBothJobs(
    bool alt_job_retried_on_non_default_network) {
  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddConnect(ASYNC, ERR_FAILED);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(ASYNC, ERR_FAILED));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  // The failure of second Job should be reported to Request as there's no more
  // pending Job to serve the Request.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(1);
  base::RunLoop().RunUntilIdle();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// This test verifies that the alternative service is not marked broken if both
// jobs fail, and the alternative job is not retried on the alternate network.
TEST_F(HttpStreamFactoryJobControllerTest,
       OnStreamFailedForBothJobsWithoutQuicRetry) {
  TestOnStreamFailedForBothJobs(false);
}

// This test verifies that the alternative service is not marked broken if both
// jobs fail, and the alternative job is retried on the alternate network.
TEST_F(HttpStreamFactoryJobControllerTest,
       OnStreamFailedForBothJobsWithQuicRetriedOnAlternateNetwork) {
  TestOnStreamFailedForBothJobs(true);
}

void HttpStreamFactoryJobControllerTest::TestAltJobFailsAfterMainJobSucceeded(
    bool alt_job_retried_on_non_default_network) {
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  // Main job succeeds, starts serving Request and it should report status
  // to Request. The alternative job will mark the main job complete and gets
  // orphaned.
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));
  // JobController shouldn't report the status of second job as request
  // is already successfully served.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);

  base::RunLoop().RunUntilIdle();

  // Reset the request as it's been successfully served.
  request_.reset();
  VerifyBrokenAlternateProtocolMapping(request_info, true);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  // Verify the brokenness is not cleared when the default network changes.
  session_->http_server_properties()->OnDefaultNetworkChanged();
  VerifyBrokenAlternateProtocolMapping(request_info, true);
}

// This test verifies that the alternatvie service is marked broken when the
// alternative job fails on default after the main job succeeded.  The
// brokenness should not be cleared when the default network changes.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobFailsOnDefaultNetworkAfterMainJobSucceeded) {
  TestAltJobFailsAfterMainJobSucceeded(false);
}

// This test verifies that the alternatvie service is marked broken when the
// alternative job fails on both networks after the main job succeeded.  The
// brokenness should not be cleared when the default network changes.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobFailsOnBothNetworksAfterMainJobSucceeded) {
  TestAltJobFailsAfterMainJobSucceeded(true);
}

// Tests that when alt job succeeds, main job is destroyed.
TEST_F(HttpStreamFactoryJobControllerTest, AltJobSucceedsMainJobDestroyed) {
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);
  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));

  // Make |alternative_job| succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false, false);
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, http_stream.get()));

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  request_.reset();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Tests that if alt job succeeds and main job is blocked, main job should be
// cancelled immediately. |request_| completion will clean up the JobController.
// Regression test for crbug.com/678768.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsMainJobBlockedControllerDestroyed) {
  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddWrite(SYNCHRONOUS,
                       client_maker_.MakeInitialSettingsPacket(1, nullptr));
  quic_data_->AddRead(ASYNC, OK);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);
  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));

  // |alternative_job| succeeds and should report status to |request_delegate_|.
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Invoke OnRequestComplete() which should delete |job_controller_| from
  // |factory_|.
  request_.reset();
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
  AlternativeService alternative_service(kProtoHTTP2, alternative_host,
                                         alternative_port);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
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

// Tests that if an orphaned job completes after |request_| is gone,
// JobController will be cleaned up.
TEST_F(HttpStreamFactoryJobControllerTest,
       OrphanedJobCompletesControllerDestroyed) {
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  // main job should not be blocked because alt job returned ERR_IO_PENDING.
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));

  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));

  // Complete main job now.
  base::RunLoop().RunUntilIdle();

  // Invoke OnRequestComplete() which should not delete |job_controller_| from
  // |factory_| because alt job is yet to finish.
  request_.reset();
  ASSERT_FALSE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  EXPECT_FALSE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Make |alternative_job| succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false, false);
  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  // This should not call request_delegate_::OnStreamReady.
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig());
  // Make sure that controller does not leak.
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

void HttpStreamFactoryJobControllerTest::TestAltJobSucceedsAfterMainJobFailed(
    bool alt_job_retried_on_non_default_network) {
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // |main_job| fails but should not report status to Request.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  base::RunLoop().RunUntilIdle();
  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  // Make |alternative_job| succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false, false);
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, http_stream.get()));

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig());

  // |alternative_job| succeeds and should report status to Request.
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// This test verifies that the alternative service is not mark broken if the
// alternative job succeeds on the default network after the main job failed.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnDefaultNetworkAfterMainJobFailed) {
  TestAltJobSucceedsAfterMainJobFailed(false);
}

// This test verifies that the alternative service is not mark broken if the
// alternative job succeeds on the alternate network after the main job failed.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnAlternateNetwrokAfterMainJobFailed) {
  TestAltJobSucceedsAfterMainJobFailed(true);
}

void HttpStreamFactoryJobControllerTest::
    TestAltJobSucceedsAfterMainJobSucceeded(
        bool alt_job_retried_on_non_default_network) {
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // |main_job| fails but should not report status to Request.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Run the message loop to make |main_job| succeed and status will be
  // reported to Request.
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));
  base::RunLoop().RunUntilIdle();
  VerifyBrokenAlternateProtocolMapping(request_info, false);

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

  // Make |alternative_job| succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false, false);

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig());

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
}

// This test verifies that the alternative service is not marked broken if the
// alternative job succeeds on the default network after the main job succeeded.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnDefaultNetworkAfterMainJobSucceeded) {
  TestAltJobSucceedsAfterMainJobSucceeded(false);
}

// This test verifies that the alternative service is marked broken until the
// default network changes if the alternative job succeeds on the non-default
// network, which failed on the default network previously, after the main job
// succeeded.  The brokenness should be cleared when the default network
// changes.
TEST_F(HttpStreamFactoryJobControllerTest,
       AltJobSucceedsOnAlternateNetworkAfterMainJobSucceeded) {
  TestAltJobSucceedsAfterMainJobSucceeded(true);
}

void HttpStreamFactoryJobControllerTest::
    TestMainJobSucceedsAfterAltJobSucceeded(
        bool alt_job_retried_on_non_default_network) {
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }
  // Make |alternative_job| succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false, false);
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, http_stream.get()));

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig());

  // Run message loop to make the main job succeed.
  base::RunLoop().RunUntilIdle();
  // If alt job was retried on the alternate network, the alternative service
  // should be marked broken until the default network changes.
  VerifyBrokenAlternateProtocolMapping(request_info,
                                       alt_job_retried_on_non_default_network);
  request_.reset();
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
  TestMainJobSucceedsAfterAltJobSucceeded(false);
}

// This test verifies that the alternative service is marked broken until the
// default network changes if the main job succeeds after the alternative job
// succeeded on the non-default network, i.e., failed on the default network
// previously.  The brokenness should be cleared when the default network
// changes.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobSucceededOnAlternateNetwork) {
  TestAltJobSucceedsAfterMainJobSucceeded(true);
}

void HttpStreamFactoryJobControllerTest::TestMainJobFailsAfterAltJobSucceeded(
    bool alt_job_retried_on_non_default_network) {
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }
  // Make |alternative_job| succeed.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false, false);
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, http_stream.get()));

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig());

  // Run message loop to make the main job fail.
  base::RunLoop().RunUntilIdle();
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// This test verifies that the alternative service is not marked broken if the
// main job fails after the alternative job succeeded on the default network.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobFailsAfterAltJobSucceededOnDefaultNetwork) {
  TestMainJobFailsAfterAltJobSucceeded(false);
}

// This test verifies that the alternative service is not marked broken if the
// main job fails after the alternative job succeeded on the non-default
// network, i.e., failed on the default network previously.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobFailsAfterAltJobSucceededOnAlternateNetwork) {
  TestMainJobFailsAfterAltJobSucceeded(true);
}

void HttpStreamFactoryJobControllerTest::TestMainJobSucceedsAfterAltJobFailed(
    bool alt_job_retried_on_non_default_network) {
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // |alternative_job| fails but should not report status to Request.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);
  // |main_job| succeeds and should report status to Request.
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));

  if (alt_job_retried_on_non_default_network) {
    // Set the alt job as if it failed on the default network and is retired on
    // the alternate network.
    JobControllerPeer::SetAltJobFailedOnDefaultNetwork(job_controller_);
  }

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
  TestMainJobSucceedsAfterAltJobFailed(false);
}

// This test verifies that the alternative service will be marked broken when
// the alternative job fails on both default and alternate networks and main job
// succeeds later.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterAltJobFailedOnBothNetworks) {
  TestMainJobSucceedsAfterAltJobFailed(true);
}

// Verifies that if the alternative job fails due to a connection change event,
// then the alternative service is not marked as broken.
TEST_F(HttpStreamFactoryJobControllerTest,
       MainJobSucceedsAfterConnectionChanged) {
  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddConnect(SYNCHRONOUS, ERR_NETWORK_CHANGED);
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // |alternative_job| fails but should not report status to Request.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);
  // |main_job| succeeds and should report status to Request.
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));
  base::RunLoop().RunUntilIdle();
  request_.reset();

  // Verify that the alternate protocol is not marked as broken.
  VerifyBrokenAlternateProtocolMapping(request_info, false);
  histogram_tester.ExpectUniqueSample("Net.AlternateServiceFailed",
                                      -ERR_NETWORK_CHANGED, 1);
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Regression test for crbug/621069.
// Get load state after main job fails and before alternative job succeeds.
TEST_F(HttpStreamFactoryJobControllerTest, GetLoadStateAfterMainJobFailed) {
  // Use COLD_START to complete alt job manually.
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // |main_job| fails but should not report status to Request.
  // The alternative job will mark the main job complete.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);

  base::RunLoop().RunUntilIdle();

  // Controller should use alternative job to get load state.
  job_controller_->GetLoadState();

  // |alternative_job| succeeds and should report status to Request.
  auto http_stream = std::make_unique<HttpBasicStream>(
      std::make_unique<ClientSocketHandle>(), false, false);
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, http_stream.get()));

  HttpStreamFactoryJobPeer::SetStream(job_factory_.alternative_job(),
                                      std::move(http_stream));
  job_controller_->OnStreamReady(job_factory_.alternative_job(), SSLConfig());

  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerTest, ResumeMainJobWhenAltJobStalls) {
  // Use COLD_START to stall alt job.
  quic_data_ = std::make_unique<MockQuicData>();
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Alt job is stalled and main job should complete successfully.
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));

  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpStreamFactoryJobControllerTest, InvalidPortForQuic) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  // Using a restricted port 101 for QUIC should fail and the alternative job
  // should post OnStreamFailedCall on the controller to resume the main job.
  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 101);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
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
  session_deps_.host_resolver = std::move(hanging_resolver);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data;
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));

  // Since the alt job has not finished host resolution, there should be no
  // delayed task posted to resume the main job.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::TimeDelta::FromMicroseconds(50));
  EXPECT_TRUE(JobControllerPeer::main_job_is_blocked(job_controller_));

  // Allow alt job host resolution to complete.
  session_deps_.host_resolver->ResolveAllPending();

  // Task to resume main job in 15 microseconds should be posted.
  EXPECT_TRUE(MainThreadHasPendingTask());
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::TimeDelta::FromMicroseconds(14));
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  FastForwardBy(base::TimeDelta::FromMicroseconds(1));

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  EXPECT_TRUE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Unpause mock quic data.
  // Will cause |alternative_job| to fail, but its failure should not be
  // reported to Request.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);
  // OnStreamFailed will post a task to resume the main job immediately but
  // won't call Resume() on the main job since it's been resumed already.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  quic_data.GetSequencedSocketData()->Resume();
  FastForwardUntilNoTasksRemain();
  // Alt job should be cleaned up
  EXPECT_FALSE(job_controller_->alternative_job());
}

TEST_F(HttpStreamFactoryJobControllerTest, DelayedTCP) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // Handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data;
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  // Main job is not blocked but hasn't resumed yet; it should resume in 15us.
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  EXPECT_FALSE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Task to resume main job in 15us should be posted.
  EXPECT_TRUE(MainThreadHasPendingTask());
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::TimeDelta::FromMicroseconds(14));
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  FastForwardBy(base::TimeDelta::FromMicroseconds(1));

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Unpause mock quic data and run all remaining tasks. Alt-job should fail
  // and be cleaned up.
  quic_data.GetSequencedSocketData()->Resume();
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(job_controller_->alternative_job());
}

// Regression test for crbug.com/789560.
TEST_F(HttpStreamFactoryJobControllerTest, ResumeMainJobLaterCanceled) {
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ProxyResolutionService::CreateDirect();
  ProxyResolutionService* proxy_resolution_service_raw =
      proxy_resolution_service.get();
  session_deps_.proxy_resolution_service = std::move(proxy_resolution_service);

  // Using hanging resolver will cause the alternative job to hang indefinitely.
  session_deps_.host_resolver = std::make_unique<HangingResolver>();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());

  base::RunLoop run_loop;
  // The main job should be resumed without delay when alt job fails.
  EXPECT_CALL(*job_factory_.main_job(), Resume())
      .Times(1)
      .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
  job_controller_->OnStreamFailed(job_factory_.alternative_job(),
                                  ERR_QUIC_PROTOCOL_ERROR, SSLConfig());
  FastForwardBy(base::TimeDelta::FromMicroseconds(0));
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
      alternative_service);

  job_controller_->OnStreamFailed(job_factory_.main_job(), ERR_FAILED,
                                  SSLConfig());
  // Jobs are restarted.
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());

  // There shouldn't be any ResumeMainJobLater() delayed tasks.
  // This EXPECT_CALL will fail before crbug.com/789560 fix.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::TimeDelta::FromMicroseconds(15));

  EXPECT_TRUE(job_controller_->main_job());
  request_.reset();
}

// Test that main job is blocked for kMaxDelayTimeForMainJob(3s) if
// http_server_properties cached an inappropriate large srtt for the server,
// which would potentially delay the main job for a extremely long time in
// delayed tcp case.
TEST_F(HttpStreamFactoryJobControllerTest, DelayedTCPWithLargeSrtt) {
  // The max delay time should be in sync with .cc file.
  base::TimeDelta kMaxDelayTimeForMainJob = base::TimeDelta::FromSeconds(3);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data;
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromSeconds(100);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  // Main job is not blocked but hasn't resumed yet; it should resume in 3s.
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  EXPECT_FALSE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Task to resume main job in 3 seconds should be posted.
  EXPECT_TRUE(MainThreadHasPendingTask());
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(kMaxDelayTimeForMainJob - base::TimeDelta::FromMicroseconds(1));
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  FastForwardBy(base::TimeDelta::FromMicroseconds(1));

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Unpause mock quic data and run all remaining tasks. Alt-job  should fail
  // and be cleaned up.
  quic_data.GetSequencedSocketData()->Resume();
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(job_controller_->alternative_job());
}

TEST_F(HttpStreamFactoryJobControllerTest,
       ResumeMainJobImmediatelyOnStreamFailed) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data;
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://www.google.com")), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  // Main job is not blocked but hasn't resumed yet; it's scheduled to resume
  // in 15us.
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  EXPECT_FALSE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Task to resume main job in 15us should be posted.
  EXPECT_TRUE(MainThreadHasPendingTask());

  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::TimeDelta::FromMicroseconds(1));

  // Now unpause the mock quic data to fail the alt job. This should immediately
  // resume the main job.
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  quic_data.GetSequencedSocketData()->Resume();
  FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Verify there is another task to resume main job with delay but should
  // not call Resume() on the main job as main job has been resumed.
  EXPECT_TRUE(MainThreadHasPendingTask());
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::TimeDelta::FromMicroseconds(15));

  FastForwardUntilNoTasksRemain();
}

// Verifies that the alternative proxy server job is not created if the URL
// scheme is HTTPS.
TEST_F(HttpStreamFactoryJobControllerTest, HttpsURL) {
  // Using hanging resolver will cause the alternative job to hang indefinitely.
  session_deps_.host_resolver = std::make_unique<HangingResolver>();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://mail.example.org/");
  Initialize(request_info);
  EXPECT_TRUE(test_proxy_delegate()->alternative_proxy_server().is_quic());

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());
  EXPECT_FALSE(job_controller_->alternative_job());

  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  base::RunLoop().RunUntilIdle();
}

// Verifies that the alternative proxy server job is not created if the main job
// does not fetch the resource through a proxy.
TEST_F(HttpStreamFactoryJobControllerTest, HttpURLWithNoProxy) {
  // Using hanging resolver will cause the alternative job to hang indefinitely.
  session_deps_.host_resolver = std::make_unique<HangingResolver>();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://mail.example.org/");

  Initialize(request_info);
  EXPECT_TRUE(test_proxy_delegate()->alternative_proxy_server().is_quic());

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_FALSE(job_controller_->main_job()->is_waiting());
  EXPECT_FALSE(job_controller_->alternative_job());

  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  base::RunLoop().RunUntilIdle();
}

// Verifies that the main job is resumed properly after a delay when the
// alternative proxy server job hangs.
TEST_F(HttpStreamFactoryJobControllerTest, DelayedTCPAlternativeProxy) {
  UseAlternativeProxy();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.mail.example.org/");

  Initialize(request_info);

  EXPECT_TRUE(test_proxy_delegate()->alternative_proxy_server().is_quic());

  // Handshake will fail asynchronously after mock data is unpaused.
  MockQuicData quic_data;
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);  // Pause
  quic_data.AddRead(ASYNC, ERR_FAILED);
  quic_data.AddWrite(ASYNC, ERR_FAILED);
  quic_data.AddSocketDataToFactory(session_deps_.socket_factory.get());

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://myproxy.org")), stats1);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // This prevents handshake from immediately succeeding.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(job_controller_->main_job()->is_waiting());
  // Main job is not blocked but hasn't resumed yet; it should resume in 15us.
  EXPECT_FALSE(JobControllerPeer::main_job_is_blocked(job_controller_));
  EXPECT_FALSE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Task to resume main job in 15us should be posted.
  EXPECT_TRUE(MainThreadHasPendingTask());
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(0);
  FastForwardBy(base::TimeDelta::FromMicroseconds(14));
  EXPECT_CALL(*job_factory_.main_job(), Resume()).Times(1);
  FastForwardBy(base::TimeDelta::FromMicroseconds(1));

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());
  EXPECT_TRUE(JobControllerPeer::main_job_is_resumed(job_controller_));

  // Unpause mock quic data and run all remaining tasks. Alt-job should fail
  // and be cleaned up.
  quic_data.GetSequencedSocketData()->Resume();
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(job_controller_->alternative_job());
}

// Verifies that if the alternative proxy server job fails immediately, the
// main job is not blocked.
TEST_F(HttpStreamFactoryJobControllerTest, FailAlternativeProxy) {
  session_deps_.socket_factory->UseMockProxyClientSockets();
  ProxyClientSocketDataProvider proxy_data(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddProxyClientSocketDataProvider(&proxy_data);

  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddConnect(SYNCHRONOUS, ERR_FAILED);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  UseAlternativeProxy();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://mail.example.org/");
  Initialize(request_info);
  EXPECT_TRUE(test_proxy_delegate()->alternative_proxy_server().is_quic());
  EXPECT_THAT(session_->proxy_resolution_service()->proxy_retry_info(),
              IsEmpty());

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(300 * 1000);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://myproxy.org")), stats1);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_TRUE(job_controller_->main_job());

  // The alternative proxy server should be marked as bad.
  EXPECT_THAT(session_->proxy_resolution_service()->proxy_retry_info(),
              ElementsAre(Key("quic://myproxy.org:443")));
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

// Verifies that if the alternative proxy server job fails due to network
// disconnection, then the proxy delegate is not notified.
TEST_F(HttpStreamFactoryJobControllerTest,
       InternetDisconnectedAlternativeProxy) {
  session_deps_.socket_factory->UseMockProxyClientSockets();
  ProxyClientSocketDataProvider proxy_data(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddProxyClientSocketDataProvider(&proxy_data);

  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddConnect(SYNCHRONOUS, ERR_INTERNET_DISCONNECTED);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  UseAlternativeProxy();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://mail.example.org/");
  Initialize(request_info);
  EXPECT_TRUE(test_proxy_delegate()->alternative_proxy_server().is_quic());

  // Enable delayed TCP and set time delay for waiting job.
  QuicStreamFactory* quic_stream_factory = session_->quic_stream_factory();
  quic_stream_factory->set_require_confirmation(false);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(300 * 1000);
  session_->http_server_properties()->SetServerNetworkStats(
      url::SchemeHostPort(GURL("https://myproxy.org")), stats1);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(job_controller_->alternative_job());
  EXPECT_TRUE(job_controller_->main_job());

  // The alternative proxy server should not be marked as bad.
  EXPECT_TRUE(test_proxy_delegate()->alternative_proxy_server().is_valid());
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
}

TEST_F(HttpStreamFactoryJobControllerTest,
       AlternativeProxyServerJobFailsAfterMainJobSucceeds) {
  base::HistogramTester histogram_tester;

  session_deps_.socket_factory->UseMockProxyClientSockets();
  ProxyClientSocketDataProvider proxy_data(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddProxyClientSocketDataProvider(&proxy_data);

  // Use COLD_START to make the alt job pending.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START);
  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  UseAlternativeProxy();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // Main job succeeds, starts serving Request and it should report status
  // to Request. The alternative job will mark the main job complete and gets
  // orphaned.
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_TRUE(job_controller_->alternative_job());

  // JobController shouldn't report the status of alternative server job as
  // request is already successfully served.
  EXPECT_CALL(request_delegate_, OnStreamFailed(_, _, _)).Times(0);
  job_controller_->OnStreamFailed(job_factory_.alternative_job(), ERR_FAILED,
                                  SSLConfig());

  // Reset the request as it's been successfully served.
  request_.reset();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));

  histogram_tester.ExpectUniqueSample("Net.QuicAlternativeProxy.Usage",
                                      2 /* ALTERNATIVE_PROXY_USAGE_LOST_RACE */,
                                      1);
}

TEST_F(HttpStreamFactoryJobControllerTest, PreconnectToHostWithValidAltSvc) {
  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddWrite(SYNCHRONOUS,
                       client_maker_.MakeInitialSettingsPacket(1, nullptr));
  quic_data_->AddRead(ASYNC, OK);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  SetPreconnect();

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  job_controller_->Preconnect(1);
  EXPECT_TRUE(job_controller_->main_job());
  EXPECT_EQ(HttpStreamFactory::PRECONNECT,
            job_controller_->main_job()->job_type());
  EXPECT_FALSE(job_controller_->alternative_job());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
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
  session_->http_server_properties()->SetSupportsSpdy(server, true);

  job_controller_->Preconnect(/*num_streams=*/5);
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

class JobControllerLimitMultipleH2Requests
    : public HttpStreamFactoryJobControllerTest {
 protected:
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
  ssl_data.next_proto = kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  Initialize(request_info);
  SpdySessionPoolPeer pool_peer(session_->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(server, true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  for (int i = 0; i < kNumRequests; ++i) {
    request_delegates.emplace_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    HttpStreamFactory::JobController* job_controller =
        new HttpStreamFactory::JobController(
            factory_, request_delegates[i].get(), session_.get(), &job_factory_,
            request_info, is_preconnect_, false /* is_websocket */,
            enable_ip_based_pooling_, enable_alternative_services_, SSLConfig(),
            SSLConfig());
    HttpStreamFactoryPeer::AddJobController(factory_, job_controller);
    auto request = job_controller->Start(
        request_delegates[i].get(), nullptr, net_log_.bound(),
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_TRUE(job_controller->main_job());
    EXPECT_FALSE(job_controller->alternative_job());
    requests.push_back(std::move(request));
  }

  for (int i = 0; i < kNumRequests; ++i) {
    EXPECT_CALL(*request_delegates[i].get(), OnStreamReadyImpl(_, _, _));
  }

  base::RunLoop().RunUntilIdle();
  requests.clear();
  EXPECT_TRUE(HttpStreamFactoryPeer::IsJobControllerDeleted(factory_));
  TestNetLogEntry::List entries;
  size_t log_position = 0;
  for (int i = 0; i < kNumRequests - 1; ++i) {
    net_log_.GetEntries(&entries);
    log_position = ExpectLogContainsSomewhereAfter(
        entries, log_position, NetLogEventType::HTTP_STREAM_JOB_THROTTLED,
        NetLogEventPhase::NONE);
  }
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
    ssl_socket_data.back().next_proto = kProtoHTTP2;
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
  session_->http_server_properties()->SetSupportsSpdy(server, true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  for (int i = 0; i < kNumRequests; ++i) {
    request_delegates.push_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    HttpStreamFactory::JobController* job_controller =
        new HttpStreamFactory::JobController(
            factory_, request_delegates[i].get(), session_.get(), &job_factory_,
            request_info, is_preconnect_, false /* is_websocket */,
            enable_ip_based_pooling_, enable_alternative_services_, SSLConfig(),
            SSLConfig());
    HttpStreamFactoryPeer::AddJobController(factory_, job_controller);
    auto request = job_controller->Start(
        request_delegates[i].get(), nullptr, net_log_.bound(),
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_TRUE(job_controller->main_job());
    EXPECT_FALSE(job_controller->alternative_job());
    requests.push_back(std::move(request));
  }

  for (int i = 0; i < kNumRequests; ++i) {
    EXPECT_CALL(*request_delegates[i].get(), OnStreamReadyImpl(_, _, _));
  }

  EXPECT_TRUE(MainThreadHasPendingTask());
  FastForwardBy(base::TimeDelta::FromMilliseconds(
      HttpStreamFactory::Job::kHTTP2ThrottleMs));
  base::RunLoop().RunUntilIdle();

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
  first_ssl_data.next_proto = kProtoHTTP2;
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
    ssl_socket_data.back().next_proto = kProtoHTTP2;
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
  session_->http_server_properties()->SetSupportsSpdy(server, true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  for (int i = 0; i < kNumRequests; ++i) {
    request_delegates.emplace_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    HttpStreamFactory::JobController* job_controller =
        new HttpStreamFactory::JobController(
            factory_, request_delegates[i].get(), session_.get(), &job_factory_,
            request_info, is_preconnect_, false /* is_websocket */,
            enable_ip_based_pooling_, enable_alternative_services_, SSLConfig(),
            SSLConfig());
    HttpStreamFactoryPeer::AddJobController(factory_, job_controller);
    auto request = job_controller->Start(
        request_delegates[i].get(), nullptr, net_log_.bound(),
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_TRUE(job_controller->main_job());
    EXPECT_FALSE(job_controller->alternative_job());
    requests.push_back(std::move(request));
  }
  // Cancel the first one.
  requests[0].reset();

  for (int i = 1; i < kNumRequests; ++i) {
    EXPECT_CALL(*request_delegates[i].get(), OnStreamReadyImpl(_, _, _));
  }
  base::RunLoop().RunUntilIdle();

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
  ssl_data.next_proto = kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  SetPreconnect();
  Initialize(request_info);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(server, true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  for (int i = 0; i < kNumRequests; ++i) {
    request_delegates.emplace_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    HttpStreamFactory::JobController* job_controller =
        new HttpStreamFactory::JobController(
            factory_, request_delegates[i].get(), session_.get(), &job_factory_,
            request_info, is_preconnect_, false /* is_websocket */,
            enable_ip_based_pooling_, enable_alternative_services_, SSLConfig(),
            SSLConfig());
    HttpStreamFactoryPeer::AddJobController(factory_, job_controller);
    job_controller->Preconnect(1);
    EXPECT_TRUE(job_controller->main_job());
    EXPECT_FALSE(job_controller->alternative_job());
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
  second_ssl_data.next_proto = kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&second_ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.example.com");
  Initialize(request_info);
  SpdySessionPoolPeer pool_peer(session_->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);

  // Sets server support HTTP/2.
  url::SchemeHostPort server(request_info.url);
  session_->http_server_properties()->SetSupportsSpdy(server, true);

  std::vector<std::unique_ptr<MockHttpStreamRequestDelegate>> request_delegates;
  std::vector<std::unique_ptr<HttpStreamRequest>> requests;
  for (int i = 0; i < 2; ++i) {
    request_delegates.emplace_back(
        std::make_unique<MockHttpStreamRequestDelegate>());
    HttpStreamFactory::JobController* job_controller =
        new HttpStreamFactory::JobController(
            factory_, request_delegates[i].get(), session_.get(), &job_factory_,
            request_info, is_preconnect_, false /* is_websocket */,
            enable_ip_based_pooling_, enable_alternative_services_, SSLConfig(),
            SSLConfig());
    HttpStreamFactoryPeer::AddJobController(factory_, job_controller);
    auto request = job_controller->Start(
        request_delegates[i].get(), nullptr, net_log_.bound(),
        HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
    EXPECT_TRUE(job_controller->main_job());
    EXPECT_FALSE(job_controller->alternative_job());
    requests.push_back(std::move(request));
  }

  for (int i = 0; i < 2; ++i) {
    EXPECT_CALL(*request_delegates[i].get(), OnStreamReadyImpl(_, _, _));
  }
  base::RunLoop().RunUntilIdle();

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
  quic_data_ = std::make_unique<MockQuicData>();
  quic_data_->AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  tcp_data_ =
      std::make_unique<SequencedSocketData>(reads, base::span<MockWrite>());

  tcp_data_->set_connect_data(MockConnect(ASYNC, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.next_proto = kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);
  SpdySessionPoolPeer pool_peer(session_->spdy_session_pool());
  pool_peer.SetEnableSendingInitialData(false);

  url::SchemeHostPort server(request_info.url);
  // Sets server supports QUIC.
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  // Sets server support HTTP/2.
  session_->http_server_properties()->SetSupportsSpdy(server, true);

  // Use default job factory so that Resume() is not mocked out.
  HttpStreamFactory::JobFactory default_job_factory;
  HttpStreamFactory::JobController* job_controller =
      new HttpStreamFactory::JobController(
          factory_, &request_delegate_, session_.get(), &default_job_factory,
          request_info, is_preconnect_, false /* is_websocket */,
          enable_ip_based_pooling_, enable_alternative_services_, SSLConfig(),
          SSLConfig());
  HttpStreamFactoryPeer::AddJobController(factory_, job_controller);
  request_ =
      job_controller->Start(&request_delegate_, nullptr, net_log_.bound(),
                            HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);

  EXPECT_TRUE(job_controller->main_job());
  EXPECT_TRUE(job_controller->alternative_job());
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));
  base::RunLoop().RunUntilIdle();
  TestNetLogEntry::List entries;
  net_log_.GetEntries(&entries);
  for (auto entry : entries) {
    ASSERT_NE(NetLogEventType::HTTP_STREAM_JOB_THROTTLED, entry.type);
  }
}

class HttpStreamFactoryJobControllerMisdirectedRequestRetry
    : public HttpStreamFactoryJobControllerTest,
      public ::testing::WithParamInterface<::testing::tuple<bool, bool>> {};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    HttpStreamFactoryJobControllerMisdirectedRequestRetry,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()));

TEST_P(HttpStreamFactoryJobControllerMisdirectedRequestRetry,
       DisableIPBasedPoolingAndAlternativeServices) {
  const bool enable_ip_based_pooling = ::testing::get<0>(GetParam());
  const bool enable_alternative_services = ::testing::get<1>(GetParam());
  if (enable_alternative_services) {
    quic_data_ = std::make_unique<MockQuicData>();
    quic_data_->AddConnect(SYNCHRONOUS, OK);
    quic_data_->AddWrite(SYNCHRONOUS,
                         client_maker_.MakeInitialSettingsPacket(1, nullptr));
    quic_data_->AddRead(ASYNC, OK);
  }
  tcp_data_ = std::make_unique<SequencedSocketData>();
  tcp_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  if (!enable_ip_based_pooling)
    DisableIPBasedPooling();
  if (!enable_alternative_services)
    DisableAlternativeServices();

  Initialize(request_info);

  url::SchemeHostPort server(request_info.url);
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  SetAlternativeService(request_info, alternative_service);

  request_ =
      job_controller_->Start(&request_delegate_, nullptr, net_log_.bound(),
                             HttpStreamRequest::HTTP_STREAM, DEFAULT_PRIORITY);
  EXPECT_TRUE(job_controller_->main_job());
  if (enable_alternative_services) {
    EXPECT_TRUE(job_controller_->alternative_job());
  } else {
    EXPECT_FALSE(job_controller_->alternative_job());
  }

  // |main_job| succeeds and should report status to Request.
  EXPECT_CALL(request_delegate_, OnStreamReadyImpl(_, _, _));
  base::RunLoop().RunUntilIdle();
}

class HttpStreamFactoryJobControllerPreconnectTest
    : public HttpStreamFactoryJobControllerTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    if (!GetParam()) {
      scoped_feature_list_.InitFromCommandLine(std::string(),
                                               "LimitEarlyPreconnects");
    }
  }

  void Initialize() {
    session_deps_.http_server_properties =
        std::make_unique<MockHttpServerProperties>();
    session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    factory_ = session_->http_stream_factory();
    request_info_.method = "GET";
    request_info_.url = GURL("https://www.example.com");
    job_controller_ = new HttpStreamFactory::JobController(
        factory_, &request_delegate_, session_.get(), &job_factory_,
        request_info_, /* is_preconnect = */ true,
        /* is_websocket = */ false,
        /* enable_ip_based_pooling = */ true,
        /* enable_alternative_services = */ true, SSLConfig(), SSLConfig());
    HttpStreamFactoryPeer::AddJobController(factory_, job_controller_);
  }

 protected:
  void Preconnect(int num_streams) {
    job_controller_->Preconnect(num_streams);
    // Only one job is started.
    EXPECT_TRUE(job_controller_->main_job());
    EXPECT_FALSE(job_controller_->alternative_job());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  HttpRequestInfo request_info_;
};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
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
  AlternativeService alternative_service(kProtoQUIC, server.host(), 443);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);

  // Set alternative service with no advertised version.
  session_->http_server_properties()->SetQuicAlternativeService(
      server, alternative_service, expiration,
      quic::QuicTransportVersionVector());

  AlternativeServiceInfo alt_svc_info =
      JobControllerPeer::GetAlternativeServiceInfoFor(
          job_controller_, request_info, &request_delegate_,
          HttpStreamRequest::HTTP_STREAM);
  // Verify that JobController get an empty list of supported QUIC versions.
  EXPECT_TRUE(alt_svc_info.advertised_versions().empty());

  // Set alternative service for the same server with the same list of versions
  // that is supported.
  quic::QuicTransportVersionVector supported_versions =
      session_->params().quic_supported_versions;
  ASSERT_TRUE(session_->http_server_properties()->SetQuicAlternativeService(
      server, alternative_service, expiration, supported_versions));

  alt_svc_info = JobControllerPeer::GetAlternativeServiceInfoFor(
      job_controller_, request_info, &request_delegate_,
      HttpStreamRequest::HTTP_STREAM);
  std::sort(supported_versions.begin(), supported_versions.end());
  EXPECT_EQ(supported_versions, alt_svc_info.advertised_versions());

  quic::QuicTransportVersion unsupported_version_1(
      quic::QUIC_VERSION_UNSUPPORTED);
  quic::QuicTransportVersion unsupported_version_2(
      quic::QUIC_VERSION_UNSUPPORTED);
  for (const quic::QuicTransportVersion& version :
       quic::AllSupportedTransportVersions()) {
    if (std::find(supported_versions.begin(), supported_versions.end(),
                  version) != supported_versions.end())
      continue;
    if (unsupported_version_1 == quic::QUIC_VERSION_UNSUPPORTED) {
      unsupported_version_1 = version;
      continue;
    }
    unsupported_version_2 = version;
    break;
  }

  // Set alternative service for the same server with two QUIC versions:
  // - one unsupported version: |unsupported_version_1|,
  // - one supported version: session_->params().quic_supported_versions[0].
  quic::QuicTransportVersionVector mixed_quic_versions = {
      unsupported_version_1, session_->params().quic_supported_versions[0]};
  ASSERT_TRUE(session_->http_server_properties()->SetQuicAlternativeService(
      server, alternative_service, expiration, mixed_quic_versions));

  alt_svc_info = JobControllerPeer::GetAlternativeServiceInfoFor(
      job_controller_, request_info, &request_delegate_,
      HttpStreamRequest::HTTP_STREAM);
  EXPECT_EQ(2u, alt_svc_info.advertised_versions().size());
  // Verify that JobController returns the list of versions specified in set.
  std::sort(mixed_quic_versions.begin(), mixed_quic_versions.end());
  EXPECT_EQ(mixed_quic_versions, alt_svc_info.advertised_versions());

  // Set alternative service for the same server with two unsupported QUIC
  // versions: |unsupported_version_1|, |unsupported_version_2|.
  ASSERT_TRUE(session_->http_server_properties()->SetQuicAlternativeService(
      server, alternative_service, expiration,
      {unsupported_version_1, unsupported_version_2}));

  alt_svc_info = JobControllerPeer::GetAlternativeServiceInfoFor(
      job_controller_, request_info, &request_delegate_,
      HttpStreamRequest::HTTP_STREAM);
  // Verify that JobController returns no valid alternative service.
  EXPECT_EQ(kProtoUnknown, alt_svc_info.alternative_service().protocol);
  EXPECT_EQ(0u, alt_svc_info.advertised_versions().size());
}

// Tests that if HttpNetworkSession has a non-empty QUIC host whitelist,
// then GetAlternativeServiceFor() will not return any QUIC alternative service
// that's not on the whitelist.
TEST_F(HttpStreamFactoryJobControllerTest, QuicHostWhitelist) {
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");

  Initialize(request_info);

  // Set HttpNetworkSession's QUIC host whitelist to only have www.example.com
  HttpNetworkSessionPeer session_peer(session_.get());
  session_peer.params()->quic_host_whitelist.insert("www.example.com");
  session_peer.params()->quic_allow_remote_alt_svc = true;

  // Set alternative service for www.google.com to be www.example.com over QUIC.
  url::SchemeHostPort server(request_info.url);
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  quic::QuicTransportVersionVector supported_versions =
      session_->params().quic_supported_versions;
  session_->http_server_properties()->SetQuicAlternativeService(
      server, AlternativeService(kProtoQUIC, "www.example.com", 443),
      expiration, supported_versions);

  AlternativeServiceInfo alt_svc_info =
      JobControllerPeer::GetAlternativeServiceInfoFor(
          job_controller_, request_info, &request_delegate_,
          HttpStreamRequest::HTTP_STREAM);

  std::sort(supported_versions.begin(), supported_versions.end());
  EXPECT_EQ(kProtoQUIC, alt_svc_info.alternative_service().protocol);
  EXPECT_EQ(supported_versions, alt_svc_info.advertised_versions());

  session_->http_server_properties()->SetQuicAlternativeService(
      server, AlternativeService(kProtoQUIC, "www.example.org", 443),
      expiration, supported_versions);

  alt_svc_info = JobControllerPeer::GetAlternativeServiceInfoFor(
      job_controller_, request_info, &request_delegate_,
      HttpStreamRequest::HTTP_STREAM);

  EXPECT_EQ(kProtoUnknown, alt_svc_info.alternative_service().protocol);
  EXPECT_EQ(0u, alt_svc_info.advertised_versions().size());
}

}  // namespace test

}  // namespace net
