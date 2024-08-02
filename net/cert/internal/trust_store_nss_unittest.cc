// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_nss.h"

#include <cert.h>
#include <certdb.h>
#include <pkcs11n.h>
#include <prtypes.h>

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/features.h"
#include "net/cert/internal/cert_issuer_source_sync_unittest.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

namespace {

unsigned TrustTypeToNSSTrust(bssl::CertificateTrustType trust) {
  switch (trust) {
    case bssl::CertificateTrustType::DISTRUSTED:
      return CERTDB_TERMINAL_RECORD;
    case bssl::CertificateTrustType::UNSPECIFIED:
      return 0;
    case bssl::CertificateTrustType::TRUSTED_ANCHOR:
      return CERTDB_TRUSTED_CA | CERTDB_VALID_CA;
    case bssl::CertificateTrustType::TRUSTED_LEAF:
      return CERTDB_TRUSTED | CERTDB_TERMINAL_RECORD;
    case bssl::CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      return CERTDB_TRUSTED_CA | CERTDB_VALID_CA | CERTDB_TRUSTED |
             CERTDB_TERMINAL_RECORD;
  }
}

std::shared_ptr<const bssl::ParsedCertificate> GetASSLTrustedBuiltinRoot() {
  bssl::CertErrors parsing_errors;
  ScopedCERTCertificate nss_cert = GetAnNssBuiltinSslTrustedRoot();
  if (!nss_cert) {
    return nullptr;
  }
  scoped_refptr<X509Certificate> ssl_trusted_root =
      x509_util::CreateX509CertificateFromCERTCertificate(nss_cert.get());
  if (!ssl_trusted_root) {
    return nullptr;
  }
  return bssl::ParsedCertificate::Create(
      bssl::UpRef(ssl_trusted_root->cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), &parsing_errors);
}

std::optional<unsigned> GetNSSTrustForCert(
    const bssl::ParsedCertificate* cert) {
  SECItem der_cert;
  der_cert.data = const_cast<uint8_t*>(cert->der_cert().data());
  der_cert.len = base::checked_cast<unsigned>(cert->der_cert().size());
  der_cert.type = siDERCertBuffer;
  ScopedCERTCertificate nss_cert(
      CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));
  if (!nss_cert) {
    return std::nullopt;
  }

  CERTCertTrust nss_cert_trust;
  if (CERT_GetCertTrust(nss_cert.get(), &nss_cert_trust) != SECSuccess) {
    return std::nullopt;
  }

  return SEC_GET_TRUST_FLAGS(&nss_cert_trust, trustSSL);
}

class TrustStoreNSSTestBase : public ::testing::Test {
 public:
  bssl::CertificateTrust ExpectedTrustForBuiltinAnchor() const {
    return bssl::CertificateTrust::ForTrustAnchor();
  }

  bssl::CertificateTrust ExpectedTrustForAnchor() const {
    return bssl::CertificateTrust::ForTrustAnchor()
        .WithEnforceAnchorConstraints()
        .WithEnforceAnchorExpiry();
  }

  bssl::CertificateTrust ExpectedTrustForAnchorOrLeaf() const {
    return bssl::CertificateTrust::ForTrustAnchorOrLeaf()
        .WithEnforceAnchorConstraints()
        .WithEnforceAnchorExpiry();
  }

  bssl::CertificateTrust ExpectedTrustForLeaf() const {
    return bssl::CertificateTrust::ForTrustedLeaf();
  }

  void SetUp() override {
    ASSERT_TRUE(first_test_nssdb_.is_open());
    ASSERT_TRUE(test_nssdb_.is_open());
    ASSERT_TRUE(other_test_nssdb_.is_open());
    bssl::ParsedCertificateList chain;
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

  void AddCertToNSSSlot(const bssl::ParsedCertificate* cert,
                        PK11SlotInfo* slot) {
    ScopedCERTCertificate nss_cert(
        x509_util::CreateCERTCertificateFromBytes(cert->der_cert()));
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
  void AddCertToNSSSlotWithTrust(const bssl::ParsedCertificate* cert,
                                 PK11SlotInfo* slot,
                                 bssl::CertificateTrustType trust) {
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
  void TrustCert(const bssl::ParsedCertificate* cert) {
    ChangeCertTrust(cert, CERTDB_TRUSTED_CA | CERTDB_VALID_CA);
  }

  // Trusts |cert| as a server, but not as a CA. Assumes the cert was already
  // imported into NSS.
  void TrustServerCert(const bssl::ParsedCertificate* cert) {
    ChangeCertTrust(cert, CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED);
  }

  // Trusts |cert| as both a server and as a CA. Assumes the cert was already
  // imported into NSS.
  void TrustCaAndServerCert(const bssl::ParsedCertificate* cert) {
    ChangeCertTrust(cert, CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED |
                              CERTDB_TRUSTED_CA | CERTDB_VALID_CA);
  }

  // Distrusts |cert|. Assumes the cert was already imported into NSS.
  void DistrustCert(const bssl::ParsedCertificate* cert) {
    ChangeCertTrust(cert, CERTDB_TERMINAL_RECORD);
  }

  void ChangeCertTrust(const bssl::ParsedCertificate* cert, int flags) {
    SECItem der_cert;
    der_cert.data = const_cast<uint8_t*>(cert->der_cert().data());
    der_cert.len = base::checked_cast<unsigned>(cert->der_cert().size());
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
  void ChangeCertTrustInSlot(const bssl::ParsedCertificate* cert,
                             PK11SlotInfo* slot,
                             bssl::CertificateTrustType trust) {
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
  bool TrustStoreContains(std::shared_ptr<const bssl::ParsedCertificate> cert,
                          bssl::ParsedCertificateList expected_matches) {
    bssl::ParsedCertificateList matches;
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
      const std::shared_ptr<const bssl::ParsedCertificate>& cert) const {
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

  bool HasTrust(const bssl::ParsedCertificateList& certs,
                bssl::CertificateTrust expected_trust) {
    bool success = true;
    for (const std::shared_ptr<const bssl::ParsedCertificate>& cert : certs) {
      bssl::CertificateTrust trust = trust_store_nss_->GetTrust(cert.get());
      std::string trust_string = trust.ToDebugString();
      std::string expected_trust_string = expected_trust.ToDebugString();
      if (trust_string != expected_trust_string) {
        EXPECT_EQ(expected_trust_string, trust_string) << GetCertString(cert);
        success = false;
      }
    }

    return success;
  }

  std::shared_ptr<const bssl::ParsedCertificate> oldroot_;
  std::shared_ptr<const bssl::ParsedCertificate> newroot_;

  std::shared_ptr<const bssl::ParsedCertificate> target_;
  std::shared_ptr<const bssl::ParsedCertificate> oldintermediate_;
  std::shared_ptr<const bssl::ParsedCertificate> newintermediate_;
  std::shared_ptr<const bssl::ParsedCertificate> newrootrollover_;
  crypto::ScopedTestNSSDB first_test_nssdb_;
  crypto::ScopedTestNSSDB test_nssdb_;
  crypto::ScopedTestNSSDB other_test_nssdb_;
  std::unique_ptr<TrustStoreNSS> trust_store_nss_;
  unsigned nickname_counter_ = 0;
};

// Specifies which kind of per-slot filtering the TrustStoreNSS is supposed to
// perform in the parametrized TrustStoreNSSTestWithSlotFilterType.
enum class SlotFilterType {
  kDontFilter,
  kAllowSpecifiedUserSlot
};

std::string SlotFilterTypeToString(SlotFilterType slot_filter_type) {
  switch (slot_filter_type) {
    case SlotFilterType::kDontFilter:
      return "DontFilter";
    case SlotFilterType::kAllowSpecifiedUserSlot:
      return "AllowSpecifiedUserSlot";
  }
}

// Used for testing a TrustStoreNSS with the slot filter type specified by the
// test parameter. These tests are cases that are expected to be the same
// regardless of the slot filter type.
class TrustStoreNSSTestWithSlotFilterType
    : public TrustStoreNSSTestBase,
      public testing::WithParamInterface<SlotFilterType> {
 public:
  TrustStoreNSSTestWithSlotFilterType() = default;
  ~TrustStoreNSSTestWithSlotFilterType() override = default;

  SlotFilterType slot_filter_type() const { return GetParam(); }

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    switch (slot_filter_type()) {
      case SlotFilterType::kDontFilter:
        return std::make_unique<TrustStoreNSS>(
            TrustStoreNSS::UseTrustFromAllUserSlots());
      case SlotFilterType::kAllowSpecifiedUserSlot:
        return std::make_unique<TrustStoreNSS>(
            crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
    }
  }
};

// Without adding any certs to the NSS DB, should get no anchor results for
// any of the test certs.
TEST_P(TrustStoreNSSTestWithSlotFilterType, CertsNotPresent) {
  EXPECT_TRUE(TrustStoreContains(target_, bssl::ParsedCertificateList()));
  EXPECT_TRUE(
      TrustStoreContains(newintermediate_, bssl::ParsedCertificateList()));
  EXPECT_TRUE(TrustStoreContains(newroot_, bssl::ParsedCertificateList()));
  EXPECT_TRUE(HasTrust({target_}, bssl::CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(
      HasTrust({newintermediate_}, bssl::CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({newroot_}, bssl::CertificateTrust::ForUnspecified()));
}

// TrustStoreNSS should return temporary certs on Chrome OS, because on Chrome
// OS temporary certs are used to supply policy-provided untrusted authority
// certs. (See https://crbug.com/978854)
// On other platforms it's not required but doesn't hurt anything.
TEST_P(TrustStoreNSSTestWithSlotFilterType, TempCertPresent) {
  ScopedCERTCertificate temp_nss_cert(
      x509_util::CreateCERTCertificateFromBytes(newintermediate_->der_cert()));
  EXPECT_TRUE(TrustStoreContains(target_, {newintermediate_}));
  EXPECT_TRUE(HasTrust({target_}, bssl::CertificateTrust::ForUnspecified()));
}

// Independent of the specified slot-based filtering mode, built-in root certs
// should never be trusted.
TEST_P(TrustStoreNSSTestWithSlotFilterType, TrustAllowedForBuiltinRootCerts) {
  auto builtin_root_cert = GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(builtin_root_cert);
  EXPECT_TRUE(
      HasTrust({builtin_root_cert}, bssl::CertificateTrust::ForUnspecified()));
}

// Check that ListCertsIgnoringNSSRoots and GetAllUserAddedCerts don't
// return built-in roots.
TEST_P(TrustStoreNSSTestWithSlotFilterType, ListCertsIgnoresBuiltinRoots) {
  ScopedCERTCertificate root_cert = GetAnNssBuiltinSslTrustedRoot();
  ASSERT_TRUE(root_cert);

  for (const auto& result :
       trust_store_nss_->TrustStoreNSS::ListCertsIgnoringNSSRoots()) {
    EXPECT_FALSE(
        x509_util::IsSameCertificate(result.cert.get(), root_cert.get()));
  }

  for (const auto& cert_with_trust : trust_store_nss_->GetAllUserAddedCerts()) {
    EXPECT_FALSE(x509_util::IsSameCertificate(
        x509_util::CreateCryptoBuffer(cert_with_trust.cert_bytes).get(),
        root_cert.get()));
  }
}

// Check that GetAllUserAddedCerts doesn't return any client certs, as it is
// only supposed to return server certs.
TEST_P(TrustStoreNSSTestWithSlotFilterType, GetAllUserAddedCertsNoClientCerts) {
  scoped_refptr<X509Certificate> client_cert =
      ImportClientCertAndKeyFromFile(GetTestCertsDirectory(), "client_1.pem",
                                     "client_1.pk8", test_nssdb_.slot());
  ASSERT_TRUE(client_cert);

  bool found = false;
  for (const auto& result :
       trust_store_nss_->TrustStoreNSS::ListCertsIgnoringNSSRoots()) {
    found |= x509_util::IsSameCertificate(result.cert.get(), client_cert.get());
  }
  EXPECT_TRUE(found);

  for (const auto& cert_with_trust : trust_store_nss_->GetAllUserAddedCerts()) {
    EXPECT_FALSE(x509_util::CryptoBufferEqual(
        x509_util::CreateCryptoBuffer(cert_with_trust.cert_bytes).get(),
        client_cert->cert_buffer()));
  }
}

// Check that GetAllUserAddedCerts will return a client cert that has had trust
// bits added for server auth.
TEST_P(TrustStoreNSSTestWithSlotFilterType,
       GetAllUserAddedCertsManualTrustClientCert) {
  scoped_refptr<X509Certificate> client_cert =
      ImportClientCertAndKeyFromFile(GetTestCertsDirectory(), "client_1.pem",
                                     "client_1.pk8", test_nssdb_.slot());
  ASSERT_TRUE(client_cert);
  std::shared_ptr<const bssl::ParsedCertificate> parsed_client_cert =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(client_cert->cert_buffer()),
          x509_util::DefaultParseCertificateOptions(), nullptr);
  ASSERT_TRUE(parsed_client_cert);
  TrustCert(parsed_client_cert.get());

  {
    bool found = false;
    for (const auto& result :
         trust_store_nss_->TrustStoreNSS::ListCertsIgnoringNSSRoots()) {
      found |=
          x509_util::IsSameCertificate(result.cert.get(), client_cert.get());
    }
    EXPECT_TRUE(found);
  }

  {
    bool found = false;
    for (const auto& cert_with_trust :
         trust_store_nss_->GetAllUserAddedCerts()) {
      found |= x509_util::CryptoBufferEqual(
          x509_util::CreateCryptoBuffer(cert_with_trust.cert_bytes).get(),
          client_cert->cert_buffer());
    }
    EXPECT_TRUE(found);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TrustStoreNSSTestWithSlotFilterType,
    ::testing::Values(SlotFilterType::kDontFilter,
                      SlotFilterType::kAllowSpecifiedUserSlot),
    [](const testing::TestParamInfo<
        TrustStoreNSSTestWithSlotFilterType::ParamType>& info) {
      return SlotFilterTypeToString(info.param);
    });

// Tests a TrustStoreNSS that ignores system root certs.
class TrustStoreNSSTestIgnoreSystemCerts : public TrustStoreNSSTestBase {
 public:
  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        TrustStoreNSS::UseTrustFromAllUserSlots());
  }
};

TEST_F(TrustStoreNSSTestIgnoreSystemCerts, UnknownCertIgnored) {
  EXPECT_TRUE(HasTrust({newroot_}, bssl::CertificateTrust::ForUnspecified()));
}

// An NSS CERTCertificate object exists for the cert, but it is not
// imported into any DB. Should be unspecified trust.
TEST_F(TrustStoreNSSTestIgnoreSystemCerts, TemporaryCertIgnored) {
  ScopedCERTCertificate nss_cert(
      x509_util::CreateCERTCertificateFromBytes(newroot_->der_cert()));
  EXPECT_TRUE(HasTrust({newroot_}, bssl::CertificateTrust::ForUnspecified()));
}

// Cert is added to user DB, but without explicitly calling
// CERT_ChangeCertTrust. Should be unspecified trust.
TEST_F(TrustStoreNSSTestIgnoreSystemCerts, UserCertWithNoTrust) {
  AddCertsToNSS();
  EXPECT_TRUE(HasTrust({newroot_}, bssl::CertificateTrust::ForUnspecified()));
}

TEST_F(TrustStoreNSSTestIgnoreSystemCerts, UserRootTrusted) {
  AddCertsToNSS();
  TrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

TEST_F(TrustStoreNSSTestIgnoreSystemCerts, UserRootDistrusted) {
  AddCertsToNSS();
  DistrustCert(newroot_.get());
  EXPECT_TRUE(HasTrust({newroot_}, bssl::CertificateTrust::ForDistrusted()));
}

TEST_F(TrustStoreNSSTestIgnoreSystemCerts, UserTrustedServer) {
  AddCertsToNSS();
  TrustServerCert(target_.get());
  EXPECT_TRUE(HasTrust({target_}, ExpectedTrustForLeaf()));
}

TEST_F(TrustStoreNSSTestIgnoreSystemCerts, UserTrustedCaAndServer) {
  AddCertsToNSS();
  TrustCaAndServerCert(target_.get());
  EXPECT_TRUE(HasTrust({target_}, ExpectedTrustForAnchorOrLeaf()));
}

TEST_F(TrustStoreNSSTestIgnoreSystemCerts, SystemRootCertIgnored) {
  std::shared_ptr<const bssl::ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);
  EXPECT_TRUE(
      HasTrust({system_root}, bssl::CertificateTrust::ForUnspecified()));
}

// A system trusted root is also present in a user DB, but without any trust
// settings in the user DB. The system trust settings should not be used.
TEST_F(TrustStoreNSSTestIgnoreSystemCerts,
       SystemRootCertIgnoredWhenPresentInUserDb) {
  std::shared_ptr<const bssl::ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlot(system_root.get(), test_nssdb_.slot());

  // TrustStoreNSS should see an Unspecified since we are ignoring the system
  // slot.
  EXPECT_TRUE(
      HasTrust({system_root}, bssl::CertificateTrust::ForUnspecified()));
}

// A system trusted root is also present in a user DB, with TRUSTED_CA settings
// in the user DB. The system trust settings should not be used, but the trust
// from the user DB should be honored.
TEST_F(TrustStoreNSSTestIgnoreSystemCerts, UserDbTrustForSystemRootHonored) {
  std::shared_ptr<const bssl::ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_ANCHOR);
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
TEST_F(TrustStoreNSSTestIgnoreSystemCerts,
       UserDbLeafTrustForSystemRootHonored) {
  std::shared_ptr<const bssl::ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  // Add unrelated trust record to test that we find the correct one.
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_ANCHOR);

  // Trust the system cert as a leaf.
  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_LEAF);

  // Add unrelated trust record to test that we find the correct one.
  AddCertToNSSSlotWithTrust(newintermediate_.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::DISTRUSTED);

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
TEST_F(TrustStoreNSSTestIgnoreSystemCerts,
       UserDbAnchorAndLeafTrustForSystemRootHonored) {
  std::shared_ptr<const bssl::ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF);

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
TEST_F(TrustStoreNSSTestIgnoreSystemCerts, UserDbDistrustForSystemRootHonored) {
  std::shared_ptr<const bssl::ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::DISTRUSTED);

  // NSS should see the cert as distrusted.
  EXPECT_EQ(CERTDB_TERMINAL_RECORD, GetNSSTrustForCert(system_root.get()));

  // TrustStoreNSS should see as Distrusted since the cert was distrusted in
  // the user slot.
  EXPECT_TRUE(HasTrust({system_root}, bssl::CertificateTrust::ForDistrusted()));
}

// A system trusted root is also present in a user DB, with a trust object with
// no SSL trust flags set in the user DB. The system trust settings should not
// be used, and the lack of trust flags in the user DB should result in
// unspecified trust.
TEST_F(TrustStoreNSSTestIgnoreSystemCerts,
       UserDbUnspecifiedTrustForSystemRootHonored) {
  std::shared_ptr<const bssl::ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);

  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::UNSPECIFIED);

  // NSS should see the cert as unspecified trust.
  EXPECT_EQ(0u, GetNSSTrustForCert(system_root.get()));

  // TrustStoreNSS should see as Unspecified since the cert was marked
  // unspecified in the user slot.
  EXPECT_TRUE(
      HasTrust({system_root}, bssl::CertificateTrust::ForUnspecified()));
}

// Tests a TrustStoreNSS that does not filter which certificates
class TrustStoreNSSTestWithoutSlotFilter : public TrustStoreNSSTestBase {
 public:
  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        TrustStoreNSS::UseTrustFromAllUserSlots());
  }
};

// If certs are present in NSS DB but aren't marked as trusted, should get no
// anchor results for any of the test certs.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, CertsPresentButNotTrusted) {
  AddCertsToNSS();

  // None of the certificates are trusted.
  EXPECT_TRUE(HasTrust({oldroot_, newroot_, target_, oldintermediate_,
                        newintermediate_, newrootrollover_},
                       bssl::CertificateTrust::ForUnspecified()));
}

// Trust a single self-signed CA certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, TrustedCA) {
  AddCertsToNSS();
  TrustCert(newroot_.get());

  // Only one of the certificates are trusted.
  EXPECT_TRUE(HasTrust(
      {oldroot_, target_, oldintermediate_, newintermediate_, newrootrollover_},
      bssl::CertificateTrust::ForUnspecified()));

  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

// Distrust a single self-signed CA certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, DistrustedCA) {
  AddCertsToNSS();
  DistrustCert(newroot_.get());

  // Only one of the certificates are trusted.
  EXPECT_TRUE(HasTrust(
      {oldroot_, target_, oldintermediate_, newintermediate_, newrootrollover_},
      bssl::CertificateTrust::ForUnspecified()));

  EXPECT_TRUE(HasTrust({newroot_}, bssl::CertificateTrust::ForDistrusted()));
}

// Trust a single intermediate certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, TrustedIntermediate) {
  AddCertsToNSS();
  TrustCert(newintermediate_.get());

  EXPECT_TRUE(HasTrust(
      {oldroot_, newroot_, target_, oldintermediate_, newrootrollover_},
      bssl::CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({newintermediate_}, ExpectedTrustForAnchor()));
}

// Distrust a single intermediate certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, DistrustedIntermediate) {
  AddCertsToNSS();
  DistrustCert(newintermediate_.get());

  EXPECT_TRUE(HasTrust(
      {oldroot_, newroot_, target_, oldintermediate_, newrootrollover_},
      bssl::CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(
      HasTrust({newintermediate_}, bssl::CertificateTrust::ForDistrusted()));
}

// Trust a single server certificate.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, TrustedServer) {
  AddCertsToNSS();
  TrustServerCert(target_.get());

  EXPECT_TRUE(HasTrust({oldroot_, newroot_, oldintermediate_, newintermediate_,
                        newrootrollover_},
                       bssl::CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({target_}, ExpectedTrustForLeaf()));
}

// Trust a single certificate with both CA and server trust bits.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, TrustedCaAndServer) {
  AddCertsToNSS();
  TrustCaAndServerCert(target_.get());

  EXPECT_TRUE(HasTrust({oldroot_, newroot_, oldintermediate_, newintermediate_,
                        newrootrollover_},
                       bssl::CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({target_}, ExpectedTrustForAnchorOrLeaf()));
}

// Trust multiple self-signed CA certificates with the same name.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, MultipleTrustedCAWithSameSubject) {
  AddCertsToNSS();
  TrustCert(oldroot_.get());
  TrustCert(newroot_.get());

  EXPECT_TRUE(
      HasTrust({target_, oldintermediate_, newintermediate_, newrootrollover_},
               bssl::CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({oldroot_, newroot_}, ExpectedTrustForAnchor()));
}

// Different trust settings for multiple self-signed CA certificates with the
// same name.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, DifferingTrustCAWithSameSubject) {
  AddCertsToNSS();
  DistrustCert(oldroot_.get());
  TrustCert(newroot_.get());

  EXPECT_TRUE(
      HasTrust({target_, oldintermediate_, newintermediate_, newrootrollover_},
               bssl::CertificateTrust::ForUnspecified()));
  EXPECT_TRUE(HasTrust({oldroot_}, bssl::CertificateTrust::ForDistrusted()));
  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

// Check that ListCertsIgnoringNssRoots and GetAllUserAddedCerts are correctly
// looking at all slots.
TEST_F(TrustStoreNSSTestWithoutSlotFilter, ListCertsLooksAtAllSlots) {
  AddCertToNSSSlotWithTrust(oldroot_.get(), first_test_nssdb_.slot(),
                            bssl::CertificateTrustType::DISTRUSTED);
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_LEAF);

  {
    bool found_newroot = false;
    bool found_oldroot = false;
    for (const auto& result :
         trust_store_nss_->TrustStoreNSS::ListCertsIgnoringNSSRoots()) {
      found_oldroot |= x509_util::IsSameCertificate(result.cert.get(),
                                                    oldroot_->cert_buffer());
      found_newroot |= x509_util::IsSameCertificate(result.cert.get(),
                                                    newroot_->cert_buffer());
    }
    EXPECT_TRUE(found_newroot);
    EXPECT_TRUE(found_oldroot);
  }

  {
    bool found_newroot = false;
    bool found_oldroot = false;
    for (const auto& cert_with_trust :
         trust_store_nss_->GetAllUserAddedCerts()) {
      found_oldroot |= x509_util::CryptoBufferEqual(
          x509_util::CreateCryptoBuffer(cert_with_trust.cert_bytes).get(),
          oldroot_->cert_buffer());
      found_newroot |= x509_util::CryptoBufferEqual(
          x509_util::CreateCryptoBuffer(cert_with_trust.cert_bytes).get(),
          newroot_->cert_buffer());
    }
    EXPECT_TRUE(found_newroot);
    EXPECT_TRUE(found_oldroot);
  }
}

// Tests for a TrustStoreNSS which does allows certificates on user slots to
// be only trusted if they are on a specific user slot.
class TrustStoreNSSTestAllowSpecifiedUserSlot : public TrustStoreNSSTestBase {
 public:
  TrustStoreNSSTestAllowSpecifiedUserSlot() = default;
  ~TrustStoreNSSTestAllowSpecifiedUserSlot() override = default;

  std::unique_ptr<TrustStoreNSS> CreateTrustStoreNSS() override {
    return std::make_unique<TrustStoreNSS>(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));
  }
};

// A certificate that is stored on a "user slot" is trusted if the
// TrustStoreNSS is allowed to trust that user slot.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnUserSlot) {
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_ANCHOR);
  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

// A certificate that is stored on a "user slot" is not trusted if the
// TrustStoreNSS is allowed to trust a user slot, but the certificate is
// stored on another user slot.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnOtherUserSlot) {
  AddCertToNSSSlotWithTrust(newroot_.get(), other_test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_ANCHOR);
  EXPECT_TRUE(HasTrust({newroot_}, bssl::CertificateTrust::ForUnspecified()));
}

// The same certificate is stored in multiple user slots with different trust
// settings. Ensure that the correct trust setting is used.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, CertOnMultipleSlots) {
  // Add unrelated trust record to test that we find the correct one.
  AddCertToNSSSlotWithTrust(newintermediate_.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::DISTRUSTED);

  AddCertToNSSSlotWithTrust(newroot_.get(), first_test_nssdb_.slot(),
                            bssl::CertificateTrustType::DISTRUSTED);
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_ANCHOR);
  AddCertToNSSSlotWithTrust(newroot_.get(), other_test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_LEAF);

  // Add unrelated trust record to test that we find the correct one.
  AddCertToNSSSlotWithTrust(target_.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::DISTRUSTED);

  EXPECT_TRUE(HasTrust({newroot_}, ExpectedTrustForAnchor()));
}

// A NSS trusted root certificate is also stored in multiple user slots with
// different trust settings. Ensure that the correct trust setting is used.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, SystemRootCertOnMultipleSlots) {
  std::shared_ptr<const bssl::ParsedCertificate> system_root =
      GetASSLTrustedBuiltinRoot();
  ASSERT_TRUE(system_root);
  EXPECT_EQ(CERTDB_TRUSTED_CA | CERTDB_VALID_CA,
            GetNSSTrustForCert(system_root.get()));

  AddCertToNSSSlotWithTrust(system_root.get(), first_test_nssdb_.slot(),
                            bssl::CertificateTrustType::DISTRUSTED);
  AddCertToNSSSlotWithTrust(system_root.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_LEAF);
  AddCertToNSSSlotWithTrust(system_root.get(), other_test_nssdb_.slot(),
                            bssl::CertificateTrustType::UNSPECIFIED);

  EXPECT_TRUE(HasTrust({system_root}, ExpectedTrustForLeaf()));
}

// Check to see ListCertsIgnoringNSSRoots and GetAllUserAddedCerts correctly
// enforce slot filters.
TEST_F(TrustStoreNSSTestAllowSpecifiedUserSlot, ListCertsFiltersBySlot) {
  // Should not be in the results.
  AddCertToNSSSlotWithTrust(oldroot_.get(), first_test_nssdb_.slot(),
                            bssl::CertificateTrustType::DISTRUSTED);
  // Should be in the results.
  AddCertToNSSSlotWithTrust(newroot_.get(), test_nssdb_.slot(),
                            bssl::CertificateTrustType::TRUSTED_LEAF);

  {
    bool found_newroot = false;
    for (const auto& result :
         trust_store_nss_->TrustStoreNSS::ListCertsIgnoringNSSRoots()) {
      EXPECT_FALSE(x509_util::IsSameCertificate(result.cert.get(),
                                                oldroot_->cert_buffer()));
      found_newroot |= x509_util::IsSameCertificate(result.cert.get(),
                                                    newroot_->cert_buffer());
    }
    EXPECT_TRUE(found_newroot);
  }

  {
    bool found_newroot = false;
    for (const auto& cert_with_trust :
         trust_store_nss_->GetAllUserAddedCerts()) {
      EXPECT_FALSE(x509_util::CryptoBufferEqual(
          x509_util::CreateCryptoBuffer(cert_with_trust.cert_bytes).get(),
          oldroot_->cert_buffer()));
      found_newroot |= x509_util::CryptoBufferEqual(
          x509_util::CreateCryptoBuffer(cert_with_trust.cert_bytes).get(),
          newroot_->cert_buffer());
    }
    EXPECT_TRUE(found_newroot);
  }
}

// TODO(crbug.com/41468842): If the internal non-removable slot is
// relevant on Chrome OS, add a test for allowing trust for certificates
// stored on that slot.

class TrustStoreNSSTestDelegate {
 public:
  TrustStoreNSSTestDelegate()
      : trust_store_nss_(TrustStoreNSS::UseTrustFromAllUserSlots()) {}

  void AddCert(std::shared_ptr<const bssl::ParsedCertificate> cert) {
    ASSERT_TRUE(test_nssdb_.is_open());
    ScopedCERTCertificate nss_cert(
        x509_util::CreateCERTCertificateFromBytes(cert->der_cert()));
    ASSERT_TRUE(nss_cert);
    SECStatus srv = PK11_ImportCert(
        test_nssdb_.slot(), nss_cert.get(), CK_INVALID_HANDLE,
        GetUniqueNickname().c_str(), PR_FALSE /* includeTrust (unused) */);
    ASSERT_EQ(SECSuccess, srv);
  }

  bssl::CertIssuerSource& source() { return trust_store_nss_; }

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
