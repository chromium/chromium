// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/cert/nss_profile_filter_chromeos.h"

#include <cert.h>
#include <pk11pub.h>
#include <secmod.h>

#include <algorithm>
#include <utility>

#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

crypto::ScopedPK11Slot GetRootCertsSlot() {
  crypto::AutoSECMODListReadLock auto_lock;
  SECMODModuleList* head = SECMOD_GetDefaultModuleList();
  for (SECMODModuleList* item = head; item != nullptr; item = item->next) {
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

ScopedCERTCertificateList ListCertsInSlot(PK11SlotInfo* slot) {
  ScopedCERTCertificateList result;
  crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot));
  if (!cert_list)
    return result;
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    result.push_back(x509_util::DupCERTCertificate(node->cert));
  }

  // Sort the result so that test comparisons can be deterministic.
  std::sort(
      result.begin(), result.end(),
      [](const ScopedCERTCertificate& lhs, const ScopedCERTCertificate& rhs) {
        return x509_util::CalculateFingerprint256(lhs.get()) <
               x509_util::CalculateFingerprint256(rhs.get());
      });
  return result;
}

}  // anonymous namespace

class NSSProfileFilterChromeOSTest : public testing::Test {
 public:
  NSSProfileFilterChromeOSTest() : user_1_("user1"), user_2_("user2") {}

  void SetUp() override {
    ASSERT_TRUE(system_slot_user_.is_open());
    ASSERT_TRUE(user_1_.constructed_successfully());
    ASSERT_TRUE(user_2_.constructed_successfully());
    user_1_.FinishInit();
    user_2_.FinishInit();

    // TODO(mattm): more accurately test public/private slot filtering somehow.
    // (The slots used to initialize a profile filter should be separate slots
    // in separate modules, while ScopedTestNSSChromeOSUser uses the same slot
    // for both.)
    crypto::ScopedPK11Slot private_slot_1(crypto::GetPrivateSlotForChromeOSUser(
        user_1_.username_hash(),
        base::OnceCallback<void(crypto::ScopedPK11Slot)>()));
    ASSERT_TRUE(private_slot_1.get());
    profile_filter_1_.Init(
        crypto::GetPublicSlotForChromeOSUser(user_1_.username_hash()),
        std::move(private_slot_1), get_system_slot());

    profile_filter_1_copy_ = profile_filter_1_;

    crypto::ScopedPK11Slot private_slot_2(crypto::GetPrivateSlotForChromeOSUser(
        user_2_.username_hash(),
        base::OnceCallback<void(crypto::ScopedPK11Slot)>()));
    ASSERT_TRUE(private_slot_2.get());
    profile_filter_2_.Init(
        crypto::GetPublicSlotForChromeOSUser(user_2_.username_hash()),
        std::move(private_slot_2),
        crypto::ScopedPK11Slot() /* no system slot */);

    certs_ = CreateCERTCertificateListFromFile(GetTestCertsDirectory(),
                                               "root_ca_cert.pem",
                                               X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(1U, certs_.size());
  }

  crypto::ScopedPK11Slot get_system_slot() {
    return crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_slot_user_.slot()));
  }

 protected:
  ScopedCERTCertificateList certs_;
  crypto::ScopedTestNSSDB system_slot_user_;
  crypto::ScopedTestNSSChromeOSUser user_1_;
  crypto::ScopedTestNSSChromeOSUser user_2_;
  NSSProfileFilterChromeOS no_slots_profile_filter_;
  NSSProfileFilterChromeOS profile_filter_1_;
  NSSProfileFilterChromeOS profile_filter_2_;
  NSSProfileFilterChromeOS profile_filter_1_copy_;
};

TEST_F(NSSProfileFilterChromeOSTest, TempCertNotAllowed) {
  EXPECT_EQ(nullptr, certs_[0]->slot);
  EXPECT_FALSE(no_slots_profile_filter_.IsCertAllowed(certs_[0].get()));
  EXPECT_FALSE(profile_filter_1_.IsCertAllowed(certs_[0].get()));
  EXPECT_FALSE(profile_filter_1_copy_.IsCertAllowed(certs_[0].get()));
  EXPECT_FALSE(profile_filter_2_.IsCertAllowed(certs_[0].get()));
}

TEST_F(NSSProfileFilterChromeOSTest, InternalSlotAllowed) {
  crypto::ScopedPK11Slot internal_slot(PK11_GetInternalSlot());
  ASSERT_TRUE(internal_slot.get());
  EXPECT_TRUE(no_slots_profile_filter_.IsModuleAllowed(internal_slot.get()));
  EXPECT_TRUE(profile_filter_1_.IsModuleAllowed(internal_slot.get()));
  EXPECT_TRUE(profile_filter_1_copy_.IsModuleAllowed(internal_slot.get()));
  EXPECT_TRUE(profile_filter_2_.IsModuleAllowed(internal_slot.get()));

  crypto::ScopedPK11Slot internal_key_slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(internal_key_slot.get());
  EXPECT_TRUE(
      no_slots_profile_filter_.IsModuleAllowed(internal_key_slot.get()));
  EXPECT_TRUE(profile_filter_1_.IsModuleAllowed(internal_key_slot.get()));
  EXPECT_TRUE(profile_filter_1_copy_.IsModuleAllowed(internal_key_slot.get()));
  EXPECT_TRUE(profile_filter_2_.IsModuleAllowed(internal_key_slot.get()));
}

TEST_F(NSSProfileFilterChromeOSTest, RootCertsAllowed) {
  crypto::ScopedPK11Slot root_certs_slot(GetRootCertsSlot());
  ASSERT_TRUE(root_certs_slot.get());
  EXPECT_TRUE(no_slots_profile_filter_.IsModuleAllowed(root_certs_slot.get()));
  EXPECT_TRUE(profile_filter_1_.IsModuleAllowed(root_certs_slot.get()));
  EXPECT_TRUE(profile_filter_1_copy_.IsModuleAllowed(root_certs_slot.get()));
  EXPECT_TRUE(profile_filter_2_.IsModuleAllowed(root_certs_slot.get()));

  ScopedCERTCertificateList root_certs(ListCertsInSlot(root_certs_slot.get()));
  ASSERT_FALSE(root_certs.empty());
  EXPECT_TRUE(no_slots_profile_filter_.IsCertAllowed(root_certs[0].get()));
  EXPECT_TRUE(profile_filter_1_.IsCertAllowed(root_certs[0].get()));
  EXPECT_TRUE(profile_filter_1_copy_.IsCertAllowed(root_certs[0].get()));
  EXPECT_TRUE(profile_filter_2_.IsCertAllowed(root_certs[0].get()));
}

TEST_F(NSSProfileFilterChromeOSTest, SoftwareSlots) {
  crypto::ScopedPK11Slot system_slot(get_system_slot());
  crypto::ScopedPK11Slot slot_1(
      crypto::GetPublicSlotForChromeOSUser(user_1_.username_hash()));
  ASSERT_TRUE(slot_1);
  crypto::ScopedPK11Slot slot_2(
      crypto::GetPublicSlotForChromeOSUser(user_2_.username_hash()));
  ASSERT_TRUE(slot_2);

  CERTCertificate* cert_1 = certs_[0].get();
  ScopedCERTCertificateList certs_2 = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs_2.size());
  CERTCertificate* cert_2 = certs_2[0].get();
  ScopedCERTCertificateList system_certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "mit.davidben.der",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, system_certs.size());
  CERTCertificate* system_cert = system_certs[0].get();

  ASSERT_EQ(SECSuccess,
            PK11_ImportCert(slot_1.get(), cert_1, CK_INVALID_HANDLE, "cert1",
                            PR_FALSE /* includeTrust (unused) */));

  ASSERT_EQ(SECSuccess,
            PK11_ImportCert(slot_2.get(), cert_2, CK_INVALID_HANDLE, "cert2",
                            PR_FALSE /* includeTrust (unused) */));
  ASSERT_EQ(SECSuccess, PK11_ImportCert(system_slot.get(), system_cert,
                                        CK_INVALID_HANDLE, "systemcert",
                                        PR_FALSE /* includeTrust (unused) */));

  EXPECT_FALSE(no_slots_profile_filter_.IsCertAllowed(cert_1));
  EXPECT_FALSE(no_slots_profile_filter_.IsCertAllowed(cert_2));
  EXPECT_FALSE(no_slots_profile_filter_.IsCertAllowed(system_cert));

  EXPECT_TRUE(profile_filter_1_.IsCertAllowed(cert_1));
  EXPECT_TRUE(profile_filter_1_copy_.IsCertAllowed(cert_1));
  EXPECT_FALSE(profile_filter_1_.IsCertAllowed(cert_2));
  EXPECT_FALSE(profile_filter_1_copy_.IsCertAllowed(cert_2));
  EXPECT_TRUE(profile_filter_1_.IsCertAllowed(system_cert));
  EXPECT_TRUE(profile_filter_1_copy_.IsCertAllowed(system_cert));

  EXPECT_FALSE(profile_filter_2_.IsCertAllowed(cert_1));
  EXPECT_TRUE(profile_filter_2_.IsCertAllowed(cert_2));
  EXPECT_FALSE(profile_filter_2_.IsCertAllowed(system_cert));
}

}  // namespace net
