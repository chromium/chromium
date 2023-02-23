// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_nss.h"

#include <cert.h>
#include <certdb.h>
#include <secmod.h>

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/cert/known_roots_nss.h"
#include "net/cert/pki/cert_issuer_source_sync_unittest.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/test_helpers.h"
#include "net/cert/pki/trust_store.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

// Returns true if the provided slot looks like a built-in root.
bool IsBuiltInRootSlot(PK11SlotInfo* slot) {
  if (!PK11_IsPresent(slot) || !PK11_HasRootCerts(slot))
    return false;
  crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot));
  if (!cert_list)
    return false;
  bool built_in_cert_found = false;
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    if (IsKnownRoot(node->cert)) {
      built_in_cert_found = true;
      break;
    }
  }
  return built_in_cert_found;
}

// Returns the slot which holds the built-in root certificates.
crypto::ScopedPK11Slot GetBuiltInRootCertsSlot() {
  crypto::AutoSECMODListReadLock auto_lock;
  SECMODModuleList* head = SECMOD_GetDefaultModuleList();
  for (SECMODModuleList* item = head; item != nullptr; item = item->next) {
    int slot_count = item->module->loaded ? item->module->slotCount : 0;
    for (int i = 0; i < slot_count; i++) {
      PK11SlotInfo* slot = item->module->slots[i];
      if (IsBuiltInRootSlot(slot))
        return crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot));
    }
  }
  return crypto::ScopedPK11Slot();
}

// Returns a built-in trusted root certificte. If multiple ones are available,
// it is not specified which one is returned. If none are available, returns
// nullptr.
std::shared_ptr<const ParsedCertificate> GetASSLTrustedBuiltinRoot() {
  crypto::ScopedPK11Slot root_certs_slot = GetBuiltInRootCertsSlot();
  if (!root_certs_slot)
    return nullptr;

  scoped_refptr<X509Certificate> ssl_trusted_root;

  crypto::ScopedCERTCertList cert_list(
      PK11_ListCertsInSlot(root_certs_slot.get()));
  if (!cert_list)
    return nullptr;
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    CERTCertTrust trust;
    if (CERT_GetCertTrust(node->cert, &trust) != SECSuccess)
      continue;
    int trust_flags = SEC_GET_TRUST_FLAGS(&trust, trustSSL);
    if ((trust_flags & CERTDB_TRUSTED_CA) == CERTDB_TRUSTED_CA) {
      ssl_trusted_root =
          x509_util::CreateX509CertificateFromCERTCertificate(node->cert);
      break;
    }
  }
  if (!ssl_trusted_root)
    return nullptr;

  CertErrors parsing_errors;
  return ParsedCertificate::Create(bssl::UpRef(ssl_trusted_root->cert_buffer()),
                                   x509_util::DefaultParseCertificateOptions(),
                                   &parsing_errors);
}

class TrustStoreNSSTestBase : public ::testing::Test {
 public:
  explicit TrustStoreNSSTestBase(bool trusted_leaf_support,
                                 bool enforce_local_anchor_constraints)
      : trusted_leaf_support_(trusted_leaf_support),
        enforce_local_anchor_constraints_(enforce_local_anchor_constraints),
        scoped_enforce_local_anchor_constraints_(
            enforce_local_anchor_constraints) {
    if (trusted_leaf_support) {
      feature_list_.InitAndEnableFeature(
          features::kTrustStoreTrustedLeafSupport);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kTrustStoreTrustedLeafSupport);
    }
  }

  TrustStoreNSSTestBase() : TrustStoreNSSTestBase(true, true) {}

  bool ExpectedTrustedLeafSupportEnabled() const {
    return trusted_leaf_support_;
  }

  bool ExpectedEnforceLocalAnchorConstraintsEnabled() const {
    return enforce_local_anchor_constraints_;
  }

  CertificateTrust ExpectedTrustForBuiltinAnchor() const {
    return CertificateTrust::ForTrustAnchor();
  }

  CertificateTrust ExpectedTrustForAnchor() const {
    CertificateTrust trust = CertificateTrust::ForTrustAnchor();
    if (ExpectedEnforceLocalAnchorConstraintsEnabled()) {
      trust = trust.WithEnforceAnchorConstraints().WithEnforceAnchorExpiry();
    }
    return trust;
  }

  CertificateTrust ExpectedTrustForAnchorOrLeaf() const {
    CertificateTrust trust;
    if (ExpectedTrustedLeafSupportEnabled()) {
      trust = CertificateTrust::ForTrustAnchorOrLeaf();
    } else {
      trust = CertificateTrust::ForTrustAnchor();
    }
    if (ExpectedEnforceLocalAnchorConstraintsEnabled()) {
      trust = trust.WithEnforceAnchorConstraints().WithEnforceAnchorExpiry();
    }
    return trust;
  }

  CertificateTrust ExpectedTrustForLeaf() const {
    if (ExpectedTrustedLeafSupportEnabled()) {
      return CertificateTrust::ForTrustedLeaf();
    } else {
      return CertificateTrust::ForUnspecified();
    }
  }

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());
    ASSERT_TRUE(other_test_nssdb_.is_open());
    ParsedCertificateList chain;
    ReadCertChainFromFile(
        "net/data/verify_certificate_chain_unittest/key-rollover/oldchain.pem",
        &chain);

    ASSERT_EQ(3U, chain.size());
    target_ = chain[0];
    oldintermediate_ = chain[1];
    oldroot_ = chain[2];
    ASSERT_TRUE(target_);
    ASSERT_TRUE(oldintermediate_);
    ASSERT_TRUE(oldroot_);

    ReadCertChainFromFile(
        "net/data/verify_certificate_chain_unittest/"
        "key-rollover/longrolloverchain.pem",
        &chain);

    ASSERT_EQ(5U, chain.size());
    newintermediate_ = chain[1];
    newroot_ = chain[2];
    newrootrollover_ = chain[3];
    ASSERT_TRUE(newintermediate_);
    ASSERT_TRUE(newroot_);
    ASSERT_TRUE(newrootrollover_);

    trust_store_nss_ = CreateTrustStoreNSS();
  }

  // Creates the TrustStoreNSS instance. Subclasses will customize the slot
  // filtering behavior here.
  virtual std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() = 0;

  std::string GetUniqueNickname() {
    return "trust_store_nss_unittest" +
           base::NumberToString(nickname_counter_++);
  }

  void AddCertToNSSSlot(const ParsedCertificate* cert, PK11SlotInfo* slot) {
    ScopedCERTCertificate nss_cert(x509_util::CreateCERTCertificateFromBytes(
        cert->der_cert().UnsafeData(), cert->der_cert().Length()));
    ASSERT_TRUE(nss_cert);
    SECStatus srv = PK11_ImportCert(slot, nss_cert.get(), CK_INVALID_HANDLE,
                                    GetUniqueNickname().c_str(),
                                    PR_FALSE /* includeTrust (unused) */);
    ASSERT_EQ(SECSuccess, srv);
  }

  void AddCertsToNSS() {
    AddCertToNSSSlot(target_.get(), test_nssdb_.slot());
    AddCertToNSSSlot(oldintermediate_.get(), test_nssdb_.slot());
    AddCertToNSSSlot(newintermediate_.get(), test_nssdb_.slot());
    AddCertToNSSSlot(oldroot_.get(), test_nssdb_.slot());
    AddCertToNSSSlot(newroot_.get(), test_nssdb_.slot());
    AddCertToNSSSlot(newrootrollover_.get(), test_nssdb_.slot());

    // Check that the certificates can be retrieved as expected.
    EXPECT_TRUE(
        TrustStoreContains(target_, {newintermediate_, oldintermediate_}));

    EXPECT_TRUE(TrustStoreContains(newintermediate_,
                                   {newroot_, newrootrollover_, oldroot_}));
    EXPECT_TRUE(TrustStoreContains(oldintermediate_,
                                   {newroot_, newrootrollover_, oldroot_}));
    EXPECT_TRUE(TrustStoreContains(newrootrollover_,
                                   {newroot_, newrootrollover_, oldroot_}));
    EXPECT_TRUE(
        TrustStoreContains(oldroot_, {newroot_, newrootrollover_, oldroot_}));
    EXPECT_TRUE(
        TrustStoreContains(newroot_, {newroot_, newrootrollover_, oldroot_}));
  }

  // Trusts |cert|. Assumes the cert was already imported into NSS.
  void TrustCert(const ParsedCertificate* cert) {
    ChangeCertTrust(cert, CERTDB_TRUSTED_CA | CERTDB_VALID_CA);
  }

  // Trusts |cert| as a server, but not as a CA. Assumes the cert was already
  // imported into NSS.
  void TrustServerCert(const ParsedCertificate* cert) {
    ChangeCertTrust(cert, CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED);
  }

  // Trusts |cert| as both a server and as a CA. Assumes the cert was already
  // imported into NSS.
  void TrustCaAndServerCert(const ParsedCertificate* cert) {
    ChangeCertTrust(cert, CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED |
                              CERTDB_TRUSTED_CA | CERTDB_VALID_CA);
  }

  // Distrusts |cert|. Assumes the cert was already imported into NSS.
  void DistrustCert(const ParsedCertificate* cert) {
    ChangeCertTrust(cert, CERTDB_TERMINAL_RECORD);
  }

  void ChangeCertTrust(const ParsedCertificate* cert, int flags) {
    SECItem der_cert;
    der_cert.data = const_cast<uint8_t*>(cert->der_cert().UnsafeData());
    der_cert.len = base::checked_cast<unsigned>(cert->der_cert().Length());
    der_cert.type = siDERCertBuffer;

    ScopedCERTCertificate nss_cert(
        CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));
    ASSERT_TRUE(nss_cert);

    CERTCertTrust trust = {0};
    trust.sslFlags = flags;
    SECStatus srv =
        CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), nss_cert.get(), &trust);
    ASSERT_EQ(SECSuccess, srv);
  }

 protected:
  bool TrustStoreContains(std::shared_ptr<const ParsedCertificate> cert,
                          ParsedCertificateList expected_matches) {
    ParsedCertificateList matches;
    trust_store_nss_->SyncGetIssuersOf(cert.get(), &matches);

    std::vector<std::string> name_result_matches;
    for (const auto& it : matches)
      name_result_matches.push_back(GetCertString(it));
    std::sort(name_result_matches.begin(), name_result_matches.end());

    std::vector<std::string> name_expected_matches;
    for (const auto& it : expected_matches)
      name_expected_matches.push_back(GetCertString(it));
    std::sort(name_expected_matches.begin(), name_expected_matches.end());

    if (name_expected_matches == name_result_matches)
      return true;

    // Print some extra information for debugging.
    EXPECT_EQ(name_expected_matches, name_result_matches);
    return false;
  }

  // Give simpler names to certificate DER (for identifying them in tests by
  // their symbolic name).
  std::string GetCertString(
      const std::shared_ptr<const ParsedCertificate>& cert) const {
    if (cert->der_cert() == oldroot_->der_cert())
      return "oldroot_";
    if (cert->der_cert() == newroot_->der_cert())
      return "newroot_";
    if (cert->der_cert() == target_->der_cert())
      return "target_";
    if (cert->der_cert() == oldintermediate_->der_cert())
      return "oldintermediate_";
    if (cert->der_cert() == newintermediate_->der_cert())
      return "newintermediate_";
    if (cert->der_cert() == newrootrollover_->der_cert())
      return "newrootrollover_";
    return cert->der_cert().AsString();
  }

  bool HasTrust(const ParsedCertificateList& certs,
                CertificateTrust expected_trust) {
    bool success = true;
    for (const std::shared_ptr<const ParsedCertificate>& cert : certs) {
      CertificateTrust trust =
          trust_store_nss_->GetTrust(cert.get(), /*debug_data=*/nullptr);
      std::string trust_string = trust.ToDebugString();
      std::string expected_trust_string = expected_trust.ToDebugString();
      if (trust_string != expected_trust_string) {
        EXPECT_EQ(expected_trust_string, trust_string) << GetCertString(cert);
        success = false;
      }
    }

    return success;
  }

  base::test::ScopedFeatureList feature_list_;
  const bool trusted_leaf_support_;
  const bool enforce_local_anchor_constraints_;
  ScopedLocalAnchorConstraintsEnforcementForTesting
      scoped_enforce_local_anchor_constraints_;

  std::shared_ptr<const ParsedCertificate> oldroot_;
  std::shared_ptr<const ParsedCertificate> newroot_;

  std::shared_ptr<const ParsedCertificate> target_;
  std::shared_ptr<const ParsedCertificate> oldintermediate_;
  std::shared_ptr<const ParsedCertificate> newintermediate_;
  std::shared_ptr<const ParsedCertificate> newrootrollover_;
  crypto::ScopedTestNSSDB test_nssdb_;
  crypto::ScopedTestNSSDB other_test_nssdb_;
  std::unique_ptr<TrustStoreNSS> trust_store_nss_;
  unsigned nickname_counter_ = 0;
};

// Specifies which kind of per-slot filtering the TrustStoreNSS is supposed to
// perform in the parametrized TrustStoreNSSTestWithSlotFilterType.
enum class SlotFilterType {
  kDontFilter,
  kDoNotAllowUserSlots,
  kAllowSpecifiedUserSlot
};

// Used for testing a TrustStoreNSS with the slot filter type specified by the
// test parameter.
class TrustStoreNSSTestWithSlotFilterType
    : public TrustStoreNSSTestBase,
      public testing::WithParamInterface<SlotFilterType> {
 public:
  TrustStoreNSSTestWithSlotFilterType() = default;
  ~TrustStoreNSSTestWithSlotFilterType() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    switch (GetParam()) {
      case SlotFilterType::kDontFilter:
        return std::make_unique<TrustStoreNSS>(
            trustSSL, TrustStoreNSS::kUseSystemTrust,
            TrustStoreNSS::UseTrustFromAllUserSlots());
      case SlotFilterType::kDoNotAllowUserSlots:
        return std::make_unique<TrustStoreNSS>(
            trustSSL, TrustStoreNSS::kUseSystemTrust,
            /*user_slot_trust_setting=*/nullptr);
      case SlotFilterType::kAllowSpecifiedUserSlot:
        return std::make_unique<TrustStoreNSS>(
            trustSSL, TrustStoreNSS::kUseSystemTrust,
            crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
    }
  }
};

// Without adding any certs to the NSS DB, should get no anchor results for
// any of the test certs.
TEST_P(TrustStoreNSSTestWithSlotFilterType, CertsNotPresent) {
  EXPECT_TRUE(TrustStoreContains(target_, ParsedCertificateList()));
  EXPECT_TRUE(TrustStoreContains(newintermediate_, ParsedCertificateList()));
  EXPECT_TRUE(TrustStoreContains(newroot_, ParsedCertificateList()));
}

// TrustStoreNSS should return temporary certs on Chrome OS, because on Chrome
// OS temporary certs are used to supply policy-provided untrusted authority
// certs. (See https://crbug.com/978854)
// On other platforms it's not required but doesn't hurt anything.
TEST_P(TrustStoreNSSTestWithSlotFilterType, TempCertPresent) {
  ScopedCERTCertificate temp_nss_cert(x509_util::CreateCERTCertificateFromBytes(
      newintermediate_->der_cert().UnsafeData(),
      newintermediate_->der_cert().Length()));
  EXPECT_TRUE(TrustStoreContains(target_, {newintermediate_}));
}

// Independent of the specified slot-based filtering mode, built-in root certs
// should always be trusted.
TEST_P(TrustStoreNSSTestWithSlotFilterType, TrustAllowedForBuiltinRootCerts) {
  auto builtin_root_cert = GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(builtin_root_cert);
  EXPECT_TRUE(HasTrust({builtin_root_cert}, ExpectedTrustForBuiltinAnchor()));
}

INSTANTIATE_TEST_SUITE_P(
    TrustStoreNSSTest_AllSlotFilterTypes,
    TrustStoreNSSTestWithSlotFilterType,
    ::testing::Values(SlotFilterType::kDontFilter,
                      SlotFilterType::kDoNotAllowUserSlots,
                      SlotFilterType::kAllowSpecifiedUserSlot));

// Tests a TrustStoreNSS that ignores system root certs.
class TrustStoreNSSTestIgnoreSystemCerts
    : public TrustStoreNSSTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  TrustStoreNSSTestIgnoreSystemCerts()
      : TrustStoreNSSTestBase(std::get<0>(GetParam()),
                              std::get<1>(GetParam())) {}
  ~TrustStoreNSSTestIgnoreSystemCerts() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        trustSSL, TrustStoreNSS::kIgnoreSystemTrust,
        TrustStoreNSS::UseTrustFromAllUserSlots());
  }
};

TEST_P(TrustStoreNSSTestIgnoreSystemCerts, UserRootTrusted) {
  AddCertsToNSS();
  TrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

TEST_P(TrustStoreNSSTestIgnoreSystemCerts, UserRootDistrusted) {
  AddCertsToNSS();
  DistrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForDistrusted()));
}

TEST_P(TrustStoreNSSTestIgnoreSystemCerts, SystemRootCertsIgnored) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);
  EXPECT_TRUE(HasTrust({system_root}, CertificateTrust::ForUnspecified()));
}

TEST_P(TrustStoreNSSTestIgnoreSystemCerts, UserTrustedServer) {
  AddCertsToNSS();
  TrustServerCert(target_.get());
  EXPECT_TRUE(HasTrust({target_}, ExpectedTrustForLeaf()));
}

TEST_P(TrustStoreNSSTestIgnoreSystemCerts, UserTrustedCaAndServer) {
  AddCertsToNSS();
  TrustCaAndServerCert(target_.get());
  EXPECT_TRUE(HasTrust({target_}, ExpectedTrustForAnchorOrLeaf()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TrustStoreNSSTestIgnoreSystemCerts,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<
        TrustStoreNSSTestIgnoreSystemCerts::ParamType>& info) {
      return std::string(std::get<0>(info.param) ? "TrustedLeafSupported"
                                                 : "TrustAnchorOnly") +
             (std::get<1>(info.param) ? "EnforceLocalAnchorConstraints"
                                      : "NoLocalAnchorConstraints");
    });

// Tests a TrustStoreNSS that does not filter which certificates
class TrustStoreNSSTestWithoutSlotFilter
    : public TrustStoreNSSTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  TrustStoreNSSTestWithoutSlotFilter()
      : TrustStoreNSSTestBase(std::get<0>(GetParam()),
                              std::get<1>(GetParam())) {}

  ~TrustStoreNSSTestWithoutSlotFilter() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        trustSSL, TrustStoreNSS::kUseSystemTrust,
        TrustStoreNSS::UseTrustFromAllUserSlots());
  }
};

// If certs are present in NSS DB but aren't marked as trusted, should get no
// anchor results for any of the test certs.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, CertsPresentButNotTrusted) {
  AddCertsToNSS();

  // None of the certificates are trusted.
  EXPECT_TRUE(HasTrust({oldroot_, newroot_, target_, oldintermediate_,
                        newintermediate_, newrootrollover_},
                       CertificateTrust::ForUnspecified()));
}

// Trust a single self-signed CA certificate.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, TrustedCA) {
  AddCertsToNSS();
  TrustCert(newroot_.get());

  // Only one of the certificates are trusted.
  EXPECT_TRUE(HasTrust(
      {oldroot_, target_, oldintermediate_, newintermediate_, newrootrollover_},
      CertificateTrust::ForUnspecified()));

  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

// Distrust a single self-signed CA certificate.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, DistrustedCA) {
  AddCertsToNSS();
  DistrustCert(newroot_.get());

  // Only one of the certificates are trusted.
  EXPECT_TRUE(HasTrust(
      {oldroot_, target_, oldintermediate_, newintermediate_, newrootrollover_},
      CertificateTrust::ForUnspecified()));

  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForDistrusted()));
}

// Trust a single intermediate certificate.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, TrustedIntermediate) {
  AddCertsToNSS();
  TrustCert(newintermediate_.get());

  EXPECT_TRUE(HasTrust(
      {oldroot_, newroot_, target_, oldintermediate_, newrootrollover_},
      CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({newintermediate_}, ExpectedTrustForAnchor()));
}

// Distrust a single intermediate certificate.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, DistrustedIntermediate) {
  AddCertsToNSS();
  DistrustCert(newintermediate_.get());

  EXPECT_TRUE(HasTrust(
      {oldroot_, newroot_, target_, oldintermediate_, newrootrollover_},
      CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({newintermediate_}, CertificateTrust::ForDistrusted()));
}

// Trust a single server certificate.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, TrustedServer) {
  AddCertsToNSS();
  TrustServerCert(target_.get());

  EXPECT_TRUE(HasTrust({oldroot_, newroot_, oldintermediate_, newintermediate_,
                        newrootrollover_},
                       CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({target_}, ExpectedTrustForLeaf()));
}

// Trust a single certificate with both CA and server trust bits.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, TrustedCaAndServer) {
  AddCertsToNSS();
  TrustCaAndServerCert(target_.get());

  EXPECT_TRUE(HasTrust({oldroot_, newroot_, oldintermediate_, newintermediate_,
                        newrootrollover_},
                       CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({target_}, ExpectedTrustForAnchorOrLeaf()));
}

// Trust multiple self-signed CA certificates with the same name.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, MultipleTrustedCAWithSameSubject) {
  AddCertsToNSS();
  TrustCert(oldroot_.get());
  TrustCert(newroot_.get());

  EXPECT_TRUE(
      HasTrust({target_, oldintermediate_, newintermediate_, newrootrollover_},
               CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({oldroot_, newroot_}, ExpectedTrustForAnchor()));
}

// Different trust settings for multiple self-signed CA certificates with the
// same name.
TEST_P(TrustStoreNSSTestWithoutSlotFilter, DifferingTrustCAWithSameSubject) {
  AddCertsToNSS();
  DistrustCert(oldroot_.get());
  TrustCert(newroot_.get());

  EXPECT_TRUE(
      HasTrust({target_, oldintermediate_, newintermediate_, newrootrollover_},
               CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({oldroot_}, CertificateTrust::ForDistrusted()));
  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TrustStoreNSSTestWithoutSlotFilter,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<
        TrustStoreNSSTestWithoutSlotFilter::ParamType>& info) {
      return std::string(std::get<0>(info.param) ? "TrustedLeafSupported"
                                                 : "TrustAnchorOnly") +
             (std::get<1>(info.param) ? "EnforceLocalAnchorConstraints"
                                      : "NoLocalAnchorConstraints");
    });

// Tests for a TrustStoreNSS which does not allow certificates on user slots
// to be trusted.
class TrustStoreNSSTestDoNotAllowUserSlots : public TrustStoreNSSTestBase {
 public:
  TrustStoreNSSTestDoNotAllowUserSlots() = default;
  ~TrustStoreNSSTestDoNotAllowUserSlots() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(trustSSL,
                                           TrustStoreNSS::kUseSystemTrust,
                                           /*user_slot_trust_setting=*/nullptr);
  }
};

// A certificate that is stored on a "user slot" is not trusted if the
// TrustStoreNSS is not allowed to trust certificates on user slots.
TEST_F(TrustStoreNSSTestDoNotAllowUserSlots, CertOnUserSlot) {
  AddCertToNSSSlot(newroot_.get(), test_nssdb_.slot());
  TrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));
}

// Tests for a TrustStoreNSS which does allows certificates on user slots to
// be only trusted if they are on a specific user slot.
class TrustStoreNSSTestAllowSpecifiedUserSlot : public TrustStoreNSSTestBase {
 public:
  TrustStoreNSSTestAllowSpecifiedUserSlot() = default;
  ~TrustStoreNSSTestAllowSpecifiedUserSlot() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        trustSSL, TrustStoreNSS::kUseSystemTrust,
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
  }
};

// A certificate that is stored on a "user slot" is trusted if the
// TrustStoreNSS is allowed to trust that user slot.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnUserSlot) {
  AddCertToNSSSlot(newroot_.get(), test_nssdb_.slot());
  TrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

// A certificate that is stored on a "user slot" is not trusted if the
// TrustStoreNSS is allowed to trust a user slot, but the certificate is
// stored on another user slot.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnOtherUserSlot) {
  AddCertToNSSSlot(newroot_.get(), other_test_nssdb_.slot());
  TrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));
}

// TODO(https://crbug.com/980443): If the internal non-removable slot is
// relevant on Chrome OS, add a test for allowing trust for certificates
// stored on that slot.

class TrustStoreNSSTestDelegate {
 public:
  TrustStoreNSSTestDelegate()
      : trust_store_nss_(trustSSL,
                         TrustStoreNSS::kUseSystemTrust,
                         TrustStoreNSS::UseTrustFromAllUserSlots()) {}

  void AddCert(std::shared_ptr<const ParsedCertificate> cert) {
    ASSERT_TRUE(test_nssdb_.is_open());
    ScopedCERTCertificate nss_cert(x509_util::CreateCERTCertificateFromBytes(
        cert->der_cert().UnsafeData(), cert->der_cert().Length()));
    ASSERT_TRUE(nss_cert);
    SECStatus srv = PK11_ImportCert(
        test_nssdb_.slot(), nss_cert.get(), CK_INVALID_HANDLE,
        GetUniqueNickname().c_str(), PR_FALSE /* includeTrust (unused) */);
    ASSERT_EQ(SECSuccess, srv);
  }

  CertIssuerSource& source() { return trust_store_nss_; }

 protected:
  std::string GetUniqueNickname() {
    return "cert_issuer_source_nss_unittest" +
           base::NumberToString(nickname_counter_++);
  }

  crypto::ScopedTestNSSDB test_nssdb_;
  TrustStoreNSS trust_store_nss_;
  unsigned int nickname_counter_ = 0;
};

INSTANTIATE_TYPED_TEST_SUITE_P(TrustStoreNSSTest2,
                               CertIssuerSourceSyncTest,
                               TrustStoreNSSTestDelegate);

// NSS doesn't normalize UTF8String values, so use the not-normalized version
// of those tests.
INSTANTIATE_TYPED_TEST_SUITE_P(TrustStoreNSSNotNormalizedTest,
                               CertIssuerSourceSyncNotNormalizedTest,
                               TrustStoreNSSTestDelegate);

}  // namespace

}  // namespace net
