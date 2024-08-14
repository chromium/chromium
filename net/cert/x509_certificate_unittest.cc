// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_certificate.h"

#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/pem.h"

using base::HexEncode;
using base::Time;

namespace net {

namespace {

// Certificates for test data. They're obtained with:
//
// $ openssl s_client -connect [host]:443 -showcerts > /tmp/host.pem < /dev/null
// $ openssl x509 -inform PEM -outform DER < /tmp/host.pem > /tmp/host.der
//
// For fingerprint
// $ openssl x509 -inform DER -fingerprint -noout < /tmp/host.der

// For valid_start, valid_expiry
// $ openssl x509 -inform DER -text -noout < /tmp/host.der |
//    grep -A 2 Validity
// $ date +%s -d '<date str>'

// Google's cert.
SHA256HashValue google_fingerprint = {
    {0x21, 0xaf, 0x58, 0x74, 0xea, 0x6b, 0xad, 0xbd, 0xe4, 0xb3, 0xb1,
     0xaa, 0x53, 0x32, 0x80, 0x8f, 0xbf, 0x8a, 0x24, 0x7d, 0x98, 0xec,
     0x7f, 0x77, 0x49, 0x38, 0x42, 0x81, 0x26, 0x7f, 0xed, 0x38}};

// The fingerprint of the Google certificate used in the parsing tests,
// which is newer than the one included in the x509_certificate_data.h
SHA256HashValue google_parse_fingerprint = {
    {0xf6, 0x41, 0xc3, 0x6c, 0xfe, 0xf4, 0x9b, 0xc0, 0x71, 0x35, 0x9e,
     0xcf, 0x88, 0xee, 0xd9, 0x31, 0x7b, 0x73, 0x8b, 0x59, 0x89, 0x41,
     0x6a, 0xd4, 0x01, 0x72, 0x0c, 0x0a, 0x4e, 0x2e, 0x63, 0x52}};

// The fingerprint for the Thawte SGC certificate
SHA256HashValue thawte_parse_fingerprint = {
    {0x10, 0x85, 0xa6, 0xf4, 0x54, 0xd0, 0xc9, 0x11, 0x98, 0xfd, 0xda,
     0xb1, 0x1a, 0x31, 0xc7, 0x16, 0xd5, 0xdc, 0xd6, 0x8d, 0xf9, 0x1c,
     0x03, 0x9c, 0xe1, 0x8d, 0xca, 0x9b, 0xeb, 0x3c, 0xde, 0x3d}};

// Dec 18 00:00:00 2009 GMT
const double kGoogleParseValidFrom = 1261094400;
// Dec 18 23:59:59 2011 GMT
const double kGoogleParseValidTo = 1324252799;

void CheckGoogleCert(const scoped_refptr<X509Certificate>& google_cert,
                     const SHA256HashValue& expected_fingerprint,
                     double valid_from,
                     double valid_to) {
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), google_cert.get());

  const CertPrincipal& subject = google_cert->subject();
  EXPECT_EQ("www.google.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Google Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());

  const CertPrincipal& issuer = google_cert->issuer();
  EXPECT_EQ("Thawte SGC CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("ZA", issuer.country_name);
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("Thawte Consulting (Pty) Ltd.", issuer.organization_names[0]);
  EXPECT_EQ(0U, issuer.organization_unit_names.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = google_cert->valid_start();
  EXPECT_EQ(valid_from, valid_start.InSecondsFSinceUnixEpoch());

  const Time& valid_expiry = google_cert->valid_expiry();
  EXPECT_EQ(valid_to, valid_expiry.InSecondsFSinceUnixEpoch());

  EXPECT_EQ(expected_fingerprint, X509Certificate::CalculateFingerprint256(
                                      google_cert->cert_buffer()));

}

void ExpectX509CertificateMembersEqual(
    const scoped_refptr<X509Certificate>& a,
    const scoped_refptr<X509Certificate>& b) {
  EXPECT_TRUE(a->subject().EqualsForTesting(b->subject()));
  EXPECT_TRUE(a->issuer().EqualsForTesting(b->issuer()));
  EXPECT_EQ(a->valid_start(), b->valid_start());
  EXPECT_EQ(a->valid_expiry(), b->valid_expiry());
  EXPECT_EQ(a->serial_number(), b->serial_number());
}

}  // namespace

TEST(X509CertificateTest, GoogleCertParsing) {
  scoped_refptr<X509Certificate> google_cert(
      X509Certificate::CreateFromBytes(google_der));

  CheckGoogleCert(google_cert, google_fingerprint,
                  1238192407,   // Mar 27 22:20:07 2009 GMT
                  1269728407);  // Mar 27 22:20:07 2010 GMT
}

TEST(X509CertificateTest, WebkitCertParsing) {
  scoped_refptr<X509Certificate> webkit_cert(
      X509Certificate::CreateFromBytes(webkit_der));

  ASSERT_NE(static_cast<X509Certificate*>(nullptr), webkit_cert.get());

  const CertPrincipal& subject = webkit_cert->subject();
  EXPECT_EQ("Cupertino", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Apple Inc.", subject.organization_names[0]);
  ASSERT_EQ(1U, subject.organization_unit_names.size());
  EXPECT_EQ("Mac OS Forge", subject.organization_unit_names[0]);

  const CertPrincipal& issuer = webkit_cert->issuer();
  EXPECT_EQ("Go Daddy Secure Certification Authority", issuer.common_name);
  EXPECT_EQ("Scottsdale", issuer.locality_name);
  EXPECT_EQ("Arizona", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("GoDaddy.com, Inc.", issuer.organization_names[0]);
  ASSERT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("http://certificates.godaddy.com/repository",
            issuer.organization_unit_names[0]);

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = webkit_cert->valid_start();
  EXPECT_EQ(
      1205883319,
      valid_start.InSecondsFSinceUnixEpoch());  // Mar 18 23:35:19 2008 GMT

  const Time& valid_expiry = webkit_cert->valid_expiry();
  EXPECT_EQ(
      1300491319,
      valid_expiry.InSecondsFSinceUnixEpoch());  // Mar 18 23:35:19 2011 GMT

  std::vector<std::string> dns_names;
  EXPECT_TRUE(webkit_cert->GetSubjectAltName(&dns_names, nullptr));
  ASSERT_EQ(2U, dns_names.size());
  EXPECT_EQ("*.webkit.org", dns_names[0]);
  EXPECT_EQ("webkit.org", dns_names[1]);

  // Test that the wildcard cert matches properly.
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("www.webkit.org"));
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("foo.webkit.org"));
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("webkit.org"));
  EXPECT_FALSE(webkit_cert->VerifyNameMatch("www.webkit.com"));
  EXPECT_FALSE(webkit_cert->VerifyNameMatch("www.foo.webkit.com"));
}

TEST(X509CertificateTest, ThawteCertParsing) {
  scoped_refptr<X509Certificate> thawte_cert(
      X509Certificate::CreateFromBytes(thawte_der));

  ASSERT_NE(static_cast<X509Certificate*>(nullptr), thawte_cert.get());

  const CertPrincipal& subject = thawte_cert->subject();
  EXPECT_EQ("www.thawte.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Thawte Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());

  const CertPrincipal& issuer = thawte_cert->issuer();
  EXPECT_EQ("thawte Extended Validation SSL CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("thawte, Inc.", issuer.organization_names[0]);
  ASSERT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("Terms of use at https://www.thawte.com/cps (c)06",
            issuer.organization_unit_names[0]);

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = thawte_cert->valid_start();
  EXPECT_EQ(
      1227052800,
      valid_start.InSecondsFSinceUnixEpoch());  // Nov 19 00:00:00 2008 GMT

  const Time& valid_expiry = thawte_cert->valid_expiry();
  EXPECT_EQ(
      1263772799,
      valid_expiry.InSecondsFSinceUnixEpoch());  // Jan 17 23:59:59 2010 GMT
}

// Test that all desired AttributeAndValue pairs can be extracted when only
// a single bssl::RelativeDistinguishedName is present. "Normally" there is only
// one AVA per RDN, but some CAs place all AVAs within a single RDN.
// This is a regression test for http://crbug.com/101009
TEST(X509CertificateTest, MultivalueRDN) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> multivalue_rdn_cert =
      ImportCertFromFile(certs_dir, "multivalue_rdn.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), multivalue_rdn_cert.get());

  const CertPrincipal& subject = multivalue_rdn_cert->subject();
  EXPECT_EQ("Multivalue RDN Test", subject.common_name);
  EXPECT_EQ("", subject.locality_name);
  EXPECT_EQ("", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Chromium", subject.organization_names[0]);
  ASSERT_EQ(1U, subject.organization_unit_names.size());
  EXPECT_EQ("Chromium net_unittests", subject.organization_unit_names[0]);
}

// Test that characters which would normally be escaped in the string form,
// such as '=' or '"', are not escaped when parsed as individual components.
// This is a regression test for http://crbug.com/102839
TEST(X509CertificateTest, UnescapedSpecialCharacters) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> unescaped_cert =
      ImportCertFromFile(certs_dir, "unescaped.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), unescaped_cert.get());

  const CertPrincipal& subject = unescaped_cert->subject();
  EXPECT_EQ("127.0.0.1", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Chromium = \"net_unittests\"", subject.organization_names[0]);
  ASSERT_EQ(2U, subject.organization_unit_names.size());
  EXPECT_EQ("net_unittests", subject.organization_unit_names[0]);
  EXPECT_EQ("Chromium", subject.organization_unit_names[1]);
}

TEST(X509CertificateTest, InvalidPrintableStringIsUtf8) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");

  std::string file_data;
  ASSERT_TRUE(base::ReadFileToString(
      certs_dir.AppendASCII(
          "subject_printable_string_containing_utf8_client_cert.pem"),
      &file_data));

  bssl::PEMTokenizer pem_tokenizer(file_data, {"CERTIFICATE"});
  ASSERT_TRUE(pem_tokenizer.GetNext());
  std::string cert_der(pem_tokenizer.data());
  ASSERT_FALSE(pem_tokenizer.GetNext());

  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle =
      x509_util::CreateCryptoBuffer(cert_der);
  ASSERT_TRUE(cert_handle);

  EXPECT_FALSE(
      X509Certificate::CreateFromBuffer(bssl::UpRef(cert_handle.get()), {}));

  X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<X509Certificate> cert =
      X509Certificate::CreateFromBufferUnsafeOptions(
          bssl::UpRef(cert_handle.get()), {}, options);

  const CertPrincipal& subject = cert->subject();
  EXPECT_EQ("Foo@#_ Clïênt Cërt", subject.common_name);
}

TEST(X509CertificateTest, TeletexStringIsLatin1) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "subject_t61string.pem");
  ASSERT_TRUE(cert);

  const CertPrincipal& subject = cert->subject();
  EXPECT_EQ(
      " !\"#$%&'()*+,-./"
      "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
      "abcdefghijklmnopqrstuvwxyz{|}~"
      " ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæç"
      "èéêëìíîïðñòóôõö÷øùúûüýþÿ",
      subject.organization_names[0]);
}

TEST(X509CertificateTest, TeletexStringControlChars) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "subject_t61string_1-32.pem");
  ASSERT_TRUE(cert);

  const CertPrincipal& subject = cert->subject();
  EXPECT_EQ(
      "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12"
      "\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20",
      subject.organization_names[0]);
}

TEST(X509CertificateTest, TeletexStringIsLatin1NotCp1252) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "subject_t61string_126-160.pem");
  ASSERT_TRUE(cert);

  const CertPrincipal& subject = cert->subject();
  // TeletexString is decoded as latin1, so 127-160 get decoded to equivalent
  // unicode control chars.
  EXPECT_EQ(
      "~\x7F\xC2\x80\xC2\x81\xC2\x82\xC2\x83\xC2\x84\xC2\x85\xC2\x86\xC2\x87"
      "\xC2\x88\xC2\x89\xC2\x8A\xC2\x8B\xC2\x8C\xC2\x8D\xC2\x8E\xC2\x8F\xC2\x90"
      "\xC2\x91\xC2\x92\xC2\x93\xC2\x94\xC2\x95\xC2\x96\xC2\x97\xC2\x98\xC2\x99"
      "\xC2\x9A\xC2\x9B\xC2\x9C\xC2\x9D\xC2\x9E\xC2\x9F\xC2\xA0",
      subject.organization_names[0]);
}

TEST(X509CertificateTest, TeletexStringIsNotARealT61String) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "subject_t61string_actual.pem");
  ASSERT_TRUE(cert);

  const CertPrincipal& subject = cert->subject();
  // If TeletexStrings were actually parsed according to T.61, this would be
  // "あ". (Probably. Not verified against a real implementation.)
  EXPECT_EQ("\x1B$@$\"", subject.organization_names[0]);
}

TEST(X509CertificateTest, SerialNumbers) {
  scoped_refptr<X509Certificate> google_cert(
      X509Certificate::CreateFromBytes(google_der));
  ASSERT_TRUE(google_cert);

  static const uint8_t google_serial[16] = {
    0x01,0x2a,0x39,0x76,0x0d,0x3f,0x4f,0xc9,
    0x0b,0xe7,0xbd,0x2b,0xcf,0x95,0x2e,0x7a,
  };
  EXPECT_EQ(google_cert->serial_number(), base::as_string_view(google_serial));
}

TEST(X509CertificateTest, SerialNumberZeroPadded) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "serial_zero_padded.pem");
  ASSERT_TRUE(cert);

  // Check a serial number where the first byte is >= 0x80, the DER returned by
  // serial() should contain the leading 0 padding byte.
  static const uint8_t expected_serial[3] = {0x00, 0x80, 0x01};
  EXPECT_EQ(cert->serial_number(), base::as_string_view(expected_serial));
}

TEST(X509CertificateTest, SerialNumberZeroPadded21BytesLong) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "serial_zero_padded_21_bytes.pem");
  ASSERT_TRUE(cert);

  // Check a serial number where the first byte is >= 0x80, causing the encoded
  // length to be 21 bytes long. This should be an error, but serial number
  // parsing is currently permissive.
  static const uint8_t expected_serial[21] = {
      0x00, 0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13};
  EXPECT_EQ(cert->serial_number(), base::as_string_view(expected_serial));
}

TEST(X509CertificateTest, SerialNumberNegative) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "serial_negative.pem");
  ASSERT_TRUE(cert);

  // RFC 5280 does not allow serial numbers to be negative, but serial number
  // parsing is currently permissive, so this does not cause an error.
  static const uint8_t expected_serial[2] = {0x80, 0x01};
  EXPECT_EQ(cert->serial_number(), base::as_string_view(expected_serial));
}

TEST(X509CertificateTest, SerialNumber37BytesLong) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "serial_37_bytes.pem");
  ASSERT_TRUE(cert);

  // Check a serial number which is very long. This should be an error, but
  // serial number parsing is currently permissive.
  static const uint8_t expected_serial[37] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
      0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
      0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
      0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25};
  EXPECT_EQ(cert->serial_number(), base::as_string_view(expected_serial));
}

TEST(X509CertificateTest, SHA256FingerprintsCorrectly) {
  scoped_refptr<X509Certificate> google_cert(
      X509Certificate::CreateFromBytes(google_der));
  ASSERT_TRUE(google_cert);

  const SHA256HashValue google_sha256_fingerprint = {
      {0x21, 0xaf, 0x58, 0x74, 0xea, 0x6b, 0xad, 0xbd, 0xe4, 0xb3, 0xb1,
       0xaa, 0x53, 0x32, 0x80, 0x8f, 0xbf, 0x8a, 0x24, 0x7d, 0x98, 0xec,
       0x7f, 0x77, 0x49, 0x38, 0x42, 0x81, 0x26, 0x7f, 0xed, 0x38}};

  EXPECT_EQ(google_sha256_fingerprint, X509Certificate::CalculateFingerprint256(
                                           google_cert->cert_buffer()));
}

TEST(X509CertificateTest, CAFingerprints) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "salesforce_com_test.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), server_cert.get());

  scoped_refptr<X509Certificate> intermediate_cert1 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2011.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), intermediate_cert1.get());

  scoped_refptr<X509Certificate> intermediate_cert2 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2016.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), intermediate_cert2.get());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(intermediate_cert1->cert_buffer()));
  scoped_refptr<X509Certificate> cert_chain1 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(server_cert->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(cert_chain1);

  intermediates.clear();
  intermediates.push_back(bssl::UpRef(intermediate_cert2->cert_buffer()));
  scoped_refptr<X509Certificate> cert_chain2 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(server_cert->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(cert_chain2);

  // No intermediate CA certicates.
  intermediates.clear();
  scoped_refptr<X509Certificate> cert_chain3 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(server_cert->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(cert_chain3);

  SHA256HashValue cert_chain1_chain_fingerprint_256 = {
      {0xac, 0xff, 0xcc, 0x63, 0x0d, 0xd0, 0xa7, 0x19, 0x78, 0xb5, 0x8a,
       0x47, 0x8b, 0x67, 0x97, 0xcb, 0x8d, 0xe1, 0x6a, 0x8a, 0x57, 0x70,
       0xda, 0x9a, 0x53, 0x72, 0xe2, 0xa0, 0x08, 0xab, 0xcc, 0x8f}};
  SHA256HashValue cert_chain2_chain_fingerprint_256 = {
      {0x67, 0x3a, 0x11, 0x20, 0xd6, 0x94, 0x14, 0xe4, 0x16, 0x9f, 0x58,
       0xe2, 0x8b, 0xf7, 0x27, 0xed, 0xbb, 0xe8, 0xa7, 0xff, 0x1c, 0x8c,
       0x0f, 0x21, 0x38, 0x16, 0x7c, 0xad, 0x1f, 0x22, 0x6f, 0x9b}};
  SHA256HashValue cert_chain3_chain_fingerprint_256 = {
      {0x16, 0x7a, 0xbd, 0xb4, 0x57, 0x04, 0x65, 0x3c, 0x3b, 0xef, 0x6e,
       0x6a, 0xa6, 0x02, 0x73, 0x30, 0x3e, 0x34, 0x1b, 0x43, 0xc2, 0x7c,
       0x98, 0x52, 0x9f, 0x34, 0x7f, 0x55, 0x97, 0xe9, 0x1a, 0x10}};
  EXPECT_EQ(cert_chain1_chain_fingerprint_256,
            cert_chain1->CalculateChainFingerprint256());
  EXPECT_EQ(cert_chain2_chain_fingerprint_256,
            cert_chain2->CalculateChainFingerprint256());
  EXPECT_EQ(cert_chain3_chain_fingerprint_256,
            cert_chain3->CalculateChainFingerprint256());
}

TEST(X509CertificateTest, ParseSubjectAltNames) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> san_cert =
      ImportCertFromFile(certs_dir, "subjectAltName_sanity_check.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), san_cert.get());

  // Ensure that testing for SAN without using it is accepted.
  EXPECT_TRUE(san_cert->GetSubjectAltName(nullptr, nullptr));

  // Ensure that it's possible to get just dNSNames.
  std::vector<std::string> dns_names;
  EXPECT_TRUE(san_cert->GetSubjectAltName(&dns_names, nullptr));

  // Ensure that it's possible to get just iPAddresses.
  std::vector<std::string> ip_addresses;
  EXPECT_TRUE(san_cert->GetSubjectAltName(nullptr, &ip_addresses));

  // Ensure that DNS names are correctly parsed.
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("test.example", dns_names[0]);

  // Ensure that both IPv4 and IPv6 addresses are correctly parsed.
  ASSERT_EQ(2U, ip_addresses.size());

  static const uint8_t kIPv4Address[] = {
      0x7F, 0x00, 0x00, 0x02
  };
  EXPECT_EQ(ip_addresses[0], base::as_string_view(kIPv4Address));

  static const uint8_t kIPv6Address[] = {
      0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
  };
  EXPECT_EQ(ip_addresses[1], base::as_string_view(kIPv6Address));

  // Ensure the subjectAltName dirName has not influenced the handling of
  // the subject commonName.
  EXPECT_EQ("127.0.0.1", san_cert->subject().common_name);

  scoped_refptr<X509Certificate> no_san_cert =
      ImportCertFromFile(certs_dir, "salesforce_com_test.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), no_san_cert.get());

  EXPECT_NE(0u, dns_names.size());
  EXPECT_NE(0u, ip_addresses.size());
  EXPECT_FALSE(no_san_cert->GetSubjectAltName(&dns_names, &ip_addresses));
  EXPECT_EQ(0u, dns_names.size());
  EXPECT_EQ(0u, ip_addresses.size());
}

TEST(X509CertificateTest, ExtractSPKIFromDERCert) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "nist.der");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), cert.get());

  std::string_view spkiBytes;
  EXPECT_TRUE(asn1::ExtractSPKIFromDERCert(
      base::as_string_view(cert->cert_span()), &spkiBytes));
  base::SHA1Digest hash = base::SHA1Hash(base::as_byte_span(spkiBytes));
  EXPECT_EQ(base::span(hash), base::as_byte_span(kNistSPKIHash));
}

TEST(X509CertificateTest, HasCanSignHttpExchangesDraftExtension) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert = ImportCertFromFile(
      certs_dir, "can_sign_http_exchanges_draft_extension.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), cert.get());

  EXPECT_TRUE(asn1::HasCanSignHttpExchangesDraftExtension(
      x509_util::CryptoBufferAsStringPiece(cert->cert_buffer())));
}

TEST(X509CertificateTest, HasCanSignHttpExchangesDraftExtensionInvalid) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert = ImportCertFromFile(
      certs_dir, "can_sign_http_exchanges_draft_extension_invalid.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), cert.get());

  EXPECT_FALSE(asn1::HasCanSignHttpExchangesDraftExtension(
      x509_util::CryptoBufferAsStringPiece(cert->cert_buffer())));
}

TEST(X509CertificateTest, DoesNotHaveCanSignHttpExchangesDraftExtension) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "ok_cert.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), cert.get());

  EXPECT_FALSE(asn1::HasCanSignHttpExchangesDraftExtension(
      x509_util::CryptoBufferAsStringPiece(cert->cert_buffer())));
}

TEST(X509CertificateTest, ExtractExtension) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "ok_cert.pem");
  ASSERT_TRUE(cert);

  bool present, critical;
  std::string_view contents;
  ASSERT_TRUE(asn1::ExtractExtensionFromDERCert(
      x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()),
      bssl::der::Input(bssl::kBasicConstraintsOid).AsStringView(), &present,
      &critical, &contents));
  EXPECT_TRUE(present);
  EXPECT_TRUE(critical);
  ASSERT_EQ(std::string_view("\x30\x00", 2), contents);

  static constexpr uint8_t kNonsenseOID[] = {0x56, 0x1d, 0x13};
  ASSERT_TRUE(asn1::ExtractExtensionFromDERCert(
      x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()),
      base::as_string_view(kNonsenseOID), &present, &critical, &contents));
  ASSERT_FALSE(present);

  scoped_refptr<X509Certificate> uid_cert =
      ImportCertFromFile(certs_dir, "ct-test-embedded-with-uids.pem");
  ASSERT_TRUE(uid_cert);
  ASSERT_TRUE(asn1::ExtractExtensionFromDERCert(
      x509_util::CryptoBufferAsStringPiece(uid_cert->cert_buffer()),
      bssl::der::Input(bssl::kBasicConstraintsOid).AsStringView(), &present,
      &critical, &contents));
  EXPECT_TRUE(present);
  EXPECT_FALSE(critical);
  ASSERT_EQ(std::string_view("\x30\x00", 2), contents);
}

// Tests CRYPTO_BUFFER deduping via X509Certificate::CreateFromBuffer.  We
// call X509Certificate::CreateFromBuffer several times and observe whether
// it returns a cached or new CRYPTO_BUFFER.
TEST(X509CertificateTest, Cache) {
  bssl::UniquePtr<CRYPTO_BUFFER> google_cert_handle;
  bssl::UniquePtr<CRYPTO_BUFFER> thawte_cert_handle;

  // Add a single certificate to the certificate cache.
  google_cert_handle = x509_util::CreateCryptoBuffer(google_der);
  ASSERT_TRUE(google_cert_handle);
  scoped_refptr<X509Certificate> cert1(
      X509Certificate::CreateFromBuffer(std::move(google_cert_handle), {}));
  ASSERT_TRUE(cert1);

  // Add the same certificate, but as a new handle.
  google_cert_handle = x509_util::CreateCryptoBuffer(google_der);
  ASSERT_TRUE(google_cert_handle);
  scoped_refptr<X509Certificate> cert2(
      X509Certificate::CreateFromBuffer(std::move(google_cert_handle), {}));
  ASSERT_TRUE(cert2);

  // A new X509Certificate should be returned.
  EXPECT_NE(cert1.get(), cert2.get());
  // But both instances should share the underlying OS certificate handle.
  EXPECT_EQ(cert1->cert_buffer(), cert2->cert_buffer());
  EXPECT_EQ(0u, cert1->intermediate_buffers().size());
  EXPECT_EQ(0u, cert2->intermediate_buffers().size());

  // Add the same certificate, but this time with an intermediate. This
  // should result in the intermediate being cached. Note that this is not
  // a legitimate chain, but is suitable for testing.
  google_cert_handle = x509_util::CreateCryptoBuffer(google_der);
  thawte_cert_handle = x509_util::CreateCryptoBuffer(thawte_der);
  ASSERT_TRUE(google_cert_handle);
  ASSERT_TRUE(thawte_cert_handle);
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(std::move(thawte_cert_handle));
  scoped_refptr<X509Certificate> cert3(X509Certificate::CreateFromBuffer(
      std::move(google_cert_handle), std::move(intermediates)));
  ASSERT_TRUE(cert3);

  // Test that the new certificate, even with intermediates, results in the
  // same underlying handle being used.
  EXPECT_EQ(cert1->cert_buffer(), cert3->cert_buffer());
  // Though they use the same OS handle, the intermediates should be different.
  EXPECT_NE(cert1->intermediate_buffers().size(),
            cert3->intermediate_buffers().size());
}

TEST(X509CertificateTest, CloneWithDifferentIntermediates) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "multi-root-chain1.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(4u, certs.size());

  auto leaf_with_no_intermediates = certs[0];

  {
    auto cloned =
        leaf_with_no_intermediates->CloneWithDifferentIntermediates({});
    // Intermediates are equal, so should return a reference to the same object.
    EXPECT_EQ(leaf_with_no_intermediates.get(), cloned.get());
  }
  {
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
    intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
    intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));
    auto cloned = leaf_with_no_intermediates->CloneWithDifferentIntermediates(
        std::move(intermediates));
    ASSERT_TRUE(cloned);
    EXPECT_NE(leaf_with_no_intermediates.get(), cloned.get());
    EXPECT_EQ(leaf_with_no_intermediates->cert_buffer(), cloned->cert_buffer());
    ExpectX509CertificateMembersEqual(leaf_with_no_intermediates, cloned);
    ASSERT_EQ(2u, cloned->intermediate_buffers().size());
    EXPECT_TRUE(x509_util::CryptoBufferEqual(
        certs[1]->cert_buffer(), cloned->intermediate_buffers()[0].get()));
    EXPECT_TRUE(x509_util::CryptoBufferEqual(
        certs[2]->cert_buffer(), cloned->intermediate_buffers()[1].get()));
  }

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> leaf_intermediates;
  leaf_intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  leaf_intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));
  auto leaf_with_intermediates = X509Certificate::CreateFromBuffer(
      bssl::UpRef(certs[0]->cert_buffer()), std::move(leaf_intermediates));
  ASSERT_TRUE(leaf_with_intermediates);

  {
    auto cloned = leaf_with_intermediates->CloneWithDifferentIntermediates({});
    EXPECT_NE(leaf_with_intermediates.get(), cloned.get());
    EXPECT_EQ(leaf_with_intermediates->cert_buffer(), cloned->cert_buffer());
    ExpectX509CertificateMembersEqual(leaf_with_intermediates, cloned);
    ASSERT_EQ(0u, cloned->intermediate_buffers().size());
  }
  {
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
    intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
    intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));
    auto cloned = leaf_with_intermediates->CloneWithDifferentIntermediates(
        std::move(intermediates));
    // Intermediates are equal, so should return a reference to the same object.
    EXPECT_EQ(leaf_with_intermediates.get(), cloned.get());
  }
  {
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
    intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));
    intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
    auto cloned = leaf_with_intermediates->CloneWithDifferentIntermediates(
        std::move(intermediates));
    // Intermediates are different (same buffers but in different order).
    ASSERT_TRUE(cloned);
    EXPECT_NE(leaf_with_intermediates.get(), cloned.get());
    EXPECT_EQ(leaf_with_intermediates->cert_buffer(), cloned->cert_buffer());
    ExpectX509CertificateMembersEqual(leaf_with_intermediates, cloned);
    ASSERT_EQ(2u, cloned->intermediate_buffers().size());
    EXPECT_TRUE(x509_util::CryptoBufferEqual(
        certs[2]->cert_buffer(), cloned->intermediate_buffers()[0].get()));
    EXPECT_TRUE(x509_util::CryptoBufferEqual(
        certs[1]->cert_buffer(), cloned->intermediate_buffers()[1].get()));
  }
}

TEST(X509CertificateTest, Pickle) {
  bssl::UniquePtr<CRYPTO_BUFFER> google_cert_handle =
      x509_util::CreateCryptoBuffer(google_der);
  ASSERT_TRUE(google_cert_handle);
  bssl::UniquePtr<CRYPTO_BUFFER> thawte_cert_handle =
      x509_util::CreateCryptoBuffer(thawte_der);
  ASSERT_TRUE(thawte_cert_handle);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(std::move(thawte_cert_handle));
  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
      std::move(google_cert_handle), std::move(intermediates));
  ASSERT_TRUE(cert);

  base::Pickle pickle;
  cert->Persist(&pickle);

  base::PickleIterator iter(pickle);
  scoped_refptr<X509Certificate> cert_from_pickle =
      X509Certificate::CreateFromPickle(&iter);
  ASSERT_TRUE(cert_from_pickle);
  EXPECT_TRUE(x509_util::CryptoBufferEqual(cert->cert_buffer(),
                                           cert_from_pickle->cert_buffer()));
  const auto& cert_intermediates = cert->intermediate_buffers();
  const auto& pickle_intermediates = cert_from_pickle->intermediate_buffers();
  ASSERT_EQ(cert_intermediates.size(), pickle_intermediates.size());
  for (size_t i = 0; i < cert_intermediates.size(); ++i) {
    EXPECT_TRUE(x509_util::CryptoBufferEqual(cert_intermediates[i].get(),
                                             pickle_intermediates[i].get()));
  }
}

TEST(X509CertificateTest, IntermediateCertificates) {
  scoped_refptr<X509Certificate> webkit_cert(
      X509Certificate::CreateFromBytes(webkit_der));
  ASSERT_TRUE(webkit_cert);

  scoped_refptr<X509Certificate> thawte_cert(
      X509Certificate::CreateFromBytes(thawte_der));
  ASSERT_TRUE(thawte_cert);

  bssl::UniquePtr<CRYPTO_BUFFER> google_handle;
  // Create object with no intermediates:
  google_handle = x509_util::CreateCryptoBuffer(google_der);
  scoped_refptr<X509Certificate> cert1;
  cert1 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(google_handle.get()), {});
  ASSERT_TRUE(cert1);
  EXPECT_EQ(0u, cert1->intermediate_buffers().size());

  // Create object with 2 intermediates:
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates2;
  intermediates2.push_back(bssl::UpRef(webkit_cert->cert_buffer()));
  intermediates2.push_back(bssl::UpRef(thawte_cert->cert_buffer()));
  scoped_refptr<X509Certificate> cert2 = X509Certificate::CreateFromBuffer(
      std::move(google_handle), std::move(intermediates2));
  ASSERT_TRUE(cert2);

  // Verify it has all the intermediates:
  const auto& cert2_intermediates = cert2->intermediate_buffers();
  ASSERT_EQ(2u, cert2_intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(cert2_intermediates[0].get(),
                                           webkit_cert->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(cert2_intermediates[1].get(),
                                           thawte_cert->cert_buffer()));
}

TEST(X509CertificateTest, Equals) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "multi-root-chain1.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(4u, certs.size());

  // Comparing X509Certificates with no intermediates.
  EXPECT_TRUE(certs[0]->EqualsExcludingChain(certs[0].get()));
  EXPECT_FALSE(certs[1]->EqualsExcludingChain(certs[0].get()));
  EXPECT_FALSE(certs[0]->EqualsExcludingChain(certs[1].get()));
  EXPECT_TRUE(certs[0]->EqualsIncludingChain(certs[0].get()));
  EXPECT_FALSE(certs[1]->EqualsIncludingChain(certs[0].get()));
  EXPECT_FALSE(certs[0]->EqualsIncludingChain(certs[1].get()));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates1;
  intermediates1.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  scoped_refptr<X509Certificate> cert0_with_intermediate =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates1));
  ASSERT_TRUE(cert0_with_intermediate);

  // Comparing X509Certificate with one intermediate to X509Certificate with no
  // intermediates.
  EXPECT_TRUE(certs[0]->EqualsExcludingChain(cert0_with_intermediate.get()));
  EXPECT_TRUE(cert0_with_intermediate->EqualsExcludingChain(certs[0].get()));
  EXPECT_FALSE(certs[0]->EqualsIncludingChain(cert0_with_intermediate.get()));
  EXPECT_FALSE(cert0_with_intermediate->EqualsIncludingChain(certs[0].get()));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates2;
  intermediates2.push_back(bssl::UpRef(certs[2]->cert_buffer()));
  scoped_refptr<X509Certificate> cert0_with_intermediate2 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates2));
  ASSERT_TRUE(cert0_with_intermediate2);

  // Comparing X509Certificate with one intermediate to X509Certificate with
  // one different intermediate.
  EXPECT_TRUE(cert0_with_intermediate2->EqualsExcludingChain(
      cert0_with_intermediate.get()));
  EXPECT_TRUE(cert0_with_intermediate->EqualsExcludingChain(
      cert0_with_intermediate2.get()));
  EXPECT_FALSE(cert0_with_intermediate2->EqualsIncludingChain(
      cert0_with_intermediate.get()));
  EXPECT_FALSE(cert0_with_intermediate->EqualsIncludingChain(
      cert0_with_intermediate2.get()));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates12;
  intermediates12.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  intermediates12.push_back(bssl::UpRef(certs[2]->cert_buffer()));
  scoped_refptr<X509Certificate> cert0_with_intermediates12 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates12));
  ASSERT_TRUE(cert0_with_intermediates12);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates21;
  intermediates21.push_back(bssl::UpRef(certs[2]->cert_buffer()));
  intermediates21.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  scoped_refptr<X509Certificate> cert0_with_intermediates21 =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates21));
  ASSERT_TRUE(cert0_with_intermediates21);

  // Comparing X509Certificate with two intermediates to X509Certificate with
  // same two intermediates but in reverse order
  EXPECT_TRUE(cert0_with_intermediates21->EqualsExcludingChain(
      cert0_with_intermediates12.get()));
  EXPECT_TRUE(cert0_with_intermediates12->EqualsExcludingChain(
      cert0_with_intermediates21.get()));
  EXPECT_FALSE(cert0_with_intermediates21->EqualsIncludingChain(
      cert0_with_intermediates12.get()));
  EXPECT_FALSE(cert0_with_intermediates12->EqualsIncludingChain(
      cert0_with_intermediates21.get()));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates12b;
  intermediates12b.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  intermediates12b.push_back(bssl::UpRef(certs[2]->cert_buffer()));
  scoped_refptr<X509Certificate> cert0_with_intermediates12b =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates12b));
  ASSERT_TRUE(cert0_with_intermediates12b);

  // Comparing X509Certificate with two intermediates to X509Certificate with
  // same two intermediates in same order.
  EXPECT_TRUE(cert0_with_intermediates12->EqualsExcludingChain(
      cert0_with_intermediates12b.get()));
  EXPECT_TRUE(cert0_with_intermediates12b->EqualsExcludingChain(
      cert0_with_intermediates12.get()));
  EXPECT_TRUE(cert0_with_intermediates12->EqualsIncludingChain(
      cert0_with_intermediates12b.get()));
  EXPECT_TRUE(cert0_with_intermediates12b->EqualsIncludingChain(
      cert0_with_intermediates12.get()));
}

TEST(X509CertificateTest, IsIssuedByEncoded) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  // Test a client certificate from MIT.
  scoped_refptr<X509Certificate> mit_davidben_cert(
      ImportCertFromFile(certs_dir, "mit.davidben.der"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), mit_davidben_cert.get());

  std::string mit_issuer{base::as_string_view(MITDN)};

  // Test a certificate from Google, issued by Thawte
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), google_cert.get());

  std::string thawte_issuer{base::as_string_view(ThawteDN)};

  // Check that the David Ben certificate is issued by MIT, but not
  // by Thawte.
  std::vector<std::string> issuers;
  issuers.clear();
  issuers.push_back(mit_issuer);
  EXPECT_TRUE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_FALSE(google_cert->IsIssuedByEncoded(issuers));

  // Check that the Google certificate is issued by Thawte and not
  // by MIT.
  issuers.clear();
  issuers.push_back(thawte_issuer);
  EXPECT_FALSE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_TRUE(google_cert->IsIssuedByEncoded(issuers));

  // Check that they both pass when given a list of the two issuers.
  issuers.clear();
  issuers.push_back(mit_issuer);
  issuers.push_back(thawte_issuer);
  EXPECT_TRUE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_TRUE(google_cert->IsIssuedByEncoded(issuers));
}

TEST(X509CertificateTest, IsSelfSigned) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(certs_dir, "mit.davidben.der"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), cert.get());
  EXPECT_FALSE(X509Certificate::IsSelfSigned(cert->cert_buffer()));

  scoped_refptr<X509Certificate> self_signed(
      ImportCertFromFile(certs_dir, "root_ca_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), self_signed.get());
  EXPECT_TRUE(X509Certificate::IsSelfSigned(self_signed->cert_buffer()));

  scoped_refptr<X509Certificate> bad_name(
      ImportCertFromFile(certs_dir, "self-signed-invalid-name.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), bad_name.get());
  EXPECT_FALSE(X509Certificate::IsSelfSigned(bad_name->cert_buffer()));

  scoped_refptr<X509Certificate> bad_sig(
      ImportCertFromFile(certs_dir, "self-signed-invalid-sig.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), bad_sig.get());
  EXPECT_FALSE(X509Certificate::IsSelfSigned(bad_sig->cert_buffer()));

  constexpr char invalid_cert_data[] = "this is not a certificate";
  bssl::UniquePtr<CRYPTO_BUFFER> invalid_cert_handle =
      x509_util::CreateCryptoBuffer(std::string_view(invalid_cert_data));
  ASSERT_TRUE(invalid_cert_handle);
  EXPECT_FALSE(X509Certificate::IsSelfSigned(invalid_cert_handle.get()));
}

TEST(X509CertificateTest, IsIssuedByEncodedWithIntermediates) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  std::string intermediate_dn = intermediate->GetSubject();
  std::string root_dn = root->GetSubject();

  // Create an X509Certificate object containing the leaf and the intermediate
  // but not the root.
  scoped_refptr<X509Certificate> cert_chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(cert_chain);

  // Check that the chain is issued by the intermediate.
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded({intermediate_dn}));

  // Check that the chain is also issued by the root.
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded({root_dn}));

  // Check that the chain is issued by either the intermediate or the root.
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded({intermediate_dn, root_dn}));

  // Check that an empty issuers list returns false.
  EXPECT_FALSE(cert_chain->IsIssuedByEncoded({}));

  // Check that the chain is not issued by Verisign
  std::string verisign_issuer{base::as_string_view(VerisignDN)};
  EXPECT_FALSE(cert_chain->IsIssuedByEncoded({verisign_issuer}));

  // Check that the chain is issued by root, though the extraneous Verisign
  // name is also given.
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded({verisign_issuer, root_dn}));
}

const struct CertificateFormatTestData {
  const char* file_name;
  X509Certificate::Format format;
  std::array<SHA256HashValue*, 3> chain_fingerprints;
} kFormatTestData[] = {
    // DER Parsing - single certificate, DER encoded
    {"google.single.der",
     X509Certificate::FORMAT_SINGLE_CERTIFICATE,
     {
         &google_parse_fingerprint,
         nullptr,
     }},
    // DER parsing - single certificate, PEM encoded
    {"google.single.pem",
     X509Certificate::FORMAT_SINGLE_CERTIFICATE,
     {
         &google_parse_fingerprint,
         nullptr,
     }},
    // PEM parsing - single certificate, PEM encoded with a PEB of
    // "CERTIFICATE"
    {"google.single.pem",
     X509Certificate::FORMAT_PEM_CERT_SEQUENCE,
     {
         &google_parse_fingerprint,
         nullptr,
     }},
    // PEM parsing - sequence of certificates, PEM encoded with a PEB of
    // "CERTIFICATE"
    {"google.chain.pem",
     X509Certificate::FORMAT_PEM_CERT_SEQUENCE,
     {
         &google_parse_fingerprint,
         &thawte_parse_fingerprint,
         nullptr,
     }},
    // PKCS#7 parsing - "degenerate" SignedData collection of certificates, DER
    // encoding
    {"google.binary.p7b",
     X509Certificate::FORMAT_PKCS7,
     {
         &google_parse_fingerprint,
         &thawte_parse_fingerprint,
         nullptr,
     }},
    // PKCS#7 parsing - "degenerate" SignedData collection of certificates, PEM
    // encoded with a PEM PEB of "CERTIFICATE"
    {"google.pem_cert.p7b",
     X509Certificate::FORMAT_PKCS7,
     {
         &google_parse_fingerprint,
         &thawte_parse_fingerprint,
         nullptr,
     }},
    // PKCS#7 parsing - "degenerate" SignedData collection of certificates, PEM
    // encoded with a PEM PEB of "PKCS7"
    {"google.pem_pkcs7.p7b",
     X509Certificate::FORMAT_PKCS7,
     {
         &google_parse_fingerprint,
         &thawte_parse_fingerprint,
         nullptr,
     }},
    // All of the above, this time using auto-detection
    {"google.single.der",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint,
         nullptr,
     }},
    {"google.single.pem",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint,
         nullptr,
     }},
    {"google.chain.pem",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint,
         &thawte_parse_fingerprint,
         nullptr,
     }},
    {"google.binary.p7b",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint,
         &thawte_parse_fingerprint,
         nullptr,
     }},
    {"google.pem_cert.p7b",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint,
         &thawte_parse_fingerprint,
         nullptr,
     }},
    {"google.pem_pkcs7.p7b",
     X509Certificate::FORMAT_AUTO,
     {
         &google_parse_fingerprint,
         &thawte_parse_fingerprint,
         nullptr,
     }},
};

class X509CertificateParseTest
    : public testing::TestWithParam<CertificateFormatTestData> {
 public:
  ~X509CertificateParseTest() override = default;
  void SetUp() override { test_data_ = GetParam(); }
  void TearDown() override {}

 protected:
  CertificateFormatTestData test_data_;
};

TEST_P(X509CertificateParseTest, CanParseFormat) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, test_data_.file_name, test_data_.format);
  ASSERT_FALSE(certs.empty());
  ASSERT_LE(certs.size(), std::size(test_data_.chain_fingerprints));
  CheckGoogleCert(certs.front(), google_parse_fingerprint,
                  kGoogleParseValidFrom, kGoogleParseValidTo);

  for (size_t i = 0; i < std::size(test_data_.chain_fingerprints); ++i) {
    if (!test_data_.chain_fingerprints[i]) {
      // No more test certificates expected - make sure no more were
      // returned before marking this test a success.
      EXPECT_EQ(i, certs.size());
      break;
    }

    // A cert is expected - make sure that one was parsed.
    ASSERT_LT(i, certs.size());
    ASSERT_TRUE(certs[i]);

    // Compare the parsed certificate with the expected certificate, by
    // comparing fingerprints.
    EXPECT_EQ(
        *test_data_.chain_fingerprints[i],
        X509Certificate::CalculateFingerprint256(certs[i]->cert_buffer()));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         X509CertificateParseTest,
                         testing::ValuesIn(kFormatTestData));

struct CertificateNameVerifyTestData {
  // true iff we expect hostname to match an entry in cert_names.
  bool expected;
  // The hostname to match.
  const char* hostname;
  // Comma separated list of certificate names to match against. Any occurrence
  // of '#' will be replaced with a null character before processing.
  const char* dns_names;
  // Comma separated list of certificate IP Addresses to match against. Each
  // address is x prefixed 16 byte hex code for v6 or dotted-decimals for v4.
  const char* ip_addrs;
};

// GTest 'magic' pretty-printer, so that if/when a test fails, it knows how
// to output the parameter that was passed. Without this, it will simply
// attempt to print out the first twenty bytes of the object, which depending
// on platform and alignment, may result in an invalid read.
void PrintTo(const CertificateNameVerifyTestData& data, std::ostream* os) {
  ASSERT_TRUE(data.hostname);
  ASSERT_TRUE(data.dns_names || data.ip_addrs);
  *os << " expected: " << data.expected << "; hostname: " << data.hostname
      << "; dns_names: " << (data.dns_names ? data.dns_names : "")
      << "; ip_addrs: " << (data.ip_addrs ? data.ip_addrs : "");
}

const CertificateNameVerifyTestData kNameVerifyTestData[] = {
    {true, "foo.com", "foo.com"},
    {true, "f", "f"},
    {false, "h", "i"},
    {true, "bar.foo.com", "*.foo.com"},
    {true, "www.test.fr", "*.test.com,*.test.co.uk,*.test.de,*.test.fr"},
    {true, "wwW.tESt.fr", ",*.*,*.test.de,*.test.FR,www"},
    {false, "f.uk", ".uk"},
    {false, "w.bar.foo.com", "?.bar.foo.com"},
    {false, "www.foo.com", "(www|ftp).foo.com"},
    {false, "www.foo.com", "www.foo.com#"},  // # = null char.
    {false, "www.foo.com", "www.foo.com#*.foo.com,#,#"},
    {false, "www.house.example", "ww.house.example"},
    {false, "test.org", "www.test.org,*.test.org,*.org"},
    {false, "w.bar.foo.com", "w*.bar.foo.com"},
    {false, "www.bar.foo.com", "ww*ww.bar.foo.com"},
    {false, "wwww.bar.foo.com", "ww*ww.bar.foo.com"},
    {false, "wwww.bar.foo.com", "w*w.bar.foo.com"},
    {false, "wwww.bar.foo.com", "w*w.bar.foo.c0m"},
    {false, "WALLY.bar.foo.com", "wa*.bar.foo.com"},
    {false, "wally.bar.foo.com", "*Ly.bar.foo.com"},
    // Hostname escaping tests
    {true, "ww%57.foo.com", "www.foo.com"},
    {true, "www%2Efoo.com", "www.foo.com"},
    {false, "www%00.foo.com", "www,foo.com,www.foo.com"},
    {false, "www%0D.foo.com", "www.foo.com,www\r.foo.com"},
    {false, "www%40foo.com", "www@foo.com"},
    {false, "www%2E%2Efoo.com", "www.foo.com,www..foo.com"},
    {false, "www%252Efoo.com", "www.foo.com"},
    // IDN tests
    {true, "xn--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br"},
    {true, "www.xn--poema-9qae5a.com.br", "*.xn--poema-9qae5a.com.br"},
    {false, "xn--poema-9qae5a.com.br",
     "*.xn--poema-9qae5a.com.br,"
     "xn--poema-*.com.br,"
     "xn--*-9qae5a.com.br,"
     "*--poema-9qae5a.com.br"},
    // The following are adapted from the  examples quoted from
    // http://tools.ietf.org/html/rfc6125#section-6.4.3
    //  (e.g., *.example.com would match foo.example.com but
    //   not bar.foo.example.com or example.com).
    {true, "foo.example.com", "*.example.com"},
    {false, "bar.foo.example.com", "*.example.com"},
    {false, "example.com", "*.example.com"},
    //   Partial wildcards are disallowed, though RFC 2818 rules allow them.
    //   That is, forms such as baz*.example.net, *baz.example.net, and
    //   b*z.example.net should NOT match domains. Instead, the wildcard must
    //   always be the left-most label, and only a single label.
    {false, "baz1.example.net", "baz*.example.net"},
    {false, "foobaz.example.net", "*baz.example.net"},
    {false, "buzz.example.net", "b*z.example.net"},
    {false, "www.test.example.net", "www.*.example.net"},
    // Wildcards should not be valid for public registry controlled domains,
    // and unknown/unrecognized domains, at least three domain components must
    // be present.
    {true, "www.test.example", "*.test.example"},
    {true, "test.example.co.uk", "*.example.co.uk"},
    {false, "test.example", "*.example"},
    {false, "example.co.uk", "*.co.uk"},
    {false, "foo.com", "*.com"},
    {false, "foo.us", "*.us"},
    {false, "foo", "*"},
    // IDN variants of wildcards and registry controlled domains.
    {true, "www.xn--poema-9qae5a.com.br", "*.xn--poema-9qae5a.com.br"},
    {true, "test.example.xn--mgbaam7a8h", "*.example.xn--mgbaam7a8h"},
    {false, "xn--poema-9qae5a.com.br", "*.com.br"},
    {false, "example.xn--mgbaam7a8h", "*.xn--mgbaam7a8h"},
    // Wildcards should be permissible for 'private' registry controlled
    // domains.
    {true, "www.appspot.com", "*.appspot.com"},
    {true, "foo.s3.amazonaws.com", "*.s3.amazonaws.com"},
    // Multiple wildcards are not valid.
    {false, "foo.example.com", "*.*.com"},
    {false, "foo.bar.example.com", "*.bar.*.com"},
    // Absolute vs relative DNS name tests. Although not explicitly specified
    // in RFC 6125, absolute reference names (those ending in a .) should
    // match either absolute or relative presented names.
    {true, "foo.com", "foo.com."},
    {true, "foo.com.", "foo.com"},
    {true, "foo.com.", "foo.com."},
    {true, "f", "f."},
    {true, "f.", "f"},
    {true, "f.", "f."},
    {true, "www-3.bar.foo.com", "*.bar.foo.com."},
    {true, "www-3.bar.foo.com.", "*.bar.foo.com"},
    {true, "www-3.bar.foo.com.", "*.bar.foo.com."},
    {false, ".", "."},
    {false, "example.com", "*.com."},
    {false, "example.com.", "*.com"},
    {false, "example.com.", "*.com."},
    {false, "foo.", "*."},
    {false, "foo", "*."},
    {false, "foo.co.uk", "*.co.uk."},
    {false, "foo.co.uk.", "*.co.uk."},
    // IP addresses in subject alternative name
    {true, "10.1.2.3", "", "10.1.2.3"},
    {true, "14.15", "", "14.0.0.15"},
    {false, "10.1.2.7", "", "10.1.2.6,10.1.2.8"},
    {false, "10.1.2.8", "foo"},
    {true, "::4.5.6.7", "", "x00000000000000000000000004050607"},
    {false, "::6.7.8.9", "::6.7.8.9",
     "x00000000000000000000000006070808,x0000000000000000000000000607080a,"
     "xff000000000000000000000006070809,6.7.8.9"},
    {true, "FE80::200:f8ff:fe21:67cf", "",
     "x00000000000000000000000006070808,xfe800000000000000200f8fffe2167cf,"
     "xff0000000000000000000000060708ff,10.0.0.1"},
    // Invalid hostnames with final numeric component.
    {false, "121.2.3.512", "1*1.2.3.512,*1.2.3.512,1*.2.3.512,*.2.3.512",
     "121.2.3.0"},
    {false, "1.2.3.4.5.6", "*.2.3.4.5.6"},
    {false, "1.2.3.4.5", "1.2.3.4.5"},
    {false, "a.0.0.1", "*.0.0.1"},
    // IP addresses in dNSName should not match commonName
    {false, "127.0.0.1", "127.0.0.1"},
    {false, "127.0.0.1", "*.0.0.1"},
    // Invalid host names.
    {false, ".", ""},
    {false, ".", "."},
    {false, "1.2.3.4..", "", "1.2.3.4"},
    {false, "www..domain.example", "www.domain.example"},
    {false, "www^domain.example", "www^domain.example"},
    {false, "www%20.domain.example", "www .domain.example"},
    {false, "www%2520.domain.example", "www .domain.example"},
    {false, "www%5E.domain.example", "www^domain.example"},
    {false, "www,domain.example", "www,domain.example"},
    {false, "0x000000002200037955161..", "0x000000002200037955161"},
    {false, "junk)(£)$*!@~#", "junk)(£)$*!@~#"},
    {false, "www.*.com", "www.*.com"},
    {false, "w$w.f.com", "w$w.f.com"},
    {false, "nocolonallowed:example", "nocolonallowed:example"},
    {false, "www-1.[::FFFF:129.144.52.38]", "*.[::FFFF:129.144.52.38]"},
    {false, "[::4.5.6.9]", "", "x00000000000000000000000004050609"},
};

class X509CertificateNameVerifyTest
    : public testing::TestWithParam<CertificateNameVerifyTestData> {
};

TEST_P(X509CertificateNameVerifyTest, VerifyHostname) {
  CertificateNameVerifyTestData test_data = GetParam();

  std::vector<std::string> dns_names, ip_addressses;
  if (test_data.dns_names) {
    // Build up the certificate DNS names list.
    std::string dns_name_line(test_data.dns_names);
    std::replace(dns_name_line.begin(), dns_name_line.end(), '#', '\0');
    dns_names = base::SplitString(dns_name_line, ",", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_ALL);
  }

  if (test_data.ip_addrs) {
    // Build up the certificate IP address list.
    std::string ip_addrs_line(test_data.ip_addrs);
    std::vector<std::string> ip_addressses_ascii = base::SplitString(
        ip_addrs_line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (size_t i = 0; i < ip_addressses_ascii.size(); ++i) {
      std::string& addr_ascii = ip_addressses_ascii[i];
      ASSERT_NE(0U, addr_ascii.length());
      if (addr_ascii[0] == 'x') {  // Hex encoded address
        addr_ascii.erase(0, 1);
        std::string bytes;
        EXPECT_TRUE(base::HexStringToString(addr_ascii, &bytes))
            << "Could not parse hex address " << addr_ascii << " i = " << i;
        ip_addressses.push_back(std::move(bytes));
        ASSERT_EQ(16U, ip_addressses.back().size()) << i;
      } else {  // Decimal groups
        std::vector<std::string> decimals_ascii_list = base::SplitString(
            addr_ascii, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
        EXPECT_EQ(4U, decimals_ascii_list.size()) << i;
        std::string addr_bytes;
        for (const auto& decimals_ascii : decimals_ascii_list) {
          int decimal_value;
          EXPECT_TRUE(base::StringToInt(decimals_ascii, &decimal_value));
          EXPECT_GE(decimal_value, 0);
          EXPECT_LE(decimal_value, 255);
          addr_bytes.push_back(static_cast<char>(decimal_value));
        }
        ip_addressses.push_back(addr_bytes);
        ASSERT_EQ(4U, ip_addressses.back().size()) << i;
      }
    }
  }

  EXPECT_EQ(test_data.expected,
            X509Certificate::VerifyHostname(test_data.hostname, dns_names,
                                            ip_addressses));
}

INSTANTIATE_TEST_SUITE_P(All,
                         X509CertificateNameVerifyTest,
                         testing::ValuesIn(kNameVerifyTestData));

const struct PublicKeyInfoTestData {
  const char* file_name;
  size_t expected_bits;
  X509Certificate::PublicKeyType expected_type;
} kPublicKeyInfoTestData[] = {
    {"rsa-768", 768, X509Certificate::kPublicKeyTypeRSA},
    {"rsa-1024", 1024, X509Certificate::kPublicKeyTypeRSA},
    {"rsa-2048", 2048, X509Certificate::kPublicKeyTypeRSA},
    {"rsa-8200", 8200, X509Certificate::kPublicKeyTypeRSA},
    {"ec-prime256v1", 256, X509Certificate::kPublicKeyTypeECDSA},
};

class X509CertificatePublicKeyInfoTest
    : public testing::TestWithParam<PublicKeyInfoTestData> {
};

TEST_P(X509CertificatePublicKeyInfoTest, GetPublicKeyInfo) {
  PublicKeyInfoTestData data = GetParam();

  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  ASSERT_TRUE(leaf->UseKeyFromFile(GetTestCertsDirectory().AppendASCII(
      base::StrCat({data.file_name, "-1.key"}))));

  size_t actual_bits = 0;
  X509Certificate::PublicKeyType actual_type =
      X509Certificate::kPublicKeyTypeUnknown;

  X509Certificate::GetPublicKeyInfo(leaf->GetCertBuffer(), &actual_bits,
                                    &actual_type);

  EXPECT_EQ(data.expected_bits, actual_bits);
  EXPECT_EQ(data.expected_type, actual_type);
}

INSTANTIATE_TEST_SUITE_P(All,
                         X509CertificatePublicKeyInfoTest,
                         testing::ValuesIn(kPublicKeyInfoTestData));

}  // namespace net
