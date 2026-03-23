// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/certificate/model/x509_certificate_model_ios.h"

#import <string_view>

#import "base/time/time.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace x509_certificate_model {

namespace {

// Helper function to check if OptionalStringOrError holds a string with
// expected value.
void ExpectString(const OptionalStringOrError& result,
                  const std::string& expected) {
  ASSERT_TRUE(std::holds_alternative<std::string>(result))
      << "Expected string but got different variant type";
  EXPECT_EQ(expected, std::get<std::string>(result));
}

// Helper function to check if OptionalStringOrError holds NotPresent.
void ExpectNotPresent(const OptionalStringOrError& result) {
  EXPECT_TRUE(std::holds_alternative<NotPresent>(result))
      << "Expected NotPresent but got different variant type";
}

}  // namespace

class X509CertificateModelIOSTest : public PlatformTest {};

TEST_F(X509CertificateModelIOSTest, InvalidCert) {
  X509CertificateModel model(net::x509_util::CreateCryptoBuffer(
      base::span<const uint8_t>({'b', 'a', 'd', '\n'})));

  EXPECT_EQ("1D 7A 36 3C E1 24 30 88 1E C5 6C 9C F1 40 9C 49 C4 91 04 36 18 E5 "
            "98 C3 56 E2 95 90 40 87 2F 5A",
            model.HashCertSHA256());
  EXPECT_FALSE(model.is_valid());
}

TEST_F(X509CertificateModelIOSTest, GetGoogleCertFields) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());

  EXPECT_EQ("F6 41 C3 6C FE F4 9B C0 71 35 9E CF 88 EE D9 31 7B 73 8B 59 89 41 "
            "6A D4 01 72 0C 0A 4E 2E 63 52",
            model.HashCertSHA256());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ("23 A5 5C E6 8E A1 B2 06 23 DE 09 E9 3F DF 3B B0 32 87 AC 73 7B 27 "
            "33 5B 43 07 FE 9E C4 85 5C 34",
            model.HashSpkiSHA256());

  EXPECT_EQ(3, model.GetVersion());
  EXPECT_EQ("2F DF BC F6 AE 91 52 6D 0F 9A A3 DF 40 34 3E 9A",
            model.GetSerialNumberHexified());

  ExpectString(model.GetIssuerCommonName(), "Thawte SGC CA");
  ExpectString(model.GetIssuerOrgName(), "Thawte Consulting (Pty) Ltd.");
  ExpectNotPresent(model.GetIssuerOrgUnitName());
  ExpectString(model.GetIssuerCountryName(), "ZA");

  ExpectString(model.GetSubjectCommonName(), "www.google.com");
  ExpectString(model.GetSubjectOrgName(), "Google Inc");
  ExpectNotPresent(model.GetSubjectOrgUnitName());

  // Test Subject locality and state.
  ExpectString(model.GetSubjectLocalityName(), "Mountain View");
  ExpectString(model.GetSubjectStateName(), "California");
  ExpectString(model.GetSubjectCountryName(), "US");

  // The following fields are not present in the test certificate.
  ExpectNotPresent(model.GetSubjectBusinessCategory());
  ExpectNotPresent(model.GetSubjectJurisdictionCountryName());
  ExpectNotPresent(model.GetSubjectJurisdictionStateName());
  ExpectNotPresent(model.GetSubjectSerialNumber());

  base::Time not_before, not_after;
  EXPECT_TRUE(model.GetTimes(&not_before, &not_after));
  // Constants copied from x509_certificate_unittest.cc.
  // Dec 18 00:00:00 2009 GMT
  const double kGoogleParseValidFrom = 1261094400;
  EXPECT_EQ(kGoogleParseValidFrom, not_before.InSecondsFSinceUnixEpoch());
  // Dec 18 23:59:59 2011 GMT
  const double kGoogleParseValidTo = 1324252799;
  EXPECT_EQ(kGoogleParseValidTo, not_after.InSecondsFSinceUnixEpoch());
}

TEST_F(X509CertificateModelIOSTest, GetNDNCertFields) {
  auto cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ndn.ca.crt");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ(1, model.GetVersion());
  // The model just returns the hex of the DER bytes, so the leading zeros are
  // included.
  EXPECT_EQ("00 DB B7 C6 06 47 AF 37 A2", model.GetSerialNumberHexified());

  ExpectString(model.GetIssuerCommonName(),
               "New Dream Network Certificate Authority");
  ExpectString(model.GetIssuerOrgName(), "New Dream Network, LLC");
  ExpectString(model.GetIssuerOrgUnitName(), "Security");
  ExpectString(model.GetSubjectCommonName(),
               "New Dream Network Certificate Authority");
  ExpectString(model.GetSubjectOrgName(), "New Dream Network, LLC");
  ExpectString(model.GetSubjectOrgUnitName(), "Security");

  base::Time not_before, not_after;
  EXPECT_TRUE(model.GetTimes(&not_before, &not_after));
  EXPECT_EQ(12800754778, not_before.ToDeltaSinceWindowsEpoch().InSeconds());
  EXPECT_EQ(13116114778, not_after.ToDeltaSinceWindowsEpoch().InSeconds());
}

TEST_F(X509CertificateModelIOSTest, PunyCodeCert) {
  auto cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "punycodetest.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  ExpectString(model.GetIssuerCommonName(), "xn--wgv71a119e.com");
  ExpectString(model.GetSubjectCommonName(), "xn--wgv71a119e.com");
}

TEST_F(X509CertificateModelIOSTest, BasicConstraints) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasBasicConstraints());
  EXPECT_TRUE(model.IsBasicConstraintsCritical());
  EXPECT_FALSE(model.IsBasicConstraintsCa());
  EXPECT_FALSE(model.GetBasicConstraintsPathLen().has_value());
}

TEST_F(X509CertificateModelIOSTest, KeyUsage) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "2029_globalsign_com_cert.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasKeyUsage());
  EXPECT_TRUE(model.IsKeyUsageCritical());
  std::string key_usage = model.GetKeyUsageString();
  EXPECT_FALSE(key_usage.empty());
}

TEST_F(X509CertificateModelIOSTest, ExtendedKeyUsage) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasExtendedKeyUsage());
  auto eku_purposes = model.GetExtendedKeyUsagePurposes();
  EXPECT_FALSE(eku_purposes.empty());
}

TEST_F(X509CertificateModelIOSTest, SubjectKeyIdentifier) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "2029_globalsign_com_cert.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasSubjectKeyIdentifier());
  EXPECT_FALSE(model.IsSubjectKeyIdentifierCritical());
  EXPECT_EQ("59 BC D9 69 F7 B0 65 BB C8 34 C5 D2 C2 EF 17 78 A6 47 1E 8B",
            model.GetSubjectKeyIdentifier());
}

TEST_F(X509CertificateModelIOSTest, AuthorityKeyIdentifier) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "2029_globalsign_com_cert.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasAuthorityKeyIdentifier());
  EXPECT_FALSE(model.IsAuthorityKeyIdentifierCritical());
  EXPECT_EQ("8A FC 14 1B 3D A3 59 67 A5 3B E1 73 92 A6 62 91 7F E4 78 30",
            model.GetAuthorityKeyIdentifier());
}

TEST_F(X509CertificateModelIOSTest, AuthorityInformationAccess) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "2029_globalsign_com_cert.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasAuthorityInformationAccess());
  EXPECT_FALSE(model.IsAuthorityInformationAccessCritical());
  auto access_descriptions = model.GetAuthorityInformationAccess();
  ASSERT_EQ(1u, access_descriptions.size());
  EXPECT_FALSE(access_descriptions[0].method.empty());
  EXPECT_EQ("http://secure.globalsign.net/cacert/SHA256extendval1.crt",
            access_descriptions[0].location);
}

TEST_F(X509CertificateModelIOSTest, CRLDistributionPoints) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasCRLDistributionPoints());
  EXPECT_FALSE(model.IsCRLDistributionPointsCritical());
  auto crl_dps = model.GetCRLDistributionPoints();
  ASSERT_EQ(1u, crl_dps.size());
  EXPECT_EQ("http://crl.thawte.com/ThawteSGCCA.crl", crl_dps[0]);
}

TEST_F(X509CertificateModelIOSTest, CertificatePolicies) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "2029_globalsign_com_cert.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasCertificatePolicies());
  EXPECT_FALSE(model.IsCertificatePoliciesCritical());
  auto policies = model.GetCertificatePolicies();
  ASSERT_EQ(1u, policies.size());
  EXPECT_EQ("1.3.6.1.4.1.4146.1.1", policies[0].identifier);
  ASSERT_EQ(1u, policies[0].qualifiers.size());
  EXPECT_FALSE(policies[0].qualifiers[0].value.empty());
}

TEST_F(X509CertificateModelIOSTest, SubjectAlternativeNames) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "subjectAltName_sanity_check.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  EXPECT_TRUE(model.HasSubjectAlternativeNames());
  EXPECT_FALSE(model.IsSubjectAlternativeNamesCritical());
  auto san_dns = model.GetSubjectAlternativeNamesDNS();
  ASSERT_EQ(1u, san_dns.size());
  EXPECT_EQ("test.example", san_dns[0]);
}

TEST_F(X509CertificateModelIOSTest, SCTList) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "lets-encrypt-dst-x3-root.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  if (model.HasSCTList()) {
    std::string sct_data = model.GetSCTListData();
    EXPECT_EQ("04 81 F1 00 EF 00 76 00 41 C8 CA B1 DF 22 46 4A 10 C6 A1 3A 09 "
              "42 87 5E 4E 31 8B 1B 03 EB EB 4B C7 68 F0 90 62 96 06 F6 00 00 "
              "01 7E 17 63 85 3D 00 00 04 03 00 47 30 45 02 20 05 FB 47 45 BD "
              "63 AD FD E7 AF 9E 7E D6 51 5A 1E AB 62 FE 2A 27 4B A0 ED 8A 4A "
              "8F B3 C8 36 8C BD 02 21 00 8B 07 10 4C BF 07 1C ED 54 DF 28 2C "
              "E3 B2 32 6B 43 48 E4 04 80 28 17 91 50 8D 28 FC 58 08 BF 7C 00 "
              "75 00 46 A5 55 EB 75 FA 91 20 30 B5 A2 89 69 F4 F3 7D 11 2C 41 "
              "74 BE FD 49 B8 85 AB F2 FC 70 FE 6D 47 00 00 01 7E 17 63 85 53 "
              "00 00 04 03 00 46 30 44 02 20 73 8C D6 ED CC 59 2D 3D 5E 1A 37 "
              "E9 42 A2 74 6D 95 1B 20 0E 19 91 40 0E AD A3 80 66 48 FB 17 32 "
              "02 20 02 3A 61 DA 61 EF CB 37 BB 97 5E AC 79 08 2B 5E 71 EA 9B "
              "7B FC B4 F5 50 04 2E E0 40 42 44 2C 79",
              sct_data);
  }
}

TEST_F(X509CertificateModelIOSTest, PublicKeyInfo) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  // Check public key algorithm.
  std::string algo = model.GetPublicKeyAlgorithm();
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_CERT_OID_PKCS1_RSA_ENCRYPTION), algo);

  // Check public key data.
  std::string data = model.GetPublicKeyData();
  EXPECT_EQ("30 81 89 02 81 81 00 E8 F9 86 0F 90 FA 86 D7 DF BD 72 26 B6 D7 44 "
            "02 83 78 73 D9 02 28 EF 88 45 39 FB 10 E8 7C AE A9 38 D5 75 C6 38 "
            "EB 0A 15 07 9B 83 E8 CD 82 D5 E3 F7 15 68 45 A1 0B 19 85 BC E2 EF "
            "84 E7 DD F2 D7 B8 98 C2 A1 BB B5 C1 51 DF D4 83 02 A7 3D 06 42 5B "
            "E1 22 C3 DE 6B 85 5F 1C D6 DA 4E 8B D3 9B EE B9 67 22 2A 1D 11 EF "
            "79 A4 B3 37 8A F4 FE 18 FD BC F9 46 23 50 97 F3 AC FC 24 46 2B 5C "
            "3B B7 45 02 03 01 00 01",
            data);

  // Check public key size.
  auto pub_key_size = model.GetPublicKeySize();
  EXPECT_TRUE(pub_key_size.has_value());
  EXPECT_EQ(1024u, pub_key_size.value());
}

TEST_F(X509CertificateModelIOSTest, SignatureInfo) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  // Check signature algorithm.
  std::string sig_algo = model.GetSignatureAlgorithm();
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_CERT_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION),
      sig_algo);

  // Check signature data.
  std::string sig_data = model.GetSignatureData();
  EXPECT_EQ("9F 43 CF 5B C4 50 29 B1 BF E2 B0 9A FF 6A 21 1D 2D 12 C3 2C 4E 5A "
            "F9 12 E2 CE B9 82 52 2D E7 1D 7E 1A 76 96 90 79 D1 24 52 38 79 BB "
            "63 8D 80 97 7C 23 20 0F 91 4D 16 B9 EA EE F4 6D 89 CA C6 BD CC 24 "
            "68 D6 43 5B CE 2A 58 BF 3C 18 E0 E0 3C 62 CF 96 02 2D 28 47 50 34 "
            "E1 27 BA CF 99 D1 50 FF 29 25 C0 36 36 15 33 52 70 BE 31 8F 9F E8 "
            "7F E7 11 0C 8D BF 84 A0 42 1A 80 89 B0 31 58 41 07 5F",
            sig_data);

  // Check signature parameters - RSA with SHA-1 has NULL parameters.
  std::string sig_params = model.GetSignatureParameters();
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_IOS_CERT_DETAILS_PARAMETERS_NONE),
            sig_params);
}

TEST_F(X509CertificateModelIOSTest, UnknownExtensions) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  X509CertificateModel model(cert.get());
  ASSERT_TRUE(model.is_valid());

  // Get unknown extensions (may be empty depending on the cert).
  auto unknown_exts = model.GetUnknownExtensions();
  // Just verify we can call this without crashing.
  for (const auto& ext : unknown_exts) {
    EXPECT_FALSE(ext.oid.empty());
  }
}

}  // namespace x509_certificate_model
