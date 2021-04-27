// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/load_timing_info.h"
#include "net/base/network_delegate.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/quic_context.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quic/core/quic_dispatcher.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quic/tools/quic_memory_cache_backend.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_dispatcher.h"
#include "net/tools/quic/quic_simple_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

// This must match the certificate used (quic-chain.pem and quic-leaf-cert.key).
const char kTestServerHost[] = "test.example.com";
// Used as a simple response from the server.
const char kHelloPath[] = "/hello.txt";
const char kHelloBodyValue[] = "Hello from QUIC Server";
const int kHelloStatus = 200;

// Used as a simple pushed response from the server.
const char kKittenPath[] = "/kitten-1.jpg";
const char kKittenBodyValue[] = "Kitten image";

// Used as a simple pushed response from the server.
const char kFaviconPath[] = "/favicon.ico";
const char kFaviconBodyValue[] = "Favion";

// Used as a simple pushed response from the server.
const char kIndexPath[] = "/index2.html";
const char kIndexBodyValue[] = "Hello from QUIC Server";
const int kIndexStatus = 200;

class MockCTPolicyEnforcerNonCompliant : public CTPolicyEnforcer {
 public:
  MockCTPolicyEnforcerNonCompliant() = default;
  ~MockCTPolicyEnforcerNonCompliant() override = default;

  ct::CTPolicyCompliance CheckCompliance(
      X509Certificate* cert,
      const ct::SCTList& verified_scts,
      const NetLogWithSource& net_log) override {
    return ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS;
  }
};

// An ExpectCTReporter that records the number of times OnExpectCTFailed() was
// called.
class MockExpectCTReporter : public TransportSecurityState::ExpectCTReporter {
 public:
  MockExpectCTReporter() = default;
  ~MockExpectCTReporter() override = default;

  void OnExpectCTFailed(
      const HostPortPair& host_port_pair,
      const GURL& report_uri,
      base::Time expiration,
      const X509Certificate* validated_certificate_chain,
      const X509Certificate* served_certificate_chain,
      const SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps,
      const NetworkIsolationKey& network_isolation_key) override {
    num_failures_++;
    report_uri_ = report_uri;
    network_isolation_key_ = network_isolation_key;
  }

  int num_failures() const { return num_failures_; }
  const GURL& report_uri() const { return report_uri_; }
  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

 private:
  int num_failures_ = 0;

  GURL report_uri_;
  NetworkIsolationKey network_isolation_key_;
};

class URLRequestQuicTest
    : public TestWithTaskEnvironment,
      public ::testing::WithParamInterface<quic::ParsedQuicVersion> {
 protected:
  URLRequestQuicTest() : context_(new TestURLRequestContext(true)) {
    QuicEnableVersion(version());
    StartQuicServer(version());

    std::unique_ptr<HttpNetworkSession::Params> params(
        new HttpNetworkSession::Params);
    CertVerifyResult verify_result;
    verify_result.verified_cert = ImportCertFromFile(
        GetTestCertsDirectory(), "quic-chain.pem");
    cert_verifier_.AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           kTestServerHost, verify_result, OK);
    // To simplify the test, and avoid the race with the HTTP request, we force
    // QUIC for these requests.
    context_->set_quic_context(&quic_context_);
    quic_context_.params()->supported_versions = {version()};
    quic_context_.params()->origins_to_force_quic_on.insert(
        HostPortPair(kTestServerHost, 443));
    params->enable_quic = true;
    params->enable_server_push_cancellation = true;
    context_->set_host_resolver(host_resolver_.get());
    context_->set_http_network_session_params(std::move(params));
    context_->set_cert_verifier(&cert_verifier_);
    context_->set_net_log(&net_log_);
    transport_security_state_.SetExpectCTReporter(&expect_ct_reporter_);
    context_->set_transport_security_state(&transport_security_state_);
  }

  void TearDown() override {
    if (server_) {
      server_->Shutdown();
      // If possible, deliver the conncetion close packet to the client before
      // destruct the TestURLRequestContext.
      base::RunLoop().RunUntilIdle();
    }
  }

  // Sets a NetworkDelegate to use for |context_|. Must be done before Init().
  void SetNetworkDelegate(NetworkDelegate* network_delegate) {
    context_->set_network_delegate(network_delegate);
  }

  // Can be used to modify |context_|. Only safe to modify before Init() is
  // called.
  TestURLRequestContext* context() { return context_.get(); }

  // Initializes the TestURLRequestContext |context_|.
  void Init() { context_->Init(); }

  std::unique_ptr<URLRequest> CreateRequest(const GURL& url,
                                            RequestPriority priority,
                                            URLRequest::Delegate* delegate) {
    return context_->CreateRequest(url, priority, delegate,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  unsigned int GetRstErrorCountReceivedByServer(
      quic::QuicRstStreamErrorCode error_code) const {
    return (static_cast<quic::QuicSimpleDispatcher*>(server_->dispatcher()))
        ->GetRstErrorCount(error_code);
  }

  static const NetLogSource FindPushUrlSource(
      const std::vector<NetLogEntry>& entries,
      const std::string& push_url) {
    std::string entry_push_url;
    for (const auto& entry : entries) {
      if (entry.phase == NetLogEventPhase::BEGIN &&
          entry.source.type ==
              NetLogSourceType::SERVER_PUSH_LOOKUP_TRANSACTION) {
        auto entry_push_url =
            GetOptionalStringValueFromParams(entry, "push_url");
        if (entry_push_url && *entry_push_url == push_url) {
          return entry.source;
        }
      }
    }
    return NetLogSource();
  }

  static const NetLogEntry* FindEndBySource(
      const std::vector<NetLogEntry>& entries,
      const NetLogSource& source) {
    for (const auto& entry : entries) {
      if (entry.phase == NetLogEventPhase::END &&
          entry.source.type == source.type && entry.source.id == source.id)
        return &entry;
    }
    return nullptr;
  }

  quic::ParsedQuicVersion version() { return GetParam(); }

  MockExpectCTReporter* expect_ct_reporter() { return &expect_ct_reporter_; }

  TransportSecurityState* transport_security_state() {
    return &transport_security_state_;
  }

 protected:
  // Returns a fully-qualified URL for |path| on the test server.
  std::string UrlFromPath(base::StringPiece path) {
    return std::string("https://") + std::string(kTestServerHost) +
           std::string(path);
  }

  RecordingTestNetLog net_log_;

 private:
  void StartQuicServer(quic::ParsedQuicVersion version) {
    // Set up in-memory cache.

    // Add the simply hello response.
    memory_cache_backend_.AddSimpleResponse(kTestServerHost, kHelloPath,
                                            kHelloStatus, kHelloBodyValue);

    // Now set up index so that it pushes kitten and favicon.
    quic::QuicBackendResponse::ServerPushInfo push_info1(
        quic::QuicUrl(UrlFromPath(kKittenPath)), spdy::Http2HeaderBlock(),
        spdy::kV3LowestPriority, kKittenBodyValue);
    quic::QuicBackendResponse::ServerPushInfo push_info2(
        quic::QuicUrl(UrlFromPath(kFaviconPath)), spdy::Http2HeaderBlock(),
        spdy::kV3LowestPriority, kFaviconBodyValue);
    memory_cache_backend_.AddSimpleResponseWithServerPushResources(
        kTestServerHost, kIndexPath, kIndexStatus, kIndexBodyValue,
        {push_info1, push_info2});
    quic::QuicConfig config;
    // Set up server certs.
    std::unique_ptr<net::ProofSourceChromium> proof_source(
        new net::ProofSourceChromium());
    base::FilePath directory = GetTestCertsDirectory();
    CHECK(proof_source->Initialize(
        directory.Append(FILE_PATH_LITERAL("quic-chain.pem")),
        directory.Append(FILE_PATH_LITERAL("quic-leaf-cert.key")),
        base::FilePath()));
    server_.reset(new QuicSimpleServer(
        quic::test::crypto_test_utils::ProofSourceForTesting(), config,
        quic::QuicCryptoServerConfig::ConfigOptions(), {version},
        &memory_cache_backend_));
    int rv =
        server_->Listen(net::IPEndPoint(net::IPAddress::IPv4AllZeros(), 0));
    EXPECT_GE(rv, 0) << "Quic server fails to start";

    std::unique_ptr<MockHostResolver> resolver(new MockHostResolver());
    resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    host_resolver_ = std::make_unique<MappedHostResolver>(std::move(resolver));
    // Use a mapped host resolver so that request for test.example.com
    // reach the server running on localhost.
    std::string map_rule =
        "MAP test.example.com test.example.com:" +
        base::NumberToString(server_->server_address().port());
    EXPECT_TRUE(host_resolver_->AddRuleFromString(map_rule));
  }

  std::string ServerPushCacheDirectory() {
    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("net").AppendASCII("data").AppendASCII(
        "quic_http_response_cache_data_with_push");
    // The file path is known to be an ascii string.
    return path.MaybeAsASCII();
  }

  MockExpectCTReporter expect_ct_reporter_;
  TransportSecurityState transport_security_state_;

  std::unique_ptr<MappedHostResolver> host_resolver_;
  std::unique_ptr<QuicSimpleServer> server_;
  std::unique_ptr<TestURLRequestContext> context_;
  QuicContext quic_context_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
  MockCertVerifier cert_verifier_;
  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
};

// A URLRequest::Delegate that checks LoadTimingInfo when response headers are
// received.
class CheckLoadTimingDelegate : public TestDelegate {
 public:
  CheckLoadTimingDelegate(bool session_reused)
      : session_reused_(session_reused) {}
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
              load_timing_info.connect_timing.dns_start.is_null());
    EXPECT_EQ(session_reused,
              load_timing_info.connect_timing.dns_end.is_null());
  }

  bool session_reused_;

  DISALLOW_COPY_AND_ASSIGN(CheckLoadTimingDelegate);
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

  void OnCompleted(URLRequest* request, bool started, int net_error) override {
    net::TestNetworkDelegate::OnCompleted(request, started, net_error);
    num_expected_requests_--;
    if (num_expected_requests_ == 0)
      std::move(all_requests_completed_callback_).Run();
  }

 private:
  base::OnceClosure all_requests_completed_callback_;
  size_t num_expected_requests_;
  DISALLOW_COPY_AND_ASSIGN(WaitForCompletionNetworkDelegate);
};

}  // namespace

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const quic::ParsedQuicVersion& v) {
  return quic::ParsedQuicVersionToString(v);
}

INSTANTIATE_TEST_SUITE_P(Version,
                         URLRequestQuicTest,
                         ::testing::ValuesIn(quic::AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(URLRequestQuicTest, TestGetRequest) {
  Init();
  CheckLoadTimingDelegate delegate(false);
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(UrlFromPath(kHelloPath)), DEFAULT_PRIORITY, &delegate);

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
  WaitForCompletionNetworkDelegate network_delegate(
      run_loop.QuitClosure(), /*num_expected_requests=*/2);
  SetNetworkDelegate(&network_delegate);
  Init();
  CheckLoadTimingDelegate delegate(false);
  delegate.set_on_complete(base::DoNothing());
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(UrlFromPath(kHelloPath)), DEFAULT_PRIORITY, &delegate);

  CheckLoadTimingDelegate delegate2(true);
  delegate2.set_on_complete(base::DoNothing());
  std::unique_ptr<URLRequest> request2 = CreateRequest(
      GURL(UrlFromPath(kHelloPath)), DEFAULT_PRIORITY, &delegate2);
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
  Init();
  HttpRawRequestHeaders raw_headers;
  TestDelegate delegate;
  TestURLRequestContext context;
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");

  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(UrlFromPath(kHelloPath)), DEFAULT_PRIORITY, &delegate);

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

// Tests that if there's an Expect-CT failure at the QUIC layer, a report is
// generated.
TEST_P(URLRequestQuicTest, ExpectCT) {
  TransportSecurityState::SetRequireCTForTesting(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // enabled_features
      {features::kPartitionConnectionsByNetworkIsolationKey,
       features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
       features::kPartitionSSLSessionsByNetworkIsolationKey},
      // disabled_features
      {});

  MockCTPolicyEnforcerNonCompliant ct_enforcer;
  context()->set_ct_policy_enforcer(&ct_enforcer);
  Init();

  GURL report_uri("https://report.test/");
  IsolationInfo isolation_info = IsolationInfo::CreateTransient();
  transport_security_state()->AddExpectCT(
      kTestServerHost, base::Time::Now() + base::TimeDelta::FromDays(1),
      true /* enforce */, report_uri, isolation_info.network_isolation_key());

  base::RunLoop run_loop;
  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(UrlFromPath(kHelloPath)), DEFAULT_PRIORITY, &delegate);
  request->set_isolation_info(isolation_info);
  request->Start();
  delegate.RunUntilComplete();

  EXPECT_EQ(ERR_QUIC_PROTOCOL_ERROR, delegate.request_status());
  ASSERT_EQ(1, expect_ct_reporter()->num_failures());
  EXPECT_EQ(report_uri, expect_ct_reporter()->report_uri());
  EXPECT_EQ(isolation_info.network_isolation_key(),
            expect_ct_reporter()->network_isolation_key());
}

}  // namespace net
