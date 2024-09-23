// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tls_stream_attempt.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/http_network_session.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"

namespace net {

using test::IsError;
using test::IsOk;

namespace {

void ValidateConnectTiming(
    const LoadTimingInfo::ConnectTiming& connect_timing) {
  EXPECT_LE(connect_timing.domain_lookup_start,
            connect_timing.domain_lookup_end);
  EXPECT_LE(connect_timing.domain_lookup_end, connect_timing.connect_start);
  EXPECT_LE(connect_timing.connect_start, connect_timing.ssl_start);
  EXPECT_LE(connect_timing.ssl_start, connect_timing.ssl_end);
  // connectEnd should cover TLS handshake.
  EXPECT_LE(connect_timing.ssl_end, connect_timing.connect_end);
}

class TlsStreamAttemptHelper : public TlsStreamAttempt::SSLConfigProvider {
 public:
  // Pass std::nullopt to `ssl_config` to make SSLConfig not immediately
  // available.
  explicit TlsStreamAttemptHelper(
      const StreamAttemptParams* params,
      std::optional<SSLConfig> ssl_config = SSLConfig())
      : attempt_(std::make_unique<TlsStreamAttempt>(
            params,
            IPEndPoint(IPAddress(192, 0, 2, 1), 443),
            HostPortPair("a.test", 443),
            this)),
        ssl_config_(std::move(ssl_config)) {}

  ~TlsStreamAttemptHelper() override = default;

  int Start() {
    return attempt_->Start(base::BindOnce(&TlsStreamAttemptHelper::OnComplete,
                                          base::Unretained(this)));
  }

  int WaitForCompletion() {
    if (!result_.has_value()) {
      base::RunLoop loop;
      completion_closure_ = loop.QuitClosure();
      loop.Run();
    }

    return *result_;
  }

  void SetSSLConfig(SSLConfig ssl_config) {
    CHECK(!ssl_config_.has_value());
    ssl_config_ = std::move(ssl_config);

    if (request_ssl_config_callback_) {
      std::move(request_ssl_config_callback_).Run(OK);
    }
  }

  TlsStreamAttempt* attempt() { return attempt_.get(); }

  std::optional<int> result() const { return result_; }

  // TlsStreamAttempt::SSLConfigProvider implementation:
  int WaitForSSLConfigReady(CompletionOnceCallback callback) override {
    if (ssl_config_.has_value()) {
      return OK;
    }

    CHECK(request_ssl_config_callback_.is_null());
    request_ssl_config_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  SSLConfig GetSSLConfig() override { return *ssl_config_; }

 private:
  void OnComplete(int rv) {
    result_ = rv;
    if (completion_closure_) {
      std::move(completion_closure_).Run();
    }
  }

  std::unique_ptr<TlsStreamAttempt> attempt_;

  CompletionOnceCallback request_ssl_config_callback_;
  std::optional<SSLConfig> ssl_config_;

  base::OnceClosure completion_closure_;
  std::optional<int> result_;
};

}  // namespace

class TlsStreamAttemptTest : public TestWithTaskEnvironment {
 public:
  TlsStreamAttemptTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        ssl_config_service_(
            std::make_unique<TestSSLConfigService>(SSLContextConfig())),
        http_network_session_(CreateHttpNetworkSession()),
        params_(StreamAttemptParams::FromHttpNetworkSession(
            http_network_session_.get())) {}

 protected:
  MockClientSocketFactory& socket_factory() { return socket_factory_; }

  const StreamAttemptParams* params() const { return &params_; }

 private:
  std::unique_ptr<HttpNetworkSession> CreateHttpNetworkSession() {
    HttpNetworkSessionContext session_context;
    session_context.cert_verifier = &cert_verifier_;
    session_context.transport_security_state = &transport_security_state_;
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.client_socket_factory = &socket_factory_;
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.http_server_properties = &http_server_properties_;
    session_context.quic_context = &quic_context_;
    return std::make_unique<HttpNetworkSession>(HttpNetworkSessionParams(),
                                                session_context);
  }

  MockClientSocketFactory socket_factory_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<TestSSLConfigService> ssl_config_service_;
  HttpServerProperties http_server_properties_;
  QuicContext quic_context_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
  StreamAttemptParams params_;
};

TEST_F(TlsStreamAttemptTest, SuccessSync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_TRUE(stream_socket);
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_IDLE);
  ValidateConnectTiming(helper.attempt()->connect_timing());
}

TEST_F(TlsStreamAttemptTest, SuccessAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_TRUE(stream_socket);
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_IDLE);
  ValidateConnectTiming(helper.attempt()->connect_timing());
}

TEST_F(TlsStreamAttemptTest, ConnectAndConfirmDelayed) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);

  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.connect_callback =
      base::BindLambdaForTesting([&] { FastForwardBy(kDelay); });
  ssl.confirm = MockConfirm(SYNCHRONOUS, OK);
  ssl.confirm_callback =
      base::BindLambdaForTesting([&] { FastForwardBy(kDelay); });
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
  ValidateConnectTiming(helper.attempt()->connect_timing());
}

TEST_F(TlsStreamAttemptTest, SSLConfigDelayed) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), /*ssl_config=*/std::nullopt);
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_CONNECTING);

  // We don't provide SSLConfig yet so the attempt should not complete.
  RunUntilIdle();
  ASSERT_FALSE(helper.result().has_value());
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_SSL_HANDSHAKE);

  helper.SetSSLConfig(SSLConfig());
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
  ValidateConnectTiming(helper.attempt()->connect_timing());
}

TEST_F(TlsStreamAttemptTest, TcpFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory().AddSocketDataProvider(&data);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_FAILED));

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_FALSE(stream_socket);

  ASSERT_FALSE(helper.attempt()->IsTlsHandshakeStarted());
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_start.is_null());
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_end.is_null());
  ASSERT_TRUE(helper.attempt()->connect_timing().ssl_start.is_null());
  ASSERT_TRUE(helper.attempt()->connect_timing().ssl_end.is_null());
}

TEST_F(TlsStreamAttemptTest, TcpTimeout) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory().AddSocketDataProvider(&data);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_CONNECTING);

  FastForwardBy(TcpStreamAttempt::kTcpHandshakeTimeout);

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_TIMED_OUT));
  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_FALSE(stream_socket);
  ASSERT_FALSE(helper.attempt()->IsTlsHandshakeStarted());
}

TEST_F(TlsStreamAttemptTest, TlsTimeout) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_SSL_HANDSHAKE);

  FastForwardBy(TlsStreamAttempt::kTlsHandshakeTimeout);

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_TIMED_OUT));
  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_FALSE(stream_socket);
  ASSERT_TRUE(helper.attempt()->IsTlsHandshakeStarted());
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_start.is_null());
  ASSERT_FALSE(helper.attempt()->connect_timing().connect_end.is_null());
  ASSERT_FALSE(helper.attempt()->connect_timing().ssl_start.is_null());
  ASSERT_FALSE(helper.attempt()->connect_timing().ssl_end.is_null());
}

TEST_F(TlsStreamAttemptTest, CertError) {
  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_COMMON_NAME_INVALID);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_CERT_COMMON_NAME_INVALID));
  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_TRUE(stream_socket);
  ASSERT_TRUE(helper.attempt()->IsTlsHandshakeStarted());
}

TEST_F(TlsStreamAttemptTest, IgnoreCertError) {
  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expected_ignore_certificate_errors = true;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  SSLConfig ssl_config;
  ssl_config.ignore_certificate_errors = true;
  TlsStreamAttemptHelper helper(params(), std::move(ssl_config));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
}

TEST_F(TlsStreamAttemptTest, HandshakeError) {
  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_BAD_SSL_CLIENT_AUTH_CERT);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_BAD_SSL_CLIENT_AUTH_CERT));
  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_FALSE(stream_socket);
  ASSERT_TRUE(helper.attempt()->IsTlsHandshakeStarted());
}

TEST_F(TlsStreamAttemptTest, NegotiatedHttp2) {
  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = kProtoHTTP2;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_TRUE(stream_socket);
  EXPECT_EQ(stream_socket->GetNegotiatedProtocol(), kProtoHTTP2);
}

TEST_F(TlsStreamAttemptTest, ClientAuthCertNeeded) {
  const HostPortPair kHostPortPair("a.test", 443);

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port = kHostPortPair;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED));

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_FALSE(stream_socket);
  scoped_refptr<SSLCertRequestInfo> cert_request_info =
      helper.attempt()->GetCertRequestInfo();
  ASSERT_TRUE(cert_request_info);
  EXPECT_EQ(cert_request_info->host_and_port, kHostPortPair);
}

}  // namespace net
