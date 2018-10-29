// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;
const char kGroupName[] = "a";

// Make sure |handle|'s load times are set correctly.  DNS and connect start
// times comes from mock client sockets in these tests, so primarily serves to
// check those times were copied, and ssl times / connect end are set correctly.
void TestLoadTimingInfo(const ClientSocketHandle& handle) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(handle.GetLoadTimingInfo(false, &load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  // None of these tests use a NetLog.
  EXPECT_EQ(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(
      load_timing_info.connect_timing,
      CONNECT_TIMING_HAS_SSL_TIMES | CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

// Just like TestLoadTimingInfo, except DNS times are expected to be null, for
// tests over proxies that do DNS lookups themselves.
void TestLoadTimingInfoNoDns(const ClientSocketHandle& handle) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(handle.GetLoadTimingInfo(false, &load_timing_info));

  // None of these tests use a NetLog.
  EXPECT_EQ(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_FALSE(load_timing_info.socket_reused);

  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              CONNECT_TIMING_HAS_SSL_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

class SSLClientSocketPoolTest : public TestWithScopedTaskEnvironment {
 protected:
  SSLClientSocketPoolTest()
      : cert_verifier_(new MockCertVerifier),
        transport_security_state_(new TransportSecurityState),
        proxy_resolution_service_(ProxyResolutionService::CreateDirect()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        http_auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        http_server_properties_(new HttpServerPropertiesImpl),
        session_(CreateNetworkSession()),
        direct_transport_socket_params_(new TransportSocketParams(
            HostPortPair("host", 443),
            false,
            OnHostResolutionCallback(),
            TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT)),
        transport_socket_pool_(kMaxSockets,
                               kMaxSocketsPerGroup,
                               &socket_factory_),
        proxy_transport_socket_params_(new TransportSocketParams(
            HostPortPair("proxy", 443),
            false,
            OnHostResolutionCallback(),
            TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT)),
        socks_socket_params_(
            new SOCKSSocketParams(proxy_transport_socket_params_,
                                  true,
                                  HostPortPair("sockshost", 443),
                                  TRAFFIC_ANNOTATION_FOR_TESTS)),
        socks_socket_pool_(kMaxSockets,
                           kMaxSocketsPerGroup,
                           &transport_socket_pool_),
        http_proxy_socket_params_(
            new HttpProxySocketParams(proxy_transport_socket_params_,
                                      NULL,
                                      quic::QUIC_VERSION_UNSUPPORTED,
                                      std::string(),
                                      HostPortPair("host", 80),
                                      session_->http_auth_cache(),
                                      session_->http_auth_handler_factory(),
                                      session_->spdy_session_pool(),
                                      session_->quic_stream_factory(),
                                      /*is_trusted_proxy=*/false,
                                      /*tunnel=*/true,
                                      TRAFFIC_ANNOTATION_FOR_TESTS)),
        http_proxy_socket_pool_(kMaxSockets,
                                kMaxSocketsPerGroup,
                                &transport_socket_pool_,
                                NULL,
                                NULL,
                                NULL) {
    ssl_config_service_->GetSSLConfig(&ssl_config_);
  }

  void CreatePool(bool transport_pool, bool http_proxy_pool, bool socks_pool) {
    pool_.reset(new SSLClientSocketPool(
        kMaxSockets, kMaxSocketsPerGroup, cert_verifier_.get(),
        NULL /* channel_id_service */, transport_security_state_.get(),
        &ct_verifier_, &ct_policy_enforcer_,
        std::string() /* ssl_session_cache_shard */, &socket_factory_,
        transport_pool ? &transport_socket_pool_ : NULL,
        socks_pool ? &socks_socket_pool_ : NULL,
        http_proxy_pool ? &http_proxy_socket_pool_ : NULL, NULL, NULL));
  }

  scoped_refptr<SSLSocketParams> SSLParams(ProxyServer::Scheme proxy) {
    return base::MakeRefCounted<SSLSocketParams>(
        proxy == ProxyServer::SCHEME_DIRECT ? direct_transport_socket_params_
                                            : NULL,
        proxy == ProxyServer::SCHEME_SOCKS5 ? socks_socket_params_ : NULL,
        proxy == ProxyServer::SCHEME_HTTP ? http_proxy_socket_params_ : NULL,
        HostPortPair("host", 443), ssl_config_, PRIVACY_MODE_DISABLED, 0);
  }

  void AddAuthToCache() {
    const base::string16 kFoo(base::ASCIIToUTF16("foo"));
    const base::string16 kBar(base::ASCIIToUTF16("bar"));
    session_->http_auth_cache()->Add(GURL("http://proxy:443/"),
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  HttpNetworkSession* CreateNetworkSession() {
    HttpNetworkSession::Context session_context;
    session_context.host_resolver = &host_resolver_;
    session_context.cert_verifier = cert_verifier_.get();
    session_context.transport_security_state = transport_security_state_.get();
    session_context.cert_transparency_verifier = &ct_verifier_;
    session_context.ct_policy_enforcer = &ct_policy_enforcer_;
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.client_socket_factory = &socket_factory_;
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.http_auth_handler_factory =
        http_auth_handler_factory_.get();
    session_context.http_server_properties = http_server_properties_.get();
    return new HttpNetworkSession(HttpNetworkSession::Params(),
                                  session_context);
  }

  void TestIPPoolingDisabled(SSLSocketDataProvider* ssl);

  MockClientSocketFactory socket_factory_;
  MockCachingHostResolver host_resolver_;
  std::unique_ptr<MockCertVerifier> cert_verifier_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  MultiLogCTVerifier ct_verifier_;
  DefaultCTPolicyEnforcer ct_policy_enforcer_;
  const std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  const std::unique_ptr<SSLConfigService> ssl_config_service_;
  const std::unique_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  const std::unique_ptr<HttpServerPropertiesImpl> http_server_properties_;
  const std::unique_ptr<HttpNetworkSession> session_;

  scoped_refptr<TransportSocketParams> direct_transport_socket_params_;
  MockTransportClientSocketPool transport_socket_pool_;

  scoped_refptr<TransportSocketParams> proxy_transport_socket_params_;

  scoped_refptr<SOCKSSocketParams> socks_socket_params_;
  MockSOCKSClientSocketPool socks_socket_pool_;

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_;
  HttpProxyClientSocketPool http_proxy_socket_pool_;

  SSLConfig ssl_config_;
  std::unique_ptr<SSLClientSocketPool> pool_;
};

TEST_F(SSLClientSocketPoolTest, TCPFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  int rv =
      handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                  ClientSocketPool::RespectLimits::ENABLED,
                  CompletionOnceCallback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_FAILED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  ASSERT_EQ(1u, handle.connection_attempts().size());
  EXPECT_THAT(handle.connection_attempts()[0].result,
              IsError(ERR_CONNECTION_FAILED));
}

TEST_F(SSLClientSocketPoolTest, TCPFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_FAILED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  ASSERT_EQ(1u, handle.connection_attempts().size());
  EXPECT_THAT(handle.connection_attempts()[0].result,
              IsError(ERR_CONNECTION_FAILED));
}

TEST_F(SSLClientSocketPoolTest, BasicDirect) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);
  EXPECT_EQ(0u, handle.connection_attempts().size());
}

// Make sure that SSLConnectJob passes on its priority to its
// socket request on Init (for the DIRECT case).
TEST_F(SSLClientSocketPoolTest, SetSocketRequestPriorityOnInitDirect) {
  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    RequestPriority priority = static_cast<RequestPriority>(i);
    StaticSocketDataProvider data;
    data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
    socket_factory_.AddSocketDataProvider(&data);
    SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
    socket_factory_.AddSSLSocketDataProvider(&ssl);

    ClientSocketHandle handle;
    TestCompletionCallback callback;
    EXPECT_EQ(
        OK, handle.Init(kGroupName, params, priority, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback.callback(), pool_.get(), NetLogWithSource()));
    EXPECT_EQ(priority, transport_socket_pool_.last_request_priority());
    handle.socket()->Disconnect();
  }
}

TEST_F(SSLClientSocketPoolTest, BasicDirectAsync) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);
}

TEST_F(SSLClientSocketPoolTest, DirectCertError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_COMMON_NAME_INVALID);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CERT_COMMON_NAME_INVALID));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);
}

TEST_F(SSLClientSocketPoolTest, DirectSSLError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_PROTOCOL_ERROR);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_SSL_PROTOCOL_ERROR));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_TRUE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, DirectWithNPN) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = kProtoHTTP11;
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);
  EXPECT_TRUE(handle.socket()->WasAlpnNegotiated());
}

TEST_F(SSLClientSocketPoolTest, DirectGotSPDY) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = kProtoHTTP2;
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfo(handle);

  EXPECT_TRUE(handle.socket()->WasAlpnNegotiated());
  EXPECT_EQ(kProtoHTTP2, handle.socket()->GetNegotiatedProtocol());
}

TEST_F(SSLClientSocketPoolTest, SOCKSFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_FAILED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, SOCKSFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_FAILED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, SOCKSBasic) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  // SOCKS5 generally has no DNS times, but the mock SOCKS5 sockets used here
  // don't go through the real logic, unlike in the HTTP proxy tests.
  TestLoadTimingInfo(handle);
}

// Make sure that SSLConnectJob passes on its priority to its
// transport socket on Init (for the SOCKS_PROXY case).
TEST_F(SSLClientSocketPoolTest, SetTransportPriorityOnInitSOCKS) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_EQ(OK,
            handle.Init(kGroupName, params, HIGHEST, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback.callback(), pool_.get(), NetLogWithSource()));
  EXPECT_EQ(HIGHEST, transport_socket_pool_.last_request_priority());
}

TEST_F(SSLClientSocketPoolTest, SOCKSBasicAsync) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  // SOCKS5 generally has no DNS times, but the mock SOCKS5 sockets used here
  // don't go through the real logic, unlike in the HTTP proxy tests.
  TestLoadTimingInfo(handle);
}

TEST_F(SSLClientSocketPoolTest, HttpProxyFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_PROXY_CONNECTION_FAILED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, HttpProxyFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_PROXY_CONNECTION_FAILED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, HttpProxyBasic) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, writes);
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoNoDns(handle);
}

// Make sure that SSLConnectJob passes on its priority to its
// transport socket on Init (for the HTTP_PROXY case).
TEST_F(SSLClientSocketPoolTest, SetTransportPriorityOnInitHTTP) {
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host:80\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, writes);
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_EQ(OK,
            handle.Init(kGroupName, params, HIGHEST, SocketTag(),
                        ClientSocketPool::RespectLimits::ENABLED,
                        callback.callback(), pool_.get(), NetLogWithSource()));
  EXPECT_EQ(HIGHEST, transport_socket_pool_.last_request_priority());
}

TEST_F(SSLClientSocketPoolTest, HttpProxyBasicAsync) {
  MockWrite writes[] = {
      MockWrite(
          "CONNECT host:80 HTTP/1.1\r\n"
          "Host: host:80\r\n"
          "Proxy-Connection: keep-alive\r\n"
          "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, writes);
  socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  TestLoadTimingInfoNoDns(handle);
}

TEST_F(SSLClientSocketPoolTest, NeedProxyAuth) {
  MockWrite writes[] = {
      MockWrite(
          "CONNECT host:80 HTTP/1.1\r\n"
          "Host: host:80\r\n"
          "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10\r\n\r\n"),
      MockRead("0123456789"),
  };
  StaticSocketDataProvider data(reads, writes);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, MEDIUM, SocketTag(),
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), pool_.get(), NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_PROXY_AUTH_REQUESTED));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  const HttpResponseInfo& tunnel_info = handle.ssl_error_response_info();
  EXPECT_EQ(tunnel_info.headers->response_code(), 407);
  std::unique_ptr<ClientSocketHandle> tunnel_handle =
      handle.release_pending_http_proxy_connection();
  EXPECT_TRUE(tunnel_handle->socket());
  EXPECT_FALSE(tunnel_handle->socket()->IsConnected());
}

TEST_F(SSLClientSocketPoolTest, IPPooling) {
  const int kTestPort = 80;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
    { "www.webkit.org",    "192.0.2.33,192.168.0.1,192.168.0.5" },
    { "code.google.com",   "192.168.0.2,192.168.0.3,192.168.0.5" },
    { "js.webkit.org",     "192.168.0.4,192.168.0.1,192.0.2.33" },
  };

  host_resolver_.set_synchronous_mode(true);
  for (size_t i = 0; i < arraysize(test_hosts); i++) {
    host_resolver_.rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    // This test requires that the HostResolver cache be populated.  Normal
    // code would have done this already, but we do it manually.
    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    std::unique_ptr<HostResolver::Request> request;
    int rv = host_resolver_.Resolve(
        info, DEFAULT_PRIORITY, &test_hosts[i].addresses,
        CompletionOnceCallback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());

    // Setup a SpdySessionKey
    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED, SocketTag());
  }

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.ssl_info.cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der));
  ASSERT_TRUE(ssl.ssl_info.cert);
  ssl.next_proto = kProtoHTTP2;
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  base::WeakPtr<SpdySession> spdy_session =
      CreateSpdySession(session_.get(), test_hosts[0].key, NetLogWithSource());

  EXPECT_TRUE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[0].key));
  EXPECT_FALSE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[1].key));
  EXPECT_TRUE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[2].key));

  session_->spdy_session_pool()->CloseAllSessions();
}

void SSLClientSocketPoolTest::TestIPPoolingDisabled(
    SSLSocketDataProvider* ssl) {
  const int kTestPort = 80;
  struct TestHosts {
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
    { "www.webkit.org",    "192.0.2.33,192.168.0.1,192.168.0.5" },
    { "js.webkit.com",     "192.168.0.4,192.168.0.1,192.0.2.33" },
  };

  host_resolver_.set_synchronous_mode(true);
  for (size_t i = 0; i < arraysize(test_hosts); i++) {
    host_resolver_.rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    // This test requires that the HostResolver cache be populated.  Normal
    // code would have done this already, but we do it manually.
    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    std::unique_ptr<HostResolver::Request> request;
    int rv = host_resolver_.Resolve(
        info, DEFAULT_PRIORITY, &test_hosts[i].addresses,
        CompletionOnceCallback(), &request, NetLogWithSource());
    EXPECT_THAT(rv, IsOk());

    // Setup a SpdySessionKey
    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED, SocketTag());
  }

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING),
  };
  StaticSocketDataProvider data(reads, base::span<MockWrite>());
  socket_factory_.AddSocketDataProvider(&data);
  socket_factory_.AddSSLSocketDataProvider(ssl);

  CreatePool(true /* tcp pool */, false, false);
  base::WeakPtr<SpdySession> spdy_session =
      CreateSpdySession(session_.get(), test_hosts[0].key, NetLogWithSource());

  EXPECT_TRUE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[0].key));
  EXPECT_FALSE(
      HasSpdySession(session_->spdy_session_pool(), test_hosts[1].key));

  session_->spdy_session_pool()->CloseAllSessions();
}

// Verifies that an SSL connection with client authentication disables SPDY IP
// pooling.
TEST_F(SSLClientSocketPoolTest, IPPoolingClientCert) {
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.ssl_info.cert = X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der));
  ASSERT_TRUE(ssl.ssl_info.cert);
  ssl.ssl_info.client_cert_sent = true;
  ssl.next_proto = kProtoHTTP2;
  TestIPPoolingDisabled(&ssl);
}

// Verifies that an SSL connection with channel ID disables SPDY IP pooling.
TEST_F(SSLClientSocketPoolTest, IPPoolingChannelID) {
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.ssl_info.channel_id_sent = true;
  ssl.next_proto = kProtoHTTP2;
  TestIPPoolingDisabled(&ssl);
}

// It would be nice to also test the timeouts in SSLClientSocketPool.

// Test that SocketTag passed into SSLClientSocketPool is applied to returned
// sockets.
#if defined(OS_ANDROID)
TEST_F(SSLClientSocketPoolTest, Tag) {
  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  TransportClientSocketPool tcp_pool(
      kMaxSockets, kMaxSocketsPerGroup, &host_resolver_,
      ClientSocketFactory::GetDefaultFactory(), NULL, NULL);
  cert_verifier_->set_default_result(OK);
  SSLClientSocketPool pool(kMaxSockets, kMaxSocketsPerGroup,
                           cert_verifier_.get(), NULL /* channel_id_service */,
                           transport_security_state_.get(), &ct_verifier_,
                           &ct_policy_enforcer_,
                           std::string() /* ssl_session_cache_shard */,
                           ClientSocketFactory::GetDefaultFactory(), &tcp_pool,
                           NULL, NULL, NULL, NULL);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  scoped_refptr<TransportSocketParams> tcp_params(new TransportSocketParams(
      test_server.host_port_pair(), false, OnHostResolutionCallback(),
      TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT));
  scoped_refptr<SSLSocketParams> params(new SSLSocketParams(
      tcp_params, NULL, NULL, test_server.host_port_pair(), ssl_config_,
      PRIVACY_MODE_DISABLED, false /* ignore_certificate_errors */));

  // Test socket is tagged before connected.
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  int rv = handle.Init(kGroupName, params, LOW, tag1,
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Test reused socket is retagged.
  StreamSocket* socket = handle.socket();
  handle.Reset();
  old_traffic = GetTaggedBytes(tag_val2);
  TestCompletionCallback callback2;
  rv = handle.Init(kGroupName, params, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  EXPECT_EQ(handle.socket(), socket);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  rv = handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                             callback.callback());
  EXPECT_EQ(read_buffer->size(), callback.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
  // Disconnect socket to prevent reuse.
  handle.socket()->Disconnect();
  handle.Reset();
}

TEST_F(SSLClientSocketPoolTest, TagTwoSockets) {
  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  TransportClientSocketPool tcp_pool(
      kMaxSockets, kMaxSocketsPerGroup, &host_resolver_,
      ClientSocketFactory::GetDefaultFactory(), NULL, NULL);
  cert_verifier_->set_default_result(OK);
  SSLClientSocketPool pool(kMaxSockets, kMaxSocketsPerGroup,
                           cert_verifier_.get(), NULL /* channel_id_service */,
                           transport_security_state_.get(), &ct_verifier_,
                           &ct_policy_enforcer_,
                           std::string() /* ssl_session_cache_shard */,
                           ClientSocketFactory::GetDefaultFactory(), &tcp_pool,
                           NULL, NULL, NULL, NULL);
  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  scoped_refptr<TransportSocketParams> tcp_params(new TransportSocketParams(
      test_server.host_port_pair(), false, OnHostResolutionCallback(),
      TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT));
  scoped_refptr<SSLSocketParams> params(new SSLSocketParams(
      tcp_params, NULL, NULL, test_server.host_port_pair(), ssl_config_,
      PRIVACY_MODE_DISABLED, false /* ignore_certificate_errors */));

  // Test connect jobs that are orphaned and then adopted, appropriately apply
  // new tag. Request socket with |tag1|.
  TestCompletionCallback callback;
  int rv = handle.Init(kGroupName, params, LOW, tag1,
                       ClientSocketPool::RespectLimits::ENABLED,
                       callback.callback(), &pool, NetLogWithSource());
  EXPECT_TRUE(rv == OK || rv == ERR_IO_PENDING) << "Result: " << rv;
  // Abort and request socket with |tag2|.
  handle.Reset();
  TestCompletionCallback callback2;
  rv = handle.Init(kGroupName, params, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback2.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(callback2.GetResult(rv), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  // Verify socket has |tag2| applied.
  uint64_t old_traffic = GetTaggedBytes(tag_val2);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv = handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                              callback2.callback(),
                              TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback2.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  rv = handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                             callback2.callback());
  EXPECT_EQ(read_buffer->size(), callback2.GetResult(rv));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
}

TEST_F(SSLClientSocketPoolTest, TagTwoSocketsFullPool) {
  // Start test server.
  EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, SSLServerConfig());
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  TransportClientSocketPool tcp_pool(
      kMaxSockets, kMaxSocketsPerGroup, &host_resolver_,
      ClientSocketFactory::GetDefaultFactory(), NULL, NULL);
  cert_verifier_->set_default_result(OK);
  SSLClientSocketPool pool(kMaxSockets, kMaxSocketsPerGroup,
                           cert_verifier_.get(), NULL /* channel_id_service */,
                           transport_security_state_.get(), &ct_verifier_,
                           &ct_policy_enforcer_,
                           std::string() /* ssl_session_cache_shard */,
                           ClientSocketFactory::GetDefaultFactory(), &tcp_pool,
                           NULL, NULL, NULL, NULL);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int32_t tag_val1 = 0x12345678;
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  int32_t tag_val2 = 0x87654321;
  SocketTag tag2(getuid(), tag_val2);
  scoped_refptr<TransportSocketParams> tcp_params(new TransportSocketParams(
      test_server.host_port_pair(), false, OnHostResolutionCallback(),
      TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DEFAULT));
  scoped_refptr<SSLSocketParams> params(new SSLSocketParams(
      tcp_params, NULL, NULL, test_server.host_port_pair(), ssl_config_,
      PRIVACY_MODE_DISABLED, false /* ignore_certificate_errors */));

  // Test that sockets paused by a full underlying socket pool are properly
  // connected and tagged when underlying pool is freed up.
  // Fill up all slots in TCP pool.
  ClientSocketHandle tcp_handles[kMaxSocketsPerGroup];
  int rv;
  for (auto& tcp_handle : tcp_handles) {
    rv = tcp_handle.Init(kGroupName, tcp_params, LOW, tag1,
                         ClientSocketPool::RespectLimits::ENABLED,
                         callback.callback(), &tcp_pool, NetLogWithSource());
    EXPECT_THAT(callback.GetResult(rv), IsOk());
    EXPECT_TRUE(tcp_handle.socket());
    EXPECT_TRUE(tcp_handle.socket()->IsConnected());
  }
  // Request two SSL sockets.
  ClientSocketHandle handle_to_be_canceled;
  rv = handle_to_be_canceled.Init(
      kGroupName, params, LOW, tag1, ClientSocketPool::RespectLimits::ENABLED,
      callback.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = handle.Init(kGroupName, params, LOW, tag2,
                   ClientSocketPool::RespectLimits::ENABLED,
                   callback.callback(), &pool, NetLogWithSource());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  // Cancel first request.
  handle_to_be_canceled.Reset();
  // Disconnect a TCP socket to free up a slot.
  tcp_handles[0].socket()->Disconnect();
  tcp_handles[0].Reset();
  // Verify |handle| gets a valid tagged socket.
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle.socket());
  EXPECT_TRUE(handle.socket()->IsConnected());
  uint64_t old_traffic = GetTaggedBytes(tag_val2);
  const char kRequest[] = "GET / HTTP/1.1\r\n\r\n";
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kRequest);
  rv =
      handle.socket()->Write(write_buffer.get(), strlen(kRequest),
                             callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(static_cast<int>(strlen(kRequest)), callback.GetResult(rv));
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1);
  EXPECT_EQ(handle.socket()->Read(read_buffer.get(), read_buffer->size(),
                                  callback.callback()),
            ERR_IO_PENDING);
  EXPECT_THAT(callback.WaitForResult(), read_buffer->size());
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);
}
#endif
}  // namespace

}  // namespace net
