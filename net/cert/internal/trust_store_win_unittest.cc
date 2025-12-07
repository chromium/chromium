// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/features.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_win.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

namespace {

::testing::AssertionResult ParseCertFromFile(
    std::string_view file_name,
    std::shared_ptr<const bssl::ParsedCertificate>* out_cert) {
  const scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(net::GetTestCertsDirectory(), file_name);
  if (!cert) {
    return ::testing::AssertionFailure() << "ImportCertFromFile failed";
  }
  bssl::CertErrors errors;
  std::shared_ptr<const bssl::ParsedCertificate> parsed =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(cert->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), &errors);
  if (!parsed) {
    return ::testing::AssertionFailure()
           << "bssl::ParseCertificate::Create failed:\n"
           << errors.ToDebugString();
  }
  *out_cert = parsed;
  return ::testing::AssertionSuccess();
}

class TrustStoreWinTest : public testing::Test {
 public:
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

  bssl::CertificateTrust ExpectedTrustForAnchor() const {
    return bssl::CertificateTrust::ForTrustAnchorOrLeaf()
        .WithEnforceAnchorExpiry()
        .WithEnforceAnchorConstraints()
        .WithRequireLeafSelfSigned();
  }

  bssl::CertificateTrust ExpectedTrustForPeer() const {
    return bssl::CertificateTrust::ForTrustedLeaf().WithRequireLeafSelfSigned();
  }

  // Returns true if |cert| successfully added to store, false otherwise.
  bool AddToStore(HCERTSTORE store,
                  std::shared_ptr<const bssl::ParsedCertificate> cert) {
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
      std::shared_ptr<const bssl::ParsedCertificate> cert,
      LPCSTR usage_identifier) {
    crypto::ScopedPCCERT_CONTEXT os_cert(CertCreateCertificateContext(
        X509_ASN_ENCODING, CRYPTO_BUFFER_data(cert->cert_buffer()),
        CRYPTO_BUFFER_len(cert->cert_buffer())));

    CERT_ENHKEY_USAGE usage = {};
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

  std::shared_ptr<const bssl::ParsedCertificate> a_by_b_, b_by_c_, b_by_f_,
      c_by_d_, c_by_e_, d_by_d_, e_by_e_, f_by_e_;
};

TEST_F(TrustStoreWinTest, GetTrustInitializationError) {
  // Simulate an initialization error by using null stores.
  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(
          TrustStoreWin::CertStores::CreateNullStoresForTesting());
  ASSERT_TRUE(trust_store_win);
  bssl::CertificateTrust trust = trust_store_win->GetTrust(d_by_d_.get());
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST_F(TrustStoreWinTest, GetTrust) {
  ASSERT_TRUE(AddToStore(stores_.roots.get(), d_by_d_));
  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), c_by_d_));
  ASSERT_TRUE(AddToStore(stores_.trusted_people.get(), a_by_b_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();
  ASSERT_TRUE(trust_store_win);

  // Explicitly trusted root should be trusted.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get()).ToDebugString());

  // Explicitly trusted peer should be trusted.
  // (Although it wouldn't actually verify since it's not self-signed but has
  // require_leaf_selfsigned set. That doesn't matter for the purposes of these
  // tests.)
  EXPECT_EQ(ExpectedTrustForPeer().ToDebugString(),
            trust_store_win->GetTrust(a_by_b_.get()).ToDebugString());

  // Intermediate for path building should not be trusted.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(c_by_d_.get()).ToDebugString());

  // Unknown roots should not be trusted (e.g. just because they're
  // self-signed doesn't make them a root)
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get()).ToDebugString());
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
            trust_store_win->GetTrust(d_by_d_.get()).ToDebugString());

  // Root cert with EKU szOID_ANY_ENHANCED_KEY_USAGE usage set should be
  // trusted.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(c_by_e_.get()).ToDebugString());

  // Root cert with EKU szOID_PKIX_KP_CLIENT_AUTH does not allow usage of
  // cert for server auth, return UNSPECIFIED.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get()).ToDebugString());

  // Root cert with no EKU usages, return UNSPECIFIED.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(c_by_d_.get()).ToDebugString());

  // Unknown cert has unspecified trust.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(f_by_e_.get()).ToDebugString());
}

// Same as GetTrustRestrictedEKU but for the Trusted People store.
TEST_F(TrustStoreWinTest, GetTrustTrustedPeopleRestrictedEKU) {
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
            trust_store_win->GetTrust(d_by_d_.get()).ToDebugString());

  // TrustedPeople cert with EKU szOID_ANY_ENHANCED_KEY_USAGE usage set should
  // be trusted.
  EXPECT_EQ(ExpectedTrustForPeer().ToDebugString(),
            trust_store_win->GetTrust(c_by_e_.get()).ToDebugString());

  // TrustedPeople cert with EKU szOID_PKIX_KP_CLIENT_AUTH does not allow usage
  // of cert for server auth, return UNSPECIFIED.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get()).ToDebugString());

  // TrustedPeople cert with no EKU usages, return UNSPECIFIED.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(c_by_d_.get()).ToDebugString());

  // Unknown cert has unspecified trust.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(f_by_e_.get()).ToDebugString());
}

// If duplicate certs are added to the root store with different EKU usages,
// the cert should be trusted if any one of the usages is valid.
// Root store set up as follows:
//
// - kMultiRootDByD: only has szOID_PKIX_KP_CLIENT_AUTH EKU set
// - kMultiRootDByD (dupe): only has szOID_PKIX_KP_SERVER_AUTH set
// - kMultiRootDByD (dupe 2): no EKU usages set
TEST_F(TrustStoreWinTest, GetTrustRestrictedEKUDuplicateCerts) {
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
            trust_store_win->GetTrust(d_by_d_.get()).ToDebugString());
}

// Test that disallowed certs will be distrusted regardless of EKU settings.
TEST_F(TrustStoreWinTest, GetTrustDisallowedCerts) {
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
  EXPECT_EQ(bssl::CertificateTrust::ForDistrusted().ToDebugString(),
            trust_store_win->GetTrust(e_by_e_.get()).ToDebugString());

  // F-by-E is in both trusted people and distrusted store. Distrust takes
  // precedence.
  EXPECT_EQ(bssl::CertificateTrust::ForDistrusted().ToDebugString(),
            trust_store_win->GetTrust(f_by_e_.get()).ToDebugString());

  // D-by-D is in root and in distrusted but without szOID_PKIX_KP_SERVER_AUTH
  // set. It should still be distrusted since the EKU settings aren't checked
  // on distrust.
  EXPECT_EQ(bssl::CertificateTrust::ForDistrusted().ToDebugString(),
            trust_store_win->GetTrust(d_by_d_.get()).ToDebugString());
}

MATCHER_P(ParsedCertEq, expected_cert, "") {
  return arg && expected_cert &&
         std::ranges::equal(arg->der_cert(), expected_cert->der_cert());
}

TEST_F(TrustStoreWinTest, GetIssuersInitializationError) {
  // Simulate an initialization error by using null stores.
  std::unique_ptr<TrustStoreWin> trust_store_win =
      TrustStoreWin::CreateForTesting(
          TrustStoreWin::CertStores::CreateNullStoresForTesting());
  ASSERT_TRUE(trust_store_win);
  bssl::ParsedCertificateList issuers;
  trust_store_win->SyncGetIssuersOf(b_by_f_.get(), &issuers);
  ASSERT_EQ(0U, issuers.size());
}

TEST_F(TrustStoreWinTest, GetIssuers) {
  ASSERT_TRUE(AddToStore(stores_.roots.get(), d_by_d_));

  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), c_by_d_));
  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), c_by_e_));
  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), f_by_e_));

  ASSERT_TRUE(AddToStore(stores_.trusted_people.get(), b_by_c_));

  ASSERT_TRUE(AddToStore(stores_.disallowed.get(), b_by_f_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();

  // No matching issuer (Trusted People and Disallowed are not consulted).
  {
    bssl::ParsedCertificateList issuers;
    trust_store_win->SyncGetIssuersOf(a_by_b_.get(), &issuers);
    ASSERT_EQ(0U, issuers.size());
  }

  // Single matching issuer found in intermediates.
  {
    bssl::ParsedCertificateList issuers;
    trust_store_win->SyncGetIssuersOf(b_by_f_.get(), &issuers);
    ASSERT_EQ(1U, issuers.size());
    EXPECT_THAT(issuers, testing::UnorderedElementsAre(ParsedCertEq(f_by_e_)));
  }

  // Single matching issuer found in roots.
  {
    bssl::ParsedCertificateList issuers;
    trust_store_win->SyncGetIssuersOf(d_by_d_.get(), &issuers);
    ASSERT_EQ(1U, issuers.size());
    EXPECT_THAT(issuers, testing::UnorderedElementsAre(ParsedCertEq(d_by_d_)));
  }

  // Multiple issuers found.
  {
    bssl::ParsedCertificateList issuers;
    trust_store_win->SyncGetIssuersOf(b_by_c_.get(), &issuers);
    ASSERT_EQ(2U, issuers.size());
    EXPECT_THAT(issuers, testing::UnorderedElementsAre(ParsedCertEq(c_by_d_),
                                                       ParsedCertEq(c_by_e_)));
  }
}

MATCHER_P(CertWithTrustEq, expected_cert_with_trust, "") {
  return arg.cert_bytes == expected_cert_with_trust.cert_bytes &&
         arg.trust.ToDebugString() ==
             expected_cert_with_trust.trust.ToDebugString();
}

TEST_F(TrustStoreWinTest, GetAllUserAddedCerts) {
  ASSERT_TRUE(AddToStore(stores_.roots.get(), d_by_d_));
  ASSERT_TRUE(
      AddToStoreWithEKURestriction(stores_.roots.get(), c_by_d_, nullptr));

  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), c_by_e_));
  ASSERT_TRUE(AddToStore(stores_.intermediates.get(), f_by_e_));

  ASSERT_TRUE(AddToStore(stores_.trusted_people.get(), b_by_c_));

  ASSERT_TRUE(AddToStore(stores_.disallowed.get(), b_by_f_));

  std::unique_ptr<TrustStoreWin> trust_store_win = CreateTrustStoreWin();

  std::vector<net::PlatformTrustStore::CertWithTrust> certs =
      trust_store_win->GetAllUserAddedCerts();
  ASSERT_EQ(5U, certs.size());
  EXPECT_THAT(certs, testing::UnorderedElementsAre(
                         CertWithTrustEq(net::PlatformTrustStore::CertWithTrust(
                             base::ToVector(d_by_d_->der_cert()),
                             bssl::CertificateTrust::ForTrustAnchorOrLeaf()
                                 .WithEnforceAnchorExpiry()
                                 .WithEnforceAnchorConstraints()
                                 .WithRequireLeafSelfSigned())),
                         CertWithTrustEq(net::PlatformTrustStore::CertWithTrust(
                             base::ToVector(c_by_e_->der_cert()),
                             bssl::CertificateTrust::ForUnspecified())),
                         CertWithTrustEq(net::PlatformTrustStore::CertWithTrust(
                             base::ToVector(f_by_e_->der_cert()),
                             bssl::CertificateTrust::ForUnspecified())),
                         CertWithTrustEq(net::PlatformTrustStore::CertWithTrust(
                             base::ToVector(b_by_c_->der_cert()),
                             bssl::CertificateTrust::ForTrustedLeaf()
                                 .WithRequireLeafSelfSigned())),
                         CertWithTrustEq(net::PlatformTrustStore::CertWithTrust(
                             base::ToVector(b_by_f_->der_cert()),
                             bssl::CertificateTrust::ForDistrusted()))));
}

// Verifies that TrustStoreWin automatically synchronizes with the Windows
// certificate stores when certificates are added or removed at the system
// level. This ensures dynamic updates without restarting the process.
//
// WARNING: This test modifies the OS trust store. If it fails to clean up
// after itself, your system may be left in an insecure state. To verify it
// cleaned up after itself, make sure certificates that begin with
// "Chromium Test Cert" are not trusted by Windows.
//
// This is a manual test and is intentionally excluded from automated
// builders and normal local runs. The test name is prefixed with "MANUAL_"
// so that it is skipped by default in net/test/net_test_suite.cc. It only
// runs when the --run-manual flag is provided.
//
// Example:
//   net_unittests --gtest_filter=TrustStoreWinTest.MANUAL_* --run-manual
//
// Run this after substantial changes to the Windows trust store integration
// to validate end-to-end synchronization behavior.
TEST_F(TrustStoreWinTest, MANUAL_AutoSyncCertStores) {
  // Create a test certificate using CertBuilder with a random private key
  // that gets discarded after the test, making it safer if cleanup fails.
  auto cert_builder = std::make_unique<net::CertBuilder>(nullptr, nullptr);
  cert_builder->SetSubjectCommonName(base::StrCat(
      {"Chromium Test Cert - ", net::CertBuilder::MakeRandomHexString(12)}));

  cert_builder->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);

  // Set validity period for the certificate
  base::Time not_before = base::Time::Now() - base::Days(1);
  base::Time not_after = base::Time::Now() + base::Days(30);
  cert_builder->SetValidity(not_before, not_after);

  // Parse the generated certificate for use with TrustStoreWin
  bssl::CertErrors errors;
  std::shared_ptr<const bssl::ParsedCertificate> test_cert =
      bssl::ParsedCertificate::Create(
          cert_builder->DupCertBuffer(),
          x509_util::DefaultParseCertificateOptions(), &errors);
  ASSERT_TRUE(test_cert) << "Failed to parse generated test certificate: "
                         << errors.ToDebugString();

  // Create a real TrustStoreWin that connects to actual Windows system stores
  auto trust_store_win = std::make_unique<TrustStoreWin>();
  trust_store_win->InitializeStores();

  // Open the same system ROOT store that TrustStoreWin uses internally.
  // We need a separate handle with write access to add/remove test
  // certificates, while TrustStoreWin uses read-only collection stores that
  // aggregate multiple Windows system store locations.
  DWORD flags = CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG;
  crypto::ScopedHCERTSTORE system_root_store(CertOpenStore(
      CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, NULL, flags, L"ROOT"));

  if (!system_root_store.get()) {
    EXPECT_NE(nullptr, system_root_store.get());
  }

  // Load the certificate.
  crypto::ScopedPCCERT_CONTEXT test_cert_context(CertCreateCertificateContext(
      X509_ASN_ENCODING, CRYPTO_BUFFER_data(test_cert->cert_buffer()),
      CRYPTO_BUFFER_len(test_cert->cert_buffer())));

  if (!test_cert_context.get()) {
    EXPECT_NE(nullptr, test_cert_context.get());
  }

  // Verify certificate is NOT trusted initially.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(test_cert.get()).ToDebugString())
      << "Certificate should not be trusted initially";

  // Add certificate to the actual Windows system ROOT store.
  BOOL add_result = CertAddCertificateContextToStore(
      system_root_store.get(), test_cert_context.get(), CERT_STORE_ADD_NEW,
      nullptr);

  if (!add_result) {
    GTEST_SKIP() << "Could not add certificate to system store, error: "
                 << GetLastError();
  }

  // Test auto-sync: Certificate should be immediately trusted without restart.
  EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
            trust_store_win->GetTrust(test_cert.get()).ToDebugString())
      << "Auto-sync should allow immediate detection of newly added "
         "certificate";

  // Cleanup: Remove the test certificate.
  crypto::ScopedPCCERT_CONTEXT cert_to_delete(CertFindCertificateInStore(
      system_root_store.get(), X509_ASN_ENCODING, 0, CERT_FIND_EXISTING,
      test_cert_context.get(), nullptr));

  if (cert_to_delete.get()) {
    CertDeleteCertificateFromStore(cert_to_delete.release());
  }

  // Verify cleanup: Certificate should no longer be trusted.
  EXPECT_EQ(bssl::CertificateTrust::ForUnspecified().ToDebugString(),
            trust_store_win->GetTrust(test_cert.get()).ToDebugString())
      << "Certificate should not be trusted after removal";
}

}  // namespace
}  // namespace net
