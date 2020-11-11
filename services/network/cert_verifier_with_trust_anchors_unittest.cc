// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cert_verifier_with_trust_anchors.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/coalescing_cert_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/cert_verifier/system_trust_store_provider_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class CertVerifierWithTrustAnchorsTest : public testing::Test {
 public:
  CertVerifierWithTrustAnchorsTest()
      : trust_anchor_used_(false),
        test_nss_user_("user1"),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  ~CertVerifierWithTrustAnchorsTest() override {}

  void SetUp() override {
    ASSERT_TRUE(test_nss_user_.constructed_successfully());
    test_nss_user_.FinishInit();

    test_cert_db_ = std::make_unique<net::NSSCertDatabaseChromeOS>(
        crypto::GetPublicSlotForChromeOSUser(test_nss_user_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            test_nss_user_.username_hash(),
            base::RepeatingCallback<void(crypto::ScopedPK11Slot)>()));

    cert_verifier_ =
        std::make_unique<CertVerifierWithTrustAnchors>(base::BindRepeating(
            &CertVerifierWithTrustAnchorsTest::OnTrustAnchorUsed,
            base::Unretained(this)));
    cert_verify_proc_ = net::CreateCertVerifyProcBuiltin(
        /*net_fetcher=*/nullptr,
        std::make_unique<SystemTrustStoreProviderChromeOS>(
            crypto::GetPublicSlotForChromeOSUser(
                test_nss_user_.username_hash())));

    cert_verifier_->InitializeOnIOThread(
        std::make_unique<net::CachingCertVerifier>(
            std::make_unique<net::CoalescingCertVerifier>(
                std::make_unique<net::MultiThreadedCertVerifier>(
                    cert_verify_proc_))));

    test_ca_cert_ = LoadCertificate("root_ca_cert.pem", net::CA_CERT);
    ASSERT_TRUE(test_ca_cert_);
    test_ca_cert_list_.push_back(
        net::x509_util::DupCERTCertificate(test_ca_cert_.get()));

    net::ScopedCERTCertificate test_server_cert =
        LoadCertificate("ok_cert.pem", net::SERVER_CERT);
    ASSERT_TRUE(test_server_cert);
    test_server_cert_ =
        net::x509_util::CreateX509CertificateFromCERTCertificate(
            test_server_cert.get());
    ASSERT_TRUE(test_server_cert_);
  }

  void TearDown() override {
    // Destroy |cert_verifier_| before destroying the TaskEnvironment, otherwise
    // BrowserThread::CurrentlyOn checks fail.
    cert_verifier_.reset();
  }

 protected:
  int VerifyTestServerCert(
      net::CompletionOnceCallback test_callback,
      net::CertVerifyResult* verify_result,
      std::unique_ptr<net::CertVerifier::Request>* request) {
    return cert_verifier_->Verify(net::CertVerifier::RequestParams(
                                      test_server_cert_.get(), "127.0.0.1", 0,
                                      /*ocsp_response=*/std::string(),
                                      /*sct_list=*/std::string()),
                                  verify_result, std::move(test_callback),
                                  request, net::NetLogWithSource());
  }

  bool SupportsAdditionalTrustAnchors() {
    return cert_verify_proc_->SupportsAdditionalTrustAnchors();
  }

  // Returns whether |cert_verifier| signalled usage of one of the additional
  // trust anchors (i.e. of |test_ca_cert_|) for the first time or since the
  // last call of this function.
  bool WasTrustAnchorUsedAndReset() {
    base::RunLoop().RunUntilIdle();
    bool result = trust_anchor_used_;
    trust_anchor_used_ = false;
    return result;
  }

  // |test_ca_cert_| is the issuer of |test_server_cert_|.
  net::ScopedCERTCertificate test_ca_cert_;
  scoped_refptr<net::X509Certificate> test_server_cert_;
  net::ScopedCERTCertificateList test_ca_cert_list_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_cert_db_;
  std::unique_ptr<CertVerifierWithTrustAnchors> cert_verifier_;
  scoped_refptr<net::CertVerifyProc> cert_verify_proc_;

 private:
  void OnTrustAnchorUsed() { trust_anchor_used_ = true; }

  net::ScopedCERTCertificate LoadCertificate(const std::string& name,
                                             net::CertType type) {
    net::ScopedCERTCertificate cert =
        net::ImportCERTCertificateFromFile(net::GetTestCertsDirectory(), name);
    if (!cert)
      return cert;

    // No certificate is trusted right after it's loaded.
    net::NSSCertDatabase::TrustBits trust =
        test_cert_db_->GetCertTrust(cert.get(), type);
    EXPECT_EQ(net::NSSCertDatabase::TRUST_DEFAULT, trust);

    return cert;
  }

  bool trust_anchor_used_;
  crypto::ScopedTestNSSChromeOSUser test_nss_user_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CertVerifierWithTrustAnchorsTest, VerifyUntrustedCert) {
  // |test_server_cert_| is untrusted, so Verify() fails.
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }

  // Issuing the same request again hits the cache. This tests the synchronous
  // path.
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }

  EXPECT_FALSE(WasTrustAnchorUsedAndReset());
}

TEST_F(CertVerifierWithTrustAnchorsTest, VerifyTrustedCert) {
  // Make the database trust |test_ca_cert_|.
  net::NSSCertDatabase::ImportCertFailureList failure_list;
  ASSERT_TRUE(test_cert_db_->ImportCACerts(
      test_ca_cert_list_, net::NSSCertDatabase::TRUSTED_SSL, &failure_list));
  ASSERT_TRUE(failure_list.empty());

  // Verify that it is now trusted.
  net::NSSCertDatabase::TrustBits trust =
      test_cert_db_->GetCertTrust(test_ca_cert_.get(), net::CA_CERT);
  EXPECT_EQ(net::NSSCertDatabase::TRUSTED_SSL, trust);

  // Verify() successfully verifies |test_server_cert_| after it was imported.
  net::CertVerifyResult verify_result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error =
      VerifyTestServerCert(callback.callback(), &verify_result, &request);
  ASSERT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_TRUE(request);
  error = callback.WaitForResult();
  EXPECT_EQ(net::OK, error);

  // The additional trust anchors were not used, since the certificate is
  // trusted from the database.
  EXPECT_FALSE(WasTrustAnchorUsedAndReset());
}

TEST_F(CertVerifierWithTrustAnchorsTest, VerifyUsingAdditionalTrustAnchor) {
  ASSERT_TRUE(SupportsAdditionalTrustAnchors());

  // |test_server_cert_| is untrusted, so Verify() fails.
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }
  EXPECT_FALSE(WasTrustAnchorUsedAndReset());

  net::CertificateList test_ca_x509cert_list =
      net::x509_util::CreateX509CertificateListFromCERTCertificates(
          test_ca_cert_list_);
  ASSERT_FALSE(test_ca_x509cert_list.empty());

  // Verify() again with the additional trust anchors.
  cert_verifier_->SetAdditionalCerts(test_ca_x509cert_list,
                                     net::CertificateList());
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::OK, error);
  }
  EXPECT_TRUE(WasTrustAnchorUsedAndReset());

  // Verify() again with the additional trust anchors will hit the cache.
  cert_verifier_->SetAdditionalCerts(test_ca_x509cert_list,
                                     net::CertificateList());
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    EXPECT_EQ(net::OK, error);
  }
  EXPECT_TRUE(WasTrustAnchorUsedAndReset());

  // Verifying after removing the trust anchors should now fail.
  cert_verifier_->SetAdditionalCerts(net::CertificateList(),
                                     net::CertificateList());
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    // Note: Changing the trust anchors should flush the cache.
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }
  // The additional trust anchors were reset, thus |cert_verifier_| should not
  // signal it's usage anymore.
  EXPECT_FALSE(WasTrustAnchorUsedAndReset());
}

TEST_F(CertVerifierWithTrustAnchorsTest,
       VerifyUsesAdditionalTrustAnchorsAfterConfigChange) {
  ASSERT_TRUE(SupportsAdditionalTrustAnchors());

  // |test_server_cert_| is untrusted, so Verify() fails.
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }
  EXPECT_FALSE(WasTrustAnchorUsedAndReset());

  net::CertificateList test_ca_x509cert_list =
      net::x509_util::CreateX509CertificateListFromCERTCertificates(
          test_ca_cert_list_);
  ASSERT_FALSE(test_ca_x509cert_list.empty());

  // Verify() again with the additional trust anchors.
  cert_verifier_->SetAdditionalCerts(test_ca_x509cert_list,
                                     net::CertificateList());
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::OK, error);
  }
  EXPECT_TRUE(WasTrustAnchorUsedAndReset());

  // Change the configuration to enable SHA-1, which should still use the
  // additional trust anchors.
  net::CertVerifier::Config config;
  config.enable_sha1_local_anchors = true;
  cert_verifier_->SetConfig(config);
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    std::unique_ptr<net::CertVerifier::Request> request;
    int error =
        VerifyTestServerCert(callback.callback(), &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::OK, error);
  }
  EXPECT_TRUE(WasTrustAnchorUsedAndReset());
}

}  // namespace network
