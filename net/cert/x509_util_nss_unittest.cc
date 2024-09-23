// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_nss.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

std::string BytesForNSSCert(CERTCertificate* cert) {
  std::string der_encoded;
  if (!x509_util::GetDEREncoded(cert, &der_encoded))
    ADD_FAILURE();
  return der_encoded;
}

}  // namespace

TEST(X509UtilNSSTest, IsSameCertificate) {
  ScopedCERTCertificate google_nss_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(google_nss_cert);

  ScopedCERTCertificate google_nss_cert2(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(google_nss_cert2);

  ScopedCERTCertificate webkit_nss_cert(
      x509_util::CreateCERTCertificateFromBytes(webkit_der));
  ASSERT_TRUE(webkit_nss_cert);

  scoped_refptr<X509Certificate> google_x509_cert(
      X509Certificate::CreateFromBytes(google_der));
  ASSERT_TRUE(google_x509_cert);

  scoped_refptr<X509Certificate> webkit_x509_cert(
      X509Certificate::CreateFromBytes(webkit_der));
  ASSERT_TRUE(webkit_x509_cert);

  EXPECT_TRUE(x509_util::IsSameCertificate(google_nss_cert.get(),
                                           google_nss_cert.get()));
  EXPECT_TRUE(x509_util::IsSameCertificate(google_nss_cert.get(),
                                           google_nss_cert2.get()));
  EXPECT_TRUE(x509_util::IsSameCertificate(google_nss_cert.get(),
                                           google_x509_cert.get()));
  EXPECT_TRUE(x509_util::IsSameCertificate(google_x509_cert.get(),
                                           google_nss_cert.get()));

  EXPECT_TRUE(x509_util::IsSameCertificate(webkit_nss_cert.get(),
                                           webkit_nss_cert.get()));
  EXPECT_TRUE(x509_util::IsSameCertificate(webkit_nss_cert.get(),
                                           webkit_x509_cert.get()));
  EXPECT_TRUE(x509_util::IsSameCertificate(webkit_x509_cert.get(),
                                           webkit_nss_cert.get()));

  EXPECT_FALSE(x509_util::IsSameCertificate(google_nss_cert.get(),
                                            webkit_nss_cert.get()));
  EXPECT_FALSE(x509_util::IsSameCertificate(google_nss_cert.get(),
                                            webkit_x509_cert.get()));
  EXPECT_FALSE(x509_util::IsSameCertificate(google_x509_cert.get(),
                                            webkit_nss_cert.get()));
}

TEST(X509UtilNSSTest, CreateCERTCertificateFromBytes) {
  ScopedCERTCertificate google_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(google_cert);
  EXPECT_STREQ(
      "CN=www.google.com,O=Google Inc,L=Mountain View,ST=California,C=US",
      google_cert->subjectName);
}

TEST(X509UtilNSSTest, CreateCERTCertificateFromBytesGarbage) {
  EXPECT_EQ(nullptr, x509_util::CreateCERTCertificateFromBytes(
                         base::span<const uint8_t>()));

  static const uint8_t garbage_data[] = "garbage";
  EXPECT_EQ(nullptr, x509_util::CreateCERTCertificateFromBytes(garbage_data));
}

TEST(X509UtilNSSTest, CreateCERTCertificateFromX509Certificate) {
  scoped_refptr<X509Certificate> x509_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(x509_cert);
  ScopedCERTCertificate nss_cert =
      x509_util::CreateCERTCertificateFromX509Certificate(x509_cert.get());
  ASSERT_TRUE(nss_cert);
  EXPECT_STREQ("CN=127.0.0.1,O=Test CA,L=Mountain View,ST=California,C=US",
               nss_cert->subjectName);
}

TEST(X509UtilNSSTest, CreateCERTCertificateListFromX509Certificate) {
  scoped_refptr<X509Certificate> x509_cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "multi-root-chain1.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(x509_cert);
  EXPECT_EQ(3U, x509_cert->intermediate_buffers().size());

  ScopedCERTCertificateList nss_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(x509_cert.get());
  ASSERT_EQ(4U, nss_certs.size());
  for (int i = 0; i < 4; ++i)
    ASSERT_TRUE(nss_certs[i]);

  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(x509_cert->cert_buffer()),
            BytesForNSSCert(nss_certs[0].get()));
  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(
                x509_cert->intermediate_buffers()[0].get()),
            BytesForNSSCert(nss_certs[1].get()));
  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(
                x509_cert->intermediate_buffers()[1].get()),
            BytesForNSSCert(nss_certs[2].get()));
  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(
                x509_cert->intermediate_buffers()[2].get()),
            BytesForNSSCert(nss_certs[3].get()));
}

TEST(X509UtilTest, CreateCERTCertificateListFromX509CertificateErrors) {
  scoped_refptr<X509Certificate> ok_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  bssl::UniquePtr<CRYPTO_BUFFER> bad_cert =
      x509_util::CreateCryptoBuffer(std::string_view("invalid"));
  ASSERT_TRUE(bad_cert);

  scoped_refptr<X509Certificate> ok_cert2(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(ok_cert);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(std::move(bad_cert));
  intermediates.push_back(bssl::UpRef(ok_cert2->cert_buffer()));
  scoped_refptr<X509Certificate> cert_with_intermediates(
      X509Certificate::CreateFromBuffer(bssl::UpRef(ok_cert->cert_buffer()),
                                        std::move(intermediates)));
  ASSERT_TRUE(cert_with_intermediates);
  EXPECT_EQ(2U, cert_with_intermediates->intermediate_buffers().size());

  // Normal CreateCERTCertificateListFromX509Certificate fails with invalid
  // certs in chain.
  ScopedCERTCertificateList nss_certs =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          cert_with_intermediates.get());
  EXPECT_TRUE(nss_certs.empty());

  // With InvalidIntermediateBehavior::kIgnore, invalid intermediate certs
  // should be silently dropped.
  nss_certs = x509_util::CreateCERTCertificateListFromX509Certificate(
      cert_with_intermediates.get(),
      x509_util::InvalidIntermediateBehavior::kIgnore);
  ASSERT_EQ(2U, nss_certs.size());
  for (const auto& nss_cert : nss_certs)
    ASSERT_TRUE(nss_cert.get());

  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(ok_cert->cert_buffer()),
            BytesForNSSCert(nss_certs[0].get()));
  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(ok_cert2->cert_buffer()),
            BytesForNSSCert(nss_certs[1].get()));
}

TEST(X509UtilNSSTest, CreateCERTCertificateListFromBytes) {
  base::FilePath cert_path =
      GetTestCertsDirectory().AppendASCII("multi-root-chain1.pem");
  std::string cert_data;
  ASSERT_TRUE(base::ReadFileToString(cert_path, &cert_data));

  ScopedCERTCertificateList certs =
      x509_util::CreateCERTCertificateListFromBytes(
          base::as_byte_span(cert_data), X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(4U, certs.size());
  EXPECT_STREQ("CN=127.0.0.1,O=Test CA,L=Mountain View,ST=California,C=US",
               certs[0]->subjectName);
  EXPECT_STREQ("CN=B CA - Multi-root", certs[1]->subjectName);
  EXPECT_STREQ("CN=C CA - Multi-root", certs[2]->subjectName);
  EXPECT_STREQ("CN=D Root CA - Multi-root", certs[3]->subjectName);
}

TEST(X509UtilNSSTest, DupCERTCertificate) {
  ScopedCERTCertificate cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(cert);

  ScopedCERTCertificate cert2 = x509_util::DupCERTCertificate(cert.get());
  // Both handles should hold a reference to the same CERTCertificate object.
  ASSERT_EQ(cert.get(), cert2.get());

  // Release the initial handle.
  cert.reset();
  // The duped handle should still be safe to access.
  EXPECT_STREQ(
      "CN=www.google.com,O=Google Inc,L=Mountain View,ST=California,C=US",
      cert2->subjectName);
}

TEST(X509UtilNSSTest, DupCERTCertificateList) {
  ScopedCERTCertificate cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(cert);
  ScopedCERTCertificate cert2(
      x509_util::CreateCERTCertificateFromBytes(webkit_der));
  ASSERT_TRUE(cert2);
  ScopedCERTCertificateList certs;
  certs.push_back(std::move(cert));
  certs.push_back(std::move(cert2));

  ScopedCERTCertificateList certs_dup =
      x509_util::DupCERTCertificateList(certs);
  ASSERT_EQ(2U, certs_dup.size());
  ASSERT_EQ(certs[0].get(), certs_dup[0].get());
  ASSERT_EQ(certs[1].get(), certs_dup[1].get());

  // Release the initial handles.
  certs.clear();
  // The duped handles should still be safe to access.
  EXPECT_STREQ(
      "CN=www.google.com,O=Google Inc,L=Mountain View,ST=California,C=US",
      certs_dup[0]->subjectName);
  EXPECT_STREQ(
      "CN=*.webkit.org,OU=Mac OS Forge,O=Apple "
      "Inc.,L=Cupertino,ST=California,C=US",
      certs_dup[1]->subjectName);
}

TEST(X509UtilNSSTest, DupCERTCertificateList_EmptyList) {
  EXPECT_EQ(0U, x509_util::DupCERTCertificateList({}).size());
}

TEST(X509UtilNSSTest, CreateX509CertificateFromCERTCertificate_NoChain) {
  ScopedCERTCertificate nss_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(nss_cert);
  scoped_refptr<X509Certificate> x509_cert =
      x509_util::CreateX509CertificateFromCERTCertificate(nss_cert.get());
  EXPECT_EQ(BytesForNSSCert(nss_cert.get()),
            x509_util::CryptoBufferAsStringPiece(x509_cert->cert_buffer()));
  EXPECT_TRUE(x509_cert->intermediate_buffers().empty());
}

TEST(X509UtilNSSTest, CreateX509CertificateFromCERTCertificate_EmptyChain) {
  ScopedCERTCertificate nss_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(nss_cert);
  scoped_refptr<X509Certificate> x509_cert =
      x509_util::CreateX509CertificateFromCERTCertificate(
          nss_cert.get(), std::vector<CERTCertificate*>());
  EXPECT_EQ(BytesForNSSCert(nss_cert.get()),
            x509_util::CryptoBufferAsStringPiece(x509_cert->cert_buffer()));
  EXPECT_TRUE(x509_cert->intermediate_buffers().empty());
}

TEST(X509UtilNSSTest, CreateX509CertificateFromCERTCertificate_WithChain) {
  ScopedCERTCertificate nss_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(nss_cert);
  ScopedCERTCertificate nss_cert2(
      x509_util::CreateCERTCertificateFromBytes(webkit_der));
  ASSERT_TRUE(nss_cert2);

  std::vector<CERTCertificate*> chain;
  chain.push_back(nss_cert2.get());

  scoped_refptr<X509Certificate> x509_cert =
      x509_util::CreateX509CertificateFromCERTCertificate(nss_cert.get(),
                                                          chain);
  EXPECT_EQ(BytesForNSSCert(nss_cert.get()),
            x509_util::CryptoBufferAsStringPiece(x509_cert->cert_buffer()));
  ASSERT_EQ(1U, x509_cert->intermediate_buffers().size());
  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(
                x509_cert->intermediate_buffers()[0].get()),
            BytesForNSSCert(nss_cert2.get()));
}

TEST(X509UtilNSSTest, CreateX509CertificateListFromCERTCertificates) {
  ScopedCERTCertificate nss_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(nss_cert);
  ScopedCERTCertificate nss_cert2(
      x509_util::CreateCERTCertificateFromBytes(webkit_der));
  ASSERT_TRUE(nss_cert2);
  ScopedCERTCertificateList nss_certs;
  nss_certs.push_back(std::move(nss_cert));
  nss_certs.push_back(std::move(nss_cert2));

  CertificateList x509_certs =
      x509_util::CreateX509CertificateListFromCERTCertificates(nss_certs);
  ASSERT_EQ(2U, x509_certs.size());

  EXPECT_EQ(BytesForNSSCert(nss_certs[0].get()),
            x509_util::CryptoBufferAsStringPiece(x509_certs[0]->cert_buffer()));
  EXPECT_EQ(BytesForNSSCert(nss_certs[1].get()),
            x509_util::CryptoBufferAsStringPiece(x509_certs[1]->cert_buffer()));
}

TEST(X509UtilNSSTest, CreateX509CertificateListFromCERTCertificates_EmptyList) {
  ScopedCERTCertificateList nss_certs;
  CertificateList x509_certs =
      x509_util::CreateX509CertificateListFromCERTCertificates(nss_certs);
  ASSERT_EQ(0U, x509_certs.size());
}

TEST(X509UtilNSSTest, GetDEREncoded) {
  ScopedCERTCertificate google_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(google_cert);
  std::string der_encoded;
  ASSERT_TRUE(x509_util::GetDEREncoded(google_cert.get(), &der_encoded));
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(google_der),
                        std::size(google_der)),
            der_encoded);
}

TEST(X509UtilNSSTest, GetDefaultNickname) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  ScopedCERTCertificate test_cert = ImportCERTCertificateFromFile(
      certs_dir, "no_subject_common_name_cert.pem");
  ASSERT_TRUE(test_cert);

  std::string nickname = x509_util::GetDefaultUniqueNickname(
      test_cert.get(), USER_CERT, nullptr /*slot*/);
  EXPECT_EQ(
      "wtc@google.com's COMODO Client Authentication and "
      "Secure Email CA ID",
      nickname);
}

TEST(X509UtilNSSTest, GetCERTNameDisplayName_CN) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  ScopedCERTCertificate test_cert =
      ImportCERTCertificateFromFile(certs_dir, "ok_cert.pem");
  ASSERT_TRUE(test_cert);
  scoped_refptr<X509Certificate> x509_test_cert =
      ImportCertFromFile(certs_dir, "ok_cert.pem");
  ASSERT_TRUE(x509_test_cert);

  std::string name = x509_util::GetCERTNameDisplayName(&test_cert->subject);
  EXPECT_EQ("127.0.0.1", name);
  EXPECT_EQ(x509_test_cert->subject().GetDisplayName(), name);
}

TEST(X509UtilNSSTest, GetCERTNameDisplayName_O) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");

  ScopedCERTCertificate test_cert =
      ImportCERTCertificateFromFile(certs_dir, "subject_t61string.pem");
  ASSERT_TRUE(test_cert);
  scoped_refptr<X509Certificate> x509_test_cert =
      ImportCertFromFile(certs_dir, "subject_t61string.pem");
  ASSERT_TRUE(x509_test_cert);

  std::string name = x509_util::GetCERTNameDisplayName(&test_cert->subject);
  EXPECT_EQ(
      " !\"#$%&'()*+,-./"
      "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
      "abcdefghijklmnopqrstuvwxyz{|}~"
      " ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæç"
      "èéêëìíîïðñòóôõö÷øùúûüýþÿ",
      name);
  EXPECT_EQ(x509_test_cert->subject().GetDisplayName(), name);
}

TEST(X509UtilNSSTest, ParseClientSubjectAltNames) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  // This cert contains one rfc822Name field, and one Microsoft UPN
  // otherName field.
  ScopedCERTCertificate san_cert =
      ImportCERTCertificateFromFile(certs_dir, "client_3.pem");
  ASSERT_TRUE(san_cert);

  std::vector<std::string> rfc822_names;
  x509_util::GetRFC822SubjectAltNames(san_cert.get(), &rfc822_names);
  ASSERT_EQ(1U, rfc822_names.size());
  EXPECT_EQ("santest@example.com", rfc822_names[0]);

  std::vector<std::string> upn_names;
  x509_util::GetUPNSubjectAltNames(san_cert.get(), &upn_names);
  ASSERT_EQ(1U, upn_names.size());
  EXPECT_EQ("santest@ad.corp.example.com", upn_names[0]);
}

TEST(X509UtilNSSTest, GetValidityTimes) {
  ScopedCERTCertificate google_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(google_cert);

  base::Time not_before, not_after;
  EXPECT_TRUE(
      x509_util::GetValidityTimes(google_cert.get(), &not_before, &not_after));

  // Constants copied from x509_certificate_unittest.cc.
  EXPECT_EQ(1238192407,  // Mar 27 22:20:07 2009 GMT
            not_before.InSecondsFSinceUnixEpoch());
  EXPECT_EQ(1269728407,  // Mar 27 22:20:07 2010 GMT
            not_after.InSecondsFSinceUnixEpoch());
}

TEST(X509UtilNSSTest, GetValidityTimesOptionalArgs) {
  ScopedCERTCertificate google_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(google_cert);

  base::Time not_before;
  EXPECT_TRUE(
      x509_util::GetValidityTimes(google_cert.get(), &not_before, nullptr));
  // Constants copied from x509_certificate_unittest.cc.
  EXPECT_EQ(1238192407,  // Mar 27 22:20:07 2009 GMT
            not_before.InSecondsFSinceUnixEpoch());

  base::Time not_after;
  EXPECT_TRUE(
      x509_util::GetValidityTimes(google_cert.get(), nullptr, &not_after));
  EXPECT_EQ(1269728407,  // Mar 27 22:20:07 2010 GMT
            not_after.InSecondsFSinceUnixEpoch());
}

TEST(X509UtilNSSTest, CalculateFingerprint256) {
  static const SHA256HashValue google_fingerprint = {
      {0x21, 0xaf, 0x58, 0x74, 0xea, 0x6b, 0xad, 0xbd, 0xe4, 0xb3, 0xb1,
       0xaa, 0x53, 0x32, 0x80, 0x8f, 0xbf, 0x8a, 0x24, 0x7d, 0x98, 0xec,
       0x7f, 0x77, 0x49, 0x38, 0x42, 0x81, 0x26, 0x7f, 0xed, 0x38}};

  ScopedCERTCertificate google_cert(
      x509_util::CreateCERTCertificateFromBytes(google_der));
  ASSERT_TRUE(google_cert);

  EXPECT_EQ(google_fingerprint,
            x509_util::CalculateFingerprint256(google_cert.get()));
}

}  // namespace net
