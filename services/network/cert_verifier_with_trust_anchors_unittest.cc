// Copyright 2014 The Chromium Authors
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
#include "base/threading/thread_task_runner_handle.h"
#include "crypto/nss_util_internal.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/coalescing_cert_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

// Wraps a net::MockCertVerifier. When SetConfig() is called with trust anchors,
// this sets |server_cert_| to pass cert verification using an additional trust
// anchor. Otherwise |server_cert_| wil fail cert verification with
// net::ERR_CERT_AUTHORITY_INVALID.
class WrappedMockCertVerifier : public net::CertVerifier {
 public:
  explicit WrappedMockCertVerifier(
      scoped_refptr<net::X509Certificate> server_cert)
      : server_cert_(std::move(server_cert)) {
    mock_cert_verifier_.set_async(true);
  }

  // net::CertVerifier implementation:
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    return mock_cert_verifier_.Verify(params, verify_result,
                                      std::move(callback), out_req, net_log);
  }
  void SetConfig(const Config& config) override {
    mock_cert_verifier_.ClearRules();

    int net_err;
    net::CertVerifyResult verify_result;
    if (config.additional_trust_anchors.empty()) {
      net_err = net::ERR_CERT_AUTHORITY_INVALID;
    } else {
      net_err = net::OK;
      verify_result.is_issued_by_additional_trust_anchor = true;
    }

    verify_result.verified_cert = server_cert_;
    verify_result.cert_status = net::MapNetErrorToCertStatus(net_err);

    mock_cert_verifier_.AddResultForCert(server_cert_, verify_result, net_err);

    mock_cert_verifier_.SetConfig(config);
  }

 private:
  scoped_refptr<net::X509Certificate> server_cert_;
  net::MockCertVerifier mock_cert_verifier_;
};

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

    cert_verifier_->InitializeOnIOThread(
        std::make_unique<net::CachingCertVerifier>(
            std::make_unique<net::CoalescingCertVerifier>(
                std::make_unique<WrappedMockCertVerifier>(test_server_cert_))));
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
  scoped_refptr<net::CertVerifyProc> cert_verify_proc_;

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

  // Verify() again with the additional trust anchors.
  cert_verifier_->SetAdditionalCerts(test_ca_cert_list_,
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
  cert_verifier_->SetAdditionalCerts(test_ca_cert_list_,
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

  // Verify() again with the additional trust anchors.
  cert_verifier_->SetAdditionalCerts(test_ca_cert_list_,
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
