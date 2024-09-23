// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_connect_job.h"

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {
namespace {

IPAddress ParseIP(const std::string& ip) {
  IPAddress address;
  CHECK(address.AssignFromIPLiteral(ip));
  return address;
}

// Just check that all connect times are set to base::TimeTicks::Now(), for
// tests that don't update the mocked out time.
void CheckConnectTimesSet(const LoadTimingInfo::ConnectTiming& connect_timing) {
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.domain_lookup_start);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.domain_lookup_end);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.connect_start);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.ssl_start);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.ssl_end);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.connect_end);
}

// Just check that all connect times are set to base::TimeTicks::Now(), except
// for DNS times, for tests that don't update the mocked out time and use a
// proxy.
void CheckConnectTimesExceptDnsSet(
    const LoadTimingInfo::ConnectTiming& connect_timing) {
  EXPECT_TRUE(connect_timing.domain_lookup_start.is_null());
  EXPECT_TRUE(connect_timing.domain_lookup_end.is_null());
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.connect_start);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.ssl_start);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.ssl_end);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.connect_end);
}

const url::SchemeHostPort kHostHttps{url::kHttpsScheme, "host", 443};
const HostPortPair kHostHttp{"host", 80};
const ProxyServer kSocksProxyServer{ProxyServer::SCHEME_SOCKS5,
                                    HostPortPair("sockshost", 443)};
const ProxyServer kHttpProxyServer{ProxyServer::SCHEME_HTTP,
                                   HostPortPair("proxy", 443)};

const ProxyChain kHttpProxyChain{kHttpProxyServer};

class SSLConnectJobTest : public WithTaskEnvironment, public testing::Test {
 public:
  SSLConnectJobTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        ssl_config_service_(
            std::make_unique<TestSSLConfigService>(SSLContextConfig())),
        http_auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()),
        session_(CreateNetworkSession()),
        common_connect_job_params_(session_->CreateCommonConnectJobParams()) {}

  ~SSLConnectJobTest() override = default;

  scoped_refptr<TransportSocketParams> CreateDirectTransportSocketParams(
      SecureDnsPolicy secure_dns_policy) const {
    return base::MakeRefCounted<TransportSocketParams>(
        kHostHttps, NetworkAnonymizationKey(), secure_dns_policy,
        OnHostResolutionCallback(),
        /*supported_alpns=*/base::flat_set<std::string>({"h2", "http/1.1"}));
  }

  scoped_refptr<TransportSocketParams> CreateProxyTransportSocketParams(
      SecureDnsPolicy secure_dns_policy) const {
    return base::MakeRefCounted<TransportSocketParams>(
        kHttpProxyServer.host_port_pair(), NetworkAnonymizationKey(),
        secure_dns_policy, OnHostResolutionCallback(),
        /*supported_alpns=*/base::flat_set<std::string>({}));
  }

  scoped_refptr<SOCKSSocketParams> CreateSOCKSSocketParams(
      SecureDnsPolicy secure_dns_policy) {
    return base::MakeRefCounted<SOCKSSocketParams>(
        ConnectJobParams(CreateProxyTransportSocketParams(secure_dns_policy)),
        kSocksProxyServer.scheme() == ProxyServer::SCHEME_SOCKS5,
        kSocksProxyServer.host_port_pair(), NetworkAnonymizationKey(),
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  scoped_refptr<HttpProxySocketParams> CreateHttpProxySocketParams(
      SecureDnsPolicy secure_dns_policy) {
    return base::MakeRefCounted<HttpProxySocketParams>(
        ConnectJobParams(CreateProxyTransportSocketParams(secure_dns_policy)),
        kHostHttp, kHttpProxyChain,
        /*proxy_server_index=*/0,
        /*tunnel=*/true, TRAFFIC_ANNOTATION_FOR_TESTS,
        NetworkAnonymizationKey(), secure_dns_policy);
  }

  std::unique_ptr<ConnectJob> CreateConnectJob(
      TestConnectJobDelegate* test_delegate,
      ProxyChain proxy_chain = ProxyChain::Direct(),
      RequestPriority priority = DEFAULT_PRIORITY,
      SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow) {
    return std::make_unique<SSLConnectJob>(
        priority, SocketTag(), &common_connect_job_params_,
        CreateSSLSocketParams(proxy_chain, secure_dns_policy), test_delegate,
        /*net_log=*/nullptr);
  }

  scoped_refptr<SSLSocketParams> CreateSSLSocketParams(
      ProxyChain proxy_chain,
      SecureDnsPolicy secure_dns_policy) {
    return base::MakeRefCounted<SSLSocketParams>(
        proxy_chain == ProxyChain::Direct()
            ? ConnectJobParams(
                  CreateDirectTransportSocketParams(secure_dns_policy))
        : proxy_chain.is_single_proxy() &&
                proxy_chain.First().scheme() == ProxyServer::SCHEME_SOCKS5
            ? ConnectJobParams(CreateSOCKSSocketParams(secure_dns_policy))
        : proxy_chain.is_single_proxy() &&
                proxy_chain.First().scheme() == ProxyServer::SCHEME_HTTP
            ? ConnectJobParams(CreateHttpProxySocketParams(secure_dns_policy))
            : ConnectJobParams(),
        HostPortPair::FromSchemeHostPort(kHostHttps), SSLConfig(),
        NetworkAnonymizationKey());
  }

  void AddAuthToCache() {
    const std::u16string kFoo(u"foo");
    const std::u16string kBar(u"bar");
    session_->http_auth_cache()->Add(
        url::SchemeHostPort(GURL("http://proxy:443/")), HttpAuth::AUTH_PROXY,
        "MyRealm1", HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
        "Basic realm=MyRealm1", AuthCredentials(kFoo, kBar), "/");
  }

  std::unique_ptr<HttpNetworkSession> CreateNetworkSession() {
    HttpNetworkSessionContext session_context;
    session_context.host_resolver = &host_resolver_;
    session_context.cert_verifier = &cert_verifier_;
    session_context.transport_security_state = &transport_security_state_;
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.client_socket_factory = &socket_factory_;
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.http_auth_handler_factory =
        http_auth_handler_factory_.get();
    session_context.http_server_properties = &http_server_properties_;
    session_context.http_user_agent_settings = &http_user_agent_settings_;
    session_context.quic_context = &quic_context_;
    return std::make_unique<HttpNetworkSession>(HttpNetworkSessionParams(),
                                                session_context);
  }

 protected:
  MockClientSocketFactory socket_factory_;
  MockHostResolver host_resolver_{/*default_result=*/MockHostResolverBase::
                                      RuleResolver::GetLocalhostResult()};
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  const std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  const std::unique_ptr<TestSSLConfigService> ssl_config_service_;
  const std::unique_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  const StaticHttpUserAgentSettings http_user_agent_settings_ = {"*",
                                                                 "test-ua"};
  HttpServerProperties http_server_properties_;
  QuicContext quic_context_;
  const std::unique_ptr<HttpNetworkSession> session_;

  const CommonConnectJobParams common_connect_job_params_;
};

TEST_F(SSLConnectJobTest, TCPFail) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    host_resolver_.set_synchronous_mode(io_mode == SYNCHRONOUS);
    StaticSocketDataProvider data;
    data.set_connect_data(MockConnect(io_mode, ERR_CONNECTION_FAILED));
    socket_factory_.AddSocketDataProvider(&data);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> ssl_connect_job =
        CreateConnectJob(&test_delegate);
    test_delegate.StartJobExpectingResult(
        ssl_connect_job.get(), ERR_CONNECTION_FAILED, io_mode == SYNCHRONOUS);
    EXPECT_FALSE(test_delegate.socket());
    EXPECT_FALSE(ssl_connect_job->IsSSLError());
    ConnectionAttempts connection_attempts =
        ssl_connect_job->GetConnectionAttempts();
    ASSERT_EQ(1u, connection_attempts.size());
    EXPECT_THAT(connection_attempts[0].result,
                test::IsError(ERR_CONNECTION_FAILED));
  }
}

TEST_F(SSLConnectJobTest, TCPTimeout) {
  const base::TimeDelta kTinyTime = base::Microseconds(1);

  // Make request hang.
  host_resolver_.set_ondemand_mode(true);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);
  ASSERT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));

  // Right up until just before the TCP connection timeout, the job does not
  // time out.
  FastForwardBy(TransportConnectJob::ConnectionTimeout() - kTinyTime);
  EXPECT_FALSE(test_delegate.has_result());

  // But at the exact time of TCP connection timeout, the job fails.
  FastForwardBy(kTinyTime);
  EXPECT_TRUE(test_delegate.has_result());
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
}

TEST_F(SSLConnectJobTest, SSLTimeoutSyncConnect) {
  const base::TimeDelta kTinyTime = base::Microseconds(1);

  // DNS lookup and transport connect complete synchronously, but SSL
  // negotiation hangs.
  host_resolver_.set_synchronous_mode(true);
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  // Make request hang.
  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);
  ASSERT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));

  // Right up until just before the SSL handshake timeout, the job does not time
  // out.
  FastForwardBy(SSLConnectJob::HandshakeTimeoutForTesting() - kTinyTime);
  EXPECT_FALSE(test_delegate.has_result());

  // But at the exact SSL handshake timeout time, the job fails.
  FastForwardBy(kTinyTime);
  EXPECT_TRUE(test_delegate.has_result());
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
}

TEST_F(SSLConnectJobTest, SSLTimeoutAsyncTcpConnect) {
  const base::TimeDelta kTinyTime = base::Microseconds(1);

  // DNS lookup is asynchronous, and later SSL negotiation hangs.
  host_resolver_.set_ondemand_mode(true);
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);
  // Connecting should hand on the TransportConnectJob connect.
  ASSERT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));

  // Right up until just before the TCP connection timeout, the job does not
  // time out.
  FastForwardBy(TransportConnectJob::ConnectionTimeout() - kTinyTime);
  EXPECT_FALSE(test_delegate.has_result());

  // The DNS lookup completes, and a TCP connection is immediately establshed,
  // which cancels the TCP connection timer. The SSL handshake timer is started,
  // and the SSL handshake hangs.
  host_resolver_.ResolveOnlyRequestNow();
  EXPECT_FALSE(test_delegate.has_result());

  // Right up until just before the SSL handshake timeout, the job does not time
  // out.
  FastForwardBy(SSLConnectJob::HandshakeTimeoutForTesting() - kTinyTime);
  EXPECT_FALSE(test_delegate.has_result());

  // But at the exact SSL handshake timeout time, the job fails.
  FastForwardBy(kTinyTime);
  EXPECT_TRUE(test_delegate.has_result());
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
}

TEST_F(SSLConnectJobTest, BasicDirectSync) {
  host_resolver_.set_synchronous_mode(true);
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);

  test_delegate.StartJobExpectingResult(ssl_connect_job.get(), OK,
                                        true /* expect_sync_result */);
  EXPECT_EQ(MEDIUM, host_resolver_.last_request_priority());

  ConnectionAttempts connection_attempts =
      ssl_connect_job->GetConnectionAttempts();
  EXPECT_EQ(0u, connection_attempts.size());
  CheckConnectTimesSet(ssl_connect_job->connect_timing());
}

TEST_F(SSLConnectJobTest, BasicDirectAsync) {
  host_resolver_.set_ondemand_mode(true);
  base::TimeTicks start_time = base::TimeTicks::Now();
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_TRUE(host_resolver_.has_pending_requests());
  EXPECT_EQ(MEDIUM, host_resolver_.last_request_priority());
  FastForwardBy(base::Seconds(5));

  base::TimeTicks resolve_complete_time = base::TimeTicks::Now();
  host_resolver_.ResolveAllPending();
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());

  ConnectionAttempts connection_attempts =
      ssl_connect_job->GetConnectionAttempts();
  EXPECT_EQ(0u, connection_attempts.size());

  // Check times. Since time is mocked out, all times will be the same, except
  // |dns_start|, which is the only one recorded before the FastForwardBy()
  // call. The test classes don't allow any other phases to be triggered on
  // demand, or delayed by a set interval.
  EXPECT_EQ(start_time, ssl_connect_job->connect_timing().domain_lookup_start);
  EXPECT_EQ(resolve_complete_time,
            ssl_connect_job->connect_timing().domain_lookup_end);
  EXPECT_EQ(resolve_complete_time,
            ssl_connect_job->connect_timing().connect_start);
  EXPECT_EQ(resolve_complete_time, ssl_connect_job->connect_timing().ssl_start);
  EXPECT_EQ(resolve_complete_time, ssl_connect_job->connect_timing().ssl_end);
  EXPECT_EQ(resolve_complete_time,
            ssl_connect_job->connect_timing().connect_end);
}

TEST_F(SSLConnectJobTest, DirectHasEstablishedConnection) {
  host_resolver_.set_ondemand_mode(true);
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory_.AddSocketDataProvider(&data);

  // SSL negotiation hangs. Value returned after SSL negotiation is complete
  // doesn't matter, as HasEstablishedConnection() may only be used between job
  // start and job complete.
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_TRUE(host_resolver_.has_pending_requests());
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, ssl_connect_job->GetLoadState());
  EXPECT_FALSE(ssl_connect_job->HasEstablishedConnection());

  // DNS resolution completes, and then the ConnectJob tries to connect the
  // socket, which should succeed asynchronously.
  host_resolver_.ResolveNow(1);
  EXPECT_EQ(LOAD_STATE_CONNECTING, ssl_connect_job->GetLoadState());
  EXPECT_FALSE(ssl_connect_job->HasEstablishedConnection());

  // Spinning the message loop causes the socket to finish connecting. The SSL
  // handshake should start and hang.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_SSL_HANDSHAKE, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());
}

TEST_F(SSLConnectJobTest, RequestPriority) {
  host_resolver_.set_ondemand_mode(true);
  for (int initial_priority = MINIMUM_PRIORITY;
       initial_priority <= MAXIMUM_PRIORITY; ++initial_priority) {
    SCOPED_TRACE(initial_priority);
    for (int new_priority = MINIMUM_PRIORITY; new_priority <= MAXIMUM_PRIORITY;
         ++new_priority) {
      SCOPED_TRACE(new_priority);
      if (initial_priority == new_priority) {
        continue;
      }
      TestConnectJobDelegate test_delegate;
      std::unique_ptr<ConnectJob> ssl_connect_job =
          CreateConnectJob(&test_delegate, ProxyChain::Direct(),
                           static_cast<RequestPriority>(initial_priority));
      EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
      EXPECT_TRUE(host_resolver_.has_pending_requests());
      int request_id = host_resolver_.num_resolve();
      EXPECT_EQ(initial_priority, host_resolver_.request_priority(request_id));

      ssl_connect_job->ChangePriority(
          static_cast<RequestPriority>(new_priority));
      EXPECT_EQ(new_priority, host_resolver_.request_priority(request_id));

      ssl_connect_job->ChangePriority(
          static_cast<RequestPriority>(initial_priority));
      EXPECT_EQ(initial_priority, host_resolver_.request_priority(request_id));
    }
  }
}

TEST_F(SSLConnectJobTest, SecureDnsPolicy) {
  for (auto secure_dns_policy :
       {SecureDnsPolicy::kAllow, SecureDnsPolicy::kDisable}) {
    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> ssl_connect_job =
        CreateConnectJob(&test_delegate, ProxyChain::Direct(), DEFAULT_PRIORITY,
                         secure_dns_policy);

    EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
    EXPECT_EQ(secure_dns_policy, host_resolver_.last_secure_dns_policy());
  }
}

TEST_F(SSLConnectJobTest, DirectHostResolutionFailure) {
  host_resolver_.rules()->AddSimulatedTimeoutFailure("host");

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct());
  test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                        ERR_NAME_NOT_RESOLVED,
                                        false /* expect_sync_result */);
  EXPECT_THAT(ssl_connect_job->GetResolveErrorInfo().error,
              test::IsError(ERR_DNS_TIMED_OUT));
}

TEST_F(SSLConnectJobTest, DirectCertError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_COMMON_NAME_INVALID);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate(
      TestConnectJobDelegate::SocketExpected::ALWAYS);
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);

  test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                        ERR_CERT_COMMON_NAME_INVALID,
                                        false /* expect_sync_result */);
  EXPECT_TRUE(ssl_connect_job->IsSSLError());
  ConnectionAttempts connection_attempts =
      ssl_connect_job->GetConnectionAttempts();
  ASSERT_EQ(1u, connection_attempts.size());
  EXPECT_THAT(connection_attempts[0].result,
              test::IsError(ERR_CERT_COMMON_NAME_INVALID));
  CheckConnectTimesSet(ssl_connect_job->connect_timing());
}

TEST_F(SSLConnectJobTest, DirectIgnoreCertErrors) {
  session_->IgnoreCertificateErrorsForTesting();

  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expected_ignore_certificate_errors = true;
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate(
      TestConnectJobDelegate::SocketExpected::ALWAYS);
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);

  test_delegate.StartJobExpectingResult(ssl_connect_job.get(), OK,
                                        /*expect_sync_result=*/false);
}

TEST_F(SSLConnectJobTest, DirectSSLError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_BAD_SSL_CLIENT_AUTH_CERT);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);

  test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                        ERR_BAD_SSL_CLIENT_AUTH_CERT,
                                        false /* expect_sync_result */);
  ConnectionAttempts connection_attempts =
      ssl_connect_job->GetConnectionAttempts();
  ASSERT_EQ(1u, connection_attempts.size());
  EXPECT_THAT(connection_attempts[0].result,
              test::IsError(ERR_BAD_SSL_CLIENT_AUTH_CERT));
}

TEST_F(SSLConnectJobTest, DirectWithNPN) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = kProtoHTTP11;
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);

  test_delegate.StartJobExpectingResult(ssl_connect_job.get(), OK,
                                        false /* expect_sync_result */);
  CheckConnectTimesSet(ssl_connect_job->connect_timing());
}

TEST_F(SSLConnectJobTest, DirectGotHTTP2) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = kProtoHTTP2;
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);

  test_delegate.StartJobExpectingResult(ssl_connect_job.get(), OK,
                                        false /* expect_sync_result */);
  EXPECT_EQ(kProtoHTTP2, test_delegate.socket()->GetNegotiatedProtocol());
  CheckConnectTimesSet(ssl_connect_job->connect_timing());
}

TEST_F(SSLConnectJobTest, SOCKSFail) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    host_resolver_.set_synchronous_mode(io_mode == SYNCHRONOUS);
    StaticSocketDataProvider data;
    data.set_connect_data(MockConnect(io_mode, ERR_CONNECTION_FAILED));
    socket_factory_.AddSocketDataProvider(&data);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
        &test_delegate, PacResultElementToProxyChain("SOCKS5 foo:333"));
    test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                          ERR_PROXY_CONNECTION_FAILED,
                                          io_mode == SYNCHRONOUS);
    EXPECT_FALSE(ssl_connect_job->IsSSLError());

    ConnectionAttempts connection_attempts =
        ssl_connect_job->GetConnectionAttempts();
    EXPECT_EQ(0u, connection_attempts.size());
  }
}

TEST_F(SSLConnectJobTest, SOCKSHostResolutionFailure) {
  host_resolver_.rules()->AddSimulatedTimeoutFailure("proxy");

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
      &test_delegate, PacResultElementToProxyChain("SOCKS5 foo:333"));
  test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                        ERR_PROXY_CONNECTION_FAILED,
                                        false /* expect_sync_result */);
  EXPECT_THAT(ssl_connect_job->GetResolveErrorInfo().error,
              test::IsError(ERR_DNS_TIMED_OUT));
}

TEST_F(SSLConnectJobTest, SOCKSBasic) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    const uint8_t kSOCKS5Request[] = {0x05, 0x01, 0x00, 0x03, 0x09, 's',
                                      'o',  'c',  'k',  's',  'h',  'o',
                                      's',  't',  0x01, 0xBB};

    MockWrite writes[] = {
        MockWrite(io_mode, kSOCKS5GreetRequest, kSOCKS5GreetRequestLength),
        MockWrite(io_mode, reinterpret_cast<const char*>(kSOCKS5Request),
                  std::size(kSOCKS5Request)),
    };

    MockRead reads[] = {
        MockRead(io_mode, kSOCKS5GreetResponse, kSOCKS5GreetResponseLength),
        MockRead(io_mode, kSOCKS5OkResponse, kSOCKS5OkResponseLength),
    };

    host_resolver_.set_synchronous_mode(io_mode == SYNCHRONOUS);
    StaticSocketDataProvider data(reads, writes);
    data.set_connect_data(MockConnect(io_mode, OK));
    socket_factory_.AddSocketDataProvider(&data);
    SSLSocketDataProvider ssl(io_mode, OK);
    socket_factory_.AddSSLSocketDataProvider(&ssl);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
        &test_delegate, PacResultElementToProxyChain("SOCKS5 foo:333"));
    test_delegate.StartJobExpectingResult(ssl_connect_job.get(), OK,
                                          io_mode == SYNCHRONOUS);
    CheckConnectTimesExceptDnsSet(ssl_connect_job->connect_timing());

    // Proxies should not set any DNS aliases.
    EXPECT_TRUE(test_delegate.socket()->GetDnsAliases().empty());
  }
}

TEST_F(SSLConnectJobTest, SOCKSHasEstablishedConnection) {
  const uint8_t kSOCKS5Request[] = {0x05, 0x01, 0x00, 0x03, 0x09, 's',
                                    'o',  'c',  'k',  's',  'h',  'o',
                                    's',  't',  0x01, 0xBB};

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kSOCKS5GreetRequest, kSOCKS5GreetRequestLength, 0),
      MockWrite(SYNCHRONOUS, reinterpret_cast<const char*>(kSOCKS5Request),
                std::size(kSOCKS5Request), 3),
  };

  MockRead reads[] = {
      // Pause so can probe current state.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, kSOCKS5GreetResponse, kSOCKS5GreetResponseLength, 2),
      MockRead(SYNCHRONOUS, kSOCKS5OkResponse, kSOCKS5OkResponseLength, 4),
  };

  host_resolver_.set_ondemand_mode(true);
  SequencedSocketData data(reads, writes);
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory_.AddSocketDataProvider(&data);

  // SSL negotiation hangs. Value returned after SSL negotiation is complete
  // doesn't matter, as HasEstablishedConnection() may only be used between job
  // start and job complete.
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
      &test_delegate, PacResultElementToProxyChain("SOCKS5 foo:333"));
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_TRUE(host_resolver_.has_pending_requests());
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, ssl_connect_job->GetLoadState());
  EXPECT_FALSE(ssl_connect_job->HasEstablishedConnection());

  // DNS resolution completes, and then the ConnectJob tries to connect the
  // socket, which should succeed asynchronously.
  host_resolver_.ResolveNow(1);
  EXPECT_EQ(LOAD_STATE_CONNECTING, ssl_connect_job->GetLoadState());
  EXPECT_FALSE(ssl_connect_job->HasEstablishedConnection());

  // Spin the message loop until the first read of the handshake.
  // HasEstablishedConnection() should return true, as a TCP connection has been
  // successfully established by this point.
  data.RunUntilPaused();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_CONNECTING, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Finish up the handshake, and spin the message loop until the SSL handshake
  // starts and hang.
  data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_SSL_HANDSHAKE, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());
}

TEST_F(SSLConnectJobTest, SOCKSRequestPriority) {
  host_resolver_.set_ondemand_mode(true);
  for (int initial_priority = MINIMUM_PRIORITY;
       initial_priority <= MAXIMUM_PRIORITY; ++initial_priority) {
    SCOPED_TRACE(initial_priority);
    for (int new_priority = MINIMUM_PRIORITY; new_priority <= MAXIMUM_PRIORITY;
         ++new_priority) {
      SCOPED_TRACE(new_priority);
      if (initial_priority == new_priority) {
        continue;
      }
      TestConnectJobDelegate test_delegate;
      std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
          &test_delegate, PacResultElementToProxyChain("SOCKS5 foo:333"),
          static_cast<RequestPriority>(initial_priority));
      EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
      EXPECT_TRUE(host_resolver_.has_pending_requests());
      int request_id = host_resolver_.num_resolve();
      EXPECT_EQ(initial_priority, host_resolver_.request_priority(request_id));

      ssl_connect_job->ChangePriority(
          static_cast<RequestPriority>(new_priority));
      EXPECT_EQ(new_priority, host_resolver_.request_priority(request_id));

      ssl_connect_job->ChangePriority(
          static_cast<RequestPriority>(initial_priority));
      EXPECT_EQ(initial_priority, host_resolver_.request_priority(request_id));
    }
  }
}

TEST_F(SSLConnectJobTest, HttpProxyFail) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    host_resolver_.set_synchronous_mode(io_mode == SYNCHRONOUS);
    StaticSocketDataProvider data;
    data.set_connect_data(MockConnect(io_mode, ERR_CONNECTION_FAILED));
    socket_factory_.AddSocketDataProvider(&data);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
        &test_delegate, PacResultElementToProxyChain("PROXY foo:444"));
    test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                          ERR_PROXY_CONNECTION_FAILED,
                                          io_mode == SYNCHRONOUS);

    EXPECT_FALSE(ssl_connect_job->IsSSLError());
    ConnectionAttempts connection_attempts =
        ssl_connect_job->GetConnectionAttempts();
    EXPECT_EQ(0u, connection_attempts.size());
  }
}

TEST_F(SSLConnectJobTest, HttpProxyHostResolutionFailure) {
  host_resolver_.rules()->AddSimulatedTimeoutFailure("proxy");

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
      &test_delegate, PacResultElementToProxyChain("PROXY foo:444"));
  test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                        ERR_PROXY_CONNECTION_FAILED,
                                        false /* expect_sync_result */);
  EXPECT_THAT(ssl_connect_job->GetResolveErrorInfo().error,
              test::IsError(ERR_DNS_TIMED_OUT));
}

TEST_F(SSLConnectJobTest, HttpProxyAuthChallenge) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(ASYNC, 5,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead(ASYNC, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
      MockRead(ASYNC, 2, "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead(ASYNC, 3, "Content-Length: 10\r\n\r\n"),
      MockRead(ASYNC, 4, "0123456789"),
      MockRead(ASYNC, 6, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, writes);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
      &test_delegate, PacResultElementToProxyChain("PROXY foo:444"));
  ASSERT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  test_delegate.WaitForAuthChallenge(1);

  EXPECT_EQ(407, test_delegate.auth_response_info().headers->response_code());
  std::string proxy_authenticate;
  ASSERT_TRUE(test_delegate.auth_response_info().headers->EnumerateHeader(
      nullptr, "Proxy-Authenticate", &proxy_authenticate));
  EXPECT_EQ(proxy_authenticate, "Basic realm=\"MyRealm1\"");

  // While waiting for auth credentials to be provided, the Job should not time
  // out.
  FastForwardBy(base::Days(1));
  test_delegate.WaitForAuthChallenge(1);
  EXPECT_FALSE(test_delegate.has_result());

  // Respond to challenge.
  test_delegate.auth_controller()->ResetAuth(AuthCredentials(u"foo", u"bar"));
  test_delegate.RunAuthCallback();

  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());

  // Proxies should not set any DNS aliases.
  EXPECT_TRUE(test_delegate.socket()->GetDnsAliases().empty());
}

TEST_F(SSLConnectJobTest, HttpProxyAuthWithCachedCredentials) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    host_resolver_.set_synchronous_mode(io_mode == SYNCHRONOUS);
    MockWrite writes[] = {
        MockWrite(io_mode,
                  "CONNECT host:80 HTTP/1.1\r\n"
                  "Host: host:80\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n"
                  "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };
    MockRead reads[] = {
        MockRead(io_mode, "HTTP/1.1 200 Connection Established\r\n\r\n"),
    };
    StaticSocketDataProvider data(reads, writes);
    data.set_connect_data(MockConnect(io_mode, OK));
    socket_factory_.AddSocketDataProvider(&data);
    AddAuthToCache();
    SSLSocketDataProvider ssl(io_mode, OK);
    socket_factory_.AddSSLSocketDataProvider(&ssl);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
        &test_delegate, PacResultElementToProxyChain("PROXY foo:444"));
    test_delegate.StartJobExpectingResult(ssl_connect_job.get(), OK,
                                          io_mode == SYNCHRONOUS);
    CheckConnectTimesExceptDnsSet(ssl_connect_job->connect_timing());
    EXPECT_TRUE(test_delegate.socket()->GetDnsAliases().empty());
  }
}

TEST_F(SSLConnectJobTest, HttpProxyRequestPriority) {
  host_resolver_.set_ondemand_mode(true);
  for (int initial_priority = MINIMUM_PRIORITY;
       initial_priority <= MAXIMUM_PRIORITY; ++initial_priority) {
    SCOPED_TRACE(initial_priority);
    for (int new_priority = MINIMUM_PRIORITY; new_priority <= MAXIMUM_PRIORITY;
         ++new_priority) {
      SCOPED_TRACE(new_priority);
      if (initial_priority == new_priority) {
        continue;
      }
      TestConnectJobDelegate test_delegate;
      std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
          &test_delegate, PacResultElementToProxyChain("PROXY foo:444"),
          static_cast<RequestPriority>(initial_priority));
      EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
      EXPECT_TRUE(host_resolver_.has_pending_requests());
      int request_id = host_resolver_.num_resolve();
      EXPECT_EQ(initial_priority, host_resolver_.request_priority(request_id));

      ssl_connect_job->ChangePriority(
          static_cast<RequestPriority>(new_priority));
      EXPECT_EQ(new_priority, host_resolver_.request_priority(request_id));

      ssl_connect_job->ChangePriority(
          static_cast<RequestPriority>(initial_priority));
      EXPECT_EQ(initial_priority, host_resolver_.request_priority(request_id));
    }
  }
}

TEST_F(SSLConnectJobTest, HttpProxyAuthHasEstablishedConnection) {
  host_resolver_.set_ondemand_mode(true);
  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(ASYNC, 3,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      // Pause reading.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 2,
               "HTTP/1.1 407 Proxy Authentication Required\r\n"
               "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"
               "Content-Length: 0\r\n\r\n"),
      // Pause reading.
      MockRead(ASYNC, ERR_IO_PENDING, 4),
      MockRead(ASYNC, 5, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  SequencedSocketData data(reads, writes);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
      &test_delegate, PacResultElementToProxyChain("PROXY foo:444"));
  ASSERT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_TRUE(host_resolver_.has_pending_requests());
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, ssl_connect_job->GetLoadState());
  EXPECT_FALSE(ssl_connect_job->HasEstablishedConnection());

  // DNS resolution completes, and then the ConnectJob tries to connect the
  // socket, which should succeed asynchronously.
  host_resolver_.ResolveOnlyRequestNow();
  EXPECT_EQ(LOAD_STATE_CONNECTING, ssl_connect_job->GetLoadState());
  EXPECT_FALSE(ssl_connect_job->HasEstablishedConnection());

  // Spinning the message loop causes the connection to be established and the
  // nested HttpProxyConnectJob to start establishing a tunnel.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
            ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Receive the auth challenge.
  data.Resume();
  test_delegate.WaitForAuthChallenge(1);
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_IDLE, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Respond to challenge.
  test_delegate.auth_controller()->ResetAuth(AuthCredentials(u"foo", u"bar"));
  test_delegate.RunAuthCallback();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
            ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Run until the next read pauses.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
            ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Receive the connection established response, at which point SSL negotiation
  // finally starts.
  data.Resume();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_SSL_HANDSHAKE, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
}

TEST_F(SSLConnectJobTest,
       HttpProxyAuthHasEstablishedConnectionWithProxyConnectionClose) {
  host_resolver_.set_ondemand_mode(true);
  MockWrite writes1[] = {
      MockWrite(ASYNC, 0,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
  };
  MockRead reads1[] = {
      // Pause reading.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 2,
               "HTTP/1.1 407 Proxy Authentication Required\r\n"
               "Proxy-Connection: Close\r\n"
               "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"
               "Content-Length: 0\r\n\r\n"),
  };
  SequencedSocketData data1(reads1, writes1);
  socket_factory_.AddSocketDataProvider(&data1);

  MockWrite writes2[] = {
      MockWrite(ASYNC, 0,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads2[] = {
      // Pause reading.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 2, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  SequencedSocketData data2(reads2, writes2);
  socket_factory_.AddSocketDataProvider(&data2);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job = CreateConnectJob(
      &test_delegate, PacResultElementToProxyChain("PROXY foo:444"));
  ASSERT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_TRUE(host_resolver_.has_pending_requests());
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, ssl_connect_job->GetLoadState());
  EXPECT_FALSE(ssl_connect_job->HasEstablishedConnection());

  // DNS resolution completes, and then the ConnectJob tries to connect the
  // socket, which should succeed asynchronously.
  host_resolver_.ResolveOnlyRequestNow();
  EXPECT_EQ(LOAD_STATE_CONNECTING, ssl_connect_job->GetLoadState());
  EXPECT_FALSE(ssl_connect_job->HasEstablishedConnection());

  // Spinning the message loop causes the connection to be established and the
  // nested HttpProxyConnectJob to start establishing a tunnel.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
            ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Receive the auth challenge.
  data1.Resume();
  test_delegate.WaitForAuthChallenge(1);
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_IDLE, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Respond to challenge.
  test_delegate.auth_controller()->ResetAuth(AuthCredentials(u"foo", u"bar"));
  test_delegate.RunAuthCallback();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
            ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Run until the next DNS lookup.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(host_resolver_.has_pending_requests());
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // DNS resolution completes, and then the ConnectJob tries to connect the
  // socket, which should succeed asynchronously.
  host_resolver_.ResolveOnlyRequestNow();
  EXPECT_EQ(LOAD_STATE_CONNECTING, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Spinning the message loop causes the connection to be established and the
  // nested HttpProxyConnectJob to start establishing a tunnel.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
            ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  // Receive the connection established response, at which point SSL negotiation
  // finally starts.
  data2.Resume();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_SSL_HANDSHAKE, ssl_connect_job->GetLoadState());
  EXPECT_TRUE(ssl_connect_job->HasEstablishedConnection());

  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
}

TEST_F(SSLConnectJobTest, DnsAliases) {
  host_resolver_.set_synchronous_mode(true);

  // Resolve an AddressList with DNS aliases.
  std::vector<std::string> aliases({"alias1", "alias2", "host"});
  host_resolver_.rules()->AddIPLiteralRuleWithDnsAliases("host", "2.2.2.2",
                                                         std::move(aliases));
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);
  TestConnectJobDelegate test_delegate;

  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);

  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();

  // Verify that the elements of the alias list are those from the
  // parameter vector.
  EXPECT_THAT(test_delegate.socket()->GetDnsAliases(),
              testing::ElementsAre("alias1", "alias2", "host"));
}

TEST_F(SSLConnectJobTest, NoAdditionalDnsAliases) {
  host_resolver_.set_synchronous_mode(true);

  // Resolve an AddressList without additional DNS aliases. (The parameter
  // is an empty vector.)
  std::vector<std::string> aliases;
  host_resolver_.rules()->AddIPLiteralRuleWithDnsAliases("host", "2.2.2.2",
                                                         std::move(aliases));
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);
  TestConnectJobDelegate test_delegate;

  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);

  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));

  base::RunLoop().RunUntilIdle();

  // Verify that the alias list only contains "host".
  EXPECT_THAT(test_delegate.socket()->GetDnsAliases(),
              testing::ElementsAre("host"));
}

// Test that `SSLConnectJob` passes the ECHConfigList from DNS to
// `SSLClientSocket`.
TEST_F(SSLConnectJobTest, EncryptedClientHello) {
  std::vector<uint8_t> ech_config_list1, ech_config_list2;
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list1));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list2));

  // Configure two HTTPS RR routes, to test we pass the correct one.
  HostResolverEndpointResult endpoint1, endpoint2;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint1.metadata.ech_config_list = ech_config_list1;
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint2.metadata.ech_config_list = ech_config_list2;
  host_resolver_.rules()->AddRule(
      "host", MockHostResolverBase::RuleResolver::RuleResult(
                  std::vector{endpoint1, endpoint2}));

  for (bool ech_enabled : {true, false}) {
    SCOPED_TRACE(ech_enabled);
    SSLContextConfig config;
    config.ech_enabled = ech_enabled;
    ssl_config_service_->UpdateSSLConfigAndNotify(config);

    // The first connection attempt will be to `endpoint1`, which will fail.
    StaticSocketDataProvider data1;
    data1.set_expected_addresses(AddressList(endpoint1.ip_endpoints));
    data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
    socket_factory_.AddSocketDataProvider(&data1);
    // The second connection attempt will be to `endpoint2`, which will succeed.
    StaticSocketDataProvider data2;
    data2.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
    data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
    socket_factory_.AddSocketDataProvider(&data2);
    // The handshake then succeeds.
    SSLSocketDataProvider ssl2(ASYNC, OK);
    // The ECH configuration should be passed if and only if the feature is
    // enabled.
    ssl2.expected_ech_config_list =
        ech_enabled ? ech_config_list2 : std::vector<uint8_t>{};
    socket_factory_.AddSSLSocketDataProvider(&ssl2);

    // The connection should ultimately succeed.
    base::HistogramTester histogram_tester;
    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> ssl_connect_job =
        CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
    EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
    EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());

    // Whether or not the feature is enabled, we should record data for the
    // ECH-capable server.
    histogram_tester.ExpectUniqueSample("Net.SSL_Connection_Error_ECH", OK, 1);
    histogram_tester.ExpectTotalCount("Net.SSL_Connection_Latency_ECH", 1);
    // The ECH result should only be recorded if ECH was actually enabled.
    if (ech_enabled) {
      histogram_tester.ExpectUniqueSample("Net.SSL.ECHResult",
                                          0 /* kSuccessInitial */, 1);
    } else {
      histogram_tester.ExpectTotalCount("Net.SSL.ECHResult", 0);
    }
  }
}

// Test that `SSLConnectJob` retries the connection if there was a stale ECH
// configuration.
TEST_F(SSLConnectJobTest, ECHStaleConfig) {
  std::vector<uint8_t> ech_config_list1, ech_config_list2, ech_config_list3;
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list1));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list2));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list3));

  // Configure two HTTPS RR routes, to test the retry uses the correct one.
  HostResolverEndpointResult endpoint1, endpoint2;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint1.metadata.ech_config_list = ech_config_list1;
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint2.metadata.ech_config_list = ech_config_list2;
  host_resolver_.rules()->AddRule(
      "host", MockHostResolverBase::RuleResolver::RuleResult(
                  std::vector{endpoint1, endpoint2}));

  // The first connection attempt will be to `endpoint1`, which will fail.
  StaticSocketDataProvider data1;
  data1.set_expected_addresses(AddressList(endpoint1.ip_endpoints));
  data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  socket_factory_.AddSocketDataProvider(&data1);
  // The second connection attempt will be to `endpoint2`, which will succeed.
  StaticSocketDataProvider data2;
  data2.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data2);
  // The handshake will then fail, but then provide retry configs.
  SSLSocketDataProvider ssl2(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl2.expected_ech_config_list = ech_config_list2;
  ssl2.ech_retry_configs = ech_config_list3;
  socket_factory_.AddSSLSocketDataProvider(&ssl2);
  // The third connection attempt should skip `endpoint1` and retry with only
  // `endpoint2`.
  StaticSocketDataProvider data3;
  data3.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data3.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data3);
  // The handshake should be passed the retry configs.
  SSLSocketDataProvider ssl3(ASYNC, OK);
  ssl3.expected_ech_config_list = ech_config_list3;
  socket_factory_.AddSSLSocketDataProvider(&ssl3);

  // The connection should ultimately succeed.
  base::HistogramTester histogram_tester;
  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());

  histogram_tester.ExpectUniqueSample("Net.SSL.ECHResult",
                                      2 /* kSuccessRetry */, 1);
}

// Test that `SSLConnectJob` retries the connection given a secure rollback
// signal.
TEST_F(SSLConnectJobTest, ECHRollback) {
  std::vector<uint8_t> ech_config_list1, ech_config_list2;
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list1));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list2));

  // Configure two HTTPS RR routes, to test the retry uses the correct one.
  HostResolverEndpointResult endpoint1, endpoint2;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint1.metadata.ech_config_list = ech_config_list1;
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint2.metadata.ech_config_list = ech_config_list2;
  host_resolver_.rules()->AddRule(
      "host", MockHostResolverBase::RuleResolver::RuleResult(
                  std::vector{endpoint1, endpoint2}));

  // The first connection attempt will be to `endpoint1`, which will fail.
  StaticSocketDataProvider data1;
  data1.set_expected_addresses(AddressList(endpoint1.ip_endpoints));
  data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  socket_factory_.AddSocketDataProvider(&data1);
  // The second connection attempt will be to `endpoint2`, which will succeed.
  StaticSocketDataProvider data2;
  data2.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data2);
  // The handshake will then fail, and provide no retry configs.
  SSLSocketDataProvider ssl2(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl2.expected_ech_config_list = ech_config_list2;
  ssl2.ech_retry_configs = std::vector<uint8_t>();
  socket_factory_.AddSSLSocketDataProvider(&ssl2);
  // The third connection attempt should skip `endpoint1` and retry with only
  // `endpoint2`.
  StaticSocketDataProvider data3;
  data3.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data3.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data3);
  // The handshake should not be passed ECH configs.
  SSLSocketDataProvider ssl3(ASYNC, OK);
  ssl3.expected_ech_config_list = std::vector<uint8_t>();
  socket_factory_.AddSSLSocketDataProvider(&ssl3);

  // The connection should ultimately succeed.
  base::HistogramTester histogram_tester;
  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());

  histogram_tester.ExpectUniqueSample("Net.SSL.ECHResult",
                                      4 /* kSuccessRollback */, 1);
}

// Test that `SSLConnectJob` will not retry more than once.
TEST_F(SSLConnectJobTest, ECHTooManyRetries) {
  std::vector<uint8_t> ech_config_list1, ech_config_list2, ech_config_list3;
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list1));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list2));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list3));

  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint.metadata.ech_config_list = ech_config_list1;
  host_resolver_.rules()->AddRule(
      "host",
      MockHostResolverBase::RuleResolver::RuleResult(std::vector{endpoint}));

  // The first connection attempt will succeed.
  StaticSocketDataProvider data1;
  data1.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data1);
  // The handshake will then fail, but provide retry configs.
  SSLSocketDataProvider ssl1(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl1.expected_ech_config_list = ech_config_list1;
  ssl1.ech_retry_configs = ech_config_list2;
  socket_factory_.AddSSLSocketDataProvider(&ssl1);
  // The second connection attempt will succeed.
  StaticSocketDataProvider data2;
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data2);
  // The handshake will then fail, but provide new retry configs.
  SSLSocketDataProvider ssl2(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl2.expected_ech_config_list = ech_config_list2;
  ssl2.ech_retry_configs = ech_config_list3;
  socket_factory_.AddSSLSocketDataProvider(&ssl2);
  // There will be no third connection attempt.

  base::HistogramTester histogram_tester;
  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(),
              test::IsError(ERR_ECH_NOT_NEGOTIATED));

  histogram_tester.ExpectUniqueSample("Net.SSL.ECHResult", 3 /* kErrorRetry */,
                                      1);
}

// Test that `SSLConnectJob` will not retry for ECH given the wrong error.
TEST_F(SSLConnectJobTest, ECHWrongRetryError) {
  std::vector<uint8_t> ech_config_list1, ech_config_list2;
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list1));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list2));

  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint.metadata.ech_config_list = ech_config_list1;
  host_resolver_.rules()->AddRule(
      "host",
      MockHostResolverBase::RuleResolver::RuleResult(std::vector{endpoint}));

  // The first connection attempt will succeed.
  StaticSocketDataProvider data1;
  data1.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data1);
  // The handshake will then fail, but provide retry configs.
  SSLSocketDataProvider ssl1(ASYNC, ERR_FAILED);
  ssl1.expected_ech_config_list = ech_config_list1;
  ssl1.ech_retry_configs = ech_config_list2;
  socket_factory_.AddSSLSocketDataProvider(&ssl1);
  // There will be no second connection attempt.

  base::HistogramTester histogram_tester;
  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_FAILED));

  histogram_tester.ExpectUniqueSample("Net.SSL.ECHResult",
                                      1 /* kErrorInitial */, 1);
}

// Test the legacy crypto callback can trigger after the ECH recovery flow.
TEST_F(SSLConnectJobTest, ECHRecoveryThenLegacyCrypto) {
  std::vector<uint8_t> ech_config_list1, ech_config_list2, ech_config_list3;
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list1));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list2));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list3));

  // Configure two HTTPS RR routes, to test the retry uses the correct one.
  HostResolverEndpointResult endpoint1, endpoint2;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint1.metadata.ech_config_list = ech_config_list1;
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint2.metadata.ech_config_list = ech_config_list2;
  host_resolver_.rules()->AddRule(
      "host", MockHostResolverBase::RuleResolver::RuleResult(
                  std::vector{endpoint1, endpoint2}));

  // The first connection attempt will be to `endpoint1`, which will fail.
  StaticSocketDataProvider data1;
  data1.set_expected_addresses(AddressList(endpoint1.ip_endpoints));
  data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  socket_factory_.AddSocketDataProvider(&data1);
  // The second connection attempt will be to `endpoint2`, which will succeed.
  StaticSocketDataProvider data2;
  data2.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data2);
  // The handshake will then fail, and provide retry configs.
  SSLSocketDataProvider ssl2(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl2.expected_ech_config_list = ech_config_list2;
  ssl2.ech_retry_configs = ech_config_list3;
  socket_factory_.AddSSLSocketDataProvider(&ssl2);
  // The third connection attempt should skip `endpoint1` and retry with only
  // `endpoint2`.
  StaticSocketDataProvider data3;
  data3.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data3.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data3);
  // The handshake should be passed the retry configs. This will progress
  // further but trigger the legacy crypto fallback.
  SSLSocketDataProvider ssl3(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  ssl3.expected_ech_config_list = ech_config_list3;
  socket_factory_.AddSSLSocketDataProvider(&ssl3);
  // The third connection attempt should still skip `endpoint1` and retry with
  // only `endpoint2`.
  StaticSocketDataProvider data4;
  data4.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data4.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data4);
  // The handshake should still be passed ECH retry configs. This time, the
  // connection enables legacy crypto and succeeds.
  SSLSocketDataProvider ssl4(ASYNC, OK);
  ssl4.expected_ech_config_list = ech_config_list3;
  socket_factory_.AddSSLSocketDataProvider(&ssl4);

  // The connection should ultimately succeed.
  base::HistogramTester histogram_tester;
  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());

  histogram_tester.ExpectUniqueSample("Net.SSL.ECHResult",
                                      2 /* kSuccessRetry */, 1);
}

// Test the ECH recovery flow can trigger after the legacy crypto fallback.
TEST_F(SSLConnectJobTest, LegacyCryptoThenECHRecovery) {
  std::vector<uint8_t> ech_config_list1, ech_config_list2, ech_config_list3;
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list1));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list2));
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list3));

  // Configure two HTTPS RR routes, to test the retry uses the correct one.
  HostResolverEndpointResult endpoint1, endpoint2;
  endpoint1.ip_endpoints = {IPEndPoint(ParseIP("1::"), 8441)};
  endpoint1.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint1.metadata.ech_config_list = ech_config_list1;
  endpoint2.ip_endpoints = {IPEndPoint(ParseIP("2::"), 8442)};
  endpoint2.metadata.supported_protocol_alpns = {"http/1.1"};
  endpoint2.metadata.ech_config_list = ech_config_list2;
  host_resolver_.rules()->AddRule(
      "host", MockHostResolverBase::RuleResolver::RuleResult(
                  std::vector{endpoint1, endpoint2}));

  // The first connection attempt will be to `endpoint1`, which will fail.
  StaticSocketDataProvider data1;
  data1.set_expected_addresses(AddressList(endpoint1.ip_endpoints));
  data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  socket_factory_.AddSocketDataProvider(&data1);
  // The second connection attempt will be to `endpoint2`, which will succeed.
  StaticSocketDataProvider data2;
  data2.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data2);
  // The handshake will then fail, and trigger the legacy cryptography fallback.
  SSLSocketDataProvider ssl2(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  ssl2.expected_ech_config_list = ech_config_list2;
  socket_factory_.AddSSLSocketDataProvider(&ssl2);
  // The third and fourth connection attempts proceed as before, but with legacy
  // cryptography enabled.
  StaticSocketDataProvider data3;
  data3.set_expected_addresses(AddressList(endpoint1.ip_endpoints));
  data3.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  socket_factory_.AddSocketDataProvider(&data3);
  StaticSocketDataProvider data4;
  data4.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data4.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data4);
  // The handshake enables legacy crypto. Now ECH fails with retry configs.
  SSLSocketDataProvider ssl4(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl4.expected_ech_config_list = ech_config_list2;
  ssl4.ech_retry_configs = ech_config_list3;
  socket_factory_.AddSSLSocketDataProvider(&ssl4);
  // The fourth connection attempt should still skip `endpoint1` and retry with
  // only `endpoint2`.
  StaticSocketDataProvider data5;
  data5.set_expected_addresses(AddressList(endpoint2.ip_endpoints));
  data5.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data5);
  // The handshake will now succeed with ECH retry configs and legacy
  // cryptography.
  SSLSocketDataProvider ssl5(ASYNC, OK);
  ssl5.expected_ech_config_list = ech_config_list3;
  socket_factory_.AddSSLSocketDataProvider(&ssl5);

  // The connection should ultimately succeed.
  base::HistogramTester histogram_tester;
  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyChain::Direct(), MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());

  histogram_tester.ExpectUniqueSample("Net.SSL.ECHResult",
                                      2 /* kSuccessRetry */, 1);
}

}  // namespace
}  // namespace net
