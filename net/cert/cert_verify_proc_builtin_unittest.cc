// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_builtin.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/time_conversions.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/gtest_util.h"
#include "net/test/revocation_builder.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_collection.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

std::unique_ptr<test_server::HttpResponse> HangRequestAndCallback(
    base::OnceClosure callback,
    const test_server::HttpRequest& request) {
  std::move(callback).Run();
  return std::make_unique<test_server::HungResponse>();
}

void FailTest(const std::string& message) {
  ADD_FAILURE() << message;
}

std::unique_ptr<test_server::HttpResponse> FailRequestAndFailTest(
    const std::string& message,
    scoped_refptr<base::TaskRunner> main_task_runner,
    const test_server::HttpRequest& request) {
  main_task_runner->PostTask(FROM_HERE, base::BindOnce(FailTest, message));
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_NOT_ACCEPTABLE);
  return response;
}

std::unique_ptr<test_server::HttpResponse> ServeResponse(
    HttpStatusCode status_code,
    const std::string& content_type,
    const std::string& content,
    const test_server::HttpRequest& request) {
  auto http_response = std::make_unique<test_server::BasicHttpResponse>();

  http_response->set_code(status_code);
  http_response->set_content_type(content_type);
  http_response->set_content(content);
  return http_response;
}

std::string MakeRandomHexString(size_t num_bytes) {
  std::vector<char> rand_bytes;
  rand_bytes.resize(num_bytes);

  base::RandBytes(rand_bytes.data(), rand_bytes.size());
  return base::HexEncode(rand_bytes.data(), rand_bytes.size());
}

static std::string MakeRandomPath(base::StringPiece suffix) {
  return "/" + MakeRandomHexString(12) + std::string(suffix);
}

int VerifyOnWorkerThread(const scoped_refptr<CertVerifyProc>& verify_proc,
                         scoped_refptr<X509Certificate> cert,
                         const std::string& hostname,
                         int flags,
                         CertVerifyResult* verify_result,
                         NetLogSource* out_source) {
  base::ScopedAllowBaseSyncPrimitivesForTesting scoped_allow_blocking;
  NetLogWithSource net_log(NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_TASK));
  int error = verify_proc->Verify(cert.get(), hostname,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  verify_result, net_log);
  *out_source = net_log.source();
  return error;
}

class MockSystemTrustStore : public SystemTrustStore {
 public:
  bssl::TrustStore* GetTrustStore() override { return &trust_store_; }

  bool IsKnownRoot(const bssl::ParsedCertificate* trust_anchor) const override {
    return mock_is_known_root_;
  }

  void AddTrustStore(bssl::TrustStore* store) {
    trust_store_.AddTrustStore(store);
  }

  void SetMockIsKnownRoot(bool is_known_root) {
    mock_is_known_root_ = is_known_root;
  }

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  int64_t chrome_root_store_version() const override { return 0; }
#endif

 private:
  bssl::TrustStoreCollection trust_store_;
  bool mock_is_known_root_ = false;
};

class BlockingTrustStore : public bssl::TrustStore {
 public:
  bssl::CertificateTrust GetTrust(
      const bssl::ParsedCertificate* cert) override {
    return backing_trust_store_.GetTrust(cert);
  }

  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override {
    sync_get_issuer_started_event_.Signal();
    sync_get_issuer_ok_to_finish_event_.Wait();

    backing_trust_store_.SyncGetIssuersOf(cert, issuers);
  }

  base::WaitableEvent sync_get_issuer_started_event_;
  base::WaitableEvent sync_get_issuer_ok_to_finish_event_;
  bssl::TrustStoreInMemory backing_trust_store_;
};

}  // namespace

class CertVerifyProcBuiltinTest : public ::testing::Test {
 public:
  void SetUp() override {
    cert_net_fetcher_ = base::MakeRefCounted<CertNetFetcherURLRequest>();

    InitializeVerifyProc({});

    context_ = CreateTestURLRequestContextBuilder()->Build();

    cert_net_fetcher_->SetURLRequestContext(context_.get());
  }

  void TearDown() override { cert_net_fetcher_->Shutdown(); }

  void InitializeVerifyProc(const CertificateList& additional_trust_anchors) {
    auto mock_system_trust_store = std::make_unique<MockSystemTrustStore>();
    mock_system_trust_store_ = mock_system_trust_store.get();
    CertVerifyProc::InstanceParams instance_params;
    instance_params.additional_trust_anchors = additional_trust_anchors;
    verify_proc_ = CreateCertVerifyProcBuiltin(
        cert_net_fetcher_, CRLSet::EmptyCRLSetForTesting(),
        std::move(mock_system_trust_store), instance_params);
  }

  void Verify(scoped_refptr<X509Certificate> cert,
              const std::string& hostname,
              int flags,
              CertVerifyResult* verify_result,
              NetLogSource* out_source,
              CompletionOnceCallback callback) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&VerifyOnWorkerThread, verify_proc_, std::move(cert),
                       hostname, flags, verify_result, out_source),
        std::move(callback));
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  // Creates a CRL issued and signed by |crl_issuer|, marking |revoked_serials|
  // as revoked, and registers it to be served by the test server.
  // Returns the full URL to retrieve the CRL from the test server.
  GURL CreateAndServeCrl(EmbeddedTestServer* test_server,
                         CertBuilder* crl_issuer,
                         const std::vector<uint64_t>& revoked_serials,
                         absl::optional<bssl::SignatureAlgorithm>
                             signature_algorithm = absl::nullopt) {
    std::string crl = BuildCrl(crl_issuer->GetSubject(), crl_issuer->GetKey(),
                               revoked_serials, signature_algorithm);
    std::string crl_path = MakeRandomPath(".crl");
    test_server->RegisterRequestHandler(
        base::BindRepeating(&test_server::HandlePrefixedRequest, crl_path,
                            base::BindRepeating(ServeResponse, HTTP_OK,
                                                "application/pkix-crl", crl)));
    return test_server->GetURL(crl_path);
  }

  void AddTrustStore(bssl::TrustStore* store) {
    mock_system_trust_store_->AddTrustStore(store);
  }

  void SetMockIsKnownRoot(bool is_known_root) {
    mock_system_trust_store_->SetMockIsKnownRoot(is_known_root);
  }

  net::URLRequestContext* context() { return context_.get(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO,
  };

  CertVerifier::Config config_;
  std::unique_ptr<net::URLRequestContext> context_;

  // Must outlive `mock_system_trust_store_`.
  scoped_refptr<CertVerifyProc> verify_proc_;

  raw_ptr<MockSystemTrustStore> mock_system_trust_store_ = nullptr;
  scoped_refptr<CertNetFetcherURLRequest> cert_net_fetcher_;
};

TEST_F(CertVerifyProcBuiltinTest, ShouldBypassHSTS) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  InitializeVerifyProc(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // CRL that marks leaf as revoked.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(&test_server, root.get(), {leaf->GetSerialNumber()}));

  test_server.StartAcceptingConnections();

  {
    scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
    ASSERT_TRUE(chain.get());

    NetLogSource verify_net_log_source;
    CertVerifyResult verify_result;
    TestCompletionCallback verify_callback;
    // Ensure HSTS upgrades for the domain which hosts the CRLs.
    context()->transport_security_state()->AddHSTS(
        test_server.base_url().host(), base::Time::Now() + base::Seconds(30),
        /*include_subdomains=*/true);
    ASSERT_TRUE(context()->transport_security_state()->ShouldUpgradeToSSL(
        test_server.base_url().host()));
    Verify(chain.get(), "www.example.com",
           CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
           &verify_result, &verify_net_log_source, verify_callback.callback());

    int error = verify_callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
  }
}

TEST_F(CertVerifyProcBuiltinTest, SimpleSuccess) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  base::HistogramTester histogram_tester;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0, &verify_result,
         &verify_net_log_source, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Net.CertVerifier.PathBuilderIterationCount"),
              testing::ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
}

TEST_F(CertVerifyProcBuiltinTest, CRLNotCheckedForKnownRoots) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  InitializeVerifyProc(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // CRL that marks leaf as revoked.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(&test_server, root.get(), {leaf->GetSerialNumber()}));

  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  NetLogSource verify_net_log_source;

  {
    CertVerifyResult verify_result;
    TestCompletionCallback verify_callback;
    Verify(chain.get(), "www.example.com",
           CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
           &verify_result, &verify_net_log_source, verify_callback.callback());

    int error = verify_callback.WaitForResult();
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
  }

  {
    // Pretend the root is a known root.
    SetMockIsKnownRoot(true);
    base::HistogramTester histogram_tester;
    CertVerifyResult verify_result;
    TestCompletionCallback verify_callback;
    Verify(chain.get(), "www.example.com",
           CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
           &verify_result, &verify_net_log_source, verify_callback.callback());

    int error = verify_callback.WaitForResult();
    // CRLs are not checked for chains issued by known roots, so verification
    // should be successful.
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Net.CertVerifier.PathBuilderIterationCount"),
                testing::ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  }
}

// Tests that if the verification deadline is exceeded during revocation
// checking, additional CRL fetches will not be attempted.
TEST_F(CertVerifyProcBuiltinTest, RevocationCheckDeadlineCRL) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});

  const base::TimeDelta timeout_increment =
      CertNetFetcherURLRequest::GetDefaultTimeoutForTesting() +
      base::Milliseconds(1);
  const int expected_request_count =
      base::ClampFloor(GetCertVerifyProcBuiltinTimeLimitForTesting() /
                       timeout_increment) +
      1;

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // Set up the test cert to have enough crlDistributionPoint urls that if the
  // first N-1 requests hang the deadline will be exceeded before the Nth
  // request is made.
  std::vector<GURL> crl_urls;
  std::vector<base::RunLoop> runloops(expected_request_count);
  for (int i = 0; i < expected_request_count; ++i) {
    std::string path = base::StringPrintf("/hung/%i", i);
    crl_urls.emplace_back(test_server.GetURL(path));
    test_server.RegisterRequestHandler(
        base::BindRepeating(&test_server::HandlePrefixedRequest, path,
                            base::BindRepeating(&HangRequestAndCallback,
                                                runloops[i].QuitClosure())));
  }
  // Add CRL URLs and handlers that will add test failures if requested.
  for (int i = expected_request_count; i < expected_request_count + 1; ++i) {
    std::string path = base::StringPrintf("/failtest/%i", i);
    crl_urls.emplace_back(test_server.GetURL(path));
    test_server.RegisterRequestHandler(base::BindRepeating(
        &test_server::HandlePrefixedRequest, path,
        base::BindRepeating(FailRequestAndFailTest,
                            "additional request made after deadline exceeded",
                            base::SequencedTaskRunner::GetCurrentDefault())));
  }
  leaf->SetCrlDistributionPointUrls(crl_urls);

  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  base::HistogramTester histogram_tester;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
         &verify_result, &verify_net_log_source, verify_callback.callback());

  for (int i = 0; i < expected_request_count; i++) {
    // Wait for request #|i| to be made.
    runloops[i].Run();
    // Advance virtual time to cause the timeout task to become runnable.
    task_environment().AdvanceClock(timeout_increment);
  }

  // Once |expected_request_count| requests have been made and timed out, the
  // overall deadline should be reached, and no more requests should have been
  // made. (If they were, the test will fail due to the ADD_FAILURE callback in
  // the request handlers.)
  int error = verify_callback.WaitForResult();
  // Soft-fail revocation checking was used, therefore verification result
  // should be OK even though none of the CRLs could be retrieved.
  EXPECT_THAT(error, IsOk());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Net.CertVerifier.PathBuilderIterationCount"),
              testing::ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
}

// Tests that if the verification deadline is exceeded during revocation
// checking, additional OCSP fetches will not be attempted.
TEST_F(CertVerifyProcBuiltinTest, RevocationCheckDeadlineOCSP) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});

  const base::TimeDelta timeout_increment =
      CertNetFetcherURLRequest::GetDefaultTimeoutForTesting() +
      base::Milliseconds(1);
  const int expected_request_count =
      base::ClampFloor(GetCertVerifyProcBuiltinTimeLimitForTesting() /
                       timeout_increment) +
      1;

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // Set up the test cert to have enough OCSP urls that if the
  // first N-1 requests hang the deadline will be exceeded before the Nth
  // request is made.
  std::vector<GURL> ocsp_urls;
  std::vector<base::RunLoop> runloops(expected_request_count);
  for (int i = 0; i < expected_request_count; ++i) {
    std::string path = base::StringPrintf("/hung/%i", i);
    ocsp_urls.emplace_back(test_server.GetURL(path));
    test_server.RegisterRequestHandler(
        base::BindRepeating(&test_server::HandlePrefixedRequest, path,
                            base::BindRepeating(&HangRequestAndCallback,
                                                runloops[i].QuitClosure())));
  }
  // Add OCSP URLs and handlers that will add test failures if requested.
  for (int i = expected_request_count; i < expected_request_count + 1; ++i) {
    std::string path = base::StringPrintf("/failtest/%i", i);
    ocsp_urls.emplace_back(test_server.GetURL(path));
    test_server.RegisterRequestHandler(base::BindRepeating(
        &test_server::HandlePrefixedRequest, path,
        base::BindRepeating(FailRequestAndFailTest,
                            "additional request made after deadline exceeded",
                            base::SequencedTaskRunner::GetCurrentDefault())));
  }
  leaf->SetCaIssuersAndOCSPUrls({}, ocsp_urls);

  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
         &verify_result, &verify_net_log_source, verify_callback.callback());

  for (int i = 0; i < expected_request_count; i++) {
    // Wait for request #|i| to be made.
    runloops[i].Run();
    // Advance virtual time to cause the timeout task to become runnable.
    task_environment().AdvanceClock(timeout_increment);
  }

  // Once |expected_request_count| requests have been made and timed out, the
  // overall deadline should be reached, and no more requests should have been
  // made. (If they were, the test will fail due to the ADD_FAILURE callback in
  // the request handlers.)
  int error = verify_callback.WaitForResult();
  // Soft-fail revocation checking was used, therefore verification result
  // should be OK even though none of the OCSP responses could be retrieved.
  EXPECT_THAT(error, IsOk());
}

#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
// Tests that if we're doing EV verification, that no OCSP revocation checking
// is done.
TEST_F(CertVerifyProcBuiltinTest, EVNoOCSPRevocationChecks) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});

  // Add test EV policy to leaf and intermediate.
  static const char kEVTestCertPolicy[] = "1.2.3.4";
  leaf->SetCertificatePolicies({kEVTestCertPolicy});
  intermediate->SetCertificatePolicies({kEVTestCertPolicy});

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // Set up the test intermediate to have an OCSP url that fails the test if
  // called.
  std::vector<GURL> ocsp_urls;
  std::string path = "/failtest";
  ocsp_urls.emplace_back(test_server.GetURL(path));
  test_server.RegisterRequestHandler(base::BindRepeating(
      &test_server::HandlePrefixedRequest, path,
      base::BindRepeating(FailRequestAndFailTest,
                          "no OCSP requests should be sent",
                          base::SequencedTaskRunner::GetCurrentDefault())));
  intermediate->SetCaIssuersAndOCSPUrls({}, ocsp_urls);
  test_server.StartAcceptingConnections();

  // Consider the root of the test chain a valid EV root for the test policy.
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(root->GetCertBuffer()),
      kEVTestCertPolicy);

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         /*flags=*/0,
         &verify_result, &verify_net_log_source, verify_callback.callback());

  // EV doesn't do revocation checking, therefore verification result
  // should be OK and EV.
  int error = verify_callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);

  auto events = net_log_observer.GetEntriesForSource(verify_net_log_source);

  auto event = base::ranges::find(
      events, NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT,
      &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);
  EXPECT_EQ(true, event->params.FindBool("is_ev_attempt"));

  event = base::ranges::find(++event, events.end(),
                             NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                             &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::NONE, event->phase);
  EXPECT_FALSE(event->params.FindString("errors"));

  event = base::ranges::find(
      ++event, events.end(),
      NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT, &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::END, event->phase);
  EXPECT_EQ(true, event->params.FindBool("has_valid_path"));
}
#endif  // defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

TEST_F(CertVerifyProcBuiltinTest, DeadlineExceededDuringSyncGetIssuers) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  InitializeVerifyProc(
      /*additional_trust_anchors=*/{root->GetX509Certificate()});

  BlockingTrustStore trust_store;
  AddTrustStore(&trust_store);

  auto intermediate_parsed_cert = bssl::ParsedCertificate::Create(
      intermediate->DupCertBuffer(), {}, nullptr);
  ASSERT_TRUE(intermediate_parsed_cert);
  trust_store.backing_trust_store_.AddCertificateWithUnspecifiedTrust(
      intermediate_parsed_cert);

  scoped_refptr<X509Certificate> chain = leaf->GetX509Certificate();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         /*flags=*/0,
         &verify_result, &verify_net_log_source, verify_callback.callback());

  // Wait for trust_store.SyncGetIssuersOf to be called.
  trust_store.sync_get_issuer_started_event_.Wait();

  // Advance the clock past the verifier deadline.
  const base::TimeDelta timeout_increment =
      GetCertVerifyProcBuiltinTimeLimitForTesting() + base::Milliseconds(1);
  task_environment().AdvanceClock(timeout_increment);

  // Signal trust_store.SyncGetIssuersOf to finish.
  trust_store.sync_get_issuer_ok_to_finish_event_.Signal();

  int error = verify_callback.WaitForResult();
  // Because the deadline was reached while retrieving the intermediate, path
  // building should have stopped there and not found the root. The partial
  // path built up to that point should be returned, and the error should be
  // CERT_AUTHORITY_INVALID.
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  ASSERT_EQ(1u, verify_result.verified_cert->intermediate_buffers().size());
  EXPECT_EQ(intermediate->GetCertBuffer(),
            verify_result.verified_cert->intermediate_buffers()[0].get());
}

namespace {

// Returns a TLV to use as an unknown signature algorithm when building a cert.
// The specific contents are as follows (the OID is from
// https://davidben.net/oid):
//
// SEQUENCE {
//   OBJECT_IDENTIFIER { 1.2.840.113554.4.1.72585.0 }
//   NULL {}
// }
std::string UnknownSignatureAlgorithmTLV() {
  const uint8_t kInvalidSignatureAlgorithmTLV[] = {
      0x30, 0x10, 0x06, 0x0c, 0x2a, 0x86, 0x48, 0x86, 0xf7,
      0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x00, 0x05, 0x00};
  return std::string(std::begin(kInvalidSignatureAlgorithmTLV),
                     std::end(kInvalidSignatureAlgorithmTLV));
}

// Returns a TLV to use as an invalid signature algorithm when building a cert.
// This is a SEQUENCE so that it will pass the bssl::ParseCertificate code
// and fail inside bssl::ParseSignatureAlgorithm.
// SEQUENCE {
//   INTEGER { 42 }
// }
std::string InvalidSignatureAlgorithmTLV() {
  const uint8_t kInvalidSignatureAlgorithmTLV[] = {0x30, 0x03, 0x02, 0x01,
                                                   0x2a};
  return std::string(std::begin(kInvalidSignatureAlgorithmTLV),
                     std::end(kInvalidSignatureAlgorithmTLV));
}

}  // namespace

TEST_F(CertVerifyProcBuiltinTest, UnknownSignatureAlgorithmTarget) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  leaf->SetSignatureAlgorithmTLV(UnknownSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Unknown signature algorithm in the leaf cert should result in the cert
  // being invalid.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest,
       UnparsableMismatchedTBSSignatureAlgorithmTarget) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  // Set only the tbsCertificate signature to an invalid value.
  leaf->SetTBSSignatureAlgorithmTLV(InvalidSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Invalid signature algorithm in the leaf cert should result in the
  // cert being invalid.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest, UnknownSignatureAlgorithmIntermediate) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  intermediate->SetSignatureAlgorithmTLV(UnknownSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Unknown signature algorithm in the intermediate cert should result in the
  // cert being invalid.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest,
       UnparsableMismatchedTBSSignatureAlgorithmIntermediate) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  // Set only the tbsCertificate signature to an invalid value.
  intermediate->SetTBSSignatureAlgorithmTLV(InvalidSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());
  ASSERT_EQ(chain->intermediate_buffers().size(), 1U);

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Invalid signature algorithm in the intermediate cert should result in the
  // cert being invalid.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

TEST_F(CertVerifyProcBuiltinTest, UnknownSignatureAlgorithmRoot) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  root->SetSignatureAlgorithmTLV(UnknownSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Unknown signature algorithm in the root cert should have no effect on
  // verification.
  EXPECT_THAT(error, IsOk());
}

// This test is disabled on Android as adding the invalid root through
// ScopedTestRoot causes it to be parsed by the Java X509 code which barfs. We
// could re-enable if Chrome on Android has fully switched to the
// builtin-verifier and ScopedTestRoot no longer has Android-specific code.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_UnparsableMismatchedTBSSignatureAlgorithmRoot \
  DISABLED_UnparsableMismatchedTBSSignatureAlgorithmRoot
#else
#define MAYBE_UnparsableMismatchedTBSSignatureAlgorithmRoot \
  UnparsableMismatchedTBSSignatureAlgorithmRoot
#endif
TEST_F(CertVerifyProcBuiltinTest,
       MAYBE_UnparsableMismatchedTBSSignatureAlgorithmRoot) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  // Set only the tbsCertificate signature to an invalid value.
  root->SetTBSSignatureAlgorithmTLV(InvalidSignatureAlgorithmTLV());

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();
  // Invalid signature algorithm in the root cert should have no effect on
  // verification.
  EXPECT_THAT(error, IsOk());
}

class CertVerifyProcBuiltinIterationTest
    : public CertVerifyProcBuiltinTest,
      public testing::WithParamInterface<bool> {
 public:
  CertVerifyProcBuiltinIterationTest() {
    if (new_iteration_limit()) {
      feature_list_.InitAndEnableFeature(
          features::kNewCertPathBuilderIterationLimit);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kNewCertPathBuilderIterationLimit);
    }
  }

  bool new_iteration_limit() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(CertVerifyProcBuiltinIterationTest, IterationLimit) {
  // Create a chain which will require many iterations in the path builder.
  std::vector<std::unique_ptr<CertBuilder>> builders =
      CertBuilder::CreateSimpleChain(6);

  base::Time not_before = base::Time::Now() - base::Days(1);
  base::Time not_after = base::Time::Now() + base::Days(1);
  for (auto& builder : builders) {
    builder->SetValidity(not_before, not_after);
  }

  // Generate certificates, making two versions of each intermediate.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (size_t i = 1; i < builders.size(); i++) {
    intermediates.push_back(builders[i]->DupCertBuffer());
    builders[i]->SetValidity(not_before, not_after + base::Seconds(1));
    intermediates.push_back(builders[i]->DupCertBuffer());
  }

  // The above alone is enough to make the path builder explore many paths, but
  // it will always return the best path it has found, so the error will be the
  // same. Instead, arrange for all those paths to be invalid (untrusted root),
  // and add a separate chain that is valid.
  CertBuilder root_ok(/*orig_cert=*/builders[2]->GetCertBuffer(),
                      /*issuer=*/nullptr);
  CertBuilder intermediate_ok(/*orig_cert=*/builders[1]->GetCertBuffer(),
                              /*issuer=*/&root_ok);
  // Using the old intermediate as a template does not preserve the subject,
  // SKID, or key.
  intermediate_ok.SetSubjectTLV(
      base::as_bytes(base::make_span(builders[1]->GetSubject())));
  intermediate_ok.SetKey(bssl::UpRef(builders[1]->GetKey()));
  intermediate_ok.SetSubjectKeyIdentifier(
      builders[1]->GetSubjectKeyIdentifier());
  // Make the valid intermediate older than the invalid ones, so that it is
  // explored last.
  intermediate_ok.SetValidity(not_before - base::Seconds(10),
                              not_after - base::Seconds(10));
  intermediates.push_back(intermediate_ok.DupCertBuffer());

  // Verify the chain.
  ScopedTestRoot scoped_root(root_ok.GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = X509Certificate::CreateFromBuffer(
      builders[0]->DupCertBuffer(), std::move(intermediates));
  ASSERT_TRUE(chain.get());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  int flags = 0;
  CertVerifyResult verify_result;
  NetLogSource verify_net_log_source;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", flags, &verify_result,
         &verify_net_log_source, callback.callback());
  int error = callback.WaitForResult();

  auto events = net_log_observer.GetEntriesForSource(verify_net_log_source);
  auto event = base::ranges::find_if(events, [](const NetLogEntry& e) {
    return e.type == NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT &&
           e.phase == NetLogEventPhase::END;
  });
  ASSERT_NE(event, events.end());

  if (new_iteration_limit()) {
    // The path builder gives up before it finishes all the invalid paths.
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_EQ(true, event->params.FindBool("exceeded_iteration_limit"));
  } else {
    // After exploring many dead ends, the path builder finds the valid path.
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(event->params.Find("exceeded_iteration_limit"));
  }
}

INSTANTIATE_TEST_SUITE_P(NewLimit,
                         CertVerifyProcBuiltinIterationTest,
                         testing::Bool());

}  // namespace net
