// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_attempt.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_handle.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/scheme_host_port.h"

using testing::UnorderedElementsAreArray;

namespace net {

using Group = HttpStreamPool::Group;

namespace {

IPEndPoint MakeIPEndPoint(std::string_view addr, uint16_t port = 443) {
  return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
}

// Helper class for testing HttpStreamPool::Attempt.
class TestAttemptDelegate final
    : public HttpStreamPool::Attempt::Delegate,
      public HostResolver::ServiceEndpointRequest::Delegate {
 public:
  TestAttemptDelegate(HttpStreamPool* pool, std::string_view destination)
      : pool_(pool), key_builder_(destination) {
    CHECK(pool_);

    fake_service_endpoint_request_ =
        std::make_unique<FakeServiceEndpointRequest>();
    fake_service_endpoint_request_->Start(this);
  }

  ~TestAttemptDelegate() override = default;

  FakeServiceEndpointRequest* fake_service_endpoint_request() {
    return fake_service_endpoint_request_.get();
  }

  TestAttemptDelegate& set_is_svcb_optional(bool is_svcb_optional) {
    is_svcb_optional_ = is_svcb_optional;
    return *this;
  }

  HttpStreamPool::Attempt* attempt() const { return attempt_.get(); }

  std::optional<int> result() const { return result_; }

  void ResetServiceEndpointRequest() { fake_service_endpoint_request_.reset(); }

  void CompleteServiceEndpointRequest(int rv) {
    service_endpoint_request_result_ = rv;
    fake_service_endpoint_request_->CallOnServiceEndpointRequestFinished(rv);
  }

  void Start() {
    CHECK(!attempt_);
    CHECK(!key_.has_value());
    key_ = key_builder_.Build();
    // Using HTTP_STREAM_POOL_ATTEMPT_MANAGER as the source type here
    // because it will be the source type in production code.
    attempt_ = std::make_unique<HttpStreamPool::Attempt>(
        *this, *pool_->stream_attempt_params(),
        NetLogWithSource::Make(
            NetLog::Get(), NetLogSourceType::HTTP_STREAM_POOL_ATTEMPT_MANAGER));
    attempt_->Start();
  }

  int WaitForResult() {
    if (result_.has_value()) {
      return result_.value();
    }
    base::RunLoop run_loop;
    wait_result_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    return *result_;
  }

  // Returns the remote IPEndPoint of the socket used to create the stream.
  // Must be called after the attempt has completed successfully.
  IPEndPoint GetRemoteIPEndPoint() const {
    CHECK(remote_ip_endpoint_.has_value()) << "Remote IPEndPoint is not set";
    return remote_ip_endpoint_.value();
  }

  LoadTimingInfo::ConnectTiming GetConnectTiming() const {
    CHECK(connect_timing_.has_value()) << "Connect timing is not set";
    return connect_timing_.value();
  }

  // Returns the negotiated protocol of the attempt. Must be called after the
  // attempt has completed successfully.
  NextProto GetNegotiatedProtocol() const {
    CHECK(negotiated_protocol_.has_value()) << "Negotiated protocol is not set";
    return negotiated_protocol_.value();
  }

  scoped_refptr<const SSLCertRequestInfo> GetSSLCertRequestInfo() const {
    return ssl_cert_request_info_;
  }

  const SSLInfo& GetCertErrorSSLInfo() const {
    CHECK(cert_error_ssl_info_.has_value()) << "SSLInfo is not set";
    return *cert_error_ssl_info_;
  }

  // HttpStreamPool::Attempt::Delegate implementation:

  const HttpStreamKey& GetHttpStreamKey() const override { return *key_; }

  HostResolver::ServiceEndpointRequest& GetServiceEndpointRequest()
      const override {
    CHECK(fake_service_endpoint_request_);
    return *fake_service_endpoint_request_.get();
  }

  bool IsServiceEndpointRequestFinished() const override {
    return service_endpoint_request_result_.has_value();
  }

  bool IsSvcbOptional() const override { return is_svcb_optional_; }

  SSLConfig GetBaseSSLConfig() const override { return ssl_config_; }

  const NextProtoVector& GetAlpnProtos() const override {
    return pool_->http_network_session()->GetAlpnProtos();
  }

  void OnStreamSocketReady(
      HttpStreamPool::Attempt* attempt,
      std::unique_ptr<StreamSocket> stream,
      LoadTimingInfo::ConnectTiming connect_timing) override {
    SetRemoteIPEndPointFromStreamSocket(*stream);
    SetResult(OK);
    connect_timing_ = connect_timing;
  }

  void OnAttemptFailure(HttpStreamPool::Attempt* attempt, int rv) override {
    SetResult(rv);
  }

  void OnCertificateError(HttpStreamPool::Attempt* attempt,
                          int rv,
                          SSLInfo ssl_info) override {
    CHECK(!cert_error_ssl_info_);
    cert_error_ssl_info_ = ssl_info;
    SetResult(rv);
  }

  void OnNeedsClientCertificate(
      HttpStreamPool::Attempt* attempt,
      scoped_refptr<SSLCertRequestInfo> cert_info) override {
    CHECK(!ssl_cert_request_info_);
    ssl_cert_request_info_ = cert_info;
    SetResult(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  }

  // HostResolver::ServiceEndpointRequest::Delegate implementations:
  void OnServiceEndpointsUpdated() override {
    if (attempt_) {
      attempt_->ProcessServiceEndpointChanges();
    }
  }

  void OnServiceEndpointRequestFinished(int rv) override {
    CHECK_EQ(rv, *service_endpoint_request_result_);
    if (attempt_) {
      attempt_->ProcessServiceEndpointChanges();
    }
  }

 private:
  void SetResult(int result) {
    CHECK(!result_.has_value());
    result_ = result;
    if (wait_result_closure_) {
      std::move(wait_result_closure_).Run();
    }
    attempt_.reset();
  }

  void SetRemoteIPEndPointFromStreamSocket(StreamSocket& stream) {
    CHECK(!remote_ip_endpoint_);
    IPEndPoint remote_ip_endpoint;
    int rv = stream.GetPeerAddress(&remote_ip_endpoint);
    CHECK_EQ(rv, OK);
    remote_ip_endpoint_ = remote_ip_endpoint;
    negotiated_protocol_ = stream.GetNegotiatedProtocol();
  }

  const raw_ptr<HttpStreamPool> pool_;

  StreamKeyBuilder key_builder_;
  std::optional<HttpStreamKey> key_;

  std::unique_ptr<FakeServiceEndpointRequest> fake_service_endpoint_request_;
  std::optional<int> service_endpoint_request_result_;
  bool is_svcb_optional_ = true;

  SSLConfig ssl_config_;

  std::unique_ptr<HttpStreamPool::Attempt> attempt_;

  std::optional<int> result_;
  std::optional<IPEndPoint> remote_ip_endpoint_;
  std::optional<LoadTimingInfo::ConnectTiming> connect_timing_;
  std::optional<NextProto> negotiated_protocol_;
  base::OnceClosure wait_result_closure_;

  scoped_refptr<const SSLCertRequestInfo> ssl_cert_request_info_;
  std::optional<SSLInfo> cert_error_ssl_info_;
};

}  // namespace

class HttpStreamPoolAttemptTest : public TestWithTaskEnvironment {
 public:
  static constexpr base::TimeDelta kTinyDelta = base::Milliseconds(1);

  HttpStreamPoolAttemptTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(features::kHappyEyeballsV3);
    InitialzePool();
  }

  HttpStreamPool& pool() { return *http_network_session_->http_stream_pool(); }

  MockClientSocketFactory* socket_factory() {
    return session_deps_.socket_factory.get();
  }

  TestAttemptDelegate CreateAttemptDelegate(
      std::string_view destination = "https://a.test") {
    return TestAttemptDelegate(&pool(), destination);
  }

 private:
  void InitialzePool() {
    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  }

  base::test::ScopedFeatureList feature_list_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
};

TEST_F(HttpStreamPoolAttemptTest, PlainHttp) {
  TestAttemptDelegate delegate = CreateAttemptDelegate("http://a.test");
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1", 80).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), OK);
}

TEST_F(HttpStreamPoolAttemptTest, EmptyServiceEndpointRequest) {
  TestAttemptDelegate delegate = CreateAttemptDelegate("http://a.test");
  delegate.CompleteServiceEndpointRequest(OK);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), ERR_NAME_NOT_RESOLVED);
}

TEST_F(HttpStreamPoolAttemptTest, AllEndpointsFailed) {
  TestAttemptDelegate delegate = CreateAttemptDelegate();
  FakeServiceEndpointRequest* endpoint_request =
      delegate.fake_service_endpoint_request();

  const std::array<IPEndPoint, 4> endpoints = {
      MakeIPEndPoint("192.0.2.1"), MakeIPEndPoint("192.0.2.2"),
      MakeIPEndPoint("2001:db8::1"), MakeIPEndPoint("2001:db8::2")};
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (const auto& endpoint : endpoints) {
    endpoint_request->add_endpoint(
        ServiceEndpointBuilder().add_ip_endpoint(endpoint).endpoint());

    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
    socket_factory()->AddSocketDataProvider(data.get());

    datas.emplace_back(std::move(data));
  }
  delegate.CompleteServiceEndpointRequest(OK);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), ERR_CONNECTION_REFUSED);
}

TEST_F(HttpStreamPoolAttemptTest, NeedsClientAuth) {
  // This should not be attempted.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");

  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");

  const HostPortPair kDestination("a.test", 443);
  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)
          .add_ip_endpoint(ipv6_endpoint1)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_data(ASYNC, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl_data.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl_data.cert_request_info->host_and_port = kDestination;
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  EXPECT_EQ(delegate.GetSSLCertRequestInfo()->host_and_port, kDestination);
}

TEST_F(HttpStreamPoolAttemptTest, CertificateError) {
  // This should not be attempted.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");

  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");

  const HostPortPair kDestination("a.test", 443);
  const scoped_refptr<X509Certificate> kCert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)
          .add_ip_endpoint(ipv6_endpoint1)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_data(ASYNC, ERR_CERT_DATE_INVALID);
  ssl_data.ssl_info.cert_status = ERR_CERT_DATE_INVALID;
  ssl_data.ssl_info.cert = kCert;
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), ERR_CERT_DATE_INVALID);
  EXPECT_EQ(delegate.GetCertErrorSSLInfo().cert_status, ERR_CERT_DATE_INVALID);
  EXPECT_EQ(delegate.GetCertErrorSSLInfo().cert, kCert);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv4Only) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6Only) {
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6OnlyFailEndpointResolutionCompleteLater) {
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());

  SequencedSocketData data;
  MockConnectCompleter connect_completer;
  data.set_connect_data(MockConnect(&connect_completer));
  socket_factory()->AddSocketDataProvider(&data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  connect_completer.Complete(ERR_CONNECTION_REFUSED);
  // Since the service endpoint request is not completed, the attempt should
  // not be complete yet.
  ASSERT_FALSE(delegate.result().has_value());

  // Complete the service endpoint request successfully. Now the attempt should
  // complete with the most recent failure.
  delegate.CompleteServiceEndpointRequest(OK);
  ASSERT_EQ(delegate.WaitForResult(), ERR_CONNECTION_REFUSED);
}

TEST_F(HttpStreamPoolAttemptTest, Http2Ok) {
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  ssl_data.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint);
  EXPECT_EQ(delegate.GetNegotiatedProtocol(), NextProto::kProtoHTTP2);
}

TEST_F(HttpStreamPoolAttemptTest, ConnectTiming) {
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data;
  MockConnectCompleter tcp_completer;
  data.set_connect_data(MockConnect(&tcp_completer));
  socket_factory()->AddSocketDataProvider(&data);
  MockConnectCompleter tls_completer;
  SSLSocketDataProvider ssl_data(&tls_completer);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  constexpr base::TimeDelta kTcpDelay = base::Milliseconds(10);
  FastForwardBy(kTcpDelay);
  tcp_completer.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  constexpr base::TimeDelta kTlsDelay = base::Milliseconds(20);
  FastForwardBy(kTlsDelay);
  tls_completer.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint);

  LoadTimingInfo::ConnectTiming connect_timing = delegate.GetConnectTiming();
  // connectEnd includes TLS handshake. See
  // https://w3c.github.io/resource-timing/#attribute-descriptions
  EXPECT_EQ(connect_timing.connect_end - connect_timing.connect_start,
            kTcpDelay + kTlsDelay);
  EXPECT_EQ(connect_timing.ssl_end - connect_timing.ssl_start, kTlsDelay);

  // The Attempt doesn't control domain lookup timing.
  EXPECT_EQ(connect_timing.domain_lookup_start, base::TimeTicks());
  EXPECT_EQ(connect_timing.domain_lookup_end, base::TimeTicks());

  // Verify the overall ordering of timing events.
  EXPECT_LE(connect_timing.connect_start, connect_timing.ssl_start);
  EXPECT_LE(connect_timing.ssl_start, connect_timing.ssl_end);
  EXPECT_LE(connect_timing.ssl_end, connect_timing.connect_end);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv4AnsweredBeforeIpv6TcpHandshake) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          // This endpoint is attempted and succeeds.
          .add_ip_endpoint(ipv6_endpoint)
          // This endpoint shouldn't be attempted.
          .add_ip_endpoint(ipv4_endpoint)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer_v6;
  data_v6.set_connect_data(MockConnect(&connect_completer_v6));
  socket_factory()->AddSocketDataProvider(&data_v6);
  SSLSocketDataProvider ssl_data_v6(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete the first attempt's TCP handshake. Shouldn't trigger the attempt
  // for IPv4.
  connect_completer_v6.Complete(OK);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv4FirstOk) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint());

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data_v4(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v4);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify an IPv6 endpoint and complete the service endpoint request. This
  // shouldn't trigger the attempt for IPv6.
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv4_endpoint}));

  // Complete the first attempt.
  connect_completer_v4.Complete(OK);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv4FirstFail) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint());

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  socket_factory()->AddSocketDataProvider(&data_v4);

  SequencedSocketData data_v6;
  socket_factory()->AddSocketDataProvider(&data_v6);
  SSLSocketDataProvider ssl_data_v6(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify an IPv6 endpoint and complete the service endpoint request. This
  // shouldn't trigger the attempt for IPv6 yet.
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv4_endpoint}));

  // Fail the first attempt. Should trigger the attempt for IPv6.
  connect_completer_v4.Complete(ERR_CONNECTION_FAILED);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv4AnsweredAfterIpv6TcpHandshake) {
  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_v6("2001:db8::1", 443).endpoint());

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer_v6;
  data_v6.set_connect_data(MockConnect(&connect_completer_v6));
  socket_factory()->AddSocketDataProvider(&data_v6);
  SSLSocketDataProvider ssl_data_v6(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify an IPv4 endpoint and complete the service endpoint request. This
  // shouldn't trigger the attempt for IPv4.
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1", 443).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  // Complete the first attempt's TCP handshake. Shouldn't trigger the attempt
  // for IPv4.
  connect_completer_v6.Complete(OK);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), MakeIPEndPoint("2001:db8::1"));
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6FailIpv4Ok) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          // This endpoint is attempted second and succeeds.
          .add_ip_endpoint(ipv4_endpoint)
          // This endpoint is attempted first and fails.
          .add_ip_endpoint(ipv6_endpoint)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6;
  data_v6.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory()->AddSocketDataProvider(&data_v6);

  SequencedSocketData data_v4;
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, MultipleFailuresSecondIpv6Ok) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          // This endpoint is attempted second and fails.
          .add_ip_endpoint(ipv4_endpoint)
          // This endpoint is attempted first and fails.
          .add_ip_endpoint(ipv6_endpoint1)
          // This endpoint is attempted third and succeeds.
          .add_ip_endpoint(ipv6_endpoint2)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6_1;
  data_v6_1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  data_v6_1.set_expected_addresses(AddressList(ipv6_endpoint1));
  socket_factory()->AddSocketDataProvider(&data_v6_1);

  SequencedSocketData data_v4;
  data_v4.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v4);

  SequencedSocketData data_v6_2;
  data_v6_2.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  data_v6_2.set_expected_addresses(AddressList(ipv6_endpoint2));
  socket_factory()->AddSocketDataProvider(&data_v6_2);
  SSLSocketDataProvider ssl_data_v6_2(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6_2);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint2);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6FailNoIpv4Later) {
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  SequencedSocketData data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_FAILED));
  socket_factory()->AddSocketDataProvider(&data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  delegate.fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint())
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete the service endpoint request without IPv4 endpoints. This should
  // make the attempt fail.
  delegate.CompleteServiceEndpointRequest(OK);
  ASSERT_EQ(delegate.WaitForResult(), ERR_CONNECTION_FAILED);
}

TEST_F(HttpStreamPoolAttemptTest, SecondIpv6Ok) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)   // This endpoint fails.
          .add_ip_endpoint(ipv6_endpoint1)  // This endpoint fails.
          .add_ip_endpoint(ipv6_endpoint2)  // This endpoint succeeds.
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6_1;
  data_v6_1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_NETWORK_CHANGED));
  socket_factory()->AddSocketDataProvider(&data_v6_1);

  SequencedSocketData data_v4;
  socket_factory()->AddSocketDataProvider(&data_v4);
  data_v4.set_connect_data(MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));

  SequencedSocketData data_v6_2;
  socket_factory()->AddSocketDataProvider(&data_v6_2);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint2);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6OnlySlowOk) {
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv6_endpoint1)
          .add_ip_endpoint(ipv6_endpoint2)  // This should not be attempted.
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data;
  MockConnectCompleter connect_completer;
  data.set_connect_data(MockConnect(&connect_completer));
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_FALSE(delegate.result().has_value());

  connect_completer.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint1);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6SlowOk) {
  // This endpoint stalls forever.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  // This endpoint succeeds slowly.
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  // This should not be attempted.
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)
          .add_ip_endpoint(ipv6_endpoint1)
          .add_ip_endpoint(ipv6_endpoint2)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer;
  data_v6.set_connect_data(MockConnect(&connect_completer));
  data_v6.set_expected_addresses(AddressList(ipv6_endpoint1));
  socket_factory()->AddSocketDataProvider(&data_v6);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  // IPv4 stalls forever.
  SequencedSocketData data_v4;
  data_v4.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v4);

  delegate.Start();
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint1, ipv4_endpoint}));

  connect_completer.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint1);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6SlowFail) {
  // This endpoint succeeds.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  // This endpoint fails slowly.
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  // This endpoint stalls.
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)
          .add_ip_endpoint(ipv6_endpoint1)
          .add_ip_endpoint(ipv6_endpoint2)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6_1;
  MockConnectCompleter connect_completer_v6_1;
  data_v6_1.set_connect_data(MockConnect(&connect_completer_v6_1));
  socket_factory()->AddSocketDataProvider(&data_v6_1);

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  SequencedSocketData data_v6_2;
  data_v6_2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&data_v6_2);

  delegate.Start();

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_FALSE(delegate.result().has_value());

  // Simulate the first attempt timing out.
  connect_completer_v6_1.Complete(ERR_CONNECTION_TIMED_OUT);
  ASSERT_FALSE(delegate.result().has_value());

  // Simulate the second attempt succeeds.
  connect_completer_v4.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6SlowIpv4Ok) {
  // This endpoint is attempted second and succeeds.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  // This endpoint is attempted first but slow.
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  // This should not be attempted.
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)
          .add_ip_endpoint(ipv6_endpoint1)
          .add_ip_endpoint(ipv6_endpoint2)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer_v6;
  data_v6.set_connect_data(MockConnect(&connect_completer_v6));
  data_v6.set_expected_addresses(AddressList(ipv6_endpoint1));
  socket_factory()->AddSocketDataProvider(&data_v6);

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint1, ipv4_endpoint}));

  connect_completer_v4.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6SlowIpv4AnsweredLater) {
  // This endpoint is attempted second and succeeds.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  // This endpoint is attempted first but slow.
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  // This should not be attempted.
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv6_endpoint1)
          .add_ip_endpoint(ipv6_endpoint2)
          .endpoint());

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer_v6;
  data_v6.set_connect_data(MockConnect(&connect_completer_v6));
  data_v6.set_expected_addresses(AddressList(ipv6_endpoint1));
  socket_factory()->AddSocketDataProvider(&data_v6);

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint1, ipv4_endpoint}));

  connect_completer_v4.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6SlowFailIpv4AnsweredLater) {
  // This endpoint is attempted second but stalls.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  // This endpoint is attempted first but fails slowly.
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  // This endpoints succeeds.
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv6_endpoint1)
          .add_ip_endpoint(ipv6_endpoint2)
          .endpoint());

  SequencedSocketData data_v6_1;
  MockConnectCompleter connect_completer_v6_1;
  data_v6_1.set_connect_data(MockConnect(&connect_completer_v6_1));
  data_v6_1.set_expected_addresses(AddressList(ipv6_endpoint1));
  socket_factory()->AddSocketDataProvider(&data_v6_1);

  SequencedSocketData data_v6_2;
  MockConnectCompleter connect_completer_v6_2;
  data_v6_2.set_connect_data(MockConnect(&connect_completer_v6_2));
  data_v6_2.set_expected_addresses(AddressList(ipv6_endpoint2));
  socket_factory()->AddSocketDataProvider(&data_v6_2);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v4);

  delegate.Start();
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  // The first attempt fails. The second IPv6 endpoint should be attempted.
  connect_completer_v6_1.Complete(ERR_CONNECTION_REFUSED);
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint1, ipv6_endpoint2}));

  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray(
                  {ipv6_endpoint1, ipv6_endpoint2, ipv4_endpoint}));

  connect_completer_v6_2.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint2);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6FailIpv4SlowOk) {
  // This endpoint is attempted second and succeeds slowly.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  // This endpoint is attempted first and fails.
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)
          .add_ip_endpoint(ipv6_endpoint)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer_v6;
  data_v6.set_connect_data(MockConnect(&connect_completer_v6));
  data_v6.set_expected_addresses(AddressList(ipv6_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v6);

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint}));

  // The first attempt fails. Should trigger the second attempt.
  connect_completer_v6.Complete(ERR_CONNECTION_REFUSED);
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint, ipv4_endpoint}));
  ASSERT_FALSE(delegate.result().has_value());

  // The second attempt is slow. Since there is no other endpoints, no further
  // attempts are made.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint, ipv4_endpoint}));
  ASSERT_FALSE(delegate.result().has_value());

  connect_completer_v4.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6FailIpv4SlowFail) {
  // This endpoint is attempted second and fails slowly.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  // This endpoint is attempted first and fails.
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)
          .add_ip_endpoint(ipv6_endpoint)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer_v6;
  data_v6.set_connect_data(MockConnect(&connect_completer_v6));
  data_v6.set_expected_addresses(AddressList(ipv6_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v6);

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v4);

  delegate.Start();
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint}));

  // The first attempt fails. Should trigger the second attempt.
  connect_completer_v6.Complete(ERR_CONNECTION_REFUSED);
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint, ipv4_endpoint}));
  ASSERT_FALSE(delegate.result().has_value());

  // The second attempt is slow. Since there is no other endpoints, no further
  // attempts are made.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint, ipv4_endpoint}));
  ASSERT_FALSE(delegate.result().has_value());

  connect_completer_v4.Complete(ERR_CONNECTION_RESET);
  ASSERT_EQ(delegate.WaitForResult(), ERR_CONNECTION_RESET);
}

TEST_F(HttpStreamPoolAttemptTest, FirstIpv6FailIpv4SlowFailSecondIpv6Ok) {
  // This endpoint is attempted second and fails slowly.
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  // This endpoint is attempted first and fails.
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  // This endpoint is attempted third and succeeds.
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint)
          .add_ip_endpoint(ipv6_endpoint1)
          .add_ip_endpoint(ipv6_endpoint2)
          .endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data_v6_1;
  MockConnectCompleter connect_completer_v6_1;
  data_v6_1.set_connect_data(MockConnect(&connect_completer_v6_1));
  data_v6_1.set_expected_addresses(AddressList(ipv6_endpoint1));
  socket_factory()->AddSocketDataProvider(&data_v6_1);

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v4);

  SequencedSocketData data_v6_2;
  MockConnectCompleter connect_completer_v6_2;
  data_v6_2.set_connect_data(MockConnect(&connect_completer_v6_2));
  data_v6_2.set_expected_addresses(AddressList(ipv6_endpoint2));
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSocketDataProvider(&data_v6_2);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  // The first attempt fails. Should trigger the second attempt.
  connect_completer_v6_1.Complete(ERR_CONNECTION_REFUSED);
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint1, ipv4_endpoint}));
  ASSERT_FALSE(delegate.result().has_value());

  // The second attempt is slow. Should trigger the second IPv6 (the third in
  // total) attempt.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray(
                  {ipv6_endpoint1, ipv6_endpoint2, ipv4_endpoint}));
  ASSERT_FALSE(delegate.result().has_value());

  connect_completer_v6_2.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint2);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv4SlowIpv6AnsweredLater) {
  // This endpoint is attempted first and stalls.
  const IPEndPoint ipv4_endpoint1 = MakeIPEndPoint("192.0.2.1");
  // This endpoint should not be attempted.
  const IPEndPoint ipv4_endpoint2 = MakeIPEndPoint("192.0.2.2");
  // This endpoint is attempted second and succeeds.
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          .add_ip_endpoint(ipv4_endpoint1)
          .add_ip_endpoint(ipv4_endpoint2)
          .endpoint());

  SequencedSocketData data_v4;
  data_v4.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  data_v4.set_expected_addresses(AddressList(ipv4_endpoint1));
  socket_factory()->AddSocketDataProvider(&data_v4);

  SequencedSocketData data_v6;
  data_v6.set_expected_addresses(AddressList(ipv6_endpoint));
  socket_factory()->AddSocketDataProvider(&data_v6);
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv4_endpoint1}));

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv4_endpoint1}));

  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv4_endpoint1, ipv6_endpoint}));

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, EndpointIpv6NotUsableForTcp) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  // Set up endpoints. The IPv6 endpoint is only usable for H3, while the IPv4
  // endpoint is usable for all protocols.
  delegate.fake_service_endpoint_request()->set_endpoints(
      {ServiceEndpointBuilder()
           .add_ip_endpoint(ipv6_endpoint)
           .set_alpns({"h3"})
           .endpoint(),
       ServiceEndpointBuilder()
           .add_ip_endpoint(ipv4_endpoint)
           // Empty alpns means all protocols are usable.
           .set_alpns({})
           .endpoint()});
  delegate.CompleteServiceEndpointRequest(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, EndpointIpv4NotUsableForTcp) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  // Set up endpoints. The IPv6 endpoint is usable for H1/H2, while the IPv4
  // endpoint is only usable for H3.
  delegate.fake_service_endpoint_request()->set_endpoints(
      {ServiceEndpointBuilder()
           .add_ip_endpoint(ipv6_endpoint)
           .set_alpns({"h2", "http/1.1"})
           .endpoint(),
       ServiceEndpointBuilder()
           .add_ip_endpoint(ipv4_endpoint)
           .set_alpns({"h3"})
           .endpoint()});
  delegate.CompleteServiceEndpointRequest(OK);

  // The IPv6 attempt fails. Since the IPv4 endpoint is not usable, the whole
  // attempt fails without attempting IPv4.
  SequencedSocketData data;
  data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  socket_factory()->AddSocketDataProvider(&data);

  delegate.Start();

  ASSERT_EQ(delegate.WaitForResult(), ERR_CONNECTION_REFUSED);
}

TEST_F(HttpStreamPoolAttemptTest, EndpointNotUsableForTcp) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  // Set up endpoints. These endpoints are only usable for H3.
  delegate.fake_service_endpoint_request()->set_endpoints(
      {ServiceEndpointBuilder()
           .add_ip_endpoint(ipv6_endpoint)
           .set_alpns({"h3"})
           .endpoint(),
       ServiceEndpointBuilder()
           .add_ip_endpoint(ipv4_endpoint)
           .set_alpns({"h3"})
           .endpoint()});
  delegate.CompleteServiceEndpointRequest(OK);

  delegate.Start();

  ASSERT_EQ(delegate.WaitForResult(), ERR_NAME_NOT_RESOLVED);
}

TEST_F(HttpStreamPoolAttemptTest, EndpointDisappearsAfterTcpHandshake) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer;
  data_v6.set_connect_data(MockConnect(&connect_completer));
  socket_factory()->AddSocketDataProvider(&data_v6);

  SequencedSocketData data_v4;
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify intermediate endpoint.
  delegate.fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint())
      .CallOnServiceEndpointsUpdated();

  // Complete TCP handshake. The attempt should not start TLS handshake yet
  // since the endpoint isn't ready for cryptographic handshake.
  connect_completer.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  // Simulate a case where the previous endpoint disappears. The attempt should
  // attempt the new endpoint.
  delegate.fake_service_endpoint_request()->set_endpoints(
      {ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint()});
  delegate.CompleteServiceEndpointRequest(OK);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest,
       SvcbReliantNoUsableEndpointsBeforeTcpHandshake) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  // The attempt is SVCB-reliant (non-optional), but all endpoints don't have
  // ALPN. These endpoints are not usable.
  delegate.set_is_svcb_optional(false)
      .fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint())
      .add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  delegate.Start();
  ASSERT_EQ(delegate.WaitForResult(), ERR_NAME_NOT_RESOLVED);
}

TEST_F(HttpStreamPoolAttemptTest, SvcbReliantAbortAfterTcpHandshake) {
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  SequencedSocketData data;
  MockConnectCompleter connect_completer;
  data.set_connect_data(MockConnect(&connect_completer));
  socket_factory()->AddSocketDataProvider(&data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify intermediate endpoint. This simulates a case where AAAA is answered
  // but HTTPS RR isn't yet. So far, the attempt is SVCB-optional so the attempt
  // starts TCP handshake but waits for HTTPS RR to be answered before starting
  // TLS handshake.
  delegate.fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint())
      .CallOnServiceEndpointsUpdated();

  // Complete TCP handshake. The attempt should not start TLS handshake yet
  // since the endpoint isn't ready for cryptographic handshake.
  connect_completer.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  // Simulate a case where the attempt becomes SVCB-reliant (non-optional) and
  // the endpoint is only available for H3 (non-TCP). The in-flight attempt
  // should be aborted.
  delegate.set_is_svcb_optional(false);
  delegate.fake_service_endpoint_request()->set_endpoints(
      {ServiceEndpointBuilder()
           .add_ip_endpoint(ipv6_endpoint)
           .set_alpns({"h3"})
           .endpoint()});
  delegate.CompleteServiceEndpointRequest(OK);
  ASSERT_EQ(delegate.WaitForResult(), ERR_ABORTED);
}

TEST_F(HttpStreamPoolAttemptTest, SvcbReliantIpv6AbortIpv4Ok) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(
      MakeTestEchKeys("a.test", /*max_name_len=*/128, &ech_config_list));

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  // Data for IPv6 endpoint. This stalls forever (and gets aborted).
  SequencedSocketData data_v6;
  data_v6.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&data_v6);

  // Data for IPv4 endpoint.
  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer;
  data_v4.set_connect_data(MockConnect(&connect_completer));
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  ssl_data.expected_ech_config_list = ech_config_list;
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify intermediate endpoints. This starts IPv6 attempt.
  delegate.fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint())
      .add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint())
      .CallOnServiceEndpointsUpdated();

  // Simulate the connection attempt delay to start IPv4 attempt.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());

  // Complete TCP handshake. The attempt should not start TLS handshake yet
  // since the endpoint isn't ready for cryptographic handshake.
  connect_completer.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  // Simulate a case where the attempt becomes SVCB-reliant (non-optional) and
  // the IPv4 endpoint is usable but the IPv6 endpoint is not. The IPv6 attempt
  // should be aborted.
  delegate.set_is_svcb_optional(false);
  delegate.fake_service_endpoint_request()->set_endpoints(
      {ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint(),
       ServiceEndpointBuilder()
           .add_ip_endpoint(ipv4_endpoint)
           .set_alpns({"http/1.1"})
           .set_ech_config_list(ech_config_list)
           .endpoint()});
  delegate.CompleteServiceEndpointRequest(OK);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6TlsHandshakeFailSynchronously) {
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  SequencedSocketData data;
  MockConnectCompleter connect_completer;
  data.set_connect_data(MockConnect(&connect_completer));
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, ERR_FAILED);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);
  socket_factory()->AddSocketDataProvider(&data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify intermediate endpoint.
  delegate.fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint())
      .CallOnServiceEndpointsUpdated();

  // Complete TCP handshake. The attempt should not start TLS handshake yet
  // since the endpoint isn't ready for cryptographic handshake.
  connect_completer.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  delegate.set_is_svcb_optional(true);
  delegate.CompleteServiceEndpointRequest(OK);

  ASSERT_EQ(delegate.WaitForResult(), ERR_FAILED);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv4TlsHandshakeFailSynchronously) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  SequencedSocketData data;
  MockConnectCompleter connect_completer;
  data.set_connect_data(MockConnect(&connect_completer));
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, ERR_FAILED);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);
  socket_factory()->AddSocketDataProvider(&data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify intermediate endpoint.
  delegate.fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint())
      .CallOnServiceEndpointsUpdated();

  // Complete TCP handshake. The attempt should not start TLS handshake yet
  // since the endpoint isn't ready for cryptographic handshake.
  connect_completer.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  delegate.set_is_svcb_optional(true);
  delegate.CompleteServiceEndpointRequest(OK);

  ASSERT_EQ(delegate.WaitForResult(), ERR_FAILED);
}

TEST_F(HttpStreamPoolAttemptTest, Ipv6Ipv4TlsHandshakeFailSynchronously) {
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();

  SequencedSocketData data_v6;
  MockConnectCompleter connect_completer_v6;
  data_v6.set_connect_data(MockConnect(&connect_completer_v6));
  SSLSocketDataProvider ssl_data_v6(SYNCHRONOUS, ERR_FAILED);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6);
  socket_factory()->AddSocketDataProvider(&data_v6);

  SequencedSocketData data_v4;
  MockConnectCompleter connect_completer_v4;
  data_v4.set_connect_data(MockConnect(&connect_completer_v4));
  SSLSocketDataProvider ssl_data_v4(SYNCHRONOUS, ERR_FAILED);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v4);
  socket_factory()->AddSocketDataProvider(&data_v4);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Notify intermediate endpoint.
  delegate.fake_service_endpoint_request()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint())
      .add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint())
      .CallOnServiceEndpointsUpdated();

  // Simulate the connection attempt delay to start IPv4 attempt.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());

  // Complete TCP handshakes. Attempts should not start TLS handshakes yet
  // since the endpoints aren't ready for cryptographic handshakes.
  connect_completer_v6.Complete(OK);
  connect_completer_v4.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  delegate.set_is_svcb_optional(true);
  delegate.CompleteServiceEndpointRequest(OK);

  ASSERT_EQ(delegate.WaitForResult(), ERR_FAILED);
}

TEST_F(HttpStreamPoolAttemptTest, WaitForTlsHandshakeReady) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          // This endpoint is attempted and succeeds.
          .add_ip_endpoint(ipv6_endpoint)
          // This endpoint shouldn't be attempted.
          .add_ip_endpoint(ipv4_endpoint)
          .endpoint());

  SequencedSocketData data;
  MockConnectCompleter tcp_completer;
  data.set_connect_data(MockConnect(&tcp_completer));
  socket_factory()->AddSocketDataProvider(&data);
  MockConnectCompleter tls_completer;
  SSLSocketDataProvider ssl_data(&tls_completer);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete TCP handshake with a delay. The first attempt should wait for the
  // endpoint to be ready for TLS handshake. This pauses the slow timer.
  const base::TimeDelta kTcpHandshakeDelay = base::Milliseconds(50);
  CHECK_LE(kTcpHandshakeDelay, HttpStreamPool::GetConnectionAttemptDelay());
  FastForwardBy(kTcpHandshakeDelay);
  tcp_completer.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  // Complete service endpoint request with a delay that is larger than the
  // connection attempt delay. Since the slow timer has been paused, the
  // second attempt should not be triggered.
  const base::TimeDelta kServiceEndpointDelay =
      HttpStreamPool::GetConnectionAttemptDelay() + base::Milliseconds(10);
  FastForwardBy(kServiceEndpointDelay);
  delegate.CompleteServiceEndpointRequest(OK);
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint}));

  // Complete TLS handshake with a delay that is smaller than the remaining
  // time for the slow timer. The total handshake delay (TCP + TLS) is smaller
  // than the connection attempt delay, so the second attempt should not be
  // triggered.
  FastForwardBy(kTinyDelta);
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint}));

  tls_completer.Complete(OK);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest, WaitForTlsHandshakeReadyIpv6Slow) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          // This endpoint is attempted but TLS handshake is slow.
          .add_ip_endpoint(ipv6_endpoint)
          // This endpoint is attempted and succeeds.
          .add_ip_endpoint(ipv4_endpoint)
          .endpoint());

  SequencedSocketData data_v6;
  MockConnectCompleter tcp_completer_v6;
  data_v6.set_connect_data(MockConnect(&tcp_completer_v6));
  socket_factory()->AddSocketDataProvider(&data_v6);
  SSLSocketDataProvider ssl_data(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  SequencedSocketData data_v4;
  socket_factory()->AddSocketDataProvider(&data_v4);
  MockConnectCompleter tls_completer_v4;
  SSLSocketDataProvider ssl_data_v4(&tls_completer_v4);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v4);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete TCP handshake with a delay. The first attempt should wait for the
  // endpoint to be ready for TLS handshake. This pauses the slow timer.
  const base::TimeDelta kTcpHandshakeDelay = base::Milliseconds(50);
  CHECK_LE(kTcpHandshakeDelay, HttpStreamPool::GetConnectionAttemptDelay());
  FastForwardBy(kTcpHandshakeDelay);
  tcp_completer_v6.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint}));

  // Complete service endpoint request with a delay that is larger than the
  // connection attempt delay. Since the slow timer has been paused, the
  // second attempt should not be triggered yet.
  const base::TimeDelta kServiceEndpointDelay =
      HttpStreamPool::GetConnectionAttemptDelay() + base::Milliseconds(10);
  FastForwardBy(kServiceEndpointDelay);
  delegate.CompleteServiceEndpointRequest(OK);

  // Simulate TLS handshake is slow. The second attempt should be triggered.
  const base::TimeDelta kRemainingDelay =
      HttpStreamPool::GetConnectionAttemptDelay() - kTcpHandshakeDelay;
  FastForwardBy(kRemainingDelay);
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint, ipv4_endpoint}));

  // Complete TLS handshake for the second attempt.
  tls_completer_v4.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest,
       WaitForTlsHandshakeReadyFirstIpv6FailIpv4SlowSecondIpv6Ok) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint1 = MakeIPEndPoint("2001:db8::1");
  const IPEndPoint ipv6_endpoint2 = MakeIPEndPoint("2001:db8::2");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder()
          // This endpoint is attempted first and TLS handshake fails.
          .add_ip_endpoint(ipv6_endpoint1)
          // This endpoint is attempted third and succeeds.
          .add_ip_endpoint(ipv6_endpoint2)
          // This endpoint is attempted second and TLS handshake is slow.
          .add_ip_endpoint(ipv4_endpoint)
          .endpoint());

  SequencedSocketData data_v6_1;
  MockConnectCompleter tcp_completer_v6_1;
  data_v6_1.set_connect_data(MockConnect(&tcp_completer_v6_1));
  socket_factory()->AddSocketDataProvider(&data_v6_1);
  MockConnectCompleter tls_completer_v6_1;
  SSLSocketDataProvider ssl_data_v6_1(&tls_completer_v6_1);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6_1);

  SequencedSocketData data_v4;
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data_v4(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v4);

  SequencedSocketData data_v6_2;
  data_v6_2.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(&data_v6_2);
  MockConnectCompleter tls_completer_v6_2;
  SSLSocketDataProvider ssl_data_v6_2(&tls_completer_v6_2);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6_2);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete TCP handshake with a delay. The first attempt should wait for the
  // endpoint to be ready for TLS handshake. This pauses the slow timer.
  const base::TimeDelta kTcpHandshakeDelay = base::Milliseconds(50);
  CHECK_LE(kTcpHandshakeDelay, HttpStreamPool::GetConnectionAttemptDelay());
  FastForwardBy(kTcpHandshakeDelay);
  tcp_completer_v6_1.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  // Complete service endpoint request with a delay that is larger than the
  // connection attempt delay. Since the slow timer has been paused, the
  // second attempt should not be triggered yet.
  const base::TimeDelta kServiceEndpointDelay =
      HttpStreamPool::GetConnectionAttemptDelay() + base::Milliseconds(10);
  FastForwardBy(kServiceEndpointDelay);
  delegate.CompleteServiceEndpointRequest(OK);
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray({ipv6_endpoint1}));

  // Complete TLS handshake for the first attempt. Should trigger the second
  // attempt.
  tls_completer_v6_1.Complete(ERR_CONNECTION_RESET);
  ASSERT_FALSE(delegate.result().has_value());
  EXPECT_THAT(
      delegate.attempt()->attempted_endpoints_for_testing(),
      testing::UnorderedElementsAreArray({ipv6_endpoint1, ipv4_endpoint}));

  // Simulate TLS handshake is slow. Currently we use separate slow timers for
  // each attempt so the delay is not cumulative. The third attempt should be
  // triggered.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  EXPECT_THAT(delegate.attempt()->attempted_endpoints_for_testing(),
              testing::UnorderedElementsAreArray(
                  {ipv6_endpoint1, ipv6_endpoint2, ipv4_endpoint}));

  tls_completer_v6_2.Complete(OK);
  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint2);
}

TEST_F(HttpStreamPoolAttemptTest, WaitForTlsHandshakeReadyIpv6SlowIpv4Late) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());

  SequencedSocketData data_v6;
  MockConnectCompleter tcp_completer_v6;
  data_v6.set_connect_data(MockConnect(&tcp_completer_v6));
  socket_factory()->AddSocketDataProvider(&data_v6);
  // IPv6 TLS handshake stalls forever.
  SSLSocketDataProvider ssl_data_v6(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6);

  SequencedSocketData data_v4;
  socket_factory()->AddSocketDataProvider(&data_v4);
  SSLSocketDataProvider ssl_data_v4(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v4);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete TCP handshake with a delay. The first attempt should wait for the
  // endpoint to be ready for TLS handshake.
  const base::TimeDelta kTcpHandshakeDelay = base::Milliseconds(50);
  CHECK_LE(kTcpHandshakeDelay, HttpStreamPool::GetConnectionAttemptDelay());
  FastForwardBy(kTcpHandshakeDelay);
  tcp_completer_v6.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  // Update service endpoint request with an IPv4 endpoint. This should not
  // trigger a new attempt yet.
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint());
  delegate.fake_service_endpoint_request()->CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete service endpoint request to make endpoints ready for TLS
  // handshake. The first attempt's TLS handshake stalls forever.
  delegate.CompleteServiceEndpointRequest(OK);
  ASSERT_FALSE(delegate.result().has_value());

  // Fire the slow timer. This should trigger IPv4 attempt.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay() -
                kTcpHandshakeDelay);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest,
       Ipv4AnsweredWhileWaitingForTlsHandshakeReady) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());

  SequencedSocketData data;
  MockConnectCompleter tcp_completer;
  data.set_connect_data(MockConnect(&tcp_completer));
  socket_factory()->AddSocketDataProvider(&data);
  MockConnectCompleter tls_completer;
  SSLSocketDataProvider ssl_data(&tls_completer);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete TCP handshake with a delay. The first attempt should wait for the
  // endpoint to be ready for TLS handshake.
  const base::TimeDelta kTcpHandshakeDelay = base::Milliseconds(50);
  CHECK_LE(kTcpHandshakeDelay, HttpStreamPool::GetConnectionAttemptDelay());
  FastForwardBy(kTcpHandshakeDelay);
  tcp_completer.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  // Update service endpoint request with an IPv4 endpoint. This should not
  // trigger a new attempt.
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint());
  delegate.fake_service_endpoint_request()->CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete service endpoint request to make endpoints ready for TLS
  // handshake.
  delegate.CompleteServiceEndpointRequest(OK);

  // Complete TLS handshake with a delay. The TLS delay is smaller than the
  // connection attempt delay, so the second attempt should not be triggered.
  const base::TimeDelta kTlsHandshakeDelay = base::Milliseconds(100);
  CHECK_LE(kTcpHandshakeDelay + kTlsHandshakeDelay,
           HttpStreamPool::GetConnectionAttemptDelay());
  FastForwardBy(kTlsHandshakeDelay);
  tls_completer.Complete(OK);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv6_endpoint);
}

TEST_F(HttpStreamPoolAttemptTest,
       SynchronousTlsHandshakeFailureAfterWaitingForTlsHandshakeReady) {
  const IPEndPoint ipv4_endpoint = MakeIPEndPoint("192.0.2.1");
  const IPEndPoint ipv6_endpoint = MakeIPEndPoint("2001:db8::1");

  TestAttemptDelegate delegate = CreateAttemptDelegate();
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv6_endpoint).endpoint());

  // IPv6 TCP succeeds.
  SequencedSocketData data_v6;
  MockConnectCompleter tcp_completer_v6;
  data_v6.set_connect_data(MockConnect(&tcp_completer_v6));
  socket_factory()->AddSocketDataProvider(&data_v6);
  // IPv6 TLS fails synchronously.
  SSLSocketDataProvider ssl_data_v6(SYNCHRONOUS, ERR_FAILED);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v6);

  // IPv4 TCP succeeds.
  SequencedSocketData data_v4;
  socket_factory()->AddSocketDataProvider(&data_v4);
  // IPv4 TLS succeeds.
  SSLSocketDataProvider ssl_data_v4(SYNCHRONOUS, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl_data_v4);

  delegate.Start();
  ASSERT_FALSE(delegate.result().has_value());

  // Complete TCP handshake for IPv6.
  tcp_completer_v6.Complete(OK);
  ASSERT_FALSE(delegate.result().has_value());

  // Add IPv4 and complete service endpoint request.
  delegate.fake_service_endpoint_request()->add_endpoint(
      ServiceEndpointBuilder().add_ip_endpoint(ipv4_endpoint).endpoint());
  delegate.CompleteServiceEndpointRequest(OK);

  ASSERT_EQ(delegate.WaitForResult(), OK);
  EXPECT_EQ(delegate.GetRemoteIPEndPoint(), ipv4_endpoint);
}

}  // namespace net
