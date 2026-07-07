// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/page_info/certificate/model/x509_certificate_model.h"

#include <string_view>

#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace x509_certificate_model {

namespace {

// Returns true if `attributes` contains an RDNAttribute whose `oid` matches
// `oid` and `value` matches `value`.
bool ContainsAttribute(
    const std::vector<X509CertificateModel::RDNAttribute>& attributes,
    std::string_view oid,
    std::string_view value) {
  for (const auto& attr : attributes) {
    if (attr.oid == oid && attr.value == value) {
      return true;
    }
  }
  return false;
}

}  // namespace

class X509CertificateModelTest : public PlatformTest {};

TEST_F(X509CertificateModelTest, InvalidCert) {
  X509CertificateModel model(net::x509_util::CreateCryptoBuffer(
      base::span<const uint8_t>({'b', 'a', 'd', '\n'})));

  EXPECT_EQ(
      "1D 7A 36 3C E1 24 30 88 1E C5 6C 9C F1 40 9C 49 C4 91 04 36 18 E5 "
      "98 C3 56 E2 95 90 40 87 2F 5A",
      model.HashCertSHA256());
  EXPECT_FALSE(model.is_valid());
}

TEST_F(X509CertificateModelTest, GetGoogleCertFields) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());

  EXPECT_EQ(
      "F6 41 C3 6C FE F4 9B C0 71 35 9E CF 88 EE D9 31 7B 73 8B 59 89 41 "
      "6A D4 01 72 0C 0A 4E 2E 63 52",
      model.HashCertSHA256());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(
      "23 A5 5C E6 8E A1 B2 06 23 DE 09 E9 3F DF 3B B0 32 87 AC 73 7B 27 "
      "33 5B 43 07 FE 9E C4 85 5C 34",
      model.HashSpkiSHA256());

  EXPECT_EQ("3", model.GetVersion());
  EXPECT_EQ("2F DF BC F6 AE 91 52 6D 0F 9A A3 DF 40 34 3E 9A",
            model.GetSerialNumberHexified());

  // Base-class single-attribute getters.
  EXPECT_EQ(OptionalStringOrError("Thawte SGC CA"),
            model.GetIssuerCommonName());
  EXPECT_EQ(OptionalStringOrError("Thawte Consulting (Pty) Ltd."),
            model.GetIssuerOrgName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetIssuerOrgUnitName());

  EXPECT_EQ(OptionalStringOrError("www.google.com"),
            model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError("Google Inc"), model.GetSubjectOrgName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgUnitName());

  // Full ordered list returned by the iOS subclass. Only check attributes not
  // already covered by the base-class single-attribute getters above.
  auto issuer = model.GetIssuerAttributesInOrder();
  // 2.5.4.6 = countryName.
  EXPECT_TRUE(ContainsAttribute(issuer, "2.5.4.6", "ZA"));

  auto subject = model.GetSubjectAttributesInOrder();
  // 2.5.4.6 = countryName.
  EXPECT_TRUE(ContainsAttribute(subject, "2.5.4.6", "US"));
  // 2.5.4.8 = stateOrProvinceName.
  EXPECT_TRUE(ContainsAttribute(subject, "2.5.4.8", "California"));
  // 2.5.4.7 = localityName.
  EXPECT_TRUE(ContainsAttribute(subject, "2.5.4.7", "Mountain View"));

  base::Time not_before, not_after;
  EXPECT_TRUE(model.GetTimes(&not_before, &not_after));
  // Constants copied from x509_certificate_unittest.cc.
  // Dec 18 00:00:00 2009 GMT
  const double kGoogleParseValidFrom = 1261094400;
  EXPECT_EQ(kGoogleParseValidFrom, not_before.InSecondsFSinceUnixEpoch());
  // Dec 18 23:59:59 2011 GMT
  const double kGoogleParseValidTo = 1324252799;
  EXPECT_EQ(kGoogleParseValidTo, not_after.InSecondsFSinceUnixEpoch());

  // Signature information.
  EXPECT_EQ("PKCS #1 SHA-1 With RSA Encryption", model.GetSignatureAlgorithm());
  EXPECT_EQ("None", model.GetSignatureParameters());
  EXPECT_EQ(
      "9F 43 CF 5B C4 50 29 B1 BF E2 B0 9A FF 6A 21 1D 2D 12 C3 2C 4E 5A "
      "F9 12 E2 CE B9 82 52 2D E7 1D 7E 1A 76 96 90 79 D1 24 52 38 79 BB "
      "63 8D 80 97 7C 23 20 0F 91 4D 16 B9 EA EE F4 6D 89 CA C6 BD CC 24 "
      "68 D6 43 5B CE 2A 58 BF 3C 18 E0 E0 3C 62 CF 96 02 2D 28 47 50 34 "
      "E1 27 BA CF 99 D1 50 FF 29 25 C0 36 36 15 33 52 70 BE 31 8F 9F E8 "
      "7F E7 11 0C 8D BF 84 A0 42 1A 80 89 B0 31 58 41 07 5F",
      model.GetSignatureData());
}

TEST_F(X509CertificateModelTest, GetNDNCertFields) {
  auto cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ndn.ca.crt");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ("1", model.GetVersion());
  // The model just returns the hex of the DER bytes, so the leading zeros are
  // included.
  EXPECT_EQ("00 DB B7 C6 06 47 AF 37 A2", model.GetSerialNumberHexified());

  EXPECT_EQ(OptionalStringOrError("New Dream Network Certificate Authority"),
            model.GetIssuerCommonName());
  EXPECT_EQ(OptionalStringOrError("New Dream Network, LLC"),
            model.GetIssuerOrgName());
  EXPECT_EQ(OptionalStringOrError("Security"), model.GetIssuerOrgUnitName());
  EXPECT_EQ(OptionalStringOrError("New Dream Network Certificate Authority"),
            model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError("New Dream Network, LLC"),
            model.GetSubjectOrgName());
  EXPECT_EQ(OptionalStringOrError("Security"), model.GetSubjectOrgUnitName());

  base::Time not_before, not_after;
  EXPECT_TRUE(model.GetTimes(&not_before, &not_after));
  EXPECT_EQ(12800754778, not_before.ToDeltaSinceWindowsEpoch().InSeconds());
  EXPECT_EQ(13116114778, not_after.ToDeltaSinceWindowsEpoch().InSeconds());
}

TEST_F(X509CertificateModelTest, PunyCodeCert) {
  auto cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "punycodetest.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(OptionalStringOrError("xn--wgv71a119e.com"),
            model.GetIssuerCommonName());
  EXPECT_EQ(OptionalStringOrError("xn--wgv71a119e.com"),
            model.GetSubjectCommonName());
}

TEST_F(X509CertificateModelTest, SubjectIA5StringInvalidCharacters) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE {
  //   SET {
  //     SEQUENCE {
  //       # commonName
  //       OBJECT_IDENTIFIER { 2.5.4.3 }
  //       # Not a valid IA5String:
  //       IA5String { "a \xf6 b" }
  //     }
  //   }
  // }
  const uint8_t kSubject[] = {0x30, 0x10, 0x31, 0x0e, 0x30, 0x0c,
                              0x06, 0x03, 0x55, 0x04, 0x03, 0x16,
                              0x05, 0x61, 0x20, 0xf6, 0x20, 0x62};
  builder->SetSubjectTLV(kSubject);

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  ASSERT_TRUE(model.is_valid());
  EXPECT_EQ(OptionalStringOrError(Error()), model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgUnitName());

  // The full ordered list still contains the single Common Name attribute, with
  // its value rendered as the hex fallback because the IA5String could not be
  // decoded.
  auto attrs = model.GetSubjectAttributesInOrder();
  ASSERT_EQ(1u, attrs.size());
  EXPECT_EQ("2.5.4.3", attrs[0].oid);
  EXPECT_EQ("61 20 F6 20 62", attrs[0].value);
}

TEST_F(X509CertificateModelTest, SubjectInvalid) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE { SET { } } -- empty RDN is invalid.
  const uint8_t kSubject[] = {0x30, 0x02, 0x31, 0x00};
  builder->SetSubjectTLV(kSubject);

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  EXPECT_FALSE(model.is_valid());
}

TEST_F(X509CertificateModelTest, SubjectEmptySequence) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE { } -- legal empty DN.
  const uint8_t kSubject[] = {0x30, 0x00};
  builder->SetSubjectTLV(kSubject);

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  ASSERT_TRUE(model.is_valid());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgUnitName());
  EXPECT_TRUE(model.GetSubjectAttributesInOrder().empty());
}

TEST_F(X509CertificateModelTest, SignatureParametersRawBytes) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE {
  //   OBJECT_IDENTIFIER { 1.2.840.113549.1.1.11 }  # sha256WithRSAEncryption
  //   OCTET_STRING { DE AD BE EF }                 # bogus, but non-NULL
  // }
  const uint8_t kAlg[] = {0x30, 0x11, 0x06, 0x09, 0x2A, 0x86, 0x48,
                          0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x04,
                          0x04, 0xDE, 0xAD, 0xBE, 0xEF};
  builder->SetSignatureAlgorithmTLV(base::as_string_view(kAlg));
  builder->SetTBSSignatureAlgorithmTLV(base::as_string_view(kAlg));

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  ASSERT_TRUE(model.is_valid());
  EXPECT_EQ("04 04 DE AD BE EF", model.GetSignatureParameters());
}

}  // namespace x509_certificate_model
