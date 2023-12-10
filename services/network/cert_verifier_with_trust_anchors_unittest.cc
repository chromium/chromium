// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cert_verifier_with_trust_anchors.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "crypto/nss_util_internal.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class CertVerifierWithTrustAnchorsTest : public testing::Test {
 public:
  CertVerifierWithTrustAnchorsTest()
      : trust_anchor_used_(false),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  ~CertVerifierWithTrustAnchorsTest() override {}

  void SetUp() override {
    test_ca_cert_ = LoadCertificate("root_ca_cert.pem");
    ASSERT_TRUE(test_ca_cert_);
    test_ca_cert_list_.push_back(test_ca_cert_);

    test_server_cert_ = LoadCertificate("ok_cert.pem");
    ASSERT_TRUE(test_server_cert_);

    cert_verifier_ = std::make_unique<network::CertVerifierWithTrustAnchors>(
        base::BindRepeating(
            &CertVerifierWithTrustAnchorsTest::OnTrustAnchorUsed,
            base::Unretained(this)));

    auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
    mock_cert_verifier_ = mock_cert_verifier.get();
    mock_cert_verifier_->set_async(true);
    mock_cert_verifier_->set_default_result(net::ERR_CERT_AUTHORITY_INVALID);
    cert_verifier_->InitializeOnIOThread(std::move(mock_cert_verifier));
  }

  void TearDown() override {
    mock_cert_verifier_ = nullptr;
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
  scoped_refptr<net::X509Certificate> test_ca_cert_;
  scoped_refptr<net::X509Certificate> test_server_cert_;
  net::CertificateList test_ca_cert_list_;
  std::unique_ptr<network::CertVerifierWithTrustAnchors> cert_verifier_;
  raw_ptr<net::MockCertVerifier> mock_cert_verifier_;

 private:
  void OnTrustAnchorUsed() { trust_anchor_used_ = true; }

  scoped_refptr<net::X509Certificate> LoadCertificate(const std::string& name) {
    return net::ImportCertFromFile(net::GetTestCertsDirectory(), name);
  }

  bool trust_anchor_used_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CertVerifierWithTrustAnchorsTest, VerifyUsingAdditionalTrustAnchor) {
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

  // Verify() again with the cert configured as non-policy provided trust
  // anchor.
  {
    net::CertVerifyResult mock_verify_result;
    mock_verify_result.is_issued_by_additional_trust_anchor = false;
    mock_verify_result.verified_cert = test_server_cert_;
    mock_cert_verifier_->AddResultForCert(test_server_cert_, mock_verify_result,
                                          net::OK);
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
  EXPECT_FALSE(WasTrustAnchorUsedAndReset());

  // Verify() again with the cert configured as an additional trust anchor.
  {
    net::CertVerifyResult mock_verify_result;
    mock_verify_result.is_issued_by_additional_trust_anchor = true;
    mock_verify_result.verified_cert = test_server_cert_;
    mock_cert_verifier_->ClearRules();
    mock_cert_verifier_->AddResultForCert(test_server_cert_, mock_verify_result,
                                          net::OK);
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
