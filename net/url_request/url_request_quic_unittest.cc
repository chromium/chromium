// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>
#include <tuple>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/load_timing_info.h"
#include "net/base/network_delegate.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/crypto_test_utils_chromium.h"
#include "net/quic/quic_context.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_dispatcher.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_memory_cache_backend.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_simple_dispatcher.h"
#include "net/tools/quic/quic_simple_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

// This must match the certificate used (quic-chain.pem and quic-leaf-cert.key).
constexpr char kTestServerHost[] = "test.example.com";

// Hosts that do not support QUIC. In some tests, an alt service record for this
// host is added, pointing at `kTestServerHost`.
constexpr char kOtherHost[] = "other.test";
constexpr char kOtherHost2[] = "other2.test";

// Used as a simple response from the server.
constexpr char kHelloPath[] = "/hello.txt";
constexpr char kHelloBodyValue[] = "Hello from QUIC Server";
constexpr int kHelloStatus = 200;

class URLRequestQuicTest : public TestWithTaskEnvironment,
                           public ::testing::WithParamInterface<
                               std::tuple<quic::ParsedQuicVersion,
                                          bool /*happy_eyeballs_v3_enabled*/>> {
 public:
  // Use meaningful test suffixes instead of /0, /1, etc.
  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      return base::StrCat(
          {quic::ParsedQuicVersionToString(std::get<0>(info.param)), "_AND_",
           std::get<bool>(info.param) ? "HEv3" : "HEv1"});
    }
  };

  // When `force_quic` is true, QUIC is forced on `kTestServerHost`.
  explicit URLRequestQuicTest(bool force_quic = true)
      : force_quic_(force_quic),
        context_builder_(CreateTestURLRequestContextBuilder()) {
    if (happy_eyeballs_v3_enabled()) {
      feature_list_.InitAndEnableFeature(features::kHappyEyeballsV3);
    } else {
      feature_list_.InitAndDisableFeature(features::kHappyEyeballsV3);
    }

    QuicEnableVersion(version());
    StartQuicServer(version());

    HttpNetworkSessionParams params;
    CertVerifyResult verify_result;
    // When forcing QUIC, the known root check is bypassed, but when not doing
    // so, need this to be true so that check to pass.
    verify_result.is_issued_by_known_root = true;
    verify_result.verified_cert =
        ImportCertFromFile(GetTestCertsDirectory(), "quic-chain.pem");
    auto cert_verifier = std::make_unique<MockCertVerifier>();
    cert_verifier->AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           kTestServerHost, verify_result, OK);
    auto quic_context = std::make_unique<QuicContext>();
    quic_context->params()->supported_versions = {version()};
    // To simplify the test, and avoid the race with the HTTP request, we
    // generally force QUIC for these requests.
    if (force_quic_) {
      quic_context->params()->origins_to_force_quic_on.insert(
          url::SchemeHostPort("https", kTestServerHost, 443));
    }
    context_builder_->set_quic_context(std::move(quic_context));
    params.enable_quic = true;
    // Allow ephemeral ports to be used with alt services stored in
    // HttpServerProperties, so tests can test alt service behavior.
    params.enable_user_alternate_protocol_ports = true;
    context_builder_->set_host_resolver(MakeMappedHostResolver());
    context_builder_->set_http_network_session_params(params);
    context_builder_->SetCertVerifier(std::move(cert_verifier));
    context_builder_->set_net_log(NetLog::Get());
  }

  void TearDown() override {
    if (server_) {
      server_->Shutdown();
      base::RunLoop().RunUntilIdle();
    }
  }

  URLRequestContextBuilder* context_builder() { return context_builder_.get(); }

  std::unique_ptr<URLRequestContext> BuildContext() {
    return context_builder_->Build();
  }

  static std::unique_ptr<URLRequest> CreateRequest(
      URLRequestContext* context,
      const GURL& url,
      URLRequest::Delegate* delegate) {
    return context->CreateRequest(url, DEFAULT_PRIORITY, delegate,
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // Returns the port that `server_` is listening on. `server_` must be non-null
  // and have been started.
  uint16_t server_port() const { return server_->server_address().port(); }

  quic::ParsedQuicVersion version() const {
    return std::get<quic::ParsedQuicVersion>(GetParam());
  }

  bool happy_eyeballs_v3_enabled() const { return std::get<bool>(GetParam()); }

 protected:
  // Returns a fully-qualified URL for `path` on the test server.
  GURL UrlFromPath(std::string_view path) {
    GURL url(base::StrCat({"https://", kTestServerHost, path}));
    CHECK(url.is_valid());
    return url;
  }

  void SetDelay(std::string_view host,
                std::string_view path,
                base::TimeDelta delay) {
    memory_cache_backend_.SetResponseDelay(
        host, path,
        quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  }

  void StartQuicServer(quic::ParsedQuicVersion version) {
    // Set up in-memory cache.

    // Add the simply hello response.
    memory_cache_backend_.AddSimpleResponse(kTestServerHost, kHelloPath,
                                            kHelloStatus, kHelloBodyValue);

    quic::QuicConfig config;
    // Set up server certs.
    server_ = std::make_unique<QuicSimpleServer>(
        net::test::ProofSourceForTestingChromium(), config,
        quic::QuicCryptoServerConfig::ConfigOptions(),
        quic::ParsedQuicVersionVector{version}, &memory_cache_backend_);
    int rv =
        server_->Listen(net::IPEndPoint(net::IPAddress::IPv4AllZeros(), 0));
    EXPECT_GE(rv, 0) << "Quic server fails to start";
  }

  // Creates a HostResolver that resolves `test.example.com` to 127.0.0.1, and
  // rewrites the port of requests to that host to the port of `server_`.
  std::unique_ptr<HostResolver> MakeMappedHostResolver() const {
    auto resolver = std::make_unique<MockHostResolver>(
        /*default_result=*/ERR_NAME_NOT_RESOLVED);
    resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    auto host_resolver =
        std::make_unique<MappedHostResolver>(std::move(resolver));
    // Use a mapped host resolver so that request for test.example.com
    // reach the server running on localhost.
    std::string map_rule = "MAP test.example.com test.example.com:" +
                           base::NumberToString(server_port());
    EXPECT_TRUE(host_resolver->AddRuleFromString(map_rule));
    return host_resolver;
  }

  const bool force_quic_;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<QuicSimpleServer> server_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
  std::unique_ptr<URLRequestContextBuilder> context_builder_;
  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
};

class URLRequestQuicWithTcpTest : public URLRequestQuicTest {
 public:
  // Don't use force QUIC or a mapped host resolver for these. That does mean
  // it's possible to get a TCP error instead of a QUIC error, so tests should
  // tolerate both types of failures, but it makes tests able to more thorougly
  // test production code behavior.
  URLRequestQuicWithTcpTest() : URLRequestQuicTest(/*force_quic=*/false) {}

  // Replaces the HostResolver, configuring an HTTPS record for
  // `kTestServerHost`. Must be called before BuildContext().
  void SetUpHttpsRecord() {
    auto host_resolver = std::make_unique<MockHostResolver>(
        /*default_result=*/ERR_NAME_NOT_RESOLVED);

    // Set up HTTPS record for `kTestServerHost`.
    HostResolverEndpointResult endpoint_result;
    endpoint_result.ip_endpoints = {
        IPEndPoint(IPAddress::IPv4Localhost(), server_port())};
    endpoint_result.metadata.supported_protocol_alpns = {
        quic::AlpnForVersion(version())};
    std::vector<HostResolverEndpointResult> endpoints;
    endpoints.push_back(endpoint_result);
    host_resolver->rules()->AddRule(
        kTestServerHost, MockHostResolverBase::RuleResolver::RuleResult(
                             std::move(endpoints),
                             /*aliases=*/{kTestServerHost}));

    context_builder_->set_host_resolver(std::move(host_resolver));
  }

  // Replaces the HostResolver, making the passed in hostname resolve to
  // localhost. Must be called before BuildContext().
  void SetUpLocalhostDnsRecord(std::string_view host) {
    auto host_resolver = std::make_unique<MockHostResolver>(
        /*default_result=*/ERR_NAME_NOT_RESOLVED);
    host_resolver->rules()->AddRule(host, "127.0.0.1");
    context_builder_->set_host_resolver(std::move(host_resolver));
  }

  // Sets up alt service entry directing requests for `source` (on any port)
  // to `dest` on the port that `server_` is listening on. It's important for
  // some tests to use a hostname for `dest` instead of a localhost IP, because
  // the the test cert is only usable when it matches the destination.
  void ConfigureAltService(URLRequestContext& context,
                           std::string_view source,
                           std::string_view dest) {
    url::SchemeHostPort alt_server(UrlFromHostAndPath(source, "/"));
    base::Time expiration = base::Time::Now() + base::Days(1);
    AlternativeService alternative_service(NextProto::kProtoQUIC,
                                           HostPortPair(dest, server_port()));
    context.http_server_properties()->SetQuicAlternativeService(
        alt_server, NetworkAnonymizationKey(), alternative_service, expiration,
        {version()});
  }

  // Called when a request should either fail with ERR_NAME_NOT_RESOLVED or a
  // ERR_QUIC_PROTOCOL_ERROR, due to an incorrect cert, depending on whether
  // the TCP or QUIC connection attempt fails second. Ideally, we'd want the
  // QUIC one to fail second, so this could check the QUIC error details, but
  // that's a difficult thing to guarantee.
  static void ExpectQuicCertErrorOrNameNotResolved(
      const URLRequest& request,
      const TestDelegate& delegate) {
    ASSERT_THAT(delegate.request_status(),
                ::testing::AnyOf(test::IsError(ERR_NAME_NOT_RESOLVED),
                                 test::IsError(ERR_QUIC_PROTOCOL_ERROR)));
    if (delegate.request_status() == ERR_NAME_NOT_RESOLVED) {
      return;
    }

    NetErrorDetails details;
    request.PopulateNetErrorDetails(&details);
    EXPECT_EQ(details.quic_connection_error,
              quic::QUIC_TLS_CERTIFICATE_UNKNOWN);
  }

  // URLRequestQuicWithTcpTests use the QUIC server's port in URLs. This is
  // detect a specific bug where HEv3 would merge alt-service and
  // non-alt-service requests when the alt-service port of requests for one
  // origin is the same as the destination port for other requests made directly
  // to the alt-service origin (which also matches the port in HTTPS DNS
  // records).
  // See https://crbug.com/455891789
  GURL UrlFromPathWithPort(std::string_view path) {
    GURL::Replacements replacements;
    std::string port = base::ToString(server_port());
    replacements.SetPortStr(port);
    GURL url = UrlFromPath(path).ReplaceComponents(replacements);
    CHECK(url.is_valid());
    return url;
  }

  static GURL UrlFromHostAndPath(std::string_view host, std::string_view path) {
    GURL url(base::StrCat({"https://", host, path}));
    CHECK(url.is_valid());
    return url;
  }

  static GURL OtherHostUrlFromPath(std::string_view path) {
    return UrlFromHostAndPath(kOtherHost, path);
  }
};

// A URLRequest::Delegate that checks LoadTimingInfo when response headers are
// received.
class CheckLoadTimingDelegate : public TestDelegate {
 public:
  explicit CheckLoadTimingDelegate(bool session_reused)
      : session_reused_(session_reused) {}

  CheckLoadTimingDelegate(const CheckLoadTimingDelegate&) = delete;
  CheckLoadTimingDelegate& operator=(const CheckLoadTimingDelegate&) = delete;

  void OnResponseStarted(URLRequest* request, int error) override {
    TestDelegate::OnResponseStarted(request, error);
    LoadTimingInfo load_timing_info;
    request->GetLoadTimingInfo(&load_timing_info);
    assertLoadTimingValid(load_timing_info, session_reused_);
  }

 private:
  void assertLoadTimingValid(const LoadTimingInfo& load_timing_info,
                             bool session_reused) {
    EXPECT_EQ(session_reused, load_timing_info.socket_reused);

    // If |session_reused| is true, these fields should all be null, non-null
    // otherwise.
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.connect_start.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.connect_end.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.ssl_start.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.ssl_end.is_null());
    EXPECT_EQ(load_timing_info.connect_timing.connect_start,
              load_timing_info.connect_timing.ssl_start);
    EXPECT_EQ(load_timing_info.connect_timing.connect_end,
              load_timing_info.connect_timing.ssl_end);
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.domain_lookup_start.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.domain_lookup_end.is_null());
  }

  bool session_reused_;
};

// A TestNetworkDelegate that invokes |all_requests_completed_callback| when
// |num_expected_requests| requests are completed.
class WaitForCompletionNetworkDelegate : public net::TestNetworkDelegate {
 public:
  WaitForCompletionNetworkDelegate(
      base::OnceClosure all_requests_completed_callback,
      size_t num_expected_requests)
      : all_requests_completed_callback_(
            std::move(all_requests_completed_callback)),
        num_expected_requests_(num_expected_requests) {}

  WaitForCompletionNetworkDelegate(const WaitForCompletionNetworkDelegate&) =
      delete;
  WaitForCompletionNetworkDelegate& operator=(
      const WaitForCompletionNetworkDelegate&) = delete;

  void OnCompleted(URLRequest* request, bool started, int net_error) override {
    net::TestNetworkDelegate::OnCompleted(request, started, net_error);
    num_expected_requests_--;
    if (num_expected_requests_ == 0)
      std::move(all_requests_completed_callback_).Run();
  }

 private:
  base::OnceClosure all_requests_completed_callback_;
  size_t num_expected_requests_;
};

INSTANTIATE_TEST_SUITE_P(
    Version,
    URLRequestQuicTest,
    ::testing::Combine(::testing::ValuesIn(AllSupportedQuicVersions()),
                       ::testing::Bool()),
    URLRequestQuicTest::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(
    Version,
    URLRequestQuicWithTcpTest,
    ::testing::Combine(::testing::ValuesIn(AllSupportedQuicVersions()),
                       ::testing::Bool()),
    URLRequestQuicTest::PrintToStringParamName());

TEST_P(URLRequestQuicTest, TestGetRequest) {
  auto context = BuildContext();
  CheckLoadTimingDelegate delegate(false);
  std::unique_ptr<URLRequest> request =
      CreateRequest(context.get(), UrlFromPath(kHelloPath), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_EQ(OK, delegate.request_status());
  EXPECT_EQ(kHelloBodyValue, delegate.data_received());
  EXPECT_TRUE(request->ssl_info().is_valid());
}

// Tests that if two requests use the same QUIC session, the second request
// should not have |LoadTimingInfo::connect_timing|.
TEST_P(URLRequestQuicTest, TestTwoRequests) {
  base::RunLoop run_loop;
  context_builder()->set_network_delegate(
      std::make_unique<WaitForCompletionNetworkDelegate>(
          run_loop.QuitClosure(), /*num_expected_requests=*/2));
  auto context = BuildContext();

  GURL url = UrlFromPath(kHelloPath);
  auto isolation_info =
      IsolationInfo::CreateForInternalRequest(url::Origin::Create(url));

  CheckLoadTimingDelegate delegate(false);
  delegate.set_on_complete(base::DoNothing());
  std::unique_ptr<URLRequest> request =
      CreateRequest(context.get(), url, &delegate);
  request->set_isolation_info(isolation_info);

  CheckLoadTimingDelegate delegate2(true);
  delegate2.set_on_complete(base::DoNothing());
  std::unique_ptr<URLRequest> request2 =
      CreateRequest(context.get(), url, &delegate2);
  request2->set_isolation_info(isolation_info);

  request->Start();
  request2->Start();
  ASSERT_TRUE(request->is_pending());
  ASSERT_TRUE(request2->is_pending());
  run_loop.Run();

  EXPECT_EQ(OK, delegate.request_status());
  EXPECT_EQ(OK, delegate2.request_status());
  EXPECT_EQ(kHelloBodyValue, delegate.data_received());
  EXPECT_EQ(kHelloBodyValue, delegate2.data_received());
}

TEST_P(URLRequestQuicTest, RequestHeadersCallback) {
  auto context = BuildContext();
  HttpRawRequestHeaders raw_headers;
  TestDelegate delegate;
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");

  std::unique_ptr<URLRequest> request =
      CreateRequest(context.get(), UrlFromPath(kHelloPath), &delegate);

  request->SetExtraRequestHeaders(extra_headers);
  request->SetRequestHeadersCallback(
      base::BindLambdaForTesting([&](HttpRawRequestHeaders raw_headers) {
        // This should be invoked before the request is completed, or any bytes
        // are read.
        EXPECT_FALSE(delegate.response_completed());
        EXPECT_FALSE(delegate.bytes_received());

        EXPECT_FALSE(raw_headers.headers().empty());
        std::string value;
        EXPECT_TRUE(raw_headers.FindHeaderForTest("x-foo", &value));
        EXPECT_EQ("bar", value);
        EXPECT_TRUE(raw_headers.FindHeaderForTest("accept-encoding", &value));
        EXPECT_EQ("gzip, deflate", value);
        EXPECT_TRUE(raw_headers.FindHeaderForTest(":path", &value));
        EXPECT_EQ("/hello.txt", value);
        EXPECT_TRUE(raw_headers.FindHeaderForTest(":authority", &value));
        EXPECT_EQ("test.example.com", value);
        EXPECT_TRUE(raw_headers.request_line().empty());
      }));
  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();
  EXPECT_EQ(OK, delegate.request_status());
}

TEST_P(URLRequestQuicTest, DelayedResponseStart) {
  auto context = BuildContext();
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      CreateRequest(context.get(), UrlFromPath(kHelloPath), &delegate);

  constexpr auto delay = base::Milliseconds(300);

  this->SetDelay(kTestServerHost, kHelloPath, delay);
  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();
  LoadTimingInfo timing_info;
  request->GetLoadTimingInfo(&timing_info);
  EXPECT_EQ(OK, delegate.request_status());
  EXPECT_GE((timing_info.receive_headers_start - timing_info.request_start),
            delay);
  EXPECT_GE(timing_info.receive_non_informational_headers_start,
            timing_info.receive_headers_start);
}

// Checks that an alt service request fails when the cert is wrong. Tests with
// mock sockets entirely skip checking certs when establishing connections, so
// it's good to have a test at this layer, using a "real" MockCertVerifier.
TEST_P(URLRequestQuicWithTcpTest, AltServiceWrongCert) {
  // An HTTPS record should have no impact on whether this test succeeds or
  // fails.
  SetUpHttpsRecord();

  auto context = BuildContext();
  ConfigureAltService(*context, kOtherHost, kTestServerHost);

  GURL alt_url = OtherHostUrlFromPath(kHelloPath);

  base::RunLoop run_loop;
  TestDelegate delegate;
  delegate.set_on_complete(run_loop.QuitClosure());
  std::unique_ptr<URLRequest> request =
      CreateRequest(context.get(), alt_url, &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  run_loop.Run();

  ExpectQuicCertErrorOrNameNotResolved(*request, delegate);
}

// Checks that an alt service request succeeds when the cert is correct. In this
// case, the `kTestServerHost` alt service entry points to `kOtherHost`, which
// is the only hostname that resolves (and serves a response using a
// `kTestServerHost` cert).
TEST_P(URLRequestQuicWithTcpTest, AltServiceRightCert) {
  SetUpLocalhostDnsRecord(kOtherHost);
  auto context = BuildContext();
  ConfigureAltService(*context, kTestServerHost, kOtherHost);

  GURL url = UrlFromPath(kHelloPath);

  base::RunLoop run_loop;
  TestDelegate delegate;
  delegate.set_on_complete(run_loop.QuitClosure());
  std::unique_ptr<URLRequest> request =
      CreateRequest(context.get(), url, &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  run_loop.Run();

  EXPECT_EQ(OK, delegate.request_status());
  EXPECT_EQ(kHelloBodyValue, delegate.data_received());
}

// Tests that the alt service destination checks block alt-service requests from
// reusing a non-alt-service QUIC session with the same destination, when the
// connection attempts are both alive at once.
TEST_P(URLRequestQuicWithTcpTest,
       AltServiceWrongCertSimultaneousNonAltServiceQuicAttempt) {
  if (happy_eyeballs_v3_enabled()) {
    // TODO(crbug.com/455891789): This case is currently broken in the HEv3
    // case. Fix case and enable test.
    GTEST_SKIP();
  }

  SetUpHttpsRecord();
  auto context = BuildContext();
  ConfigureAltService(*context, kOtherHost, kTestServerHost);

  GURL url = UrlFromPathWithPort(kHelloPath);
  GURL alt_url = OtherHostUrlFromPath(kHelloPath);

  base::RunLoop run_loop1;
  CheckLoadTimingDelegate delegate1(/*session_reused=*/false);
  delegate1.set_on_complete(run_loop1.QuitClosure());
  std::unique_ptr<URLRequest> request1 =
      CreateRequest(context.get(), url, &delegate1);

  base::RunLoop run_loop2;
  TestDelegate delegate2;
  delegate2.set_on_complete(run_loop2.QuitClosure());
  std::unique_ptr<URLRequest> request2 =
      CreateRequest(context.get(), alt_url, &delegate2);

  request1->Start();
  request2->Start();
  ASSERT_TRUE(request1->is_pending());
  ASSERT_TRUE(request2->is_pending());
  run_loop1.Run();
  run_loop2.Run();

  EXPECT_EQ(OK, delegate1.request_status());
  EXPECT_EQ(kHelloBodyValue, delegate1.data_received());
  ExpectQuicCertErrorOrNameNotResolved(*request2, delegate2);
}

// Same as above, but with the request to the host with the alt-service
// destination started first.
TEST_P(URLRequestQuicWithTcpTest,
       AltServiceWrongCertSimultaneousNonAltServiceQuicAttemptReverseOrder) {
  if (happy_eyeballs_v3_enabled()) {
    // TODO(crbug.com/455891789): This case is currently broken in the HEv3
    // case. Fix case and enable test.
    GTEST_SKIP();
  }

  SetUpHttpsRecord();
  auto context = BuildContext();
  ConfigureAltService(*context, kOtherHost, kTestServerHost);

  GURL url = UrlFromPathWithPort(kHelloPath);
  GURL alt_url = OtherHostUrlFromPath(kHelloPath);

  base::RunLoop run_loop1;
  CheckLoadTimingDelegate delegate1(/*session_reused=*/false);
  delegate1.set_on_complete(run_loop1.QuitClosure());
  std::unique_ptr<URLRequest> request1 =
      CreateRequest(context.get(), url, &delegate1);

  base::RunLoop run_loop2;
  TestDelegate delegate2;
  delegate2.set_on_complete(run_loop2.QuitClosure());
  std::unique_ptr<URLRequest> request2 =
      CreateRequest(context.get(), alt_url, &delegate2);

  request2->Start();
  request1->Start();
  ASSERT_TRUE(request1->is_pending());
  ASSERT_TRUE(request2->is_pending());
  run_loop2.Run();
  run_loop1.Run();

  EXPECT_EQ(OK, delegate1.request_status());
  EXPECT_EQ(kHelloBodyValue, delegate1.data_received());
  ExpectQuicCertErrorOrNameNotResolved(*request2, delegate2);
}

// Tests that the alt service destination checks block alt-service requests from
// reusing a pre-existing non-alt-service QUIC session with the same
// destination.
TEST_P(URLRequestQuicWithTcpTest,
       AltServiceWrongCertExistingNonAltServiceQuicSession) {
  SetUpHttpsRecord();
  auto context = BuildContext();
  ConfigureAltService(*context, kOtherHost, kTestServerHost);

  GURL url = UrlFromPathWithPort(kHelloPath);
  GURL alt_url = OtherHostUrlFromPath(kHelloPath);

  base::RunLoop run_loop1;
  CheckLoadTimingDelegate delegate1(/*session_reused=*/false);
  delegate1.set_on_complete(run_loop1.QuitClosure());
  std::unique_ptr<URLRequest> request1 =
      CreateRequest(context.get(), url, &delegate1);
  request1->Start();
  ASSERT_TRUE(request1->is_pending());
  run_loop1.Run();
  EXPECT_EQ(OK, delegate1.request_status());
  EXPECT_EQ(kHelloBodyValue, delegate1.data_received());

  base::RunLoop run_loop2;
  TestDelegate delegate2;
  delegate2.set_on_complete(run_loop2.QuitClosure());
  std::unique_ptr<URLRequest> request2 =
      CreateRequest(context.get(), alt_url, &delegate2);
  request2->Start();
  ASSERT_TRUE(request2->is_pending());
  run_loop2.Run();
  ExpectQuicCertErrorOrNameNotResolved(*request2, delegate2);
}

// Tests the case where two hosts have the same QUIC alt service destination,
// but the server only serves a response that's valid for one of the two hosts.
TEST_P(URLRequestQuicWithTcpTest, TwoAltServiceRequestsOneWrongCert) {
  if (happy_eyeballs_v3_enabled()) {
    // TODO(crbug.com/455891789): This case is currently broken in the HEv3
    // case. Fix case and enable test.
    GTEST_SKIP();
  }

  SetUpLocalhostDnsRecord(kOtherHost2);
  auto context = BuildContext();
  ConfigureAltService(*context, kTestServerHost, kOtherHost2);
  ConfigureAltService(*context, kOtherHost, kOtherHost2);

  GURL url = UrlFromPath(kHelloPath);
  GURL alt_url = OtherHostUrlFromPath(kHelloPath);

  base::RunLoop run_loop1;
  CheckLoadTimingDelegate delegate1(/*session_reused=*/false);
  delegate1.set_on_complete(run_loop1.QuitClosure());
  std::unique_ptr<URLRequest> request1 =
      CreateRequest(context.get(), url, &delegate1);

  base::RunLoop run_loop2;
  TestDelegate delegate2;
  delegate2.set_on_complete(run_loop2.QuitClosure());
  std::unique_ptr<URLRequest> request2 =
      CreateRequest(context.get(), alt_url, &delegate2);

  request1->Start();
  request2->Start();
  ASSERT_TRUE(request1->is_pending());
  ASSERT_TRUE(request2->is_pending());
  run_loop1.Run();
  run_loop2.Run();

  EXPECT_EQ(OK, delegate1.request_status());
  EXPECT_EQ(kHelloBodyValue, delegate1.data_received());
  ExpectQuicCertErrorOrNameNotResolved(*request2, delegate2);
}

// Same as above, but with the order flipped.
TEST_P(URLRequestQuicWithTcpTest,
       TwoAltServiceRequestsOneWrongCertReverseOrder) {
  if (happy_eyeballs_v3_enabled()) {
    // TODO(crbug.com/455891789): This case is currently broken in the HEv3
    // case. Fix case and enable test.
    GTEST_SKIP();
  }

  SetUpLocalhostDnsRecord(kOtherHost2);
  auto context = BuildContext();
  ConfigureAltService(*context, kTestServerHost, kOtherHost2);
  ConfigureAltService(*context, kOtherHost, kOtherHost2);

  GURL url = UrlFromPath(kHelloPath);
  GURL alt_url = OtherHostUrlFromPath(kHelloPath);

  base::RunLoop run_loop1;
  CheckLoadTimingDelegate delegate1(/*session_reused=*/false);
  delegate1.set_on_complete(run_loop1.QuitClosure());
  std::unique_ptr<URLRequest> request1 =
      CreateRequest(context.get(), url, &delegate1);

  base::RunLoop run_loop2;
  TestDelegate delegate2;
  delegate2.set_on_complete(run_loop2.QuitClosure());
  std::unique_ptr<URLRequest> request2 =
      CreateRequest(context.get(), alt_url, &delegate2);

  request2->Start();
  request1->Start();
  ASSERT_TRUE(request1->is_pending());
  ASSERT_TRUE(request2->is_pending());
  run_loop2.Run();
  run_loop1.Run();

  EXPECT_EQ(OK, delegate1.request_status());
  EXPECT_EQ(kHelloBodyValue, delegate1.data_received());
  ExpectQuicCertErrorOrNameNotResolved(*request2, delegate2);
}

// Tests that the alt service destination checks block alt-service requests from
// reusing a pre-existing alt-service QUIC session with the same destination,
// but different target origin.
TEST_P(URLRequestQuicWithTcpTest,
       AltServiceRequestWrongCertExistingAltServiceQuicSession) {
  SetUpLocalhostDnsRecord(kOtherHost2);
  auto context = BuildContext();
  ConfigureAltService(*context, kTestServerHost, kOtherHost2);
  ConfigureAltService(*context, kOtherHost, kOtherHost2);

  GURL url = UrlFromPath(kHelloPath);
  GURL alt_url = OtherHostUrlFromPath(kHelloPath);

  base::RunLoop run_loop1;
  CheckLoadTimingDelegate delegate1(/*session_reused=*/false);
  delegate1.set_on_complete(run_loop1.QuitClosure());
  std::unique_ptr<URLRequest> request1 =
      CreateRequest(context.get(), url, &delegate1);
  request1->Start();
  ASSERT_TRUE(request1->is_pending());
  run_loop1.Run();
  EXPECT_EQ(OK, delegate1.request_status());

  base::RunLoop run_loop2;
  TestDelegate delegate2;
  delegate2.set_on_complete(run_loop2.QuitClosure());
  std::unique_ptr<URLRequest> request2 =
      CreateRequest(context.get(), alt_url, &delegate2);
  request2->Start();
  ASSERT_TRUE(request2->is_pending());
  run_loop2.Run();
  EXPECT_EQ(kHelloBodyValue, delegate1.data_received());
  ExpectQuicCertErrorOrNameNotResolved(*request2, delegate2);
}

}  // namespace

}  // namespace net
