// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job.h"

#include <list>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/scheme_host_port.h"

using ::testing::_;

namespace net {

using test::IsError;
using test::IsOk;

using Group = HttpStreamPool::Group;
using Job = HttpStreamPool::Job;

namespace {

class FakeServiceEndpointRequest : public HostResolver::ServiceEndpointRequest {
 public:
  FakeServiceEndpointRequest() = default;

  void set_start_result(int start_result) { start_result_ = start_result; }

  void set_endpoints(std::vector<ServiceEndpoint> endpoints) {
    endpoints_ = std::move(endpoints);
  }

  FakeServiceEndpointRequest& add_endpoint(ServiceEndpoint endpoint) {
    endpoints_.emplace_back(std::move(endpoint));
    return *this;
  }

  FakeServiceEndpointRequest& set_aliases(std::set<std::string> aliases) {
    aliases_ = std::move(aliases);
    return *this;
  }

  FakeServiceEndpointRequest& set_crypto_ready(bool endpoints_crypto_ready) {
    endpoints_crypto_ready_ = endpoints_crypto_ready;
    return *this;
  }

  FakeServiceEndpointRequest& set_resolve_error_info(
      ResolveErrorInfo resolve_error_info) {
    resolve_error_info_ = resolve_error_info;
    return *this;
  }

  RequestPriority priority() const { return priority_; }
  FakeServiceEndpointRequest& set_priority(RequestPriority priority) {
    priority_ = priority;
    return *this;
  }

  FakeServiceEndpointRequest& CallOnServiceEndpointsUpdated();

  FakeServiceEndpointRequest& CallOnServiceEndpointRequestFinished(int rv);

  // HostResolver::ServiceEndpointRequest methods:
  int Start(Delegate* delegate) override;
  const std::vector<ServiceEndpoint>& GetEndpointResults() override;
  const std::set<std::string>& GetDnsAliasResults() override;
  bool EndpointsCryptoReady() override;
  ResolveErrorInfo GetResolveErrorInfo() override;
  void ChangeRequestPriority(RequestPriority priority) override;

 private:
  raw_ptr<Delegate> delegate_;

  int start_result_ = ERR_IO_PENDING;
  std::vector<ServiceEndpoint> endpoints_;
  std::set<std::string> aliases_;
  bool endpoints_crypto_ready_ = false;
  ResolveErrorInfo resolve_error_info_;
  RequestPriority priority_ = RequestPriority::IDLE;
};

FakeServiceEndpointRequest&
FakeServiceEndpointRequest::CallOnServiceEndpointsUpdated() {
  CHECK(delegate_);
  delegate_->OnServiceEndpointsUpdated();
  return *this;
}

FakeServiceEndpointRequest&
FakeServiceEndpointRequest::CallOnServiceEndpointRequestFinished(int rv) {
  CHECK(delegate_);
  endpoints_crypto_ready_ = true;
  delegate_->OnServiceEndpointRequestFinished(rv);
  return *this;
}

int FakeServiceEndpointRequest::Start(Delegate* delegate) {
  CHECK(!delegate_);
  CHECK(delegate);
  delegate_ = delegate;
  return start_result_;
}

const std::vector<ServiceEndpoint>&
FakeServiceEndpointRequest::GetEndpointResults() {
  return endpoints_;
}

const std::set<std::string>& FakeServiceEndpointRequest::GetDnsAliasResults() {
  return aliases_;
}

bool FakeServiceEndpointRequest::EndpointsCryptoReady() {
  return endpoints_crypto_ready_;
}

ResolveErrorInfo FakeServiceEndpointRequest::GetResolveErrorInfo() {
  return resolve_error_info_;
}

void FakeServiceEndpointRequest::ChangeRequestPriority(
    RequestPriority priority) {
  priority_ = priority;
}

class FakeServiceEndpointResolver : public HostResolver {
 public:
  FakeServiceEndpointResolver() = default;

  FakeServiceEndpointResolver(const FakeServiceEndpointResolver&) = delete;
  FakeServiceEndpointResolver& operator=(const FakeServiceEndpointResolver&) =
      delete;

  ~FakeServiceEndpointResolver() override = default;

  FakeServiceEndpointRequest* AddFakeRequest();

  // HostResolver methods:
  void OnShutdown() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters) override;
  std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) override;

 private:
  std::list<std::unique_ptr<FakeServiceEndpointRequest>> requests_;
};

FakeServiceEndpointRequest* FakeServiceEndpointResolver::AddFakeRequest() {
  std::unique_ptr<FakeServiceEndpointRequest> request =
      std::make_unique<FakeServiceEndpointRequest>();
  FakeServiceEndpointRequest* raw_request = request.get();
  requests_.emplace_back(std::move(request));
  return raw_request;
}

void FakeServiceEndpointResolver::OnShutdown() {}

std::unique_ptr<HostResolver::ResolveHostRequest>
FakeServiceEndpointResolver::CreateRequest(
    url::SchemeHostPort host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters) {
  NOTREACHED_NORETURN();
}

std::unique_ptr<HostResolver::ResolveHostRequest>
FakeServiceEndpointResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkAnonymizationKey& network_anonymization_key,
    const NetLogWithSource& net_log,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  NOTREACHED_NORETURN();
}

std::unique_ptr<HostResolver::ServiceEndpointRequest>
FakeServiceEndpointResolver::CreateServiceEndpointRequest(
    Host host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveHostParameters parameters) {
  CHECK(!requests_.empty());
  std::unique_ptr<FakeServiceEndpointRequest> request =
      std::move(requests_.front());
  requests_.pop_front();
  request->set_priority(parameters.initial_priority);
  return request;
}

IPEndPoint MakeIPEndPoint(std::string_view addr, uint16_t port = 80) {
  return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
}

// A helper to build a ServiceEndpoint.
class EndpointHelper {
 public:
  EndpointHelper() = default;

  EndpointHelper& add_v4(std::string_view addr, uint16_t port = 80) {
    endpoint_.ipv4_endpoints.emplace_back(MakeIPEndPoint(addr));
    return *this;
  }

  EndpointHelper& add_v6(std::string_view addr, uint16_t port = 80) {
    endpoint_.ipv6_endpoints.emplace_back(MakeIPEndPoint(addr));
    return *this;
  }

  ServiceEndpoint endpoint() const { return endpoint_; }

 private:
  ServiceEndpoint endpoint_;
};

// A helper to request an HttpStream. On success, it keeps the provided
// HttpStream. On failure, it keeps error information.
class StreamRequester : public HttpStreamRequest::Delegate {
 public:
  StreamRequester() : destination_(url::SchemeHostPort("http", "a.test", 80)) {}

  explicit StreamRequester(const HttpStreamKey& key)
      : destination_(key.destination()),
        privacy_mode_(key.privacy_mode()),
        secure_dns_policy_(key.secure_dns_policy()),
        disable_cert_network_fetches_(key.disable_cert_network_fetches()) {}

  StreamRequester(const StreamRequester&) = delete;
  StreamRequester& operator=(const StreamRequester&) = delete;

  ~StreamRequester() override = default;

  StreamRequester& set_destination(std::string_view destination) {
    return set_destination(url::SchemeHostPort(GURL(destination)));
  }

  StreamRequester& set_destination(url::SchemeHostPort destination) {
    destination_ = std::move(destination);
    return *this;
  }

  StreamRequester& set_priority(RequestPriority priority) {
    priority_ = priority;
    return *this;
  }

  HttpStreamKey GetStreamKey() const {
    return HttpStreamKey(destination_, privacy_mode_, SocketTag(),
                         NetworkAnonymizationKey(), secure_dns_policy_,
                         disable_cert_network_fetches_);
  }

  HttpStreamRequest* RequestStream(HttpStreamPool& pool) {
    HttpStreamKey stream_key = GetStreamKey();
    Group& group = pool.GetOrCreateGroupForTesting(stream_key);
    request_ = group.RequestStream(this, priority_, allowed_bad_certs_,
                                   NetLogWithSource());
    return request_.get();
  }

  void CancelRequest() { request_.reset(); }

  // HttpStreamRequest::Delegate methods:
  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    stream_ = std::move(stream);
    result_ = OK;
  }

  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {
    NOTREACHED_NORETURN();
  }

  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {
    NOTREACHED_NORETURN();
  }

  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_error_info) override {
    result_ = status;
    net_error_details_ = net_error_details;
    resolve_error_info_ = resolve_error_info;
  }

  void OnCertificateError(int status, const SSLInfo& ssl_info) override {
    result_ = status;
    cert_error_ssl_info_ = ssl_info;
  }

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {
    NOTREACHED_NORETURN();
  }

  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override {
    CHECK(!cert_info_);
    result_ = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    cert_info_ = cert_info;
  }

  void OnQuicBroken() override {}

  std::unique_ptr<HttpStream> ReleaseStream() { return std::move(stream_); }

  std::optional<int> result() const { return result_; }

  const NetErrorDetails& net_error_details() const {
    return net_error_details_;
  }

  const ResolveErrorInfo& resolve_error_info() const {
    return resolve_error_info_;
  }

  const SSLInfo& cert_error_ssl_info() const { return cert_error_ssl_info_; }

  scoped_refptr<SSLCertRequestInfo> cert_info() const { return cert_info_; }

 private:
  url::SchemeHostPort destination_;
  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  SecureDnsPolicy secure_dns_policy_ = SecureDnsPolicy::kAllow;
  bool disable_cert_network_fetches_ = true;

  RequestPriority priority_ = RequestPriority::IDLE;

  std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;

  std::unique_ptr<HttpStreamRequest> request_;

  std::unique_ptr<HttpStream> stream_;
  std::optional<int> result_;
  NetErrorDetails net_error_details_;
  ResolveErrorInfo resolve_error_info_;
  SSLInfo cert_error_ssl_info_;
  scoped_refptr<SSLCertRequestInfo> cert_info_;
};

}  // namespace

class HttpStreamPoolJobTest : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolJobTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    session_deps_.alternate_host_resolver =
        std::make_unique<FakeServiceEndpointResolver>();
    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    pool_ = std::make_unique<HttpStreamPool>(http_network_session_.get());
  }

 protected:
  HttpStreamPool& pool() { return *pool_; }

  FakeServiceEndpointResolver* resolver() {
    return static_cast<FakeServiceEndpointResolver*>(
        session_deps_.alternate_host_resolver.get());
  }

  MockClientSocketFactory* socket_factory() {
    return session_deps_.socket_factory.get();
  }

  SSLConfigService* ssl_config_service() {
    return session_deps_.ssl_config_service.get();
  }

 private:
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
  std::unique_ptr<HttpStreamPool> pool_;
};

TEST_F(HttpStreamPoolJobTest, ResolveEndpointFailedSync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request->set_start_result(ERR_FAILED);
  StreamRequester requester;
  requester.RequestStream(pool());
  EXPECT_THAT(*requester.result(), IsError(ERR_FAILED));
}

TEST_F(HttpStreamPoolJobTest, ResolveEndpointFailedMultipleRequests) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.RequestStream(pool());

  StreamRequester requester2;
  requester2.RequestStream(pool());

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  RunUntilIdle();

  EXPECT_THAT(*requester1.result(), IsError(ERR_FAILED));
  EXPECT_THAT(*requester2.result(), IsError(ERR_FAILED));
}

TEST_F(HttpStreamPoolJobTest, LoadState) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());

  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  EXPECT_THAT(*requester.result(), IsError(ERR_FAILED));

  RunUntilIdle();
  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(HttpStreamPoolJobTest, ResolveErrorInfo) {
  ResolveErrorInfo resolve_error_info(ERR_NAME_NOT_RESOLVED);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  endpoint_request->set_resolve_error_info(resolve_error_info);

  StreamRequester requester;
  requester.RequestStream(pool());

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);
  RunUntilIdle();
  EXPECT_THAT(*requester.result(), IsError(ERR_NAME_NOT_RESOLVED));
  ASSERT_EQ(requester.resolve_error_info(), resolve_error_info);
}

TEST_F(HttpStreamPoolJobTest, SetPriority) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  StreamRequester requester1;
  HttpStreamRequest* request1 =
      requester1.set_priority(RequestPriority::LOW).RequestStream(pool());
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);

  StreamRequester requester2;
  HttpStreamRequest* request2 =
      requester2.set_priority(RequestPriority::IDLE).RequestStream(pool());
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);

  request2->SetPriority(RequestPriority::HIGHEST);
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::HIGHEST);

  // Check `request2` completes first.

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data1.get());

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());

  endpoint_request->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointsUpdated();
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);

  RunUntilIdle();
  ASSERT_FALSE(request1->completed());
  ASSERT_TRUE(request2->completed());
  std::unique_ptr<HttpStream> stream = requester2.ReleaseStream();
  ASSERT_TRUE(stream);
}

TEST_F(HttpStreamPoolJobTest, TcpFailSync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(*requester.result(), IsError(ERR_FAILED));
}

TEST_F(HttpStreamPoolJobTest, TcpFailAsync) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(*requester.result(), IsError(ERR_FAILED));
}

TEST_F(HttpStreamPoolJobTest, TlsOk) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(EndpointHelper().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(*requester.result(), IsOk());
}

TEST_F(HttpStreamPoolJobTest, TlsCryptoReadyDelayed) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(EndpointHelper().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_FALSE(requester.result().has_value());

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_THAT(*requester.result(), IsOk());
}

TEST_F(HttpStreamPoolJobTest, CertificateError) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const scoped_refptr<X509Certificate> kCert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_DATE_INVALID);
  ssl.ssl_info.cert_status = ERR_CERT_DATE_INVALID;
  ssl.ssl_info.cert = kCert;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  constexpr std::string_view kDestination = "https://a.test";
  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(EndpointHelper().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_FALSE(requester1.result().has_value());
  EXPECT_FALSE(requester2.result().has_value());

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_THAT(*requester1.result(), IsError(ERR_CERT_DATE_INVALID));
  EXPECT_THAT(*requester2.result(), IsError(ERR_CERT_DATE_INVALID));
  ASSERT_TRUE(
      requester1.cert_error_ssl_info().cert->EqualsIncludingChain(kCert.get()));
  ASSERT_TRUE(
      requester2.cert_error_ssl_info().cert->EqualsIncludingChain(kCert.get()));
}

TEST_F(HttpStreamPoolJobTest, NeedsClientAuth) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(EndpointHelper().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_FALSE(requester1.result().has_value());
  EXPECT_FALSE(requester2.result().has_value());

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_EQ(requester1.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
  EXPECT_EQ(requester2.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
}

// Tests that after a fatal error (e.g., the server required a client cert),
// following attempt failures are ignored and the existing requests get the
// same fatal error.
TEST_F(HttpStreamPoolJobTest, TcpFailAfterNeedsClientAuth) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data1 = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data1.get());
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data2.get());

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(EndpointHelper().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_EQ(requester1.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
  EXPECT_EQ(requester2.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
}

TEST_F(HttpStreamPoolJobTest, RequestCancelledBeforeAttemptSuccess) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  requester.CancelRequest();
  RunUntilIdle();

  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolJobTest, OneIPEndPointFailed) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data2.get());

  endpoint_request->add_endpoint(
      EndpointHelper().add_v6("2001:db8::1").add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(*requester.result(), IsOk());
}

TEST_F(HttpStreamPoolJobTest, IPEndPointTimedout) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(TcpStreamAttempt::kTcpHandshakeTimeout);
  ASSERT_TRUE(requester.result().has_value());
  EXPECT_THAT(*requester.result(), IsError(ERR_TIMED_OUT));
}

TEST_F(HttpStreamPoolJobTest, IPEndPointsSlow) {
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());

  auto data1 = std::make_unique<SequencedSocketData>();
  // Make the first and the second attempt stalled.
  data1->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());
  // The third attempt succeeds.
  auto data3 = std::make_unique<SequencedSocketData>();
  data3->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data3.get());

  endpoint_request->add_endpoint(EndpointHelper()
                                     .add_v6("2001:db8::1")
                                     .add_v6("2001:db8::2")
                                     .add_v4("192.0.2.1")
                                     .endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  Job* job = pool()
                 .GetOrCreateGroupForTesting(requester.GetStreamKey())
                 .GetJobForTesting();
  ASSERT_EQ(job->InFlightAttemptCount(), 1u);
  ASSERT_FALSE(request->completed());

  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_EQ(job->InFlightAttemptCount(), 2u);
  ASSERT_EQ(job->PendingRequestCount(), 0u);
  ASSERT_FALSE(request->completed());

  // FastForwardBy() executes non-delayed tasks so the request finishes
  // immediately.
  FastForwardBy(HttpStreamPool::kConnectionAttemptDelay);
  ASSERT_TRUE(request->completed());
  EXPECT_THAT(*requester.result(), IsOk());
}

TEST_F(HttpStreamPoolJobTest, ReachedGroupLimit) {
  constexpr size_t kMaxPerGroup = 4;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  // Create streams up to the per-group limit for a destination.
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> data_providers;
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    auto requester = std::make_unique<StreamRequester>();
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());

    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
  }

  endpoint_request->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  Group& group =
      pool().GetOrCreateGroupForTesting(requesters[0]->GetStreamKey());
  Job* job = group.GetJobForTesting();
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), kMaxPerGroup);
  ASSERT_EQ(job->PendingRequestCount(), 0u);

  // This request should not start an attempt as the group reached its limit.
  StreamRequester stalled_requester;
  HttpStreamRequest* stalled_request = stalled_requester.RequestStream(pool());
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());
  data_providers.emplace_back(std::move(data));

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), kMaxPerGroup);
  ASSERT_EQ(job->PendingRequestCount(), 1u);

  // Finish all in-flight attempts successfully.
  RunUntilIdle();
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), 0u);
  ASSERT_EQ(job->PendingRequestCount(), 1u);

  // Release one HttpStream and close it to make non-reusable.
  std::unique_ptr<StreamRequester> released_requester =
      std::move(requesters.back());
  requesters.pop_back();
  std::unique_ptr<HttpStream> released_stream =
      released_requester->ReleaseStream();

  // Need to initialize the HttpStream as HttpBasicStream doesn't disconnect
  // the underlying stream socket when not initialized.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  released_stream->RegisterRequest(&request_info);
  released_stream->InitializeStream(/*can_send_early=*/false,
                                    RequestPriority::IDLE, NetLogWithSource(),
                                    base::DoNothing());

  released_stream->Close(/*not_reusable=*/true);
  released_stream.reset();

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), 1u);
  ASSERT_EQ(job->PendingRequestCount(), 0u);

  RunUntilIdle();

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(job->InFlightAttemptCount(), 0u);
  ASSERT_EQ(job->PendingRequestCount(), 0u);
  ASSERT_TRUE(stalled_request->completed());
  std::unique_ptr<HttpStream> stream = stalled_requester.ReleaseStream();
  ASSERT_TRUE(stream);
}

TEST_F(HttpStreamPoolJobTest, ReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  const HttpStreamKey key_a(url::SchemeHostPort("http", "a.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  const HttpStreamKey key_b(url::SchemeHostPort("http", "b.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  // Create HttpStreams up to the group limit in group A.
  Group& group_a = pool().GetOrCreateGroupForTesting(key_a);
  std::vector<std::unique_ptr<HttpStream>> streams_a;
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    streams_a.emplace_back(
        group_a.CreateTextBasedStream(std::make_unique<FakeStreamSocket>()));
  }

  ASSERT_FALSE(pool().ReachedMaxStreamLimit());
  ASSERT_TRUE(group_a.ReachedMaxStreamLimit());
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group_a.ActiveStreamSocketCount(), kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  // Create a HttpStream in group B. It should not be blocked because both
  // per-group and per-pool limits are not reached yet.
  StreamRequester requester1(key_b);
  HttpStreamRequest* request1 = requester1.RequestStream(pool());
  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data1.get());

  endpoint_request->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  ASSERT_TRUE(request1->completed());

  // The pool and group A reached limits, group B doesn't.
  Group& group_b = pool().GetOrCreateGroupForTesting(key_b);
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_TRUE(group_a.ReachedMaxStreamLimit());
  ASSERT_FALSE(group_b.ReachedMaxStreamLimit());

  // Create another HttpStream in group B. It should be blocked because the pool
  // reached limit, event when group B doesn't reach its limit.
  StreamRequester requester2(key_b);
  HttpStreamRequest* request2 = requester2.RequestStream(pool());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data2.get());

  RunUntilIdle();
  Job* job_b = group_b.GetJobForTesting();
  ASSERT_FALSE(request2->completed());
  ASSERT_EQ(job_b->InFlightAttemptCount(), 0u);
  ASSERT_EQ(job_b->PendingRequestCount(), 1u);

  // Release one HttpStream from group A. It should unblock the in-flight
  // request in group B.
  std::unique_ptr<HttpStream> released_stream = std::move(streams_a.back());
  streams_a.pop_back();
  released_stream.reset();
  RunUntilIdle();

  ASSERT_TRUE(request2->completed());
  ASSERT_EQ(job_b->PendingRequestCount(), 0u);
}

TEST_F(HttpStreamPoolJobTest, ReachedPoolLimitHighPriorityGroupFirst) {
  constexpr size_t kMaxPerGroup = 1;
  constexpr size_t kMaxPerPool = 2;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  // Create 4 requests with different destinations and priorities.
  constexpr struct Item {
    std::string_view host;
    std::string_view ip_address;
    RequestPriority priority;
  } items[] = {
      {"a.test", "192.0.2.1", RequestPriority::IDLE},
      {"b.test", "192.0.2.2", RequestPriority::IDLE},
      {"c.test", "192.0.2.3", RequestPriority::LOWEST},
      {"d.test", "192.0.2.4", RequestPriority::HIGHEST},
  };

  std::vector<FakeServiceEndpointRequest*> endpoint_requests;
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> socket_datas;
  for (const auto& [host, ip_address, priority] : items) {
    FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
    endpoint_request->add_endpoint(
        EndpointHelper().add_v4(ip_address).endpoint());
    endpoint_requests.emplace_back(endpoint_request);

    auto requester = std::make_unique<StreamRequester>();
    requester->set_destination(url::SchemeHostPort("http", host, 80))
        .set_priority(priority);
    requesters.emplace_back(std::move(requester));

    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    socket_datas.emplace_back(std::move(data));
  }

  // Complete the first two requests to reach the pool's limit.
  for (size_t i = 0; i < kMaxPerPool; ++i) {
    HttpStreamRequest* request = requesters[i]->RequestStream(pool());
    endpoint_requests[i]->CallOnServiceEndpointRequestFinished(OK);
    RunUntilIdle();
    ASSERT_TRUE(request->completed());
  }

  ASSERT_TRUE(pool().ReachedMaxStreamLimit());

  // Start the remaining requests. These requests should be blocked.
  HttpStreamRequest* request_c = requesters[2]->RequestStream(pool());
  endpoint_requests[2]->CallOnServiceEndpointRequestFinished(OK);

  HttpStreamRequest* request_d = requesters[3]->RequestStream(pool());
  endpoint_requests[3]->CallOnServiceEndpointRequestFinished(OK);

  RunUntilIdle();

  ASSERT_FALSE(request_c->completed());
  ASSERT_FALSE(request_d->completed());

  // Release the HttpStream from group A. It should unblock group D, which has
  // higher priority than group C.
  std::unique_ptr<HttpStream> stream_a = requesters[0]->ReleaseStream();
  stream_a.reset();

  RunUntilIdle();

  ASSERT_FALSE(request_c->completed());
  ASSERT_TRUE(request_d->completed());

  // Release the HttpStream from group B. It should unblock group C.
  std::unique_ptr<HttpStream> stream_b = requesters[1]->ReleaseStream();
  stream_b.reset();

  RunUntilIdle();

  ASSERT_TRUE(request_c->completed());
}

TEST_F(HttpStreamPoolJobTest, RequestStreamIdleStreamSocket) {
  StreamRequester requester;
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  HttpStreamRequest* request = requester.RequestStream(pool());
  RunUntilIdle();
  ASSERT_TRUE(request->completed());

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolJobTest, UseIdleStreamSocketAfterRelease) {
  StreamRequester requester;
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());

  // Create HttpStreams up to the group's limit.
  std::vector<std::unique_ptr<HttpStream>> streams;
  for (size_t i = 0; i < pool().max_stream_sockets_per_group(); ++i) {
    std::unique_ptr<HttpStream> http_stream =
        group.CreateTextBasedStream(std::make_unique<FakeStreamSocket>());
    streams.emplace_back(std::move(http_stream));
  }
  ASSERT_EQ(group.ActiveStreamSocketCount(),
            pool().max_stream_sockets_per_group());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);

  // Request a stream. The request should be blocked.
  resolver()->AddFakeRequest();
  HttpStreamRequest* request = requester.RequestStream(pool());
  RunUntilIdle();
  Job* job = group.GetJobForTesting();
  ASSERT_FALSE(request->completed());
  ASSERT_EQ(job->PendingRequestCount(), 1u);

  // Release an active HttpStream. The underlying StreamSocket should be used
  // to the pending request.
  std::unique_ptr<HttpStream> released_stream = std::move(streams.back());
  streams.pop_back();

  released_stream.reset();
  ASSERT_TRUE(request->completed());
  ASSERT_EQ(job->PendingRequestCount(), 0u);
}

TEST_F(HttpStreamPoolJobTest,
       CloseIdleStreamAttemptConnectionReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  const HttpStreamKey key_a(url::SchemeHostPort("http", "a.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  const HttpStreamKey key_b(url::SchemeHostPort("http", "b.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  // Add idle streams up to the group's limit in group A.
  Group& group_a = pool().GetOrCreateGroupForTesting(key_a);
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    group_a.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  }
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 2u);
  ASSERT_FALSE(pool().ReachedMaxStreamLimit());

  // Create an HttpStream in group B. The pool should reach its limit.
  Group& group_b = pool().GetOrCreateGroupForTesting(key_b);
  std::unique_ptr<HttpStream> stream1 =
      group_b.CreateTextBasedStream(std::make_unique<FakeStreamSocket>());
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());

  // Request a stream in group B. The request should close an idle stream in
  // group A.
  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();
  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  ASSERT_TRUE(request->completed());
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 1u);
}

// Tests that all in-flight requests and connection attempts are canceled
// when an IP address change event happens.
TEST_F(HttpStreamPoolJobTest, CancelAttemptAndRequestsOnIPAddressChange) {
  FakeServiceEndpointRequest* endpoint_request1 = resolver()->AddFakeRequest();
  FakeServiceEndpointRequest* endpoint_request2 = resolver()->AddFakeRequest();

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data1.get());

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());

  StreamRequester requester1;
  requester1.set_destination("https://a.test").RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination("https://b.test").RequestStream(pool());

  endpoint_request1->add_endpoint(
      EndpointHelper().add_v4("192.0.2.1").endpoint());
  endpoint_request1->CallOnServiceEndpointRequestFinished(OK);
  endpoint_request2->add_endpoint(
      EndpointHelper().add_v4("192.0.2.2").endpoint());
  endpoint_request2->CallOnServiceEndpointRequestFinished(OK);

  Job* job1 = pool()
                  .GetOrCreateGroupForTesting(requester1.GetStreamKey())
                  .GetJobForTesting();
  Job* job2 = pool()
                  .GetOrCreateGroupForTesting(requester2.GetStreamKey())
                  .GetJobForTesting();
  ASSERT_EQ(job1->RequestCount(), 1u);
  ASSERT_EQ(job1->InFlightAttemptCount(), 1u);
  ASSERT_EQ(job2->RequestCount(), 1u);
  ASSERT_EQ(job2->InFlightAttemptCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();
  ASSERT_EQ(job1->RequestCount(), 0u);
  ASSERT_EQ(job1->InFlightAttemptCount(), 0u);
  ASSERT_EQ(job2->RequestCount(), 0u);
  ASSERT_EQ(job2->InFlightAttemptCount(), 0u);
  EXPECT_THAT(*requester1.result(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_THAT(*requester2.result(), IsError(ERR_NETWORK_CHANGED));
}

// Tests that the network change error is reported even when a different error
// has already happened.
TEST_F(HttpStreamPoolJobTest, IPAddressChangeAfterNeedsClientAuth) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  FakeServiceEndpointRequest* endpoint_request = resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(EndpointHelper().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();
  EXPECT_THAT(*requester1.result(), IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED));
  EXPECT_THAT(*requester2.result(), IsError(ERR_NETWORK_CHANGED));
}

TEST_F(HttpStreamPoolJobTest, SSLConfigChangedCloseIdleStream) {
  StreamRequester requester;
  requester.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  ssl_config_service()->NotifySSLContextConfigChange();
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolJobTest,
       SSLConfigChangedReleasedStreamGenerationOutdated) {
  StreamRequester requester;
  requester.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  ssl_config_service()->NotifySSLContextConfigChange();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  // Release the HttpStream, the underlying StreamSocket should not be pooled
  // as an idle stream since the generation is different.
  stream.reset();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolJobTest, SSLConfigForServersChanged) {
  // Create idle streams in group A and group B.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  Group& group_a =
      pool().GetOrCreateGroupForTesting(requester_a.GetStreamKey());
  group_a.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 1u);

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");
  Group& group_b =
      pool().GetOrCreateGroupForTesting(requester_b.GetStreamKey());
  group_b.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 1u);

  // Simulate an SSLConfigForServers change event for group A. The idle stream
  // in group A should be gone but the idle stream in group B should remain.
  pool().OnSSLConfigForServersChanged({HostPortPair::FromSchemeHostPort(
      requester_a.GetStreamKey().destination())});
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 1u);
}

}  // namespace net
