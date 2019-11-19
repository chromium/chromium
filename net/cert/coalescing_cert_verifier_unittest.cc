// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/coalescing_cert_verifier.h"

#include <memory>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

using CoalescingCertVerifierTest = TestWithTaskEnvironment;

// Tests that synchronous completion does not cause any issues.
TEST_F(CoalescingCertVerifierTest, SyncCompletion) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(false);  // Force sync completion.
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  CoalescingCertVerifier verifier(std::move(mock_verifier_owner));

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1, result2;
  TestCompletionCallback callback1, callback2;
  std::unique_ptr<CertVerifier::Request> request1, request2;

  // Start an (asynchronous) initial request.
  int error = verifier.Verify(request_params, &result1, callback1.callback(),
                              &request1, NetLogWithSource());
  ASSERT_THAT(error, IsOk());
  ASSERT_FALSE(request1);
  ASSERT_TRUE(result1.verified_cert);
}

// Test that requests with identical parameters only result in a single
// underlying verification; that is, the second Request is joined to the
// in-progress first Request.
TEST_F(CoalescingCertVerifierTest, InflightJoin) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  base::HistogramTester histograms;

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(true);  // Always complete via PostTask
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  CoalescingCertVerifier verifier(std::move(mock_verifier_owner));

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1, result2;
  TestCompletionCallback callback1, callback2;
  std::unique_ptr<CertVerifier::Request> request1, request2;

  // Start an (asynchronous) initial request.
  int error = verifier.Verify(request_params, &result1, callback1.callback(),
                              &request1, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request1);

  // Simulate the underlying verifier returning different results if another
  // verification is done.
  mock_verifier->ClearRules();
  mock_verifier->AddResultForCert(test_cert, fake_result, ERR_CERT_REVOKED);

  // Start a second request; this should join the first request.
  error = verifier.Verify(request_params, &result2, callback2.callback(),
                          &request2, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  // Ensure only one request was ever started.
  EXPECT_EQ(2u, verifier.requests_for_testing());
  EXPECT_EQ(1u, verifier.inflight_joins_for_testing());

  // Make sure both results completed.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  // There should only have been one Job started.
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms.ExpectTotalCount("Net.CertVerifier_First_Job_Latency", 1);
}

// Test that changing configurations between Requests prevents the second
// Request from being attached to the first Request. There should be two
// Requests to the underlying CertVerifier, and the correct results should be
// received by each.
TEST_F(CoalescingCertVerifierTest, DoesNotJoinAfterConfigChange) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  base::HistogramTester histograms;

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(true);  // Always complete via PostTask
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  CoalescingCertVerifier verifier(std::move(mock_verifier_owner));

  CertVerifier::Config config1;
  verifier.SetConfig(config1);

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1, result2;
  TestCompletionCallback callback1, callback2;
  std::unique_ptr<CertVerifier::Request> request1, request2;

  // Start an (asynchronous) initial request.
  int error = verifier.Verify(request_params, &result1, callback1.callback(),
                              &request1, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request1);

  // Change the configuration, and change the result to to simulate the
  // configuration change affecting behavior.
  CertVerifier::Config config2;
  config2.enable_rev_checking = !config1.enable_rev_checking;
  verifier.SetConfig(config2);
  mock_verifier->ClearRules();
  mock_verifier->AddResultForCert(test_cert, fake_result, ERR_CERT_REVOKED);

  // Start a second request; this should not join the first request, as the
  // config is different.
  error = verifier.Verify(request_params, &result2, callback2.callback(),
                          &request2, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  // Ensure a total of two requests were started, and neither were joined.
  EXPECT_EQ(2u, verifier.requests_for_testing());
  EXPECT_EQ(0u, verifier.inflight_joins_for_testing());

  // Make sure both results completed.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  EXPECT_THAT(callback2.WaitForResult(), IsError(ERR_CERT_REVOKED));

  // There should have been two separate Jobs.
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 2);
  histograms.ExpectTotalCount("Net.CertVerifier_First_Job_Latency", 1);
}

// Test that when two Requests are attached to the same Job, it's safe to
// delete the second Request while processing the response to the first. The
// second Request should not cause the second callback to be called.
TEST_F(CoalescingCertVerifierTest, DeleteSecondRequestDuringFirstCompletion) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(true);  // Always complete via PostTask
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  CoalescingCertVerifier verifier(std::move(mock_verifier_owner));

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1, result2;
  TestCompletionCallback callback1, callback2;
  std::unique_ptr<CertVerifier::Request> request1, request2;

  // Start an (asynchronous) initial request. When this request is completed,
  // it will delete (reset) |request2|, which should prevent it from being
  // called.
  int error = verifier.Verify(
      request_params, &result1,
      base::BindLambdaForTesting([&callback1, &request2](int result) {
        request2.reset();
        callback1.callback().Run(result);
      }),
      &request1, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request1);

  // Start a second request; this should join the first request.
  error = verifier.Verify(request_params, &result2, callback2.callback(),
                          &request2, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  // Ensure only one underlying verification was started.
  ASSERT_EQ(2u, verifier.requests_for_testing());
  ASSERT_EQ(1u, verifier.inflight_joins_for_testing());

  // Make sure that only the first callback is invoked; because the second
  // CertVerifier::Request was deleted during processing the first's callback,
  // the second callback should not be invoked.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  ASSERT_FALSE(callback2.have_result());
  ASSERT_FALSE(request2);

  // While CoalescingCertVerifier doesn't use PostTask, make sure to flush the
  // tasks as well, in case the implementation changes in the future.
  RunUntilIdle();
  ASSERT_FALSE(callback2.have_result());
  ASSERT_FALSE(request2);
}

// Test that it's safe to delete the CoalescingCertVerifier during completion,
// even when there are outstanding Requests to be processed. The additional
// Requests should not invoke the user callback once the
// CoalescingCertVerifier is deleted.
TEST_F(CoalescingCertVerifierTest, DeleteVerifierDuringCompletion) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(true);  // Always complete via PostTask
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  auto verifier =
      std::make_unique<CoalescingCertVerifier>(std::move(mock_verifier_owner));

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1, result2;
  TestCompletionCallback callback1, callback2;
  std::unique_ptr<CertVerifier::Request> request1, request2;

  // Start an (asynchronous) initial request. When this request is completed,
  // it will delete (reset) |request2|, which should prevent it from being
  // called.
  int error = verifier->Verify(
      request_params, &result1,
      base::BindLambdaForTesting([&callback1, &verifier](int result) {
        verifier.reset();
        callback1.callback().Run(result);
      }),
      &request1, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request1);

  // Start a second request; this should join the first request.
  error = verifier->Verify(request_params, &result2, callback2.callback(),
                           &request2, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  // Ensure only one underlying verification was started.
  ASSERT_EQ(2u, verifier->requests_for_testing());
  ASSERT_EQ(1u, verifier->inflight_joins_for_testing());

  // Make sure that only the first callback is invoked. This will delete the
  // underlying CoalescingCertVerifier, which should prevent the second
  // request's callback from being invoked.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
  ASSERT_FALSE(callback2.have_result());
  ASSERT_TRUE(request2);

  // While CoalescingCertVerifier doesn't use PostTask, make sure to flush the
  // tasks as well, in case the implementation changes in the future.
  RunUntilIdle();
  ASSERT_FALSE(callback2.have_result());
  ASSERT_TRUE(request2);
}

// Test that it's safe to delete a Request before the underlying verifier has
// completed. This is a guard against memory safety (e.g. when this Request
// is the last/only Request remaining).
TEST_F(CoalescingCertVerifierTest, DeleteRequestBeforeCompletion) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(true);  // Always complete via PostTask
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  CoalescingCertVerifier verifier(std::move(mock_verifier_owner));

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1;
  TestCompletionCallback callback1;
  std::unique_ptr<CertVerifier::Request> request1;

  // Start an (asynchronous) initial request.
  int error = verifier.Verify(request_params, &result1, callback1.callback(),
                              &request1, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request1);

  // Abandon the request before it's completed.
  request1.reset();
  EXPECT_FALSE(callback1.have_result());

  // Make sure the request never completes / the callback is never invoked.
  RunUntilIdle();
  EXPECT_FALSE(callback1.have_result());
}

// Test that it's safe to delete a Request before the underlying verifier has
// completed. This is a correctness test, to ensure that other Requests are
// still notified.
TEST_F(CoalescingCertVerifierTest,
       DeleteFirstRequestBeforeCompletionStillCompletesSecondRequest) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(true);  // Always complete via PostTask
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  CoalescingCertVerifier verifier(std::move(mock_verifier_owner));

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1, result2;
  TestCompletionCallback callback1, callback2;
  std::unique_ptr<CertVerifier::Request> request1, request2;

  // Start an (asynchronous) initial request.
  int error = verifier.Verify(request_params, &result1, callback1.callback(),
                              &request1, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request1);

  // Start a second request; this should join the first request.
  error = verifier.Verify(request_params, &result2, callback2.callback(),
                          &request2, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  // Ensure only one underlying verification was started.
  ASSERT_EQ(2u, verifier.requests_for_testing());
  ASSERT_EQ(1u, verifier.inflight_joins_for_testing());

  // Abandon the first request before it's completed.
  request1.reset();

  // Make sure the first request never completes / the callback is never
  // invoked, while the second request completes normally.
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_FALSE(callback1.have_result());

  // Simulate the second request going away during processing.
  request2.reset();

  // Flush any events, although there should not be any.
  RunUntilIdle();
  EXPECT_FALSE(callback1.have_result());
}

TEST_F(CoalescingCertVerifierTest, DeleteRequestDuringCompletion) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(true);  // Always complete via PostTask
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  CoalescingCertVerifier verifier(std::move(mock_verifier_owner));

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1;
  TestCompletionCallback callback1;
  std::unique_ptr<CertVerifier::Request> request1;

  // Start an (asynchronous) initial request.
  int error = verifier.Verify(
      request_params, &result1,
      base::BindLambdaForTesting([&callback1, &request1](int result) {
        // Delete the Request during the completion callback. This should be
        // perfectly safe, and not cause any memory trouble, because the
        // Request was already detached from the Job prior to being invoked.
        request1.reset();
        callback1.callback().Run(result);
      }),
      &request1, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request1);

  // The result should be available, even though the request is deleted
  // during the result processing. This should not cause any memory errors.
  EXPECT_THAT(callback1.WaitForResult(), IsOk());
}

TEST_F(CoalescingCertVerifierTest, DeleteVerifierBeforeRequest) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  base::HistogramTester histograms;

  CertVerifyResult fake_result;
  fake_result.verified_cert = test_cert;

  std::unique_ptr<MockCertVerifier> mock_verifier_owner =
      std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_verifier = mock_verifier_owner.get();
  mock_verifier->set_async(true);  // Always complete via PostTask
  mock_verifier->AddResultForCert(test_cert, fake_result, OK);

  auto verifier =
      std::make_unique<CoalescingCertVerifier>(std::move(mock_verifier_owner));

  CertVerifier::RequestParams request_params(test_cert, "www.example.com", 0,
                                             /*ocsp_response=*/std::string(),
                                             /*sct_list=*/std::string());

  CertVerifyResult result1;
  TestCompletionCallback callback1;
  std::unique_ptr<CertVerifier::Request> request1;

  // Start an (asynchronous) initial request.
  int error = verifier->Verify(request_params, &result1, callback1.callback(),
                               &request1, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request1);

  // Delete the CoalescingCertVerifier first. This should orphan all
  // outstanding Requests and delete all associated Jobs.
  verifier.reset();

  // Flush any pending tasks; there should not be any, at this point, but use
  // it in case the implementation changes.
  RunUntilIdle();

  // Make sure the callback was never called.
  EXPECT_FALSE(callback1.have_result());

  // Delete the Request. This should be a no-op as the Request was orphaned
  // when the CoalescingCertVerifier was deleted.
  request1.reset();

  // There should not have been any histograms logged.
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 0);
  histograms.ExpectTotalCount("Net.CertVerifier_First_Job_Latency", 0);
}

}  // namespace net
