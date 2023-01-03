// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/test_helpers.h"
#include "net/cert/pki/trust_store.h"
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

constexpr CertificateTrust ExpectedTrustForAnchor() {
  return CertificateTrust::ForTrustAnchor().WithEnforceAnchorExpiry();
}

::testing::AssertionResult ParseCertFromFile(
    base::StringPiece file_name,
    std::shared_ptr<const ParsedCertificate>* out_cert) {
  const scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(net::GetTestCertsDirectory(), file_name);
  if (!cert) {
    return ::testing::AssertionFailure() << "ImportCertFromFile failed";
  }
  CertErrors errors;
  std::shared_ptr<const ParsedCertificate> parsed = ParsedCertificate::Create(
      bssl::UpRef(cert->cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), &errors);
  if (!parsed) {
    return ::testing::AssertionFailure() << "ParseCertificate::Create failed:\n"
                                         << errors.ToDebugString();
  }
  *out_cert = parsed;
  return ::testing::AssertionSuccess();
}

class TrustStoreWinTest : public testing::Test {
 public:
  void SetUp() override {
    root_store_.reset(CertOpenStore(CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING,
                                    NULL, 0, nullptr));
    intermediate_store_.reset(CertOpenStore(
        CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING, NULL, 0, nullptr));
    disallowed_store_.reset(CertOpenStore(CERT_STORE_PROV_MEMORY,
                                          X509_ASN_ENCODING, NULL, 0, nullptr));

    ASSERT_TRUE(ParseCertFromFile("multi-root-A-by-B.pem", &a_by_b_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-B-by-C.pem", &b_by_c_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-B-by-F.pem", &b_by_f_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-C-by-D.pem", &c_by_d_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-C-by-E.pem", &c_by_e_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-D-by-D.pem", &d_by_d_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-E-by-E.pem", &e_by_e_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-F-by-E.pem", &f_by_e_));
  }

  // Returns true if |cert| successfully added to store, false otherwise.
  bool AddToStore(HCERTSTORE store,
                  std::shared_ptr<const ParsedCertificate> cert) {
    crypto::ScopedPCCERT_CONTEXT os_cert(CertCreateCertificateContext(
        X509_ASN_ENCODING, CRYPTO_BUFFER_data(cert->cert_buffer()),
        CRYPTO_BUFFER_len(cert->cert_buffer())));
    return CertAddCertificateContextToStore(store, os_cert.get(),
                                            CERT_STORE_ADD_ALWAYS, nullptr);
  }

  // Returns true if cert at file_name successfully added to store with
  // restricted usage, false otherwise.
  bool AddToStoreWithEKURestriction(
      HCERTSTORE store,
      std::shared_ptr<const ParsedCertificate> cert,
      LPCSTR usage_identifier) {
    crypto::ScopedPCCERT_CONTEXT os_cert(CertCreateCertificateContext(
        X509_ASN_ENCODING, CRYPTO_BUFFER_data(cert->cert_buffer()),
        CRYPTO_BUFFER_len(cert->cert_buffer())));

    CERT_ENHKEY_USAGE usage;
    memset(&usage, 0, sizeof(usage));
    if (!CertSetEnhancedKeyUsage(os_cert.get(), &usage)) {
      return false;
    }
    if (usage_identifier) {
      if (!CertAddEnhancedKeyUsageIdentifier(os_cert.get(), usage_identifier)) {
        return false;
      }
    }
    return !!CertAddCertificateContextToStore(store, os_cert.get(),
                                              CERT_STORE_ADD_ALWAYS, nullptr);
  }

  std::unique_ptr<TrustStoreWin> CreateTrustStoreWin() {
    return TrustStoreWin::CreateForTesting(std::move(root_store_),
                                           std::move(intermediate_store_),
                                           std::move(disallowed_store_));
  }

  // The cert stores that will be used to create the trust store. These handles
  // will be null after CreateTrustStoreWin() is called.
  crypto::ScopedHCERTSTORE root_store_;
  crypto::ScopedHCERTSTORE intermediate_store_;
  crypto::ScopedHCERTSTORE disallowed_store_;

  std::shared_ptr<const ParsedCertificate> a_by_b_, b_by_c_, b_by_f_, c_by_d_,
      c_by_e_, d_by_d_, e_by_e_, f_by_e_;
};

TEST_F(TrustStoreWinTest, GetTrustInitializationError) {
  // Simulate an initialization error by using null stores.
  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(crypto::ScopedHCERTSTORE(),
                                      crypto::ScopedHCERTSTORE(),
                                      crypto::ScopedHCERTSTORE());
  ASSERT_TRUE(trust_store_win);
  CertificateTrust trust = trust_store_win->GetTrust(d_by_d_.get(), nullptr);
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST_F(TrustStoreWinTest, GetTrust) {
  ASSERT_TRUE(AddToStore(root_store_.get(), d_by_d_));
  ASSERT_TRUE(AddToStore(intermediate_store_.get(), c_by_d_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // Explicitly trusted root should be trusted.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get(), nullptr).ToDebugString());

  // Intermediate for path building should not be trusted.
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(c_by_d_.get(), nullptr).ToDebugString());

  // Unknown roots should not be trusted (e.g. just because they're
  // self-signed doesn't make them a root)
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get(), nullptr).ToDebugString());
}

// This test has a special TrustStoreWin setup with restricted EKU usages.
// Specifically, the only certs set up in the root store are set up
// as follows:
//
// - kMultiRootDByD: only has szOID_PKIX_KP_SERVER_AUTH EKU set
// - kMultiRootEByE: only has szOID_PKIX_KP_CLIENT_AUTH set
// - kMultiRootCByE: only has szOID_ANY_ENHANCED_KEY_USAGE set
// - kMultiRootCByD: no EKU usages set
TEST_F(TrustStoreWinTest, GetTrustRestrictedEKU) {
  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store_.get(), d_by_d_,
                                           szOID_PKIX_KP_SERVER_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store_.get(), e_by_e_,
                                           szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store_.get(), c_by_e_,
                                           szOID_ANY_ENHANCED_KEY_USAGE));
  ASSERT_TRUE(
      AddToStoreWithEKURestriction(root_store_.get(), c_by_d_, nullptr));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // Root cert with EKU szOID_PKIX_KP_SERVER_AUTH usage set should be
  // trusted.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get(), nullptr).ToDebugString());

  // Root cert with EKU szOID_ANY_ENHANCED_KEY_USAGE usage set should be
  // trusted.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(c_by_e_.get(), nullptr).ToDebugString());

  // Root cert with EKU szOID_PKIX_KP_CLIENT_AUTH does not allow usage of
  // cert for server auth, return UNSPECIFIED.
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get(), nullptr).ToDebugString());

  // Root cert with no EKU usages, return UNSPECIFIED.
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(c_by_d_.get(), nullptr).ToDebugString());

  // Unknown cert has unspecified trust.
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(f_by_e_.get(), nullptr).ToDebugString());
}

// If duplicate certs are added to the root store with different EKU usages,
// the cert should be trusted if any one of the usages is valid.
// Root store set up as follows:
//
// - kMultiRootDByD: only has szOID_PKIX_KP_CLIENT_AUTH EKU set
// - kMultiRootDByD (dupe): only has szOID_PKIX_KP_SERVER_AUTH set
// - kMultiRootDByD (dupe 2): no EKU usages set
TEST_F(TrustStoreWinTest, GetTrustRestrictedEKUDuplicateCerts) {
  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store_.get(), d_by_d_,
                                           szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(root_store_.get(), d_by_d_,
                                           szOID_PKIX_KP_SERVER_AUTH));
  ASSERT_TRUE(
      AddToStoreWithEKURestriction(root_store_.get(), d_by_d_, nullptr));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // One copy of the Root cert is trusted for TLS Server Auth.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get(), nullptr).ToDebugString());
}

// Test that disallowed certs will be distrusted regardless of EKU settings.
TEST_F(TrustStoreWinTest, GetTrustDisallowedCerts) {
  ASSERT_TRUE(AddToStore(root_store_.get(), d_by_d_));
  ASSERT_TRUE(AddToStore(root_store_.get(), e_by_e_));

  ASSERT_TRUE(AddToStoreWithEKURestriction(disallowed_store_.get(), d_by_d_,
                                           szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStore(disallowed_store_.get(), e_by_e_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // E-by-E is in both root and distrusted store. Distrust takes precedence.
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get(), nullptr).ToDebugString());

  // D-by-D is in root and in distrusted but without szOID_PKIX_KP_SERVER_AUTH
  // set. It should still be distrusted since the EKU settings aren't checked
  // on distrust.
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get(), nullptr).ToDebugString());
}

MATCHER_P(ParsedCertEq, expected_cert, "") {
  return arg && expected_cert &&
         base::ranges::equal(arg->der_cert().AsSpan(),
                             expected_cert->der_cert().AsSpan());
}

TEST_F(TrustStoreWinTest, GetIssuersInitializationError) {
  // Simulate an initialization error by using null stores.
  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(crypto::ScopedHCERTSTORE(),
                                      crypto::ScopedHCERTSTORE(),
                                      crypto::ScopedHCERTSTORE());
  ASSERT_TRUE(trust_store_win);
  ParsedCertificateList issuers;
  trust_store_win->SyncGetIssuersOf(b_by_f_.get(), &issuers);
  ASSERT_EQ(0U, issuers.size());
}

TEST_F(TrustStoreWinTest, GetIssuers) {
  ASSERT_TRUE(AddToStore(root_store_.get(), d_by_d_));
  ASSERT_TRUE(AddToStore(intermediate_store_.get(), c_by_d_));
  ASSERT_TRUE(AddToStore(intermediate_store_.get(), c_by_e_));
  ASSERT_TRUE(AddToStore(intermediate_store_.get(), f_by_e_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();

  // No matching issuer.
  {
    ParsedCertificateList issuers;
    trust_store_win->SyncGetIssuersOf(a_by_b_.get(), &issuers);
    ASSERT_EQ(0U, issuers.size());
  }

  // Single matching issuer found in intermediates.
  {
    ParsedCertificateList issuers;
    trust_store_win->SyncGetIssuersOf(b_by_f_.get(), &issuers);
    ASSERT_EQ(1U, issuers.size());
    EXPECT_THAT(issuers, testing::UnorderedElementsAre(ParsedCertEq(f_by_e_)));
  }

  // Single matching issuer found in roots.
  {
    ParsedCertificateList issuers;
    trust_store_win->SyncGetIssuersOf(d_by_d_.get(), &issuers);
    ASSERT_EQ(1U, issuers.size());
    EXPECT_THAT(issuers, testing::UnorderedElementsAre(ParsedCertEq(d_by_d_)));
  }

  // Multiple issuers found.
  {
    ParsedCertificateList issuers;
    trust_store_win->SyncGetIssuersOf(b_by_c_.get(), &issuers);
    ASSERT_EQ(2U, issuers.size());
    EXPECT_THAT(issuers, testing::UnorderedElementsAre(ParsedCertEq(c_by_d_),
                                                       ParsedCertEq(c_by_e_)));
  }
}

}  // namespace
}  // namespace net
