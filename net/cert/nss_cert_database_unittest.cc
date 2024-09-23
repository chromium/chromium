// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/nss_cert_database.h"

#include <cert.h>
#include <certdb.h>
#include <pk11pub.h>
#include <seccomon.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/mozilla_security_manager/nsNSSCertificateDB.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

std::string GetSubjectCN(CERTCertificate* cert) {
  char* cn = CERT_GetCommonName(&cert->subject);
  std::string s = cn;
  PORT_Free(cn);
  return s;
}

bool GetCertIsPerm(const CERTCertificate* cert) {
  PRBool is_perm;
  CHECK_EQ(CERT_GetCertIsPerm(cert, &is_perm), SECSuccess);
  return is_perm != PR_FALSE;
}

const NSSCertDatabase::CertInfo* FindCertInfoForCert(
    const NSSCertDatabase::CertInfoList& cert_info_list,
    CERTCertificate* target_cert) {
  for (const auto& c : cert_info_list) {
    if (x509_util::IsSameCertificate(c.cert.get(), target_cert)) {
      return &c;
    }
  }
  return nullptr;
}

class MockCertDatabaseObserver : public CertDatabase::Observer {
 public:
  MockCertDatabaseObserver() { CertDatabase::GetInstance()->AddObserver(this); }

  ~MockCertDatabaseObserver() override {
    CertDatabase::GetInstance()->RemoveObserver(this);
  }

  void OnTrustStoreChanged() override { trust_store_changes_++; }

  void OnClientCertStoreChanged() override { client_cert_store_changes_++; }

  int trust_store_changes_ = 0;
  int client_cert_store_changes_ = 0;
};

class MockNSSCertDatabaseObserver : public NSSCertDatabase::Observer {
 public:
  explicit MockNSSCertDatabaseObserver(NSSCertDatabase* nss_cert_database)
      : nss_cert_database_(nss_cert_database) {
    nss_cert_database_->AddObserver(this);
  }

  ~MockNSSCertDatabaseObserver() override {
    nss_cert_database_->RemoveObserver(this);
  }

  void OnTrustStoreChanged() override { trust_store_changes_++; }

  void OnClientCertStoreChanged() override { client_cert_store_changes_++; }

  int trust_store_changes() const {
    // Also check that the NSSCertDatabase notifications were mirrored to the
    // CertDatabase observers.
    EXPECT_EQ(global_db_observer_.trust_store_changes_, trust_store_changes_);

    return trust_store_changes_;
  }

  int client_cert_store_changes() const {
    // Also check that the NSSCertDatabase notifications were mirrored to the
    // CertDatabase observers.
    EXPECT_EQ(global_db_observer_.client_cert_store_changes_,
              client_cert_store_changes_);

    return client_cert_store_changes_;
  }

  int all_changes() const {
    return trust_store_changes() + client_cert_store_changes();
  }

 private:
  raw_ptr<NSSCertDatabase> nss_cert_database_;
  MockCertDatabaseObserver global_db_observer_;
  int trust_store_changes_ = 0;
  int client_cert_store_changes_ = 0;
};

}  // namespace

class CertDatabaseNSSTest : public TestWithTaskEnvironment {
 public:
  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());
    cert_db_ = std::make_unique<NSSCertDatabase>(
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* public slot */,
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* private slot */);
    observer_ = std::make_unique<MockNSSCertDatabaseObserver>(cert_db_.get());
    public_slot_ = cert_db_->GetPublicSlot();
    crl_set_ = CRLSet::BuiltinCRLSet();

    // Test db should be empty at start of test.
    EXPECT_EQ(0U, ListCerts().size());
  }

  void TearDown() override {
    // Run the message loop to process any observer callbacks (e.g. for the
    // ClientSocketFactory singleton) so that the scoped ref ptrs created in
    // NSSCertDatabase::NotifyObservers* get released.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  PK11SlotInfo* GetPublicSlot() { return public_slot_.get(); }

  static std::string ReadTestFile(const std::string& name) {
    std::string result;
    base::FilePath cert_path = GetTestCertsDirectory().AppendASCII(name);
    EXPECT_TRUE(base::ReadFileToString(cert_path, &result));
    return result;
  }

  static bool ReadCertIntoList(const std::string& name,
                               ScopedCERTCertificateList* certs) {
    ScopedCERTCertificate cert =
        ImportCERTCertificateFromFile(GetTestCertsDirectory(), name);
    if (!cert)
      return false;

    certs->push_back(std::move(cert));
    return true;
  }

  ScopedCERTCertificateList ListCerts() {
    ScopedCERTCertificateList result;
    crypto::ScopedCERTCertList cert_list(
        PK11_ListCertsInSlot(test_nssdb_.slot()));
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

  std::unique_ptr<NSSCertDatabase> cert_db_;
  std::unique_ptr<MockNSSCertDatabaseObserver> observer_;
  crypto::ScopedTestNSSDB test_nssdb_;
  crypto::ScopedPK11Slot public_slot_;
  scoped_refptr<CRLSet> crl_set_;
};

TEST_F(CertDatabaseNSSTest, ListCerts) {
  // This test isn't terribly useful, though it might help with memory
  // leak tests.
  base::test::TestFuture<ScopedCERTCertificateList> future;
  cert_db_->ListCerts(future.GetCallback());

  ScopedCERTCertificateList certs = future.Take();
  // The test DB is empty, but let's assume there will always be something in
  // the other slots.
  EXPECT_LT(0U, certs.size());
}

TEST_F(CertDatabaseNSSTest, ListCertsInfo) {
  // Since ListCertsInfo queries all the "permanent" certs NSS knows about,
  // including NSS builtin trust anchors and any locally installed certs of the
  // user running the test, it's hard to do really precise testing here. Try to
  // do some general testing as well as testing that a cert added through
  // ScopedTestNSSDB is handled properly.

  // Load a test certificate
  ScopedCERTCertificateList test_root_certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, test_root_certs.size());
  // Should be only a temp certificate at this point, and thus not be returned
  // in the listed certs.
  EXPECT_FALSE(GetCertIsPerm(test_root_certs[0].get()));

  // Get lists of all certs both including and excluding NSS roots.
  NSSCertDatabase::CertInfoList certs_including_nss;
  NSSCertDatabase::CertInfoList certs_excluding_nss;
  {
    base::test::TestFuture<NSSCertDatabase::CertInfoList> future;
    cert_db_->ListCertsInfo(future.GetCallback(),
                            NSSCertDatabase::NSSRootsHandling::kInclude);
    certs_including_nss = future.Take();
  }
  {
    base::test::TestFuture<NSSCertDatabase::CertInfoList> future;
    cert_db_->ListCertsInfo(future.GetCallback(),
                            NSSCertDatabase::NSSRootsHandling::kExclude);
    certs_excluding_nss = future.Take();
  }

  // The tests based on GetAnNssSslTrustedBuiltinRoot could be flaky in obscure
  // local configurations (if the user running the test has manually imported
  // the same certificate into their user NSS DB.) Oh well.
  ScopedCERTCertificate nss_root = GetAnNssBuiltinSslTrustedRoot();
  // (Also this will fail if we ever do the "don't load libnssckbi.so" thing.)
  ASSERT_TRUE(nss_root);
  {
    const NSSCertDatabase::CertInfo* nss_root_info =
        FindCertInfoForCert(certs_including_nss, nss_root.get());
    ASSERT_TRUE(nss_root_info);
    EXPECT_TRUE(nss_root_info->web_trust_anchor);
    EXPECT_FALSE(nss_root_info->untrusted);
    EXPECT_FALSE(nss_root_info->device_wide);
    EXPECT_FALSE(nss_root_info->hardware_backed);
    EXPECT_TRUE(nss_root_info->on_read_only_slot);
  }
  EXPECT_FALSE(FindCertInfoForCert(certs_excluding_nss, nss_root.get()));

  // Test root cert should not be in the lists retrieved before it was imported.
  EXPECT_FALSE(
      FindCertInfoForCert(certs_including_nss, test_root_certs[0].get()));
  EXPECT_FALSE(
      FindCertInfoForCert(certs_excluding_nss, test_root_certs[0].get()));

  // Import the NSS root into the test DB.
  SECStatus srv =
      PK11_ImportCert(test_nssdb_.slot(), nss_root.get(), CK_INVALID_HANDLE,
                      net::x509_util::GetDefaultUniqueNickname(
                          nss_root.get(), net::CA_CERT, test_nssdb_.slot())
                          .c_str(),
                      PR_FALSE /* includeTrust (unused) */);
  ASSERT_EQ(SECSuccess, srv);

  // Import test certificate to the test DB.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(test_root_certs,
                                      NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  // Get new lists of all certs both including and excluding NSS roots, which
  // should now also include the test db certificates.
  NSSCertDatabase::CertInfoList certs_including_nss_with_local;
  NSSCertDatabase::CertInfoList certs_excluding_nss_with_local;
  {
    base::test::TestFuture<NSSCertDatabase::CertInfoList> future;
    cert_db_->ListCertsInfo(future.GetCallback(),
                            NSSCertDatabase::NSSRootsHandling::kInclude);
    certs_including_nss_with_local = future.Take();
  }
  {
    base::test::TestFuture<NSSCertDatabase::CertInfoList> future;
    cert_db_->ListCertsInfo(future.GetCallback(),
                            NSSCertDatabase::NSSRootsHandling::kExclude);
    certs_excluding_nss_with_local = future.Take();
  }

  // After adding the certs to the test db, the number certs returned should be
  // 1 more than before in kInclude and and 2 more in kExclude cases.
  EXPECT_EQ(certs_including_nss_with_local.size(),
            1 + certs_including_nss.size());
  EXPECT_EQ(certs_excluding_nss_with_local.size(),
            2 + certs_excluding_nss.size());

  // Using kExclude should give a smaller number of results than kInclude.
  // (Although this would be wrong if we ever do the "don't load libnssckbi.so"
  // thing.)
  EXPECT_LT(certs_excluding_nss_with_local.size(),
            certs_including_nss_with_local.size());

  // The NSS root that was imported to the test db should be in both lists now.
  {
    const NSSCertDatabase::CertInfo* nss_root_info =
        FindCertInfoForCert(certs_including_nss_with_local, nss_root.get());
    ASSERT_TRUE(nss_root_info);
    EXPECT_TRUE(nss_root_info->web_trust_anchor);
    EXPECT_FALSE(nss_root_info->untrusted);
    EXPECT_FALSE(nss_root_info->device_wide);
    EXPECT_FALSE(nss_root_info->hardware_backed);
    // `on_read_only_slot` is not tested here as the way it is calculated could
    // be potentially flaky if the cert exists on both a readonly and
    // non-readonly slot.
  }
  {
    const NSSCertDatabase::CertInfo* nss_root_info =
        FindCertInfoForCert(certs_excluding_nss_with_local, nss_root.get());
    ASSERT_TRUE(nss_root_info);
    EXPECT_FALSE(nss_root_info->web_trust_anchor);
    EXPECT_TRUE(nss_root_info->untrusted);
    EXPECT_FALSE(nss_root_info->device_wide);
    EXPECT_FALSE(nss_root_info->hardware_backed);
    // `on_read_only_slot` is not tested here as the way it is calculated could
    // be potentially flaky if the cert exists on both a readonly and
    // non-readonly slot.
  }

  // Ensure the test root cert is present in the lists retrieved after it was
  // imported, and that the info returned is as expected.
  {
    const NSSCertDatabase::CertInfo* test_cert_info = FindCertInfoForCert(
        certs_including_nss_with_local, test_root_certs[0].get());
    ASSERT_TRUE(test_cert_info);
    EXPECT_TRUE(test_cert_info->web_trust_anchor);
    EXPECT_FALSE(test_cert_info->untrusted);
    EXPECT_FALSE(test_cert_info->device_wide);
    EXPECT_FALSE(test_cert_info->hardware_backed);
    EXPECT_FALSE(test_cert_info->on_read_only_slot);
  }
  {
    const NSSCertDatabase::CertInfo* test_cert_info = FindCertInfoForCert(
        certs_excluding_nss_with_local, test_root_certs[0].get());
    ASSERT_TRUE(test_cert_info);
    EXPECT_TRUE(test_cert_info->web_trust_anchor);
    EXPECT_FALSE(test_cert_info->untrusted);
    EXPECT_FALSE(test_cert_info->device_wide);
    EXPECT_FALSE(test_cert_info->hardware_backed);
    EXPECT_FALSE(test_cert_info->on_read_only_slot);
  }
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12WrongPassword) {
  std::string pkcs12_data = ReadTestFile("client.p12");

  EXPECT_EQ(
      ERR_PKCS12_IMPORT_BAD_PASSWORD,
      cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data, std::u16string(),
                                 true,  // is_extractable
                                 nullptr));

  // Test db should still be empty.
  EXPECT_EQ(0U, ListCerts().size());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->all_changes());
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12AsExtractableAndExportAgain) {
  std::string pkcs12_data = ReadTestFile("client.p12");

  EXPECT_EQ(OK,
            cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data, u"12345",
                                       true,  // is_extractable
                                       nullptr));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer_->client_cert_store_changes());
  EXPECT_EQ(0, observer_->trust_store_changes());

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  EXPECT_EQ("testusercert", GetSubjectCN(cert_list[0].get()));

  // TODO(mattm): move export test to separate test case?
  std::string exported_data;
  EXPECT_EQ(1,
            cert_db_->ExportToPKCS12(cert_list, u"exportpw", &exported_data));
  ASSERT_LT(0U, exported_data.size());
  // TODO(mattm): further verification of exported data?

  base::RunLoop().RunUntilIdle();
  // Exporting should not cause an observer notification.
  EXPECT_EQ(1, observer_->all_changes());
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12Twice) {
  std::string pkcs12_data = ReadTestFile("client.p12");

  EXPECT_EQ(OK,
            cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data, u"12345",
                                       true,  // is_extractable
                                       nullptr));
  EXPECT_EQ(1U, ListCerts().size());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer_->client_cert_store_changes());
  EXPECT_EQ(0, observer_->trust_store_changes());

  // NSS has a SEC_ERROR_PKCS12_DUPLICATE_DATA error, but it doesn't look like
  // it's ever used.  This test verifies that.
  EXPECT_EQ(OK,
            cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data, u"12345",
                                       true,  // is_extractable
                                       nullptr));
  EXPECT_EQ(1U, ListCerts().size());

  base::RunLoop().RunUntilIdle();
  // Theoretically it should not send another notification for re-importing the
  // same thing, but probably not worth the effort to try to detect this case.
  EXPECT_EQ(2, observer_->client_cert_store_changes());
  EXPECT_EQ(0, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12AsUnextractableAndExportAgain) {
  std::string pkcs12_data = ReadTestFile("client.p12");

  EXPECT_EQ(OK,
            cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data, u"12345",
                                       false,  // is_extractable
                                       nullptr));

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  EXPECT_EQ("testusercert", GetSubjectCN(cert_list[0].get()));

  std::string exported_data;
  EXPECT_EQ(0,
            cert_db_->ExportToPKCS12(cert_list, u"exportpw", &exported_data));
}

// Importing a PKCS#12 file with a certificate but no corresponding
// private key should not mark an existing private key as unextractable.
TEST_F(CertDatabaseNSSTest, ImportFromPKCS12OnlyMarkIncludedKey) {
  std::string pkcs12_data = ReadTestFile("client.p12");
  EXPECT_EQ(OK,
            cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data, u"12345",
                                       true,  // is_extractable
                                       nullptr));

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());

  // Now import a PKCS#12 file with just a certificate but no private key.
  pkcs12_data = ReadTestFile("client-nokey.p12");
  EXPECT_EQ(OK,
            cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data, u"12345",
                                       false,  // is_extractable
                                       nullptr));

  cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());

  // Make sure the imported private key is still extractable.
  std::string exported_data;
  EXPECT_EQ(1,
            cert_db_->ExportToPKCS12(cert_list, u"exportpw", &exported_data));
  ASSERT_LT(0U, exported_data.size());
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12InvalidFile) {
  std::string pkcs12_data = "Foobarbaz";

  EXPECT_EQ(
      ERR_PKCS12_IMPORT_INVALID_FILE,
      cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data, std::u16string(),
                                 true,  // is_extractable
                                 nullptr));

  // Test db should still be empty.
  EXPECT_EQ(0U, ListCerts().size());
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12EmptyPassword) {
  std::string pkcs12_data = ReadTestFile("client-empty-password.p12");

  EXPECT_EQ(OK, cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data,
                                           std::u16string(),
                                           true,  // is_extractable
                                           nullptr));
  EXPECT_EQ(1U, ListCerts().size());
}

TEST_F(CertDatabaseNSSTest, ImportFromPKCS12NullPassword) {
  std::string pkcs12_data = ReadTestFile("client-null-password.p12");

  EXPECT_EQ(OK, cert_db_->ImportFromPKCS12(GetPublicSlot(), pkcs12_data,
                                           std::u16string(),
                                           true,  // is_extractable
                                           nullptr));
  EXPECT_EQ(1U, ListCerts().size());
}

TEST_F(CertDatabaseNSSTest, ImportCACert_SSLTrust) {
  ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());
  EXPECT_FALSE(GetCertIsPerm(certs[0].get()));

  // Import it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(certs, NSSCertDatabase::TRUSTED_SSL,
                                      &failed));

  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  CERTCertificate* cert = cert_list[0].get();
  EXPECT_EQ("Test Root CA", GetSubjectCN(cert));

  EXPECT_EQ(NSSCertDatabase::TRUSTED_SSL,
            cert_db_->GetCertTrust(cert, CA_CERT));

  EXPECT_EQ(
      unsigned(CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA),
      cert->trust->sslFlags);
  EXPECT_EQ(unsigned(CERTDB_VALID_CA), cert->trust->emailFlags);
  EXPECT_EQ(unsigned(CERTDB_VALID_CA), cert->trust->objectSigningFlags);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  EXPECT_EQ(1, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportCACert_EmailTrust) {
  ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());
  EXPECT_FALSE(GetCertIsPerm(certs[0].get()));

  // Import it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(certs, NSSCertDatabase::TRUSTED_EMAIL,
                                      &failed));

  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  CERTCertificate* cert = cert_list[0].get();
  EXPECT_EQ("Test Root CA", GetSubjectCN(cert));

  EXPECT_EQ(NSSCertDatabase::TRUSTED_EMAIL,
            cert_db_->GetCertTrust(cert, CA_CERT));

  EXPECT_EQ(unsigned(CERTDB_VALID_CA), cert->trust->sslFlags);
  EXPECT_EQ(
      unsigned(CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA),
      cert->trust->emailFlags);
  EXPECT_EQ(unsigned(CERTDB_VALID_CA), cert->trust->objectSigningFlags);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  // Theoretically we could avoid notifying for changes that aren't relevant
  // for server auth, but probably not worth the effort.
  EXPECT_EQ(1, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportCACert_ObjSignTrust) {
  ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());
  EXPECT_FALSE(GetCertIsPerm(certs[0].get()));

  // Import it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(certs, NSSCertDatabase::TRUSTED_OBJ_SIGN,
                                      &failed));

  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  CERTCertificate* cert = cert_list[0].get();
  EXPECT_EQ("Test Root CA", GetSubjectCN(cert));

  EXPECT_EQ(NSSCertDatabase::TRUSTED_OBJ_SIGN,
            cert_db_->GetCertTrust(cert, CA_CERT));

  EXPECT_EQ(unsigned(CERTDB_VALID_CA), cert->trust->sslFlags);
  EXPECT_EQ(unsigned(CERTDB_VALID_CA), cert->trust->emailFlags);
  EXPECT_EQ(
      unsigned(CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA),
      cert->trust->objectSigningFlags);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  // Theoretically we could avoid notifying for changes that aren't relevant
  // for server auth, but probably not worth the effort.
  EXPECT_EQ(1, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportCA_NotCACert) {
  ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());
  EXPECT_FALSE(GetCertIsPerm(certs[0].get()));

  // Import it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(certs, NSSCertDatabase::TRUSTED_SSL,
                                      &failed));
  ASSERT_EQ(1U, failed.size());
  // Note: this compares pointers directly.  It's okay in this case because
  // ImportCACerts returns the same pointers that were passed in.  In the
  // general case x509_util::CryptoBufferEqual should be used.
  EXPECT_EQ(certs[0], failed[0].certificate);
  EXPECT_THAT(failed[0].net_error, IsError(ERR_IMPORT_CA_CERT_NOT_CA));

  EXPECT_EQ(0U, ListCerts().size());
}

TEST_F(CertDatabaseNSSTest, ImportCACertHierarchy) {
  ScopedCERTCertificateList certs;
  ASSERT_TRUE(ReadCertIntoList("multi-root-D-by-D.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-C-by-D.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-B-by-C.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-A-by-B.pem", &certs));

  // Import it.
  NSSCertDatabase::ImportCertFailureList failed;
  // Have to specify email trust for the cert verification of the child cert to
  // work (see
  // http://mxr.mozilla.org/mozilla/source/security/nss/lib/certhigh/certvfy.c#752
  // "XXX This choice of trustType seems arbitrary.")
  EXPECT_TRUE(cert_db_->ImportCACerts(
      certs, NSSCertDatabase::TRUSTED_SSL | NSSCertDatabase::TRUSTED_EMAIL,
      &failed));

  ASSERT_EQ(1U, failed.size());
  EXPECT_EQ("127.0.0.1", GetSubjectCN(failed[0].certificate.get()));
  EXPECT_THAT(failed[0].net_error, IsError(ERR_IMPORT_CA_CERT_NOT_CA));

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(3U, cert_list.size());
  EXPECT_EQ("B CA - Multi-root", GetSubjectCN(cert_list[0].get()));
  EXPECT_EQ("D Root CA - Multi-root", GetSubjectCN(cert_list[1].get()));
  EXPECT_EQ("C CA - Multi-root", GetSubjectCN(cert_list[2].get()));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  EXPECT_EQ(1, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportCACertHierarchyDupeRoot) {
  ScopedCERTCertificateList certs;
  ASSERT_TRUE(ReadCertIntoList("multi-root-D-by-D.pem", &certs));

  // First import just the root.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(
      certs, NSSCertDatabase::TRUSTED_SSL | NSSCertDatabase::TRUSTED_EMAIL,
      &failed));

  EXPECT_EQ(0U, failed.size());
  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  EXPECT_EQ("D Root CA - Multi-root", GetSubjectCN(cert_list[0].get()));

  ASSERT_TRUE(ReadCertIntoList("multi-root-C-by-D.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-B-by-C.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-A-by-B.pem", &certs));

  // Now import with the other certs in the list too.  Even though the root is
  // already present, we should still import the rest.
  failed.clear();
  EXPECT_TRUE(cert_db_->ImportCACerts(
      certs, NSSCertDatabase::TRUSTED_SSL | NSSCertDatabase::TRUSTED_EMAIL,
      &failed));

  ASSERT_EQ(2U, failed.size());
  EXPECT_EQ("D Root CA - Multi-root",
            GetSubjectCN(failed[0].certificate.get()));
  EXPECT_THAT(failed[0].net_error, IsError(ERR_IMPORT_CERT_ALREADY_EXISTS));
  EXPECT_EQ("127.0.0.1", GetSubjectCN(failed[1].certificate.get()));
  EXPECT_THAT(failed[1].net_error, IsError(ERR_IMPORT_CA_CERT_NOT_CA));

  cert_list = ListCerts();
  ASSERT_EQ(3U, cert_list.size());
  EXPECT_EQ("B CA - Multi-root", GetSubjectCN(cert_list[0].get()));
  EXPECT_EQ("D Root CA - Multi-root", GetSubjectCN(cert_list[1].get()));
  EXPECT_EQ("C CA - Multi-root", GetSubjectCN(cert_list[2].get()));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  EXPECT_EQ(2, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportCACertHierarchyUntrusted) {
  ScopedCERTCertificateList certs;
  ASSERT_TRUE(ReadCertIntoList("multi-root-D-by-D.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-C-by-D.pem", &certs));

  // Import it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(certs, NSSCertDatabase::TRUST_DEFAULT,
                                      &failed));

  ASSERT_EQ(1U, failed.size());
  EXPECT_EQ("C CA - Multi-root", GetSubjectCN(failed[0].certificate.get()));
  // TODO(mattm): should check for net error equivalent of
  // SEC_ERROR_UNTRUSTED_ISSUER
  EXPECT_THAT(failed[0].net_error, IsError(ERR_FAILED));

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  EXPECT_EQ("D Root CA - Multi-root", GetSubjectCN(cert_list[0].get()));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  // We generate a notification even if not trusting the root. The certs could
  // still affect trust decisions by affecting path building.
  EXPECT_EQ(1, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportCACertHierarchyTree) {
  ScopedCERTCertificateList certs;
  ASSERT_TRUE(ReadCertIntoList("multi-root-E-by-E.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-C-by-E.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-F-by-E.pem", &certs));

  // Import it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(
      certs, NSSCertDatabase::TRUSTED_SSL | NSSCertDatabase::TRUSTED_EMAIL,
      &failed));

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(3U, cert_list.size());
  EXPECT_EQ("F CA - Multi-root", GetSubjectCN(cert_list[0].get()));
  EXPECT_EQ("C CA - Multi-root", GetSubjectCN(cert_list[1].get()));
  EXPECT_EQ("E Root CA - Multi-root", GetSubjectCN(cert_list[2].get()));
}

TEST_F(CertDatabaseNSSTest, ImportCACertNotHierarchy) {
  ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());
  ASSERT_TRUE(ReadCertIntoList("multi-root-F-by-E.pem", &certs));
  ASSERT_TRUE(ReadCertIntoList("multi-root-C-by-E.pem", &certs));

  // Import it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(
      certs, NSSCertDatabase::TRUSTED_SSL | NSSCertDatabase::TRUSTED_EMAIL,
      &failed));

  ASSERT_EQ(2U, failed.size());
  // TODO(mattm): should check for net error equivalent of
  // SEC_ERROR_UNKNOWN_ISSUER
  EXPECT_EQ("F CA - Multi-root", GetSubjectCN(failed[0].certificate.get()));
  EXPECT_THAT(failed[0].net_error, IsError(ERR_FAILED));
  EXPECT_EQ("C CA - Multi-root", GetSubjectCN(failed[1].certificate.get()));
  EXPECT_THAT(failed[1].net_error, IsError(ERR_FAILED));

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  EXPECT_EQ("Test Root CA", GetSubjectCN(cert_list[0].get()));
}

// Test importing a server cert + chain to the NSS DB with default trust. After
// importing, all the certs should be found in the DB and should have default
// trust flags.
TEST_F(CertDatabaseNSSTest, ImportServerCert) {
  // Import the server and its chain.
  ScopedCERTCertificateList certs_to_import;
  ASSERT_TRUE(
      ReadCertIntoList("ok_cert_by_intermediate.pem", &certs_to_import));
  ASSERT_TRUE(ReadCertIntoList("intermediate_ca_cert.pem", &certs_to_import));
  ASSERT_TRUE(ReadCertIntoList("root_ca_cert.pem", &certs_to_import));

  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportServerCert(
      certs_to_import, NSSCertDatabase::TRUST_DEFAULT, &failed));
  EXPECT_EQ(0U, failed.size());

  // All the certs in the imported list should now be found in the NSS DB.
  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(3U, cert_list.size());
  CERTCertificate* found_server_cert = nullptr;
  CERTCertificate* found_intermediate_cert = nullptr;
  CERTCertificate* found_root_cert = nullptr;
  for (const auto& cert : cert_list) {
    if (GetSubjectCN(cert.get()) == "127.0.0.1")
      found_server_cert = cert.get();
    else if (GetSubjectCN(cert.get()) == "Test Intermediate CA")
      found_intermediate_cert = cert.get();
    else if (GetSubjectCN(cert.get()) == "Test Root CA")
      found_root_cert = cert.get();
  }
  ASSERT_TRUE(found_server_cert);
  ASSERT_TRUE(found_intermediate_cert);
  ASSERT_TRUE(found_root_cert);

  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(found_server_cert, SERVER_CERT));
  EXPECT_EQ(0U, found_server_cert->trust->sslFlags);
  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(found_intermediate_cert, CA_CERT));
  EXPECT_EQ(0U, found_intermediate_cert->trust->sslFlags);
  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(found_root_cert, CA_CERT));
  EXPECT_EQ(0U, found_root_cert->trust->sslFlags);

  // Verification fails, as the intermediate & CA certs are imported without
  // trust.
  scoped_refptr<X509Certificate> x509_found_server_cert =
      x509_util::CreateX509CertificateFromCERTCertificate(found_server_cert);
  ASSERT_TRUE(x509_found_server_cert);
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_found_server_cert.get(), "127.0.0.1",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  EXPECT_EQ(0, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportServerCert_SelfSigned) {
  ScopedCERTCertificateList certs;
  ASSERT_TRUE(ReadCertIntoList("punycodetest.pem", &certs));

  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::TRUST_DEFAULT,
                                         &failed));

  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  CERTCertificate* puny_cert = cert_list[0].get();

  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(puny_cert, SERVER_CERT));
  EXPECT_EQ(0U, puny_cert->trust->sslFlags);

  scoped_refptr<X509Certificate> x509_puny_cert =
      x509_util::CreateX509CertificateFromCERTCertificate(puny_cert);
  ASSERT_TRUE(x509_puny_cert);
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_puny_cert.get(), "xn--wgv71a119e.com",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  EXPECT_EQ(0, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportServerCert_SelfSigned_Trusted) {
  ScopedCERTCertificateList certs;
  ASSERT_TRUE(ReadCertIntoList("punycodetest.pem", &certs));

  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::TRUSTED_SSL,
                                         &failed));

  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList cert_list = ListCerts();
  ASSERT_EQ(1U, cert_list.size());
  CERTCertificate* puny_cert = cert_list[0].get();

  EXPECT_EQ(NSSCertDatabase::TRUSTED_SSL,
            cert_db_->GetCertTrust(puny_cert, SERVER_CERT));
  EXPECT_EQ(unsigned(CERTDB_TRUSTED | CERTDB_TERMINAL_RECORD),
            puny_cert->trust->sslFlags);

  scoped_refptr<X509Certificate> x509_puny_cert =
      x509_util::CreateX509CertificateFromCERTCertificate(puny_cert);
  ASSERT_TRUE(x509_puny_cert);
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_puny_cert.get(), "xn--wgv71a119e.com",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  // TODO(mattm): this should be 1, but ImportServerCert doesn't currently
  // generate notifications.
  EXPECT_EQ(0, observer_->trust_store_changes());
}

TEST_F(CertDatabaseNSSTest, ImportCaAndServerCert) {
  ScopedCERTCertificateList ca_certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_certs.size());

  // Import CA cert and trust it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(ca_certs, NSSCertDatabase::TRUSTED_SSL,
                                      &failed));
  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());

  // Import server cert with default trust.
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::TRUST_DEFAULT,
                                         &failed));
  EXPECT_EQ(0U, failed.size());

  // Server cert should verify.
  scoped_refptr<X509Certificate> x509_server_cert =
      x509_util::CreateX509CertificateFromCERTCertificate(certs[0].get());
  ASSERT_TRUE(x509_server_cert);
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_server_cert.get(), "127.0.0.1",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
}

TEST_F(CertDatabaseNSSTest, ImportCaAndServerCert_DistrustServer) {
  ScopedCERTCertificateList ca_certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "root_ca_cert.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_certs.size());

  // Import CA cert and trust it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(ca_certs, NSSCertDatabase::TRUSTED_SSL,
                                      &failed));
  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());

  // Import server cert without inheriting trust from issuer (explicit
  // distrust).
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::DISTRUSTED_SSL,
                                         &failed));
  EXPECT_EQ(0U, failed.size());
  EXPECT_EQ(NSSCertDatabase::DISTRUSTED_SSL,
            cert_db_->GetCertTrust(certs[0].get(), SERVER_CERT));

  EXPECT_EQ(unsigned(CERTDB_TERMINAL_RECORD), certs[0]->trust->sslFlags);

  // Server cert should fail to verify.
  scoped_refptr<X509Certificate> x509_server_cert =
      x509_util::CreateX509CertificateFromCERTCertificate(certs[0].get());
  ASSERT_TRUE(x509_server_cert);
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_server_cert.get(), "127.0.0.1",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);
}

TEST_F(CertDatabaseNSSTest, TrustIntermediateCa) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  ScopedCERTCertificateList ca_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get());
  ASSERT_EQ(1U, ca_certs.size());

  // Import Root CA cert and distrust it.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportCACerts(ca_certs, NSSCertDatabase::DISTRUSTED_SSL,
                                      &failed));
  EXPECT_EQ(0U, failed.size());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  EXPECT_EQ(1, observer_->trust_store_changes());

  ScopedCERTCertificateList intermediate_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          intermediate->GetX509Certificate().get());
  ASSERT_EQ(1U, intermediate_certs.size());

  // Import Intermediate CA cert and trust it.
  EXPECT_TRUE(cert_db_->ImportCACerts(intermediate_certs,
                                      NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer_->client_cert_store_changes());
  EXPECT_EQ(2, observer_->trust_store_changes());

  scoped_refptr<X509Certificate> x509_server_cert = leaf->GetX509Certificate();
  ScopedCERTCertificateList certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          x509_server_cert.get());
  ASSERT_EQ(1U, certs.size());

  // Import server cert with default trust.
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::TRUST_DEFAULT,
                                         &failed));
  EXPECT_EQ(0U, failed.size());
  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(certs[0].get(), SERVER_CERT));

  // Server cert should verify.
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_server_cert.get(), "www.example.com",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Trust the root cert and distrust the intermediate.
  EXPECT_TRUE(cert_db_->SetCertTrust(
      ca_certs[0].get(), CA_CERT, NSSCertDatabase::TRUSTED_SSL));
  EXPECT_TRUE(cert_db_->SetCertTrust(
      intermediate_certs[0].get(), CA_CERT, NSSCertDatabase::DISTRUSTED_SSL));
  EXPECT_EQ(
      unsigned(CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_TRUSTED_CLIENT_CA),
      ca_certs[0]->trust->sslFlags);
  EXPECT_EQ(unsigned(CERTDB_VALID_CA), ca_certs[0]->trust->emailFlags);
  EXPECT_EQ(unsigned(CERTDB_VALID_CA), ca_certs[0]->trust->objectSigningFlags);
  EXPECT_EQ(unsigned(CERTDB_TERMINAL_RECORD),
            intermediate_certs[0]->trust->sslFlags);
  EXPECT_EQ(unsigned(CERTDB_VALID_CA),
            intermediate_certs[0]->trust->emailFlags);
  EXPECT_EQ(unsigned(CERTDB_VALID_CA),
            intermediate_certs[0]->trust->objectSigningFlags);

  // Server cert should fail to verify.
  CertVerifyResult verify_result2;
  error = verify_proc->Verify(x509_server_cert.get(), "www.example.com",
                              /*ocsp_response=*/std::string(),
                              /*sct_list=*/std::string(), flags,
                              &verify_result2, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result2.cert_status);
}

TEST_F(CertDatabaseNSSTest, TrustIntermediateCa2) {
  NSSCertDatabase::ImportCertFailureList failed;
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  ScopedCERTCertificateList intermediate_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          intermediate->GetX509Certificate().get());
  ASSERT_EQ(1U, intermediate_certs.size());

  // Import Intermediate CA cert and trust it.
  EXPECT_TRUE(cert_db_->ImportCACerts(intermediate_certs,
                                      NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  scoped_refptr<X509Certificate> x509_server_cert = leaf->GetX509Certificate();
  ScopedCERTCertificateList certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          x509_server_cert.get());

  // Import server cert with default trust.
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::TRUST_DEFAULT,
                                         &failed));
  EXPECT_EQ(0U, failed.size());
  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(certs[0].get(), SERVER_CERT));

  // Server cert should verify.
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_server_cert.get(), "www.example.com",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Without explicit trust of the intermediate, verification should fail.
  EXPECT_TRUE(cert_db_->SetCertTrust(
      intermediate_certs[0].get(), CA_CERT, NSSCertDatabase::TRUST_DEFAULT));

  // Server cert should fail to verify.
  CertVerifyResult verify_result2;
  error = verify_proc->Verify(x509_server_cert.get(), "www.example.com",
                              /*ocsp_response=*/std::string(),
                              /*sct_list=*/std::string(), flags,
                              &verify_result2, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result2.cert_status);
}

TEST_F(CertDatabaseNSSTest, TrustIntermediateCa3) {
  NSSCertDatabase::ImportCertFailureList failed;
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  ScopedCERTCertificateList ca_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get());
  ASSERT_EQ(1U, ca_certs.size());

  // Import Root CA cert and default trust it.
  EXPECT_TRUE(cert_db_->ImportCACerts(ca_certs, NSSCertDatabase::TRUST_DEFAULT,
                                      &failed));
  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList intermediate_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          intermediate->GetX509Certificate().get());
  ASSERT_EQ(1U, intermediate_certs.size());

  // Import Intermediate CA cert and trust it.
  EXPECT_TRUE(cert_db_->ImportCACerts(intermediate_certs,
                                      NSSCertDatabase::TRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  scoped_refptr<X509Certificate> x509_server_cert = leaf->GetX509Certificate();
  ScopedCERTCertificateList certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          x509_server_cert.get());
  ASSERT_EQ(1U, certs.size());

  // Import server cert with default trust.
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::TRUST_DEFAULT,
                                         &failed));
  EXPECT_EQ(0U, failed.size());
  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(certs[0].get(), SERVER_CERT));

  // Server cert should verify.
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_server_cert.get(), "www.example.com",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Without explicit trust of the intermediate, verification should fail.
  EXPECT_TRUE(cert_db_->SetCertTrust(
      intermediate_certs[0].get(), CA_CERT, NSSCertDatabase::TRUST_DEFAULT));

  // Server cert should fail to verify.
  CertVerifyResult verify_result2;
  error = verify_proc->Verify(x509_server_cert.get(), "www.example.com",
                              /*ocsp_response=*/std::string(),
                              /*sct_list=*/std::string(), flags,
                              &verify_result2, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result2.cert_status);
}

TEST_F(CertDatabaseNSSTest, TrustIntermediateCa4) {
  NSSCertDatabase::ImportCertFailureList failed;
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  ScopedCERTCertificateList ca_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          root->GetX509Certificate().get());
  ASSERT_EQ(1U, ca_certs.size());

  // Import Root CA cert and trust it.
  EXPECT_TRUE(cert_db_->ImportCACerts(ca_certs, NSSCertDatabase::TRUSTED_SSL,
                                      &failed));
  EXPECT_EQ(0U, failed.size());

  ScopedCERTCertificateList intermediate_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          intermediate->GetX509Certificate().get());
  ASSERT_EQ(1U, intermediate_certs.size());

  // Import Intermediate CA cert and distrust it.
  EXPECT_TRUE(cert_db_->ImportCACerts(
      intermediate_certs, NSSCertDatabase::DISTRUSTED_SSL, &failed));
  EXPECT_EQ(0U, failed.size());

  scoped_refptr<X509Certificate> x509_server_cert = leaf->GetX509Certificate();
  ScopedCERTCertificateList certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          x509_server_cert.get());
  ASSERT_EQ(1U, certs.size());

  // Import server cert with default trust.
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::TRUST_DEFAULT,
                                         &failed));
  EXPECT_EQ(0U, failed.size());
  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(certs[0].get(), SERVER_CERT));

  // Server cert should not verify.
  scoped_refptr<CertVerifyProc> verify_proc(
      CertVerifyProc::CreateBuiltinWithChromeRootStore(
          /*cert_net_fetcher=*/nullptr, crl_set_,
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          /*root_store_data=*/nullptr,
          /*instance_params=*/{}, std::nullopt));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(x509_server_cert.get(), "www.example.com",
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);

  // Without explicit distrust of the intermediate, verification should succeed.
  EXPECT_TRUE(cert_db_->SetCertTrust(
      intermediate_certs[0].get(), CA_CERT, NSSCertDatabase::TRUST_DEFAULT));

  // Server cert should verify.
  CertVerifyResult verify_result2;
  error = verify_proc->Verify(x509_server_cert.get(), "www.example.com",
                              /*ocsp_response=*/std::string(),
                              /*sct_list=*/std::string(), flags,
                              &verify_result2, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result2.cert_status);
}

// Importing two certificates with the same issuer and subject common name,
// but overall distinct subject names, should succeed and generate a unique
// nickname for the second certificate.
TEST_F(CertDatabaseNSSTest, ImportDuplicateCommonName) {
  ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "duplicate_cn_1.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());

  EXPECT_EQ(0U, ListCerts().size());

  // Import server cert with default trust.
  NSSCertDatabase::ImportCertFailureList failed;
  EXPECT_TRUE(cert_db_->ImportServerCert(certs, NSSCertDatabase::TRUST_DEFAULT,
                                         &failed));
  EXPECT_EQ(0U, failed.size());
  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(certs[0].get(), SERVER_CERT));

  ScopedCERTCertificateList new_certs = ListCerts();
  ASSERT_EQ(1U, new_certs.size());

  // Now attempt to import a different certificate with the same common name.
  ScopedCERTCertificateList certs2 = CreateCERTCertificateListFromFile(
      GetTestCertsDirectory(), "duplicate_cn_2.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs2.size());

  // Import server cert with default trust.
  EXPECT_TRUE(cert_db_->ImportServerCert(certs2, NSSCertDatabase::TRUST_DEFAULT,
                                         &failed));
  EXPECT_EQ(0U, failed.size());
  EXPECT_EQ(NSSCertDatabase::TRUST_DEFAULT,
            cert_db_->GetCertTrust(certs2[0].get(), SERVER_CERT));

  new_certs = ListCerts();
  ASSERT_EQ(2U, new_certs.size());
  EXPECT_STRNE(new_certs[0]->nickname, new_certs[1]->nickname);
}

}  // namespace net
