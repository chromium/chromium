// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_connect_job.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/gtest_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// Just check that all connect times are set to base::TimeTicks::Now(), for
// tests that don't update the mocked out time.
void CheckConnectTimesSet(const LoadTimingInfo::ConnectTiming& connect_timing) {
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.dns_start);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.dns_end);
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
  EXPECT_TRUE(connect_timing.dns_start.is_null());
  EXPECT_TRUE(connect_timing.dns_end.is_null());
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.connect_start);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.ssl_start);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.ssl_end);
  EXPECT_EQ(base::TimeTicks::Now(), connect_timing.connect_end);
}

class SSLConnectJobTest : public WithTaskEnvironment, public testing::Test {
 public:
  SSLConnectJobTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        proxy_resolution_service_(ProxyResolutionService::CreateDirect()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        http_auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault()),
        session_(CreateNetworkSession()),
        direct_transport_socket_params_(
            new TransportSocketParams(HostPortPair("host", 443),
                                      NetworkIsolationKey(),
                                      false /* disable_secure_dns */,
                                      OnHostResolutionCallback())),
        proxy_transport_socket_params_(
            new TransportSocketParams(HostPortPair("proxy", 443),
                                      NetworkIsolationKey(),
                                      false /* disable_secure_dns */,
                                      OnHostResolutionCallback())),
        socks_socket_params_(
            new SOCKSSocketParams(proxy_transport_socket_params_,
                                  true,
                                  HostPortPair("sockshost", 443),
                                  NetworkIsolationKey(),
                                  TRAFFIC_ANNOTATION_FOR_TESTS)),
        http_proxy_socket_params_(
            new HttpProxySocketParams(proxy_transport_socket_params_,
                                      nullptr /* ssl_params */,
                                      false /* is_quic */,
                                      HostPortPair("host", 80),
                                      /*is_trusted_proxy=*/false,
                                      /*tunnel=*/true,
                                      TRAFFIC_ANNOTATION_FOR_TESTS,
                                      NetworkIsolationKey())),
        common_connect_job_params_(session_->CreateCommonConnectJobParams()) {}

  ~SSLConnectJobTest() override = default;

  std::unique_ptr<ConnectJob> CreateConnectJob(
      TestConnectJobDelegate* test_delegate,
      ProxyServer::Scheme proxy_scheme = ProxyServer::SCHEME_DIRECT,
      RequestPriority priority = DEFAULT_PRIORITY) {
    return std::make_unique<SSLConnectJob>(
        priority, SocketTag(), &common_connect_job_params_,
        SSLParams(proxy_scheme), test_delegate, nullptr /* net_log */);
  }

  scoped_refptr<SSLSocketParams> SSLParams(ProxyServer::Scheme proxy) {
    return base::MakeRefCounted<SSLSocketParams>(
        proxy == ProxyServer::SCHEME_DIRECT ? direct_transport_socket_params_
                                            : nullptr,
        proxy == ProxyServer::SCHEME_SOCKS5 ? socks_socket_params_ : nullptr,
        proxy == ProxyServer::SCHEME_HTTP ? http_proxy_socket_params_ : nullptr,
        HostPortPair("host", 443), SSLConfig(), PRIVACY_MODE_DISABLED,
        NetworkIsolationKey());
  }

  void AddAuthToCache() {
    const base::string16 kFoo(base::ASCIIToUTF16("foo"));
    const base::string16 kBar(base::ASCIIToUTF16("bar"));
    session_->http_auth_cache()->Add(
        GURL("http://proxy:443/"), HttpAuth::AUTH_PROXY, "MyRealm1",
        HttpAuth::AUTH_SCHEME_BASIC, NetworkIsolationKey(),
        "Basic realm=MyRealm1", AuthCredentials(kFoo, kBar), "/");
  }

  HttpNetworkSession* CreateNetworkSession() {
    HttpNetworkSession::Context session_context;
    session_context.host_resolver = &host_resolver_;
    session_context.cert_verifier = &cert_verifier_;
    session_context.transport_security_state = &transport_security_state_;
    session_context.cert_transparency_verifier = &ct_verifier_;
    session_context.ct_policy_enforcer = &ct_policy_enforcer_;
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.client_socket_factory = &socket_factory_;
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.http_auth_handler_factory =
        http_auth_handler_factory_.get();
    session_context.http_server_properties = &http_server_properties_;
    session_context.quic_context = &quic_context_;
    return new HttpNetworkSession(HttpNetworkSession::Params(),
                                  session_context);
  }

 protected:
  MockClientSocketFactory socket_factory_;
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  MultiLogCTVerifier ct_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  const std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  const std::unique_ptr<SSLConfigService> ssl_config_service_;
  const std::unique_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  HttpServerProperties http_server_properties_;
  QuicContext quic_context_;
  const std::unique_ptr<HttpNetworkSession> session_;

  scoped_refptr<TransportSocketParams> direct_transport_socket_params_;

  scoped_refptr<TransportSocketParams> proxy_transport_socket_params_;
  scoped_refptr<SOCKSSocketParams> socks_socket_params_;
  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_;

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
  const base::TimeDelta kTinyTime = base::TimeDelta::FromMicroseconds(1);

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
  const base::TimeDelta kTinyTime = base::TimeDelta::FromMicroseconds(1);

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
  const base::TimeDelta kTinyTime = base::TimeDelta::FromMicroseconds(1);

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
      CreateConnectJob(&test_delegate, ProxyServer::SCHEME_DIRECT, MEDIUM);

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
      CreateConnectJob(&test_delegate, ProxyServer::SCHEME_DIRECT, MEDIUM);
  EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_TRUE(host_resolver_.has_pending_requests());
  EXPECT_EQ(MEDIUM, host_resolver_.last_request_priority());
  FastForwardBy(base::TimeDelta::FromSeconds(5));

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
  EXPECT_EQ(start_time, ssl_connect_job->connect_timing().dns_start);
  EXPECT_EQ(resolve_complete_time, ssl_connect_job->connect_timing().dns_end);
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
      CreateConnectJob(&test_delegate, ProxyServer::SCHEME_DIRECT, MEDIUM);
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
      if (initial_priority == new_priority)
        continue;
      TestConnectJobDelegate test_delegate;
      std::unique_ptr<ConnectJob> ssl_connect_job =
          CreateConnectJob(&test_delegate, ProxyServer::SCHEME_DIRECT,
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

TEST_F(SSLConnectJobTest, DisableSecureDns) {
  for (bool disable_secure_dns : {false, true}) {
    TestConnectJobDelegate test_delegate;
    direct_transport_socket_params_ =
        base::MakeRefCounted<TransportSocketParams>(
            HostPortPair("host", 443), NetworkIsolationKey(),
            disable_secure_dns, OnHostResolutionCallback());
    auto common_connect_job_params = session_->CreateCommonConnectJobParams();
    std::unique_ptr<ConnectJob> ssl_connect_job =
        std::make_unique<SSLConnectJob>(DEFAULT_PRIORITY, SocketTag(),
                                        &common_connect_job_params,
                                        SSLParams(ProxyServer::SCHEME_DIRECT),
                                        &test_delegate, nullptr /* net_log */);

    EXPECT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
    EXPECT_EQ(disable_secure_dns,
              host_resolver_.last_secure_dns_mode_override().has_value());
    if (disable_secure_dns) {
      EXPECT_EQ(net::DnsConfig::SecureDnsMode::OFF,
                host_resolver_.last_secure_dns_mode_override().value());
    }
  }
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

TEST_F(SSLConnectJobTest, DirectSSLError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate);

  test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                        ERR_SSL_PROTOCOL_ERROR,
                                        false /* expect_sync_result */);
  ConnectionAttempts connection_attempts =
      ssl_connect_job->GetConnectionAttempts();
  ASSERT_EQ(1u, connection_attempts.size());
  EXPECT_THAT(connection_attempts[0].result,
              test::IsError(ERR_SSL_PROTOCOL_ERROR));
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
  EXPECT_TRUE(test_delegate.socket()->WasAlpnNegotiated());
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
  EXPECT_TRUE(test_delegate.socket()->WasAlpnNegotiated());
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
    std::unique_ptr<ConnectJob> ssl_connect_job =
        CreateConnectJob(&test_delegate, ProxyServer::SCHEME_SOCKS5);
    test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                          ERR_PROXY_CONNECTION_FAILED,
                                          io_mode == SYNCHRONOUS);
    EXPECT_FALSE(ssl_connect_job->IsSSLError());

    ConnectionAttempts connection_attempts =
        ssl_connect_job->GetConnectionAttempts();
    EXPECT_EQ(0u, connection_attempts.size());
  }
}

TEST_F(SSLConnectJobTest, SOCKSBasic) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    const char kSOCKS5Request[] = {0x05, 0x01, 0x00, 0x03, 0x09, 's',
                                   'o',  'c',  'k',  's',  'h',  'o',
                                   's',  't',  0x01, 0xBB};

    MockWrite writes[] = {
        MockWrite(io_mode, kSOCKS5GreetRequest, kSOCKS5GreetRequestLength),
        MockWrite(io_mode, kSOCKS5Request, base::size(kSOCKS5Request)),
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
    std::unique_ptr<ConnectJob> ssl_connect_job =
        CreateConnectJob(&test_delegate, ProxyServer::SCHEME_SOCKS5);
    test_delegate.StartJobExpectingResult(ssl_connect_job.get(), OK,
                                          io_mode == SYNCHRONOUS);
    CheckConnectTimesExceptDnsSet(ssl_connect_job->connect_timing());
  }
}

TEST_F(SSLConnectJobTest, SOCKSHasEstablishedConnection) {
  const char kSOCKS5Request[] = {0x05, 0x01, 0x00, 0x03, 0x09, 's', 'o',  'c',
                                 'k',  's',  'h',  'o',  's',  't', 0x01, 0xBB};

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, kSOCKS5GreetRequest, kSOCKS5GreetRequestLength, 0),
      MockWrite(SYNCHRONOUS, kSOCKS5Request, base::size(kSOCKS5Request), 3),
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
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyServer::SCHEME_SOCKS5);
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
      if (initial_priority == new_priority)
        continue;
      TestConnectJobDelegate test_delegate;
      std::unique_ptr<ConnectJob> ssl_connect_job =
          CreateConnectJob(&test_delegate, ProxyServer::SCHEME_SOCKS5,
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
    std::unique_ptr<ConnectJob> ssl_connect_job =
        CreateConnectJob(&test_delegate, ProxyServer::SCHEME_HTTP);
    test_delegate.StartJobExpectingResult(ssl_connect_job.get(),
                                          ERR_PROXY_CONNECTION_FAILED,
                                          io_mode == SYNCHRONOUS);

    EXPECT_FALSE(ssl_connect_job->IsSSLError());
    ConnectionAttempts connection_attempts =
        ssl_connect_job->GetConnectionAttempts();
    EXPECT_EQ(0u, connection_attempts.size());
  }
}

TEST_F(SSLConnectJobTest, HttpProxyAuthChallenge) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
      MockWrite(ASYNC, 5,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
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
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyServer::SCHEME_HTTP);
  ASSERT_THAT(ssl_connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  test_delegate.WaitForAuthChallenge(1);

  EXPECT_EQ(407, test_delegate.auth_response_info().headers->response_code());
  std::string proxy_authenticate;
  ASSERT_TRUE(test_delegate.auth_response_info().headers->EnumerateHeader(
      nullptr, "Proxy-Authenticate", &proxy_authenticate));
  EXPECT_EQ(proxy_authenticate, "Basic realm=\"MyRealm1\"");

  // While waiting for auth credentials to be provided, the Job should not time
  // out.
  FastForwardBy(base::TimeDelta::FromDays(1));
  test_delegate.WaitForAuthChallenge(1);
  EXPECT_FALSE(test_delegate.has_result());

  // Respond to challenge.
  test_delegate.auth_controller()->ResetAuth(
      AuthCredentials(base::ASCIIToUTF16("foo"), base::ASCIIToUTF16("bar")));
  test_delegate.RunAuthCallback();

  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
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
    std::unique_ptr<ConnectJob> ssl_connect_job =
        CreateConnectJob(&test_delegate, ProxyServer::SCHEME_HTTP);
    test_delegate.StartJobExpectingResult(ssl_connect_job.get(), OK,
                                          io_mode == SYNCHRONOUS);
    CheckConnectTimesExceptDnsSet(ssl_connect_job->connect_timing());
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
      if (initial_priority == new_priority)
        continue;
      TestConnectJobDelegate test_delegate;
      std::unique_ptr<ConnectJob> ssl_connect_job =
          CreateConnectJob(&test_delegate, ProxyServer::SCHEME_HTTP,
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
                "Proxy-Connection: keep-alive\r\n\r\n"),
      MockWrite(ASYNC, 3,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
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
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyServer::SCHEME_HTTP);
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
  test_delegate.auth_controller()->ResetAuth(
      AuthCredentials(base::ASCIIToUTF16("foo"), base::ASCIIToUTF16("bar")));
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
                "Proxy-Connection: keep-alive\r\n\r\n"),
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
  std::unique_ptr<ConnectJob> ssl_connect_job =
      CreateConnectJob(&test_delegate, ProxyServer::SCHEME_HTTP);
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
  test_delegate.auth_controller()->ResetAuth(
      AuthCredentials(base::ASCIIToUTF16("foo"), base::ASCIIToUTF16("bar")));
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

}  // namespace
}  // namespace net
