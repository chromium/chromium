// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/caching_cert_verifier.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_database.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

using testing::_;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;

namespace net {

class CachingCertVerifierTest : public TestWithTaskEnvironment {
 public:
  CachingCertVerifierTest() : verifier_(std::make_unique<MockCertVerifier>()) {}
  ~CachingCertVerifierTest() override = default;

 protected:
  CachingCertVerifier verifier_;
};

TEST_F(CachingCertVerifierTest, CacheHit) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource()));
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(1u, verifier_.GetCacheSize());

  error = verifier_.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  // Synchronous completion.
  ASSERT_NE(ERR_IO_PENDING, error);
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_FALSE(request);
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(1u, verifier_.cache_hits());
  ASSERT_EQ(1u, verifier_.GetCacheSize());
}

TEST_F(CachingCertVerifierTest, CacheHitCTResultsCached) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  auto cert_verifier = std::make_unique<MockCertVerifier>();
  // Mock the cert verification and CT verification results.
  CertVerifyResult mock_result;
  mock_result.cert_status = OK;
  mock_result.verified_cert = test_cert;

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::GetX509CertSCT(&sct);
  SignedCertificateTimestampAndStatus sct_and_status(sct, ct::SCT_STATUS_OK);
  SignedCertificateTimestampAndStatusList sct_list{sct_and_status};
  mock_result.scts = sct_list;
  cert_verifier->AddResultForCert(test_cert, mock_result, OK);

  // We don't use verifier_ here because we needed to call AddResultForCert from
  // the mock verifier.
  CachingCertVerifier cache_verifier(std::move(cert_verifier));

  int result;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  result = callback.GetResult(cache_verifier.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource()));
  ASSERT_EQ(OK, result);
  ASSERT_EQ(1u, verify_result.scts.size());
  ASSERT_EQ(ct::SCT_STATUS_OK, verify_result.scts[0].status);
  ASSERT_EQ(1u, cache_verifier.requests());
  ASSERT_EQ(0u, cache_verifier.cache_hits());
  ASSERT_EQ(1u, cache_verifier.GetCacheSize());

  result = cache_verifier.Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  // Synchronous completion.
  ASSERT_EQ(OK, result);
  ASSERT_FALSE(request);
  ASSERT_EQ(1u, verify_result.scts.size());
  ASSERT_EQ(ct::SCT_STATUS_OK, verify_result.scts[0].status);
  ASSERT_EQ(2u, cache_verifier.requests());
  ASSERT_EQ(1u, cache_verifier.cache_hits());
  ASSERT_EQ(1u, cache_verifier.GetCacheSize());
}

// Tests the same server certificate with different intermediate CA
// certificates.  These should be treated as different certificate chains even
// though the two X509Certificate objects contain the same server certificate.
TEST_F(CachingCertVerifierTest, DifferentCACerts) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "salesforce_com_test.pem");
  ASSERT_TRUE(server_cert);

  scoped_refptr<X509Certificate> intermediate_cert1 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2011.pem");
  ASSERT_TRUE(intermediate_cert1);

  scoped_refptr<X509Certificate> intermediate_cert2 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2016.pem");
  ASSERT_TRUE(intermediate_cert2);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(intermediate_cert1->cert_buffer()));
  scoped_refptr<X509Certificate> cert_chain1 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(server_cert->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(cert_chain1);

  intermediates.clear();
  intermediates.push_back(bssl::UpRef(intermediate_cert2->cert_buffer()));
  scoped_refptr<X509Certificate> cert_chain2 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(server_cert->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(cert_chain2);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(cert_chain1, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource()));
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(1u, verifier_.GetCacheSize());

  error = callback.GetResult(verifier_.Verify(
      CertVerifier::RequestParams(cert_chain2, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource()));
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(2u, verifier_.requests());
  ASSERT_EQ(0u, verifier_.cache_hits());
  ASSERT_EQ(2u, verifier_.GetCacheSize());
}

TEST_F(CachingCertVerifierTest, ObserverIsForwarded) {
  auto mock_cert_verifier = std::make_unique<MockCertVerifier>();
  MockCertVerifier* mock_cert_verifier_ptr = mock_cert_verifier.get();
  CachingCertVerifier cache_verifier(std::move(mock_cert_verifier));

  CertVerifierObserverCounter observer_(&cache_verifier);
  EXPECT_EQ(observer_.change_count(), 0u);
  // A CertVerifierChanged event on the wrapped verifier should be forwarded to
  // observers registered on CachingCertVerifier.
  mock_cert_verifier_ptr->SimulateOnCertVerifierChanged();
  EXPECT_EQ(observer_.change_count(), 1u);
}

namespace {
enum class ChangeType {
  kSetConfig,
  kCertVerifierChanged,
  kCertDBChanged,
};
}  // namespace

class CachingCertVerifierCacheClearingTest
    : public testing::TestWithParam<ChangeType> {
 public:
  CachingCertVerifierCacheClearingTest() {
    auto mock_cert_verifier = std::make_unique<MockCertVerifier>();
    mock_verifier_ = mock_cert_verifier.get();
    verifier_ =
        std::make_unique<CachingCertVerifier>(std::move(mock_cert_verifier));
  }

  ChangeType change_type() const { return GetParam(); }

  void DoCacheClearingAction() {
    switch (change_type()) {
      case ChangeType::kSetConfig:
        verifier_->SetConfig({});
        break;
      case ChangeType::kCertVerifierChanged:
        mock_verifier_->SimulateOnCertVerifierChanged();
        break;
      case ChangeType::kCertDBChanged:
        CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
        base::RunLoop().RunUntilIdle();
        break;
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<CachingCertVerifier> verifier_;
  raw_ptr<MockCertVerifier> mock_verifier_;
};

TEST_P(CachingCertVerifierCacheClearingTest, CacheClearedSyncVerification) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  mock_verifier_->set_async(false);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_EQ(1u, verifier_->requests());
  ASSERT_EQ(0u, verifier_->cache_hits());
  ASSERT_EQ(1u, verifier_->GetCacheSize());

  DoCacheClearingAction();
  ASSERT_EQ(0u, verifier_->GetCacheSize());

  error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_FALSE(request);
  ASSERT_EQ(2u, verifier_->requests());
  ASSERT_EQ(0u, verifier_->cache_hits());
  ASSERT_EQ(1u, verifier_->GetCacheSize());
}

TEST_P(CachingCertVerifierCacheClearingTest, CacheClearedAsyncVerification) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert.get());

  mock_verifier_->set_async(true);

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request);
  ASSERT_EQ(1u, verifier_->requests());
  ASSERT_EQ(0u, verifier_->cache_hits());
  ASSERT_EQ(0u, verifier_->GetCacheSize());

  DoCacheClearingAction();
  ASSERT_EQ(0u, verifier_->GetCacheSize());

  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  // Async result should not have been cached since it was from a verification
  // started before the config changed.
  ASSERT_EQ(0u, verifier_->GetCacheSize());

  error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  ASSERT_EQ(ERR_IO_PENDING, error);
  ASSERT_TRUE(request);
  ASSERT_EQ(2u, verifier_->requests());
  ASSERT_EQ(0u, verifier_->cache_hits());
  ASSERT_EQ(0u, verifier_->GetCacheSize());

  error = callback.WaitForResult();
  ASSERT_TRUE(IsCertificateError(error));
  // New async result should be cached since it was from a verification started
  // after the config changed.
  ASSERT_EQ(1u, verifier_->GetCacheSize());

  // Verify again. Result should be synchronous this time since it will get the
  // cached result.
  error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  ASSERT_TRUE(IsCertificateError(error));
  ASSERT_FALSE(request);
  ASSERT_EQ(3u, verifier_->requests());
  ASSERT_EQ(1u, verifier_->cache_hits());
  ASSERT_EQ(1u, verifier_->GetCacheSize());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CachingCertVerifierCacheClearingTest,
                         testing::Values(ChangeType::kSetConfig,
                                         ChangeType::kCertVerifierChanged,
                                         ChangeType::kCertDBChanged));

}  // namespace net
