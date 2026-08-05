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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"

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

// Returns true if any directoryName GeneralName in `names` carries an RDN
// matching (`oid`, `value`) in its parsed `directory_name` list.
bool ContainsDirectoryNameAttribute(
    const std::vector<X509CertificateModel::GeneralName>& names,
    std::string_view oid,
    std::string_view value) {
  for (const auto& name : names) {
    if (name.type == X509CertificateModel::GeneralName::Type::kDirectoryName &&
        ContainsAttribute(name.directory_name, oid, value)) {
      return true;
    }
  }
  return false;
}

// Returns true if `names` contains a GeneralName whose `type` matches `type`
// and `value` matches `value`.
bool ContainsGeneralName(
    const std::vector<X509CertificateModel::GeneralName>& names,
    X509CertificateModel::GeneralName::Type type,
    std::string_view value) {
  for (const auto& name : names) {
    if (name.type == type && name.value == value) {
      return true;
    }
  }
  return false;
}

// Builds a valid certificate from ok_cert.pem carrying a single extension with
// the given `oid` and raw `value`, and returns the parsed model. Used by the
// "...Invalid" tests to exercise a malformed extension.
X509CertificateModel ModelWithExtension(bssl::der::Input oid,
                                        base::span<const uint8_t> value) {
  std::unique_ptr<net::CertBuilder> builder = net::CertBuilder::FromFile(
      net::GetTestCertsDirectory().AppendASCII("ok_cert.pem"), nullptr);
  CHECK(builder);
  builder->SetExtension(oid, std::string(base::as_string_view(value)),
                        /*critical=*/false);
  return X509CertificateModel(bssl::UpRef(builder->GetCertBuffer()));
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

  // Subject Public Key Info.
  EXPECT_EQ("PKCS #1 RSA Encryption", model.GetPublicKeyAlgorithm());
  EXPECT_EQ("None", model.GetPublicKeyParameters());
  std::optional<size_t> public_key_size = model.GetPublicKeySize();
  EXPECT_TRUE(public_key_size.has_value());
  EXPECT_EQ(1024u, public_key_size.value());
  EXPECT_EQ(
      "30 81 89 02 81 81 00 E8 F9 86 0F 90 FA 86 D7 DF BD 72 26 B6 D7 44 "
      "02 83 78 73 D9 02 28 EF 88 45 39 FB 10 E8 7C AE A9 38 D5 75 C6 38 "
      "EB 0A 15 07 9B 83 E8 CD 82 D5 E3 F7 15 68 45 A1 0B 19 85 BC E2 EF "
      "84 E7 DD F2 D7 B8 98 C2 A1 BB B5 C1 51 DF D4 83 02 A7 3D 06 42 5B "
      "E1 22 C3 DE 6B 85 5F 1C D6 DA 4E 8B D3 9B EE B9 67 22 2A 1D 11 EF "
      "79 A4 B3 37 8A F4 FE 18 FD BC F9 46 23 50 97 F3 AC FC 24 46 2B 5C "
      "3B B7 45 02 03 01 00 01",
      model.GetPublicKeyData());

  // google.single.pem has 4 extensions in this DER order: Basic Constraints,
  // CRL Distribution Points, Extended Key Usage, and Authority Information
  // Access.
  EXPECT_THAT(
      model.GetExtensionOidsInOrder(),
      testing::ElementsAre(bssl::der::Input(bssl::kBasicConstraintsOid),
                           bssl::der::Input(bssl::kCrlDistributionPointsOid),
                           bssl::der::Input(bssl::kExtKeyUsageOid),
                           bssl::der::Input(bssl::kAuthorityInfoAccessOid)));

  EXPECT_TRUE(model.IsBasicConstraintsCritical());
  EXPECT_FALSE(model.IsBasicConstraintsCA());
  EXPECT_FALSE(model.GetBasicConstraintsPathLen().has_value());

  EXPECT_FALSE(model.IsCRLDistributionPointsCritical());
  auto crl_dps = model.GetCRLDistributionPointsFullNames();
  ASSERT_TRUE(crl_dps.has_value());
  ASSERT_EQ(1u, crl_dps->size());
  EXPECT_EQ(X509CertificateModel::GeneralName::Type::kURI, (*crl_dps)[0].type);
  EXPECT_EQ("http://crl.thawte.com/ThawteSGCCA.crl", (*crl_dps)[0].value);

  EXPECT_FALSE(model.IsExtendedKeyUsageCritical());
  auto eku_purposes = model.GetExtendedKeyUsagePurposes();
  ASSERT_TRUE(eku_purposes.has_value());
  ASSERT_EQ(3u, eku_purposes->size());
  EXPECT_EQ("Server Authentication", (*eku_purposes)[0]);
  EXPECT_EQ("Client Authentication", (*eku_purposes)[1]);
  // Unknown OID: Netscape Server Gated Crypto (2.16.840.1.113730.4.1) falls
  // back to dotted decimal notation.
  EXPECT_EQ("2.16.840.1.113730.4.1", (*eku_purposes)[2]);

  // Two AccessDescriptions in DER order: OCSP then CA Issuers, both URIs.
  EXPECT_FALSE(model.IsAuthorityInformationAccessCritical());
  auto aia = model.GetAuthorityInformationAccess();
  ASSERT_TRUE(aia.has_value());
  ASSERT_EQ(2u, aia->size());
  EXPECT_EQ("OCSP", (*aia)[0].method);
  ASSERT_TRUE((*aia)[0].location.has_value());
  EXPECT_EQ(X509CertificateModel::GeneralName::Type::kURI,
            (*aia)[0].location->type);
  EXPECT_EQ("http://ocsp.thawte.com", (*aia)[0].location->value);
  EXPECT_EQ("CA Issuers", (*aia)[1].method);
  ASSERT_TRUE((*aia)[1].location.has_value());
  EXPECT_EQ(X509CertificateModel::GeneralName::Type::kURI,
            (*aia)[1].location->type);
  EXPECT_EQ("http://www.thawte.com/repository/Thawte_SGC_CA.crt",
            (*aia)[1].location->value);
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

TEST_F(X509CertificateModelTest, ECPublicKey) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);
  builder->GenerateECKey();
  // This is a self-signed cert, so the signature algorithm must match the new
  // key type, otherwise signing fails.
  builder->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kEcdsaSha256);

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ("Elliptic Curve Public Key", model.GetPublicKeyAlgorithm());
  // The DER-encoded named curve OID for prime256v1 (1.2.840.10045.3.1.7).
  EXPECT_EQ("06 08 2A 86 48 CE 3D 03 01 07", model.GetPublicKeyParameters());
  std::optional<size_t> public_key_size = model.GetPublicKeySize();
  EXPECT_TRUE(public_key_size.has_value());
  EXPECT_EQ(256u, public_key_size.value());
  EXPECT_FALSE(model.GetPublicKeyData().empty());
}

TEST_F(X509CertificateModelTest, Mldsa44PublicKey) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);
  builder->GenerateMldsa44Key();
  // This is a self-signed cert, so the signature algorithm must match the new
  // key type, otherwise signing fails.
  builder->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kMldsa44);

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ("ML-DSA-44", model.GetPublicKeyAlgorithm());
  EXPECT_EQ("None", model.GetPublicKeyParameters());
  std::optional<size_t> public_key_size = model.GetPublicKeySize();
  EXPECT_TRUE(public_key_size.has_value());
  EXPECT_EQ(10496u, public_key_size.value());
  EXPECT_FALSE(model.GetPublicKeyData().empty());
}

TEST_F(X509CertificateModelTest, GlobalsignComCert) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "2029_globalsign_com_cert.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  // The cert has 9 extensions in this DER order. The last one, Netscape Cert
  // Type (2.16.840.1.113730.1.1), has no BoringSSL OID constant.
  static constexpr uint8_t kNetscapeCertTypeOid[] = {
      0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42, 0x01, 0x01};
  EXPECT_THAT(
      model.GetExtensionOidsInOrder(),
      testing::ElementsAre(bssl::der::Input(bssl::kSubjectKeyIdentifierOid),
                           bssl::der::Input(bssl::kAuthorityKeyIdentifierOid),
                           bssl::der::Input(bssl::kAuthorityInfoAccessOid),
                           bssl::der::Input(bssl::kCrlDistributionPointsOid),
                           bssl::der::Input(bssl::kBasicConstraintsOid),
                           bssl::der::Input(bssl::kKeyUsageOid),
                           bssl::der::Input(bssl::kExtKeyUsageOid),
                           bssl::der::Input(bssl::kCertificatePoliciesOid),
                           bssl::der::Input(kNetscapeCertTypeOid)));

  EXPECT_FALSE(model.IsBasicConstraintsCritical());
  EXPECT_FALSE(model.IsBasicConstraintsCA());
  EXPECT_FALSE(model.GetBasicConstraintsPathLen().has_value());

  EXPECT_TRUE(model.IsKeyUsageCritical());
  EXPECT_EQ(
      "Digital Signature, Non-repudiation, Key Encipherment, Data Encipherment",
      model.GetKeyUsageString());

  EXPECT_FALSE(model.IsExtendedKeyUsageCritical());
  auto eku_purposes = model.GetExtendedKeyUsagePurposes();
  ASSERT_TRUE(eku_purposes.has_value());
  ASSERT_EQ(2u, eku_purposes->size());
  EXPECT_EQ("Server Authentication", (*eku_purposes)[0]);
  EXPECT_EQ("Client Authentication", (*eku_purposes)[1]);

  EXPECT_FALSE(model.IsSubjectKeyIdentifierCritical());
  EXPECT_EQ("59 BC D9 69 F7 B0 65 BB C8 34 C5 D2 C2 EF 17 78 A6 47 1E 8B",
            model.GetSubjectKeyIdentifier());

  EXPECT_FALSE(model.IsAuthorityKeyIdentifierCritical());
  EXPECT_EQ("8A FC 14 1B 3D A3 59 67 A5 3B E1 73 92 A6 62 91 7F E4 78 30",
            model.GetAuthorityKeyIdentifier());
  // The extension decodes, but authorityCertIssuer / serial are absent: present
  // but empty.
  auto aki_issuer = model.GetAuthorityKeyIdentifierIssuer();
  ASSERT_TRUE(aki_issuer.has_value());
  EXPECT_TRUE(aki_issuer->empty());
  EXPECT_EQ("", model.GetAuthorityKeyIdentifierSerial());

  // A single CA Issuers AccessDescription with a URI location.
  EXPECT_FALSE(model.IsAuthorityInformationAccessCritical());
  auto aia = model.GetAuthorityInformationAccess();
  ASSERT_TRUE(aia.has_value());
  ASSERT_EQ(1u, aia->size());
  EXPECT_EQ("CA Issuers", (*aia)[0].method);
  ASSERT_TRUE((*aia)[0].location.has_value());
  EXPECT_EQ(X509CertificateModel::GeneralName::Type::kURI,
            (*aia)[0].location->type);
  EXPECT_EQ("http://secure.globalsign.net/cacert/SHA256extendval1.crt",
            (*aia)[0].location->value);

  EXPECT_FALSE(model.IsCRLDistributionPointsCritical());
  auto crl_dps = model.GetCRLDistributionPointsFullNames();
  ASSERT_TRUE(crl_dps.has_value());
  ASSERT_EQ(1u, crl_dps->size());
  EXPECT_EQ(X509CertificateModel::GeneralName::Type::kURI, (*crl_dps)[0].type);
  EXPECT_EQ("http://crl.globalsign.net/SHA256ExtendVal1.crl",
            (*crl_dps)[0].value);

  // A single policy with one CPS pointer qualifier.
  EXPECT_FALSE(model.IsCertificatePoliciesCritical());
  auto policies = model.GetCertificatePolicies();
  EXPECT_FALSE(policies.has_error);
  ASSERT_EQ(1u, policies.policies.size());
  EXPECT_EQ("1.3.6.1.4.1.4146.1.1", policies.policies[0].policy_oid);
  ASSERT_EQ(1u, policies.policies[0].qualifiers.size());
  EXPECT_EQ(X509CertificateModel::PolicyQualifier::Type::kCpsUri,
            policies.policies[0].qualifiers[0].type);
  EXPECT_EQ("http://www.globalsign.net/repository/",
            policies.policies[0].qualifiers[0].cps_uri);
}

TEST_F(X509CertificateModelTest, DiginotarCert) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "diginotar_public_ca_2025.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  // The cert has 7 extensions in this DER order.
  EXPECT_THAT(
      model.GetExtensionOidsInOrder(),
      testing::ElementsAre(bssl::der::Input(bssl::kAuthorityInfoAccessOid),
                           bssl::der::Input(bssl::kAuthorityKeyIdentifierOid),
                           bssl::der::Input(bssl::kBasicConstraintsOid),
                           bssl::der::Input(bssl::kCertificatePoliciesOid),
                           bssl::der::Input(bssl::kCrlDistributionPointsOid),
                           bssl::der::Input(bssl::kKeyUsageOid),
                           bssl::der::Input(bssl::kSubjectKeyIdentifierOid)));

  EXPECT_TRUE(model.IsBasicConstraintsCritical());
  EXPECT_TRUE(model.IsBasicConstraintsCA());
  std::optional<uint8_t> path_len = model.GetBasicConstraintsPathLen();
  ASSERT_TRUE(path_len.has_value());
  EXPECT_EQ(0u, path_len.value());

  EXPECT_FALSE(model.IsSubjectKeyIdentifierCritical());
  EXPECT_EQ("DF 33 C0 AF 92 FE 37 FC B6 D8 16 16 D0 D9 B1 91 D5 FA 6E A5",
            model.GetSubjectKeyIdentifier());

  EXPECT_FALSE(model.IsAuthorityKeyIdentifierCritical());
  EXPECT_EQ("88 68 BF E0 8E 35 C4 3B 38 6B 62 F7 28 3B 84 81 C8 0C D7 4D",
            model.GetAuthorityKeyIdentifier());

  // A single OCSP AccessDescription with a URI location.
  EXPECT_FALSE(model.IsAuthorityInformationAccessCritical());
  auto aia = model.GetAuthorityInformationAccess();
  ASSERT_TRUE(aia.has_value());
  ASSERT_EQ(1u, aia->size());
  EXPECT_EQ("OCSP", (*aia)[0].method);
  ASSERT_TRUE((*aia)[0].location.has_value());
  EXPECT_EQ(X509CertificateModel::GeneralName::Type::kURI,
            (*aia)[0].location->type);
  EXPECT_EQ("http://validation.diginotar.nl", (*aia)[0].location->value);

  EXPECT_FALSE(model.IsCertificatePoliciesCritical());
  auto policies = model.GetCertificatePolicies();
  EXPECT_FALSE(policies.has_error);
  ASSERT_EQ(1u, policies.policies.size());
  EXPECT_EQ("2.16.528.1.1001.1.1.1.1.5.2.6.4", policies.policies[0].policy_oid);
  ASSERT_EQ(2u, policies.policies[0].qualifiers.size());

  using PolicyQualifier = X509CertificateModel::PolicyQualifier;
  EXPECT_EQ(PolicyQualifier::Type::kCpsUri,
            policies.policies[0].qualifiers[0].type);
  EXPECT_EQ("http://www.diginotar.nl/cps",
            policies.policies[0].qualifiers[0].cps_uri);

  EXPECT_EQ(PolicyQualifier::Type::kUserNotice,
            policies.policies[0].qualifiers[1].type);
  ASSERT_TRUE(policies.policies[0].qualifiers[1].user_notice.has_value());
  const auto& notice = *policies.policies[0].qualifiers[1].user_notice;
  EXPECT_TRUE(notice.organization.empty());
  EXPECT_TRUE(notice.notice_numbers.empty());
  EXPECT_EQ(
      "Conditions, as mentioned on our website (www.diginotar.nl), are "
      "applicable to all our products and services.",
      notice.explicit_text);
}

TEST_F(X509CertificateModelTest, DiginotarCyberCa) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "diginotar_cyber_ca.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  // The cert has 6 extensions in this DER order.
  EXPECT_THAT(
      model.GetExtensionOidsInOrder(),
      testing::ElementsAre(bssl::der::Input(bssl::kBasicConstraintsOid),
                           bssl::der::Input(bssl::kCertificatePoliciesOid),
                           bssl::der::Input(bssl::kKeyUsageOid),
                           bssl::der::Input(bssl::kAuthorityKeyIdentifierOid),
                           bssl::der::Input(bssl::kCrlDistributionPointsOid),
                           bssl::der::Input(bssl::kSubjectKeyIdentifierOid)));

  EXPECT_TRUE(model.IsBasicConstraintsCritical());
  EXPECT_TRUE(model.IsBasicConstraintsCA());
  std::optional<uint8_t> path_len = model.GetBasicConstraintsPathLen();
  ASSERT_TRUE(path_len.has_value());
  EXPECT_EQ(1u, path_len.value());

  EXPECT_TRUE(model.IsKeyUsageCritical());
  EXPECT_EQ("Certificate Signer, CRL Signer", model.GetKeyUsageString());

  EXPECT_FALSE(model.IsSubjectKeyIdentifierCritical());
  EXPECT_EQ("AB F9 68 DF CF 4A 37 D7 7B 45 8C 5F 72 DE 40 44 C3 65 BB C2",
            model.GetSubjectKeyIdentifier());

  EXPECT_FALSE(model.IsAuthorityKeyIdentifierCritical());
  EXPECT_EQ("A6 0C 1D 9F 61 FF 07 17 B5 BF 38 46 DB 43 30 D5 8E B0 52 06",
            model.GetAuthorityKeyIdentifier());
}

TEST_F(X509CertificateModelTest, SubjectKeyIdentifierInvalid) {
  // extnValue is an INTEGER (02) where RFC 5280 requires an OCTET STRING (04).
  const uint8_t kSubjectKeyId[] = {0x02, 0x04, 0xde, 0xad, 0xbe, 0xef};
  X509CertificateModel model = ModelWithExtension(
      bssl::der::Input(bssl::kSubjectKeyIdentifierOid), kSubjectKeyId);
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(std::nullopt, model.GetSubjectKeyIdentifier());
  EXPECT_EQ("02 04 DE AD BE EF", model.GetSubjectKeyIdentifierRaw());
}

// A KeyUsage that is not a BIT STRING fails to decode.
TEST_F(X509CertificateModelTest, KeyUsageInvalid) {
  // INTEGER where a BIT STRING is required.
  const uint8_t kKeyUsage[] = {0x02, 0x02, 0x05, 0xa0};
  X509CertificateModel model =
      ModelWithExtension(bssl::der::Input(bssl::kKeyUsageOid), kKeyUsage);
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(std::nullopt, model.GetKeyUsageString());
  EXPECT_EQ("02 02 05 A0", model.GetKeyUsageStringRaw());
}

// An ExtendedKeyUsage that is not a SEQUENCE fails to decode.
TEST_F(X509CertificateModelTest, ExtendedKeyUsageInvalid) {
  // INTEGER where a SEQUENCE OF OID is required.
  const uint8_t kEku[] = {0x02, 0x01, 0x2a};
  X509CertificateModel model =
      ModelWithExtension(bssl::der::Input(bssl::kExtKeyUsageOid), kEku);
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(std::nullopt, model.GetExtendedKeyUsagePurposes());
  EXPECT_EQ("02 01 2A", model.GetExtendedKeyUsagePurposesRaw());
}

// A CRLDistributionPoints that is not a SEQUENCE fails to decode.
TEST_F(X509CertificateModelTest, CRLDistributionPointsInvalid) {
  // INTEGER where a SEQUENCE is required.
  const uint8_t kCrldp[] = {0x02, 0x01, 0x2a};
  X509CertificateModel model = ModelWithExtension(
      bssl::der::Input(bssl::kCrlDistributionPointsOid), kCrldp);
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(std::nullopt, model.GetCRLDistributionPointsFullNames());
  EXPECT_EQ("02 01 2A", model.GetCRLDistributionPointsFullNamesRaw());
}

// An IssuerAlternativeName that is not a SEQUENCE of GeneralName fails.
TEST_F(X509CertificateModelTest, IssuerAlternativeNameInvalid) {
  // INTEGER where a SEQUENCE OF GeneralName is required.
  const uint8_t kIan[] = {0x02, 0x01, 0x2a};
  X509CertificateModel model =
      ModelWithExtension(bssl::der::Input(kIssuerAltNameOid), kIan);
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(std::nullopt, model.GetIssuerAlternativeNames());
  EXPECT_EQ("02 01 2A", model.GetIssuerAlternativeNamesRaw());
}

// An AuthorityKeyIdentifier that is not a SEQUENCE fails to decode: all three
// getters return nullopt and the shared Raw getter returns the raw bytes.
TEST_F(X509CertificateModelTest, AuthorityKeyIdentifierInvalid) {
  // INTEGER where a SEQUENCE is required.
  const uint8_t kAki[] = {0x02, 0x01, 0x2a};
  X509CertificateModel model = ModelWithExtension(
      bssl::der::Input(bssl::kAuthorityKeyIdentifierOid), kAki);
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(std::nullopt, model.GetAuthorityKeyIdentifier());
  EXPECT_EQ(std::nullopt, model.GetAuthorityKeyIdentifierIssuer());
  EXPECT_EQ(std::nullopt, model.GetAuthorityKeyIdentifierSerial());
  EXPECT_EQ("02 01 2A", model.GetAuthorityKeyIdentifierRaw());
}

// An AuthorityInformationAccess that is not a SEQUENCE fails at the top level.
TEST_F(X509CertificateModelTest, AuthorityInformationAccessInvalid) {
  // INTEGER where a SEQUENCE is required.
  const uint8_t kAia[] = {0x02, 0x01, 0x2a};
  X509CertificateModel model =
      ModelWithExtension(bssl::der::Input(bssl::kAuthorityInfoAccessOid), kAia);
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(std::nullopt, model.GetAuthorityInformationAccess());
  EXPECT_EQ("02 01 2A", model.GetAuthorityInformationAccessRaw());
}

// A single AccessDescription whose accessLocation is a malformed GeneralName is
// kept with its method but a nullopt location; the others decode normally and
// the extension does not fail at the top level.
TEST_F(X509CertificateModelTest, AuthorityInformationAccessInvalidLocation) {
  // Four AccessDescriptions; the 3rd's accessLocation uses GeneralName tag [9],
  // which is not a defined choice, so only that location fails to decode.
  const uint8_t kAia[] = {
      0x30, 0x81, 0x9f, 0x30, 0x25, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05,
      0x07, 0x30, 0x01, 0x86, 0x19, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f,
      0x6f, 0x63, 0x73, 0x70, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65,
      0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x31, 0x30, 0x27, 0x06, 0x08, 0x2b, 0x06,
      0x01, 0x05, 0x05, 0x07, 0x30, 0x02, 0x86, 0x1b, 0x68, 0x74, 0x74, 0x70,
      0x3a, 0x2f, 0x2f, 0x63, 0x61, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c,
      0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x32, 0x2e, 0x63, 0x72, 0x74, 0x30,
      0x24, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x89,
      0x18, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x62, 0x61, 0x64, 0x2e,
      0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f,
      0x33, 0x30, 0x27, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30,
      0x02, 0x86, 0x1b, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x63, 0x61,
      0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d,
      0x2f, 0x34, 0x2e, 0x63, 0x72, 0x74};
  X509CertificateModel model =
      ModelWithExtension(bssl::der::Input(bssl::kAuthorityInfoAccessOid), kAia);
  ASSERT_TRUE(model.is_valid());

  auto aia = model.GetAuthorityInformationAccess();
  ASSERT_TRUE(aia.has_value());
  ASSERT_EQ(4u, aia->size());

  // Items 1, 2, 4 decode normally.
  ASSERT_TRUE((*aia)[0].location.has_value());
  EXPECT_EQ("http://ocsp.example.com/1", (*aia)[0].location->value);
  ASSERT_TRUE((*aia)[1].location.has_value());
  EXPECT_EQ("http://ca.example.com/2.crt", (*aia)[1].location->value);
  ASSERT_TRUE((*aia)[3].location.has_value());
  EXPECT_EQ("http://ca.example.com/4.crt", (*aia)[3].location->value);

  // Item 3 keeps its method but has no decoded location; its raw bytes are
  // surfaced instead.
  EXPECT_EQ("OCSP", (*aia)[2].method);
  EXPECT_FALSE((*aia)[2].location.has_value());
  EXPECT_EQ(
      "89 18 68 74 74 70 3A 2F 2F 62 61 64 2E 65 78 61 6D 70 6C 65 2E 63 6F "
      "6D 2F 33",
      (*aia)[2].raw_location);
}

TEST_F(X509CertificateModelTest, AuthorityKeyIdentifierAllFields) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "diginotar_cyber_ca.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ("A6 0C 1D 9F 61 FF 07 17 B5 BF 38 46 DB 43 30 D5 8E B0 52 06",
            model.GetAuthorityKeyIdentifier());

  auto aki_issuer = model.GetAuthorityKeyIdentifierIssuer();
  ASSERT_TRUE(aki_issuer.has_value());
  EXPECT_FALSE(aki_issuer->empty());
  // 2.5.4.3 = commonName.
  EXPECT_TRUE(ContainsDirectoryNameAttribute(*aki_issuer, "2.5.4.3",
                                             "GTE CyberTrust Global Root"));
  // 2.5.4.6 = countryName.
  EXPECT_TRUE(ContainsDirectoryNameAttribute(*aki_issuer, "2.5.4.6", "US"));
  // 2.5.4.10 = organizationName.
  EXPECT_TRUE(ContainsDirectoryNameAttribute(*aki_issuer, "2.5.4.10",
                                             "GTE Corporation"));
  // 2.5.4.11 = organizationalUnitName.
  EXPECT_TRUE(ContainsDirectoryNameAttribute(*aki_issuer, "2.5.4.11",
                                             "GTE CyberTrust Solutions, Inc."));

  EXPECT_EQ("01 A5", model.GetAuthorityKeyIdentifierSerial());
}

TEST_F(X509CertificateModelTest,
       AuthorityInfoAccessNonstandardOidAndLocationType) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE {
  //  SEQUENCE {
  //    OBJECT_IDENTIFIER { 1.4.9.20 }
  //    [1 PRIMITIVE] { "foo@example.com" }  -- rfc822Name
  //  }
  // }
  const uint8_t kAIA[] = {0x30, 0x18, 0x30, 0x16, 0x06, 0x03, 0x2c, 0x09, 0x14,
                          0x81, 0x0f, 0x66, 0x6f, 0x6f, 0x40, 0x65, 0x78, 0x61,
                          0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d};
  builder->SetExtension(bssl::der::Input(bssl::kAuthorityInfoAccessOid),
                        std::string(base::as_string_view(kAIA)));

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  ASSERT_TRUE(model.is_valid());

  auto aia = model.GetAuthorityInformationAccess();
  ASSERT_TRUE(aia.has_value());
  ASSERT_EQ(1u, aia->size());
  // Unknown accessMethod OID falls back to dotted decimal notation.
  EXPECT_EQ("1.4.9.20", (*aia)[0].method);
  ASSERT_TRUE((*aia)[0].location.has_value());
  EXPECT_EQ(X509CertificateModel::GeneralName::Type::kRFC822Name,
            (*aia)[0].location->type);
  EXPECT_EQ("foo@example.com", (*aia)[0].location->value);
}

TEST_F(X509CertificateModelTest, SubjectAltNameSanityTest) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "subjectAltName_sanity_check.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_FALSE(model.IsSubjectAlternativeNameCritical());

  // subjectAltName_sanity_check.pem carries: IP 127.0.0.2, IP fe80::1,
  // DNS test.example, email test@test.example, otherName 1.2.3.4,
  // DirName CN=127.0.0.3.
  using GeneralName = X509CertificateModel::GeneralName;
  auto names_opt = model.GetSubjectAlternativeNames();
  ASSERT_TRUE(names_opt.has_value());
  const auto& names = *names_opt;

  EXPECT_TRUE(
      ContainsGeneralName(names, GeneralName::Type::kIPAddress, "127.0.0.2"));
  EXPECT_TRUE(
      ContainsGeneralName(names, GeneralName::Type::kIPAddress, "fe80::1"));
  EXPECT_TRUE(
      ContainsGeneralName(names, GeneralName::Type::kDNSName, "test.example"));
  EXPECT_TRUE(ContainsGeneralName(names, GeneralName::Type::kRFC822Name,
                                  "test@test.example"));

  // otherName: type-id 1.2.3.4, value decoded from the inner UTF8String.
  const GeneralName* other_name = nullptr;
  for (const auto& name : names) {
    if (name.type == GeneralName::Type::kOtherName) {
      other_name = &name;
      break;
    }
  }
  ASSERT_NE(other_name, nullptr);
  EXPECT_EQ("1.2.3.4", other_name->other_name_oid);
  EXPECT_EQ("ignore me", other_name->value);

  // The directoryName entry carries parsed RDNs in `directory_name`, not in
  // `value`; assert via the RDN helper. 2.5.4.3 = commonName.
  EXPECT_TRUE(ContainsDirectoryNameAttribute(names, "2.5.4.3", "127.0.0.3"));
}

TEST_F(X509CertificateModelTest, IssuerAltNameTest) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE {                                    -- GeneralNames
  //   [2 PRIMITIVE] { "test.example" }            -- dNSName
  //   [1 PRIMITIVE] { "test@test.example" }       -- rfc822Name
  //   [6 PRIMITIVE] { "http://test.example/" }    -- URI
  //   [7 PRIMITIVE] { 7F 00 00 02 }               -- iPAddress 127.0.0.2
  // }
  const uint8_t kIAN[] = {0x30, 0x3d, 0x82, 0x0c, 0x74, 0x65, 0x73, 0x74, 0x2e,
                          0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x81, 0x11,
                          0x74, 0x65, 0x73, 0x74, 0x40, 0x74, 0x65, 0x73, 0x74,
                          0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x86,
                          0x14, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x74,
                          0x65, 0x73, 0x74, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70,
                          0x6c, 0x65, 0x2f, 0x87, 0x04, 0x7f, 0x00, 0x00, 0x02};
  builder->SetExtension(bssl::der::Input(kIssuerAltNameOid),
                        std::string(base::as_string_view(kIAN)));

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  ASSERT_TRUE(model.is_valid());

  EXPECT_FALSE(model.IsIssuerAlternativeNameCritical());

  using GeneralName = X509CertificateModel::GeneralName;
  auto names = model.GetIssuerAlternativeNames();
  ASSERT_TRUE(names.has_value());
  ASSERT_EQ(4u, names->size());
  EXPECT_TRUE(
      ContainsGeneralName(*names, GeneralName::Type::kDNSName, "test.example"));
  EXPECT_TRUE(ContainsGeneralName(*names, GeneralName::Type::kRFC822Name,
                                  "test@test.example"));
  EXPECT_TRUE(ContainsGeneralName(*names, GeneralName::Type::kURI,
                                  "http://test.example/"));
  EXPECT_TRUE(
      ContainsGeneralName(*names, GeneralName::Type::kIPAddress, "127.0.0.2"));
}

TEST_F(X509CertificateModelTest, CertificatePoliciesSanityTest) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "policies_sanity_check.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_FALSE(model.IsCertificatePoliciesCritical());

  using PolicyQualifier = X509CertificateModel::PolicyQualifier;
  auto result = model.GetCertificatePolicies();
  // Fully valid extension: no error
  EXPECT_FALSE(result.has_error);
  EXPECT_FALSE(result.raw_der.empty());
  const auto& policies = result.policies;
  ASSERT_EQ(2u, policies.size());

  // First policy: bare OID with no qualifiers.
  EXPECT_EQ("1.2.3.4.5", policies[0].policy_oid);
  EXPECT_TRUE(policies[0].qualifiers.empty());

  // Second policy: one CPS pointer followed by three UserNotice variants.
  EXPECT_EQ("1.3.5.8.12", policies[1].policy_oid);
  ASSERT_EQ(4u, policies[1].qualifiers.size());

  // Qualifier 0: CPS URI.
  EXPECT_EQ(PolicyQualifier::Type::kCpsUri, policies[1].qualifiers[0].type);
  EXPECT_EQ("http://cps.example.com/foo", policies[1].qualifiers[0].cps_uri);

  // Qualifier 1: UserNotice with organization, notice numbers, and text.
  EXPECT_EQ(PolicyQualifier::Type::kUserNotice, policies[1].qualifiers[1].type);
  ASSERT_TRUE(policies[1].qualifiers[1].user_notice.has_value());
  const auto& notice1 = *policies[1].qualifiers[1].user_notice;
  EXPECT_EQ("Organization Name", notice1.organization);
  EXPECT_THAT(notice1.notice_numbers, testing::ElementsAre("1", "2", "3", "4"));
  EXPECT_EQ("Explicit Text Here", notice1.explicit_text);

  // Qualifier 2: UserNotice with only explicitText.
  EXPECT_EQ(PolicyQualifier::Type::kUserNotice, policies[1].qualifiers[2].type);
  ASSERT_TRUE(policies[1].qualifiers[2].user_notice.has_value());
  const auto& notice2 = *policies[1].qualifiers[2].user_notice;
  EXPECT_TRUE(notice2.organization.empty());
  EXPECT_TRUE(notice2.notice_numbers.empty());
  EXPECT_EQ("Explicit Text Two", notice2.explicit_text);

  // Qualifier 3: UserNotice with organization and a single notice number, but
  // no explicitText.
  EXPECT_EQ(PolicyQualifier::Type::kUserNotice, policies[1].qualifiers[3].type);
  ASSERT_TRUE(policies[1].qualifiers[3].user_notice.has_value());
  const auto& notice3 = *policies[1].qualifiers[3].user_notice;
  EXPECT_EQ("Organization Name Two", notice3.organization);
  EXPECT_THAT(notice3.notice_numbers, testing::ElementsAre("42"));
  EXPECT_TRUE(notice3.explicit_text.empty());
}

// A UserNotice explicitText that is not valid UTF-8 makes the qualifier fail to
// decode. Decoding stops: the failing policy is retained (its UserNotice
// qualifier kept with no decoded notice), has_error is set, and raw_der holds a
// hex dump of the whole extension.
TEST_F(X509CertificateModelTest, CertificatePoliciesInvalidUtf8UserNotice) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // \xa1 is a UTF-8 continuation byte with no leading byte, which is invalid.
  //
  // SEQUENCE {
  //   SEQUENCE {
  //     OBJECT_IDENTIFIER { 1.2.3 }
  //     SEQUENCE {
  //       SEQUENCE {
  //         OBJECT_IDENTIFIER { 1.3.6.1.5.5.7.2.2 }  # unotice
  //         SEQUENCE {
  //           UTF8String { "Explicit \xa1 Text" }    # explicitText
  //         }
  //       }
  //     }
  //   }
  // }
  const uint8_t kPolicies[] = {
      0x30, 0x27, 0x30, 0x25, 0x06, 0x02, 0x2a, 0x03, 0x30, 0x1f, 0x30,
      0x1d, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x02,
      0x30, 0x11, 0x0c, 0x0f, 0x45, 0x78, 0x70, 0x6c, 0x69, 0x63, 0x69,
      0x74, 0x20, 0xa1, 0x20, 0x54, 0x65, 0x78, 0x74};
  builder->SetExtension(bssl::der::Input(bssl::kCertificatePoliciesOid),
                        std::string(base::as_string_view(kPolicies)),
                        /*critical=*/false);

  X509CertificateModel model(bssl::UpRef(builder->GetCertBuffer()));
  ASSERT_TRUE(model.is_valid());

  using PolicyQualifier = X509CertificateModel::PolicyQualifier;
  auto result = model.GetCertificatePolicies();

  // The UserNotice failed to decode, so has_error is set.
  EXPECT_TRUE(result.has_error);

  // The failing policy is retained, with its UserNotice qualifier kept but not
  // decoded.
  ASSERT_EQ(1u, result.policies.size());
  EXPECT_EQ("1.2.3", result.policies[0].policy_oid);
  ASSERT_EQ(1u, result.policies[0].qualifiers.size());
  EXPECT_EQ(PolicyQualifier::Type::kUserNotice,
            result.policies[0].qualifiers[0].type);
  EXPECT_FALSE(result.policies[0].qualifiers[0].user_notice.has_value());

  // The failure surfaces the whole extension as a hex dump in `raw_der`.
  EXPECT_EQ(
      "30 27 30 25 06 02 2A 03 30 1F 30 1D 06 08 2B 06 01 05 05 07 02 02 30 "
      "11 0C 0F 45 78 70 6C 69 63 69 74 20 A1 20 54 65 78 74",
      result.raw_der);
}

}  // namespace x509_certificate_model
