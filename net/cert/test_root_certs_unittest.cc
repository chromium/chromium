// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

namespace {

// The local test root certificate.
const char kRootCertificateFile[] = "root_ca_cert.pem";
// A certificate issued by the local test root for 127.0.0.1.
const char kGoodCertificateFile[] = "ok_cert.pem";

}  // namespace

class TestRootCertsTest : public testing::TestWithParam<bool> {
 public:
  scoped_refptr<CertVerifyProc> CreateCertVerifyProc() {
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
    // If CCV/CRS is optional, test with and without CCV/CRS.
    if (use_chrome_cert_validator()) {
      return CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, CRLSet::BuiltinCRLSet().get(),
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr, /*instance_params=*/{}, std::nullopt);
    } else {
      return CertVerifyProc::CreateSystemVerifyProc(
          /*cert_net_fetcher=*/nullptr, CRLSet::BuiltinCRLSet().get());
    }
#elif BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    return CertVerifyProc::CreateBuiltinWithChromeRootStore(
        /*cert_net_fetcher=*/nullptr, CRLSet::BuiltinCRLSet().get(),
        std::make_unique<DoNothingCTVerifier>(),
        base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
        /*root_store_data=*/nullptr, /*instance_params=*/{}, std::nullopt);
#elif BUILDFLAG(IS_FUCHSIA)
    return CertVerifyProc::CreateBuiltinVerifyProc(
        /*cert_net_fetcher=*/nullptr, CRLSet::BuiltinCRLSet().get(),
        std::make_unique<DoNothingCTVerifier>(),
        base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
        /*instance_params=*/{}, std::nullopt);
#else
  return CertVerifyProc::CreateSystemVerifyProc(/*cert_net_fetcher=*/nullptr,
                                                CRLSet::BuiltinCRLSet().get());
#endif
  }

  // Whether we use Chrome Cert Validator or not. Only relevant for platforms
  // where CHROME_ROOT_STORE_OPTIONAL is set; on other platforms both test
  // params will run the same test.
  bool use_chrome_cert_validator() { return GetParam(); }
};

// Test basic functionality when adding from an existing X509Certificate.
TEST_P(TestRootCertsTest, AddFromPointer) {
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kRootCertificateFile);
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), root_cert.get());

  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(nullptr), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  {
    ScopedTestRoot scoped_root(root_cert);
    EXPECT_FALSE(test_roots->IsEmpty());
  }
  EXPECT_TRUE(test_roots->IsEmpty());
}

// Test that TestRootCerts actually adds the appropriate trust status flags
// when requested, and that the trusted status is cleared once the root is
// removed the TestRootCerts. This test acts as a canary/sanity check for
// the results of the rest of net_unittests, ensuring that the trust status
// is properly being set and cleared.
TEST_P(TestRootCertsTest, OverrideTrust) {
  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(nullptr), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  scoped_refptr<X509Certificate> test_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kGoodCertificateFile);
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());

  // Test that the good certificate fails verification, because the root
  // certificate should not yet be trusted.
  int flags = 0;
  CertVerifyResult bad_verify_result;
  scoped_refptr<CertVerifyProc> verify_proc(CreateCertVerifyProc());
  int bad_status = verify_proc->Verify(test_cert.get(), "127.0.0.1",
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string(), flags,
                                       &bad_verify_result, NetLogWithSource());
  EXPECT_NE(OK, bad_status);
  EXPECT_NE(0u, bad_verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_FALSE(bad_verify_result.is_issued_by_known_root);

  // Add the root certificate and mark it as trusted.
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kRootCertificateFile);
  ASSERT_TRUE(root_cert);
  ScopedTestRoot scoped_root(root_cert);
  EXPECT_FALSE(test_roots->IsEmpty());

  // Test that the certificate verification now succeeds, because the
  // TestRootCerts is successfully imbuing trust.
  CertVerifyResult good_verify_result;
  int good_status = verify_proc->Verify(
      test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &good_verify_result,
      NetLogWithSource());
  EXPECT_THAT(good_status, IsOk());
  EXPECT_EQ(0u, good_verify_result.cert_status);
  EXPECT_FALSE(good_verify_result.is_issued_by_known_root);

  test_roots->Clear();
  EXPECT_TRUE(test_roots->IsEmpty());

  // Ensure that when the TestRootCerts is cleared, the trust settings
  // revert to their original state, and don't linger. If trust status
  // lingers, it will likely break other tests in net_unittests.
  CertVerifyResult restored_verify_result;
  int restored_status = verify_proc->Verify(
      test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &restored_verify_result,
      NetLogWithSource());
  EXPECT_NE(OK, restored_status);
  EXPECT_NE(0u,
            restored_verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(bad_status, restored_status);
  EXPECT_EQ(bad_verify_result.cert_status, restored_verify_result.cert_status);
  EXPECT_FALSE(restored_verify_result.is_issued_by_known_root);
}

TEST_P(TestRootCertsTest, OverrideKnownRoot) {
  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(nullptr), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  // Use a runtime generated certificate chain so that the cert lifetime is not
  // too long, and so that it will have an allowable hostname for a publicly
  // trusted cert.
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();

  // Add the root certificate and mark it as trusted and as a known root.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  ScopedTestKnownRoot scoped_known_root(root->GetX509Certificate().get());
  EXPECT_FALSE(test_roots->IsEmpty());

  // Test that the certificate verification sets the `is_issued_by_known_root`
  // flag.
  CertVerifyResult good_verify_result;
  scoped_refptr<CertVerifyProc> verify_proc(CreateCertVerifyProc());
  int flags = 0;
  int good_status =
      verify_proc->Verify(leaf->GetX509Certificate().get(), "www.example.com",
                          /*ocsp_response=*/std::string(),
                          /*sct_list=*/std::string(), flags,
                          &good_verify_result, NetLogWithSource());
  EXPECT_THAT(good_status, IsOk());
  EXPECT_EQ(0u, good_verify_result.cert_status);
  EXPECT_TRUE(good_verify_result.is_issued_by_known_root);

  test_roots->Clear();
  EXPECT_TRUE(test_roots->IsEmpty());

  // Ensure that when the TestRootCerts is cleared, the test known root status
  // revert to their original state, and don't linger. If known root status
  // lingers, it will likely break other tests in net_unittests.
  // Trust the root again so that the `is_issued_by_known_root` value will be
  // calculated, and ensure that it is false now.
  ScopedTestRoot scoped_root2(root->GetX509Certificate());
  CertVerifyResult restored_verify_result;
  int restored_status =
      verify_proc->Verify(leaf->GetX509Certificate().get(), "www.example.com",
                          /*ocsp_response=*/std::string(),
                          /*sct_list=*/std::string(), flags,
                          &restored_verify_result, NetLogWithSource());
  EXPECT_THAT(restored_status, IsOk());
  EXPECT_EQ(0u, restored_verify_result.cert_status);
  EXPECT_FALSE(restored_verify_result.is_issued_by_known_root);
}

TEST_P(TestRootCertsTest, Moveable) {
  TestRootCerts* test_roots = TestRootCerts::GetInstance();
  ASSERT_NE(static_cast<TestRootCerts*>(nullptr), test_roots);
  EXPECT_TRUE(test_roots->IsEmpty());

  scoped_refptr<X509Certificate> test_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kGoodCertificateFile);
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());

  int flags = 0;
  CertVerifyResult bad_verify_result;
  int bad_status;
  scoped_refptr<CertVerifyProc> verify_proc(CreateCertVerifyProc());
  {
    // Empty ScopedTestRoot at outer scope has no effect.
    ScopedTestRoot scoped_root_outer;
    EXPECT_TRUE(test_roots->IsEmpty());

    // Test that the good certificate fails verification, because the root
    // certificate should not yet be trusted.
    bad_status = verify_proc->Verify(test_cert.get(), "127.0.0.1",
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string(), flags,
                                     &bad_verify_result, NetLogWithSource());
    EXPECT_NE(OK, bad_status);
    EXPECT_NE(0u,
              bad_verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);

    {
      // Add the root certificate and mark it as trusted.
      scoped_refptr<X509Certificate> root_cert =
          ImportCertFromFile(GetTestCertsDirectory(), kRootCertificateFile);
      ASSERT_TRUE(root_cert);
      ScopedTestRoot scoped_root_inner(root_cert);
      EXPECT_FALSE(test_roots->IsEmpty());

      // Test that the certificate verification now succeeds, because the
      // TestRootCerts is successfully imbuing trust.
      CertVerifyResult good_verify_result;
      int good_status = verify_proc->Verify(
          test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
          /*sct_list=*/std::string(), flags, &good_verify_result,
          NetLogWithSource());
      EXPECT_THAT(good_status, IsOk());
      EXPECT_EQ(0u, good_verify_result.cert_status);

      EXPECT_FALSE(scoped_root_inner.IsEmpty());
      EXPECT_TRUE(scoped_root_outer.IsEmpty());
      // Move from inner scoped root to outer
      scoped_root_outer = std::move(scoped_root_inner);
      EXPECT_FALSE(test_roots->IsEmpty());
      EXPECT_FALSE(scoped_root_outer.IsEmpty());
    }
    // After inner scoper was freed, test root is still trusted since ownership
    // was moved to the outer scoper.
    EXPECT_FALSE(test_roots->IsEmpty());
    EXPECT_FALSE(scoped_root_outer.IsEmpty());

    // Test that the certificate verification still succeeds, because the
    // TestRootCerts is successfully imbuing trust.
    CertVerifyResult good_verify_result;
    int good_status = verify_proc->Verify(
        test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), flags, &good_verify_result,
        NetLogWithSource());
    EXPECT_THAT(good_status, IsOk());
    EXPECT_EQ(0u, good_verify_result.cert_status);
  }
  EXPECT_TRUE(test_roots->IsEmpty());

  // Ensure that when the TestRootCerts is cleared, the trust settings
  // revert to their original state, and don't linger. If trust status
  // lingers, it will likely break other tests in net_unittests.
  CertVerifyResult restored_verify_result;
  int restored_status = verify_proc->Verify(
      test_cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &restored_verify_result,
      NetLogWithSource());
  EXPECT_NE(OK, restored_status);
  EXPECT_NE(0u,
            restored_verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(bad_status, restored_status);
  EXPECT_EQ(bad_verify_result.cert_status, restored_verify_result.cert_status);
}

INSTANTIATE_TEST_SUITE_P(All, TestRootCertsTest, ::testing::Bool());

// TODO(rsleevi): Add tests for revocation checking via CRLs, ensuring that
// TestRootCerts properly injects itself into the validation process. See
// http://crbug.com/63958

}  // namespace net
