// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tls_stream_attempt.h"

#include <memory>

#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_network_session.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/ssl/test_static_ech_mode_getter.h"
#include "net/test/cert_builder.h"
#include "net/test/gtest_util.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace net {

using test::IsError;
using test::IsOk;

namespace {

// Returns the default HostPortPair used for stream attempts in tests.
// Avoids declaring a static constant because HostPortPair has a non-trivial
// constructor (allocates a std::string), which is forbidden by the Chromium
// C++ Style Guide.
HostPortPair DefaultHostPortPair() {
  return HostPortPair("a.test", 443);
}

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

class TlsStreamAttemptHelper : public TlsStreamAttempt::Delegate {
 public:
  using ServiceEndpointResult =
      base::expected<ServiceEndpoint,
                     TlsStreamAttempt::GetServiceEndpointError>;

  // Pass std::nullopt to `service_endpoint` to make the ServiceEndpoint not
  // immediately available.
  explicit TlsStreamAttemptHelper(
      const StreamAttemptParams* params,
      SSLConfig base_ssl_config = SSLConfig(),
      std::optional<ServiceEndpoint> service_endpoint = ServiceEndpoint())
      : attempt_(std::make_unique<TlsStreamAttempt>(
            params,
            IPEndPoint(IPAddress(192, 0, 2, 1), 443),
            handles::kInvalidNetworkHandle,
            perfetto::Track(),
            DefaultHostPortPair(),
            std::move(base_ssl_config),
            this)),
        service_endpoint_result_(std::move(service_endpoint)) {}

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

  void SetServiceEndpoint(ServiceEndpointResult result) {
    CHECK(!service_endpoint_result_.has_value());
    service_endpoint_result_ = std::move(result);

    if (request_service_endpoint_callback_) {
      std::move(request_service_endpoint_callback_).Run(OK);
    }
  }

  void ResetAttempt() { attempt_.reset(); }

  TlsStreamAttempt* attempt() { return attempt_.get(); }

  std::optional<int> result() const { return result_; }

  // TlsStreamAttempt::Delegate implementation:

  void OnTcpHandshakeComplete() override {}

  int WaitForTlsHandshakeReady(CompletionOnceCallback callback) override {
    if (service_endpoint_result_.has_value()) {
      return OK;
    }

    CHECK(request_service_endpoint_callback_.is_null());
    request_service_endpoint_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  ServiceEndpointResult GetServiceEndpointForTlsHandshake() override {
    return *service_endpoint_result_;
  }

  CompletionOnceCallback TakeServiceEndpointWaitingCallback() {
    CHECK(request_service_endpoint_callback_);
    return std::move(request_service_endpoint_callback_);
  }

 private:
  void OnComplete(int rv) {
    result_ = rv;
    if (completion_closure_) {
      std::move(completion_closure_).Run();
    }
  }

  std::unique_ptr<TlsStreamAttempt> attempt_;

  CompletionOnceCallback request_service_endpoint_callback_;
  std::optional<ServiceEndpointResult> service_endpoint_result_;

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

  void SetEchEnabled(bool ech_enabled) {
    SSLContextConfig config = ssl_config_service_->GetSSLContextConfig();
    config.ech_enabled = ech_enabled;
    ssl_config_service_->UpdateSSLConfigAndNotify(config);
  }

  void SetEchMode(EchMode ech_mode) {
    ssl_config_service_->SetEchModeGetter(
        std::make_unique<TestStaticEchModeGetter>(
            ech_mode, DefaultHostPortPair().host()));
  }

  void SetTrustedTrustAnchorIDs(
      absl::flat_hash_set<std::vector<uint8_t>> trust_anchor_ids) {
    SSLContextConfig config = ssl_config_service_->GetSSLContextConfig();
    config.trust_anchor_ids = std::move(trust_anchor_ids);
    ssl_config_service_->UpdateSSLConfigAndNotify(config);
  }

  void SetTrustedTrustAnchorIDs(
      absl::flat_hash_set<std::vector<uint8_t>> trust_anchor_ids,
      std::vector<std::vector<uint8_t>> mtc_trust_anchor_ids) {
    SSLContextConfig config = ssl_config_service_->GetSSLContextConfig();
    config.trust_anchor_ids = std::move(trust_anchor_ids);
    config.mtc_trust_anchor_ids = std::move(mtc_trust_anchor_ids);
    ssl_config_service_->UpdateSSLConfigAndNotify(config);
  }

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
    session_context.net_log = NetLog::Get();
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

TEST_F(TlsStreamAttemptTest, ServiceEndpointDelayed) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                /*service_endpoint=*/std::nullopt);
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_CONNECTING);

  // We don't provide ServiceEndpoint yet so the attempt should not complete.
  RunUntilIdle();
  ASSERT_FALSE(helper.result().has_value());
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_SSL_HANDSHAKE);

  helper.SetServiceEndpoint(ServiceEndpoint());
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
  ValidateConnectTiming(helper.attempt()->connect_timing());
}

TEST_F(TlsStreamAttemptTest, GetServiceEndpointAborted) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                /*service_endpoint=*/std::nullopt);
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_SSL_HANDSHAKE);

  helper.SetServiceEndpoint(
      base::unexpected(TlsStreamAttempt::GetServiceEndpointError::kAbort));
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_ABORTED));
}

// Regression test for crbug.com/402288759. Callback passed to
// WaitForServiceEndpointReady() could be moved and invoked later.
TEST_F(TlsStreamAttemptTest, ServiceEndpointWaitingCallbackInvokedAfterReset) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                /*service_endpoint=*/std::nullopt);
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_CONNECTING);

  // We don't provide SSLConfig yet so the attempt should not complete.
  FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(helper.result().has_value());
  ASSERT_EQ(helper.attempt()->GetLoadState(), LOAD_STATE_SSL_HANDSHAKE);

  CompletionOnceCallback callback = helper.TakeServiceEndpointWaitingCallback();
  helper.ResetAttempt();

  // Invoking `callback` should do nothing.
  std::move(callback).Run(OK);
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
  ssl.next_proto = NextProto::kProtoHTTP2;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<StreamSocket> stream_socket =
      helper.attempt()->ReleaseStreamSocket();
  ASSERT_TRUE(stream_socket);
  EXPECT_EQ(stream_socket->GetNegotiatedProtocol(), NextProto::kProtoHTTP2);
}

TEST_F(TlsStreamAttemptTest, ClientAuthCertNeeded) {
  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port = DefaultHostPortPair();
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
  EXPECT_EQ(cert_request_info->host_and_port, DefaultHostPortPair());
}

TEST_F(TlsStreamAttemptTest, EchOk) {
  SetEchEnabled(true);

  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("public.example", /*max_name_len=*/128,
                              &ech_config_list));

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expected_ech_config_list = ech_config_list;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  ServiceEndpoint service_endpoint;
  service_endpoint.metadata.ech_config_list = ech_config_list;

  TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                std::move(service_endpoint));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
}

TEST_F(TlsStreamAttemptTest, EchRetryOk) {
  SetEchEnabled(true);

  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("public1.example", /*max_name_len=*/128,
                              &ech_config_list));

  std::vector<uint8_t> ech_retry_config_list;
  ASSERT_TRUE(MakeTestEchKeys("public2.example", /*max_name_len=*/128,
                              &ech_config_list));

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl.expected_ech_config_list = ech_config_list;
  ssl.ech_retry_configs = ech_retry_config_list;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  StaticSocketDataProvider retry_data;
  socket_factory().AddSocketDataProvider(&retry_data);
  SSLSocketDataProvider retry_ssl(ASYNC, OK);
  retry_ssl.expected_ech_config_list = ech_retry_config_list;
  socket_factory().AddSSLSocketDataProvider(&retry_ssl);

  ServiceEndpoint service_endpoint;
  service_endpoint.metadata.ech_config_list = ech_config_list;

  TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                std::move(service_endpoint));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
}

TEST_F(TlsStreamAttemptTest, EchRetryFail) {
  SetEchEnabled(true);

  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("public1.example", /*max_name_len=*/128,
                              &ech_config_list));

  std::vector<uint8_t> ech_retry_config_list;
  ASSERT_TRUE(MakeTestEchKeys("public2.example", /*max_name_len=*/128,
                              &ech_config_list));

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl.expected_ech_config_list = ech_config_list;
  ssl.ech_retry_configs = ech_retry_config_list;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  StaticSocketDataProvider retry_data;
  socket_factory().AddSocketDataProvider(&retry_data);
  SSLSocketDataProvider retry_ssl(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  retry_ssl.expected_ech_config_list = ech_retry_config_list;
  socket_factory().AddSSLSocketDataProvider(&retry_ssl);

  ServiceEndpoint service_endpoint;
  service_endpoint.metadata.ech_config_list = ech_config_list;

  TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                std::move(service_endpoint));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_ECH_NOT_NEGOTIATED));
}

// Tests that strict ECH mode triggers error when the server provides
// an empty retry config.
TEST_F(TlsStreamAttemptTest, EchStrictRetryEmptyFail) {
  SetEchEnabled(true);
  SetEchMode(EchMode::kStrict);

  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("public1.example", /*max_name_len=*/128,
                              &ech_config_list));

  std::vector<uint8_t> ech_retry_config_list;

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_ECH_NOT_NEGOTIATED);
  ssl.expected_ech_config_list = ech_config_list;
  ssl.ech_retry_configs = ech_retry_config_list;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  StaticSocketDataProvider retry_data;
  socket_factory().AddSocketDataProvider(&retry_data);
  SSLSocketDataProvider retry_ssl(ASYNC, ERR_STRICT_ECH_REQUIRED);
  socket_factory().AddSSLSocketDataProvider(&retry_ssl);

  ServiceEndpoint service_endpoint;
  service_endpoint.metadata.ech_config_list = ech_config_list;

  TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                std::move(service_endpoint));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsError(ERR_STRICT_ECH_REQUIRED));
}

// Tests that TlsStreamAttempt sends TLS Trust Anchor IDs unconditionally based
// on features.
TEST_F(TlsStreamAttemptTest, TrustAnchorIDsInitialSuccess) {
  const std::vector<uint8_t> id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> id2 = {0x02, 0x02};
  const std::vector<uint8_t> id3 = {0x03, 0x03};
  const std::vector<uint8_t> id4 = {0x04, 0x04};

  for (bool trust_anchor_ids_enabled : {false, true}) {
    SCOPED_TRACE(trust_anchor_ids_enabled);
    for (bool non_mtc_enabled : {false, true}) {
      SCOPED_TRACE(non_mtc_enabled);
      for (bool mtc_enabled : {false, true}) {
        SCOPED_TRACE(mtc_enabled);

        SetTrustedTrustAnchorIDs({id1, id2}, {id3, id4});
        ServiceEndpoint service_endpoint;
        service_endpoint.metadata.trust_anchor_ids = {id1, id3};

        std::vector<base::test::FeatureRef> enabled_features;
        std::vector<base::test::FeatureRef> disabled_features;

        if (trust_anchor_ids_enabled) {
          enabled_features.push_back(features::kTLSTrustAnchorIDs);
        } else {
          disabled_features.push_back(features::kTLSTrustAnchorIDs);
        }

        if (non_mtc_enabled) {
          enabled_features.push_back(features::kNonMtcTrustAnchorIDs);
        } else {
          disabled_features.push_back(features::kNonMtcTrustAnchorIDs);
        }

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
        if (mtc_enabled) {
          enabled_features.push_back(features::kVerifyMTCs);
        } else {
          disabled_features.push_back(features::kVerifyMTCs);
        }
#endif

        base::test::ScopedFeatureList feature_list;
        feature_list.InitWithFeatures(enabled_features, disabled_features);

        StaticSocketDataProvider data;
        socket_factory().AddSocketDataProvider(&data);
        SSLSocketDataProvider ssl(ASYNC, OK);

        bool expect_any = false;
        std::vector<std::vector<uint8_t>> expected_ids;
        if (trust_anchor_ids_enabled) {
          if (non_mtc_enabled) {
            expect_any = true;
            expected_ids.push_back({0x01, 0x02, 0x03});
            expected_ids.push_back({0x02, 0x02});
          }
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
          if (mtc_enabled) {
            expect_any = true;
            expected_ids.push_back({0x03, 0x03});
            expected_ids.push_back({0x04, 0x04});
          }
#endif
        }

        if (expect_any) {
          ssl.expected_trust_anchor_ids = expected_ids;
        } else {
          ssl.expect_no_trust_anchor_ids = true;
        }
        socket_factory().AddSSLSocketDataProvider(&ssl);

        TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                      std::move(service_endpoint));
        int rv = helper.Start();
        EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

        base::HistogramTester histogram_tester;
        rv = helper.WaitForCompletion();
        EXPECT_THAT(rv, IsOk());
        histogram_tester.ExpectTotalCount(
            "Net.SSL_Connection_Error_TrustAnchorIDs", 0);
        histogram_tester.ExpectTotalCount(
            "Net.SSL_Connection_Latency_TrustAnchorIDs", 0);
        histogram_tester.ExpectUniqueSample(
            "Net.SSL.TrustAnchorIDsResult",
            SSLClientSocket::TrustAnchorIDsResult::kNoDnsSuccessInitial, 1);
      }
    }
  }
}

// Tests that if the Trust Anchor IDs feature is enabled, but no IDs are
// configured, the extension is not sent.
TEST_F(TlsStreamAttemptTest, TrustAnchorIDsNoTrustAnchorIDsConfigured) {
  base::test::ScopedFeatureList feature_list;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  feature_list.InitWithFeatures(
      {features::kTLSTrustAnchorIDs, features::kVerifyMTCs,
       features::kNonMtcTrustAnchorIDs},
      {});
#else
  feature_list.InitWithFeatures(
      {features::kTLSTrustAnchorIDs, features::kNonMtcTrustAnchorIDs}, {});
#endif

  const std::vector<uint8_t> id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> id3 = {0x03, 0x03};
  const std::vector<uint8_t> id4 = {0x04, 0x04};

  ServiceEndpoint service_endpoint;
  service_endpoint.metadata.trust_anchor_ids = {id1, id3, id4};

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expect_no_trust_anchor_ids = true;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), SSLConfig(),
                                std::move(service_endpoint));
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::HistogramTester histogram_tester;
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
  histogram_tester.ExpectTotalCount("Net.SSL_Connection_Error_TrustAnchorIDs",
                                    0);
  histogram_tester.ExpectTotalCount("Net.SSL_Connection_Latency_TrustAnchorIDs",
                                    0);
  histogram_tester.ExpectUniqueSample(
      "Net.SSL.TrustAnchorIDsResult",
      SSLClientSocket::TrustAnchorIDsResult::kNoDnsSuccessInitial, 1);
}

//
TEST_F(TlsStreamAttemptTest, ServerPaddingNotRequested) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAddTLSServerHandshakePadding);

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), SSLConfig(), ServiceEndpoint());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::HistogramTester histogram_tester;
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
  histogram_tester.ExpectTotalCount("Net.SSL_Connection_Latency_ServerPadding",
                                    0);
}

TEST_F(TlsStreamAttemptTest, ServerPaddingRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAddTLSServerHandshakePadding,
      {{"AddTLSServerHandshakePaddingBytes", "128"}});

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expected_server_padding_to_request = 128;
  ssl.ssl_info.server_padding_received = true;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), SSLConfig(), ServiceEndpoint());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::HistogramTester histogram_tester;
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
  histogram_tester.ExpectTotalCount("Net.SSL_Connection_Latency_ServerPadding",
                                    1);
}

TEST_F(TlsStreamAttemptTest, ServerPaddingRequestZeroPadding) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAddTLSServerHandshakePadding,
      {{"AddTLSServerHandshakePaddingBytes", "0"}});

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expected_server_padding_to_request = 0;
  ssl.ssl_info.server_padding_received = true;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), SSLConfig(), ServiceEndpoint());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::HistogramTester histogram_tester;
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
  histogram_tester.ExpectTotalCount("Net.SSL_Connection_Latency_ServerPadding",
                                    1);
}

TEST_F(TlsStreamAttemptTest, ServerPaddingRequestButNotReceived) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAddTLSServerHandshakePadding,
      {{"AddTLSServerHandshakePaddingBytes", "0"}});

  StaticSocketDataProvider data;
  socket_factory().AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expected_server_padding_to_request = 0;
  ssl.ssl_info.server_padding_received = false;
  socket_factory().AddSSLSocketDataProvider(&ssl);

  TlsStreamAttemptHelper helper(params(), SSLConfig(), ServiceEndpoint());
  int rv = helper.Start();
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  base::HistogramTester histogram_tester;
  rv = helper.WaitForCompletion();
  EXPECT_THAT(rv, IsOk());
  histogram_tester.ExpectTotalCount("Net.SSL_Connection_Latency_ServerPadding",
                                    0);
}

}  // namespace net
