// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_connect_job.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/hex_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/base/session_usage.h"
#include "net/base/test_proxy_delegate.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using ::testing::_;

namespace net {

namespace {

const char kEndpointHost[] = "www.endpoint.test";

enum HttpProxyType { HTTP, HTTPS, SPDY };

const char kHttpProxyHost[] = "httpproxy.example.test";
const char kHttpsProxyHost[] = "httpsproxy.example.test";
const char kQuicProxyHost[] = "quicproxy.example.test";
const char kHttpsNestedProxyHost[] = "last-hop-https-proxy.example.test";

const ProxyServer kHttpProxyServer{ProxyServer::SCHEME_HTTP,
                                   HostPortPair(kHttpProxyHost, 80)};
const ProxyServer kHttpsProxyServer{ProxyServer::SCHEME_HTTPS,
                                    HostPortPair(kHttpsProxyHost, 443)};
const ProxyServer kHttpsNestedProxyServer{
    ProxyServer::SCHEME_HTTPS, HostPortPair(kHttpsNestedProxyHost, 443)};

const ProxyChain kHttpProxyChain{kHttpProxyServer};
const ProxyChain kHttpsProxyChain{kHttpsProxyServer};
// TODO(crbug.com/365771838): Add tests for non-ip protection nested proxy
// chains if support is enabled for all builds.
const ProxyChain kHttpsNestedProxyChain =
    ProxyChain::ForIpProtection({{kHttpsProxyServer, kHttpsNestedProxyServer}});

constexpr char kTestHeaderName[] = "Foo";
// Note: `kTestSpdyHeaderName` should be a lowercase version of
// `kTestHeaderName`.
constexpr char kTestSpdyHeaderName[] = "foo";

// Match QuicStreamRequests' proxy chains.
MATCHER_P(QSRHasProxyChain,
          proxy_chain,
          base::StringPrintf("QuicStreamRequest %s ProxyChain %s",
                             negation ? "does not have" : "has",
                             proxy_chain.ToDebugString().c_str())) {
  *result_listener << "where the proxy chain is "
                   << arg->session_key().proxy_chain().ToDebugString();
  return arg->session_key().proxy_chain() == proxy_chain;
}

MATCHER_P(
    IsQuicVersion,
    quic_version,
    base::StringPrintf("QUIC version %s %s",
                       negation ? "is not" : "is",
                       quic::ParsedQuicVersionToString(quic_version).c_str())) {
  *result_listener << "where the QUIC version is "
                   << quic::ParsedQuicVersionToString(arg);
  return arg == quic_version;
}

}  // namespace

class HttpProxyConnectJobTestBase : public WithTaskEnvironment {
 public:
  HttpProxyConnectJobTestBase()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Used a mock HostResolver that does not have a cache.
    session_deps_.host_resolver = std::make_unique<MockHostResolver>(
        /*default_result=*/MockHostResolverBase::RuleResolver::
            GetLocalhostResult());
    session_deps_.http_user_agent_settings =
        std::make_unique<StaticHttpUserAgentSettings>("*", "test-ua");

    network_quality_estimator_ =
        std::make_unique<TestNetworkQualityEstimator>();
    session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    InitCommonConnectJobParams();
  }

  virtual ~HttpProxyConnectJobTestBase() {
    // Reset global field trial parameters to defaults values.
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    HttpProxyConnectJob::UpdateFieldTrialParametersForTesting();
  }

  // This may only be called at the start of the test, before any ConnectJobs
  // have been created.
  void InitCommonConnectJobParams() {
    common_connect_job_params_ = std::make_unique<CommonConnectJobParams>(
        session_->CreateCommonConnectJobParams());
    // TODO(mmenke): Consider reworking this so it can be done through
    // |session_deps_|.
    common_connect_job_params_->proxy_delegate = proxy_delegate_.get();
    common_connect_job_params_->network_quality_estimator =
        network_quality_estimator_.get();
  }

  // This may only be called at the start of the test, before any ConnectJobs
  // have been created.
  void InitProxyDelegate() {
    proxy_delegate_ = std::make_unique<TestProxyDelegate>();
    proxy_delegate_->set_extra_header_name(kTestHeaderName);
    InitCommonConnectJobParams();
  }

 protected:
  std::unique_ptr<TestProxyDelegate> proxy_delegate_;

  // These data providers may be pointed to by the socket factory in
  // `session_deps_`.
  std::unique_ptr<SSLSocketDataProvider> ssl_data_;
  std::unique_ptr<SSLSocketDataProvider> old_ssl_data_;
  std::unique_ptr<SSLSocketDataProvider> nested_second_proxy_ssl_data_;
  std::unique_ptr<SequencedSocketData> data_;

  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> session_;
  std::unique_ptr<TestNetworkQualityEstimator> network_quality_estimator_;
  std::unique_ptr<CommonConnectJobParams> common_connect_job_params_;
};

class HttpProxyConnectJobTest : public HttpProxyConnectJobTestBase,
                                public ::testing::TestWithParam<HttpProxyType> {
 public:
  // Initializes the field trial parameters for the field trial that determines
  // connection timeout based on the network quality.
  void InitAdaptiveTimeoutFieldTrialWithParams(
      bool use_default_params,
      int ssl_http_rtt_multiplier,
      int non_ssl_http_rtt_multiplier,
      base::TimeDelta min_proxy_connection_timeout,
      base::TimeDelta max_proxy_connection_timeout) {
    std::string trial_name = "NetAdaptiveProxyConnectionTimeout";
    std::string group_name = "GroupName";

    std::map<std::string, std::string> params;
    if (!use_default_params) {
      params["ssl_http_rtt_multiplier"] =
          base::NumberToString(ssl_http_rtt_multiplier);
      params["non_ssl_http_rtt_multiplier"] =
          base::NumberToString(non_ssl_http_rtt_multiplier);
      params["min_proxy_connection_timeout_seconds"] =
          base::NumberToString(min_proxy_connection_timeout.InSeconds());
      params["max_proxy_connection_timeout_seconds"] =
          base::NumberToString(max_proxy_connection_timeout.InSeconds());
    }
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    EXPECT_TRUE(
        base::AssociateFieldTrialParams(trial_name, group_name, params));
    EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(trial_name, group_name));

    // Force static global that reads the field trials to update.
    HttpProxyConnectJob::UpdateFieldTrialParametersForTesting();
  }

  scoped_refptr<TransportSocketParams> CreateHttpProxyParams(
      SecureDnsPolicy secure_dns_policy) const {
    if (GetParam() != HTTP) {
      return nullptr;
    }
    return base::MakeRefCounted<TransportSocketParams>(
        kHttpProxyServer.host_port_pair(), NetworkAnonymizationKey(),
        secure_dns_policy, OnHostResolutionCallback(),
        /*supported_alpns=*/base::flat_set<std::string>());
  }

  scoped_refptr<SSLSocketParams> CreateHttpsProxyParams(
      SecureDnsPolicy secure_dns_policy) const {
    if (GetParam() == HTTP) {
      return nullptr;
    }
    return base::MakeRefCounted<SSLSocketParams>(
        ConnectJobParams(base::MakeRefCounted<TransportSocketParams>(
            kHttpsProxyServer.host_port_pair(), NetworkAnonymizationKey(),
            secure_dns_policy, OnHostResolutionCallback(),
            /*supported_alpns=*/base::flat_set<std::string>())),
        HostPortPair(kHttpsProxyHost, 443), SSLConfig(),
        NetworkAnonymizationKey());
  }

  // Returns a correctly constructed HttpProxyParams for a single HTTP or HTTPS
  // proxy.
  scoped_refptr<HttpProxySocketParams> CreateParams(
      bool tunnel,
      SecureDnsPolicy secure_dns_policy) {
    ConnectJobParams params;
    if (GetParam() == HTTP) {
      params = ConnectJobParams(CreateHttpProxyParams(secure_dns_policy));
    } else {
      params = ConnectJobParams(CreateHttpsProxyParams(secure_dns_policy));
    }
    return base::MakeRefCounted<HttpProxySocketParams>(
        std::move(params), HostPortPair(kEndpointHost, tunnel ? 443 : 80),
        GetParam() == HTTP ? kHttpProxyChain : kHttpsProxyChain,
        /*proxy_chain_index=*/0, tunnel, TRAFFIC_ANNOTATION_FOR_TESTS,
        NetworkAnonymizationKey(), secure_dns_policy);
  }

  // Creates a correctly constructed `SSLSocketParams()` corresponding to the
  // proxy server in `proxy_chain` at index `proxy_chain_index`.
  scoped_refptr<SSLSocketParams> CreateNestedHttpsProxyParams(
      bool tunnel,
      SecureDnsPolicy secure_dns_policy,
      const ProxyChain& proxy_chain,
      size_t proxy_chain_index) const {
    DCHECK_NE(GetParam(), HTTP);

    const ProxyServer& proxy_server =
        proxy_chain.GetProxyServer(proxy_chain_index);

    if (proxy_chain_index != 0) {
      // For all but the first hop in a multi-hop proxy, the SSLSocketParams
      // should be created such that it tunnels over a direct encrypted
      // connection made to the first hop (possibly via intermediate tunnels
      // through other hops)... Build an HttpProxySocketParams for the
      // previous hop that will establish this.
      size_t previous_hop_proxy_chain_index = proxy_chain_index - 1;

      return base::MakeRefCounted<SSLSocketParams>(
          ConnectJobParams(CreateNestedParams(tunnel, secure_dns_policy,
                                              proxy_chain,
                                              previous_hop_proxy_chain_index)),
          proxy_server.host_port_pair(), SSLConfig(),
          NetworkAnonymizationKey());
    }

    // If we are creating the SSLSocketParams for the first hop, establish a
    // direct encrypted connection to it.
    return base::MakeRefCounted<SSLSocketParams>(
        ConnectJobParams(base::MakeRefCounted<TransportSocketParams>(
            proxy_server.host_port_pair(), NetworkAnonymizationKey(),
            secure_dns_policy, OnHostResolutionCallback(),
            /*supported_alpns=*/base::flat_set<std::string>())),
        proxy_server.host_port_pair(), SSLConfig(), NetworkAnonymizationKey());
  }

  // Creates a correctly constructed `HttpProxySocketParams()` corresponding to
  // the proxy server in `proxy_chain` at index `proxy_chain_index` (and set to
  // create a CONNECT for either the next hop in the proxy or to
  // `kEndpointHost`).
  scoped_refptr<HttpProxySocketParams> CreateNestedParams(
      bool tunnel,
      SecureDnsPolicy secure_dns_policy,
      const ProxyChain& proxy_chain,
      size_t proxy_chain_index) const {
    DCHECK_NE(GetParam(), HTTP);
    HostPortPair connect_host_port_pair;
    scoped_refptr<SSLSocketParams> ssl_params = CreateNestedHttpsProxyParams(
        tunnel, secure_dns_policy, proxy_chain, proxy_chain_index);
    if (proxy_chain_index + 1 != proxy_chain.length()) {
      // For all but the last hop in the proxy, what we CONNECT to is the next
      // hop in the proxy.
      size_t next_hop_proxy_chain_index = proxy_chain_index + 1;
      const ProxyServer& next_hop_proxy_server =
          proxy_chain.GetProxyServer(next_hop_proxy_chain_index);
      connect_host_port_pair = next_hop_proxy_server.host_port_pair();
    } else {
      // If we aren't testing multi-hop proxies or this HttpProxySocketParams
      // corresponds to the last hop, then we need to CONNECT to the
      // destination site.
      connect_host_port_pair = HostPortPair(kEndpointHost, tunnel ? 443 : 80);
    }
    return base::MakeRefCounted<HttpProxySocketParams>(
        ConnectJobParams(std::move(ssl_params)), connect_host_port_pair,
        proxy_chain, proxy_chain_index, tunnel, TRAFFIC_ANNOTATION_FOR_TESTS,
        NetworkAnonymizationKey(), secure_dns_policy);
  }

  std::unique_ptr<HttpProxyConnectJob> CreateConnectJobForHttpRequest(
      ConnectJob::Delegate* delegate,
      RequestPriority priority = DEFAULT_PRIORITY,
      SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow) {
    return CreateConnectJob(CreateParams(false /* tunnel */, secure_dns_policy),
                            delegate, priority);
  }

  std::unique_ptr<HttpProxyConnectJob> CreateConnectJobForTunnel(
      ConnectJob::Delegate* delegate,
      RequestPriority priority = DEFAULT_PRIORITY,
      SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow) {
    return CreateConnectJob(CreateParams(true /* tunnel */, secure_dns_policy),
                            delegate, priority);
  }

  // Creates an HttpProxyConnectJob corresponding to `kHttpsNestedProxyChain`.
  // This is done by working backwards through the proxy chain and creating
  // socket params such that connect jobs will be created recursively with
  // dependencies in the correct order (in other words, the inner-most connect
  // job will establish a connection to the first proxy, and then that
  // connection will get used to establish a connection to the second proxy, and
  // finally a connection will be established to the destination).
  std::unique_ptr<HttpProxyConnectJob> CreateConnectJobForNestedProxyTunnel(
      ConnectJob::Delegate* delegate,
      RequestPriority priority = DEFAULT_PRIORITY,
      SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow) {
    size_t last_hop_proxy_server_index = kHttpsNestedProxyChain.length() - 1;
    return CreateConnectJob(
        CreateNestedParams(/*tunnel=*/true, secure_dns_policy,
                           kHttpsNestedProxyChain, last_hop_proxy_server_index),
        delegate, priority);
  }

  std::unique_ptr<HttpProxyConnectJob> CreateConnectJob(
      scoped_refptr<HttpProxySocketParams> http_proxy_socket_params,
      ConnectJob::Delegate* delegate,
      RequestPriority priority) {
    return std::make_unique<HttpProxyConnectJob>(
        priority, SocketTag(), common_connect_job_params_.get(),
        std::move(http_proxy_socket_params), delegate, /*net_log=*/nullptr);
  }

  void Initialize(base::span<const MockRead> reads,
                  base::span<const MockWrite> writes,
                  base::span<const MockRead> spdy_reads,
                  base::span<const MockWrite> spdy_writes,
                  IoMode connect_and_ssl_io_mode,
                  bool two_ssl_proxies = false) {
    if (GetParam() == SPDY) {
      data_ = std::make_unique<SequencedSocketData>(spdy_reads, spdy_writes);
    } else {
      data_ = std::make_unique<SequencedSocketData>(reads, writes);
    }

    data_->set_connect_data(MockConnect(connect_and_ssl_io_mode, OK));

    session_deps_.socket_factory->AddSocketDataProvider(data_.get());

    if (GetParam() != HTTP) {
      // Keep the old ssl_data in case there is a draining socket.
      old_ssl_data_.swap(ssl_data_);
      ssl_data_ =
          std::make_unique<SSLSocketDataProvider>(connect_and_ssl_io_mode, OK);
      if (GetParam() == SPDY) {
        InitializeSpdySsl(ssl_data_.get());
      }
      session_deps_.socket_factory->AddSSLSocketDataProvider(ssl_data_.get());
    }

    if (two_ssl_proxies) {
      // For testing nested proxies we need another SSLSocketDataProvider
      // corresponding to the SSL connection established to the second hop in
      // the proxy.
      nested_second_proxy_ssl_data_ =
          std::make_unique<SSLSocketDataProvider>(connect_and_ssl_io_mode, OK);
      if (GetParam() == SPDY) {
        InitializeSpdySsl(nested_second_proxy_ssl_data_.get());
      }
      session_deps_.socket_factory->AddSSLSocketDataProvider(
          nested_second_proxy_ssl_data_.get());
    }
  }

  void InitializeSpdySsl(SSLSocketDataProvider* ssl_data) {
    ssl_data->next_proto = kProtoHTTP2;
  }

  // Return the timeout for establishing the lower layer connection. i.e., for
  // an HTTP proxy, the TCP connection timeout, and for an HTTPS proxy, the
  // TCP+SSL connection timeout. In many cases, this will return the return
  // value of the "AlternateNestedConnectionTimeout()".
  base::TimeDelta GetNestedConnectionTimeout() {
    base::TimeDelta normal_nested_connection_timeout =
        TransportConnectJob::ConnectionTimeout();
    if (GetParam() != HTTP) {
      normal_nested_connection_timeout +=
          SSLConnectJob::HandshakeTimeoutForTesting();
    }

    // Doesn't actually matter whether or not this is for a tunnel - the
    // connection timeout is the same, though it probably shouldn't be the
    // same, since tunnels need an extra round trip.
    base::TimeDelta alternate_connection_timeout =
        HttpProxyConnectJob::AlternateNestedConnectionTimeout(
            *CreateParams(true /* tunnel */, SecureDnsPolicy::kAllow),
            network_quality_estimator_.get());

    // If there's an alternate connection timeout, and it's less than the
    // standard TCP+SSL timeout (Which is also applied by the nested connect
    // jobs), return the alternate connection timeout. Otherwise, return the
    // normal timeout.
    if (!alternate_connection_timeout.is_zero() &&
        alternate_connection_timeout < normal_nested_connection_timeout) {
      return alternate_connection_timeout;
    }

    return normal_nested_connection_timeout;
  }

 protected:
  SpdyTestUtil spdy_util_;

  TestCompletionCallback callback_;
};

// All tests are run with three different proxy types: HTTP, HTTPS (non-SPDY)
// and SPDY.
INSTANTIATE_TEST_SUITE_P(HttpProxyType,
                         HttpProxyConnectJobTest,
                         ::testing::Values(HTTP, HTTPS, SPDY));

TEST_P(HttpProxyConnectJobTest, NoTunnel) {
  InitProxyDelegate();
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);
    base::HistogramTester histogram_tester;

    Initialize(base::span<MockRead>(), base::span<MockWrite>(),
               base::span<MockRead>(), base::span<MockWrite>(), io_mode);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForHttpRequest(&test_delegate);
    test_delegate.StartJobExpectingResult(connect_job.get(), OK,
                                          io_mode == SYNCHRONOUS);
    EXPECT_EQ(proxy_delegate_->on_before_tunnel_request_call_count(), 0u);

    // Proxies should not set any DNS aliases.
    EXPECT_TRUE(test_delegate.socket()->GetDnsAliases().empty());

    bool is_secure = GetParam() == HTTPS || GetParam() == SPDY;
    bool is_http2 = GetParam() == SPDY;
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http1.Http.Success",
        (!is_secure && !is_http2) ? 1 : 0);
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http1.Https.Success",
        (is_secure && !is_http2) ? 1 : 0);
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http2.Https.Success",
        (is_secure && is_http2) ? 1 : 0);
  }
}

// Pauses an HttpProxyConnectJob at various states, and check the value of
// HasEstablishedConnection().
TEST_P(HttpProxyConnectJobTest, HasEstablishedConnectionNoTunnel) {
  session_deps_.host_resolver->set_ondemand_mode(true);

  SequencedSocketData data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  // Set up SSL, if needed.
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  switch (GetParam()) {
    case HTTP:
      // No SSL needed.
      break;
    case HTTPS:
      // SSL negotiation is the last step in non-tunnel connections over HTTPS
      // proxies, so pause there, to check the final state before completion.
      ssl_data = SSLSocketDataProvider(SYNCHRONOUS, ERR_IO_PENDING);
      session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
      break;
    case SPDY:
      InitializeSpdySsl(&ssl_data);
      session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
      break;
  }

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job =
      CreateConnectJobForHttpRequest(&test_delegate);

  // Connecting should run until the request hits the HostResolver.
  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_TRUE(session_deps_.host_resolver->has_pending_requests());
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, connect_job->GetLoadState());
  EXPECT_FALSE(connect_job->HasEstablishedConnection());

  // Once the HostResolver completes, the job should start establishing a
  // connection, which will complete asynchronously.
  session_deps_.host_resolver->ResolveOnlyRequestNow();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_CONNECTING, connect_job->GetLoadState());
  EXPECT_FALSE(connect_job->HasEstablishedConnection());

  switch (GetParam()) {
    case HTTP:
    case SPDY:
      // Connection completes. Since no tunnel is established, the socket is
      // returned immediately, and HasEstablishedConnection() is only specified
      // to work before the ConnectJob completes.
      EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
      break;
    case HTTPS:
      base::RunLoop().RunUntilIdle();
      EXPECT_FALSE(test_delegate.has_result());
      EXPECT_EQ(LOAD_STATE_SSL_HANDSHAKE, connect_job->GetLoadState());
      EXPECT_TRUE(connect_job->HasEstablishedConnection());

      // Unfortunately, there's no API to advance the paused SSL negotiation,
      // so just end the test here.
  }
}

// Pauses an HttpProxyConnectJob at various states, and check the value of
// HasEstablishedConnection().
TEST_P(HttpProxyConnectJobTest, HasEstablishedConnectionTunnel) {
  session_deps_.host_resolver->set_ondemand_mode(true);

  // HTTP proxy CONNECT request / response, with a pause during the read.
  MockWrite http1_writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                "Host: www.endpoint.test:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
  };
  MockRead http1_reads[] = {
      // Pause at first read.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 2, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  SequencedSocketData http1_data(http1_reads, http1_writes);
  http1_data.set_connect_data(MockConnect(ASYNC, OK));

  // SPDY proxy CONNECT request / response, with a pause during the read.
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead spdy_reads[] = {
      // Pause at first read.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      CreateMockRead(resp, 2, ASYNC),
      MockRead(ASYNC, 0, 3),
  };
  SequencedSocketData spdy_data(spdy_reads, spdy_writes);
  spdy_data.set_connect_data(MockConnect(ASYNC, OK));

  // Will point to either the HTTP/1.x or SPDY data, depending on GetParam().
  SequencedSocketData* sequenced_data = nullptr;

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(ssl_data.ssl_info.cert);

  switch (GetParam()) {
    case HTTP:
      sequenced_data = &http1_data;
      break;
    case HTTPS:
      sequenced_data = &http1_data;
      ssl_data.next_proto = NextProto::kProtoHTTP11;
      session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
      break;
    case SPDY:
      sequenced_data = &spdy_data;
      InitializeSpdySsl(&ssl_data);
      session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);
      break;
  }

  session_deps_.socket_factory->AddSocketDataProvider(sequenced_data);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job =
      CreateConnectJobForTunnel(&test_delegate);

  // Connecting should run until the request hits the HostResolver.
  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_TRUE(session_deps_.host_resolver->has_pending_requests());
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, connect_job->GetLoadState());
  EXPECT_FALSE(connect_job->HasEstablishedConnection());

  // Once the HostResolver completes, the job should start establishing a
  // connection, which will complete asynchronously.
  session_deps_.host_resolver->ResolveOnlyRequestNow();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_CONNECTING, connect_job->GetLoadState());
  EXPECT_FALSE(connect_job->HasEstablishedConnection());

  // Run until the socket starts reading the proxy's handshake response.
  sequenced_data->RunUntilPaused();
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL, connect_job->GetLoadState());
  EXPECT_TRUE(connect_job->HasEstablishedConnection());

  // Finish the read, and run the job until it's complete.
  sequenced_data->Resume();
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());

  // Proxies should not set any DNS aliases.
  EXPECT_TRUE(test_delegate.socket()->GetDnsAliases().empty());

  // Although the underlying proxy connection may use TLS or negotiate ALPN, the
  // tunnel itself is a TCP connection to the origin and should not report these
  // values.
  SSLInfo ssl_info;
  EXPECT_FALSE(test_delegate.socket()->GetSSLInfo(&ssl_info));
  EXPECT_EQ(test_delegate.socket()->GetNegotiatedProtocol(),
            NextProto::kProtoUnknown);
}

TEST_P(HttpProxyConnectJobTest, ProxyDelegateExtraHeaders) {
  InitProxyDelegate();

  ProxyServer proxy_server(
      GetParam() == HTTP ? ProxyServer::SCHEME_HTTP : ProxyServer::SCHEME_HTTPS,
      HostPortPair(GetParam() == HTTP ? kHttpProxyHost : kHttpsProxyHost,
                   GetParam() == HTTP ? 80 : 443));
  std::string proxy_server_uri = ProxyServerToProxyUri(proxy_server);

  std::string http1_request = base::StringPrintf(
      "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
      "Host: www.endpoint.test:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "%s: %s\r\n\r\n",
      kTestHeaderName, proxy_server_uri.c_str());
  MockWrite writes[] = {
      MockWrite(ASYNC, 0, http1_request.c_str()),
  };

  const char kResponseHeaderName[] = "bar";
  const char kResponseHeaderValue[] = "Response";
  std::string http1_response = base::StringPrintf(
      "HTTP/1.1 200 Connection Established\r\n"
      "%s: %s\r\n\r\n",
      kResponseHeaderName, kResponseHeaderValue);
  MockRead reads[] = {
      MockRead(ASYNC, 1, http1_response.c_str()),
  };

  const char* const kExtraRequestHeaders[] = {
      kTestSpdyHeaderName,
      proxy_server_uri.c_str(),
      "user-agent",
      "test-ua",
  };
  const char* const kExtraResponseHeaders[] = {
      kResponseHeaderName,
      kResponseHeaderValue,
  };
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      kExtraRequestHeaders, std::size(kExtraRequestHeaders) / 2, 1,
      HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyGetReply(
      kExtraResponseHeaders, std::size(kExtraResponseHeaders) / 2, 1));
  MockRead spdy_reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes, spdy_reads, spdy_writes, ASYNC);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job =
      CreateConnectJobForTunnel(&test_delegate);
  test_delegate.StartJobExpectingResult(connect_job.get(), OK,
                                        false /* expect_sync_result */);

  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 1u);
  proxy_delegate_->VerifyOnTunnelHeadersReceived(
      ProxyChain(proxy_server), 0, kResponseHeaderName, kResponseHeaderValue);
}

// Test HTTP CONNECTs and SPDY CONNECTs through two proxies
// (HTTPS -> HTTPS -> HTTPS and SPDY -> SPDY -> HTTPS).
TEST_P(HttpProxyConnectJobTest, NestedProxyProxyDelegateExtraHeaders) {
  if (GetParam() == HTTP) {
    return;
  }
  InitProxyDelegate();

  const ProxyServer& first_hop_proxy_server =
      kHttpsNestedProxyChain.GetProxyServer(/*chain_index=*/0);
  const ProxyServer& second_hop_proxy_server =
      kHttpsNestedProxyChain.GetProxyServer(/*chain_index=*/1);

  std::string first_hop_proxy_server_uri =
      ProxyServerToProxyUri(first_hop_proxy_server);
  std::string second_hop_proxy_server_uri =
      ProxyServerToProxyUri(second_hop_proxy_server);

  std::string first_hop_http1_request = base::StringPrintf(
      "CONNECT last-hop-https-proxy.example.test:443 HTTP/1.1\r\n"
      "Host: last-hop-https-proxy.example.test:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "%s: %s\r\n\r\n",
      kTestHeaderName, first_hop_proxy_server_uri.c_str());
  std::string second_hop_http1_request = base::StringPrintf(
      "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
      "Host: www.endpoint.test:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "User-Agent: test-ua\r\n"
      "%s: %s\r\n\r\n",
      kTestHeaderName, second_hop_proxy_server_uri.c_str());

  const char kResponseHeaderName[] = "bar";
  std::string first_hop_http1_response = base::StringPrintf(
      "HTTP/1.1 200 Connection Established\r\n"
      "%s: %s\r\n\r\n",
      kResponseHeaderName, first_hop_proxy_server_uri.c_str());

  std::string second_hop_http1_response = base::StringPrintf(
      "HTTP/1.1 200 Connection Established\r\n"
      "%s: %s\r\n\r\n",
      kResponseHeaderName, second_hop_proxy_server_uri.c_str());

  MockWrite writes[] = {
      MockWrite(ASYNC, 0, first_hop_http1_request.c_str()),
      MockWrite(ASYNC, 2, second_hop_http1_request.c_str()),
  };

  MockRead reads[] = {
      MockRead(ASYNC, 1, first_hop_http1_response.c_str()),
      MockRead(ASYNC, 3, second_hop_http1_response.c_str()),
  };

  const char* const kFirstHopExtraRequestHeaders[] = {
      kTestSpdyHeaderName,
      first_hop_proxy_server_uri.c_str(),
      "user-agent",
      "test-ua",
  };
  const char* const kSecondHopExtraRequestHeaders[] = {
      kTestSpdyHeaderName,
      second_hop_proxy_server_uri.c_str(),
      "user-agent",
      "test-ua",
  };
  const char* const kFirstHopExtraResponseHeaders[] = {
      kResponseHeaderName,
      first_hop_proxy_server_uri.c_str(),
  };
  const char* const kSecondHopExtraResponseHeaders[] = {
      kResponseHeaderName,
      second_hop_proxy_server_uri.c_str(),
  };

  spdy::SpdySerializedFrame first_hop_req(spdy_util_.ConstructSpdyConnect(
      kFirstHopExtraRequestHeaders, std::size(kFirstHopExtraRequestHeaders) / 2,
      1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      second_hop_proxy_server.host_port_pair()));

  spdy::SpdySerializedFrame first_hop_resp(spdy_util_.ConstructSpdyGetReply(
      kFirstHopExtraResponseHeaders,
      std::size(kFirstHopExtraResponseHeaders) / 2, 1));

  // Use a new `SpdyTestUtil()` instance for the second hop response and request
  // because otherwise, the serialized frames that get generated for these will
  // use header compression and won't match what actually gets sent on the wire
  // (where header compression doesn't affect these requests because they are
  // associated with different streams).
  SpdyTestUtil new_spdy_util;

  spdy::SpdySerializedFrame second_hop_req(new_spdy_util.ConstructSpdyConnect(
      kSecondHopExtraRequestHeaders,
      std::size(kSecondHopExtraRequestHeaders) / 2, 1,
      HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));

  // Since the second request and response are sent over the tunnel established
  // previously, from a socket-perspective these need to be wrapped as data
  // frames.
  spdy::SpdySerializedFrame wrapped_second_hop_req(
      spdy_util_.ConstructWrappedSpdyFrame(second_hop_req, 1));

  spdy::SpdySerializedFrame second_hop_resp(new_spdy_util.ConstructSpdyGetReply(
      kSecondHopExtraResponseHeaders,
      std::size(kSecondHopExtraResponseHeaders) / 2, 1));

  spdy::SpdySerializedFrame wrapped_second_hop_resp(
      spdy_util_.ConstructWrappedSpdyFrame(second_hop_resp, 1));

  MockWrite spdy_writes[] = {
      CreateMockWrite(first_hop_req, 0),
      CreateMockWrite(wrapped_second_hop_req, 2),
  };
  MockRead spdy_reads[] = {
      CreateMockRead(first_hop_resp, 1, ASYNC),
      // TODO(crbug.com/41180906): We have to manually delay this read so
      // that the higher-level SPDY stream doesn't get notified of an available
      // read before the write it initiated (the second CONNECT) finishes,
      // triggering a DCHECK.
      MockRead(ASYNC, ERR_IO_PENDING, 3),
      CreateMockRead(wrapped_second_hop_resp, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, writes, spdy_reads, spdy_writes, ASYNC,
             /*two_ssl_proxies=*/true);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job =
      CreateConnectJobForNestedProxyTunnel(&test_delegate);

  if (GetParam() != SPDY) {
    test_delegate.StartJobExpectingResult(connect_job.get(), OK,
                                          /*expect_sync_result=*/false);
  } else {
    EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));

    data_->RunUntilPaused();
    base::RunLoop().RunUntilIdle();
    data_->Resume();

    EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
  }
  ASSERT_EQ(proxy_delegate_->on_tunnel_headers_received_call_count(), 2u);
  proxy_delegate_->VerifyOnTunnelHeadersReceived(
      kHttpsNestedProxyChain, /*chain_index=*/0, kResponseHeaderName,
      first_hop_proxy_server_uri, /*call_index=*/0);
  proxy_delegate_->VerifyOnTunnelHeadersReceived(
      kHttpsNestedProxyChain, /*chain_index=*/1, kResponseHeaderName,
      second_hop_proxy_server_uri, /*call_index=*/1);
}

// Test the case where auth credentials are not cached.
TEST_P(HttpProxyConnectJobTest, NeedAuth) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);

    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);

    MockWrite writes[] = {
        MockWrite(io_mode, 0,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n\r\n"),
        MockWrite(io_mode, 5,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n"
                  "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };
    MockRead reads[] = {
        // No credentials.
        MockRead(io_mode, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
        MockRead(io_mode, 2,
                 "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
        MockRead(io_mode, 3, "Content-Length: 10\r\n\r\n"),
        MockRead(io_mode, 4, "0123456789"),
        MockRead(io_mode, 6, "HTTP/1.1 200 Connection Established\r\n\r\n"),
    };

    SpdyTestUtil spdy_util;
    spdy::SpdySerializedFrame connect(spdy_util.ConstructSpdyConnect(
        nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair(kEndpointHost, 443)));
    spdy::SpdySerializedFrame rst(
        spdy_util.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
    spdy_util.UpdateWithStreamDestruction(1);

    // After calling trans.RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    const char* const kSpdyAuthCredentials[] = {
        "user-agent",
        "test-ua",
        "proxy-authorization",
        "Basic Zm9vOmJhcg==",
    };
    spdy::SpdySerializedFrame connect2(spdy_util.ConstructSpdyConnect(
        kSpdyAuthCredentials, std::size(kSpdyAuthCredentials) / 2, 3,
        HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair(kEndpointHost, 443)));

    MockWrite spdy_writes[] = {
        CreateMockWrite(connect, 0, io_mode),
        CreateMockWrite(rst, 2, io_mode),
        CreateMockWrite(connect2, 3, io_mode),
    };

    // The proxy responds to the connect with a 407, using a persistent
    // connection.
    const char kAuthStatus[] = "407";
    const char* const kAuthChallenge[] = {
        "proxy-authenticate",
        "Basic realm=\"MyRealm1\"",
    };
    spdy::SpdySerializedFrame connect_auth_resp(
        spdy_util.ConstructSpdyReplyError(kAuthStatus, kAuthChallenge,
                                          std::size(kAuthChallenge) / 2, 1));

    spdy::SpdySerializedFrame connect2_resp(
        spdy_util.ConstructSpdyGetReply(nullptr, 0, 3));
    MockRead spdy_reads[] = {
        CreateMockRead(connect_auth_resp, 1, ASYNC),
        CreateMockRead(connect2_resp, 4, ASYNC),
        MockRead(ASYNC, OK, 5),
    };

    Initialize(reads, writes, spdy_reads, spdy_writes, io_mode);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);
    ASSERT_EQ(ERR_IO_PENDING, connect_job->Connect());
    // Auth callback is always invoked asynchronously when a challenge is
    // observed.
    EXPECT_EQ(0, test_delegate.num_auth_challenges());

    test_delegate.WaitForAuthChallenge(1);
    ASSERT_TRUE(test_delegate.auth_response_info().headers);
    EXPECT_EQ(407, test_delegate.auth_response_info().headers->response_code());
    std::string proxy_authenticate;
    ASSERT_TRUE(test_delegate.auth_response_info().headers->EnumerateHeader(
        nullptr, "Proxy-Authenticate", &proxy_authenticate));
    EXPECT_EQ(proxy_authenticate, "Basic realm=\"MyRealm1\"");
    ASSERT_TRUE(test_delegate.auth_controller());
    EXPECT_FALSE(test_delegate.has_result());

    test_delegate.auth_controller()->ResetAuth(AuthCredentials(u"foo", u"bar"));
    test_delegate.RunAuthCallback();
    // Per API contract, the request can not complete synchronously.
    EXPECT_FALSE(test_delegate.has_result());

    EXPECT_EQ(OK, test_delegate.WaitForResult());
    EXPECT_EQ(1, test_delegate.num_auth_challenges());

    // Close the H2 session to prevent reuse.
    if (GetParam() == SPDY) {
      session_->CloseAllConnections(ERR_FAILED, "Very good reason");
    }
    // Also need to clear the auth cache before re-running the test.
    session_->http_auth_cache()->ClearAllEntries();
  }
}

// Test the case where auth credentials are not cached and the first time
// credentials are sent, they are rejected.
TEST_P(HttpProxyConnectJobTest, NeedAuthTwice) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);

    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);

    MockWrite writes[] = {
        MockWrite(io_mode, 0,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n\r\n"),
        MockWrite(io_mode, 2,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n"
                  "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
        MockWrite(io_mode, 4,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n"
                  "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };
    MockRead reads[] = {
        // No credentials.
        MockRead(io_mode, 1,
                 "HTTP/1.1 407 Proxy Authentication Required\r\n"
                 "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"
                 "Content-Length: 0\r\n\r\n"),
        MockRead(io_mode, 3,
                 "HTTP/1.1 407 Proxy Authentication Required\r\n"
                 "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"
                 "Content-Length: 0\r\n\r\n"),
        MockRead(io_mode, 5, "HTTP/1.1 200 Connection Established\r\n\r\n"),
    };

    SpdyTestUtil spdy_util;
    spdy::SpdySerializedFrame connect(spdy_util.ConstructSpdyConnect(
        nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair(kEndpointHost, 443)));
    spdy::SpdySerializedFrame rst(
        spdy_util.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
    spdy_util.UpdateWithStreamDestruction(1);

    // After calling trans.RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    const char* const kSpdyAuthCredentials[] = {
        "user-agent",
        "test-ua",
        "proxy-authorization",
        "Basic Zm9vOmJhcg==",
    };
    spdy::SpdySerializedFrame connect2(spdy_util.ConstructSpdyConnect(
        kSpdyAuthCredentials, std::size(kSpdyAuthCredentials) / 2, 3,
        HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair(kEndpointHost, 443)));
    spdy::SpdySerializedFrame rst2(
        spdy_util.ConstructSpdyRstStream(3, spdy::ERROR_CODE_CANCEL));
    spdy_util.UpdateWithStreamDestruction(3);

    spdy::SpdySerializedFrame connect3(spdy_util.ConstructSpdyConnect(
        kSpdyAuthCredentials, std::size(kSpdyAuthCredentials) / 2, 5,
        HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair(kEndpointHost, 443)));
    MockWrite spdy_writes[] = {
        CreateMockWrite(connect, 0, io_mode),
        CreateMockWrite(rst, 2, io_mode),
        CreateMockWrite(connect2, 3, io_mode),
        CreateMockWrite(rst2, 5, io_mode),
        CreateMockWrite(connect3, 6, io_mode),
    };

    // The proxy responds to the connect with a 407, using a persistent
    // connection.
    const char kAuthStatus[] = "407";
    const char* const kAuthChallenge[] = {
        "proxy-authenticate",
        "Basic realm=\"MyRealm1\"",
    };
    spdy::SpdySerializedFrame connect_auth_resp(
        spdy_util.ConstructSpdyReplyError(kAuthStatus, kAuthChallenge,
                                          std::size(kAuthChallenge) / 2, 1));
    spdy::SpdySerializedFrame connect2_auth_resp(
        spdy_util.ConstructSpdyReplyError(kAuthStatus, kAuthChallenge,
                                          std::size(kAuthChallenge) / 2, 3));
    spdy::SpdySerializedFrame connect3_resp(
        spdy_util.ConstructSpdyGetReply(nullptr, 0, 5));
    MockRead spdy_reads[] = {
        CreateMockRead(connect_auth_resp, 1, ASYNC),
        CreateMockRead(connect2_auth_resp, 4, ASYNC),
        CreateMockRead(connect3_resp, 7, ASYNC),
        MockRead(ASYNC, OK, 8),
    };

    Initialize(reads, writes, spdy_reads, spdy_writes, io_mode);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);
    ASSERT_EQ(ERR_IO_PENDING, connect_job->Connect());
    // Auth callback is always invoked asynchronously when a challenge is
    // observed.
    EXPECT_EQ(0, test_delegate.num_auth_challenges());

    test_delegate.WaitForAuthChallenge(1);
    ASSERT_TRUE(test_delegate.auth_response_info().headers);
    EXPECT_EQ(407, test_delegate.auth_response_info().headers->response_code());
    std::string proxy_authenticate;
    ASSERT_TRUE(test_delegate.auth_response_info().headers->EnumerateHeader(
        nullptr, "Proxy-Authenticate", &proxy_authenticate));
    EXPECT_EQ(proxy_authenticate, "Basic realm=\"MyRealm1\"");
    EXPECT_FALSE(test_delegate.has_result());

    test_delegate.auth_controller()->ResetAuth(AuthCredentials(u"foo", u"bar"));
    test_delegate.RunAuthCallback();
    // Per API contract, the auth callback can't be invoked synchronously.
    EXPECT_FALSE(test_delegate.auth_controller());
    EXPECT_FALSE(test_delegate.has_result());

    test_delegate.WaitForAuthChallenge(2);
    ASSERT_TRUE(test_delegate.auth_response_info().headers);
    EXPECT_EQ(407, test_delegate.auth_response_info().headers->response_code());
    ASSERT_TRUE(test_delegate.auth_response_info().headers->EnumerateHeader(
        nullptr, "Proxy-Authenticate", &proxy_authenticate));
    EXPECT_EQ(proxy_authenticate, "Basic realm=\"MyRealm1\"");
    EXPECT_FALSE(test_delegate.has_result());

    test_delegate.auth_controller()->ResetAuth(AuthCredentials(u"foo", u"bar"));
    test_delegate.RunAuthCallback();
    // Per API contract, the request can't complete synchronously.
    EXPECT_FALSE(test_delegate.has_result());

    EXPECT_EQ(OK, test_delegate.WaitForResult());
    EXPECT_EQ(2, test_delegate.num_auth_challenges());

    // Close the H2 session to prevent reuse.
    if (GetParam() == SPDY) {
      session_->CloseAllConnections(ERR_FAILED, "Very good reason");
    }
    // Also need to clear the auth cache before re-running the test.
    session_->http_auth_cache()->ClearAllEntries();
  }
}

// Test the case where auth credentials are cached.
TEST_P(HttpProxyConnectJobTest, HaveAuth) {
  // Prepopulate auth cache.
  const std::u16string kFoo(u"foo");
  const std::u16string kBar(u"bar");
  url::SchemeHostPort proxy_scheme_host_port(
      GetParam() == HTTP ? GURL(std::string("http://") + kHttpProxyHost)
                         : GURL(std::string("https://") + kHttpsProxyHost));
  session_->http_auth_cache()->Add(
      proxy_scheme_host_port, HttpAuth::AUTH_PROXY, "MyRealm1",
      HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
      "Basic realm=MyRealm1", AuthCredentials(kFoo, kBar), "/");

  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);

    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);

    MockWrite writes[] = {
        MockWrite(io_mode, 0,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n"
                  "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };
    MockRead reads[] = {
        MockRead(io_mode, 1, "HTTP/1.1 200 Connection Established\r\n\r\n"),
    };

    const char* const kSpdyAuthCredentials[] = {
        "user-agent",
        "test-ua",
        "proxy-authorization",
        "Basic Zm9vOmJhcg==",
    };
    SpdyTestUtil spdy_util;
    spdy::SpdySerializedFrame connect(spdy_util.ConstructSpdyConnect(
        kSpdyAuthCredentials, std::size(kSpdyAuthCredentials) / 2, 1,
        HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair(kEndpointHost, 443)));

    MockWrite spdy_writes[] = {
        CreateMockWrite(connect, 0, ASYNC),
    };

    spdy::SpdySerializedFrame connect_resp(
        spdy_util.ConstructSpdyGetReply(nullptr, 0, 1));
    MockRead spdy_reads[] = {
        // SpdySession starts trying to read from the socket as soon as it's
        // created, so this cannot be SYNCHRONOUS.
        CreateMockRead(connect_resp, 1, ASYNC),
        MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
    };

    Initialize(reads, writes, spdy_reads, spdy_writes, io_mode);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);
    // SPDY operations always complete asynchronously.
    test_delegate.StartJobExpectingResult(
        connect_job.get(), OK, io_mode == SYNCHRONOUS && GetParam() != SPDY);

    // Close the H2 session to prevent reuse.
    if (GetParam() == SPDY) {
      session_->CloseAllConnections(ERR_FAILED, "Very good reason");
    }
  }
}

TEST_P(HttpProxyConnectJobTest, HostResolutionFailure) {
  session_deps_.host_resolver->rules()->AddSimulatedTimeoutFailure(
      kHttpProxyHost);
  session_deps_.host_resolver->rules()->AddSimulatedTimeoutFailure(
      kHttpsProxyHost);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job =
      CreateConnectJobForHttpRequest(&test_delegate, DEFAULT_PRIORITY);
  test_delegate.StartJobExpectingResult(connect_job.get(),
                                        ERR_PROXY_CONNECTION_FAILED,
                                        false /* expect_sync_result */);
  EXPECT_THAT(connect_job->GetResolveErrorInfo().error,
              test::IsError(ERR_DNS_TIMED_OUT));
}

TEST_P(HttpProxyConnectJobTest, RequestPriority) {
  // Make request hang during host resolution, so can observe priority there.
  session_deps_.host_resolver->set_ondemand_mode(true);

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
      std::unique_ptr<ConnectJob> connect_job = CreateConnectJobForHttpRequest(
          &test_delegate, static_cast<RequestPriority>(initial_priority));
      EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
      EXPECT_FALSE(test_delegate.has_result());

      MockHostResolverBase* host_resolver = session_deps_.host_resolver.get();
      size_t request_id = host_resolver->last_id();
      EXPECT_EQ(initial_priority, host_resolver->request_priority(request_id));

      connect_job->ChangePriority(static_cast<RequestPriority>(new_priority));
      EXPECT_EQ(new_priority, host_resolver->request_priority(request_id));

      connect_job->ChangePriority(
          static_cast<RequestPriority>(initial_priority));
      EXPECT_EQ(initial_priority, host_resolver->request_priority(request_id));
    }
  }
}

TEST_P(HttpProxyConnectJobTest, SecureDnsPolicy) {
  for (auto secure_dns_policy :
       {SecureDnsPolicy::kAllow, SecureDnsPolicy::kDisable}) {
    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job = CreateConnectJobForHttpRequest(
        &test_delegate, DEFAULT_PRIORITY, secure_dns_policy);

    EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
    EXPECT_EQ(secure_dns_policy,
              session_deps_.host_resolver->last_secure_dns_policy());
  }
}

TEST_P(HttpProxyConnectJobTest, SpdySessionKeyDisableSecureDns) {
  if (GetParam() != SPDY) {
    return;
  }

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  InitializeSpdySsl(&ssl_data);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  // SPDY proxy CONNECT request / response, with a pause during the read.
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead spdy_reads[] = {CreateMockRead(resp, 1), MockRead(ASYNC, 0, 2)};
  SequencedSocketData spdy_data(spdy_reads, spdy_writes);
  spdy_data.set_connect_data(MockConnect(ASYNC, OK));
  SequencedSocketData* sequenced_data = &spdy_data;
  session_deps_.socket_factory->AddSocketDataProvider(sequenced_data);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job = CreateConnectJobForTunnel(
      &test_delegate, DEFAULT_PRIORITY, SecureDnsPolicy::kDisable);

  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
  EXPECT_TRUE(
      common_connect_job_params_->spdy_session_pool->FindAvailableSession(
          SpdySessionKey(kHttpsProxyServer.host_port_pair(),
                         PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                         SessionUsage::kProxy, SocketTag(),
                         NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                         /*disable_cert_verification_network_fetches=*/true),
          /* enable_ip_based_pooling = */ false,
          /* is_websocket = */ false, NetLogWithSource()));
  EXPECT_FALSE(
      common_connect_job_params_->spdy_session_pool->FindAvailableSession(
          SpdySessionKey(kHttpsProxyServer.host_port_pair(),
                         PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                         SessionUsage::kProxy, SocketTag(),
                         NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                         /*disable_cert_verification_network_fetches=*/true),
          /* enable_ip_based_pooling = */ false,
          /* is_websocket = */ false, NetLogWithSource()));
}

// Make sure that HttpProxyConnectJob does not pass on its priority to its
// SPDY session's socket request on Init, or on SetPriority.
TEST_P(HttpProxyConnectJobTest, SetSpdySessionSocketRequestPriority) {
  if (GetParam() != SPDY) {
    return;
  }
  session_deps_.host_resolver->set_synchronous_mode(true);

  // The SPDY CONNECT request should have a priority of kH2QuicTunnelPriority,
  // even though the ConnectJob's priority is set to HIGHEST after connection
  // establishment.
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      nullptr /* extra_headers */, 0 /* extra_header_count */,
      1 /* stream_id */, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0, ASYNC)};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead spdy_reads[] = {CreateMockRead(resp, 1, ASYNC),
                           MockRead(ASYNC, 0, 2)};

  Initialize(base::span<MockRead>(), base::span<MockWrite>(), spdy_reads,
             spdy_writes, SYNCHRONOUS);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job =
      CreateConnectJobForTunnel(&test_delegate, IDLE);
  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_FALSE(test_delegate.has_result());

  connect_job->ChangePriority(HIGHEST);

  // Wait for tunnel to be established. If the frame has a MEDIUM priority
  // instead of highest, the written data will not match what is expected, and
  // the test will fail.
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
}

TEST_P(HttpProxyConnectJobTest, SpdyInadequateTransportSecurity) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kSpdySessionForProxyAdditionalChecks);

  if (GetParam() != SPDY) {
    return;
  }

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  InitializeSpdySsl(&ssl_data);
  // TLS 1.1 is inadequate.
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_1,
                                &ssl_data.ssl_info.connection_status);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  SequencedSocketData spdy_data;
  spdy_data.set_connect_data(MockConnect(ASYNC, OK));
  SequencedSocketData* sequenced_data = &spdy_data;
  session_deps_.socket_factory->AddSocketDataProvider(sequenced_data);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job = CreateConnectJobForTunnel(
      &test_delegate, DEFAULT_PRIORITY, SecureDnsPolicy::kDisable);

  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(),
              test::IsError(ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY));
  EXPECT_FALSE(
      common_connect_job_params_->spdy_session_pool->FindAvailableSession(
          SpdySessionKey(kHttpsProxyServer.host_port_pair(),
                         PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                         SessionUsage::kProxy, SocketTag(),
                         NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                         /*disable_cert_verification_network_fetches=*/true),
          /*enable_ip_based_pooling=*/false,
          /*is_websocket=*/false, NetLogWithSource()));
}

TEST_P(HttpProxyConnectJobTest, SpdyValidAlps) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kSpdySessionForProxyAdditionalChecks);

  if (GetParam() != SPDY) {
    return;
  }

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  InitializeSpdySsl(&ssl_data);
  ssl_data.peer_application_settings = HexDecode(
      "000000"      // length
      "04"          // type SETTINGS
      "00"          // flags
      "00000000");  // stream ID
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  // SPDY proxy CONNECT request / response, with a pause during the read.
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead spdy_reads[] = {CreateMockRead(resp, 1), MockRead(ASYNC, 0, 2)};
  SequencedSocketData spdy_data(spdy_reads, spdy_writes);
  spdy_data.set_connect_data(MockConnect(ASYNC, OK));
  SequencedSocketData* sequenced_data = &spdy_data;
  session_deps_.socket_factory->AddSocketDataProvider(sequenced_data);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job = CreateConnectJobForTunnel(
      &test_delegate, DEFAULT_PRIORITY, SecureDnsPolicy::kDisable);

  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
  EXPECT_TRUE(
      common_connect_job_params_->spdy_session_pool->FindAvailableSession(
          SpdySessionKey(kHttpsProxyServer.host_port_pair(),
                         PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                         SessionUsage::kProxy, SocketTag(),
                         NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                         /*disable_cert_verification_network_fetches=*/true),
          /*enable_ip_based_pooling=*/false,
          /*is_websocket=*/false, NetLogWithSource()));
}

TEST_P(HttpProxyConnectJobTest, SpdyInvalidAlpsCheckEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kSpdySessionForProxyAdditionalChecks);

  if (GetParam() != SPDY) {
    return;
  }

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  InitializeSpdySsl(&ssl_data);
  ssl_data.peer_application_settings = "invalid";
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  SequencedSocketData spdy_data;
  spdy_data.set_connect_data(MockConnect(ASYNC, OK));
  SequencedSocketData* sequenced_data = &spdy_data;
  session_deps_.socket_factory->AddSocketDataProvider(sequenced_data);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job = CreateConnectJobForTunnel(
      &test_delegate, DEFAULT_PRIORITY, SecureDnsPolicy::kDisable);

  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(),
              test::IsError(ERR_HTTP2_PROTOCOL_ERROR));
  EXPECT_FALSE(
      common_connect_job_params_->spdy_session_pool->FindAvailableSession(
          SpdySessionKey(kHttpsProxyServer.host_port_pair(),
                         PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                         SessionUsage::kProxy, SocketTag(),
                         NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                         /*disable_cert_verification_network_fetches=*/true),
          /*enable_ip_based_pooling=*/false,
          /*is_websocket=*/false, NetLogWithSource()));
}

TEST_P(HttpProxyConnectJobTest, SpdyInvalidAlpsCheckDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kSpdySessionForProxyAdditionalChecks);

  if (GetParam() != SPDY) {
    return;
  }

  SSLSocketDataProvider ssl_data(ASYNC, OK);
  InitializeSpdySsl(&ssl_data);
  ssl_data.peer_application_settings = "invalid";
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  // SPDY proxy CONNECT request / response, with a pause during the read.
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0)};
  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
  MockRead spdy_reads[] = {CreateMockRead(resp, 1), MockRead(ASYNC, 0, 2)};
  SequencedSocketData spdy_data(spdy_reads, spdy_writes);
  spdy_data.set_connect_data(MockConnect(ASYNC, OK));
  SequencedSocketData* sequenced_data = &spdy_data;
  session_deps_.socket_factory->AddSocketDataProvider(sequenced_data);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<ConnectJob> connect_job = CreateConnectJobForTunnel(
      &test_delegate, DEFAULT_PRIORITY, SecureDnsPolicy::kDisable);

  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
  EXPECT_TRUE(
      common_connect_job_params_->spdy_session_pool->FindAvailableSession(
          SpdySessionKey(kHttpsProxyServer.host_port_pair(),
                         PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                         SessionUsage::kProxy, SocketTag(),
                         NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                         /*disable_cert_verification_network_fetches=*/true),
          /*enable_ip_based_pooling=*/false,
          /*is_websocket=*/false, NetLogWithSource()));
}

TEST_P(HttpProxyConnectJobTest, TCPError) {
  // SPDY and HTTPS are identical, as they only differ once a connection is
  // established.
  if (GetParam() == SPDY) {
    return;
  }
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);
    base::HistogramTester histogram_tester;

    SequencedSocketData data;
    data.set_connect_data(MockConnect(io_mode, ERR_CONNECTION_CLOSED));
    session_deps_.socket_factory->AddSocketDataProvider(&data);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForHttpRequest(&test_delegate);
    test_delegate.StartJobExpectingResult(
        connect_job.get(), ERR_PROXY_CONNECTION_FAILED, io_mode == SYNCHRONOUS);

    bool is_secure_proxy = GetParam() == HTTPS;
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http1.Http.Error",
        is_secure_proxy ? 0 : 1);
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http1.Https.Error",
        is_secure_proxy ? 1 : 0);
  }
}

TEST_P(HttpProxyConnectJobTest, SSLError) {
  if (GetParam() == HTTP) {
    return;
  }

  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);
    base::HistogramTester histogram_tester;

    SequencedSocketData data;
    data.set_connect_data(MockConnect(io_mode, OK));
    session_deps_.socket_factory->AddSocketDataProvider(&data);

    SSLSocketDataProvider ssl_data(io_mode, ERR_CERT_AUTHORITY_INVALID);
    if (GetParam() == SPDY) {
      InitializeSpdySsl(&ssl_data);
    }
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);
    test_delegate.StartJobExpectingResult(connect_job.get(),
                                          ERR_PROXY_CERTIFICATE_INVALID,
                                          io_mode == SYNCHRONOUS);

    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http1.Https.Error", 1);
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http1.Http.Error", 0);
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http2.Https.Error", 0);
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http2.Http.Error", 0);
  }
}

TEST_P(HttpProxyConnectJobTest, TunnelUnexpectedClose) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);

    MockWrite writes[] = {
        MockWrite(io_mode, 0,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n\r\n"),
    };
    MockRead reads[] = {
        MockRead(io_mode, 1, "HTTP/1.1 200 Conn"),
        MockRead(io_mode, ERR_CONNECTION_CLOSED, 2),
    };
    spdy::SpdySerializedFrame req(SpdyTestUtil().ConstructSpdyConnect(
        nullptr /*extra_headers */, 0 /*extra_header_count */,
        1 /* stream_id */, HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair(kEndpointHost, 443)));
    MockWrite spdy_writes[] = {CreateMockWrite(req, 0, io_mode)};
    // Sync reads don't really work with SPDY, since it constantly reads from
    // the socket.
    MockRead spdy_reads[] = {
        MockRead(ASYNC, ERR_CONNECTION_CLOSED, 1),
    };

    Initialize(reads, writes, spdy_reads, spdy_writes, io_mode);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);

    if (GetParam() == SPDY) {
      // SPDY cannot process a headers block unless it's complete and so it
      // returns ERR_CONNECTION_CLOSED in this case. SPDY also doesn't return
      // this failure synchronously.
      test_delegate.StartJobExpectingResult(connect_job.get(),
                                            ERR_CONNECTION_CLOSED,
                                            false /* expect_sync_result */);
    } else {
      test_delegate.StartJobExpectingResult(connect_job.get(),
                                            ERR_RESPONSE_HEADERS_TRUNCATED,
                                            io_mode == SYNCHRONOUS);
    }
  }
}

TEST_P(HttpProxyConnectJobTest, Tunnel1xxResponse) {
  // Tests that 1xx responses are rejected for a CONNECT request.
  if (GetParam() == SPDY) {
    // SPDY doesn't have 1xx responses.
    return;
  }

  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);

    MockWrite writes[] = {
        MockWrite(io_mode, 0,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n\r\n"),
    };
    MockRead reads[] = {
        MockRead(io_mode, 1, "HTTP/1.1 100 Continue\r\n\r\n"),
        MockRead(io_mode, 2, "HTTP/1.1 200 Connection Established\r\n\r\n"),
    };

    Initialize(reads, writes, base::span<MockRead>(), base::span<MockWrite>(),
               io_mode);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);
    test_delegate.StartJobExpectingResult(connect_job.get(),
                                          ERR_TUNNEL_CONNECTION_FAILED,
                                          io_mode == SYNCHRONOUS);
  }
}

TEST_P(HttpProxyConnectJobTest, TunnelSetupError) {
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);

    MockWrite writes[] = {
        MockWrite(io_mode, 0,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n\r\n"),
    };
    MockRead reads[] = {
        MockRead(io_mode, 1, "HTTP/1.1 304 Not Modified\r\n\r\n"),
    };
    SpdyTestUtil spdy_util;
    spdy::SpdySerializedFrame req(spdy_util.ConstructSpdyConnect(
        nullptr /* extra_headers */, 0 /* extra_header_count */,
        1 /* stream_id */, HttpProxyConnectJob::kH2QuicTunnelPriority,
        HostPortPair("www.endpoint.test", 443)));
    spdy::SpdySerializedFrame rst(
        spdy_util.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
    MockWrite spdy_writes[] = {
        CreateMockWrite(req, 0, io_mode),
        CreateMockWrite(rst, 2, io_mode),
    };
    spdy::SpdySerializedFrame resp(spdy_util.ConstructSpdyReplyError(1));
    // Sync reads don't really work with SPDY, since it constantly reads from
    // the socket.
    MockRead spdy_reads[] = {
        CreateMockRead(resp, 1, ASYNC),
        MockRead(ASYNC, OK, 3),
    };

    Initialize(reads, writes, spdy_reads, spdy_writes, io_mode);

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate, LOW);
    test_delegate.StartJobExpectingResult(
        connect_job.get(), ERR_TUNNEL_CONNECTION_FAILED,
        io_mode == SYNCHRONOUS && GetParam() != SPDY);
    // Need to close the session to prevent reuse in the next loop iteration.
    session_->spdy_session_pool()->CloseAllSessions();
  }
}

TEST_P(HttpProxyConnectJobTest, SslClientAuth) {
  if (GetParam() == HTTP) {
    return;
  }
  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);
    base::HistogramTester histogram_tester;

    SequencedSocketData socket_data(MockConnect(io_mode, OK),
                                    base::span<const MockRead>(),
                                    base::span<const MockWrite>());
    session_deps_.socket_factory->AddSocketDataProvider(&socket_data);
    SSLSocketDataProvider ssl_data(io_mode, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
    if (GetParam() == SPDY) {
      InitializeSpdySsl(&ssl_data);
    }
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data);

    // Redirects in the HTTPS case return errors, but also return sockets.
    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);
    test_delegate.StartJobExpectingResult(connect_job.get(),
                                          ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
                                          io_mode == SYNCHRONOUS);

    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http1.Https.Error", 1);
    histogram_tester.ExpectTotalCount(
        "Net.HttpProxy.ConnectLatency.Http1.Http.Error", 0);
  }
}

TEST_P(HttpProxyConnectJobTest, TunnelSetupRedirect) {
  const std::string kRedirectTarget = "https://foo.google.com/";

  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    session_deps_.host_resolver->set_synchronous_mode(io_mode == SYNCHRONOUS);

    const std::string kResponseText =
        "HTTP/1.1 302 Found\r\n"
        "Location: " +
        kRedirectTarget +
        "\r\n"
        "Set-Cookie: foo=bar\r\n"
        "\r\n";

    MockWrite writes[] = {
        MockWrite(io_mode, 0,
                  "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                  "Host: www.endpoint.test:443\r\n"
                  "Proxy-Connection: keep-alive\r\n"
                  "User-Agent: test-ua\r\n\r\n"),
    };
    MockRead reads[] = {
        MockRead(io_mode, 1, kResponseText.c_str()),
    };
    SpdyTestUtil spdy_util;
    spdy::SpdySerializedFrame req(spdy_util.ConstructSpdyConnect(
        nullptr /* extra_headers */, 0 /* extra_header_count */, 1,
        DEFAULT_PRIORITY, HostPortPair(kEndpointHost, 443)));
    spdy::SpdySerializedFrame rst(
        spdy_util.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));

    MockWrite spdy_writes[] = {
        CreateMockWrite(req, 0, io_mode),
        CreateMockWrite(rst, 3, io_mode),
    };

    const char* const responseHeaders[] = {
        "location",
        kRedirectTarget.c_str(),
        "set-cookie",
        "foo=bar",
    };
    const int responseHeadersSize = std::size(responseHeaders) / 2;
    spdy::SpdySerializedFrame resp(spdy_util.ConstructSpdyReplyError(
        "302", responseHeaders, responseHeadersSize, 1));
    MockRead spdy_reads[] = {
        CreateMockRead(resp, 1, ASYNC),
        MockRead(ASYNC, 0, 2),
    };

    Initialize(reads, writes, spdy_reads, spdy_writes, io_mode);

    // Redirects during CONNECT returns an error.
    TestConnectJobDelegate test_delegate(
        TestConnectJobDelegate::SocketExpected::ON_SUCCESS_ONLY);
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);

    // H2 never completes synchronously.
    bool expect_sync_result = (io_mode == SYNCHRONOUS && GetParam() != SPDY);

    // We don't trust 302 responses to CONNECT from proxies.
    test_delegate.StartJobExpectingResult(
        connect_job.get(), ERR_TUNNEL_CONNECTION_FAILED, expect_sync_result);
    EXPECT_FALSE(test_delegate.socket());

    // Need to close the session to prevent reuse in the next loop iteration.
    session_->spdy_session_pool()->CloseAllSessions();
  }
}

// Test timeouts in the case of an auth challenge and response.
TEST_P(HttpProxyConnectJobTest, TestTimeoutsAuthChallenge) {
  // Wait until this amount of time before something times out.
  const base::TimeDelta kTinyTime = base::Microseconds(1);

  enum class TimeoutPhase {
    CONNECT,
    PROXY_HANDSHAKE,
    SECOND_PROXY_HANDSHAKE,

    NONE,
  };

  const TimeoutPhase kTimeoutPhases[] = {
      TimeoutPhase::CONNECT,
      TimeoutPhase::PROXY_HANDSHAKE,
      TimeoutPhase::SECOND_PROXY_HANDSHAKE,
      TimeoutPhase::NONE,
  };

  session_deps_.host_resolver->set_ondemand_mode(true);

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                "Host: www.endpoint.test:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
      MockWrite(ASYNC, 3,
                "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                "Host: www.endpoint.test:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      // Pause before first response is read.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 2,
               "HTTP/1.1 407 Proxy Authentication Required\r\n"
               "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"
               "Content-Length: 0\r\n\r\n"),

      // Pause again before second response is read.
      MockRead(ASYNC, ERR_IO_PENDING, 4),
      MockRead(ASYNC, 5, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  SpdyTestUtil spdy_util;
  spdy::SpdySerializedFrame connect(spdy_util.ConstructSpdyConnect(
      nullptr, 0, 1, HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));
  spdy::SpdySerializedFrame rst(
      spdy_util.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  spdy_util.UpdateWithStreamDestruction(1);

  // After calling trans.RestartWithAuth(), this is the request we should
  // be issuing -- the final header line contains the credentials.
  const char* const kSpdyAuthCredentials[] = {
      "user-agent",
      "test-ua",
      "proxy-authorization",
      "Basic Zm9vOmJhcg==",
  };
  spdy::SpdySerializedFrame connect2(spdy_util.ConstructSpdyConnect(
      kSpdyAuthCredentials, std::size(kSpdyAuthCredentials) / 2, 3,
      HttpProxyConnectJob::kH2QuicTunnelPriority,
      HostPortPair(kEndpointHost, 443)));
  // This may be sent in some tests, either when tearing down a successful
  // connection, or on timeout.
  spdy::SpdySerializedFrame rst2(
      spdy_util.ConstructSpdyRstStream(3, spdy::ERROR_CODE_CANCEL));
  MockWrite spdy_writes[] = {
      CreateMockWrite(connect, 0, ASYNC),
      CreateMockWrite(rst, 3, ASYNC),
      CreateMockWrite(connect2, 4, ASYNC),
      CreateMockWrite(rst2, 8, ASYNC),
  };

  // The proxy responds to the connect with a 407, using a persistent
  // connection.
  const char kAuthStatus[] = "407";
  const char* const kAuthChallenge[] = {
      "proxy-authenticate",
      "Basic realm=\"MyRealm1\"",
  };
  spdy::SpdySerializedFrame connect_auth_resp(spdy_util.ConstructSpdyReplyError(
      kAuthStatus, kAuthChallenge, std::size(kAuthChallenge) / 2, 1));
  spdy::SpdySerializedFrame connect2_resp(
      spdy_util.ConstructSpdyGetReply(nullptr, 0, 3));
  MockRead spdy_reads[] = {
      // Pause before first response is read.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      CreateMockRead(connect_auth_resp, 2, ASYNC),
      // Pause again before second response is read.
      MockRead(ASYNC, ERR_IO_PENDING, 5),
      CreateMockRead(connect2_resp, 6, ASYNC),
      MockRead(ASYNC, OK, 7),
  };

  for (TimeoutPhase timeout_phase : kTimeoutPhases) {
    SCOPED_TRACE(static_cast<int>(timeout_phase));

    // Need to close the session to prevent reuse of a session from the last
    // loop iteration.
    session_->spdy_session_pool()->CloseAllSessions();
    // And clear the auth cache to prevent reusing cache entries.
    session_->http_auth_cache()->ClearAllEntries();

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);

    // Connecting should run until the request hits the HostResolver.
    EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
    EXPECT_FALSE(test_delegate.has_result());
    EXPECT_TRUE(session_deps_.host_resolver->has_pending_requests());
    EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, connect_job->GetLoadState());

    // Run until just before timeout.
    FastForwardBy(GetNestedConnectionTimeout() - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    // Wait until timeout, if appropriate.
    if (timeout_phase == TimeoutPhase::CONNECT) {
      FastForwardBy(kTinyTime);
      ASSERT_TRUE(test_delegate.has_result());
      EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
      continue;
    }

    // Add mock reads for socket needed in next step. Connect phase is timed out
    // before establishing a connection, so don't need them for
    // TimeoutPhase::CONNECT.
    Initialize(reads, writes, spdy_reads, spdy_writes, SYNCHRONOUS);

    // Finish resolution.
    session_deps_.host_resolver->ResolveOnlyRequestNow();
    EXPECT_FALSE(test_delegate.has_result());
    EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
              connect_job->GetLoadState());

    // Wait until just before negotiation with the tunnel should time out.
    FastForwardBy(HttpProxyConnectJob::TunnelTimeoutForTesting() - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    if (timeout_phase == TimeoutPhase::PROXY_HANDSHAKE) {
      FastForwardBy(kTinyTime);
      ASSERT_TRUE(test_delegate.has_result());
      EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
      continue;
    }

    data_->Resume();
    test_delegate.WaitForAuthChallenge(1);
    EXPECT_FALSE(test_delegate.has_result());

    // ConnectJobs cannot timeout while showing an auth dialog.
    FastForwardBy(base::Days(1));
    EXPECT_FALSE(test_delegate.has_result());

    // Send credentials
    test_delegate.auth_controller()->ResetAuth(AuthCredentials(u"foo", u"bar"));
    test_delegate.RunAuthCallback();
    EXPECT_FALSE(test_delegate.has_result());

    FastForwardBy(HttpProxyConnectJob::TunnelTimeoutForTesting() - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    if (timeout_phase == TimeoutPhase::SECOND_PROXY_HANDSHAKE) {
      FastForwardBy(kTinyTime);
      ASSERT_TRUE(test_delegate.has_result());
      EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
      continue;
    }

    data_->Resume();
    EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
  }
}

// Same as above, except test the case the first connection cannot be reused
// once credentials are received.
TEST_P(HttpProxyConnectJobTest, TestTimeoutsAuthChallengeNewConnection) {
  // Proxy-Connection: Close doesn't make sense with H2.
  if (GetParam() == SPDY) {
    return;
  }

  enum class TimeoutPhase {
    CONNECT,
    PROXY_HANDSHAKE,
    SECOND_CONNECT,
    SECOND_PROXY_HANDSHAKE,

    // This has to be last for the H2 proxy case, since success will populate
    // the H2 session pool.
    NONE,
  };

  const TimeoutPhase kTimeoutPhases[] = {
      TimeoutPhase::CONNECT,        TimeoutPhase::PROXY_HANDSHAKE,
      TimeoutPhase::SECOND_CONNECT, TimeoutPhase::SECOND_PROXY_HANDSHAKE,
      TimeoutPhase::NONE,
  };

  // Wait until this amount of time before something times out.
  const base::TimeDelta kTinyTime = base::Microseconds(1);

  session_deps_.host_resolver->set_ondemand_mode(true);

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                "Host: www.endpoint.test:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n\r\n"),
  };
  MockRead reads[] = {
      // Pause at read.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 2,
               "HTTP/1.1 407 Proxy Authentication Required\r\n"
               "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"
               "Proxy-Connection: Close\r\n"
               "Content-Length: 0\r\n\r\n"),
  };

  MockWrite writes2[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.endpoint.test:443 HTTP/1.1\r\n"
                "Host: www.endpoint.test:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "User-Agent: test-ua\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads2[] = {
      // Pause at read.
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, 2, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  for (TimeoutPhase timeout_phase : kTimeoutPhases) {
    SCOPED_TRACE(static_cast<int>(timeout_phase));

    // Need to clear the auth cache to prevent reusing cache entries.
    session_->http_auth_cache()->ClearAllEntries();

    TestConnectJobDelegate test_delegate;
    std::unique_ptr<ConnectJob> connect_job =
        CreateConnectJobForTunnel(&test_delegate);

    // Connecting should run until the request hits the HostResolver.
    EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
    EXPECT_FALSE(test_delegate.has_result());
    EXPECT_TRUE(session_deps_.host_resolver->has_pending_requests());
    EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, connect_job->GetLoadState());

    // Run until just before timeout.
    FastForwardBy(GetNestedConnectionTimeout() - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    // Wait until timeout, if appropriate.
    if (timeout_phase == TimeoutPhase::CONNECT) {
      FastForwardBy(kTinyTime);
      ASSERT_TRUE(test_delegate.has_result());
      EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
      continue;
    }

    // Add mock reads for socket needed in next step. Connect phase is timed out
    // before establishing a connection, so don't need them for
    // TimeoutPhase::CONNECT.
    Initialize(reads, writes, base::span<MockRead>(), base::span<MockWrite>(),
               SYNCHRONOUS);

    // Finish resolution.
    session_deps_.host_resolver->ResolveOnlyRequestNow();
    EXPECT_FALSE(test_delegate.has_result());
    EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
              connect_job->GetLoadState());

    // Wait until just before negotiation with the tunnel should time out.
    FastForwardBy(HttpProxyConnectJob::TunnelTimeoutForTesting() - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    if (timeout_phase == TimeoutPhase::PROXY_HANDSHAKE) {
      FastForwardBy(kTinyTime);
      ASSERT_TRUE(test_delegate.has_result());
      EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
      continue;
    }

    data_->Resume();
    test_delegate.WaitForAuthChallenge(1);
    EXPECT_FALSE(test_delegate.has_result());

    // ConnectJobs cannot timeout while showing an auth dialog.
    FastForwardBy(base::Days(1));
    EXPECT_FALSE(test_delegate.has_result());

    // Send credentials
    test_delegate.auth_controller()->ResetAuth(AuthCredentials(u"foo", u"bar"));
    test_delegate.RunAuthCallback();
    EXPECT_FALSE(test_delegate.has_result());

    // Since the connection was not reusable, a new connection needs to be
    // established.
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(test_delegate.has_result());
    EXPECT_TRUE(session_deps_.host_resolver->has_pending_requests());
    EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, connect_job->GetLoadState());

    // Run until just before timeout.
    FastForwardBy(GetNestedConnectionTimeout() - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    // Wait until timeout, if appropriate.
    if (timeout_phase == TimeoutPhase::SECOND_CONNECT) {
      FastForwardBy(kTinyTime);
      ASSERT_TRUE(test_delegate.has_result());
      EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
      continue;
    }

    // Add mock reads for socket needed in next step. Connect phase is timed out
    // before establishing a connection, so don't need them for
    // TimeoutPhase::SECOND_CONNECT.
    Initialize(reads2, writes2, base::span<MockRead>(), base::span<MockWrite>(),
               SYNCHRONOUS);

    // Finish resolution.
    session_deps_.host_resolver->ResolveOnlyRequestNow();
    EXPECT_FALSE(test_delegate.has_result());
    EXPECT_EQ(LOAD_STATE_ESTABLISHING_PROXY_TUNNEL,
              connect_job->GetLoadState());

    // Wait until just before negotiation with the tunnel should time out.
    FastForwardBy(HttpProxyConnectJob::TunnelTimeoutForTesting() - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    if (timeout_phase == TimeoutPhase::SECOND_PROXY_HANDSHAKE) {
      FastForwardBy(kTinyTime);
      ASSERT_TRUE(test_delegate.has_result());
      EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
      continue;
    }

    data_->Resume();
    ASSERT_TRUE(test_delegate.has_result());
    EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
  }
}

TEST_P(HttpProxyConnectJobTest, ConnectionTimeoutNoNQE) {
  // Doesn't actually matter whether or not this is for a tunnel - the
  // connection timeout is the same, though it probably shouldn't be the same,
  // since tunnels need an extra round trip.
  base::TimeDelta alternate_connection_timeout =
      HttpProxyConnectJob::AlternateNestedConnectionTimeout(
          *CreateParams(true /* tunnel */, SecureDnsPolicy::kAllow),
          /*network_quality_estimator=*/nullptr);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On Android and iOS, when there's no NQE, there's a hard-coded alternate
  // proxy timeout.
  EXPECT_EQ(base::Seconds(10), alternate_connection_timeout);
#else
  // On other platforms, there is not.
  EXPECT_EQ(base::TimeDelta(), alternate_connection_timeout);
#endif
}

TEST_P(HttpProxyConnectJobTest, ConnectionTimeoutMin) {
  // Set RTT estimate to a low value.
  base::TimeDelta rtt_estimate = base::Milliseconds(1);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);

  EXPECT_LE(base::TimeDelta(), GetNestedConnectionTimeout());

  // Test against a large value.
  EXPECT_GE(base::Minutes(10), GetNestedConnectionTimeout());

  EXPECT_EQ(base::Seconds(8), GetNestedConnectionTimeout());
}

TEST_P(HttpProxyConnectJobTest, ConnectionTimeoutMax) {
  // Set RTT estimate to a high value.
  base::TimeDelta rtt_estimate = base::Seconds(100);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);

  EXPECT_LE(base::TimeDelta(), GetNestedConnectionTimeout());

  // Test against a large value.
  EXPECT_GE(base::Minutes(10), GetNestedConnectionTimeout());

  EXPECT_EQ(base::Seconds(30), GetNestedConnectionTimeout());
}

// Tests the connection timeout values when the field trial parameters are
// specified.
TEST_P(HttpProxyConnectJobTest, ConnectionTimeoutWithExperiment) {
  // Timeout should be kMultiplier times the HTTP RTT estimate.
  const int kMultiplier = 4;
  const base::TimeDelta kMinTimeout = base::Seconds(8);
  const base::TimeDelta kMaxTimeout = base::Seconds(20);

  InitAdaptiveTimeoutFieldTrialWithParams(false, kMultiplier, kMultiplier,
                                          kMinTimeout, kMaxTimeout);
  EXPECT_LE(base::TimeDelta(), GetNestedConnectionTimeout());

  base::TimeDelta rtt_estimate = base::Seconds(4);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  base::TimeDelta expected_connection_timeout = kMultiplier * rtt_estimate;
  EXPECT_EQ(expected_connection_timeout, GetNestedConnectionTimeout());

  // Connection timeout should not exceed kMaxTimeout.
  rtt_estimate = base::Seconds(25);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMaxTimeout, GetNestedConnectionTimeout());

  // Connection timeout should not be less than kMinTimeout.
  rtt_estimate = base::Seconds(0);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMinTimeout, GetNestedConnectionTimeout());
}

// Tests the connection timeout values when the field trial parameters are
// specified.
TEST_P(HttpProxyConnectJobTest, ConnectionTimeoutExperimentDifferentParams) {
  // Timeout should be kMultiplier times the HTTP RTT estimate.
  const int kMultiplier = 3;
  const base::TimeDelta kMinTimeout = base::Seconds(2);
  const base::TimeDelta kMaxTimeout = base::Seconds(30);

  InitAdaptiveTimeoutFieldTrialWithParams(false, kMultiplier, kMultiplier,
                                          kMinTimeout, kMaxTimeout);
  EXPECT_LE(base::TimeDelta(), GetNestedConnectionTimeout());

  base::TimeDelta rtt_estimate = base::Seconds(2);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMultiplier * rtt_estimate, GetNestedConnectionTimeout());

  // A change in RTT estimate should also change the connection timeout.
  rtt_estimate = base::Seconds(7);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMultiplier * rtt_estimate, GetNestedConnectionTimeout());

  // Connection timeout should not exceed kMaxTimeout.
  rtt_estimate = base::Seconds(35);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMaxTimeout, GetNestedConnectionTimeout());

  // Connection timeout should not be less than kMinTimeout.
  rtt_estimate = base::Seconds(0);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMinTimeout, GetNestedConnectionTimeout());
}

TEST_P(HttpProxyConnectJobTest, ConnectionTimeoutWithConnectionProperty) {
  const int kSecureMultiplier = 3;
  const int kNonSecureMultiplier = 5;
  const base::TimeDelta kMinTimeout = base::Seconds(2);
  const base::TimeDelta kMaxTimeout = base::Seconds(30);

  InitAdaptiveTimeoutFieldTrialWithParams(
      false, kSecureMultiplier, kNonSecureMultiplier, kMinTimeout, kMaxTimeout);

  const base::TimeDelta kRttEstimate = base::Seconds(2);
  network_quality_estimator_->SetStartTimeNullHttpRtt(kRttEstimate);
  // By default, connection timeout should return the timeout for secure
  // proxies.
  if (GetParam() != HTTP) {
    EXPECT_EQ(kSecureMultiplier * kRttEstimate, GetNestedConnectionTimeout());
  } else {
    EXPECT_EQ(kNonSecureMultiplier * kRttEstimate,
              GetNestedConnectionTimeout());
  }
}

// Tests the connection timeout values when the field trial parameters are not
// specified.
TEST_P(HttpProxyConnectJobTest, ProxyPoolTimeoutWithExperimentDefaultParams) {
  InitAdaptiveTimeoutFieldTrialWithParams(true, 0, 0, base::TimeDelta(),
                                          base::TimeDelta());
  EXPECT_LE(base::TimeDelta(), GetNestedConnectionTimeout());

  // Timeout should be |http_rtt_multiplier| times the HTTP RTT
  // estimate.
  base::TimeDelta rtt_estimate = base::Milliseconds(10);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  // Connection timeout should not be less than the HTTP RTT estimate.
  EXPECT_LE(rtt_estimate, GetNestedConnectionTimeout());

  // A change in RTT estimate should also change the connection timeout.
  rtt_estimate = base::Seconds(10);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  // Connection timeout should not be less than the HTTP RTT estimate.
  EXPECT_LE(rtt_estimate, GetNestedConnectionTimeout());

  // Set RTT to a very large value.
  rtt_estimate = base::Minutes(60);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_GT(rtt_estimate, GetNestedConnectionTimeout());

  // Set RTT to a very small value.
  rtt_estimate = base::Seconds(0);
  network_quality_estimator_->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_LT(rtt_estimate, GetNestedConnectionTimeout());
}

// A Mock QuicSessionPool which can intercept calls to RequestSession.
class MockQuicSessionPool : public QuicSessionPool {
 public:
  explicit MockQuicSessionPool(HttpServerProperties* http_server_properties,
                               CertVerifier* cert_verifier,
                               TransportSecurityState* transport_security_state,
                               QuicContext* context)
      : QuicSessionPool(/*net_log=*/nullptr,
                        /*host_resolver=*/nullptr,
                        /*ssl_config_service=*/nullptr,
                        /*client_socket_factory=*/nullptr,
                        http_server_properties,
                        cert_verifier,
                        transport_security_state,
                        /*proxy_delegate=*/nullptr,
                        /*sct_auditing_delegate=*/nullptr,
                        /*socket_performance_watcher_factory=*/nullptr,
                        /*quic_crypto_client_stream_factory=*/nullptr,
                        context) {}

  MockQuicSessionPool(const MockQuicSessionPool&) = delete;
  MockQuicSessionPool& operator=(const MockQuicSessionPool&) = delete;

  ~MockQuicSessionPool() override = default;

  // Requests are cancelled during test tear-down, so ignore those calls.
  MOCK_METHOD1(CancelRequest, void(QuicSessionRequest* request));

  MOCK_METHOD(
      int,
      RequestSession,
      (const QuicSessionKey& session_key,
       url::SchemeHostPort destination,
       quic::ParsedQuicVersion quic_version,
       const std::optional<NetworkTrafficAnnotationTag> proxy_annotation_tag,
       const HttpUserAgentSettings* http_user_agent_settings,
       RequestPriority priority,
       bool use_dns_aliases,
       int cert_verify_flags,
       const GURL& url,
       const NetLogWithSource& net_log,
       QuicSessionRequest* request));
};

class HttpProxyConnectQuicJobTest : public HttpProxyConnectJobTestBase,
                                    public testing::Test {
 public:
  HttpProxyConnectQuicJobTest()
      : mock_quic_session_pool_(session_->http_server_properties(),
                                session_->cert_verifier(),
                                session_->context().transport_security_state,
                                session_->context().quic_context) {
    common_connect_job_params_->quic_session_pool = &mock_quic_session_pool_;
  }

 protected:
  MockQuicSessionPool mock_quic_session_pool_;
};

// Test that a QUIC session is properly requested from the QuicSessionPool.
TEST_F(HttpProxyConnectQuicJobTest, RequestQuicProxy) {
  // Create params for a single-hop QUIC proxy. This consists of an
  // HttpProxySocketParams, an SSLSocketParams from which a few values are used,
  // and a TransportSocketParams which is totally unused but must be non-null.
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({ProxyServer(
      ProxyServer::SCHEME_QUIC, HostPortPair(kQuicProxyHost, 443))});
  SSLConfig quic_ssl_config;
  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params =
      base::MakeRefCounted<HttpProxySocketParams>(
          quic_ssl_config, HostPortPair(kEndpointHost, 443), proxy_chain,
          /*proxy_chain_index=*/0, /*tunnel=*/true,
          TRAFFIC_ANNOTATION_FOR_TESTS, NetworkAnonymizationKey(),
          SecureDnsPolicy::kAllow);

  TestConnectJobDelegate test_delegate;
  auto connect_job = std::make_unique<HttpProxyConnectJob>(
      DEFAULT_PRIORITY, SocketTag(), common_connect_job_params_.get(),
      std::move(http_proxy_socket_params), &test_delegate,
      /*net_log=*/nullptr);

  // Expect a session to be requested, and then leave it pending.
  EXPECT_CALL(mock_quic_session_pool_,
              RequestSession(_, _, _, _, _, _, _, _, _, _,
                             QSRHasProxyChain(proxy_chain.Prefix(0))))
      .Times(1)
      .WillRepeatedly(testing::Return(ERR_IO_PENDING));

  // Expect the request to be cancelled during test tear-down.
  EXPECT_CALL(mock_quic_session_pool_, CancelRequest).Times(1);

  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
}

// Test that for QUIC sessions to the proxy, version RFCv1 is used.
TEST_F(HttpProxyConnectQuicJobTest, QuicProxyRequestUsesRfcV1) {
  // While the default supported QUIC version is RFCv1, to test that RFCv1 is
  // forced for proxy connections we need to specify a different default. If
  // that ever changes and we still want to continue forcing QUIC connections to
  // proxy servers to use RFCv1, then we won't need to modify
  // `supported_versions` anymore (and could merge this test with
  // RequestQuicProxy above).
  ASSERT_EQ(DefaultSupportedQuicVersions()[0],
            quic::ParsedQuicVersion::RFCv1());

  auto supported_versions = quic::ParsedQuicVersionVector{
      quic::ParsedQuicVersion::RFCv2(), quic::ParsedQuicVersion::RFCv1()};
  common_connect_job_params_->quic_supported_versions = &supported_versions;

  ProxyChain proxy_chain = ProxyChain::ForIpProtection({ProxyServer(
      ProxyServer::SCHEME_QUIC, HostPortPair(kQuicProxyHost, 443))});
  SSLConfig quic_ssl_config;
  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params =
      base::MakeRefCounted<HttpProxySocketParams>(
          quic_ssl_config, HostPortPair(kEndpointHost, 443), proxy_chain,
          /*proxy_chain_index=*/0, /*tunnel=*/true,
          TRAFFIC_ANNOTATION_FOR_TESTS, NetworkAnonymizationKey(),
          SecureDnsPolicy::kAllow);

  TestConnectJobDelegate test_delegate;
  auto connect_job = std::make_unique<HttpProxyConnectJob>(
      DEFAULT_PRIORITY, SocketTag(), common_connect_job_params_.get(),
      std::move(http_proxy_socket_params), &test_delegate,
      /*net_log=*/nullptr);

  // Expect a session to be requested, and then leave it pending.
  EXPECT_CALL(
      mock_quic_session_pool_,
      RequestSession(_, _, IsQuicVersion(quic::ParsedQuicVersion::RFCv1()), _,
                     _, _, _, _, _, _, QSRHasProxyChain(proxy_chain.Prefix(0))))

      .Times(1)
      .WillRepeatedly(testing::Return(ERR_IO_PENDING));

  // Expect the request to be cancelled during test tear-down.
  EXPECT_CALL(mock_quic_session_pool_, CancelRequest).Times(1);

  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));

  // Since we set `common_connect_job_params_->quic_supported_versions` to the
  // address of a local variable above, clear it here to avoid having a dangling
  // pointer.
  common_connect_job_params_->quic_supported_versions = nullptr;
}

// Test that a QUIC session is properly requested from the QuicSessionPool,
// including a ProxyChain containing additional QUIC proxies, but excluding any
// proxies later in the chain.
TEST_F(HttpProxyConnectQuicJobTest, RequestMultipleQuicProxies) {
  // Create params for a two-proxy QUIC proxy, as a prefix of a larger chain.
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer(ProxyServer::SCHEME_QUIC, HostPortPair("qproxy1", 443)),
      // The proxy_chain_index points to this ProxyServer:
      ProxyServer(ProxyServer::SCHEME_QUIC, HostPortPair("qproxy2", 443)),
      ProxyServer(ProxyServer::SCHEME_HTTPS, HostPortPair("hproxy1", 443)),
      ProxyServer(ProxyServer::SCHEME_HTTPS, HostPortPair("hproxy2", 443)),
  });
  SSLConfig quic_ssl_config;
  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params =
      base::MakeRefCounted<HttpProxySocketParams>(
          quic_ssl_config, HostPortPair(kEndpointHost, 443), proxy_chain,
          /*proxy_chain_index=*/1, /*tunnel=*/true,
          TRAFFIC_ANNOTATION_FOR_TESTS, NetworkAnonymizationKey(),
          SecureDnsPolicy::kAllow);

  TestConnectJobDelegate test_delegate;
  auto connect_job = std::make_unique<HttpProxyConnectJob>(
      DEFAULT_PRIORITY, SocketTag(), common_connect_job_params_.get(),
      std::move(http_proxy_socket_params), &test_delegate,
      /*net_log=*/nullptr);

  // Expect a session to be requested, and then leave it pending. The requested
  // QUIC session is to `qproxy2`, via proxy chain [`qproxy1`].
  EXPECT_CALL(mock_quic_session_pool_,
              RequestSession(_, _, _, _, _, _, _, _, _, _,
                             QSRHasProxyChain(proxy_chain.Prefix(1))))
      .Times(1)
      .WillRepeatedly(testing::Return(ERR_IO_PENDING));

  // Expect the request to be cancelled during test tear-down.
  EXPECT_CALL(mock_quic_session_pool_, CancelRequest).Times(1);

  EXPECT_THAT(connect_job->Connect(), test::IsError(ERR_IO_PENDING));
}

}  // namespace net
