// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/load_timing_info.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_status_code.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/crypto_test_utils_chromium.h"
#include "net/quic/quic_context.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_memory_cache_backend.h"
#include "net/tools/quic/quic_simple_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;
using testing::Contains;
using testing::Eq;
using testing::ByRef;

namespace net {

namespace {

constexpr int kAltSvcPort = 6121;
constexpr char kOriginHost[] = "mail.example.com";
constexpr char kAltSvcHost[] = "test.example.com";
// Used as a simple response from the server.
constexpr char kHelloPath[] = "/hello.txt";
constexpr char kHelloAltSvcResponse[] = "Hello from QUIC Server";
constexpr char kHelloOriginResponse[] = "Hello from TCP Server";
constexpr int kHelloStatus = 200;

constexpr char kMetricPrefixURLRequestQuick[] = "URLRequestQuic.";
constexpr char kMetricRequestTimeMs[] = "request_time";

perf_test::PerfResultReporter SetUpURLRequestQuicReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixURLRequestQuick, story);
  reporter.RegisterImportantMetric(kMetricRequestTimeMs, "ms");
  return reporter;
}

std::unique_ptr<test_server::HttpResponse> HandleRequest(
    const test_server::HttpRequest& request) {
  auto http_response = std::make_unique<test_server::BasicHttpResponse>();
  std::string alpn =
      quic::AlpnForVersion(DefaultSupportedQuicVersions().front());
  http_response->AddCustomHeader(
      "Alt-Svc", base::StringPrintf("%s=\"%s:%d\"", alpn.c_str(), kAltSvcHost,
                                    kAltSvcPort));
  http_response->set_code(HTTP_OK);
  http_response->set_content(kHelloOriginResponse);
  http_response->set_content_type("text/plain");
  return std::move(http_response);
}

class URLRequestQuicPerfTest : public ::testing::Test {
 protected:
  URLRequestQuicPerfTest()
      : task_environment_(
            std::make_unique<base::test::SingleThreadTaskEnvironment>(
                base::test::SingleThreadTaskEnvironment::MainThreadType::IO)),
        cert_verifier_(std::make_unique<MockCertVerifier>()) {
    StartTcpServer();
    StartQuicServer();

    // Host mapping.
    auto resolver = std::make_unique<MockHostResolver>();
    resolver->rules()->AddRule(kAltSvcHost, "127.0.0.1");
    auto host_resolver =
        std::make_unique<MappedHostResolver>(std::move(resolver));
    std::string map_rule = base::StringPrintf("MAP %s 127.0.0.1:%d",
                                              kOriginHost, tcp_server_->port());
    EXPECT_TRUE(host_resolver->AddRuleFromString(map_rule));

    HttpNetworkSessionParams params;
    params.enable_quic = true;
    params.enable_user_alternate_protocol_ports = true;
    auto quic_context = std::make_unique<QuicContext>();
    quic_context->params()->allow_remote_alt_svc = true;
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_host_resolver(std::move(host_resolver));
    context_builder->set_http_network_session_params(params);
    context_builder->SetCertVerifier(std::move(cert_verifier_));
    context_builder->set_quic_context(std::move(quic_context));
    context_ = context_builder->Build();
  }

  void TearDown() override {
    CHECK(quic_server_);
    quic_server_->Shutdown();
    // If possible, deliver the connection close packet to the client before
    // destruct the URLRequestContext.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<URLRequest> CreateRequest(const GURL& url,
                                            RequestPriority priority,
                                            URLRequest::Delegate* delegate) {
    return context_->CreateRequest(url, priority, delegate,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  URLRequestContext* context() const { return context_.get(); }

 private:
  void StartQuicServer() {
    quic::QuicConfig config;
    memory_cache_backend_.AddSimpleResponse(kOriginHost, kHelloPath,
                                            kHelloStatus, kHelloAltSvcResponse);
    quic_server_ = std::make_unique<QuicSimpleServer>(
        net::test::ProofSourceForTestingChromium(), config,
        quic::QuicCryptoServerConfig::ConfigOptions(),
        quic::AllSupportedVersions(), &memory_cache_backend_);
    int rv = quic_server_->Listen(
        net::IPEndPoint(net::IPAddress::IPv4AllZeros(), kAltSvcPort));
    ASSERT_GE(rv, 0) << "Quic server failed to start";

    CertVerifyResult verify_result;
    verify_result.verified_cert = ImportCertFromFile(
        GetTestCertsDirectory(), "quic-chain.pem");
    verify_result.is_issued_by_known_root = true;
    cert_verifier().AddResultForCert(verify_result.verified_cert.get(),
                                     verify_result, OK);
  }

  void StartTcpServer() {
    tcp_server_ = std::make_unique<EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    tcp_server_->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(tcp_server_->Start()) << "HTTP/1.1 server failed to start";

    CertVerifyResult verify_result;
    verify_result.verified_cert = tcp_server_->GetCertificate();
    cert_verifier().AddResultForCert(tcp_server_->GetCertificate(),
                                     verify_result, OK);
  }

  MockCertVerifier& cert_verifier() {
    // `cert_verifier_` becomes unset when it is passed to the
    // URLRequestContext, but we need to be available earlier than then so that
    // StartTcpServer() can call it. So look for it in both places.
    return cert_verifier_ ? *cert_verifier_ :
                          // This cast is safe because we set a MockCertVerifier
                          // in the constructor.
               *static_cast<MockCertVerifier*>(context_->cert_verifier());
  }

  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  std::unique_ptr<EmbeddedTestServer> tcp_server_;
  std::unique_ptr<QuicSimpleServer> quic_server_;
  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<MockCertVerifier> cert_verifier_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
};

}  // namespace

TEST_F(URLRequestQuicPerfTest, TestGetRequest) {
  bool quic_succeeded = false;
  GURL url(base::StringPrintf("https://%s%s", kOriginHost, kHelloPath));
  base::TimeTicks start = base::TimeTicks::Now();
  constexpr int kNumRequest = 1000;
  for (int i = 0; i < kNumRequest; ++i) {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request =
        CreateRequest(url, DEFAULT_PRIORITY, &delegate);

    request->Start();
    EXPECT_TRUE(request->is_pending());
    delegate.RunUntilComplete();

    EXPECT_EQ(OK, delegate.request_status());
    if (delegate.data_received() == kHelloAltSvcResponse) {
      quic_succeeded = true;
    } else {
      EXPECT_EQ(kHelloOriginResponse, delegate.data_received());
    }
  }
  base::TimeTicks end = base::TimeTicks::Now();
  auto reporter = SetUpURLRequestQuicReporter("get");
  reporter.AddResult(kMetricRequestTimeMs,
                     (end - start).InMillisecondsF() / kNumRequest);

  EXPECT_TRUE(quic_succeeded);
}

}  // namespace net
