// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include "base/containers/span.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

struct ChromeRootCertInfo {
  base::span<const uint8_t> root_cert_der;
};

namespace {

#include "net/data/ssl/chrome_root_store/chrome-root-store-test-data-inc.cc"

scoped_refptr<ParsedCertificate> ParseCertFromFile(base::FilePath dir_name,
                                                   std::string file_name) {
  const scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(dir_name, file_name);
  if (!cert) {
    return nullptr;
  }
  CertErrors errors;
  scoped_refptr<ParsedCertificate> parsed = ParsedCertificate::Create(
      bssl::UpRef(cert->cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), &errors);
  EXPECT_TRUE(parsed) << errors.ToDebugString();
  return parsed;
}

const base::FilePath kCertDirPath = GetTestNetDataDirectory().AppendASCII(
    "ssl/chrome_root_store/testdata/certs");

const char kCertInStoreFile1[] =
    "568d6905a2c88708a4b3025190edcfedb1974a606a13c6e5290fcb2ae63edab5.pem";
const char kCertInStoreFile2[] =
    "6b9c08e86eb0f767cfad65cd98b62149e5494a67f5845e7bd1ed019f27b86bd6.pem";
const char kCertNotInStoreFile[] =
    "c45d7bb08e6d67e62e4235110b564e5f78fd92ef058c840aea4e6455d7585c60.pem";

TEST(TrustStoreChromeTestNoFixture, ContainsCert) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList));

  scoped_refptr<ParsedCertificate> trust_cert_1, trust_cert_2,
      trust_cert_missing;
  ASSERT_TRUE(trust_cert_1 =
                  ParseCertFromFile(kCertDirPath, kCertInStoreFile1));
  ASSERT_TRUE(trust_store_chrome->Contains(trust_cert_1.get()));

  ASSERT_TRUE(trust_cert_2 =
                  ParseCertFromFile(kCertDirPath, kCertInStoreFile2));
  ASSERT_TRUE(trust_store_chrome->Contains(trust_cert_2.get()));

  // Cert should not be in the trust store.
  ASSERT_TRUE(trust_cert_missing =
                  ParseCertFromFile(kCertDirPath, kCertNotInStoreFile));
  ASSERT_FALSE(trust_store_chrome->Contains(trust_cert_missing.get()));
}

TEST(TrustStoreChromeTestNoFixture, ContainsCerts) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList));

  CertificateTrust trust;
  scoped_refptr<ParsedCertificate> trust_cert_1, trust_cert_2,
      trust_cert_missing;

  ASSERT_TRUE(trust_cert_1 =
                  ParseCertFromFile(kCertDirPath, kCertInStoreFile1));
  trust =
      trust_store_chrome->GetTrust(trust_cert_1.get(), /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::TRUSTED_ANCHOR, trust.type);

  ASSERT_TRUE(trust_cert_2 =
                  ParseCertFromFile(kCertDirPath, kCertInStoreFile2));
  trust =
      trust_store_chrome->GetTrust(trust_cert_2.get(), /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::TRUSTED_ANCHOR, trust.type);

  ASSERT_TRUE(trust_cert_missing =
                  ParseCertFromFile(kCertDirPath, kCertNotInStoreFile));
  trust = trust_store_chrome->GetTrust(trust_cert_missing.get(),
                                       /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrustType::UNSPECIFIED, trust.type);
}

}  // namespace
}  // namespace net
