// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_builtin.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/crl_set.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert_net/cert_net_fetcher_impl.h"
#include "net/der/encode_values.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/gtest_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

class DummySystemTrustStoreProvider : public SystemTrustStoreProvider {
 public:
  std::unique_ptr<SystemTrustStore> CreateSystemTrustStore() override {
    return CreateEmptySystemTrustStore();
  }
};

std::unique_ptr<test_server::HttpResponse> HangRequestAndCallback(
    base::Closure callback,
    const test_server::HttpRequest& request) {
  callback.Run();
  return std::make_unique<test_server::HungResponse>();
}

void FailTest(const std::string& message) {
  ADD_FAILURE() << message;
}

std::unique_ptr<test_server::HttpResponse> FailRequestAndFailTest(
    const std::string& message,
    scoped_refptr<base::TaskRunner> main_task_runner,
    const test_server::HttpRequest& request) {
  main_task_runner->PostTask(FROM_HERE, base::Bind(FailTest, message));
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_NOT_ACCEPTABLE);
  return response;
}

int VerifyOnWorkerThread(const scoped_refptr<CertVerifyProc>& verify_proc,
                         scoped_refptr<X509Certificate> cert,
                         const std::string& hostname,
                         int flags,
                         const CertificateList& additional_trust_anchors,
                         CertVerifyResult* verify_result) {
  base::ScopedAllowBaseSyncPrimitivesForTesting scoped_allow_blocking;
  scoped_refptr<CRLSet> crl_set = CRLSet::EmptyCRLSetForTesting();
  int error =
      verify_proc->Verify(cert.get(), hostname,
                          /*ocsp_response=*/std::string(),
                          /*sct_list=*/std::string(), flags, crl_set.get(),
                          additional_trust_anchors, verify_result);
  verify_result->DetachFromSequence();
  return error;
}

}  // namespace

class CertVerifyProcBuiltinTest : public ::testing::Test {
 public:
  // CertVerifyProcBuiltinTest() {}

  void SetUp() override {
    cert_net_fetcher_ = base::MakeRefCounted<CertNetFetcherImpl>();
    verify_proc_ = CreateCertVerifyProcBuiltin(
        cert_net_fetcher_, std::make_unique<DummySystemTrustStoreProvider>());

    context_ = std::make_unique<net::TestURLRequestContext>();

    cert_net_fetcher_->SetURLRequestContext(context_.get());
  }

  void TearDown() override { cert_net_fetcher_->Shutdown(); }

  void Verify(scoped_refptr<X509Certificate> cert,
              const std::string& hostname,
              int flags,
              const CertificateList& additional_trust_anchors,
              CertVerifyResult* verify_result,
              CompletionOnceCallback callback) {
    verify_result->DetachFromSequence();
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&VerifyOnWorkerThread, verify_proc_, std::move(cert),
                       hostname, flags, additional_trust_anchors,
                       verify_result),
        std::move(callback));
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void CreateChain(std::unique_ptr<CertBuilder>* out_leaf,
                   std::unique_ptr<CertBuilder>* out_intermediate,
                   std::unique_ptr<CertBuilder>* out_root) {
    CertBuilder::CreateSimpleChain(out_leaf, out_intermediate, out_root);
    ASSERT_TRUE(*out_leaf && *out_intermediate && *out_root);
    // This test uses MOCK_TIME, so need to set the cert validity dates based
    // on whatever the mock time happens to start at.
    base::Time not_before = base::Time::Now() - base::TimeDelta::FromDays(1);
    base::Time not_after = base::Time::Now() + base::TimeDelta::FromDays(10);
    (*out_leaf)->SetValidity(not_before, not_after);
    (*out_intermediate)->SetValidity(not_before, not_after);
    (*out_root)->SetValidity(not_before, not_after);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO,
  };

  CertVerifier::Config config_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  scoped_refptr<CertVerifyProc> verify_proc_;
  scoped_refptr<CertNetFetcherImpl> cert_net_fetcher_;
};

TEST_F(CertVerifyProcBuiltinTest, SimpleSuccess) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CreateChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0,
         /*additional_trust_anchors=*/{root->GetX509Certificate()},
         &verify_result, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());
}

// Tests that if the verification deadline is exceeded during revocation
// checking, additional CRL fetches will not be attempted.
TEST_F(CertVerifyProcBuiltinTest, RevocationCheckDeadlineCRL) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CreateChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  const base::TimeDelta timeout_increment =
      CertNetFetcherImpl::GetDefaultTimeoutForTesting() +
      base::TimeDelta::FromMilliseconds(1);
  const int expected_request_count =
      GetCertVerifyProcBuiltinTimeLimitForTesting() / timeout_increment + 1;

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
    test_server.RegisterRequestHandler(base::Bind(
        &test_server::HandlePrefixedRequest, path,
        base::BindRepeating(FailRequestAndFailTest,
                            "additional request made after deadline exceeded",
                            base::SequencedTaskRunnerHandle::Get())));
  }
  leaf->SetCrlDistributionPointUrls(crl_urls);

  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
         /*additional_trust_anchors=*/{root->GetX509Certificate()},
         &verify_result, verify_callback.callback());

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
}

// Tests that if the verification deadline is exceeded during revocation
// checking, additional OCSP fetches will not be attempted.
TEST_F(CertVerifyProcBuiltinTest, RevocationCheckDeadlineOCSP) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CreateChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  const base::TimeDelta timeout_increment =
      CertNetFetcherImpl::GetDefaultTimeoutForTesting() +
      base::TimeDelta::FromMilliseconds(1);
  const int expected_request_count =
      GetCertVerifyProcBuiltinTimeLimitForTesting() / timeout_increment + 1;

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
    test_server.RegisterRequestHandler(base::Bind(
        &test_server::HandlePrefixedRequest, path,
        base::BindRepeating(FailRequestAndFailTest,
                            "additional request made after deadline exceeded",
                            base::SequencedTaskRunnerHandle::Get())));
  }
  leaf->SetCaIssuersAndOCSPUrls({}, ocsp_urls);

  test_server.StartAcceptingConnections();

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
         /*additional_trust_anchors=*/{root->GetX509Certificate()},
         &verify_result, verify_callback.callback());

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
// Tests that if the verification deadline is exceeded during EV revocation
// checking, the certificate is verified as non-EV.
TEST_F(CertVerifyProcBuiltinTest, EVRevocationCheckDeadline) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CreateChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Add test EV policy to leaf and intermediate.
  static const char kEVTestCertPolicy[] = "1.2.3.4";
  leaf->SetCertificatePolicies({kEVTestCertPolicy});
  intermediate->SetCertificatePolicies({kEVTestCertPolicy});

  const base::TimeDelta timeout_increment =
      CertNetFetcherImpl::GetDefaultTimeoutForTesting() +
      base::TimeDelta::FromMilliseconds(1);
  const int expected_request_count =
      GetCertVerifyProcBuiltinTimeLimitForTesting() / timeout_increment + 1;

  EmbeddedTestServer test_server(EmbeddedTestServer::TYPE_HTTP);
  ASSERT_TRUE(test_server.InitializeAndListen());

  // Set up the test intermediate to have enough OCSP urls that if all the
  // requests hang the deadline will be exceeded.
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
  intermediate->SetCaIssuersAndOCSPUrls({}, ocsp_urls);

  test_server.StartAcceptingConnections();

  // Consider the root of the test chain a valid EV root for the test policy.
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(root->GetCertBuffer()),
      kEVTestCertPolicy);

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  CertVerifyResult verify_result;
  TestCompletionCallback verify_callback;
  Verify(chain.get(), "www.example.com",
         /*flags=*/0,
         /*additional_trust_anchors=*/{root->GetX509Certificate()},
         &verify_result, verify_callback.callback());

  for (int i = 0; i < expected_request_count; i++) {
    // Wait for request #|i| to be made.
    runloops[i].Run();
    // Advance virtual time to cause the timeout task to become runnable.
    task_environment().AdvanceClock(timeout_increment);
  }

  // Once |expected_request_count| requests have been made and timed out, the
  // overall deadline should be reached, causing the EV verification attempt to
  // fail.
  int error = verify_callback.WaitForResult();
  // EV uses soft-fail revocation checking, therefore verification result
  // should be OK but not EV.
  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_EV);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}
#endif  // defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

TEST_F(CertVerifyProcBuiltinTest, DebugData) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CreateChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  base::Time time = base::Time::Now();

  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  Verify(chain.get(), "www.example.com", /*flags=*/0,
         /*additional_trust_anchors=*/{root->GetX509Certificate()},
         &verify_result, callback.callback());

  int error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  auto* debug_data = CertVerifyProcBuiltinResultDebugData::Get(&verify_result);
  ASSERT_TRUE(debug_data);
  // No delayed tasks involved, so the mock time should not have advanced.
  EXPECT_EQ(time, debug_data->verification_time());

  base::Time der_verification_time_converted_back_to_base_time;
  EXPECT_TRUE(net::der::GeneralizedTimeToTime(
      debug_data->der_verification_time(),
      &der_verification_time_converted_back_to_base_time));
  // GeneralizedTime only has seconds precision.
  EXPECT_EQ(
      0,
      (time - der_verification_time_converted_back_to_base_time).InSeconds());
}

}  // namespace net
