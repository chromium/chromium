// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/base/load_timing_info.h"
#include "net/base/network_delegate.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/crypto/proof_source_chromium.h"
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

class URLRequestQuicTest : public TestWithTaskEnvironment {
 protected:
  URLRequestQuicTest() : context_(new TestURLRequestContext(true)) {
    StartQuicServer();

    std::unique_ptr<HttpNetworkSession::Params> params(
        new HttpNetworkSession::Params);
    CertVerifyResult verify_result;
    verify_result.verified_cert = ImportCertFromFile(
        GetTestCertsDirectory(), "quic-chain.pem");
    cert_verifier_.AddResultForCertAndHost(verify_result.verified_cert.get(),
                                           "test.example.com", verify_result,
                                           OK);
    // To simplify the test, and avoid the race with the HTTP request, we force
    // QUIC for these requests.
    params->quic_params.origins_to_force_quic_on.insert(
        HostPortPair(kTestServerHost, 443));
    params->enable_quic = true;
    params->enable_server_push_cancellation = true;
    context_->set_host_resolver(host_resolver_.get());
    context_->set_http_network_session_params(std::move(params));
    context_->set_cert_verifier(&cert_verifier_);
    context_->set_net_log(&net_log_);
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

 protected:
  TestNetLog net_log_;

 private:
  void StartQuicServer() {
    // Set up in-memory cache.
    memory_cache_backend_.AddSimpleResponse(kTestServerHost, kHelloPath,
                                            kHelloStatus, kHelloBodyValue);
    memory_cache_backend_.InitializeBackend(ServerPushCacheDirectory());
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
        quic::QuicCryptoServerConfig::ConfigOptions(),
        quic::AllSupportedVersions(), &memory_cache_backend_));
    int rv =
        server_->Listen(net::IPEndPoint(net::IPAddress::IPv4AllZeros(), 0));
    EXPECT_GE(rv, 0) << "Quic server fails to start";

    std::unique_ptr<MockHostResolver> resolver(new MockHostResolver());
    resolver->rules()->AddRule("test.example.com", "127.0.0.1");
    host_resolver_.reset(new MappedHostResolver(std::move(resolver)));
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

  std::unique_ptr<MappedHostResolver> host_resolver_;
  std::unique_ptr<QuicSimpleServer> server_;
  std::unique_ptr<TestURLRequestContext> context_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
  MockCertVerifier cert_verifier_;
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
      const base::Closure& all_requests_completed_callback,
      size_t num_expected_requests)
      : all_requests_completed_callback_(all_requests_completed_callback),
        num_expected_requests_(num_expected_requests) {}

  void OnCompleted(URLRequest* request, bool started, int net_error) override {
    net::TestNetworkDelegate::OnCompleted(request, started, net_error);
    num_expected_requests_--;
    if (num_expected_requests_ == 0)
      all_requests_completed_callback_.Run();
  }

 private:
  const base::Closure all_requests_completed_callback_;
  size_t num_expected_requests_;
  DISALLOW_COPY_AND_ASSIGN(WaitForCompletionNetworkDelegate);
};

}  // namespace

TEST_F(URLRequestQuicTest, TestGetRequest) {
  Init();
  CheckLoadTimingDelegate delegate(false);
  std::string url =
      base::StringPrintf("https://%s%s", kTestServerHost, kHelloPath);
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_TRUE(request->status().is_success());
  EXPECT_EQ(kHelloBodyValue, delegate.data_received());
}

TEST_F(URLRequestQuicTest, CancelPushIfCached_SomeCached) {
  Init();

  // Send a request to the pushed url: /kitten-1.jpg to pull the resource into
  // cache.
  CheckLoadTimingDelegate delegate_0(false);
  std::string url_0 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/kitten-1.jpg");
  std::unique_ptr<URLRequest> request_0 =
      CreateRequest(GURL(url_0), DEFAULT_PRIORITY, &delegate_0);

  request_0->Start();
  ASSERT_TRUE(request_0->is_pending());

  // Spin the message loop until the client receives the response for the first
  // request.
  do {
    base::RunLoop().RunUntilIdle();
  } while (request_0->status().is_io_pending());
  EXPECT_TRUE(request_0->status().is_success());

  // Send a request to /index2.html which pushes /kitten-1.jpg and /favicon.ico.
  // Should cancel push for /kitten-1.jpg.
  CheckLoadTimingDelegate delegate(true);
  std::string url =
      base::StringPrintf("https://%s%s", kTestServerHost, "/index2.html");
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());

  // Spin the message loop until the client receives the response for the second
  // request.
  do {
    base::RunLoop().RunUntilIdle();
  } while (request->status().is_io_pending());
  EXPECT_TRUE(request->status().is_success());

  // Extract net logs on client side to verify push lookup transactions.
  auto entries = net_log_.GetEntriesWithType(
      NetLogEventType::SERVER_PUSH_LOOKUP_TRANSACTION);

  ASSERT_EQ(4u, entries.size());

  std::string value;
  std::string push_url_1 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/kitten-1.jpg");
  std::string push_url_2 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/favicon.ico");

  const NetLogSource source_1 = FindPushUrlSource(entries, push_url_1);
  EXPECT_TRUE(source_1.IsValid());

  // No net error code for this lookup transaction, the push is found.
  const NetLogEntry* end_entry_1 = FindEndBySource(entries, source_1);
  EXPECT_FALSE(end_entry_1->HasParams());
  EXPECT_FALSE(GetOptionalNetErrorCodeFromParams(*end_entry_1));

  const NetLogSource source_2 = FindPushUrlSource(entries, push_url_2);
  EXPECT_TRUE(source_2.IsValid());
  EXPECT_NE(source_1.id, source_2.id);

  // Net error code -400 is found for this lookup transaction, the push is not
  // found in the cache.
  const NetLogEntry* end_entry_2 = FindEndBySource(entries, source_2);
  EXPECT_TRUE(end_entry_2->HasParams());
  EXPECT_EQ(-400, GetNetErrorCodeFromParams(*end_entry_2));

  // Verify the reset error count received on the server side.
  EXPECT_LE(1u, GetRstErrorCountReceivedByServer(quic::QUIC_STREAM_CANCELLED));
}

TEST_F(URLRequestQuicTest, CancelPushIfCached_AllCached) {
  Init();

  // Send a request to the pushed url: /kitten-1.jpg to pull the resource into
  // cache.
  CheckLoadTimingDelegate delegate_0(false);
  std::string url_0 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/kitten-1.jpg");
  std::unique_ptr<URLRequest> request_0 =
      CreateRequest(GURL(url_0), DEFAULT_PRIORITY, &delegate_0);

  request_0->Start();
  ASSERT_TRUE(request_0->is_pending());

  // Spin the message loop until the client receives the response for the first
  // request.
  do {
    base::RunLoop().RunUntilIdle();
  } while (request_0->status().is_io_pending());
  EXPECT_TRUE(request_0->status().is_success());

  // Send a request to the pushed url: /favicon.ico to pull the resource into
  // cache.
  CheckLoadTimingDelegate delegate_1(true);
  std::string url_1 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/favicon.ico");
  std::unique_ptr<URLRequest> request_1 =
      CreateRequest(GURL(url_1), DEFAULT_PRIORITY, &delegate_1);

  request_1->Start();
  ASSERT_TRUE(request_1->is_pending());

  // Spin the message loop until the client receives the response for the second
  // request.
  do {
    base::RunLoop().RunUntilIdle();
  } while (request_1->status().is_io_pending());
  EXPECT_TRUE(request_1->status().is_success());

  // Send a request to /index2.html which pushes /kitten-1.jpg and /favicon.ico.
  // Should cancel push for both pushed resources, since they're already cached.
  CheckLoadTimingDelegate delegate(true);
  std::string url =
      base::StringPrintf("https://%s%s", kTestServerHost, "/index2.html");
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());

  // Spin the message loop until the client receives the response for the third
  // request.
  do {
    base::RunLoop().RunUntilIdle();
  } while (request->status().is_io_pending());
  EXPECT_TRUE(request->status().is_success());

  // Extract net logs on client side to verify push lookup transactions.
  auto entries = net_log_.GetEntriesWithType(
      NetLogEventType::SERVER_PUSH_LOOKUP_TRANSACTION);

  EXPECT_EQ(4u, entries.size());

  std::string value;
  std::string push_url_1 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/kitten-1.jpg");
  std::string push_url_2 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/favicon.ico");

  const NetLogSource source_1 = FindPushUrlSource(entries, push_url_1);
  EXPECT_TRUE(source_1.IsValid());

  // No net error code for this lookup transaction, the push is found.
  const NetLogEntry* end_entry_1 = FindEndBySource(entries, source_1);
  EXPECT_FALSE(end_entry_1->HasParams());
  EXPECT_FALSE(GetOptionalNetErrorCodeFromParams(*end_entry_1));

  const NetLogSource source_2 = FindPushUrlSource(entries, push_url_2);
  EXPECT_TRUE(source_1.IsValid());
  EXPECT_NE(source_1.id, source_2.id);

  // No net error code for this lookup transaction, the push is found.
  const NetLogEntry* end_entry_2 = FindEndBySource(entries, source_2);
  EXPECT_FALSE(end_entry_2->HasParams());
  EXPECT_FALSE(GetOptionalNetErrorCodeFromParams(*end_entry_2));

  // Verify the reset error count received on the server side.
  EXPECT_LE(2u, GetRstErrorCountReceivedByServer(quic::QUIC_STREAM_CANCELLED));
}

TEST_F(URLRequestQuicTest, DoNotCancelPushIfNotFoundInCache) {
  Init();

  // Send a request to /index2.hmtl which pushes /kitten-1.jpg and /favicon.ico
  // and shouldn't cancel any since neither is in cache.
  CheckLoadTimingDelegate delegate(false);
  std::string url =
      base::StringPrintf("https://%s%s", kTestServerHost, "/index2.html");
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());

  // Spin the message loop until the client receives response.
  do {
    base::RunLoop().RunUntilIdle();
  } while (request->status().is_io_pending());
  EXPECT_TRUE(request->status().is_success());

  // Extract net logs on client side to verify push lookup transactions.
  auto entries = net_log_.GetEntriesWithType(
      NetLogEventType::SERVER_PUSH_LOOKUP_TRANSACTION);

  EXPECT_EQ(4u, entries.size());

  std::string value;
  std::string push_url_1 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/kitten-1.jpg");
  std::string push_url_2 =
      base::StringPrintf("https://%s%s", kTestServerHost, "/favicon.ico");

  const NetLogSource source_1 = FindPushUrlSource(entries, push_url_1);
  EXPECT_TRUE(source_1.IsValid());
  const NetLogEntry* end_entry_1 = FindEndBySource(entries, source_1);
  EXPECT_TRUE(end_entry_1->HasParams());
  EXPECT_EQ(-400, GetNetErrorCodeFromParams(*end_entry_1));

  const NetLogSource source_2 = FindPushUrlSource(entries, push_url_2);
  EXPECT_TRUE(source_2.IsValid());
  EXPECT_NE(source_1.id, source_2.id);
  const NetLogEntry* end_entry_2 = FindEndBySource(entries, source_2);
  EXPECT_TRUE(end_entry_2->HasParams());
  EXPECT_EQ(-400, GetNetErrorCodeFromParams(*end_entry_2));

  // Verify the reset error count received on the server side.
  EXPECT_EQ(0u, GetRstErrorCountReceivedByServer(quic::QUIC_STREAM_CANCELLED));
}

// Tests that if two requests use the same QUIC session, the second request
// should not have |LoadTimingInfo::connect_timing|.
TEST_F(URLRequestQuicTest, TestTwoRequests) {
  base::RunLoop run_loop;
  WaitForCompletionNetworkDelegate network_delegate(
      run_loop.QuitClosure(), /*num_expected_requests=*/2);
  SetNetworkDelegate(&network_delegate);
  Init();
  CheckLoadTimingDelegate delegate(false);
  delegate.set_on_complete(base::DoNothing());
  std::string url =
      base::StringPrintf("https://%s%s", kTestServerHost, kHelloPath);
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate);

  CheckLoadTimingDelegate delegate2(true);
  delegate2.set_on_complete(base::DoNothing());
  std::unique_ptr<URLRequest> request2 =
      CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate2);
  request->Start();
  request2->Start();
  ASSERT_TRUE(request->is_pending());
  ASSERT_TRUE(request2->is_pending());
  run_loop.Run();

  EXPECT_TRUE(request->status().is_success());
  EXPECT_TRUE(request2->status().is_success());
  EXPECT_EQ(kHelloBodyValue, delegate.data_received());
  EXPECT_EQ(kHelloBodyValue, delegate2.data_received());
}

TEST_F(URLRequestQuicTest, RequestHeadersCallback) {
  Init();
  HttpRawRequestHeaders raw_headers;
  TestDelegate delegate;
  TestURLRequestContext context;
  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("X-Foo", "bar");

  std::string url =
      base::StringPrintf("https://%s%s", kTestServerHost, kHelloPath);
  std::unique_ptr<URLRequest> request =
      CreateRequest(GURL(url), DEFAULT_PRIORITY, &delegate);

  request->SetExtraRequestHeaders(extra_headers);
  request->SetRequestHeadersCallback(base::Bind(
      &HttpRawRequestHeaders::Assign, base::Unretained(&raw_headers)));
  request->Start();
  ASSERT_TRUE(request->is_pending());
  do {
    base::RunLoop().RunUntilIdle();
  } while (!delegate.response_started_count());
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
}

}  // namespace net
