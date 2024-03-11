// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {
namespace {

#include "net/data/ssl/chrome_root_store/chrome-root-store-test-data-inc.cc"

std::shared_ptr<const bssl::ParsedCertificate> ToParsedCertificate(
    const X509Certificate& cert) {
  bssl::CertErrors errors;
  std::shared_ptr<const bssl::ParsedCertificate> parsed =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(cert.cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), &errors);
  EXPECT_TRUE(parsed) << errors.ToDebugString();
  return parsed;
}

TEST(TrustStoreChromeTestNoFixture, ContainsCert) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          /*version=*/1);

  // Check every certificate in test_store.certs is included.
  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(certs.size(), 2u);

  for (const auto& cert : certs) {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*cert);
    ASSERT_TRUE(trust_store_chrome->Contains(parsed.get()));
    bssl::CertificateTrust trust = trust_store_chrome->GetTrust(parsed.get());
    EXPECT_EQ(bssl::CertificateTrust::ForTrustAnchor().ToDebugString(),
              trust.ToDebugString());
  }

  // Other certificates should not be included. Which test cert used here isn't
  // important as long as it isn't one of the certificates in the
  // chrome_root_store/test_store.certs.
  scoped_refptr<X509Certificate> other_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(other_cert);
  std::shared_ptr<const bssl::ParsedCertificate> other_parsed =
      ToParsedCertificate(*other_cert);
  ASSERT_FALSE(trust_store_chrome->Contains(other_parsed.get()));
  bssl::CertificateTrust trust =
      trust_store_chrome->GetTrust(other_parsed.get());
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST(TrustStoreChromeTestNoFixture, Constraints) {
  std::unique_ptr<TrustStoreChrome> trust_store_chrome =
      TrustStoreChrome::CreateTrustStoreForTesting(
          base::span<const ChromeRootCertInfo>(kChromeRootCertList),
          /*version=*/1);

  const std::string kUnconstrainedCertHash =
      "568d6905a2c88708a4b3025190edcfedb1974a606a13c6e5290fcb2ae63edab5";
  const std::string kConstrainedCertHash =
      "6b9c08e86eb0f767cfad65cd98b62149e5494a67f5845e7bd1ed019f27b86bd6";

  std::shared_ptr<const bssl::ParsedCertificate> constrained_cert;
  std::shared_ptr<const bssl::ParsedCertificate> unconstrained_cert;

  CertificateList certs = CreateCertificateListFromFile(
      GetTestNetDataDirectory().AppendASCII("ssl/chrome_root_store"),
      "test_store.certs", X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(certs.size(), 2u);
  for (const auto& cert : certs) {
    std::shared_ptr<const bssl::ParsedCertificate> parsed =
        ToParsedCertificate(*cert);
    std::string sha256_hex = base::ToLowerASCII(
        base::HexEncode(crypto::SHA256Hash(parsed->der_cert())));
    if (sha256_hex == kConstrainedCertHash) {
      constrained_cert = parsed;
    } else if (sha256_hex == kUnconstrainedCertHash) {
      unconstrained_cert = parsed;
    }
  }

  ASSERT_TRUE(unconstrained_cert);
  EXPECT_TRUE(
      trust_store_chrome->GetConstraintsForCert(unconstrained_cert.get())
          .empty());

  ASSERT_TRUE(constrained_cert);
  base::span<const ChromeRootCertConstraints> constraints =
      trust_store_chrome->GetConstraintsForCert(constrained_cert.get());
  ASSERT_EQ(constraints.size(), 2U);

  EXPECT_FALSE(constraints[0].sct_all_after.has_value());
  ASSERT_TRUE(constraints[0].sct_not_after.has_value());
  EXPECT_EQ(
      constraints[0].sct_not_after.value().InMillisecondsSinceUnixEpoch() /
          1000,
      0x5af);

  EXPECT_FALSE(constraints[1].sct_not_after.has_value());
  ASSERT_TRUE(constraints[1].sct_all_after.has_value());
  EXPECT_EQ(
      constraints[1].sct_all_after.value().InMillisecondsSinceUnixEpoch() /
          1000,
      0x2579);

  // Other certificates should return nullptr if they are queried for CRS
  // constraints. Which test cert used here isn't important as long as it isn't
  // one of the certificates in the chrome_root_store/test_store.certs.
  scoped_refptr<X509Certificate> other_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(other_cert);
  std::shared_ptr<const bssl::ParsedCertificate> other_parsed =
      ToParsedCertificate(*other_cert);
  ASSERT_TRUE(other_parsed);
  EXPECT_FALSE(trust_store_chrome->Contains(other_parsed.get()));
  EXPECT_TRUE(
      trust_store_chrome->GetConstraintsForCert(other_parsed.get()).empty());
}

}  // namespace
}  // namespace net
