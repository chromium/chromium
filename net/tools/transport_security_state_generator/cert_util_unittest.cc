// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <string>
#include <vector>

#include "net/tools/transport_security_state_generator/cert_util.h"
#include "net/tools/transport_security_state_generator/spki_hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/x509v3.h"

namespace net::transport_security_state {

namespace {

// Certficate with the subject CN set to "Chromium", the subject organisation
// set to "The Chromium Projects", and the subject organizational unit set to
// "Security."
static const char kSelfSignedWithCommonNamePEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDeTCCAmGgAwIBAgIJAKZbsC4gPYAUMA0GCSqGSIb3DQEBCwUAMFMxETAPBgNV\n"
    "BAMMCENocm9taXVtMR4wHAYDVQQKDBVUaGUgQ2hyb21pdW0gUHJvamVjdHMxETAP\n"
    "BgNVBAsMCFNlY3VyaXR5MQswCQYDVQQGEwJVUzAeFw0xNzAxMjkyMDU1NDFaFw0x\n"
    "ODAxMjkyMDU1NDFaMFMxETAPBgNVBAMMCENocm9taXVtMR4wHAYDVQQKDBVUaGUg\n"
    "Q2hyb21pdW0gUHJvamVjdHMxETAPBgNVBAsMCFNlY3VyaXR5MQswCQYDVQQGEwJV\n"
    "UzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMlir9M85QOvQ5ok+uvH\n"
    "XF7kmW21B22Ffdw+B2mXTV6NLGvINCdwocIlebQlAdWS2QY/WM08uAYJ3m0IGD+t\n"
    "6OG4zG3vOmWMdFQy4XkxMsDkbV11F9n4dsF5TXEvILlupOtOWu6Up8vfFkii/x+/\n"
    "bz4aGBDdFu6U8TdQ8ELSmHxJYi4LM0lUKTdLLte3T5Grv3UUXQW33Qs6RXZlH/ul\n"
    "jf7/v0HQefM3XdT9djG1XRv8Ga32c8tz+wtSw7PPIWjt0ZDJxZ2/fX7YLwAt2D6N\n"
    "zQgrNJtL0/I/j9sO6A0YQeHzmnlyoAd14VhBfEllZc51pFaut31wpbPPxtH0K0Ro\n"
    "2XUCAwEAAaNQME4wHQYDVR0OBBYEFD7eitJ8KlIaVS4J9w2Nz+5OE8H0MB8GA1Ud\n"
    "IwQYMBaAFD7eitJ8KlIaVS4J9w2Nz+5OE8H0MAwGA1UdEwQFMAMBAf8wDQYJKoZI\n"
    "hvcNAQELBQADggEBAFjuy0Jhj2E/ALOkOst53/nHIpT5suru4H6YEmmPye+KCQnC\n"
    "ws1msPyLQ8V10/kyQzJTSLbeehNyOaK99KJk+hZBVEKBa9uH3WXPpiwz1xr3STJO\n"
    "hhV2wXGTMqe5gryR7r+n88+2TpRiZ/mAVyJm4NQgev4HZbFsl3sT50AQrrEbHHiY\n"
    "Sh38NCR8JCVuzLBjcEEIWxjhDPkdNPJtx3cBkIDP+Cz1AUSPretGk7CQAGivq7Kq\n"
    "9y6A59guc1RFVPeEQAxUIUDZGDQlB3PtmrXrp1/LAaDYvQCstDBgiZoamy+xSROP\n"
    "BU2KIzRj2EUOWqtIURU4Q2QC1fbVqxVjfPowX/A=\n"
    "-----END CERTIFICATE-----\n";

// Certificate without a subject CN. The subject organisation is set to
// "The Chromium Projects" and the subject origanisational unit is set to
// "Security".
static const char kSelfSignedWithoutCommonNamePEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDUzCCAjugAwIBAgIJAI18Ifktf3YOMA0GCSqGSIb3DQEBCwUAMEAxHjAcBgNV\n"
    "BAoMFVRoZSBDaHJvbWl1bSBQcm9qZWN0czERMA8GA1UECwwIU2VjdXJpdHkxCzAJ\n"
    "BgNVBAYTAlVTMB4XDTE3MDEyOTIxMTMwMloXDTE4MDEyOTIxMTMwMlowQDEeMBwG\n"
    "A1UECgwVVGhlIENocm9taXVtIFByb2plY3RzMREwDwYDVQQLDAhTZWN1cml0eTEL\n"
    "MAkGA1UEBhMCVVMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCxfBIg\n"
    "4hVljlFbyZ88mhLEKCfy/8X127H16ywcy+q+jlj7YtlWqGKlfIjKQkXKeI/xUB1F\n"
    "ZC1S0kmVycAoahb4m+NqkfBkuxbpc5gYsv9TdgiNIhEezx6Z9OTPjGnTZVDjJNsQ\n"
    "MVKfG+DD3qAf22PhpU2zGXCF2ECL7J/Lh6Wu/W3InuIcJGm3D7F182UK86stvC/+\n"
    "mS9K7AJyX320vHWYsVB/jA9w6cSdlZf454E+wtsS0b+UIMF6fewg2Xb/FYxRsOjp\n"
    "ppVpF8/2v6JzDjBhdZkYufR5M43tCEUBBK6TwfXAPfK3v2IDcoW+iOuztW5/cdTs\n"
    "rVaGK9YqRDIeFWKNAgMBAAGjUDBOMB0GA1UdDgQWBBRh2Ef5+mRtj2sJHpXWlWai\n"
    "D3zNXTAfBgNVHSMEGDAWgBRh2Ef5+mRtj2sJHpXWlWaiD3zNXTAMBgNVHRMEBTAD\n"
    "AQH/MA0GCSqGSIb3DQEBCwUAA4IBAQAmxdLSlb76yre3VmugMQqybSkJr4+OZm6c\n"
    "ES6TQeBzNrbPQhYPAfTUa2i4Cx5r4tMTp1IfUKgtng4qnKyLRgC+BV4zAfSRxbuw\n"
    "aqicO1Whtl/Vs2Cdou10EU68kKOxLqNdzfXVVSQ/HxGFJFFJdSLfjpRTcfbORfeh\n"
    "BfFQkjdlK8DdX8pPLjHImFKXT/8IpPPq41k2KuIhG3cd2vBNV7n7U793LSE+dPQk\n"
    "0jKehPOfiPBl1nWr7ZTF8bYtgxboVsv73E6IoQhPGPnnDF3ISQ5/ulDQNXJr2PI3\n"
    "ZYZ4PtSKcBi97BucW7lkt3bWY44TZGVHY1s4EGQFqU4aDyP+aR7Z\n"
    "-----END CERTIFICATE-----\n";

// Certificate without a subject CN, organisation or organizational unit.
static const char kSelfSignedWithoutSubject[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIC7TCCAdWgAwIBAgIJAOPMcoAKhzZPMA0GCSqGSIb3DQEBCwUAMA0xCzAJBgNV\n"
    "BAYTAlVTMB4XDTE3MDEyOTIxNDA1MloXDTE4MDEyOTIxNDA1MlowDTELMAkGA1UE\n"
    "BhMCVVMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDLn0oths5iUbDN\n"
    "h5IssWAf4jBRVh0c7AfVpnsriSdpgMEfApjE4Fcb3ma/8g+f2SB0x7bSLKMfpKZl\n"
    "v7tQBuNXsbMcv1l4Ip595ZznSr74Fpuc6K0pqaVUSrgt2EVDp6lx12fFcXMI08Ar\n"
    "76v06loe7HnO+cOCAXn3Yd89UznB7w8a+RiJlUzb4vksksSQyxCOYwahx6kuN9vh\n"
    "MkjmzoVSbO6vtHktECsq5M2k98GZMmbXimW+lkyqsG3qJnmAYsIapDE1droPp5Cx\n"
    "l/tQ95CKEZQDuF4Zv+fgg0eHnnCAhuCPnM8GblOTsAsSjNd8GM+4eJPPtAHdB1nn\n"
    "HCYB/QadAgMBAAGjUDBOMB0GA1UdDgQWBBTxlQlna2f2VttJkEoeayPsCF7SxzAf\n"
    "BgNVHSMEGDAWgBTxlQlna2f2VttJkEoeayPsCF7SxzAMBgNVHRMEBTADAQH/MA0G\n"
    "CSqGSIb3DQEBCwUAA4IBAQBUOmDhs3K1v+tPeO+TWFw8NDfOkcWy6EX+c6K7mSwF\n"
    "mJjqWsEUBp+WbTK6RoVjuLucH5mRF3FmRrW/hOnxIWxpHg5/9vodReLDPnUw0Anb\n"
    "QoxKgJ41VfD8aGK8GDPOrETwbIR6+d9P6bDKukiuW41Yh5TjXLufaQ1g9C1AIEoG\n"
    "88Akr6g9Q0vJJXGl9YcPFz6M1wm3l/lH08v2Ual52elFXYcDcoxhLCOdImmWGlnn\n"
    "MYXxdl1ivj3hHgFXxkIbrlYKVSBhwPPgjVYKkimFcZF5Xw7wfmIl/WUtVaRpmkGp\n"
    "3TgH7jdRQ1WXlROBct/4Z8jzs7i+Ttk8oxct2r+PdqeZ\n"
    "-----END CERTIFICATE-----\n";

// Valid PEM certificate headers but invalid BASE64 content.
static const char kInvalidCertificatePEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "This is invalid base64.\n"
    "It contains some (#$*) invalid characters.\n"
    "-----END CERTIFICATE-----\n";

// Valid PEM public key headers but invalid BASE64 content.
static const char kInvalidPublicKeyPEM[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "This is invalid base64.\n"
    "It contains some (#$*) invalid characters.\n"
    "-----END PUBLIC KEY-----\n";

// Valid 2048 bit RSA public key.
static const char kPublicKeyPEM[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAujzwcb5bJuC/A/Y9izGl\n"
    "LlA3fnKGbeyn53BdVznJN4fQwU82WKVYdqt8d/1ZDRdYyhGrTgXJeCURe9VSJyX1\n"
    "X2a5EApSFsopP8Yjy0Rl6dNOLO84KCW9dPmfHC3uP0ac4hnHT5dUr05YvhJmHCkf\n"
    "as6v/aEgpPLDhRF6UruSUh+gIpUg/F3+vlD99HLfbloukoDtQyxW+86s9sO7RQ00\n"
    "pd79VOoa/v09FvoS7MFgnBBOtvBQLOXjEH7/qBsnrXFtHBeOtxSLar/FL3OhVXuh\n"
    "dUTRyc1Mg0ECtz8zHZugW+LleIm5Bf5Yr0bN1O/HfDPCkDaCldcm6xohEHn9pBaW\n"
    "+wIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

// Valid 2048 bit RSA public key with incorrect PEM headers.
static const char kUnknownPEMHeaders[] =
    "-----BEGIN OF SOMETHING-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAujzwcb5bJuC/A/Y9izGl\n"
    "LlA3fnKGbeyn53BdVznJN4fQwU82WKVYdqt8d/1ZDRdYyhGrTgXJeCURe9VSJyX1\n"
    "X2a5EApSFsopP8Yjy0Rl6dNOLO84KCW9dPmfHC3uP0ac4hnHT5dUr05YvhJmHCkf\n"
    "as6v/aEgpPLDhRF6UruSUh+gIpUg/F3+vlD99HLfbloukoDtQyxW+86s9sO7RQ00\n"
    "pd79VOoa/v09FvoS7MFgnBBOtvBQLOXjEH7/qBsnrXFtHBeOtxSLar/FL3OhVXuh\n"
    "dUTRyc1Mg0ECtz8zHZugW+LleIm5Bf5Yr0bN1O/HfDPCkDaCldcm6xohEHn9pBaW\n"
    "+wIDAQAB\n"
    "-----END OF SOMETHING-----\n";

TEST(CertUtilTest, GetX509CertificateFromPEM) {
  EXPECT_NE(nullptr, GetX509CertificateFromPEM(kSelfSignedWithCommonNamePEM));
  EXPECT_NE(nullptr, GetX509CertificateFromPEM(kSelfSignedWithoutSubject));
  EXPECT_EQ(nullptr, GetX509CertificateFromPEM(kInvalidCertificatePEM));
  EXPECT_EQ(nullptr, GetX509CertificateFromPEM(kInvalidPublicKeyPEM));
}

// Test that the SPKI digest is correctly calculated for valid certificates.
TEST(CertUtilTest, CalculateSPKIHashFromCertificate) {
  SPKIHash hash1;
  bssl::UniquePtr<X509> cert1 =
      GetX509CertificateFromPEM(kSelfSignedWithCommonNamePEM);
  EXPECT_TRUE(CalculateSPKIHashFromCertificate(cert1.get(), &hash1));
  std::vector<uint8_t> hash_vector(hash1.data(), hash1.data() + hash1.size());
  EXPECT_THAT(
      hash_vector,
      testing::ElementsAreArray(
          {0xAC, 0xFB, 0x2B, 0xF3, 0x6A, 0x90, 0x47, 0xF1, 0x74, 0xAE, 0xF1,
           0xCE, 0x63, 0x3D, 0xA9, 0x45, 0xCB, 0xA,  0xA7, 0x3F, 0x16, 0x2A,
           0xF3, 0x88, 0x9A, 0xE2, 0x72, 0xC,  0x07, 0x63, 0x45, 0xB0}));

  SPKIHash hash2;
  bssl::UniquePtr<X509> cert2 =
      GetX509CertificateFromPEM(kSelfSignedWithoutCommonNamePEM);
  EXPECT_TRUE(CalculateSPKIHashFromCertificate(cert2.get(), &hash2));
  std::vector<uint8_t> hash_vector2(hash2.data(), hash2.data() + hash2.size());
  EXPECT_THAT(
      hash_vector2,
      testing::ElementsAreArray(
          {0x40, 0xBC, 0xD6, 0xE4, 0x10, 0x70, 0x37, 0x3C, 0xF7, 0x21, 0x51,
           0xD7, 0x27, 0x64, 0xFD, 0xF1, 0xA,  0x89, 0x0,  0xAD, 0x75, 0xDF,
           0xB3, 0xEA, 0x21, 0xFC, 0x6E, 0x67, 0xD5, 0xAE, 0xA4, 0x94}));
}

// Test that the SPKI digest for public key's are calculated correctly.
TEST(CertUtilTest, CalculateSPKIHashFromKey) {
  SPKIHash hash1;
  EXPECT_TRUE(CalculateSPKIHashFromKey(kPublicKeyPEM, &hash1));
  std::vector<uint8_t> hash_vector(hash1.data(), hash1.data() + hash1.size());
  EXPECT_THAT(
      hash_vector,
      testing::ElementsAreArray(
          {0x63, 0xB0, 0x21, 0x4,  0x3,  0x13, 0x9E, 0x36, 0xEE, 0xCB, 0x6F,
           0xA5, 0x7A, 0x94, 0x56, 0x18, 0xBA, 0x41, 0x13, 0x8C, 0x4A, 0x48,
           0x99, 0x80, 0x51, 0x66, 0xF8, 0x85, 0x2,  0xFC, 0x48, 0x9E}));
  SPKIHash hash2;
  EXPECT_FALSE(CalculateSPKIHashFromKey(kInvalidPublicKeyPEM, &hash2));

  SPKIHash hash3;
  EXPECT_FALSE(
      CalculateSPKIHashFromKey(kSelfSignedWithoutCommonNamePEM, &hash3));

  SPKIHash hash4;
  EXPECT_FALSE(CalculateSPKIHashFromKey(kUnknownPEMHeaders, &hash4));
}

// Test that the subject name is extracted correctly. This should default to the
// subject common name and fall back to the organisation + organizational unit.
TEST(CertUtilTest, ExtractSubjectNameFromCertificate) {
  std::string name1;
  bssl::UniquePtr<X509> cert1 =
      GetX509CertificateFromPEM(kSelfSignedWithCommonNamePEM);
  EXPECT_TRUE(ExtractSubjectNameFromCertificate(cert1.get(), &name1));

  // For certficates with the subject common name field set, we should get the
  // value of the subject common name.
  EXPECT_EQ("Chromium", name1);

  std::string name2;
  bssl::UniquePtr<X509> cert2 =
      GetX509CertificateFromPEM(kSelfSignedWithoutCommonNamePEM);
  EXPECT_TRUE(ExtractSubjectNameFromCertificate(cert2.get(), &name2));

  // For certificates without a subject common name field, we should get
  // the subject organization + " " + organizational unit instead.
  EXPECT_EQ("The Chromium Projects Security", name2);

  std::string name3;
  bssl::UniquePtr<X509> cert3 =
      GetX509CertificateFromPEM(kSelfSignedWithoutSubject);
  EXPECT_FALSE(ExtractSubjectNameFromCertificate(cert3.get(), &name3));
}

}  // namespace

}  // namespace net::transport_security_state
