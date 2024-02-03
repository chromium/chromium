// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ignore_errors_cert_verifier.h"

#include <string_view>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/spki_hash_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::CertVerifier;
using net::MockCertVerifier;
using net::HashValue;
using net::SHA256HashValue;
using net::X509Certificate;
using net::TestCompletionCallback;
using net::CertVerifyResult;
using net::NetLogWithSource;

using net::ERR_CERT_INVALID;
using net::ERR_IO_PENDING;
using net::OK;

using net::test::IsError;
using net::test::IsOk;

namespace network {

static const char kTestUserDataDirSwitch[] = "test-user-data-dir";

static std::vector<std::string> MakeAllowlist() {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  net::CertificateList certs = net::CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  std::string_view cert_spki;
  SHA256HashValue hash;
  net::asn1::ExtractSPKIFromDERCert(
      net::x509_util::CryptoBufferAsStringPiece(certs[1]->cert_buffer()),
      &cert_spki);

  crypto::SHA256HashString(cert_spki, &hash, sizeof(SHA256HashValue));
  std::string hash_base64 = base::Base64Encode(hash.data);
  return {"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=", "foobar", hash_base64,
          "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB="};
}

class IgnoreErrorsCertVerifierTest : public ::testing::Test {
 public:
  IgnoreErrorsCertVerifierTest()
      : mock_verifier_(new MockCertVerifier()),
        verifier_(base::WrapUnique(mock_verifier_.get()), SPKIHashSet()) {}
  ~IgnoreErrorsCertVerifierTest() override { mock_verifier_ = nullptr; }

 protected:
  void SetUp() override {
    verifier_.SetAllowlistForTesting(CreateSPKIHashSet(MakeAllowlist()));
  }

  // The wrapped CertVerifier. Defaults to returning ERR_CERT_INVALID. Owned by
  // |verifier_|.
  raw_ptr<MockCertVerifier> mock_verifier_;
  IgnoreErrorsCertVerifier verifier_;
};

static void GetNonAllowlistedTestCert(scoped_refptr<X509Certificate>* out) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert);
  out->swap(test_cert);
}

static CertVerifier::RequestParams MakeRequestParams(
    const scoped_refptr<X509Certificate>& cert) {
  return CertVerifier::RequestParams(cert, "example.com", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
}

static void GetAllowlistedTestCert(scoped_refptr<X509Certificate>* out) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  *out = net::CreateCertificateChainFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(*out);
  ASSERT_EQ(2U, (*out)->intermediate_buffers().size());
}

TEST_F(IgnoreErrorsCertVerifierTest, TestNoMatchCertOk) {
  mock_verifier_->set_default_result(OK);

  scoped_refptr<X509Certificate> test_cert;
  ASSERT_NO_FATAL_FAILURE(GetNonAllowlistedTestCert(&test_cert));
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  EXPECT_THAT(callback.GetResult(verifier_.Verify(
                  MakeRequestParams(test_cert), &verify_result,
                  callback.callback(), &request, NetLogWithSource())),
              IsOk());
}

TEST_F(IgnoreErrorsCertVerifierTest, TestNoMatchCertError) {
  scoped_refptr<X509Certificate> test_cert;
  ASSERT_NO_FATAL_FAILURE(GetNonAllowlistedTestCert(&test_cert));
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  EXPECT_THAT(callback.GetResult(verifier_.Verify(
                  MakeRequestParams(test_cert), &verify_result,
                  callback.callback(), &request, NetLogWithSource())),
              IsError(ERR_CERT_INVALID));
}

TEST_F(IgnoreErrorsCertVerifierTest, TestMatch) {
  scoped_refptr<X509Certificate> test_cert;
  ASSERT_NO_FATAL_FAILURE(GetAllowlistedTestCert(&test_cert));
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  EXPECT_THAT(callback.GetResult(verifier_.Verify(
                  MakeRequestParams(test_cert), &verify_result,
                  callback.callback(), &request, NetLogWithSource())),
              IsOk());
}

class IgnoreCertificateErrorsSPKIListFlagTest
    : public ::testing::TestWithParam<bool> {
 public:
  IgnoreCertificateErrorsSPKIListFlagTest() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (GetParam()) {
      command_line.AppendSwitchASCII(kTestUserDataDirSwitch, "/foo/bar/baz");
    }
    command_line.AppendSwitchASCII(switches::kIgnoreCertificateErrorsSPKIList,
                                   base::JoinString(MakeAllowlist(), ","));

    auto mock_verifier = std::make_unique<MockCertVerifier>();
    mock_verifier->set_default_result(ERR_CERT_INVALID);
    verifier_ = IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
        command_line, kTestUserDataDirSwitch, std::move(mock_verifier));
  }
  ~IgnoreCertificateErrorsSPKIListFlagTest() override {}

 protected:
  std::unique_ptr<CertVerifier> verifier_;
};

// Only if both --user-data-dir and --ignore-certificate-errors-from-spki-list
// are present, certificate verification is bypassed.
TEST_P(IgnoreCertificateErrorsSPKIListFlagTest, TestUserDataDirSwitchRequired) {
  scoped_refptr<X509Certificate> test_cert;
  ASSERT_NO_FATAL_FAILURE(GetAllowlistedTestCert(&test_cert));
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  if (GetParam()) {
    EXPECT_THAT(callback.GetResult(verifier_->Verify(
                    MakeRequestParams(test_cert), &verify_result,
                    callback.callback(), &request, NetLogWithSource())),
                IsOk());
  } else {
    EXPECT_THAT(callback.GetResult(verifier_->Verify(
                    MakeRequestParams(test_cert), &verify_result,
                    callback.callback(), &request, NetLogWithSource())),
                IsError(ERR_CERT_INVALID));
  }
}

INSTANTIATE_TEST_SUITE_P(WithUserDataDirSwitchPresent,
                         IgnoreCertificateErrorsSPKIListFlagTest,
                         ::testing::Bool());

}  // namespace network
