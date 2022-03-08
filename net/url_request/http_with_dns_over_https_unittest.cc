// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/public/util.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/log/net_log.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/test_doh_server.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {
namespace {

using net::test::IsOk;

const char kDohHostname[] = "doh-server.example";
const char kHostname[] = "bar.example.com";
const char kTestBody[] = "<html><body>TEST RESPONSE</body></html>";

class TestHostResolverProc : public HostResolverProc {
 public:
  TestHostResolverProc()
      : HostResolverProc(nullptr), insecure_queries_served_(0) {}

  int Resolve(const std::string& hostname,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override {
    insecure_queries_served_++;
    *addrlist = AddressList::CreateFromIPAddress(IPAddress(127, 0, 0, 1), 443);
    return OK;
  }

  uint32_t insecure_queries_served() { return insecure_queries_served_; }

 private:
  ~TestHostResolverProc() override {}
  uint32_t insecure_queries_served_;
};

class HttpWithDnsOverHttpsTest : public TestWithTaskEnvironment {
 public:
  HttpWithDnsOverHttpsTest()
      : host_resolver_proc_(new TestHostResolverProc()),
        request_context_(true),
        test_server_(EmbeddedTestServer::Type::TYPE_HTTPS),
        test_https_requests_served_(0) {
    EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {kHostname};
    test_server_.SetSSLConfig(cert_config);
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&HttpWithDnsOverHttpsTest::HandleDefaultRequest,
                            base::Unretained(this)));
    doh_server_.SetHostname(kDohHostname);
    doh_server_.AddAddressRecord(kHostname, IPAddress(127, 0, 0, 1));
    EXPECT_TRUE(doh_server_.Start());
    EXPECT_TRUE(test_server_.Start());

    // TODO(crbug.com/1252155): Simplify this.
    HostResolver::ManagerOptions manager_options;
    // Without a DnsConfig, HostResolverManager will not use DoH, even in
    // kSecure mode. See https://crbug.com/1251715. However,
    // DnsClient::BuildEffectiveConfig special-cases overrides that override
    // everything, so that gets around it. Ideally, we would instead mock out a
    // system DnsConfig via the usual pathway.
    manager_options.dns_config_overrides =
        DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
    manager_options.dns_config_overrides.secure_dns_mode =
        SecureDnsMode::kSecure;
    manager_options.dns_config_overrides.dns_over_https_config =
        *DnsOverHttpsConfig::FromString(doh_server_.GetPostOnlyTemplate());
    manager_options.dns_config_overrides.use_local_ipv6 = true;
    resolver_ = HostResolver::CreateStandaloneContextResolver(
        /*net_log=*/nullptr, manager_options);

    // Configure `resolver_` to use `host_resolver_proc_` to resolve
    // `doh_server_` itself. Additionally, without an explicit HostResolverProc,
    // HostResolverManager::HaveTestProcOverride disables the built-in DNS
    // client.
    resolver_->SetProcParamsForTesting(
        ProcTaskParams(host_resolver_proc_.get(), 1));

    resolver_->SetRequestContext(&request_context_);
    request_context_.set_host_resolver(resolver_.get());

    request_context_.Init();
  }

  URLRequestContext* context() { return &request_context_; }

  std::unique_ptr<test_server::HttpResponse> HandleDefaultRequest(
      const test_server::HttpRequest& request) {
    std::unique_ptr<test_server::BasicHttpResponse> http_response(
        new test_server::BasicHttpResponse);
    test_https_requests_served_++;
    http_response->set_content(kTestBody);
    http_response->set_content_type("text/html");
    return std::move(http_response);
  }

 protected:
  std::unique_ptr<ContextHostResolver> resolver_;
  scoped_refptr<net::TestHostResolverProc> host_resolver_proc_;
  TestURLRequestContext request_context_;
  TestDohServer doh_server_;
  EmbeddedTestServer test_server_;
  uint32_t test_https_requests_served_;
};

class TestHttpDelegate : public HttpStreamRequest::Delegate {
 public:
  explicit TestHttpDelegate(base::RunLoop* loop) : loop_(loop) {}
  ~TestHttpDelegate() override {}
  void OnStreamReady(const SSLConfig& used_ssl_config,
                     const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    stream->Close(false);
    loop_->Quit();
  }

  void OnWebSocketHandshakeStreamReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {}

  void OnBidirectionalStreamImplReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {}

  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const SSLConfig& used_ssl_config,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_eror_info) override {}

  void OnCertificateError(int status,
                          const SSLConfig& used_ssl_config,
                          const SSLInfo& ssl_info) override {}

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const SSLConfig& used_ssl_config,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {}

  void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                         SSLCertRequestInfo* cert_info) override {}

  void OnQuicBroken() override {}

 private:
  raw_ptr<base::RunLoop> loop_;
};

// This test sets up a request which will reenter the connection pools by
// triggering a DNS over HTTPS request. It also sets up an idle socket
// which was a precondition for the crash we saw in  https://crbug.com/830917.
TEST_F(HttpWithDnsOverHttpsTest, EndToEnd) {
  // Create and start http server.
  EmbeddedTestServer http_server(EmbeddedTestServer::Type::TYPE_HTTP);
  http_server.RegisterRequestHandler(base::BindRepeating(
      &HttpWithDnsOverHttpsTest::HandleDefaultRequest, base::Unretained(this)));
  EXPECT_TRUE(http_server.Start());

  // Set up an idle socket.
  HttpTransactionFactory* transaction_factory =
      request_context_.http_transaction_factory();
  HttpStreamFactory::JobFactory default_job_factory;
  HttpNetworkSession* network_session = transaction_factory->GetSession();
  base::RunLoop loop;
  TestHttpDelegate request_delegate(&loop);

  HttpStreamFactory* factory = network_session->http_stream_factory();
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = http_server.GetURL("localhost", "/preconnect");

  std::unique_ptr<HttpStreamRequest> request(factory->RequestStream(
      request_info, DEFAULT_PRIORITY, SSLConfig(), SSLConfig(),
      &request_delegate, false, false, NetLogWithSource()));
  loop.Run();

  ClientSocketPool::GroupId group_id(
      url::SchemeHostPort(request_info.url), PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkIsolationKey(), SecureDnsPolicy::kAllow);
  EXPECT_EQ(network_session
                ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                ProxyServer::Direct())
                ->IdleSocketCountInGroup(group_id),
            1u);

  // The domain "localhost" is resolved locally, so no DNS lookups should have
  // occurred.
  EXPECT_EQ(doh_server_.QueriesServed(), 0);
  EXPECT_EQ(host_resolver_proc_->insecure_queries_served(), 0u);
  // A stream was established, but no HTTPS request has been made yet.
  EXPECT_EQ(test_https_requests_served_, 0u);

  // Make a request that will trigger a DoH query as well.
  TestDelegate d;
  GURL main_url = test_server_.GetURL(kHostname, "/test");
  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      main_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  base::RunLoop().Run();
  EXPECT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(http_server.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(doh_server_.ShutdownAndWaitUntilComplete());

  // There should be two DoH lookups for kHostname (both A and AAAA records are
  // queried).
  EXPECT_EQ(doh_server_.QueriesServed(), 2);
  // The requests to the DoH server are pooled, so there should only be one
  // insecure lookup for the DoH server hostname.
  EXPECT_EQ(host_resolver_proc_->insecure_queries_served(), 1u);
  // There should be one non-DoH HTTPS request for the connection to kHostname.
  EXPECT_EQ(test_https_requests_served_, 1u);

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(d.request_status(), 0);
  EXPECT_EQ(d.data_received(), kTestBody);
}

TEST_F(HttpWithDnsOverHttpsTest, EndToEndFail) {
  // Fail all DoH requests.
  doh_server_.SetFailRequests(true);

  // Make a request that will trigger a DoH query.
  TestDelegate d;
  GURL main_url = test_server_.GetURL(kHostname, "/test");
  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      main_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  base::RunLoop().Run();
  EXPECT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(doh_server_.ShutdownAndWaitUntilComplete());

  // No HTTPS connection to the test server will be attempted due to the
  // host resolution error.
  EXPECT_EQ(test_https_requests_served_, 0u);

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(d.request_status(), net::ERR_NAME_NOT_RESOLVED);

  const auto& resolve_error_info = req->response_info().resolve_error_info;
  EXPECT_TRUE(resolve_error_info.is_secure_network_error);
  EXPECT_EQ(resolve_error_info.error, net::ERR_DNS_MALFORMED_RESPONSE);
}

// An end-to-end test of the HTTPS upgrade behavior.
TEST_F(HttpWithDnsOverHttpsTest, HttpsUpgrade) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb, {{"UseDnsHttpsSvcbHttpUpgrade", "true"}});

  GURL https_url = test_server_.GetURL(kHostname, "/test");
  EXPECT_TRUE(https_url.SchemeIs(url::kHttpsScheme));
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  GURL http_url = https_url.ReplaceComponents(replacements);

  doh_server_.AddRecord(BuildTestHttpsServiceRecord(
      dns_util::GetNameForHttpsQuery(url::SchemeHostPort(https_url)),
      /*priority=*/1, /*service_name=*/".", /*params=*/{}));

  // Fetch the http URL.
  TestDelegate d;
  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      http_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  base::RunLoop().Run();
  ASSERT_THAT(d.request_status(), IsOk());

  // The request should have been redirected to https.
  EXPECT_EQ(d.received_redirect_count(), 1);
  EXPECT_EQ(req->url(), https_url);

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(d.request_status(), 0);
  EXPECT_EQ(d.data_received(), kTestBody);
}

// An end-to-end test for requesting a domain with a basic HTTPS record. Expect
// this to exercise connection logic for extra HostResolver results with
// metadata.
TEST_F(HttpWithDnsOverHttpsTest, HttpsMetadata) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb, {{"UseDnsHttpsSvcbHttpUpgrade", "true"}});

  GURL main_url = test_server_.GetURL(kHostname, "/test");
  EXPECT_TRUE(main_url.SchemeIs(url::kHttpsScheme));

  doh_server_.AddRecord(BuildTestHttpsServiceRecord(
      dns_util::GetNameForHttpsQuery(url::SchemeHostPort(main_url)),
      /*priority=*/1, /*service_name=*/".", /*params=*/{}));

  // Fetch the http URL.
  TestDelegate d;

  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      main_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  base::RunLoop().Run();
  ASSERT_THAT(d.request_status(), IsOk());

  // There should be three DoH lookups for kHostname (A, AAAA, and HTTPS).
  EXPECT_EQ(doh_server_.QueriesServed(), 3);

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(d.request_status(), 0);
  EXPECT_EQ(d.data_received(), kTestBody);
}

}  // namespace
}  // namespace net
