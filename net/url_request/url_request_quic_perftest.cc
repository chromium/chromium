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
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_manager_test_utils.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/traced_value.h"
#include "net/base/load_timing_info.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_status_code.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/quic_context.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
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
using base::trace_event::MemoryAllocatorDump;

namespace net {

namespace {

const int kAltSvcPort = 6121;
const char kOriginHost[] = "mail.example.com";
const char kAltSvcHost[] = "test.example.com";
// Used as a simple response from the server.
const char kHelloPath[] = "/hello.txt";
const char kHelloAltSvcResponse[] = "Hello from QUIC Server";
const char kHelloOriginResponse[] = "Hello from TCP Server";
const int kHelloStatus = 200;

static constexpr char kMetricPrefixURLRequestQuick[] = "URLRequestQuic.";
static constexpr char kMetricRequestTimeMs[] = "request_time";
static constexpr char kMetricActiveQuicJobsCount[] = "active_quic_jobs";
static constexpr char kMetricActiveQuicSessionsCount[] = "active_quic_sessions";

perf_test::PerfResultReporter SetUpURLRequestQuicReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixURLRequestQuick, story);
  reporter.RegisterImportantMetric(kMetricRequestTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricActiveQuicJobsCount, "count");
  reporter.RegisterImportantMetric(kMetricActiveQuicSessionsCount, "count");
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
                base::test::SingleThreadTaskEnvironment::MainThreadType::IO)) {
    memory_dump_manager_ =
        base::trace_event::MemoryDumpManager::CreateInstanceForTesting();
    base::trace_event::InitializeMemoryDumpManagerForInProcessTesting(
        /*is_coordinator=*/false);
    memory_dump_manager_->set_dumper_registrations_ignored_for_testing(false);
    memory_dump_manager_->set_dumper_registrations_ignored_for_testing(true);
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
    context_builder->SetCertVerifier(std::make_unique<MockCertVerifier>());
    context_builder->set_quic_context(std::move(quic_context));
    context_ = context_builder->Build();
  }

  void TearDown() override {
    if (quic_server_) {
      quic_server_->Shutdown();
      // If possible, deliver the connection close packet to the client before
      // destruct the URLRequestContext.
      base::RunLoop().RunUntilIdle();
    }
    // |tcp_server_| shuts down in EmbeddedTestServer destructor.
    memory_dump_manager_.reset();
    task_environment_.reset();
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
        quic::test::crypto_test_utils::ProofSourceForTesting(), config,
        quic::QuicCryptoServerConfig::ConfigOptions(),
        quic::AllSupportedVersions(), &memory_cache_backend_);
    int rv = quic_server_->Listen(
        net::IPEndPoint(net::IPAddress::IPv4AllZeros(), kAltSvcPort));
    ASSERT_GE(rv, 0) << "Quic server fails to start";

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
    ASSERT_TRUE(tcp_server_->Start()) << "HTTP/1.1 server fails to start";

    CertVerifyResult verify_result;
    verify_result.verified_cert = tcp_server_->GetCertificate();
    cert_verifier().AddResultForCert(tcp_server_->GetCertificate(),
                                     verify_result, OK);
  }

  MockCertVerifier& cert_verifier() {
    // This cast is safe because we set a MockCertVerifier in the constructor.
    return *static_cast<MockCertVerifier*>(context_->cert_verifier());
  }

  std::unique_ptr<base::trace_event::MemoryDumpManager> memory_dump_manager_;
  std::unique_ptr<EmbeddedTestServer> tcp_server_;
  std::unique_ptr<QuicSimpleServer> quic_server_;
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  std::unique_ptr<URLRequestContext> context_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
};

void CheckScalarInDump(const MemoryAllocatorDump* dump,
                       const std::string& name,
                       const char* expected_units,
                       uint64_t expected_value) {
  MemoryAllocatorDump::Entry expected(name, expected_units, expected_value);
  EXPECT_THAT(dump->entries(), Contains(Eq(ByRef(expected))));
}

}  // namespace

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/852937): Fix this test on Fuchsia and re-enable.
#define MAYBE_TestGetRequest DISABLED_TestGetRequest
#else
#define MAYBE_TestGetRequest TestGetRequest
#endif
TEST_F(URLRequestQuicPerfTest, MAYBE_TestGetRequest) {
  bool quic_succeeded = false;
  GURL url(base::StringPrintf("https://%s%s", kOriginHost, kHelloPath));
  base::TimeTicks start = base::TimeTicks::Now();
  const int kNumRequest = 1000;
  for (int i = 0; i < kNumRequest; ++i) {
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request =
        CreateRequest(url, DEFAULT_PRIORITY, &delegate);

    request->Start();
    EXPECT_TRUE(request->is_pending());
    base::RunLoop().Run();

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
  base::trace_event::MemoryDumpManager::GetInstance()->SetupForTracing(
      base::trace_event::TraceConfig::MemoryDumpConfig());

  base::RunLoop run_loop;
  base::trace_event::MemoryDumpRequestArgs args{
      1 /* dump_guid*/, base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
      base::trace_event::MemoryDumpLevelOfDetail::LIGHT};

  auto on_memory_dump_done =
      [](base::OnceClosure quit_closure, const URLRequestContext* context,
         bool success, uint64_t dump_guid,
         std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd) {
        ASSERT_TRUE(success);
        const auto& allocator_dumps = pmd->allocator_dumps();

        auto it = allocator_dumps.find(
            base::StringPrintf("net/url_request_context/unknown/0x%" PRIxPTR,
                               reinterpret_cast<uintptr_t>(context)));
        ASSERT_NE(allocator_dumps.end(), it);
        MemoryAllocatorDump* url_request_context_dump = it->second.get();
        CheckScalarInDump(
            url_request_context_dump,
            base::trace_event::MemoryAllocatorDump::kNameObjectCount,
            base::trace_event::MemoryAllocatorDump::kUnitsObjects, 0);

        it = allocator_dumps.find(base::StringPrintf(
            "net/http_network_session_0x%" PRIxPTR "/quic_stream_factory",
            reinterpret_cast<uintptr_t>(
                context->http_transaction_factory()->GetSession())));
        ASSERT_NE(allocator_dumps.end(), it);
        MemoryAllocatorDump* quic_stream_factory_dump = it->second.get();
        CheckScalarInDump(quic_stream_factory_dump, "active_jobs",
                          base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                          0);

        CheckScalarInDump(quic_stream_factory_dump, "all_sessions",
                          base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                          1);

        std::string stream_factory_dump_name = base::StringPrintf(
            "net/http_network_session_0x%" PRIxPTR "/stream_factory",
            reinterpret_cast<uintptr_t>(
                context->http_transaction_factory()->GetSession()));
        ASSERT_EQ(0u, allocator_dumps.count(stream_factory_dump_name));
        std::move(quit_closure).Run();
      };
  base::trace_event::MemoryDumpManager::GetInstance()->CreateProcessDump(
      args,
      base::BindOnce(on_memory_dump_done, run_loop.QuitClosure(), context()));
  run_loop.Run();
  base::trace_event::MemoryDumpManager::GetInstance()->TeardownForTracing();
}

}  // namespace net
