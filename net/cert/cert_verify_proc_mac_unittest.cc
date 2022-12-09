// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_mac.h"

#include <Security/Security.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251 but deprecation
// warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

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

#pragma clang diagnostic pop

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
  scoped_refptr<CertVerifyProc> verify_proc =
      base::MakeRefCounted<CertVerifyProcMac>();
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
}

// Test that CertVerifierMac on 10.15+ appropriately flags certificates
// that violate https://support.apple.com/en-us/HT210176 as having
// too long validity, rather than being invalid certificates.
TEST(CertVerifyProcMacTest, CertValidityTooLong) {
  // Load root_ca_cert.pem into the test root store.
  ScopedTestRoot test_root(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem").get());

  scoped_refptr<X509Certificate> cert(ImportCertFromFile(
      GetTestCertsDirectory(), "900_days_after_2019_07_01.pem"));

  int flags = 0;
  CertVerifyResult verify_result;
  scoped_refptr<CertVerifyProc> verify_proc =
      base::MakeRefCounted<CertVerifyProcMac>();
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());

  if (base::mac::IsAtLeastOS10_15()) {
    EXPECT_THAT(error, IsError(ERR_CERT_VALIDITY_TOO_LONG));
    EXPECT_EQ((verify_result.cert_status & CERT_STATUS_ALL_ERRORS),
              CERT_STATUS_VALIDITY_TOO_LONG);
  } else {
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_VALIDITY_TOO_LONG);
    EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_INVALID);
  }
}

}  // namespace

}  // namespace net
