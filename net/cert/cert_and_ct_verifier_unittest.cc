// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_and_ct_verifier.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

// Callback that allows a test to check that the callback was canceled. The
// FailTest() callback can own the CallbackHelper while the test keeps a WeakPtr
// to check whether it has been deleted. The callback itself will fail the test
// if it is run.
struct CallbackHelper {
  base::WeakPtrFactory<CallbackHelper> factory{this};
};
void FailTest(std::unique_ptr<CallbackHelper> helper, int result) {
  FAIL();
}

class FakeCTVerifier : public CTVerifier {
 public:
  // CTVerifier implementation:
  void Verify(X509Certificate* cert,
              base::StringPiece stapled_ocsp_response,
              base::StringPiece sct_list_from_tls_extension,
              SignedCertificateTimestampAndStatusList* output_scts,
              const NetLogWithSource& net_log) override {
    *output_scts = scts_;
  }

  // Test setup interface:
  void set_scts(const SignedCertificateTimestampAndStatusList& scts) {
    scts_ = scts;
  }

 private:
  SignedCertificateTimestampAndStatusList scts_;
};

}  // namespace

class CertAndCTVerifierTest : public TestWithTaskEnvironment {
 public:
  CertAndCTVerifierTest() = default;
  ~CertAndCTVerifierTest() override = default;
};

// Tests that both certificate and certificate transparency details in the
// CertVerifyResult are filled by the CertAndCTVerifier, when completion occurs
// synchronously.
TEST_F(CertAndCTVerifierTest, CertAndCTDetailsFilled_Sync) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  // Mock the cert verification and CT verification results.
  CertVerifyResult mock_result;
  mock_result.cert_status = OK;
  mock_result.verified_cert = test_cert;
  auto cert_verifier = std::make_unique<MockCertVerifier>();
  cert_verifier->AddResultForCert(test_cert, mock_result, OK);
  cert_verifier->set_async(false);

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::GetX509CertSCT(&sct);
  SignedCertificateTimestampAndStatus sct_and_status(sct, ct::SCT_STATUS_OK);
  SignedCertificateTimestampAndStatusList sct_list{sct_and_status};
  auto ct_verifier = std::make_unique<FakeCTVerifier>();
  ct_verifier->set_scts(sct_list);

  CertAndCTVerifier cert_and_ct_verifier(std::move(cert_verifier),
                                         std::move(ct_verifier));

  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  int result = callback.GetResult(cert_and_ct_verifier.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource()));
  EXPECT_THAT(result, IsOk());
  ASSERT_EQ(1u, verify_result.scts.size());
  EXPECT_EQ(ct::SCT_STATUS_OK, verify_result.scts[0].status);
}

// Tests that both certificate and certificate transparency details in the
// CertVerifyResult are filled by the CertAndCTVerifier, when completion occurs
// asynchronously.
TEST_F(CertAndCTVerifierTest, CertAndCTDetailsFilled_Async) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  // Mock the cert verification and CT verification results.
  CertVerifyResult mock_result;
  mock_result.cert_status = OK;
  mock_result.verified_cert = test_cert;
  auto cert_verifier = std::make_unique<MockCertVerifier>();
  cert_verifier->AddResultForCert(test_cert, mock_result, OK);
  cert_verifier->set_async(true);

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::GetX509CertSCT(&sct);
  SignedCertificateTimestampAndStatus sct_and_status(sct, ct::SCT_STATUS_OK);
  SignedCertificateTimestampAndStatusList sct_list{sct_and_status};
  auto ct_verifier = std::make_unique<FakeCTVerifier>();
  ct_verifier->set_scts(sct_list);

  CertAndCTVerifier cert_and_ct_verifier(std::move(cert_verifier),
                                         std::move(ct_verifier));

  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  int result = callback.GetResult(cert_and_ct_verifier.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource()));
  EXPECT_THAT(result, IsOk());
  ASSERT_EQ(1u, verify_result.scts.size());
  EXPECT_EQ(ct::SCT_STATUS_OK, verify_result.scts[0].status);
}

// Tests that the callback of a canceled request is never run.
TEST_F(CertAndCTVerifierTest, CancelRequest) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  // Mock the cert verification and CT verification results.
  CertVerifyResult mock_result;
  mock_result.cert_status = OK;
  mock_result.verified_cert = test_cert;
  auto cert_verifier = std::make_unique<MockCertVerifier>();
  cert_verifier->AddResultForCert(test_cert, mock_result, OK);
  cert_verifier->set_async(true);

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::GetX509CertSCT(&sct);
  SignedCertificateTimestampAndStatus sct_and_status(sct, ct::SCT_STATUS_OK);
  SignedCertificateTimestampAndStatusList sct_list{sct_and_status};
  auto ct_verifier = std::make_unique<FakeCTVerifier>();
  ct_verifier->set_scts(sct_list);

  CertAndCTVerifier cert_and_ct_verifier(std::move(cert_verifier),
                                         std::move(ct_verifier));

  CertVerifyResult verify_result;
  std::unique_ptr<CertVerifier::Request> request;

  auto helper = std::make_unique<CallbackHelper>();
  base::WeakPtr<CallbackHelper> weak_helper = helper->factory.GetWeakPtr();

  int result = cert_and_ct_verifier.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, base::BindOnce(&FailTest, std::move(helper)), &request,
      NetLogWithSource());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_TRUE(request);
  request.reset();

  // Check that the callback was reset when the request was reset.
  ASSERT_TRUE(weak_helper.WasInvalidated());

  RunUntilIdle();
}

// Tests that the callback of a request is never run if the CertAndCTVerifier is
// deleted.
TEST_F(CertAndCTVerifierTest, DeleteVerifier) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  // Mock the cert verification and CT verification results.
  CertVerifyResult mock_result;
  mock_result.cert_status = OK;
  mock_result.verified_cert = test_cert;
  auto cert_verifier = std::make_unique<MockCertVerifier>();
  cert_verifier->AddResultForCert(test_cert, mock_result, OK);
  cert_verifier->set_async(true);

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::GetX509CertSCT(&sct);
  SignedCertificateTimestampAndStatus sct_and_status(sct, ct::SCT_STATUS_OK);
  SignedCertificateTimestampAndStatusList sct_list{sct_and_status};
  auto ct_verifier = std::make_unique<FakeCTVerifier>();
  ct_verifier->set_scts(sct_list);

  auto cert_and_ct_verifier = std::make_unique<CertAndCTVerifier>(
      std::move(cert_verifier), std::move(ct_verifier));

  CertVerifyResult verify_result;
  std::unique_ptr<CertVerifier::Request> request;

  auto helper = std::make_unique<CallbackHelper>();
  base::WeakPtr<CallbackHelper> weak_helper = helper->factory.GetWeakPtr();

  int result = cert_and_ct_verifier->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, base::BindOnce(&FailTest, std::move(helper)), &request,
      NetLogWithSource());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  ASSERT_TRUE(request);
  cert_and_ct_verifier.reset();

  // Check that the callback was reset when the verifier was deleted.
  ASSERT_TRUE(weak_helper.WasInvalidated());

  RunUntilIdle();
}

// Tests that cancelling the request and stopping without ever running anything
// works as expected.
TEST_F(CertAndCTVerifierTest, CancelRequestThenQuit) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  // Mock the cert verification and CT verification results.
  CertVerifyResult mock_result;
  mock_result.cert_status = OK;
  mock_result.verified_cert = test_cert;
  auto cert_verifier = std::make_unique<MockCertVerifier>();
  cert_verifier->AddResultForCert(test_cert, mock_result, OK);
  cert_verifier->set_async(true);

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::GetX509CertSCT(&sct);
  SignedCertificateTimestampAndStatus sct_and_status(sct, ct::SCT_STATUS_OK);
  SignedCertificateTimestampAndStatusList sct_list{sct_and_status};
  auto ct_verifier = std::make_unique<FakeCTVerifier>();
  ct_verifier->set_scts(sct_list);

  auto cert_and_ct_verifier = std::make_unique<CertAndCTVerifier>(
      std::move(cert_verifier), std::move(ct_verifier));

  CertVerifyResult verify_result;
  std::unique_ptr<CertVerifier::Request> request;

  auto helper = std::make_unique<CallbackHelper>();
  base::WeakPtr<CallbackHelper> weak_helper = helper->factory.GetWeakPtr();

  int result = cert_and_ct_verifier->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, base::BindOnce(&FailTest, std::move(helper)), &request,
      NetLogWithSource());
  ASSERT_THAT(result, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  request.reset();

  // Check that the callback was reset when the request was reset.
  ASSERT_TRUE(weak_helper.WasInvalidated());

  // Destroy |cert_and_ct_verifier| by going out of scope.
}

// Regression test for crbug.com/1153484: If the CertVerifier aborts, the
// CT verification step should be skipped.
TEST_F(CertAndCTVerifierTest, CertVerifierErrorShouldSkipCT) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  // Mock the cert verification result as aborted (like what happens if the
  // MojoCertVerifier gets disconnected).
  CertVerifyResult mock_result;
  mock_result.cert_status = CERT_STATUS_INVALID;
  mock_result.verified_cert = test_cert;
  auto cert_verifier = std::make_unique<MockCertVerifier>();
  cert_verifier->AddResultForCert(test_cert, mock_result, ERR_ABORTED);
  cert_verifier->set_async(true);

  // Mock valid SCTs.
  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::GetX509CertSCT(&sct);
  SignedCertificateTimestampAndStatus sct_and_status(sct, ct::SCT_STATUS_OK);
  SignedCertificateTimestampAndStatusList sct_list{sct_and_status};
  auto ct_verifier = std::make_unique<FakeCTVerifier>();
  ct_verifier->set_scts(sct_list);

  CertAndCTVerifier cert_and_ct_verifier(std::move(cert_verifier),
                                         std::move(ct_verifier));

  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  int result = callback.GetResult(cert_and_ct_verifier.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource()));
  EXPECT_THAT(result, IsError(ERR_ABORTED));
  // SCTs should not be filled in because CTVerifier::Verify() should not be
  // called.
  ASSERT_EQ(0u, verify_result.scts.size());
}

TEST_F(CertAndCTVerifierTest, ObserverIsForwarded) {
  auto mock_cert_verifier_owner = std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_cert_verifier = mock_cert_verifier_owner.get();

  CertAndCTVerifier cert_and_ct_verifier(std::move(mock_cert_verifier_owner),
                                         std::make_unique<FakeCTVerifier>());

  CertVerifierObserverCounter observer(&cert_and_ct_verifier);
  EXPECT_EQ(observer.change_count(), 0u);
  // A CertVerifierChanged event on the wrapped verifier should be forwarded to
  // observers registered on CertAndCTVerifier.
  mock_cert_verifier->SimulateOnCertVerifierChanged();
  EXPECT_EQ(observer.change_count(), 1u);
}

}  // namespace net
