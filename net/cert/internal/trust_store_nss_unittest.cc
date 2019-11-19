// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_nss.h"

#include <cert.h>
#include <certdb.h>
#include <secmod.h>

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/internal/cert_issuer_source_sync_unittest.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

// Returns the slot which holds the built-in root certificates.
crypto::ScopedPK11Slot GetRootCertsSlot() {
  crypto::AutoSECMODListReadLock auto_lock;
  SECMODModuleList* head = SECMOD_GetDefaultModuleList();
  for (SECMODModuleList* item = head; item != NULL; item = item->next) {
    int slot_count = item->module->loaded ? item->module->slotCount : 0;
    for (int i = 0; i < slot_count; i++) {
      PK11SlotInfo* slot = item->module->slots[i];
      if (!PK11_IsPresent(slot))
        continue;
      if (PK11_HasRootCerts(slot))
        return crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot));
    }
  }
  return crypto::ScopedPK11Slot();
}

// Returns a built-in trusted root certificte. If multiple ones are available,
// it is not specified which one is returned. If none are available, returns
// nullptr.
scoped_refptr<ParsedCertificate> GetASSLTrustedBuiltinRoot() {
  crypto::ScopedPK11Slot root_certs_slot = GetRootCertsSlot();
  if (!root_certs_slot)
    return nullptr;

  scoped_refptr<X509Certificate> ssl_trusted_root;

  CERTCertList* cert_list = PK11_ListCertsInSlot(root_certs_slot.get());
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
  CERT_DestroyCertList(cert_list);
  if (!ssl_trusted_root)
    return nullptr;

  CertErrors parsing_errors;
  return ParsedCertificate::Create(bssl::UpRef(ssl_trusted_root->cert_buffer()),
                                   x509_util::DefaultParseCertificateOptions(),
                                   &parsing_errors);
}

class TrustStoreNSSTestBase : public ::testing::Test {
 public:
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
  bool TrustStoreContains(scoped_refptr<ParsedCertificate> cert,
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
      const scoped_refptr<ParsedCertificate>& cert) const {
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
                CertificateTrustType expected_trust) {
    bool success = true;
    for (const scoped_refptr<ParsedCertificate>& cert : certs) {
      CertificateTrust trust;
      trust_store_nss_->GetTrust(cert.get(), &trust, /*debug_data=*/nullptr);
      if (trust.type != expected_trust) {
        EXPECT_EQ(expected_trust, trust.type) << GetCertString(cert);
        success = false;
      }
    }

    return success;
  }

  scoped_refptr<ParsedCertificate> oldroot_;
  scoped_refptr<ParsedCertificate> newroot_;

  scoped_refptr<ParsedCertificate> target_;
  scoped_refptr<ParsedCertificate> oldintermediate_;
  scoped_refptr<ParsedCertificate> newintermediate_;
  scoped_refptr<ParsedCertificate> newrootrollover_;
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
        return std::make_unique<TrustStoreNSS>(trustSSL);
      case SlotFilterType::kDoNotAllowUserSlots:
        return std::make_unique<TrustStoreNSS>(
            trustSSL, TrustStoreNSS::DisallowTrustForCertsOnUserSlots());
      case SlotFilterType::kAllowSpecifiedUserSlot:
        return std::make_unique<TrustStoreNSS>(
            trustSSL,
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

#if !defined(OS_CHROMEOS)
// TrustStoreNSS should not return temporary certs. (See
// https://crbug.com/951166)
TEST_P(TrustStoreNSSTestWithSlotFilterType, TempCertNotPresent) {
  ScopedCERTCertificate temp_nss_cert(x509_util::CreateCERTCertificateFromBytes(
      newintermediate_->der_cert().UnsafeData(),
      newintermediate_->der_cert().Length()));
  EXPECT_TRUE(TrustStoreContains(target_, ParsedCertificateList()));
}
#else   // !defined(OS_CHROMEOS)
// TrustStoreNSS should return temporary certs on Chrome OS, because on Chrome
// OS temporary certs are used to supply policy-provided untrusted authority
// certs. (See https://crbug.com/978854)
TEST_P(TrustStoreNSSTestWithSlotFilterType, TempCertPresent) {
  ScopedCERTCertificate temp_nss_cert(x509_util::CreateCERTCertificateFromBytes(
      newintermediate_->der_cert().UnsafeData(),
      newintermediate_->der_cert().Length()));
  EXPECT_TRUE(TrustStoreContains(target_, {newintermediate_}));
}
#endif  // !defined(OS_CHROMEOS)

// Independent of the specified slot-based filtering mode, built-in root certs
// should always be trusted.
TEST_P(TrustStoreNSSTestWithSlotFilterType, TrustAllowedForBuiltinRootCerts) {
  auto builtin_root_cert = GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(builtin_root_cert);
  EXPECT_TRUE(
      HasTrust({builtin_root_cert}, CertificateTrustType::TRUSTED_ANCHOR));
}

INSTANTIATE_TEST_SUITE_P(
    TrustStoreNSSTest_AllSlotFilterTypes,
    TrustStoreNSSTestWithSlotFilterType,
    ::testing::Values(SlotFilterType::kDontFilter,
                      SlotFilterType::kDoNotAllowUserSlots,
                      SlotFilterType::kAllowSpecifiedUserSlot));

// Tests a TrustStoreNSS that does not filter which certificates
class TrustStoreNSSTestWithoutSlotFilter : public TrustStoreNSSTestBase {
 public:
  TrustStoreNSSTestWithoutSlotFilter() = default;
  ~TrustStoreNSSTestWithoutSlotFilter() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(trustSSL);
  }
};

// If certs are present in NSS DB but aren't marked as trusted, should get no
// anchor results for any of the test certs.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, CertsPresentButNotTrusted) {
  AddCertsToNSS();

  // None of the certificates are trusted.
  EXPECT_TRUE(HasTrust({oldroot_, newroot_, target_, oldintermediate_,
                        newintermediate_, newrootrollover_},
                       CertificateTrustType::UNSPECIFIED));
}

// Trust a single self-signed CA certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, TrustedCA) {
  AddCertsToNSS();
  TrustCert(newroot_.get());

  // Only one of the certificates are trusted.
  EXPECT_TRUE(HasTrust(
      {oldroot_, target_, oldintermediate_, newintermediate_, newrootrollover_},
      CertificateTrustType::UNSPECIFIED));

  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrustType::TRUSTED_ANCHOR));
}

// Distrust a single self-signed CA certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, DistrustedCA) {
  AddCertsToNSS();
  DistrustCert(newroot_.get());

  // Only one of the certificates are trusted.
  EXPECT_TRUE(HasTrust(
      {oldroot_, target_, oldintermediate_, newintermediate_, newrootrollover_},
      CertificateTrustType::UNSPECIFIED));

  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrustType::DISTRUSTED));
}

// Trust a single intermediate certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, TrustedIntermediate) {
  AddCertsToNSS();
  TrustCert(newintermediate_.get());

  EXPECT_TRUE(HasTrust(
      {oldroot_, newroot_, target_, oldintermediate_, newrootrollover_},
      CertificateTrustType::UNSPECIFIED));
  EXPECT_TRUE(
      HasTrust({newintermediate_}, CertificateTrustType::TRUSTED_ANCHOR));
}

// Distrust a single intermediate certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, DistrustedIntermediate) {
  AddCertsToNSS();
  DistrustCert(newintermediate_.get());

  EXPECT_TRUE(HasTrust(
      {oldroot_, newroot_, target_, oldintermediate_, newrootrollover_},
      CertificateTrustType::UNSPECIFIED));
  EXPECT_TRUE(HasTrust({newintermediate_}, CertificateTrustType::DISTRUSTED));
}

// Trust a single server certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, TrustedServer) {
  AddCertsToNSS();
  TrustServerCert(target_.get());

  // Server-trusted certificates are handled as UNSPECIFIED since we don't
  // support the notion of explictly trusted server certs. See
  // https://crbug.com/814994.
  EXPECT_TRUE(HasTrust({oldroot_, newroot_, target_, oldintermediate_,
                        newintermediate_, newrootrollover_},
                       CertificateTrustType::UNSPECIFIED));
}

// Trust multiple self-signed CA certificates with the same name.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, MultipleTrustedCAWithSameSubject) {
  AddCertsToNSS();
  TrustCert(oldroot_.get());
  TrustCert(newroot_.get());

  EXPECT_TRUE(
      HasTrust({target_, oldintermediate_, newintermediate_, newrootrollover_},
               CertificateTrustType::UNSPECIFIED));
  EXPECT_TRUE(
      HasTrust({oldroot_, newroot_}, CertificateTrustType::TRUSTED_ANCHOR));
}

// Different trust settings for multiple self-signed CA certificates with the
// same name.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, DifferingTrustCAWithSameSubject) {
  AddCertsToNSS();
  DistrustCert(oldroot_.get());
  TrustCert(newroot_.get());

  EXPECT_TRUE(
      HasTrust({target_, oldintermediate_, newintermediate_, newrootrollover_},
               CertificateTrustType::UNSPECIFIED));
  EXPECT_TRUE(HasTrust({oldroot_}, CertificateTrustType::DISTRUSTED));
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrustType::TRUSTED_ANCHOR));
}

// Tests for a TrustStoreNSS which does not allow certificates on user slots
// to be trusted.
class TrustStoreNSSTestDoNotAllowUserSlots : public TrustStoreNSSTestBase {
 public:
  TrustStoreNSSTestDoNotAllowUserSlots() = default;
  ~TrustStoreNSSTestDoNotAllowUserSlots() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        trustSSL, TrustStoreNSS::DisallowTrustForCertsOnUserSlots());
  }
};

// A certificate that is stored on a "user slot" is not trusted if the
// TrustStoreNSS is not allowed to trust certificates on user slots.
TEST_F(TrustStoreNSSTestDoNotAllowUserSlots, CertOnUserSlot) {
  AddCertToNSSSlot(newroot_.get(), test_nssdb_.slot());
  TrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrustType::UNSPECIFIED));
}

// Tests for a TrustStoreNSS which does allows certificates on user slots to
// be only trusted if they are on a specific user slot.
class TrustStoreNSSTestAllowSpecifiedUserSlot : public TrustStoreNSSTestBase {
 public:
  TrustStoreNSSTestAllowSpecifiedUserSlot() = default;
  ~TrustStoreNSSTestAllowSpecifiedUserSlot() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        trustSSL,
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
  }
};

// A certificate that is stored on a "user slot" is trusted if the
// TrustStoreNSS is allowed to trust that user slot.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnUserSlot) {
  AddCertToNSSSlot(newroot_.get(), test_nssdb_.slot());
  TrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrustType::TRUSTED_ANCHOR));
}

// A certificate that is stored on a "user slot" is not trusted if the
// TrustStoreNSS is allowed to trust a user slot, but the certificate is
// stored on another user slot.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnOtherUserSlot) {
  AddCertToNSSSlot(newroot_.get(), other_test_nssdb_.slot());
  TrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, CertificateTrustType::UNSPECIFIED));
}

// TODO(https://crbug.com/980443): If the internal non-removable slot is
// relevant on Chrome OS, add a test for allowing trust for certificates
// stored on that slot.

class TrustStoreNSSTestDelegate {
 public:
  TrustStoreNSSTestDelegate() : trust_store_nss_(trustSSL) {}

  void AddCert(scoped_refptr<ParsedCertificate> cert) {
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
