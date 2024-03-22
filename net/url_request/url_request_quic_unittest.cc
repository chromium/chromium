// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
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

namespace net {

namespace {

// This must match the certificate used (quic-chain.pem and quic-leaf-cert.key).
const char kTestServerHost[] = "test.example.com";
// Used as a simple response from the server.
const char kHelloPath[] = "/hello.txt";
const char kHelloBodyValue[] = "Hello from QUIC Server";
const int kHelloStatus = 200;

class URLRequestQuicTest
    : public TestWithTaskEnvironment,
      public ::testing::WithParamInterface<quic::ParsedQuicVersion> {
 protected:
  URLRequestQuicTest()
      : context_builder_(CreateTestURLRequestContextBuilder()) {
    QuicEnableVersion(version());
    StartQuicServer(version());

    HttpNetworkSessionParams params;
    CertVerifyResult verify_result;
    verify_result.verified_cert = ImportCertFromFile(
        GetTestCertsDirectory(), "quic-chain.pem");
    auto cert_verifier = std::make_unique<MockCertVerifier>();
    cert_verifier->AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           kTestServerHost, verify_result, OK);
    // To simplify the test, and avoid the race with the HTTP request, we force
    // QUIC for these requests.
    auto quic_context = std::make_unique<QuicContext>();
    quic_context->params()->supported_versions = {version()};
    quic_context->params()->origins_to_force_quic_on.insert(
        HostPortPair(kTestServerHost, 443));
    context_builder_->set_quic_context(std::move(quic_context));
    params.enable_quic = true;
    context_builder_->set_host_resolver(std::move(host_resolver_));
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
    auto context = context_builder_->Build();
    return context;
  }

  static std::unique_ptr<URLRequest> CreateRequest(
      URLRequestContext* context,
      const GURL& url,
      URLRequest::Delegate* delegate) {
    return context->CreateRequest(url, DEFAULT_PRIORITY, delegate,
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  unsigned int GetRstErrorCountReceivedByServer(
      quic::QuicRstStreamErrorCode error_code) const {
    return (static_cast<quic::QuicSimpleDispatcher*>(server_->dispatcher()))
        ->GetRstErrorCount(error_code);
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

 protected:
  // Returns a fully-qualified URL for |path| on the test server.
  std::string UrlFromPath(std::string_view path) {
    return std::string("https://") + std::string(kTestServerHost) +
           std::string(path);
  }

  void SetDelay(std::string_view host,
                std::string_view path,
                base::TimeDelta delay) {
    memory_cache_backend_.SetResponseDelay(
        host, path,
        quic::QuicTime::Delta::FromMilliseconds(delay.InMilliseconds()));
  }

 private:
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

    auto resolver = std::make_unique<MockHostResolver>();
    resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    host_resolver_ = std::make_unique<MappedHostResolver>(std::move(resolver));
    // Use a mapped host resolver so that request for test.example.com
    // reach the server running on localhost.
    std::string map_rule =
        "MAP test.example.com test.example.com:" +
        base::NumberToString(server_->server_address().port());
    EXPECT_TRUE(host_resolver_->AddRuleFromString(map_rule));
  }

  std::unique_ptr<MappedHostResolver> host_resolver_;
  std::unique_ptr<QuicSimpleServer> server_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
  std::unique_ptr<URLRequestContextBuilder> context_builder_;
  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
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

}  // namespace

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const quic::ParsedQuicVersion& v) {
  return quic::ParsedQuicVersionToString(v);
}

INSTANTIATE_TEST_SUITE_P(Version,
                         URLRequestQuicTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(URLRequestQuicTest, TestGetRequest) {
  auto context = BuildContext();
  CheckLoadTimingDelegate delegate(false);
  std::unique_ptr<URLRequest> request =
      CreateRequest(context.get(), GURL(UrlFromPath(kHelloPath)), &delegate);

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

  GURL url = GURL(UrlFromPath(kHelloPath));
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
      CreateRequest(context.get(), GURL(UrlFromPath(kHelloPath)), &delegate);

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
      CreateRequest(context.get(), GURL(UrlFromPath(kHelloPath)), &delegate);

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

}  // namespace net
