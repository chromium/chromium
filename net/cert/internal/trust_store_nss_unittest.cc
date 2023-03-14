// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_nss.h"

#include <cert.h>
#include <certdb.h>
#include <pkcs11n.h>
#include <prtypes.h>
#include <secmod.h>

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
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
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

std::string SystemTrustSettingToString(
    TrustStoreNSS::SystemTrustSetting system_trust_setting) {
  switch (system_trust_setting) {
    case TrustStoreNSS::SystemTrustSetting::kUseSystemTrust:
      return "UseSystemTrust";
    case TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust:
      return "IgnoreSystemTrust";
  };
}

unsigned TrustTypeToNSSTrust(CertificateTrustType trust) {
  switch (trust) {
    case CertificateTrustType::DISTRUSTED:
      return CERTDB_TERMINAL_RECORD;
    case CertificateTrustType::UNSPECIFIED:
      return 0;
    case CertificateTrustType::TRUSTED_ANCHOR:
      return CERTDB_TRUSTED_CA | CERTDB_VALID_CA;
    case CertificateTrustType::TRUSTED_LEAF:
      return CERTDB_TRUSTED | CERTDB_TERMINAL_RECORD;
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      return CERTDB_TRUSTED_CA | CERTDB_VALID_CA | CERTDB_TRUSTED |
             CERTDB_TERMINAL_RECORD;
  }
}

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

absl::optional<unsigned> GetNSSTrustForCert(const ParsedCertificate* cert) {
  SECItem der_cert;
  der_cert.data = const_cast<uint8_t*>(cert->der_cert().UnsafeData());
  der_cert.len = base::checked_cast<unsigned>(cert->der_cert().Length());
  der_cert.type = siDERCertBuffer;
  ScopedCERTCertificate nss_cert(
      CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));
  if (!nss_cert) {
    return absl::nullopt;
  }

  CERTCertTrust nss_cert_trust;
  if (CERT_GetCertTrust(nss_cert.get(), &nss_cert_trust) != SECSuccess) {
    return absl::nullopt;
  }

  return SEC_GET_TRUST_FLAGS(&nss_cert_trust, trustSSL);
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
    ASSERT_TRUE(first_test_nssdb_.is_open());
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

  // Import `cert` into `slot` and create a trust record with `trust` type.
  // Tries to ensure that the created trust record ends up in the same `slot`.
  // (That isn't always the case if `cert` exists in multiple slots and
  // CERT_ChangeCertTrust was just used on an arbitrary CERTCertificate handle
  // for `cert`.)
  void AddCertToNSSSlotWithTrust(const ParsedCertificate* cert,
                                 PK11SlotInfo* slot,
                                 CertificateTrustType trust) {
    AddCertToNSSSlot(cert, slot);
    ChangeCertTrustInSlot(cert, slot, trust);
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

  // Change the trust for `cert` in `slot` to `trust`.
  // `cert` must already exist in `slot'.
  // Tries to ensure that the created trust record ends up in the same `slot`.
  // (That isn't always the case if `cert` exists in multiple slots and
  // CERT_ChangeCertTrust was just used on an arbitrary CERTCertificate handle
  // for `cert`.)
  // (An alternative approach would be to create the CKO_NSS_TRUST object
  // directly using PK11_CreateManagedGenericObject, which has the advantage of
  // being able to specify the slot directly, but the disadvantage that there's
  // no guarantee the way the test creates the trust object matches what NSS
  // actually does. See
  // https://crrev.com/c/3732801/9/net/cert/internal/trust_store_nss_unittest.cc#412
  // for some example code if that's ever needed.)
  void ChangeCertTrustInSlot(const ParsedCertificate* cert,
                             PK11SlotInfo* slot,
                             CertificateTrustType trust) {
    crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot));
    ASSERT_TRUE(cert_list);

    for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
         !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
      if (x509_util::IsSameCertificate(node->cert, cert->cert_buffer())) {
        CERTCertTrust nss_trust = {0};
        nss_trust.sslFlags = TrustTypeToNSSTrust(trust);
        if (CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), node->cert,
                                 &nss_trust) != SECSuccess) {
          ADD_FAILURE() << "CERT_ChangeCertTrust failed: " << PORT_GetError();
        }
        return;
      }
    }
    ADD_FAILURE() << "cert not found in slot";
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
  crypto::ScopedTestNSSDB first_test_nssdb_;
  crypto::ScopedTestNSSDB test_nssdb_;
  crypto::ScopedTestNSSDB other_test_nssdb_;
  std::unique_ptr<TrustStoreNSS> trust_store_nss_;
  unsigned nickname_counter_ = 0;
};

// Specifies which kind of per-slot filtering the TrustStoreNSS is supposed to
// perform in the parametrized TrustStoreNSSTestWithSlotFilterType.
// TODO(https://crbug.com/1412591): The SlotFilterType enum is shared with
// TrustStoreNSS::ResultDebugData::SlotFilterType for convenience. Once the old
// code path and trial code is cleaned up, the type definition can be moved
// back to here.
using SlotFilterType = TrustStoreNSS::ResultDebugData::SlotFilterType;

std::string SlotFilterTypeToString(SlotFilterType slot_filter_type) {
  switch (slot_filter_type) {
    case SlotFilterType::kDontFilter:
      return "DontFilter";
    case SlotFilterType::kDoNotAllowUserSlots:
      return "DoNotAllowUserSlots";
    case SlotFilterType::kAllowSpecifiedUserSlot:
      return "AllowSpecifiedUserSlot";
  }
}

// Used for testing a TrustStoreNSS with the slot filter type specified by the
// test parameter. These tests are cases that are expected to be the same
// regardless of the slot filter type.
class TrustStoreNSSTestWithSlotFilterType
    : public TrustStoreNSSTestBase,
      public testing::WithParamInterface<
          std::tuple<TrustStoreNSS::SystemTrustSetting, SlotFilterType>> {
 public:
  TrustStoreNSSTestWithSlotFilterType() = default;
  ~TrustStoreNSSTestWithSlotFilterType() override = default;

  TrustStoreNSS::SystemTrustSetting system_trust_setting() const {
    return std::get<0>(GetParam());
  }
  SlotFilterType slot_filter_type() const { return std::get<1>(GetParam()); }

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    switch (slot_filter_type()) {
      case SlotFilterType::kDontFilter:
        return std::make_unique<TrustStoreNSS>(
            system_trust_setting(), TrustStoreNSS::UseTrustFromAllUserSlots());
      case SlotFilterType::kDoNotAllowUserSlots:
        return std::make_unique<TrustStoreNSS>(
            system_trust_setting(),
            /*user_slot_trust_setting=*/nullptr);
      case SlotFilterType::kAllowSpecifiedUserSlot:
        return std::make_unique<TrustStoreNSS>(
            system_trust_setting(),
            crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
    }
  }
};

class DebugData : public base::SupportsUserData {
 public:
  ~DebugData() override = default;
};

TEST_P(TrustStoreNSSTestWithSlotFilterType, DebugData) {
  DebugData debug_data;
  trust_store_nss_->GetTrust(target_.get(), &debug_data);
  const TrustStoreNSS::ResultDebugData* trust_debug_data =
      TrustStoreNSS::ResultDebugData::Get(&debug_data);
  ASSERT_TRUE(trust_debug_data);
  EXPECT_EQ(system_trust_setting() ==
                TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust,
            trust_debug_data->ignore_system_trust_settings());
  EXPECT_EQ(slot_filter_type(), trust_debug_data->slot_filter_type());
}

// Without adding any certs to the NSS DB, should get no anchor results for
// any of the test certs.
TEST_P(TrustStoreNSSTestWithSlotFilterType, CertsNotPresent) {
  EXPECT_TRUE(TrustStoreContains(target_, ParsedCertificateList()));
  EXPECT_TRUE(TrustStoreContains(newintermediate_, ParsedCertificateList()));
  EXPECT_TRUE(TrustStoreContains(newroot_, ParsedCertificateList()));
  EXPECT_TRUE(HasTrust({target_}, CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({newintermediate_}, CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));
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
  EXPECT_TRUE(HasTrust({target_}, CertificateTrust::ForUnspecified()));
}

// Independent of the specified slot-based filtering mode, built-in root certs
// should always be trusted if kUseSystemTrust, otherwise they always should
// not.
TEST_P(TrustStoreNSSTestWithSlotFilterType, TrustAllowedForBuiltinRootCerts) {
  auto builtin_root_cert = GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(builtin_root_cert);
  switch (system_trust_setting()) {
    case TrustStoreNSS::SystemTrustSetting::kUseSystemTrust:
      EXPECT_TRUE(
          HasTrust({builtin_root_cert}, ExpectedTrustForBuiltinAnchor()));
      break;
    case TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust:
      EXPECT_TRUE(
          HasTrust({builtin_root_cert}, CertificateTrust::ForUnspecified()));
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TrustStoreNSSTestWithSlotFilterType,
    ::testing::Combine(
        ::testing::Values(
            TrustStoreNSS::SystemTrustSetting::kUseSystemTrust,
            TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust),
        ::testing::Values(SlotFilterType::kDontFilter,
                          SlotFilterType::kDoNotAllowUserSlots,
                          SlotFilterType::kAllowSpecifiedUserSlot)),
    [](const testing::TestParamInfo<
        TrustStoreNSSTestWithSlotFilterType::ParamType>& info) {
      return base::StrCat({SystemTrustSettingToString(std::get<0>(info.param)),
                           SlotFilterTypeToString(std::get<1>(info.param))});
    });

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
        TrustStoreNSS::kIgnoreSystemTrust,
        TrustStoreNSS::UseTrustFromAllUserSlots());
  }
};

TEST_P(TrustStoreNSSTestIgnoreSystemCerts, UnknownCertIgnored) {
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));
}

// An NSS CERTCertificate object exists for the cert, but it is not
// imported into any DB. Should be unspecified trust.
TEST_P(TrustStoreNSSTestIgnoreSystemCerts, TemporaryCertIgnored) {
  ScopedCERTCertificate nss_cert(x509_util::CreateCERTCertificateFromBytes(
      newroot_->der_cert().UnsafeData(), newroot_->der_cert().Length()));
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));
}

// Cert is added to user DB, but without explicitly calling
// CERT_ChangeCertTrust. Should be unspecified trust.
TEST_P(TrustStoreNSSTestIgnoreSystemCerts, UserCertWithNoTrust) {
  AddCertsToNSS();
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));
}

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

TEST_P(TrustStoreNSSTestIgnoreSystemCerts, SystemRootCertIgnored) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);
  EXPECT_TRUE(HasTrust({system_root}, CertificateTrust::ForUnspecified()));
}

// A system trusted root is also present in a user DB, but without any trust
// settings in the user DB. The system trust settings should not be used.
TEST_P(TrustStoreNSSTestIgnoreSystemCerts,
       SystemRootCertIgnoredWhenPresentInUserDb) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlot(system_root.get(), test_nssdb_.slot());

  // TrustStoreNSS should see an Unspecified since we are ignoring the system
  // slot.
  EXPECT_TRUE(HasTrust({system_root}, CertificateTrust::ForUnspecified()));
}

// A system trusted root is also present in a user DB, with TRUSTED_CA settings
// in the user DB. The system trust settings should not be used, but the trust
// from the user DB should be honored.
TEST_P(TrustStoreNSSTestIgnoreSystemCerts, UserDbTrustForSystemRootHonored) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_ANCHOR);
  // NSS should see the cert as trusted.
  EXPECT_EQ(CERTDB_TRUSTED_CA | CERTDB_VALID_CA,
            GetNSSTrustForCert(system_root.get()));

  // TrustStoreNSS should see as TrustAnchor since the cert was trusted in the
  // user slot.
  EXPECT_TRUE(HasTrust({system_root}, ExpectedTrustForAnchor()));
}

// A system trusted root is also present in a user DB, with leaf trust in the
// user DB. The system trust settings should not be used, but the trust from
// the user DB should be honored.
TEST_P(TrustStoreNSSTestIgnoreSystemCerts,
       UserDbLeafTrustForSystemRootHonored) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  // Add unrelated trust record to test that we find the correct one.
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_ANCHOR);

  // Trust the system cert as a leaf.
  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_LEAF);

  // Add unrelated trust record to test that we find the correct one.
  AddCertToNSSSlotWithTrust(newintermediate_.get(), test_nssdb_.slot(),
                            CertificateTrustType::DISTRUSTED);

  // NSS should see the cert as a trusted leaf.
  EXPECT_EQ(CERTDB_TRUSTED | CERTDB_TERMINAL_RECORD,
            GetNSSTrustForCert(system_root.get()));

  // TrustStoreNSS should see as TrustedLeaf since the cert was trusted in the
  // user slot.
  EXPECT_TRUE(HasTrust({system_root}, ExpectedTrustForLeaf()));
}

// A system trusted root is also present in a user DB, with both CA and leaf
// trust in the user DB. The system trust settings should not be used, but the
// trust from the user DB should be honored.
TEST_P(TrustStoreNSSTestIgnoreSystemCerts,
       UserDbAnchorAndLeafTrustForSystemRootHonored) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF);

  // NSS should see the cert as both trusted leaf and CA.
  EXPECT_EQ(CERTDB_TRUSTED_CA | CERTDB_VALID_CA | CERTDB_TRUSTED |
                CERTDB_TERMINAL_RECORD,
            GetNSSTrustForCert(system_root.get()));

  // TrustStoreNSS should see as TrustAnchor since the cert was trusted in the
  // user slot. The TrustStoreNSS implementation isn't able to pick up both the
  // CA and Leaf trust in this case, but we don't really care.
  EXPECT_TRUE(HasTrust({system_root}, ExpectedTrustForAnchor()));
}

// A system trusted root is also present in a user DB, with TERMINAL_RECORD
// settings in the user DB. The system trust settings should not be used, and
// the distrust from the user DB should be honored.
TEST_P(TrustStoreNSSTestIgnoreSystemCerts, UserDbDistrustForSystemRootHonored) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            CertificateTrustType::DISTRUSTED);

  // NSS should see the cert as distrusted.
  EXPECT_EQ(CERTDB_TERMINAL_RECORD, GetNSSTrustForCert(system_root.get()));

  // TrustStoreNSS should see as Distrusted since the cert was distrusted in
  // the user slot.
  EXPECT_TRUE(HasTrust({system_root}, CertificateTrust::ForDistrusted()));
}

// A system trusted root is also present in a user DB, with a trust object with
// no SSL trust flags set in the user DB. The system trust settings should not
// be used, and the lack of trust flags in the user DB should result in
// unspecified trust.
TEST_P(TrustStoreNSSTestIgnoreSystemCerts,
       UserDbUnspecifiedTrustForSystemRootHonored) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            CertificateTrustType::UNSPECIFIED);

  // NSS should see the cert as unspecified trust.
  EXPECT_EQ(0u, GetNSSTrustForCert(system_root.get()));

  // TrustStoreNSS should see as Unspecified since the cert was marked
  // unspecified in the user slot.
  EXPECT_TRUE(HasTrust({system_root}, CertificateTrust::ForUnspecified()));
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
        TrustStoreNSS::kUseSystemTrust,
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
class TrustStoreNSSTestDoNotAllowUserSlots
    : public TrustStoreNSSTestBase,
      public testing::WithParamInterface<TrustStoreNSS::SystemTrustSetting> {
 public:
  TrustStoreNSSTestDoNotAllowUserSlots() = default;
  ~TrustStoreNSSTestDoNotAllowUserSlots() override = default;

  TrustStoreNSS::SystemTrustSetting system_trust_setting() const {
    return GetParam();
  }

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(system_trust_setting(),
                                           /*user_slot_trust_setting=*/nullptr);
  }
};

// A certificate that is stored on a "user slot" is not trusted if the
// TrustStoreNSS is not allowed to trust certificates on user slots.
TEST_P(TrustStoreNSSTestDoNotAllowUserSlots, CertOnUserSlot) {
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_ANCHOR);
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));

  // We don't do any filtering of the certs returned by GetIssuersOf since
  // there isn't a security reason to.
  EXPECT_TRUE(TrustStoreContains(newintermediate_, {newroot_}));
}

// If an NSS trusted root is present in a user slot but
// user_slot_trust_setting=null, that trust setting should be ignored.
TEST_P(TrustStoreNSSTestDoNotAllowUserSlots, UserDbTrustForSystemRootIgnored) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);
  EXPECT_EQ(CERTDB_TRUSTED_CA | CERTDB_VALID_CA,
            GetNSSTrustForCert(system_root.get()));

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_LEAF);

  switch (system_trust_setting()) {
    case TrustStoreNSS::SystemTrustSetting::kUseSystemTrust:
      // This should actually be CertificateTrust::ForTrustAnchor, but the old
      // kUseSystemTrust code path doesn't handle trust settings in multiple
      // slots correctly, and we aren't going to fix that now.
      EXPECT_TRUE(HasTrust({system_root}, ExpectedTrustForLeaf()));
      break;
    case TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust:
      EXPECT_TRUE(HasTrust({system_root}, CertificateTrust::ForUnspecified()));
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TrustStoreNSSTestDoNotAllowUserSlots,
    ::testing::Values(TrustStoreNSS::SystemTrustSetting::kUseSystemTrust,
                      TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust),
    [](const testing::TestParamInfo<
        TrustStoreNSSTestDoNotAllowUserSlots::ParamType>& info) {
      return SystemTrustSettingToString(info.param);
    });

// Tests for a TrustStoreNSS which does allows certificates on user slots to
// be only trusted if they are on a specific user slot.
class TrustStoreNSSTestAllowSpecifiedUserSlot
    : public TrustStoreNSSTestBase,
      public testing::WithParamInterface<TrustStoreNSS::SystemTrustSetting> {
 public:
  TrustStoreNSSTestAllowSpecifiedUserSlot() = default;
  ~TrustStoreNSSTestAllowSpecifiedUserSlot() override = default;

  TrustStoreNSS::SystemTrustSetting system_trust_setting() const {
    return GetParam();
  }

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        system_trust_setting(),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
  }
};

// A certificate that is stored on a "user slot" is trusted if the
// TrustStoreNSS is allowed to trust that user slot.
TEST_P(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnUserSlot) {
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_ANCHOR);
  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

// A certificate that is stored on a "user slot" is not trusted if the
// TrustStoreNSS is allowed to trust a user slot, but the certificate is
// stored on another user slot.
TEST_P(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnOtherUserSlot) {
  AddCertToNSSSlotWithTrust(newroot_.get(), other_test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_ANCHOR);
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));
}

// The same certificate is stored in multiple user slots with different trust
// settings. Ensure that the correct trust setting is used.
TEST_P(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnMultipleSlots) {
  // Add unrelated trust record to test that we find the correct one.
  AddCertToNSSSlotWithTrust(newintermediate_.get(), test_nssdb_.slot(),
                            CertificateTrustType::DISTRUSTED);

  AddCertToNSSSlotWithTrust(newroot_.get(), first_test_nssdb_.slot(),
                            CertificateTrustType::DISTRUSTED);
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_ANCHOR);
  AddCertToNSSSlotWithTrust(newroot_.get(), other_test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_LEAF);

  // Add unrelated trust record to test that we find the correct one.
  AddCertToNSSSlotWithTrust(target_.get(), test_nssdb_.slot(),
                            CertificateTrustType::DISTRUSTED);

  switch (system_trust_setting()) {
    case TrustStoreNSS::SystemTrustSetting::kUseSystemTrust:
      // The kUseSystemTrust implementation doesn't attempt to handle this case
      // correctly, but it actually is slightly more broken than expected and
      // ends up returning Unspecified rather than any of the trust settings
      // set above since the PK11_GetAllSlotsForCert call in
      // TrustStoreNSS::IsCertAllowedForTrust doesn't actually do what it
      // claims to do.
      EXPECT_TRUE(HasTrust({newroot_}, CertificateTrust::ForUnspecified()));
      break;
    case TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust:
      EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
      break;
  }
}

// A NSS trusted root certificate is also stored in multiple user slots with
// different trust settings. Ensure that the correct trust setting is used.
TEST_P(TrustStoreNSSTestAllowSpecifiedUserSlot, SystemRootCertOnMultipleSlots) {
  std::shared_ptr<const ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);
  EXPECT_EQ(CERTDB_TRUSTED_CA | CERTDB_VALID_CA,
            GetNSSTrustForCert(system_root.get()));

  AddCertToNSSSlotWithTrust(system_root.get(), first_test_nssdb_.slot(),
                            CertificateTrustType::DISTRUSTED);
  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            CertificateTrustType::TRUSTED_LEAF);
  AddCertToNSSSlotWithTrust(system_root.get(), other_test_nssdb_.slot(),
                            CertificateTrustType::UNSPECIFIED);
  switch (system_trust_setting()) {
    case TrustStoreNSS::SystemTrustSetting::kUseSystemTrust:
      // The kUseSystemTrust implementation doesn't attempt to handle this case
      // correctly, it returns distrust due to how NSS evaluates the trust
      // records internally with regards to the order the slots were
      // created.
      EXPECT_TRUE(HasTrust({system_root}, CertificateTrust::ForDistrusted()));
      break;
    case TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust:
      EXPECT_TRUE(HasTrust({system_root}, ExpectedTrustForLeaf()));
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TrustStoreNSSTestAllowSpecifiedUserSlot,
    ::testing::Values(TrustStoreNSS::SystemTrustSetting::kUseSystemTrust,
                      TrustStoreNSS::SystemTrustSetting::kIgnoreSystemTrust),
    [](const testing::TestParamInfo<
        TrustStoreNSSTestAllowSpecifiedUserSlot::ParamType>& info) {
      return SystemTrustSettingToString(info.param);
    });

// TODO(https://crbug.com/980443): If the internal non-removable slot is
// relevant on Chrome OS, add a test for allowing trust for certificates
// stored on that slot.

class TrustStoreNSSTestDelegate {
 public:
  TrustStoreNSSTestDelegate()
      : trust_store_nss_(TrustStoreNSS::kUseSystemTrust,
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
