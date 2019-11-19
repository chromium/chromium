// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/nss_cert_database_chromeos.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

bool IsCertInCertificateList(const X509Certificate* cert,
                             const ScopedCERTCertificateList& cert_list) {
  for (const auto& other : cert_list) {
    if (x509_util::IsSameCertificate(other.get(), cert))
      return true;
  }
  return false;
}

bool IsCertInCertificateList(CERTCertificate* cert,
                             const ScopedCERTCertificateList& cert_list) {
  for (const auto& other : cert_list) {
    if (x509_util::IsSameCertificate(other.get(), cert))
      return true;
  }
  return false;
}

void SwapCertLists(ScopedCERTCertificateList* destination,
                   ScopedCERTCertificateList source) {
  ASSERT_TRUE(destination);

  destination->swap(source);
}

}  // namespace

class NSSCertDatabaseChromeOSTest : public TestWithTaskEnvironment,
                                    public CertDatabase::Observer {
 public:
  NSSCertDatabaseChromeOSTest()
      : observer_added_(false),
        db_changed_count_(0),
        user_1_("user1"),
        user_2_("user2") {}

  void SetUp() override {
    // Initialize nss_util slots.
    ASSERT_TRUE(user_1_.constructed_successfully());
    ASSERT_TRUE(user_2_.constructed_successfully());
    user_1_.FinishInit();
    user_2_.FinishInit();

    // Create NSSCertDatabaseChromeOS for each user.
    db_1_.reset(new NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(user_1_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            user_1_.username_hash(),
            base::Callback<void(crypto::ScopedPK11Slot)>())));
    db_1_->SetSystemSlot(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(system_db_.slot())));
    db_2_.reset(new NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(user_2_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            user_2_.username_hash(),
            base::Callback<void(crypto::ScopedPK11Slot)>())));

    // Add observer to CertDatabase for checking that notifications from
    // NSSCertDatabaseChromeOS are proxied to the CertDatabase.
    CertDatabase::GetInstance()->AddObserver(this);
    observer_added_ = true;
  }

  void TearDown() override {
    if (observer_added_)
      CertDatabase::GetInstance()->RemoveObserver(this);
  }

  // CertDatabase::Observer:
  void OnCertDBChanged() override { db_changed_count_++; }

 protected:
  bool observer_added_;
  int db_changed_count_;

  crypto::ScopedTestNSSChromeOSUser user_1_;
  crypto::ScopedTestNSSChromeOSUser user_2_;
  crypto::ScopedTestNSSDB system_db_;
  std::unique_ptr<NSSCertDatabaseChromeOS> db_1_;
  std::unique_ptr<NSSCertDatabaseChromeOS> db_2_;
};

// Test that ListModules() on each user includes that user's NSS software slot,
// and does not include the software slot of the other user. (Does not check the
// private slot, since it is the same as the public slot in tests.)
TEST_F(NSSCertDatabaseChromeOSTest, ListModules) {
  std::vector<crypto::ScopedPK11Slot> modules_1;
  std::vector<crypto::ScopedPK11Slot> modules_2;

  db_1_->ListModules(&modules_1, false /* need_rw */);
  db_2_->ListModules(&modules_2, false /* need_rw */);

  bool found_1 = false;
  for (std::vector<crypto::ScopedPK11Slot>::iterator it = modules_1.begin();
       it != modules_1.end(); ++it) {
    EXPECT_NE(db_2_->GetPublicSlot().get(), (*it).get());
    if ((*it).get() == db_1_->GetPublicSlot().get())
      found_1 = true;
  }
  EXPECT_TRUE(found_1);

  bool found_2 = false;
  for (std::vector<crypto::ScopedPK11Slot>::iterator it = modules_2.begin();
       it != modules_2.end(); ++it) {
    EXPECT_NE(db_1_->GetPublicSlot().get(), (*it).get());
    if ((*it).get() == db_2_->GetPublicSlot().get())
      found_2 = true;
  }
  EXPECT_TRUE(found_2);
}

// Test that ImportCACerts imports the cert to the correct slot, and that
// ListCerts includes the added cert for the correct user, and does not include
// it for the other user.
TEST_F(NSSCertDatabaseChromeOSTest, ImportCACerts) {
  // Load test certs from disk.
  ScopedCERTCertificateList certs_1 = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs_1.size());

  ScopedCERTCertificateList certs_2 = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "2048-rsa-root.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs_2.size());

  // Import one cert for each user.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(
      db_1_->ImportCACerts(certs_1, NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());
  failed.clear();
  EXPECT_TRUE(
      db_2_->ImportCACerts(certs_2, NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  // Get cert list for each user.
  ScopedCERTCertificateList user_1_certlist;
  ScopedCERTCertificateList user_2_certlist;
  db_1_->ListCerts(
      base::BindOnce(&SwapCertLists, base::Unretained(&user_1_certlist)));
  db_2_->ListCerts(
      base::BindOnce(&SwapCertLists, base::Unretained(&user_2_certlist)));

  // Run the message loop so the observer notifications get processed and
  // lookups are completed.
  RunUntilIdle();
  // Should have gotten two OnCertDBChanged notifications.
  ASSERT_EQ(2, db_changed_count_);

  EXPECT_TRUE(IsCertInCertificateList(certs_1[0].get(), user_1_certlist));
  EXPECT_FALSE(IsCertInCertificateList(certs_1[0].get(), user_2_certlist));

  EXPECT_TRUE(IsCertInCertificateList(certs_2[0].get(), user_2_certlist));
  EXPECT_FALSE(IsCertInCertificateList(certs_2[0].get(), user_1_certlist));
}

// Test that ImportServerCerts imports the cert to the correct slot, and that
// ListCerts includes the added cert for the correct user, and does not include
// it for the other user.
TEST_F(NSSCertDatabaseChromeOSTest, ImportServerCert) {
  // Load test certs from disk.
  ScopedCERTCertificateList certs_1 = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs_1.size());

  ScopedCERTCertificateList certs_2 = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "2048-rsa-ee-by-2048-rsa-intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs_2.size());

  // Import one cert for each user.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(
      db_1_->ImportServerCert(certs_1, NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());
  failed.clear();
  EXPECT_TRUE(
      db_2_->ImportServerCert(certs_2, NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  // Get cert list for each user.
  ScopedCERTCertificateList user_1_certlist;
  ScopedCERTCertificateList user_2_certlist;
  db_1_->ListCerts(
      base::BindOnce(&SwapCertLists, base::Unretained(&user_1_certlist)));
  db_2_->ListCerts(
      base::BindOnce(&SwapCertLists, base::Unretained(&user_2_certlist)));

  // Run the message loop so the observer notifications get processed and
  // lookups are completed.
  RunUntilIdle();
  // TODO(mattm): ImportServerCert doesn't actually cause any observers to
  // fire. Is that correct?
  EXPECT_EQ(0, db_changed_count_);

  EXPECT_TRUE(IsCertInCertificateList(certs_1[0].get(), user_1_certlist));
  EXPECT_FALSE(IsCertInCertificateList(certs_1[0].get(), user_2_certlist));

  EXPECT_TRUE(IsCertInCertificateList(certs_2[0].get(), user_2_certlist));
  EXPECT_FALSE(IsCertInCertificateList(certs_2[0].get(), user_1_certlist));
}

// Tests that There is no crash if the database is deleted while ListCerts
// is being processed on the worker pool.
TEST_F(NSSCertDatabaseChromeOSTest, NoCrashIfShutdownBeforeDoneOnWorkerPool) {
  ScopedCERTCertificateList certlist;
  db_1_->ListCerts(base::BindOnce(&SwapCertLists, base::Unretained(&certlist)));
  EXPECT_EQ(0U, certlist.size());

  db_1_.reset();

  RunUntilIdle();

  EXPECT_LT(0U, certlist.size());
}

TEST_F(NSSCertDatabaseChromeOSTest, ListCertsReadsSystemSlot) {
  scoped_refptr<X509Certificate> cert_1(
      ImportClientCertAndKeyFromFile(GetTestCertsDirectory(),
                                     "client_1.pem",
                                     "client_1.pk8",
                                     db_1_->GetPublicSlot().get()));

  scoped_refptr<X509Certificate> cert_2(
      ImportClientCertAndKeyFromFile(GetTestCertsDirectory(),
                                     "client_2.pem",
                                     "client_2.pk8",
                                     db_1_->GetSystemSlot().get()));

  ScopedCERTCertificateList certs;
  db_1_->ListCerts(base::BindOnce(&SwapCertLists, base::Unretained(&certs)));
  RunUntilIdle();
  EXPECT_TRUE(IsCertInCertificateList(cert_1.get(), certs));
  EXPECT_TRUE(IsCertInCertificateList(cert_2.get(), certs));
}

TEST_F(NSSCertDatabaseChromeOSTest, ListCertsDoesNotCrossReadSystemSlot) {
  scoped_refptr<X509Certificate> cert_1(
      ImportClientCertAndKeyFromFile(GetTestCertsDirectory(),
                                     "client_1.pem",
                                     "client_1.pk8",
                                     db_2_->GetPublicSlot().get()));

  scoped_refptr<X509Certificate> cert_2(
      ImportClientCertAndKeyFromFile(GetTestCertsDirectory(),
                                     "client_2.pem",
                                     "client_2.pk8",
                                     system_db_.slot()));
  ScopedCERTCertificateList certs;
  db_2_->ListCerts(base::BindOnce(&SwapCertLists, base::Unretained(&certs)));
  RunUntilIdle();
  EXPECT_TRUE(IsCertInCertificateList(cert_1.get(), certs));
  EXPECT_FALSE(IsCertInCertificateList(cert_2.get(), certs));
}

}  // namespace net
