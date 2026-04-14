// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <vector>

#include "base/big_endian.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/network_change_notifier.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cookies/site_for_cookies.h"
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
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/log/net_log.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_cache_observer.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_target_type.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_doh_server.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {
namespace {

using net::test::IsError;
using net::test::IsOk;

const char kDohHostname[] = "doh-server.example";
const char kHostname[] = "bar.example.com";
const char kTestBody[] = "<html><body>TEST RESPONSE</body></html>";

class TestHostResolverProc : public HostResolverProc {
 public:
  TestHostResolverProc() : HostResolverProc(nullptr) {}

  int Resolve(const std::string& hostname,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override {
    insecure_queries_served_++;
    *addrlist = AddressList::CreateFromIPAddress(IPAddress(127, 0, 0, 1), 0);
    return OK;
  }

  uint32_t insecure_queries_served() { return insecure_queries_served_; }

 private:
  ~TestHostResolverProc() override = default;
  uint32_t insecure_queries_served_ = 0;
};

// Runs and waits for the DoH probe to complete in automatic mode. The resolver
// must have a single DoH server, and the DoH server must serve addresses for
// `kDohProbeHostname`.
class DohProber : public NetworkChangeNotifier::DNSObserver {
 public:
  explicit DohProber(ContextHostResolver* resolver) : resolver_(resolver) {}

  void ProbeAndWaitForCompletion() {
    std::unique_ptr<HostResolver::ProbeRequest> probe_request =
        resolver_->CreateDohProbeRequest();
    EXPECT_THAT(probe_request->Start(), IsError(ERR_IO_PENDING));
    if (NumAvailableDohServers() == 0) {
      NetworkChangeNotifier::AddDNSObserver(this);
      loop_.Run();
      NetworkChangeNotifier::RemoveDNSObserver(this);
    }
    EXPECT_GT(NumAvailableDohServers(), 0u);
  }

  void OnDNSChanged() override {
    if (NumAvailableDohServers() > 0) {
      loop_.Quit();
    }
  }

 private:
  size_t NumAvailableDohServers() {
    ResolveContext* context = resolver_->resolve_context_for_testing();
    return context->NumAvailableDohServers(
        context->current_session_for_testing());
  }

  raw_ptr<ContextHostResolver> resolver_;
  base::RunLoop loop_;
};

// A test fixture that creates a DoH server with a `URLRequestContext`
// configured to use it.
class DnsOverHttpsIntegrationTest : public TestWithTaskEnvironment {
 public:
  DnsOverHttpsIntegrationTest()
      : DnsOverHttpsIntegrationTest(/*start_server=*/true) {}

  URLRequestContext* context() { return request_context_.get(); }

  void ResetContext(SecureDnsMode mode = SecureDnsMode::kSecure,
                    bool require_network_anonymization_key = false,
                    bool use_post_only_template = true,
                    bool enable_reporting = false) {
    // TODO(crbug.com/40198637): Simplify this.
    HostResolver::ManagerOptions manager_options;
    // Without a DnsConfig, HostResolverManager will not use DoH, even in
    // kSecure mode. See https://crbug.com/1251715. However,
    // DnsClient::BuildEffectiveConfig special-cases overrides that override
    // everything, so that gets around it. Ideally, we would instead mock out a
    // system DnsConfig via the usual pathway.
    manager_options.dns_config_overrides =
        DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
    manager_options.dns_config_overrides.secure_dns_mode = mode;
    manager_options.dns_config_overrides.dns_over_https_config =
        *DnsOverHttpsConfig::FromString(use_post_only_template
                                            ? doh_server_.GetPostOnlyTemplate()
                                            : doh_server_.GetTemplate());
    manager_options.dns_config_overrides.use_local_ipv6 = true;
    auto resolver = HostResolver::CreateStandaloneContextResolver(
        NetLog::Get(), manager_options);

    // Configure `resolver_` to use `host_resolver_proc_` to resolve
    // `doh_server_` itself. Additionally, without an explicit HostResolverProc,
    // HostResolverManager::HaveTestProcOverride disables the built-in DNS
    // client.
    auto* resolver_raw = resolver.get();
    resolver->SetHostResolverSystemParamsForTest(
        HostResolverSystemTask::Params(host_resolver_proc_, 1));

    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_require_network_anonymization_key(
        require_network_anonymization_key);
    context_builder->set_host_resolver(std::move(resolver));

    if (enable_reporting) {
      context_builder->set_network_error_logging_enabled(true);

      auto cert_verifier = std::make_unique<MockCertVerifier>();
      cert_verifier->set_default_result(OK);
      context_builder->SetCertVerifier(std::move(cert_verifier));

      auto reporting_policy = std::make_unique<ReportingPolicy>();
      reporting_policy->delivery_interval = base::Milliseconds(1);
      context_builder->set_reporting_policy(std::move(reporting_policy));
    }

    auto ssl_config_service =
        std::make_unique<TestSSLConfigService>(SSLContextConfig());
    ssl_config_service_ = ssl_config_service.get();
    context_builder->set_ssl_config_service(std::move(ssl_config_service));
    request_context_ = context_builder->Build();

    if (mode == SecureDnsMode::kAutomatic) {
      DohProber prober(resolver_raw);
      prober.ProbeAndWaitForCompletion();
    }
  }

  void AddHostWithEch(const url::SchemeHostPort& host,
                      const IPAddress& address,
                      base::span<const uint8_t> ech_config_list) {
    doh_server_.AddAddressRecord(host.host(), address);
    doh_server_.AddRecord(BuildTestHttpsServiceRecord(
        dns_util::GetNameForHttpsQuery(host),
        /*priority=*/1, /*service_name=*/host.host(),
        {BuildTestHttpsServiceEchConfigParam(ech_config_list)}));
  }

 protected:
  explicit DnsOverHttpsIntegrationTest(bool start_server)
      : host_resolver_proc_(base::MakeRefCounted<TestHostResolverProc>()) {
    if (start_server) {
      doh_server_.SetHostname(kDohHostname);
      EXPECT_TRUE(doh_server_.Start());

      // In `kAutomatic` mode, DoH support depends on a probe for
      // `kDohProbeHostname`.
      doh_server_.AddAddressRecord(kDohProbeHostname,
                                   IPAddress::IPv4Localhost());

      ResetContext();
    }
  }

  TestDohServer doh_server_;
  scoped_refptr<net::TestHostResolverProc> host_resolver_proc_;
  std::unique_ptr<URLRequestContext> request_context_;
  raw_ptr<TestSSLConfigService> ssl_config_service_;
};

// A convenience wrapper over `DnsOverHttpsIntegrationTest` that also starts an
// HTTPS server.
class HttpsWithDnsOverHttpsTest : public DnsOverHttpsIntegrationTest {
 public:
  HttpsWithDnsOverHttpsTest() {
    EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {kHostname};
    https_server_.SetSSLConfig(cert_config);
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&HttpsWithDnsOverHttpsTest::HandleDefaultRequest,
                            base::Unretained(this)));
    EXPECT_TRUE(https_server_.Start());

    doh_server_.AddAddressRecord(kHostname, IPAddress(127, 0, 0, 1));
  }

  std::unique_ptr<test_server::HttpResponse> HandleDefaultRequest(
      const test_server::HttpRequest& request) {
    auto http_response = std::make_unique<test_server::BasicHttpResponse>();
    test_https_requests_served_++;
    http_response->set_content(kTestBody);
    http_response->set_content_type("text/html");
    return std::move(http_response);
  }

 protected:
  EmbeddedTestServer https_server_{EmbeddedTestServer::Type::TYPE_HTTPS};
  uint32_t test_https_requests_served_ = 0;
};

class TestHttpDelegate : public HttpStreamRequest::Delegate {
 public:
  explicit TestHttpDelegate(HttpNetworkSession* session) : session_(session) {}
  ~TestHttpDelegate() override = default;

  void WaitForCompletion(std::unique_ptr<HttpStreamRequest> request) {
    request_ = std::move(request);
    loop_.Run();
  }

  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    stream->Close(false);
    loop_.Quit();
  }

  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {}

  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {}

  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_eror_info) override {}

  void OnCertificateError(int status, const SSLInfo& ssl_info) override {}

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {}

  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override {}

  void OnQuicBroken() override {}

 private:
  raw_ptr<HttpNetworkSession> session_;
  base::RunLoop loop_;
  std::unique_ptr<HttpStreamRequest> request_;
};

// This test sets up a request which will reenter the connection pools by
// triggering a DNS over HTTPS request. It also sets up an idle socket
// which was a precondition for the crash we saw in  https://crbug.com/830917.
TEST_F(HttpsWithDnsOverHttpsTest, EndToEnd) {
  // Create and start http server.
  EmbeddedTestServer http_server(EmbeddedTestServer::Type::TYPE_HTTP);
  http_server.RegisterRequestHandler(
      base::BindRepeating(&HttpsWithDnsOverHttpsTest::HandleDefaultRequest,
                          base::Unretained(this)));
  EXPECT_TRUE(http_server.Start());

  // Set up an idle socket.
  HttpTransactionFactory* transaction_factory =
      request_context_->http_transaction_factory();
  HttpStreamFactory::JobFactory default_job_factory;
  HttpNetworkSession* network_session = transaction_factory->GetSession();
  TestHttpDelegate request_delegate(network_session);

  HttpStreamFactory* factory = network_session->http_stream_factory();
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = http_server.GetURL("localhost", "/preconnect");

  std::unique_ptr<HttpStreamRequest> request(factory->RequestStream(
      request_info, DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
      &request_delegate, false, false, NetLogWithSource()));
  request_delegate.WaitForCompletion(std::move(request));

  size_t idle_socket_count = 0;
  ClientSocketPool::GroupId group_id(
      url::SchemeHostPort(request_info.url), PrivacyMode::PRIVACY_MODE_DISABLED,
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false, handles::kInvalidNetworkHandle);
  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    idle_socket_count =
        network_session->http_stream_pool()
            ->GetOrCreateGroupForTesting(GroupIdToHttpStreamKey(group_id))
            .IdleStreamSocketCount();
  } else {
    idle_socket_count =
        network_session
            ->GetSocketPool(HttpNetworkSession::SocketPoolType::kNormal,
                            ProxyChain::Direct())
            ->IdleSocketCountInGroup(group_id);
  }
  EXPECT_EQ(idle_socket_count, 1u);

  // The domain "localhost" is resolved locally, so no DNS lookups should have
  // occurred.
  EXPECT_EQ(doh_server_.QueriesServed(), 0);
  EXPECT_EQ(host_resolver_proc_->insecure_queries_served(), 0u);
  // A stream was established, but no HTTPS request has been made yet.
  EXPECT_EQ(test_https_requests_served_, 0u);

  // Make a request that will trigger a DoH query as well.
  TestDelegate d;
  GURL main_url = https_server_.GetURL(kHostname, "/test");
  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      main_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(http_server.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(doh_server_.ShutdownAndWaitUntilComplete());

  // There should be three DoH lookups for kHostname (A, AAAA, and HTTPS).
  EXPECT_EQ(doh_server_.QueriesServed(), 3);
  // The requests to the DoH server are pooled, so there should only be one
  // insecure lookup for the DoH server hostname.
  EXPECT_EQ(host_resolver_proc_->insecure_queries_served(), 1u);
  // There should be one non-DoH HTTPS request for the connection to kHostname.
  EXPECT_EQ(test_https_requests_served_, 1u);

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(d.request_status(), 0);
  EXPECT_EQ(d.data_received(), kTestBody);
}

TEST_F(HttpsWithDnsOverHttpsTest, EndToEndWithIsolationInfo) {
  // Reset context with require_network_anonymization_key = true.
  ResetContext(SecureDnsMode::kSecure,
               /*require_network_anonymization_key=*/true);

  // Make a request with a valid IsolationInfo.
  TestDelegate d;
  GURL main_url = https_server_.GetURL(kHostname, "/test");
  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      main_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));

  url::Origin origin = url::Origin::Create(main_url);
  req->set_isolation_info(
      IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, origin,
                            origin, SiteForCookies::FromOrigin(origin)));

  req->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(doh_server_.ShutdownAndWaitUntilComplete());

  // There should be three DoH lookups for kHostname (A, AAAA, and HTTPS).
  EXPECT_EQ(doh_server_.QueriesServed(), 3);

  // We are also implicitly checking that the DoH requests bypassed the
  // IsolationInfo-is-not-empty check in `URLRequest::Start` that occurs when
  // `require_network_anonymization_key` is set since DoH requests use an empty
  // IsolationInfo with a custom partition (which we exempt from the check).
}

TEST_F(HttpsWithDnsOverHttpsTest, EndToEndFail) {
  // Fail all DoH requests.
  doh_server_.SetFailRequests(true);

  // Make a request that will trigger a DoH query.
  TestDelegate d;
  GURL main_url = https_server_.GetURL(kHostname, "/test");
  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      main_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
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
TEST_F(HttpsWithDnsOverHttpsTest, HttpsUpgrade) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});
  ResetContext();

  GURL https_url = https_server_.GetURL(kHostname, "/test");
  EXPECT_TRUE(https_url.SchemeIs(url::kHttpsScheme));
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  GURL http_url = https_url.ReplaceComponents(replacements);

  // `service_name` is `kHostname` rather than "." because "." specifies the
  // query name. For non-defaults ports, the query name uses port prefix naming
  // and does not match the A/AAAA records.
  doh_server_.AddRecord(BuildTestHttpsServiceRecord(
      dns_util::GetNameForHttpsQuery(url::SchemeHostPort(https_url)),
      /*priority=*/1, /*service_name=*/kHostname, /*params=*/{}));

  for (auto mode : {SecureDnsMode::kSecure, SecureDnsMode::kAutomatic}) {
    SCOPED_TRACE(kSecureDnsModes.at(mode));
    ResetContext(mode);

    // Fetch the http URL.
    TestDelegate d;
    std::unique_ptr<URLRequest> req(context()->CreateRequest(
        http_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
    req->Start();
    d.RunUntilComplete();
    ASSERT_THAT(d.request_status(), IsOk());

    // The request should have been redirected to https.
    EXPECT_EQ(d.received_redirect_count(), 1);
    EXPECT_EQ(req->url(), https_url);

    EXPECT_TRUE(d.response_completed());
    EXPECT_EQ(d.request_status(), 0);
    EXPECT_EQ(d.data_received(), kTestBody);
  }
}

// An end-to-end test for requesting a domain with a basic HTTPS record. Expect
// this to exercise connection logic for extra HostResolver results with
// metadata.
TEST_F(HttpsWithDnsOverHttpsTest, HttpsMetadata) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});
  ResetContext();

  GURL main_url = https_server_.GetURL(kHostname, "/test");
  EXPECT_TRUE(main_url.SchemeIs(url::kHttpsScheme));

  doh_server_.AddRecord(BuildTestHttpsServiceRecord(
      dns_util::GetNameForHttpsQuery(url::SchemeHostPort(main_url)),
      /*priority=*/1, /*service_name=*/kHostname, /*params=*/{}));

  // Fetch the http URL.
  TestDelegate d;

  std::unique_ptr<URLRequest> req(context()->CreateRequest(
      main_url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS));
  req->Start();
  d.RunUntilComplete();
  ASSERT_THAT(d.request_status(), IsOk());

  // There should be three DoH lookups for kHostname (A, AAAA, and HTTPS).
  EXPECT_EQ(doh_server_.QueriesServed(), 3);

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(d.request_status(), 0);
  EXPECT_EQ(d.data_received(), kTestBody);
}

TEST_F(DnsOverHttpsIntegrationTest, EncryptedClientHello) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUseDnsHttpsSvcb,
                             {// Disable timeouts.
                              {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
                              {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
                              {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}}}},
      /*disabled_features=*/{});

  // Configure a test server that speaks ECH.
  static constexpr char kRealName[] = "secret.example";
  static constexpr char kPublicName[] = "public.example";
  EmbeddedTestServer::ServerCertificateConfig server_cert_config;
  server_cert_config.dns_names = {kRealName};

  SSLServerConfig ssl_server_config;
  std::vector<uint8_t> ech_config_list;
  ssl_server_config.ech_keys =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/128, &ech_config_list);
  ASSERT_TRUE(ssl_server_config.ech_keys);

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(server_cert_config, ssl_server_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));
  GURL url = test_server.GetURL(kRealName, "/defaultresponse");
  AddHostWithEch(url::SchemeHostPort(url), addr.front().address(),
                 ech_config_list);

  for (bool ech_enabled : {true, false}) {
    SCOPED_TRACE(ech_enabled);

    // Create a new `URLRequestContext`, to ensure there are no cached
    // sockets, etc., from the previous loop iteration.
    ResetContext();

    SSLContextConfig config;
    config.ech_enabled = ech_enabled;
    ssl_config_service_->UpdateSSLConfigAndNotify(config);

    TestDelegate d;
    std::unique_ptr<URLRequest> r = context()->CreateRequest(
        url, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS);
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_THAT(d.request_status(), IsOk());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_EQ(ech_enabled, r->ssl_info().encrypted_client_hello);
  }
}

// Test that, if the DNS returns a stale ECHConfigList (or other key mismatch),
// the client can recover and connect to the server, provided the server can
// handshake as the public name.
TEST_F(DnsOverHttpsIntegrationTest, EncryptedClientHelloStaleKey) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUseDnsHttpsSvcb,
                             {// Disable timeouts.
                              {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
                              {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
                              {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}}}},
      /*disabled_features=*/{});
  ResetContext();

  static constexpr char kRealNameStale[] = "secret1.example";
  static constexpr char kRealNameWrongPublicName[] = "secret2.example";
  static constexpr char kPublicName[] = "public.example";
  static constexpr char kWrongPublicName[] = "wrong-public.example";

  std::vector<uint8_t> ech_config_list, ech_config_list_stale,
      ech_config_list_wrong_public_name;
  bssl::UniquePtr<SSL_ECH_KEYS> ech_keys =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/128, &ech_config_list);
  ASSERT_TRUE(ech_keys);
  ASSERT_TRUE(MakeTestEchKeys(kPublicName, /*max_name_len=*/128,
                              &ech_config_list_stale));
  ASSERT_TRUE(MakeTestEchKeys(kWrongPublicName, /*max_name_len=*/128,
                              &ech_config_list_wrong_public_name));

  // Configure an ECH-supporting server that can speak for all names except
  // `kWrongPublicName`.
  EmbeddedTestServer::ServerCertificateConfig server_cert_config;
  server_cert_config.dns_names = {kRealNameStale, kRealNameWrongPublicName,
                                  kPublicName};
  SSLServerConfig ssl_server_config;
  ssl_server_config.ech_keys = std::move(ech_keys);
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(server_cert_config, ssl_server_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));
  GURL url_stale = test_server.GetURL(kRealNameStale, "/defaultresponse");
  GURL url_wrong_public_name =
      test_server.GetURL(kRealNameWrongPublicName, "/defaultresponse");
  AddHostWithEch(url::SchemeHostPort(url_stale), addr.front().address(),
                 ech_config_list_stale);
  AddHostWithEch(url::SchemeHostPort(url_wrong_public_name),
                 addr.front().address(), ech_config_list_wrong_public_name);

  // Connecting to `url_stale` should succeed. Although the server will not
  // decrypt the ClientHello, it can handshake as `kPublicName` and provide new
  // keys for the client to use.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r = context()->CreateRequest(
        url_stale, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS);
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_THAT(d.request_status(), IsOk());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_TRUE(r->ssl_info().encrypted_client_hello);
  }

  // Connecting to `url_wrong_public_name` should fail. The server can neither
  // decrypt the ClientHello, nor handshake as `kWrongPublicName`.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r =
        context()->CreateRequest(url_wrong_public_name, DEFAULT_PRIORITY, &d,
                                 TRAFFIC_ANNOTATION_FOR_TESTS);
    r->Start();
    EXPECT_TRUE(r->is_pending());

    d.RunUntilComplete();

    EXPECT_THAT(d.request_status(),
                IsError(ERR_ECH_FALLBACK_CERTIFICATE_INVALID));
  }
}

TEST_F(DnsOverHttpsIntegrationTest, EncryptedClientHelloFallback) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUseDnsHttpsSvcb,
                             {// Disable timeouts.
                              {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
                              {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
                              {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}}}},
      /*disabled_features=*/{});
  ResetContext();

  static constexpr char kRealNameStale[] = "secret1.example";
  static constexpr char kRealNameWrongPublicName[] = "secret2.example";
  static constexpr char kPublicName[] = "public.example";
  static constexpr char kWrongPublicName[] = "wrong-public.example";

  std::vector<uint8_t> ech_config_list_stale, ech_config_list_wrong_public_name;
  ASSERT_TRUE(MakeTestEchKeys(kPublicName, /*max_name_len=*/128,
                              &ech_config_list_stale));
  ASSERT_TRUE(MakeTestEchKeys(kWrongPublicName, /*max_name_len=*/128,
                              &ech_config_list_wrong_public_name));

  // Configure a server, without ECH, that can speak for all names except
  // `kWrongPublicName`.
  EmbeddedTestServer::ServerCertificateConfig server_cert_config;
  server_cert_config.dns_names = {kRealNameStale, kRealNameWrongPublicName,
                                  kPublicName};
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(server_cert_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));
  GURL url_stale = test_server.GetURL(kRealNameStale, "/defaultresponse");
  GURL url_wrong_public_name =
      test_server.GetURL(kRealNameWrongPublicName, "/defaultresponse");
  AddHostWithEch(url::SchemeHostPort(url_stale), addr.front().address(),
                 ech_config_list_stale);
  AddHostWithEch(url::SchemeHostPort(url_wrong_public_name),
                 addr.front().address(), ech_config_list_wrong_public_name);

  // Connecting to `url_stale` should succeed. Although the server will not
  // decrypt the ClientHello, it can handshake as `kPublicName` and trigger an
  // authenticated fallback.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r = context()->CreateRequest(
        url_stale, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS);
    r->Start();
    EXPECT_TRUE(r->is_pending());
    d.RunUntilComplete();
    EXPECT_THAT(d.request_status(), IsOk());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_FALSE(r->ssl_info().encrypted_client_hello);
  }

  // Connecting to `url_wrong_public_name` should fail. The server can neither
  // decrypt the ClientHello, nor handshake as `kWrongPublicName`.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r =
        context()->CreateRequest(url_wrong_public_name, DEFAULT_PRIORITY, &d,
                                 TRAFFIC_ANNOTATION_FOR_TESTS);
    r->Start();
    EXPECT_TRUE(r->is_pending());
    d.RunUntilComplete();
    EXPECT_THAT(d.request_status(),
                IsError(ERR_ECH_FALLBACK_CERTIFICATE_INVALID));
  }
}

TEST_F(DnsOverHttpsIntegrationTest, EncryptedClientHelloFallbackTLS12) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUseDnsHttpsSvcb,
                             {// Disable timeouts.
                              {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
                              {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
                              {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}}}},
      /*disabled_features=*/{});
  ResetContext();

  static constexpr char kRealNameStale[] = "secret1.example";
  static constexpr char kRealNameWrongPublicName[] = "secret2.example";
  static constexpr char kPublicName[] = "public.example";
  static constexpr char kWrongPublicName[] = "wrong-public.example";

  std::vector<uint8_t> ech_config_list_stale, ech_config_list_wrong_public_name;
  ASSERT_TRUE(MakeTestEchKeys(kPublicName, /*max_name_len=*/128,
                              &ech_config_list_stale));
  ASSERT_TRUE(MakeTestEchKeys(kWrongPublicName, /*max_name_len=*/128,
                              &ech_config_list_wrong_public_name));

  // Configure a server, without ECH or TLS 1.3, that can speak for all names
  // except `kWrongPublicName`.
  EmbeddedTestServer::ServerCertificateConfig server_cert_config;
  server_cert_config.dns_names = {kRealNameStale, kRealNameWrongPublicName,
                                  kPublicName};
  SSLServerConfig ssl_server_config;
  ssl_server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTPS);
  test_server.SetSSLConfig(server_cert_config, ssl_server_config);
  RegisterDefaultHandlers(&test_server);
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));
  GURL url_stale = test_server.GetURL(kRealNameStale, "/defaultresponse");
  GURL url_wrong_public_name =
      test_server.GetURL(kRealNameWrongPublicName, "/defaultresponse");
  AddHostWithEch(url::SchemeHostPort(url_stale), addr.front().address(),
                 ech_config_list_stale);
  AddHostWithEch(url::SchemeHostPort(url_wrong_public_name),
                 addr.front().address(), ech_config_list_wrong_public_name);

  // Connecting to `url_stale` should succeed. Although the server will not
  // decrypt the ClientHello, it can handshake as `kPublicName` and trigger an
  // authenticated fallback.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r = context()->CreateRequest(
        url_stale, DEFAULT_PRIORITY, &d, TRAFFIC_ANNOTATION_FOR_TESTS);
    r->Start();
    EXPECT_TRUE(r->is_pending());
    d.RunUntilComplete();
    EXPECT_THAT(d.request_status(), IsOk());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
    EXPECT_FALSE(r->ssl_info().encrypted_client_hello);
  }

  // Connecting to `url_wrong_public_name` should fail. The server can neither
  // decrypt the ClientHello, nor handshake as `kWrongPublicName`.
  {
    TestDelegate d;
    std::unique_ptr<URLRequest> r =
        context()->CreateRequest(url_wrong_public_name, DEFAULT_PRIORITY, &d,
                                 TRAFFIC_ANNOTATION_FOR_TESTS);
    r->Start();
    EXPECT_TRUE(r->is_pending());
    d.RunUntilComplete();
    EXPECT_THAT(d.request_status(),
                IsError(ERR_ECH_FALLBACK_CERTIFICATE_INVALID));
  }
}

class DnsOverHttpsReportingTest : public DnsOverHttpsIntegrationTest {
 public:
  DnsOverHttpsReportingTest()
      : DnsOverHttpsIntegrationTest(/*start_server=*/false) {
    feature_list_.InitWithFeatures(
        {features::kPartitionConnectionsByNetworkIsolationKey},
        // Disable HTTPS record lookups to simplify what requests we expect.
        {features::kUseDnsHttpsSvcb});

    EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {kHostname};
    cert_config.ip_addresses = {IPAddress::IPv4Localhost()};
    https_server_.SetSSLConfig(cert_config);
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&DnsOverHttpsReportingTest::HandleReportingRequest,
                            base::Unretained(this)));
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&DnsOverHttpsReportingTest::HandleDefaultRequest));
    EXPECT_TRUE(https_server_.Start());

    doh_server_.SetHostname(kDohHostname);
    doh_server_.AddAddressRecord(kHostname, IPAddress::IPv4Localhost());

    // Use the HTTPS server IP address as the report destination so we don't
    // have to worry about DNS resolution while the test DoH server is
    // potentially configured to respond with errors instead of addresses.
    std::string report_to =
        "{\"group\":\"group\",\"max_age\":86400,\"endpoints\":[{\"url\":\"" +
        https_server_.GetURL("/upload").spec() + "\"}]}";
    doh_server_.AddCustomResponseHeader("Report-To", report_to);
    doh_server_.AddCustomResponseHeader(
        "NEL",
        "{\"report_to\":\"group\",\"max_age\":86400, "
        "\"success_fraction\":0.0, "
        "\"failure_fraction\":1.0}");

    EXPECT_TRUE(doh_server_.Start());

    ResetContext(SecureDnsMode::kSecure,
                 /*require_network_anonymization_key=*/false,
                 /*use_post_only_template=*/false,
                 /*enable_reporting=*/true);
  }

  ~DnsOverHttpsReportingTest() override {
    EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
  }

  std::unique_ptr<test_server::HttpResponse> HandleReportingRequest(
      const test_server::HttpRequest& request) {
    // Handle the preflight that occurs to check whether sending reports is
    // allowed.
    if (request.method == test_server::METHOD_OPTIONS) {
      auto http_response = std::make_unique<test_server::BasicHttpResponse>();
      http_response->set_code(HTTP_OK);
      http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
      http_response->AddCustomHeader("Access-Control-Allow-Headers",
                                     "content-type");
      return http_response;
    }

    // Handle reports being sent.
    if (request.method == test_server::METHOD_POST &&
        request.relative_url == "/upload") {
      auto http_response = std::make_unique<test_server::BasicHttpResponse>();
      http_response->set_code(HTTP_OK);
      // Note that failing DoH queries can get retried and possibly cause
      // multiple reports to be generated, but for our purposes we only care
      // whether at least one report was uploaded. Thus, we won't make any
      // assumptions here about the previous value of `report_uploaded_`.
      set_report_uploaded(true);
      return http_response;
    }
    return nullptr;
  }

  static std::unique_ptr<test_server::HttpResponse> HandleDefaultRequest(
      const test_server::HttpRequest& request) {
    auto http_response = std::make_unique<test_server::BasicHttpResponse>();
    http_response->set_code(HTTP_OK);
    http_response->set_content(kTestBody);
    http_response->set_content_type("text/html");
    return http_response;
  }

  bool report_uploaded() {
    base::AutoLock lock(report_uploaded_lock_);
    return report_uploaded_;
  }

  void set_report_uploaded(bool report_uploaded) {
    base::AutoLock lock(report_uploaded_lock_);
    report_uploaded_ = report_uploaded;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  EmbeddedTestServer https_server_{EmbeddedTestServer::Type::TYPE_HTTPS};
  base::Lock report_uploaded_lock_;
  bool report_uploaded_ GUARDED_BY(report_uploaded_lock_) = false;
};

// Regression test for crbug.com/500671397. Ensures that if a DoH query
// results in a reporting API upload it doesn't trigger a CHECK.
TEST_F(DnsOverHttpsReportingTest, ReportingApi) {
  class TestObserver : public ReportingCacheObserver {
   public:
    void OnReportAdded(const ReportingReport* report) override {
      EXPECT_EQ(
          report->network_anonymization_key,
          DnsHTTPAttempt::GetDohIsolationInfo().network_anonymization_key());
      // Note that failing DoH queries can get retried and possibly cause
      // multiple reports to be generated, but for our purposes we only care
      // whether at least one report was added. Thus, we won't make any
      // assumptions here about the previous value of `report_added_`.
      report_added_ = true;
    }
    bool report_added() const { return report_added_; }

   private:
    bool report_added_ = false;
  };
  TestObserver observer;
  context()->reporting_service()->GetContextForTesting()->AddCacheObserver(
      &observer);

  // 1. Perform a DoH query to get a NEL policy.
  {
    GURL url = https_server_.GetURL(kHostname, "/foo");
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context()->CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    request->Start();
    delegate.RunUntilComplete();
    ASSERT_THAT(delegate.request_status(), IsOk());
  }

  // 2. Perform a failing DoH query to trigger one or more NEL reports (the DoH
  // query may be retried several times, resulting in multiple reports possibly
  // being generated).
  doh_server_.SetFailRequests(true);
  {
    GURL url = https_server_.GetURL("fail.com", "/foo");
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context()->CreateRequest(
        url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    request->Start();
    delegate.RunUntilComplete();
    EXPECT_THAT(delegate.request_status(), IsError(ERR_NAME_NOT_RESOLVED));
  }

  ReportingService* reporting_service = context()->reporting_service();
  ReportingCache* cache = reporting_service->GetContextForTesting()->cache();

  // 3. Wait for the report to be queued.
  EXPECT_TRUE(base::test::RunUntil([&]() { return observer.report_added(); }));

  // 4. Wait for the report to be delivered (and thus removed from the cache).
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return cache->GetFullReportCountForTesting() == 0; }));
  EXPECT_TRUE(report_uploaded());

  // 5. The test passes if no CHECKs were encountered at this point.

  context()->reporting_service()->GetContextForTesting()->RemoveCacheObserver(
      &observer);
}
}  // namespace
}  // namespace net
