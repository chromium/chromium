// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include "base/containers/span.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

#include "net/data/ssl/chrome_root_store/chrome-root-store-test-data-inc.cc"

std::shared_ptr<const ParsedCertificate> ToParsedCertificate(
    const X509Certificate& cert) {
  CertErrors errors;
  std::shared_ptr<const ParsedCertificate> parsed = ParsedCertificate::Create(
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
    std::shared_ptr<const ParsedCertificate> parsed =
        ToParsedCertificate(*cert);
    ASSERT_TRUE(trust_store_chrome->Contains(parsed.get()));
    CertificateTrust trust = trust_store_chrome->GetTrust(parsed.get());
    EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
              trust.ToDebugString());
  }

  // Other certificates should not be included. Which test cert used here isn't
  // important as long as it isn't one of the certificates in the
  // chrome_root_store/test_store.certs.
  scoped_refptr<X509Certificate> other_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(other_cert);
  std::shared_ptr<const ParsedCertificate> other_parsed =
      ToParsedCertificate(*other_cert);
  ASSERT_FALSE(trust_store_chrome->Contains(other_parsed.get()));
  CertificateTrust trust = trust_store_chrome->GetTrust(other_parsed.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

}  // namespace
}  // namespace net
