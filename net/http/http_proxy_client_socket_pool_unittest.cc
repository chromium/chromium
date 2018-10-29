// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include <map>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;
const char * const kAuthHeaders[] = {
  "proxy-authorization", "Basic Zm9vOmJhcg=="
};
const int kAuthHeadersSize = arraysize(kAuthHeaders) / 2;

enum HttpProxyType {
  HTTP,
  HTTPS,
  SPDY
};

const char kHttpProxyHost[] = "httpproxy.example.com";
const char kHttpsProxyHost[] = "httpsproxy.example.com";

}  // namespace

class HttpProxyClientSocketPoolTest
    : public ::testing::TestWithParam<HttpProxyType>,
      public WithScopedTaskEnvironment {
 protected:
  HttpProxyClientSocketPoolTest()
      : transport_socket_pool_(kMaxSockets,
                               kMaxSocketsPerGroup,
                               &socket_factory_),
        ssl_socket_pool_(kMaxSockets,
                         kMaxSocketsPerGroup,
                         session_deps_.cert_verifier.get(),
                         NULL /* channel_id_store */,
                         NULL /* transport_security_state */,
                         NULL /* cert_transparency_verifier */,
                         NULL /* ct_policy_enforcer */,
                         std::string() /* ssl_session_cache_shard */,
                         &socket_factory_,
                         &transport_socket_pool_,
                         NULL,
                         NULL,
                         session_deps_.ssl_config_service.get(),
                         NetLogWithSource().net_log()),
        field_trial_list_(nullptr),
        pool_(
            std::make_unique<HttpProxyClientSocketPool>(kMaxSockets,
                                                        kMaxSocketsPerGroup,
                                                        &transport_socket_pool_,
                                                        &ssl_socket_pool_,
                                                        &estimator_,
                                                        nullptr)) {
    session_ = CreateNetworkSession();
  }

  virtual ~HttpProxyClientSocketPoolTest() = default;

  // Initializes the field trial paramters for the field trial that determines
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
          base::IntToString(ssl_http_rtt_multiplier);
      params["non_ssl_http_rtt_multiplier"] =
          base::IntToString(non_ssl_http_rtt_multiplier);
      params["min_proxy_connection_timeout_seconds"] =
          base::IntToString(min_proxy_connection_timeout.InSeconds());
      params["max_proxy_connection_timeout_seconds"] =
          base::IntToString(max_proxy_connection_timeout.InSeconds());
    }
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    EXPECT_TRUE(
        base::AssociateFieldTrialParams(trial_name, group_name, params));
    EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(trial_name, group_name));

    // Reset |pool_| so that the field trial parameters are read by the
    // |pool_|.
    pool_ = std::make_unique<HttpProxyClientSocketPool>(
        kMaxSockets, kMaxSocketsPerGroup, &transport_socket_pool_,
        &ssl_socket_pool_, &estimator_, NetLogWithSource().net_log());
  }

  void AddAuthToCache() {
    const base::string16 kFoo(base::ASCIIToUTF16("foo"));
    const base::string16 kBar(base::ASCIIToUTF16("bar"));
    GURL proxy_url(GetParam() == HTTP
                       ? (std::string("http://") + kHttpProxyHost)
                       : (std::string("https://") + kHttpsProxyHost));
    session_->http_auth_cache()->Add(proxy_url,
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  scoped_refptr<TransportSocketParams> CreateHttpProxyParams() const {
    if (GetParam() != HTTP)
      return NULL;
    return new TransportSocketParams(
        HostPortPair(kHttpProxyHost, 80),
        false,
        OnHostResolutionCallback(),
        TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT);
  }

  scoped_refptr<SSLSocketParams> CreateHttpsProxyParams() const {
    if (GetParam() == HTTP)
      return NULL;
    return new SSLSocketParams(
        new TransportSocketParams(
            HostPortPair(kHttpsProxyHost, 443), false,
            OnHostResolutionCallback(),
            TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT),
        NULL, NULL, HostPortPair(kHttpsProxyHost, 443), SSLConfig(),
        PRIVACY_MODE_DISABLED, false /* ignore_certificate_errors */);
  }

  // Returns the a correctly constructed HttpProxyParms
  // for the HTTP or HTTPS proxy.
  scoped_refptr<HttpProxySocketParams> CreateParams(bool tunnel) {
    return base::MakeRefCounted<HttpProxySocketParams>(
        CreateHttpProxyParams(), CreateHttpsProxyParams(),
        quic::QUIC_VERSION_UNSUPPORTED, std::string(),
        HostPortPair("www.google.com", tunnel ? 443 : 80),
        session_->http_auth_cache(), session_->http_auth_handler_factory(),
        session_->spdy_session_pool(), session_->quic_stream_factory(),
        /*is_trusted_proxy=*/false, tunnel, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  scoped_refptr<HttpProxySocketParams> CreateTunnelParams() {
    return CreateParams(true);
  }

  scoped_refptr<HttpProxySocketParams> CreateNoTunnelParams() {
    return CreateParams(false);
  }

  MockTaggingClientSocketFactory* socket_factory() { return &socket_factory_; }

  void Initialize(base::span<const MockRead> reads,
                  base::span<const MockWrite> writes,
                  base::span<const MockRead> spdy_reads,
                  base::span<const MockWrite> spdy_writes) {
    if (GetParam() == SPDY) {
      data_.reset(new SequencedSocketData(spdy_reads, spdy_writes));
    } else {
      data_.reset(new SequencedSocketData(reads, writes));
    }

    data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));

    socket_factory()->AddSocketDataProvider(data_.get());

    if (GetParam() != HTTP) {
      ssl_data_.reset(new SSLSocketDataProvider(SYNCHRONOUS, OK));
      if (GetParam() == SPDY) {
        InitializeSpdySsl();
      }
      socket_factory()->AddSSLSocketDataProvider(ssl_data_.get());
    }
  }

  void InitializeSpdySsl() { ssl_data_->next_proto = kProtoHTTP2; }

  std::unique_ptr<HttpNetworkSession> CreateNetworkSession() {
    return SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  }

  RequestPriority GetLastTransportRequestPriority() const {
    return transport_socket_pool_.last_request_priority();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  TestNetworkQualityEstimator* estimator() { return &estimator_; }

  MockTransportClientSocketPool* transport_socket_pool() {
    return &transport_socket_pool_;
  }
  SSLClientSocketPool* ssl_socket_pool() { return &ssl_socket_pool_; }

 private:
  MockTaggingClientSocketFactory socket_factory_;
  SpdySessionDependencies session_deps_;

  TestNetworkQualityEstimator estimator_;

  MockTransportClientSocketPool transport_socket_pool_;
  MockHostResolver host_resolver_;
  std::unique_ptr<CertVerifier> cert_verifier_;
  SSLClientSocketPool ssl_socket_pool_;

  std::unique_ptr<HttpNetworkSession> session_;

  base::HistogramTester histogram_tester_;

  base::FieldTrialList field_trial_list_;

 protected:
  SpdyTestUtil spdy_util_;
  std::unique_ptr<SSLSocketDataProvider> ssl_data_;
  std::unique_ptr<SequencedSocketData> data_;
  std::unique_ptr<HttpProxyClientSocketPool> pool_;
  ClientSocketHandle handle_;
  TestCompletionCallback callback_;
};

// All tests are run with three different proxy types: HTTP, HTTPS (non-SPDY)
// and SPDY.
INSTANTIATE_TEST_CASE_P(HttpProxyType,
                        HttpProxyClientSocketPoolTest,
                        ::testing::Values(HTTP, HTTPS, SPDY));

TEST_P(HttpProxyClientSocketPoolTest, NoTunnel) {
  Initialize(base::span<MockRead>(), base::span<MockWrite>(),
             base::span<MockRead>(), base::span<MockWrite>());

  int rv =
      handle_.Init("a", CreateNoTunnelParams(), LOW, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   CompletionOnceCallback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  EXPECT_TRUE(handle_.socket()->IsConnected());

  bool is_secure_proxy = GetParam() == HTTPS || GetParam() == SPDY;
  histogram_tester().ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Insecure.Success", is_secure_proxy ? 0 : 1);
  histogram_tester().ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Secure.Success", is_secure_proxy ? 1 : 0);
}

// Make sure that HttpProxyConnectJob passes on its priority to its
// (non-SSL) socket request on Init.
TEST_P(HttpProxyClientSocketPoolTest, SetSocketRequestPriorityOnInit) {
  Initialize(base::span<MockRead>(), base::span<MockWrite>(),
             base::span<MockRead>(), base::span<MockWrite>());
  EXPECT_EQ(OK, handle_.Init("a", CreateNoTunnelParams(), HIGHEST, SocketTag(),
                             ClientSocketPool::RespectLimits::ENABLED,
                             CompletionOnceCallback(), pool_.get(),
                             NetLogWithSource()));
  EXPECT_EQ(HIGHEST, GetLastTransportRequestPriority());
}

TEST_P(HttpProxyClientSocketPoolTest, NeedAuth) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.google.com:443 HTTP/1.1\r\n"
                "Host: www.google.com:443\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(ASYNC, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead(ASYNC, 2, "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 4, "0123456789"),
  };
  spdy::SpdySerializedFrame req(spdy_util_.ConstructSpdyConnect(
      NULL, 0, 1, LOW, HostPortPair("www.google.com", 443)));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite spdy_writes[] = {
      CreateMockWrite(req, 0, ASYNC), CreateMockWrite(rst, 2, ASYNC),
  };
  spdy::SpdyHeaderBlock resp_block;
  resp_block[spdy::kHttp2StatusHeader] = "407";
  resp_block["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";

  spdy::SpdySerializedFrame resp(
      spdy_util_.ConstructSpdyReply(1, std::move(resp_block)));
  MockRead spdy_reads[] = {CreateMockRead(resp, 1, ASYNC),
                           MockRead(ASYNC, 0, 3)};

  Initialize(reads, writes, spdy_reads, spdy_writes);

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  rv = callback_.WaitForResult();
  EXPECT_THAT(rv, IsError(ERR_PROXY_AUTH_REQUESTED));
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  ProxyClientSocket* tunnel_socket =
      static_cast<ProxyClientSocket*>(handle_.socket());
  if (GetParam() == SPDY) {
    EXPECT_TRUE(tunnel_socket->IsConnected());
    EXPECT_TRUE(tunnel_socket->IsUsingSpdy());
  } else {
    EXPECT_FALSE(tunnel_socket->IsConnected());
    EXPECT_FALSE(tunnel_socket->IsUsingSpdy());
  }
}

TEST_P(HttpProxyClientSocketPoolTest, HaveAuth) {
  // It's pretty much impossible to make the SPDY case behave synchronously
  // so we skip this test for SPDY
  if (GetParam() == SPDY)
    return;
  std::string proxy_host_port = GetParam() == HTTP
                                    ? (kHttpProxyHost + std::string(":80"))
                                    : (kHttpsProxyHost + std::string(":443"));
  std::string request =
      "CONNECT www.google.com:443 HTTP/1.1\r\n"
      "Host: www.google.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n";
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, request.c_str()),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  Initialize(reads, writes, base::span<MockRead>(), base::span<MockWrite>());
  AddAuthToCache();

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  EXPECT_TRUE(handle_.socket()->IsConnected());
}

TEST_P(HttpProxyClientSocketPoolTest, AsyncHaveAuth) {
  std::string proxy_host_port = GetParam() == HTTP
                                    ? (kHttpProxyHost + std::string(":80"))
                                    : (kHttpsProxyHost + std::string(":443"));
  std::string request =
      "CONNECT www.google.com:443 HTTP/1.1\r\n"
      "Host: www.google.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n";
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, request.c_str()),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, LOW,
                                      HostPortPair("www.google.com", 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0, ASYNC)};
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyGetReply(NULL, 0, 1));
  MockRead spdy_reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      // Connection stays open.
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes, spdy_reads, spdy_writes);
  AddAuthToCache();

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  EXPECT_TRUE(handle_.socket()->IsConnected());
}

// Make sure that HttpProxyConnectJob passes on its priority to its
// SPDY session's socket request on Init (if applicable).
TEST_P(HttpProxyClientSocketPoolTest,
       SetSpdySessionSocketRequestPriorityOnInit) {
  if (GetParam() != SPDY)
    return;

  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, MEDIUM,
                                      HostPortPair("www.google.com", 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0, ASYNC)};
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyGetReply(NULL, 0, 1));
  MockRead spdy_reads[] = {CreateMockRead(resp, 1, ASYNC),
                           MockRead(ASYNC, 0, 2)};

  Initialize(base::span<MockRead>(), base::span<MockWrite>(), spdy_reads,
             spdy_writes);
  AddAuthToCache();

  EXPECT_EQ(
      ERR_IO_PENDING,
      handle_.Init("a", CreateTunnelParams(), MEDIUM, SocketTag(),
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback_.callback(), pool_.get(), NetLogWithSource()));
  EXPECT_EQ(MEDIUM, GetLastTransportRequestPriority());

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
}

TEST_P(HttpProxyClientSocketPoolTest, TCPError) {
  if (GetParam() == SPDY)
    return;
  data_.reset(new SequencedSocketData());
  data_->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_CLOSED));

  socket_factory()->AddSocketDataProvider(data_.get());

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_PROXY_CONNECTION_FAILED));

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  bool is_secure_proxy = GetParam() == HTTPS;
  histogram_tester().ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Insecure.Error", is_secure_proxy ? 0 : 1);
  histogram_tester().ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Secure.Error", is_secure_proxy ? 1 : 0);
}

TEST_P(HttpProxyClientSocketPoolTest, SSLError) {
  if (GetParam() == HTTP)
    return;
  data_.reset(new SequencedSocketData());
  data_->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data_.get());

  ssl_data_.reset(new SSLSocketDataProvider(ASYNC,
                                            ERR_CERT_AUTHORITY_INVALID));
  if (GetParam() == SPDY) {
    InitializeSpdySsl();
  }
  socket_factory()->AddSSLSocketDataProvider(ssl_data_.get());

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_THAT(callback_.WaitForResult(),
              IsError(ERR_PROXY_CERTIFICATE_INVALID));

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
  histogram_tester().ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Secure.Error", 1);
  histogram_tester().ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Insecure.Error", 0);
}

TEST_P(HttpProxyClientSocketPoolTest, SslClientAuth) {
  if (GetParam() == HTTP)
    return;
  data_.reset(new SequencedSocketData());
  data_->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data_.get());

  ssl_data_.reset(new SSLSocketDataProvider(ASYNC,
                                            ERR_SSL_CLIENT_AUTH_CERT_NEEDED));
  if (GetParam() == SPDY) {
    InitializeSpdySsl();
  }
  socket_factory()->AddSSLSocketDataProvider(ssl_data_.get());

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_THAT(callback_.WaitForResult(),
              IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED));

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
  histogram_tester().ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Secure.Error", 1);
  histogram_tester().ExpectTotalCount(
      "Net.HttpProxy.ConnectLatency.Insecure.Error", 0);
}

TEST_P(HttpProxyClientSocketPoolTest, TunnelUnexpectedClose) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.google.com:443 HTTP/1.1\r\n"
                "Host: www.google.com:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 Conn"),
    MockRead(ASYNC, ERR_CONNECTION_CLOSED, 2),
  };
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, LOW,
                                      HostPortPair("www.google.com", 443)));
  MockWrite spdy_writes[] = {CreateMockWrite(req, 0, ASYNC)};
  MockRead spdy_reads[] = {
    MockRead(ASYNC, ERR_CONNECTION_CLOSED, 1),
  };

  Initialize(reads, writes, spdy_reads, spdy_writes);
  AddAuthToCache();

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  if (GetParam() == SPDY) {
    // SPDY cannot process a headers block unless it's complete and so it
    // returns ERR_CONNECTION_CLOSED in this case.
    EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
  } else {
    EXPECT_THAT(callback_.WaitForResult(),
                IsError(ERR_RESPONSE_HEADERS_TRUNCATED));
  }
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, Tunnel1xxResponse) {
  // Tests that 1xx responses are rejected for a CONNECT request.
  if (GetParam() == SPDY) {
    // SPDY doesn't have 1xx responses.
    return;
  }

  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.google.com:443 HTTP/1.1\r\n"
                "Host: www.google.com:443\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 100 Continue\r\n\r\n"),
    MockRead(ASYNC, 2, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  Initialize(reads, writes, base::span<MockRead>(), base::span<MockWrite>());

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_TUNNEL_CONNECTION_FAILED));
}

TEST_P(HttpProxyClientSocketPoolTest, TunnelSetupError) {
  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.google.com:443 HTTP/1.1\r\n"
                "Host: www.google.com:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 304 Not Modified\r\n\r\n"),
  };
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, LOW,
                                      HostPortPair("www.google.com", 443)));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite spdy_writes[] = {
      CreateMockWrite(req, 0, ASYNC), CreateMockWrite(rst, 2, ASYNC),
  };
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyReplyError(1));
  MockRead spdy_reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, 0, 3),
  };

  Initialize(reads, writes, spdy_reads, spdy_writes);
  AddAuthToCache();

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  rv = callback_.WaitForResult();
  // All Proxy CONNECT responses are not trustworthy
  EXPECT_THAT(rv, IsError(ERR_TUNNEL_CONNECTION_FAILED));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, TunnelSetupRedirect) {
  const std::string redirectTarget = "https://foo.google.com/";

  const std::string responseText = "HTTP/1.1 302 Found\r\n"
                                   "Location: " + redirectTarget + "\r\n"
                                   "Set-Cookie: foo=bar\r\n"
                                   "\r\n";
  MockWrite writes[] = {
      MockWrite(ASYNC, 0,
                "CONNECT www.google.com:443 HTTP/1.1\r\n"
                "Host: www.google.com:443\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, responseText.c_str()),
  };
  spdy::SpdySerializedFrame req(
      spdy_util_.ConstructSpdyConnect(kAuthHeaders, kAuthHeadersSize, 1, LOW,
                                      HostPortPair("www.google.com", 443)));
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));

  MockWrite spdy_writes[] = {
      CreateMockWrite(req, 0, ASYNC), CreateMockWrite(rst, 3, ASYNC),
  };

  const char* const responseHeaders[] = {
    "location", redirectTarget.c_str(),
    "set-cookie", "foo=bar",
  };
  const int responseHeadersSize = arraysize(responseHeaders) / 2;
  spdy::SpdySerializedFrame resp(spdy_util_.ConstructSpdyReplyError(
      "302", responseHeaders, responseHeadersSize, 1));
  MockRead spdy_reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, 0, 2),
  };

  Initialize(reads, writes, spdy_reads, spdy_writes);
  AddAuthToCache();

  int rv = handle_.Init("a", CreateTunnelParams(), LOW, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback_.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  rv = callback_.WaitForResult();

  if (GetParam() == HTTP) {
    // We don't trust 302 responses to CONNECT from HTTP proxies.
    EXPECT_THAT(rv, IsError(ERR_TUNNEL_CONNECTION_FAILED));
    EXPECT_FALSE(handle_.is_initialized());
    EXPECT_FALSE(handle_.socket());
  } else {
    // Expect ProxyClientSocket to return the proxy's response, sanitized.
    EXPECT_THAT(rv, IsError(ERR_HTTPS_PROXY_TUNNEL_RESPONSE));
    EXPECT_TRUE(handle_.is_initialized());
    ASSERT_TRUE(handle_.socket());

    const ProxyClientSocket* tunnel_socket =
        static_cast<ProxyClientSocket*>(handle_.socket());
    const HttpResponseInfo* response = tunnel_socket->GetConnectResponseInfo();
    const HttpResponseHeaders* headers = response->headers.get();

    // Make sure Set-Cookie header was stripped.
    EXPECT_FALSE(headers->HasHeader("set-cookie"));

    // Make sure Content-Length: 0 header was added.
    EXPECT_TRUE(headers->HasHeaderValue("content-length", "0"));

    // Make sure Location header was included and correct.
    std::string location;
    EXPECT_TRUE(headers->IsRedirect(&location));
    EXPECT_EQ(location, redirectTarget);
  }
}

TEST_P(HttpProxyClientSocketPoolTest, ProxyPoolMinTimeout) {
  // Set RTT estimate to a low value.
  base::TimeDelta rtt_estimate = base::TimeDelta::FromMilliseconds(1);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);

  EXPECT_LE(base::TimeDelta(), pool_->ConnectionTimeout());

  // Test against a large value.
  EXPECT_GE(base::TimeDelta::FromMinutes(10), pool_->ConnectionTimeout());

#if (defined(OS_ANDROID) || defined(OS_IOS))
  EXPECT_EQ(base::TimeDelta::FromSeconds(8), pool_->ConnectionTimeout());
#else
  EXPECT_EQ(base::TimeDelta::FromSeconds(30), pool_->ConnectionTimeout());
#endif
}

TEST_P(HttpProxyClientSocketPoolTest, ProxyPoolMaxTimeout) {
  // Set RTT estimate to a high value.
  base::TimeDelta rtt_estimate = base::TimeDelta::FromSeconds(100);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);

  EXPECT_LE(base::TimeDelta(), pool_->ConnectionTimeout());

  // Test against a large value.
  EXPECT_GE(base::TimeDelta::FromMinutes(10), pool_->ConnectionTimeout());

#if (defined(OS_ANDROID) || defined(OS_IOS))
  EXPECT_EQ(base::TimeDelta::FromSeconds(30), pool_->ConnectionTimeout());
#else
  EXPECT_EQ(base::TimeDelta::FromSeconds(60), pool_->ConnectionTimeout());
#endif
}

// Tests the connection timeout values when the field trial parameters are
// specified.
TEST_P(HttpProxyClientSocketPoolTest, ProxyPoolTimeoutWithExperiment) {
  // Timeout should be kMultiplier times the HTTP RTT estimate.
  const int kMultiplier = 4;
  const base::TimeDelta kMinTimeout = base::TimeDelta::FromSeconds(8);
  const base::TimeDelta kMaxTimeout = base::TimeDelta::FromSeconds(20);

  InitAdaptiveTimeoutFieldTrialWithParams(false, kMultiplier, kMultiplier,
                                          kMinTimeout, kMaxTimeout);
  EXPECT_LE(base::TimeDelta(), pool_->ConnectionTimeout());

  base::TimeDelta rtt_estimate = base::TimeDelta::FromSeconds(4);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  base::TimeDelta expected_connection_timeout = kMultiplier * rtt_estimate;
  EXPECT_EQ(expected_connection_timeout, pool_->ConnectionTimeout());

  // Connection timeout should not exceed kMaxTimeout.
  rtt_estimate = base::TimeDelta::FromSeconds(25);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMaxTimeout, pool_->ConnectionTimeout());

  // Connection timeout should not be less than kMinTimeout.
  rtt_estimate = base::TimeDelta::FromSeconds(0);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMinTimeout, pool_->ConnectionTimeout());
}

// Tests the connection timeout values when the field trial parameters are
// specified.
TEST_P(HttpProxyClientSocketPoolTest,
       ProxyPoolTimeoutWithExperimentDifferentParams) {
  // Timeout should be kMultiplier times the HTTP RTT estimate.
  const int kMultiplier = 3;
  const base::TimeDelta kMinTimeout = base::TimeDelta::FromSeconds(2);
  const base::TimeDelta kMaxTimeout = base::TimeDelta::FromSeconds(30);

  InitAdaptiveTimeoutFieldTrialWithParams(false, kMultiplier, kMultiplier,
                                          kMinTimeout, kMaxTimeout);
  EXPECT_LE(base::TimeDelta(), pool_->ConnectionTimeout());

  base::TimeDelta rtt_estimate = base::TimeDelta::FromSeconds(2);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMultiplier * rtt_estimate, pool_->ConnectionTimeout());

  // A change in RTT estimate should also change the connection timeout.
  rtt_estimate = base::TimeDelta::FromSeconds(7);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMultiplier * rtt_estimate, pool_->ConnectionTimeout());

  // Connection timeout should not exceed kMaxTimeout.
  rtt_estimate = base::TimeDelta::FromSeconds(35);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMaxTimeout, pool_->ConnectionTimeout());

  // Connection timeout should not be less than kMinTimeout.
  rtt_estimate = base::TimeDelta::FromSeconds(0);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_EQ(kMinTimeout, pool_->ConnectionTimeout());
}

TEST_P(HttpProxyClientSocketPoolTest, ProxyPoolTimeoutWithConnectionProperty) {
  const int kSecureMultiplier = 3;
  const int kNonSecureMultiplier = 5;
  const base::TimeDelta kMinTimeout = base::TimeDelta::FromSeconds(2);
  const base::TimeDelta kMaxTimeout = base::TimeDelta::FromSeconds(30);

  InitAdaptiveTimeoutFieldTrialWithParams(
      false, kSecureMultiplier, kNonSecureMultiplier, kMinTimeout, kMaxTimeout);

  HttpProxyClientSocketPool::HttpProxyConnectJobFactory job_factory(
      transport_socket_pool(), ssl_socket_pool(), estimator(), nullptr);

  const base::TimeDelta kRttEstimate = base::TimeDelta::FromSeconds(2);
  estimator()->SetStartTimeNullHttpRtt(kRttEstimate);
  // By default, connection timeout should return the timeout for secure
  // proxies.
  EXPECT_EQ(kSecureMultiplier * kRttEstimate, job_factory.ConnectionTimeout());
  EXPECT_EQ(kSecureMultiplier * kRttEstimate,
            job_factory.ConnectionTimeoutWithConnectionProperty(true));
  EXPECT_EQ(kNonSecureMultiplier * kRttEstimate,
            job_factory.ConnectionTimeoutWithConnectionProperty(false));
}

// Tests the connection timeout values when the field trial parameters are not
// specified.
TEST_P(HttpProxyClientSocketPoolTest,
       ProxyPoolTimeoutWithExperimentDefaultParams) {
  InitAdaptiveTimeoutFieldTrialWithParams(true, 0, 0, base::TimeDelta(),
                                          base::TimeDelta());
  EXPECT_LE(base::TimeDelta(), pool_->ConnectionTimeout());

  // Timeout should be |http_rtt_multiplier| times the HTTP RTT
  // estimate.
  base::TimeDelta rtt_estimate = base::TimeDelta::FromMilliseconds(10);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  // Connection timeout should not be less than the HTTP RTT estimate.
  EXPECT_LE(rtt_estimate, pool_->ConnectionTimeout());

  // A change in RTT estimate should also change the connection timeout.
  rtt_estimate = base::TimeDelta::FromSeconds(10);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  // Connection timeout should not be less than the HTTP RTT estimate.
  EXPECT_LE(rtt_estimate, pool_->ConnectionTimeout());

  // Set RTT to a very large value.
  rtt_estimate = base::TimeDelta::FromMinutes(60);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_GT(rtt_estimate, pool_->ConnectionTimeout());

  // Set RTT to a very small value.
  rtt_estimate = base::TimeDelta::FromSeconds(0);
  estimator()->SetStartTimeNullHttpRtt(rtt_estimate);
  EXPECT_LT(rtt_estimate, pool_->ConnectionTimeout());
}

// It would be nice to also test the timeouts in HttpProxyClientSocketPool.

// Test that SocketTag passed into HttpProxyClientSocketPool is applied to
// returned underlying TCP sockets.
#if defined(OS_ANDROID)
TEST_P(HttpProxyClientSocketPoolTest, Tag) {
  Initialize(base::span<MockRead>(), base::span<MockWrite>(),
             base::span<MockRead>(), base::span<MockWrite>());
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);

  // Verify requested socket is tagged properly.
  int rv =
      handle_.Init("a", CreateNoTunnelParams(), LOW, tag1,
                   ClientSocketPool::RespectLimits::ENABLED,
                   CompletionOnceCallback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  EXPECT_TRUE(handle_.socket()->IsConnected());
  EXPECT_EQ(socket_factory()->GetLastProducedTCPSocket()->tag(), tag1);
  EXPECT_TRUE(
      socket_factory()->GetLastProducedTCPSocket()->tagged_before_connected());

  // Verify reused socket is retagged properly.
  StreamSocket* socket = handle_.socket();
  handle_.Reset();
  rv = handle_.Init("a", CreateNoTunnelParams(), LOW, tag2,
                    ClientSocketPool::RespectLimits::ENABLED,
                    CompletionOnceCallback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle_.socket());
  EXPECT_TRUE(handle_.socket()->IsConnected());
  EXPECT_EQ(handle_.socket(), socket);
  EXPECT_EQ(socket_factory()->GetLastProducedTCPSocket()->tag(), tag2);
  handle_.socket()->Disconnect();
  handle_.Reset();
}
#endif

}  // namespace net
