// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_win.h"

#include <memory>

#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using net::test::IsError;
using net::test::IsOk;

// Tests that Windows debug data for the AuthRoot version is provided for
// successful certificate validations (in this case, using `ScopedTestRoot`).
TEST(CertVerifyProcWinTest, ReadsAuthRootVersionSuccessfulValidation) {
  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root);
  ScopedTestRoot test_root(root.get());

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert);

  scoped_refptr<CertVerifyProc> verify_proc =
      base::MakeRefCounted<CertVerifyProcWin>();

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());

  EXPECT_THAT(error, IsOk());

  const CertVerifyProcWin::ResultDebugData* win_debug_data =
      CertVerifyProcWin::ResultDebugData::Get(&verify_result);
  ASSERT_TRUE(win_debug_data);

  // Unfortunately, it's not possible to use something like
  // `registry_util::RegistryOverrideManager` to provide a fully fake CTL
  // (e.g. created by `CryptMsgEncodeAndSignCTL`), as CryptoAPI will still
  // attempt to validate the CTL and fail chain building if it is not able to.
  // While it's possible to check in a Microsoft-signed CTL as a resource and
  // use that to override, that still leaves a fair amount of dependency on
  // platform-specific behaviours.
  // Given the lack of easy substitution, the current test merely ensures that
  // DebugData is attached, but can't check that the values are sensible (e.g.
  // `!authroot_this_update().is_null()`), because the system that the test is
  // running on may not have populated the AuthRoot registry. However, the
  // following lines reflect "expected" results for a system with AuthRoot.
  // EXPECT_FALSE(win_debug_data->authroot_this_update().is_null());
  // EXPECT_FALSE(win_debug_data->authroot_sequence_number().empty());
}

// Tests that Windows debug data for the AuthRoot version is still provided
// even if certificate validation fails early (e.g. for an untrusted CA). This
// information should be available regardless of the verification result.
TEST(CertVerifyProcWinTest, ReadsAuthRootVersionFailedValidation) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert);

  scoped_refptr<CertVerifyProc> verify_proc =
      base::MakeRefCounted<CertVerifyProcWin>();

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());

  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));

  const CertVerifyProcWin::ResultDebugData* win_debug_data =
      CertVerifyProcWin::ResultDebugData::Get(&verify_result);
  ASSERT_TRUE(win_debug_data);

  // Unfortunately, it's not possible to use something like
  // `registry_util::RegistryOverrideManager` to provide a fully fake CTL
  // (e.g. created by `CryptMsgEncodeAndSignCTL`), as CryptoAPI will still
  // attempt to validate the CTL and fail chain building if it is not able to.
  // While it's possible to check in a Microsoft-signed CTL as a resource and
  // use that to override, that still leaves a fair amount of dependency on
  // platform-specific behaviours.
  // Given the lack of easy substitution, the current test merely ensures that
  // DebugData is attached, but can't check that the values are sensible (e.g.
  // `!authroot_this_update().is_null()`), because the system that the test is
  // running on may not have populated the AuthRoot registry. However, the
  // following lines reflect "expected" results for a system with AuthRoot.
  // EXPECT_FALSE(win_debug_data->authroot_this_update().is_null());
  // EXPECT_FALSE(win_debug_data->authroot_sequence_number().empty());
}

}  // namespace

}  // namespace net