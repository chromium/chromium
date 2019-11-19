// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_certificate.h"

#include "base/strings/stringprintf.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/test_helpers.h"
#include "net/der/input.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

// Pretty-prints a GeneralizedTime as a human-readable string for use in test
// expectations (it is more readable to specify the expected results as a
// string).
std::string ToString(const der::GeneralizedTime& time) {
  return base::StringPrintf(
      "year=%d, month=%d, day=%d, hours=%d, minutes=%d, seconds=%d", time.year,
      time.month, time.day, time.hours, time.minutes, time.seconds);
}

std::string GetFilePath(const std::string& file_name) {
  return std::string("net/data/parse_certificate_unittest/") + file_name;
}

// Loads certificate data and expectations from the PEM file |file_name|.
// Verifies that parsing the Certificate matches expectations:
//   * If expected to fail, emits the expected errors
//   * If expected to succeeds, the parsed fields match expectations
void RunCertificateTest(const std::string& file_name) {
  std::string data;
  std::string expected_errors;
  std::string expected_tbs_certificate;
  std::string expected_signature_algorithm;
  std::string expected_signature;

  // Read the certificate data and test expectations from a single PEM file.
  const PemBlockMapping mappings[] = {
      {"CERTIFICATE", &data},
      {"ERRORS", &expected_errors, true /*optional*/},
      {"SIGNATURE", &expected_signature, true /*optional*/},
      {"SIGNATURE ALGORITHM", &expected_signature_algorithm, true /*optional*/},
      {"TBS CERTIFICATE", &expected_tbs_certificate, true /*optional*/},
  };
  std::string test_file_path = GetFilePath(file_name);
  ASSERT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

  // Note that empty expected_errors doesn't necessarily mean success.
  bool expected_result = !expected_tbs_certificate.empty();

  // Parsing the certificate.
  der::Input tbs_certificate_tlv;
  der::Input signature_algorithm_tlv;
  der::BitString signature_value;
  CertErrors errors;
  bool actual_result =
      ParseCertificate(der::Input(&data), &tbs_certificate_tlv,
                       &signature_algorithm_tlv, &signature_value, &errors);

  EXPECT_EQ(expected_result, actual_result);
  VerifyCertErrors(expected_errors, errors, test_file_path);

  // Ensure that the parsed certificate matches expectations.
  if (expected_result && actual_result) {
    EXPECT_EQ(0, signature_value.unused_bits());
    EXPECT_EQ(der::Input(&expected_signature), signature_value.bytes());
    EXPECT_EQ(der::Input(&expected_signature_algorithm),
              signature_algorithm_tlv);
    EXPECT_EQ(der::Input(&expected_tbs_certificate), tbs_certificate_tlv);
  }
}

// Tests parsing a Certificate.
TEST(ParseCertificateTest, Version3) {
  RunCertificateTest("cert_version3.pem");
}

// Tests parsing a simplified Certificate-like structure (the sub-fields for
// algorithm and tbsCertificate are not actually valid, but ParseCertificate()
// doesn't check them)
TEST(ParseCertificateTest, Skeleton) {
  RunCertificateTest("cert_skeleton.pem");
}

// Tests parsing a Certificate that is not a sequence fails.
TEST(ParseCertificateTest, NotSequence) {
  RunCertificateTest("cert_not_sequence.pem");
}

// Tests that uncomsumed data is not allowed after the main SEQUENCE.
TEST(ParseCertificateTest, DataAfterSignature) {
  RunCertificateTest("cert_data_after_signature.pem");
}

// Tests that parsing fails if the signature BIT STRING is missing.
TEST(ParseCertificateTest, MissingSignature) {
  RunCertificateTest("cert_missing_signature.pem");
}

// Tests that parsing fails if the signature is present but not a BIT STRING.
TEST(ParseCertificateTest, SignatureNotBitString) {
  RunCertificateTest("cert_signature_not_bit_string.pem");
}

// Tests that parsing fails if the main SEQUENCE is empty (missing all the
// fields).
TEST(ParseCertificateTest, EmptySequence) {
  RunCertificateTest("cert_empty_sequence.pem");
}

// Tests what happens when the signature algorithm is present, but has the wrong
// tag.
TEST(ParseCertificateTest, AlgorithmNotSequence) {
  RunCertificateTest("cert_algorithm_not_sequence.pem");
}

// Loads tbsCertificate data and expectations from the PEM file |file_name|.
// Verifies that parsing the TBSCertificate succeeds, and each parsed field
// matches the expectations.
//
// TODO(eroman): Get rid of the |expected_version| parameter -- this should be
// encoded in the test expectations file.
void RunTbsCertificateTestGivenVersion(const std::string& file_name,
                                       CertificateVersion expected_version) {
  std::string data;
  std::string expected_serial_number;
  std::string expected_signature_algorithm;
  std::string expected_issuer;
  std::string expected_validity_not_before;
  std::string expected_validity_not_after;
  std::string expected_subject;
  std::string expected_spki;
  std::string expected_issuer_unique_id;
  std::string expected_subject_unique_id;
  std::string expected_extensions;
  std::string expected_errors;

  // Read the certificate data and test expectations from a single PEM file.
  const PemBlockMapping mappings[] = {
      {"TBS CERTIFICATE", &data},
      {"SIGNATURE ALGORITHM", &expected_signature_algorithm, true},
      {"SERIAL NUMBER", &expected_serial_number, true},
      {"ISSUER", &expected_issuer, true},
      {"VALIDITY NOTBEFORE", &expected_validity_not_before, true},
      {"VALIDITY NOTAFTER", &expected_validity_not_after, true},
      {"SUBJECT", &expected_subject, true},
      {"SPKI", &expected_spki, true},
      {"ISSUER UNIQUE ID", &expected_issuer_unique_id, true},
      {"SUBJECT UNIQUE ID", &expected_subject_unique_id, true},
      {"EXTENSIONS", &expected_extensions, true},
      {"ERRORS", &expected_errors, true},
  };
  std::string test_file_path = GetFilePath(file_name);
  ASSERT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

  bool expected_result = !expected_spki.empty();

  ParsedTbsCertificate parsed;
  CertErrors errors;
  bool actual_result =
      ParseTbsCertificate(der::Input(&data), {}, &parsed, &errors);

  EXPECT_EQ(expected_result, actual_result);
  VerifyCertErrors(expected_errors, errors, test_file_path);

  if (!expected_result || !actual_result)
    return;

  // Ensure that the ParsedTbsCertificate matches expectations.
  EXPECT_EQ(expected_version, parsed.version);

  EXPECT_EQ(der::Input(&expected_serial_number), parsed.serial_number);
  EXPECT_EQ(der::Input(&expected_signature_algorithm),
            parsed.signature_algorithm_tlv);

  EXPECT_EQ(der::Input(&expected_issuer), parsed.issuer_tlv);

  // In the test expectations PEM file, validity is described as a
  // textual string of the parsed value (rather than as DER).
  EXPECT_EQ(expected_validity_not_before, ToString(parsed.validity_not_before));
  EXPECT_EQ(expected_validity_not_after, ToString(parsed.validity_not_after));

  EXPECT_EQ(der::Input(&expected_subject), parsed.subject_tlv);
  EXPECT_EQ(der::Input(&expected_spki), parsed.spki_tlv);

  EXPECT_EQ(der::Input(&expected_issuer_unique_id),
            parsed.issuer_unique_id.bytes());
  EXPECT_EQ(!expected_issuer_unique_id.empty(), parsed.has_issuer_unique_id);
  EXPECT_EQ(der::Input(&expected_subject_unique_id),
            parsed.subject_unique_id.bytes());
  EXPECT_EQ(!expected_subject_unique_id.empty(), parsed.has_subject_unique_id);

  EXPECT_EQ(der::Input(&expected_extensions), parsed.extensions_tlv);
  EXPECT_EQ(!expected_extensions.empty(), parsed.has_extensions);
}

void RunTbsCertificateTest(const std::string& file_name) {
  RunTbsCertificateTestGivenVersion(file_name, CertificateVersion::V3);
}

// Tests parsing a TBSCertificate for v3 that contains no optional fields.
TEST(ParseTbsCertificateTest, Version3NoOptionals) {
  RunTbsCertificateTest("tbs_v3_no_optionals.pem");
}

// Tests parsing a TBSCertificate for v3 that contains extensions.
TEST(ParseTbsCertificateTest, Version3WithExtensions) {
  RunTbsCertificateTest("tbs_v3_extensions.pem");
}

// Tests parsing a TBSCertificate which lacks a version number (causing it to
// default to v1).
TEST(ParseTbsCertificateTest, Version1) {
  RunTbsCertificateTestGivenVersion("tbs_v1.pem", CertificateVersion::V1);
}

// The version was set to v1 explicitly rather than omitting the version field.
TEST(ParseTbsCertificateTest, ExplicitVersion1) {
  RunTbsCertificateTest("tbs_explicit_v1.pem");
}

// Extensions are not defined in version 1.
TEST(ParseTbsCertificateTest, Version1WithExtensions) {
  RunTbsCertificateTest("tbs_v1_extensions.pem");
}

// Extensions are not defined in version 2.
TEST(ParseTbsCertificateTest, Version2WithExtensions) {
  RunTbsCertificateTest("tbs_v2_extensions.pem");
}

// A boring version 2 certificate with none of the optional fields.
TEST(ParseTbsCertificateTest, Version2NoOptionals) {
  RunTbsCertificateTestGivenVersion("tbs_v2_no_optionals.pem",
                                    CertificateVersion::V2);
}

// A version 2 certificate with an issuer unique ID field.
TEST(ParseTbsCertificateTest, Version2IssuerUniqueId) {
  RunTbsCertificateTestGivenVersion("tbs_v2_issuer_unique_id.pem",
                                    CertificateVersion::V2);
}

// A version 2 certificate with both a issuer and subject unique ID field.
TEST(ParseTbsCertificateTest, Version2IssuerAndSubjectUniqueId) {
  RunTbsCertificateTestGivenVersion("tbs_v2_issuer_and_subject_unique_id.pem",
                                    CertificateVersion::V2);
}

// A version 3 certificate with all of the optional fields (issuer unique id,
// subject unique id, and extensions).
TEST(ParseTbsCertificateTest, Version3AllOptionals) {
  RunTbsCertificateTest("tbs_v3_all_optionals.pem");
}

// The version was set to v4, which is unrecognized.
TEST(ParseTbsCertificateTest, Version4) {
  RunTbsCertificateTest("tbs_v4.pem");
}

// Tests that extraneous data after extensions in a v3 is rejected.
TEST(ParseTbsCertificateTest, Version3DataAfterExtensions) {
  RunTbsCertificateTest("tbs_v3_data_after_extensions.pem");
}

// Tests using a real-world certificate (whereas the other tests are fabricated
// (and in fact invalid) data.
TEST(ParseTbsCertificateTest, Version3Real) {
  RunTbsCertificateTest("tbs_v3_real.pem");
}

// Parses a TBSCertificate whose "validity" field expresses both notBefore
// and notAfter using UTCTime.
TEST(ParseTbsCertificateTest, ValidityBothUtcTime) {
  RunTbsCertificateTest("tbs_validity_both_utc_time.pem");
}

// Parses a TBSCertificate whose "validity" field expresses both notBefore
// and notAfter using GeneralizedTime.
TEST(ParseTbsCertificateTest, ValidityBothGeneralizedTime) {
  RunTbsCertificateTest("tbs_validity_both_generalized_time.pem");
}

// Parses a TBSCertificate whose "validity" field expresses notBefore using
// UTCTime and notAfter using GeneralizedTime.
TEST(ParseTbsCertificateTest, ValidityUTCTimeAndGeneralizedTime) {
  RunTbsCertificateTest("tbs_validity_utc_time_and_generalized_time.pem");
}

// Parses a TBSCertificate whose validity" field expresses notBefore using
// GeneralizedTime and notAfter using UTCTime. Also of interest, notBefore >
// notAfter. Parsing will succeed, however no time can satisfy this constraint.
TEST(ParseTbsCertificateTest, ValidityGeneralizedTimeAndUTCTime) {
  RunTbsCertificateTest("tbs_validity_generalized_time_and_utc_time.pem");
}

// Parses a TBSCertificate whose "validity" field does not strictly follow
// the DER rules (and fails to be parsed).
TEST(ParseTbsCertificateTest, ValidityRelaxed) {
  RunTbsCertificateTest("tbs_validity_relaxed.pem");
}

// Parses a KeyUsage with a single 0 bit.
TEST(ParseKeyUsageTest, OneBitAllZeros) {
  const uint8_t der[] = {
      0x03, 0x02,  // BIT STRING
      0x07,        // Number of unused bits
      0x00,        // bits
  };

  der::BitString key_usage;
  ASSERT_FALSE(ParseKeyUsage(der::Input(der), &key_usage));
}

// Parses a KeyUsage with 32 bits that are all 0.
TEST(ParseKeyUsageTest, 32BitsAllZeros) {
  const uint8_t der[] = {
      0x03, 0x05,  // BIT STRING
      0x00,        // Number of unused bits
      0x00, 0x00, 0x00, 0x00,
  };

  der::BitString key_usage;
  ASSERT_FALSE(ParseKeyUsage(der::Input(der), &key_usage));
}

// Parses a KeyUsage with 32 bits, one of which is 1 (but not in recognized
// set).
TEST(ParseKeyUsageTest, 32BitsOneSet) {
  const uint8_t der[] = {
      0x03, 0x05,  // BIT STRING
      0x00,        // Number of unused bits
      0x00, 0x00, 0x00, 0x02,
  };

  der::BitString key_usage;
  ASSERT_TRUE(ParseKeyUsage(der::Input(der), &key_usage));

  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_NON_REPUDIATION));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DATA_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_AGREEMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_CRL_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_ENCIPHER_ONLY));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DECIPHER_ONLY));
}

// Parses a KeyUsage containing bit string 101.
TEST(ParseKeyUsageTest, ThreeBits) {
  const uint8_t der[] = {
      0x03, 0x02,  // BIT STRING
      0x05,        // Number of unused bits
      0xA0,        // bits
  };

  der::BitString key_usage;
  ASSERT_TRUE(ParseKeyUsage(der::Input(der), &key_usage));

  EXPECT_TRUE(key_usage.AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_NON_REPUDIATION));
  EXPECT_TRUE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DATA_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_AGREEMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_CRL_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_ENCIPHER_ONLY));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DECIPHER_ONLY));
}

// Parses a KeyUsage containing DECIPHER_ONLY, which is the
// only bit that doesn't fit in the first byte.
TEST(ParseKeyUsageTest, DecipherOnly) {
  const uint8_t der[] = {
      0x03, 0x03,  // BIT STRING
      0x07,        // Number of unused bits
      0x00, 0x80,  // bits
  };

  der::BitString key_usage;
  ASSERT_TRUE(ParseKeyUsage(der::Input(der), &key_usage));

  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_NON_REPUDIATION));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DATA_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_AGREEMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_CRL_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_ENCIPHER_ONLY));
  EXPECT_TRUE(key_usage.AssertsBit(KEY_USAGE_BIT_DECIPHER_ONLY));
}

// Parses an empty KeyUsage.
TEST(ParseKeyUsageTest, Empty) {
  const uint8_t der[] = {
      0x03, 0x01,  // BIT STRING
      0x00,        // Number of unused bits
  };

  der::BitString key_usage;
  ASSERT_FALSE(ParseKeyUsage(der::Input(der), &key_usage));
}

// Test fixture for testing ParseCrlDistributionPoints.
//
// Test data is encoded in certificate files. This fixture is responsible for
// reading and parsing the certificates to get at the extension under test.
class ParseCrlDistributionPointsTest : public ::testing::Test {
 public:
 protected:
  bool GetCrlDps(const char* file_name,
                 std::vector<ParsedDistributionPoint>* dps) {
    std::string cert_bytes;
    // Read the test certificate file.
    const PemBlockMapping mappings[] = {
        {"CERTIFICATE", &cert_bytes},
    };
    std::string test_file_path = GetFilePath(file_name);
    EXPECT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

    // Extract the CRLDP from the test Certificate.
    CertErrors errors;
    scoped_refptr<ParsedCertificate> cert = ParsedCertificate::Create(
        bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
            reinterpret_cast<const uint8_t*>(cert_bytes.data()),
            cert_bytes.size(), nullptr)),
        {}, &errors);

    if (!cert)
      return false;

    auto it = cert->extensions().find(CrlDistributionPointsOid());
    if (it == cert->extensions().end())
      return false;

    der::Input crl_dp_tlv = it->second.value;

    // Keep the certificate data alive, since this function will return
    // der::Inputs that reference it. Run the function under test (for parsing
    //
    // TODO(eroman): The use of ParsedCertificate in this test should be removed
    // in lieu of lazy parsing.
    keep_alive_certs_.push_back(cert);

    return ParseCrlDistributionPoints(crl_dp_tlv, dps);
  }

 private:
  ParsedCertificateList keep_alive_certs_;
};

TEST_F(ParseCrlDistributionPointsTest, OneUriNoIssuer) {
  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(GetCrlDps("crldp_1uri_noissuer.pem", &dps));

  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint& dp1 = dps.front();
  EXPECT_FALSE(dp1.has_crl_issuer);
  ASSERT_EQ(1u, dp1.uris.size());
  EXPECT_EQ(dp1.uris.front(), std::string("http://www.example.com/foo.crl"));
}

TEST_F(ParseCrlDistributionPointsTest, ThreeUrisNoIssuer) {
  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(GetCrlDps("crldp_3uri_noissuer.pem", &dps));

  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint& dp1 = dps.front();
  EXPECT_FALSE(dp1.has_crl_issuer);
  ASSERT_EQ(3u, dp1.uris.size());
  EXPECT_EQ(dp1.uris[0], std::string("http://www.example.com/foo1.crl"));
  EXPECT_EQ(dp1.uris[1], std::string("http://www.example.com/blah.crl"));
  EXPECT_EQ(dp1.uris[2], std::string("not-even-a-url"));
}

TEST_F(ParseCrlDistributionPointsTest, CrlIssuerAsDirname) {
  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(GetCrlDps("crldp_issuer_as_dirname.pem", &dps));

  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint& dp1 = dps.front();
  EXPECT_TRUE(dp1.has_crl_issuer);
  // TODO(eroman): This has directory names under the fullName which are not
  // being parsed or reflected here.
  ASSERT_EQ(0u, dp1.uris.size());
}

TEST_F(ParseCrlDistributionPointsTest, FullnameAsDirname) {
  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(GetCrlDps("crldp_full_name_as_dirname.pem", &dps));

  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint& dp1 = dps.front();
  EXPECT_FALSE(dp1.has_crl_issuer);
  // TODO(eroman): This has 1 directory name under the fullName which is not
  // being reflected here.
  ASSERT_EQ(0u, dp1.uris.size());
}

bool ParseAuthorityKeyIdentifierTestData(
    const char* file_name,
    std::string* backing_bytes,
    ParsedAuthorityKeyIdentifier* authority_key_identifier) {
  // Read the test file.
  const PemBlockMapping mappings[] = {
      {"AUTHORITY_KEY_IDENTIFIER", backing_bytes},
  };
  std::string test_file_path =
      std::string(
          "net/data/parse_certificate_unittest/authority_key_identifier/") +
      file_name;
  EXPECT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

  return ParseAuthorityKeyIdentifier(der::Input(backing_bytes),
                                     authority_key_identifier);
}

TEST(ParseAuthorityKeyIdentifierTest, EmptyInput) {
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(
      ParseAuthorityKeyIdentifier(der::Input(), &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, EmptySequence) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  // TODO(mattm): should this be an error? RFC 5280 doesn't explicitly say it.
  ASSERT_TRUE(ParseAuthorityKeyIdentifierTestData(
      "empty_sequence.pem", &backing_bytes, &authority_key_identifier));

  EXPECT_FALSE(authority_key_identifier.key_identifier);
  EXPECT_FALSE(authority_key_identifier.authority_cert_issuer);
  EXPECT_FALSE(authority_key_identifier.authority_cert_serial_number);
}

TEST(ParseAuthorityKeyIdentifierTest, KeyIdentifier) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  ASSERT_TRUE(ParseAuthorityKeyIdentifierTestData(
      "key_identifier.pem", &backing_bytes, &authority_key_identifier));

  ASSERT_TRUE(authority_key_identifier.key_identifier);
  const uint8_t kExpectedValue[] = {0xDE, 0xAD, 0xB0, 0x0F};
  EXPECT_EQ(der::Input(kExpectedValue),
            authority_key_identifier.key_identifier);
}

TEST(ParseAuthorityKeyIdentifierTest, IssuerAndSerial) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  ASSERT_TRUE(ParseAuthorityKeyIdentifierTestData(
      "issuer_and_serial.pem", &backing_bytes, &authority_key_identifier));

  EXPECT_FALSE(authority_key_identifier.key_identifier);

  ASSERT_TRUE(authority_key_identifier.authority_cert_issuer);
  const uint8_t kExpectedIssuer[] = {0xa4, 0x11, 0x30, 0x0f, 0x31, 0x0d, 0x30,
                                     0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
                                     0x04, 0x52, 0x6f, 0x6f, 0x74};
  EXPECT_EQ(der::Input(kExpectedIssuer),
            authority_key_identifier.authority_cert_issuer);

  ASSERT_TRUE(authority_key_identifier.authority_cert_serial_number);
  const uint8_t kExpectedSerial[] = {0x27, 0x4F};
  EXPECT_EQ(der::Input(kExpectedSerial),
            authority_key_identifier.authority_cert_serial_number);
}

TEST(ParseAuthorityKeyIdentifierTest, KeyIdentifierAndIssuerAndSerial) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  ASSERT_TRUE(ParseAuthorityKeyIdentifierTestData(
      "key_identifier_and_issuer_and_serial.pem", &backing_bytes,
      &authority_key_identifier));

  ASSERT_TRUE(authority_key_identifier.key_identifier);
  const uint8_t kExpectedValue[] = {0xDE, 0xAD, 0xB0, 0x0F};
  EXPECT_EQ(der::Input(kExpectedValue),
            authority_key_identifier.key_identifier);

  ASSERT_TRUE(authority_key_identifier.authority_cert_issuer);
  const uint8_t kExpectedIssuer[] = {0xa4, 0x11, 0x30, 0x0f, 0x31, 0x0d, 0x30,
                                     0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
                                     0x04, 0x52, 0x6f, 0x6f, 0x74};
  EXPECT_EQ(der::Input(kExpectedIssuer),
            authority_key_identifier.authority_cert_issuer);

  ASSERT_TRUE(authority_key_identifier.authority_cert_serial_number);
  const uint8_t kExpectedSerial[] = {0x27, 0x4F};
  EXPECT_EQ(der::Input(kExpectedSerial),
            authority_key_identifier.authority_cert_serial_number);
}

TEST(ParseAuthorityKeyIdentifierTest, IssuerOnly) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "issuer_only.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, SerialOnly) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "serial_only.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, InvalidContents) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "invalid_contents.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, InvalidKeyIdentifier) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "invalid_key_identifier.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, InvalidIssuer) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "invalid_issuer.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, InvalidSerial) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "invalid_serial.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, ExtraContentsAfterIssuerAndSerial) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "extra_contents_after_issuer_and_serial.pem", &backing_bytes,
      &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, ExtraContentsAfterExtensionSequence) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "extra_contents_after_extension_sequence.pem", &backing_bytes,
      &authority_key_identifier));
}

TEST(ParseSubjectKeyIdentifierTest, EmptyInput) {
  der::Input subject_key_identifier;
  EXPECT_FALSE(
      ParseSubjectKeyIdentifier(der::Input(), &subject_key_identifier));
}

TEST(ParseSubjectKeyIdentifierTest, Valid) {
  // OCTET_STRING {`abcd`}
  const uint8_t kInput[] = {0x04, 0x02, 0xab, 0xcd};
  const uint8_t kExpected[] = {0xab, 0xcd};
  der::Input subject_key_identifier;
  EXPECT_TRUE(
      ParseSubjectKeyIdentifier(der::Input(kInput), &subject_key_identifier));
  EXPECT_EQ(der::Input(kExpected), subject_key_identifier);
}

TEST(ParseSubjectKeyIdentifierTest, ExtraData) {
  // OCTET_STRING {`abcd`}
  // NULL
  const uint8_t kInput[] = {0x04, 0x02, 0xab, 0xcd, 0x05};
  der::Input subject_key_identifier;
  EXPECT_FALSE(
      ParseSubjectKeyIdentifier(der::Input(kInput), &subject_key_identifier));
}

}  // namespace

}  // namespace net
