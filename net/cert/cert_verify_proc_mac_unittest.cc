// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_mac.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/macros.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/test_keychain_search_list_mac.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

// Test that a CRLSet blocking one of the intermediates supplied by the server
// can be worked around by the chopping workaround for path building. (Once the
// supplied chain is chopped back to just the target, a better path can be
// found out-of-band. Normally that would be by AIA fetching, for the purposes
// of this test the better path is supplied by a test keychain.)
//
// In this test, there are two possible paths to validate a leaf (A):
// 1. A(B) -> B(C) -> C(E) -> E(E)
// 2. A(B) -> B(F) -> F(E) -> E(E)
//
// A(B) -> B(C) -> C(E) is supplied to the verifier.
// B(F) and F(E) are supplied in a test keychain.
// C is blocked by a CRLset.
//
// The verifier should rollback until it just tries A(B) alone, at which point
// it will pull B(F) & F(E) from the keychain and succeed.
TEST(CertVerifyProcMacTest, MacCRLIntermediate) {
  if (base::mac::IsAtLeastOS10_12()) {
    // TODO(crbug.com/671889): Investigate SecTrustSetKeychains issue on Sierra.
    LOG(INFO) << "Skipping test, SecTrustSetKeychains does not work on 10.12";
    return;
  }
  CertificateList path_2_certs;
  ASSERT_TRUE(
      LoadCertificateFiles({"multi-root-A-by-B.pem", "multi-root-B-by-C.pem",
                            "multi-root-C-by-E.pem", "multi-root-E-by-E.pem"},
                           &path_2_certs));

  CertificateList path_3_certs;
  ASSERT_TRUE(
      LoadCertificateFiles({"multi-root-A-by-B.pem", "multi-root-B-by-F.pem",
                            "multi-root-F-by-E.pem", "multi-root-E-by-E.pem"},
                           &path_3_certs));

  // Add E as trust anchor.
  ScopedTestRoot test_root_E(path_3_certs[3].get());  // E-by-E

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(
      bssl::UpRef(path_2_certs[1]->cert_buffer()));  // B-by-C
  intermediates.push_back(
      bssl::UpRef(path_2_certs[2]->cert_buffer()));  // C-by-E
  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
      bssl::UpRef(path_3_certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert);

  std::unique_ptr<TestKeychainSearchList> test_keychain_search_list(
      TestKeychainSearchList::Create());
  ASSERT_TRUE(test_keychain_search_list);

  base::FilePath keychain_path(
      GetTestCertsDirectory().AppendASCII("multi-root-BFE.keychain"));
  // SecKeychainOpen does not fail if the file doesn't exist, so assert it here
  // for easier debugging.
  ASSERT_TRUE(base::PathExists(keychain_path));
  SecKeychainRef keychain;
  OSStatus status =
      SecKeychainOpen(keychain_path.MaybeAsASCII().c_str(), &keychain);
  ASSERT_EQ(errSecSuccess, status);
  ASSERT_TRUE(keychain);
  base::ScopedCFTypeRef<SecKeychainRef> scoped_keychain(keychain);
  test_keychain_search_list->AddKeychain(keychain);

  scoped_refptr<CRLSet> crl_set;
  std::string crl_set_bytes;
  // CRL which blocks C by SPKI.
  EXPECT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("multi-root-crlset-C.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  int flags = 0;
  CertVerifyResult verify_result;

  scoped_refptr<CertVerifyProc> verify_proc = new CertVerifyProcMac;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, crl_set.get(), CertificateList(),
      &verify_result);

  ASSERT_EQ(OK, error);
  ASSERT_EQ(0U, verify_result.cert_status);
  ASSERT_TRUE(verify_result.verified_cert.get());

  const auto& verified_intermediates =
      verify_result.verified_cert->intermediate_buffers();
  ASSERT_EQ(3U, verified_intermediates.size());

  scoped_refptr<X509Certificate> intermediate =
      X509Certificate::CreateFromBuffer(
          bssl::UpRef(verified_intermediates[1].get()), {});
  ASSERT_TRUE(intermediate);

  scoped_refptr<X509Certificate> expected_intermediate = path_3_certs[2];
  EXPECT_TRUE(expected_intermediate->EqualsExcludingChain(intermediate.get()))
      << "Expected: " << expected_intermediate->subject().common_name
      << " issued by " << expected_intermediate->issuer().common_name
      << "; Got: " << intermediate->subject().common_name << " issued by "
      << intermediate->issuer().common_name;
}

// Test that if a keychain is present which trusts a less-desirable root (ex,
// one using SHA1), that the keychain reordering hack will cause the better
// root in the System Roots to be used instead.
// TODO(crbug.com/867174): Re-enable this test.
TEST(CertVerifyProcMacTest, DISABLED_MacKeychainReordering) {
  // Note: target cert expires Dec 30 23:59:59 2019 GMT
  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "gms.hongleong.com.my-verisign-chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);

  // Create a test keychain search list that will Always Trust the SHA1
  // cross-signed VeriSign Class 3 Public Primary Certification Authority - G5
  std::unique_ptr<TestKeychainSearchList> test_keychain_search_list(
      TestKeychainSearchList::Create());
  ASSERT_TRUE(test_keychain_search_list);

  base::FilePath keychain_path(GetTestCertsDirectory().AppendASCII(
      "verisign_class3_g5_crosssigned-trusted.keychain"));
  // SecKeychainOpen does not fail if the file doesn't exist, so assert it here
  // for easier debugging.
  ASSERT_TRUE(base::PathExists(keychain_path));
  SecKeychainRef keychain;
  OSStatus status =
      SecKeychainOpen(keychain_path.MaybeAsASCII().c_str(), &keychain);
  ASSERT_EQ(errSecSuccess, status);
  ASSERT_TRUE(keychain);
  base::ScopedCFTypeRef<SecKeychainRef> scoped_keychain(keychain);
  test_keychain_search_list->AddKeychain(keychain);

  int flags = 0;
  CertVerifyResult verify_result;
  scoped_refptr<CertVerifyProc> verify_proc = new CertVerifyProcMac;
  int error = verify_proc->Verify(
      cert.get(), "gms.hongleong.com.my", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result);

  ASSERT_EQ(OK, error);
  EXPECT_FALSE(verify_result.has_sha1);
  ASSERT_TRUE(verify_result.verified_cert.get());

  const auto& verified_intermediates =
      verify_result.verified_cert->intermediate_buffers();
  ASSERT_EQ(2U, verified_intermediates.size());
}

// Test that the system root certificate keychain is in the expected location
// and can be opened. Other tests would fail if this was not true, but this
// test makes the reason for the failure obvious.
TEST(CertVerifyProcMacTest, MacSystemRootCertificateKeychainLocation) {
  const char* root_keychain_path =
      "/System/Library/Keychains/SystemRootCertificates.keychain";
  ASSERT_TRUE(base::PathExists(base::FilePath(root_keychain_path)));

  SecKeychainRef keychain;
  OSStatus status = SecKeychainOpen(root_keychain_path, &keychain);
  ASSERT_EQ(errSecSuccess, status);
  CFRelease(keychain);
}

// Test that CertVerifyProcMac reacts appropriately when Apple's certificate
// verifier rejects a certificate with a fatal error. This is a regression
// test for https://crbug.com/472291.
// (Since 10.12, this causes a recoverable error instead of a fatal one.)
// TODO(mattm): Try to find a different way to cause a fatal error that works
// on 10.12.
TEST(CertVerifyProcMacTest, LargeKey) {
  // Load root_ca_cert.pem into the test root store.
  ScopedTestRoot test_root(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem").get());

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "large_key.pem"));

  // Apple's verifier rejects this certificate as invalid because the
  // RSA key is too large. If a future version of OS X changes this,
  // large_key.pem may need to be regenerated with a larger key.
  int flags = 0;
  CertVerifyResult verify_result;
  scoped_refptr<CertVerifyProc> verify_proc = new CertVerifyProcMac;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
}

}  // namespace

}  // namespace net
