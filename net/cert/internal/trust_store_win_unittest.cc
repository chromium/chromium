// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_win.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

// These tests use a series of cross-signed certificates. The overall
// hierarchy is documented in
// //net/data/ssl/scripts/generate-multi-root-test-chains.sh. TrustStoreWin is
// set up as follows:

// Trusted as Root: D-by-D
constexpr char kMultiRootDByD[] = "multi-root-D-by-D.pem";

// Known Intermediates: C-by-D, C-by-E, F-by-E
constexpr char kMultiRootCByD[] = "multi-root-C-by-D.pem";
constexpr char kMultiRootCByE[] = "multi-root-C-by-E.pem";
constexpr char kMultiRootFByE[] = "multi-root-F-by-E.pem";

scoped_refptr<ParsedCertificate> ParseCertFromFile(
    base::StringPiece file_name) {
  const scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(net::GetTestCertsDirectory(), file_name);
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

void AddToStore(HCERTSTORE store, const std::string file_name) {
  const scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(net::GetTestCertsDirectory(), file_name);
  crypto::ScopedPCCERT_CONTEXT os_cert(CertCreateCertificateContext(
      X509_ASN_ENCODING, CRYPTO_BUFFER_data(cert->cert_buffer()),
      CRYPTO_BUFFER_len(cert->cert_buffer())));
  CertAddCertificateContextToStore(store, os_cert.get(), CERT_STORE_ADD_ALWAYS,
                                   NULL);
}

std::unique_ptr<TrustStoreWin> CreateTrustStoreWin() {
  crypto::ScopedHCERTSTORE root_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE intermediate_cert_store(CertOpenStore(
      CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE all_certs_store(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));

  CertAddStoreToCollection(all_certs_store.get(), intermediate_cert_store.get(),
                           /*dwUpdateFlags=*/0, /*dwPriority=*/0);
  CertAddStoreToCollection(all_certs_store.get(), root_store.get(),
                           /*dwUpdateFlags=*/0, /*dwPriority=*/0);
  AddToStore(root_store.get(), kMultiRootDByD);
  AddToStore(intermediate_cert_store.get(), kMultiRootCByD);
  AddToStore(intermediate_cert_store.get(), kMultiRootCByE);
  AddToStore(intermediate_cert_store.get(), kMultiRootFByE);

  return TrustStoreWin::CreateForTesting(std::move(root_store),
                                         std::move(all_certs_store));
}

TEST(TrustStoreWin, GetTrust) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();

  constexpr struct TestData {
    base::StringPiece file_name;
    CertificateTrustType expected_result;
  } kTestData[] = {
      // Explicitly trusted root should be trusted.
      {kMultiRootDByD, CertificateTrustType::TRUSTED_ANCHOR},
      // Intermediate for path building should not be trusted.
      {kMultiRootCByD, CertificateTrustType::UNSPECIFIED},
      // Unknown roots should not be trusted (e.g. just because they're
      // self-signed doesn't make them a root)
      {"multi-root-E-by-E.pem", CertificateTrustType::UNSPECIFIED},
  };
  for (const auto& test_data : kTestData) {
    SCOPED_TRACE(test_data.file_name);
    CertificateTrust trust;
    auto parsed_cert = ParseCertFromFile(test_data.file_name);
    trust_store_win->GetTrust(parsed_cert, &trust, nullptr);
    EXPECT_EQ(test_data.expected_result, trust.type);
  }
}

MATCHER_P(ParsedCertEq, expected_cert, "") {
  return arg && expected_cert &&
         base::ranges::equal(arg->der_cert().AsSpan(),
                             expected_cert->der_cert().AsSpan());
}

TEST(TrustStoreWin, GetIssuersNoIssuerFound) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();

  ParsedCertificateList issuers;
  scoped_refptr<ParsedCertificate> cert =
      ParseCertFromFile("multi-root-A-by-B.pem");
  trust_store_win->SyncGetIssuersOf(cert.get(), &issuers);
  ASSERT_EQ(0U, issuers.size());
}

TEST(TrustStoreWin, GetIssuersSingleIssuerFoundFromIntermediates) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ParsedCertificateList issuers;
  scoped_refptr<ParsedCertificate> cert =
      ParseCertFromFile("multi-root-B-by-F.pem");
  trust_store_win->SyncGetIssuersOf(cert.get(), &issuers);
  ASSERT_EQ(1U, issuers.size());
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           ParsedCertEq(ParseCertFromFile(kMultiRootFByE))));
}

TEST(TrustStoreWin, GetIssuersSingleIssuerFoundFromRoot) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ParsedCertificateList issuers;
  scoped_refptr<ParsedCertificate> cert = ParseCertFromFile(kMultiRootDByD);
  trust_store_win->SyncGetIssuersOf(cert.get(), &issuers);
  ASSERT_EQ(1U, issuers.size());
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(ParsedCertEq(cert)));
}

TEST(TrustStoreWin, GetIssuersMultipleIssuersFound) {
  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ParsedCertificateList issuers;
  scoped_refptr<ParsedCertificate> cert =
      ParseCertFromFile("multi-root-B-by-C.pem");
  trust_store_win->SyncGetIssuersOf(cert.get(), &issuers);
  ASSERT_EQ(2U, issuers.size());
  EXPECT_THAT(issuers, testing::UnorderedElementsAre(
                           ParsedCertEq(ParseCertFromFile(kMultiRootCByD)),
                           ParsedCertEq(ParseCertFromFile(kMultiRootCByE))));
}

}  // namespace
}  // namespace net