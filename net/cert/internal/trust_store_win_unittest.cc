// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/features.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/internal/trust_store_features.h"
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

class TrustStoreWinTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  TrustStoreWinTest()
      : scoped_enforce_local_anchor_constraints_(
            ExpectedEnforceLocalAnchorConstraintsEnabled()) {
    if (ExpectedTrustedLeafSupportEnabled()) {
      feature_list_.InitAndEnableFeature(
          features::kTrustStoreTrustedLeafSupport);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kTrustStoreTrustedLeafSupport);
    }
  }

  void SetUp() override {
    ASSERT_TRUE(ParseCertFromFile("multi-root-A-by-B.pem", &a_by_b_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-B-by-C.pem", &b_by_c_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-B-by-F.pem", &b_by_f_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-C-by-D.pem", &c_by_d_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-C-by-E.pem", &c_by_e_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-D-by-D.pem", &d_by_d_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-E-by-E.pem", &e_by_e_));
    ASSERT_TRUE(ParseCertFromFile("multi-root-F-by-E.pem", &f_by_e_));
  }

  bool ExpectedTrustedLeafSupportEnabled() const {
    return std::get<0>(GetParam());
  }

  bool ExpectedEnforceLocalAnchorConstraintsEnabled() const {
    return std::get<1>(GetParam());
  }

  CertificateTrust ExpectedTrustForAnchor() const {
    if (ExpectedTrustedLeafSupportEnabled()) {
      return CertificateTrust::ForTrustAnchorOrLeaf()
          .WithEnforceAnchorExpiry()
          .WithEnforceAnchorConstraints(
              ExpectedEnforceLocalAnchorConstraintsEnabled())
          .WithRequireLeafSelfSigned();
    } else {
      return CertificateTrust::ForTrustAnchor()
          .WithEnforceAnchorExpiry()
          .WithEnforceAnchorConstraints(
              ExpectedEnforceLocalAnchorConstraintsEnabled());
    }
  }

  CertificateTrust ExpectedTrustForPeer() const {
    if (ExpectedTrustedLeafSupportEnabled()) {
      return CertificateTrust::ForTrustedLeaf().WithRequireLeafSelfSigned();
    } else {
      return CertificateTrust::ForUnspecified();
    }
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
    return TrustStoreWin::CreateForTesting(std::move(stores_));
  }

  // The cert stores that will be used to create the trust store. These handles
  // will be null after CreateTrustStoreWin() is called.
  TrustStoreWin::CertStores stores_ =
      TrustStoreWin::CertStores::CreateInMemoryStoresForTesting();

  std::shared_ptr<const ParsedCertificate> a_by_b_, b_by_c_, b_by_f_, c_by_d_,
      c_by_e_, d_by_d_, e_by_e_, f_by_e_;

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedLocalAnchorConstraintsEnforcementForTesting
      scoped_enforce_local_anchor_constraints_;
};

TEST_P(TrustStoreWinTest, GetTrustInitializationError) {
  // Simulate an initialization error by using null stores.
  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(
          TrustStoreWin::CertStores::CreateNullStoresForTesting());
  ASSERT_TRUE(trust_store_win);
  CertificateTrust trust = trust_store_win->GetTrust(d_by_d_.get(), nullptr);
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST_P(TrustStoreWinTest, GetTrust) {
  ASSERT_TRUE(AddToStore(stores_.roots.get(), d_by_d_));
  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), c_by_d_));
  ASSERT_TRUE(AddToStore(stores_.trusted_people.get(), a_by_b_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // Explicitly trusted root should be trusted.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get(), nullptr).ToDebugString());

  // Explicitly trusted peer should be trusted.
  // (Although it wouldn't actually verify since it's not self-signed but has
  // require_leaf_selfsigned set. That doesn't matter for the purposes of these
  // tests.)
  EXPECT_EQ(ExpectedTrustForPeer().ToDebugString(),
            trust_store_win->GetTrust(a_by_b_.get(), nullptr).ToDebugString());

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
TEST_P(TrustStoreWinTest, GetTrustRestrictedEKU) {
  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.roots.get(), d_by_d_,
                                           szOID_PKIX_KP_SERVER_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.roots.get(), e_by_e_,
                                           szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.roots.get(), c_by_e_,
                                           szOID_ANY_ENHANCED_KEY_USAGE));
  ASSERT_TRUE(
      AddToStoreWithEKURestriction(stores_.roots.get(), c_by_d_, nullptr));

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

// Same as GetTrustRestrictedEKU but for the Trusted People store.
TEST_P(TrustStoreWinTest, GetTrustTrustedPeopleRestrictedEKU) {
  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.trusted_people.get(),
                                           d_by_d_, szOID_PKIX_KP_SERVER_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.trusted_people.get(),
                                           e_by_e_, szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(
      stores_.trusted_people.get(), c_by_e_, szOID_ANY_ENHANCED_KEY_USAGE));
  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.trusted_people.get(),
                                           c_by_d_, nullptr));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // TrustedPeople cert with EKU szOID_PKIX_KP_SERVER_AUTH usage set should be
  // trusted.
  EXPECT_EQ(ExpectedTrustForPeer().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get(), nullptr).ToDebugString());

  // TrustedPeople cert with EKU szOID_ANY_ENHANCED_KEY_USAGE usage set should
  // be trusted.
  EXPECT_EQ(ExpectedTrustForPeer().ToDebugString(),
            trust_store_win->GetTrust(c_by_e_.get(), nullptr).ToDebugString());

  // TrustedPeople cert with EKU szOID_PKIX_KP_CLIENT_AUTH does not allow usage
  // of cert for server auth, return UNSPECIFIED.
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get(), nullptr).ToDebugString());

  // TrustedPeople cert with no EKU usages, return UNSPECIFIED.
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
TEST_P(TrustStoreWinTest, GetTrustRestrictedEKUDuplicateCerts) {
  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.roots.get(), d_by_d_,
                                           szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.roots.get(), d_by_d_,
                                           szOID_PKIX_KP_SERVER_AUTH));
  ASSERT_TRUE(
      AddToStoreWithEKURestriction(stores_.roots.get(), d_by_d_, nullptr));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // One copy of the Root cert is trusted for TLS Server Auth.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get(), nullptr).ToDebugString());
}

// Test that disallowed certs will be distrusted regardless of EKU settings.
TEST_P(TrustStoreWinTest, GetTrustDisallowedCerts) {
  ASSERT_TRUE(AddToStore(stores_.roots.get(), d_by_d_));
  ASSERT_TRUE(AddToStore(stores_.roots.get(), e_by_e_));
  ASSERT_TRUE(AddToStore(stores_.trusted_people.get(), f_by_e_));

  ASSERT_TRUE(AddToStoreWithEKURestriction(stores_.disallowed.get(), d_by_d_,
                                           szOID_PKIX_KP_CLIENT_AUTH));
  ASSERT_TRUE(AddToStore(stores_.disallowed.get(), e_by_e_));
  ASSERT_TRUE(AddToStore(stores_.disallowed.get(), f_by_e_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // E-by-E is in both root and distrusted store. Distrust takes precedence.
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get(), nullptr).ToDebugString());

  // F-by-E is in both trusted people and distrusted store. Distrust takes
  // precedence.
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust_store_win->GetTrust(f_by_e_.get(), nullptr).ToDebugString());

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

TEST_P(TrustStoreWinTest, GetIssuersInitializationError) {
  // Simulate an initialization error by using null stores.
  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(
          TrustStoreWin::CertStores::CreateNullStoresForTesting());
  ASSERT_TRUE(trust_store_win);
  ParsedCertificateList issuers;
  trust_store_win->SyncGetIssuersOf(b_by_f_.get(), &issuers);
  ASSERT_EQ(0U, issuers.size());
}

TEST_P(TrustStoreWinTest, GetIssuers) {
  ASSERT_TRUE(AddToStore(stores_.roots.get(), d_by_d_));

  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), c_by_d_));
  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), c_by_e_));
  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), f_by_e_));

  ASSERT_TRUE(AddToStore(stores_.trusted_people.get(), b_by_c_));

  ASSERT_TRUE(AddToStore(stores_.disallowed.get(), b_by_f_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();

  // No matching issuer (Trusted People and Disallowed are not consulted).
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

INSTANTIATE_TEST_SUITE_P(
    All,
    TrustStoreWinTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<TrustStoreWinTest::ParamType>& info) {
      return std::string(std::get<0>(info.param) ? "TrustedLeafSupported"
                                                 : "TrustAnchorOnly") +
             (std::get<1>(info.param) ? "EnforceLocalAnchorConstraints"
                                      : "NoLocalAnchorConstraints");
    });

}  // namespace
}  // namespace net
