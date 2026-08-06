// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/tpm_parser.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/containers/to_vector.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "crypto/test_support.h"
#include "crypto/tpm.rs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto::tpm {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;

namespace {

constexpr uint16_t kTpmAlgRsaSsa = std::to_underlying(TpmAlg::TPM_ALG_RSASSA);
constexpr uint16_t kTpmAlgEcdsa = std::to_underlying(TpmAlg::TPM_ALG_ECDSA);
constexpr uint16_t kTpmAlgSha1 = std::to_underlying(TpmAlg::TPM_ALG_SHA1);
constexpr uint16_t kTpmAlgSha256 = std::to_underlying(TpmAlg::TPM_ALG_SHA256);

constexpr uint32_t kTpmGeneratedValue =
    std::to_underlying(TpmConstant::TPM_GENERATED_VALUE);
constexpr uint32_t kTpmCcHash = std::to_underlying(TpmCc::TPM_CC_HASH);
constexpr uint32_t kTpmCcSign = std::to_underlying(TpmCc::TPM_CC_SIGN);
constexpr uint16_t kTpmStNoSessions =
    std::to_underlying(TpmSt::TPM_ST_NO_SESSIONS);
constexpr uint16_t kTpmStSessions = std::to_underlying(TpmSt::TPM_ST_SESSIONS);
constexpr uint16_t kTpmStAttestCertify =
    std::to_underlying(TpmSt::TPM_ST_ATTEST_CERTIFY);
constexpr uint16_t kTpmStHashcheck =
    std::to_underlying(TpmSt::TPM_ST_HASHCHECK);

constexpr std::array<uint8_t, 4> kChallenge = {1, 2, 3, 4};

// Builds a serialized TPMT_SIGNATURE containing an RSASSA signature.
// TPMT_SIGNATURE layout (TPM 2.0 Part 2, Section 11.1.1):
// - sigAlg (TPMI_ALG_SIG_SCHEME): 2 bytes (uint16_t, e.g. kTpmAlgRsaSsa)
// - signature (union based on sigAlg):
//   For TPMS_SIGNATURE_RSASSA (Section 11.1.3):
//   - hash (TPMI_ALG_HASH): 2 bytes (uint16_t, e.g. hash_alg)
//   - sig (TPM2B_PUBLIC_KEY_RSA):
//     - size (uint16_t): 2 bytes
//     - buffer (bytes): sig.size() bytes
std::vector<uint8_t> BuildTpmRsaSignature(uint16_t hash_alg,
                                          base::span<const uint8_t> sig) {
  size_t size = 2 + 2 + 2 + sig.size();
  std::vector<uint8_t> tpm_sig(size);
  base::SpanWriter<uint8_t> writer(tpm_sig);
  writer.WriteU16BigEndian(kTpmAlgRsaSsa);
  writer.WriteU16BigEndian(hash_alg);
  writer.WriteU16BigEndian(sig.size());
  writer.Write(sig);
  CHECK_EQ(writer.remaining(), 0u);
  return tpm_sig;
}

// Builds a serialized TPMT_SIGNATURE containing an ECDSA signature.
// TPMT_SIGNATURE layout (TPM 2.0 Part 2, Section 11.1.1):
// - sigAlg (TPMI_ALG_SIG_SCHEME): 2 bytes (uint16_t, e.g. kTpmAlgEcdsa)
// - signature (union based on sigAlg):
//   For TPMS_SIGNATURE_ECC (Section 11.1.5):
//   - hash (TPMI_ALG_HASH): 2 bytes (uint16_t, e.g. hash_alg)
//   - signature (TPMS_ECC_POINT containing r and s as TPM2B_ECC_PARAMETERs):
//     - r.size (uint16_t): 2 bytes
//     - r.buffer (bytes): r.size() bytes
//     - s.size (uint16_t): 2 bytes
//     - s.buffer (bytes): s.size() bytes
std::vector<uint8_t> BuildTpmEcdsaSignature(uint16_t hash_alg,
                                            base::span<const uint8_t> r,
                                            base::span<const uint8_t> s) {
  size_t size = 2 + 2 + 2 + r.size() + 2 + s.size();
  std::vector<uint8_t> tpm_sig(size);
  base::SpanWriter<uint8_t> writer(tpm_sig);
  writer.WriteU16BigEndian(kTpmAlgEcdsa);
  writer.WriteU16BigEndian(hash_alg);
  writer.WriteU16BigEndian(r.size());
  writer.Write(r);
  writer.WriteU16BigEndian(s.size());
  writer.Write(s);
  CHECK_EQ(writer.remaining(), 0u);
  return tpm_sig;
}

// Builds a serialized TPMS_ATTEST structure representing a certify statement.
// TPMS_ATTEST layout (TPM 2.0 Part 2, Section 10.12.1):
// - magic (TPM_GENERATED): 4 bytes (uint32_t, e.g. magic)
// - type (TPMI_ST_ATTEST): 2 bytes (uint16_t, e.g. type)
// - qualifiedSigner (TPM2B_NAME): 2 bytes size + 0 bytes buffer = 2 bytes
// - extraData (TPM2B_DATA, holds challenge):
//   - size (uint16_t): 2 bytes
//   - buffer (bytes): challenge.size() bytes
// - clockInfo (TPMS_CLOCK_INFO): 17 bytes (8 clock + 4 reset + 4 restart + 1
// safe)
// - firmwareVersion (uint64_t): 8 bytes
// - attested (union based on type):
//   For TPMS_CERTIFY_INFO (Section 10.12.3):
//   - name (TPM2B_NAME): 2 bytes size + 0 bytes buffer = 2 bytes
//   - qualifiedName (TPM2B_NAME): 2 bytes size + 0 bytes buffer = 2 bytes
std::vector<uint8_t> BuildFakeCertifyStatement(
    base::span<const uint8_t> challenge,
    uint32_t magic,
    uint16_t type) {
  size_t size = 4 + 2 + 2 + 2 + challenge.size() + 17 + 8 + 2 + 2;
  std::vector<uint8_t> statement(size);
  base::SpanWriter<uint8_t> writer(statement);
  writer.WriteU32BigEndian(magic);
  writer.WriteU16BigEndian(type);
  writer.WriteU16BigEndian(0);  // qualified_signer size = 0
  writer.WriteU16BigEndian(challenge.size());
  writer.Write(challenge);

  std::array<uint8_t, 17> clock_info = {0};
  writer.Write(clock_info);

  std::array<uint8_t, 8> firmware_version = {0};
  writer.Write(firmware_version);

  writer.WriteU16BigEndian(0);  // name size = 0
  writer.WriteU16BigEndian(0);  // qualified_name size = 0

  CHECK_EQ(writer.remaining(), 0u);
  return statement;
}

// Builds a serialized TPM response representing a certify command output.
// TPM Response Layout:
// - Header (10 bytes):
//   - tag (TPMI_ST_COMMAND_TAG): 2 bytes (uint16_t, TPM_ST_NO_SESSIONS =
//   0x8001)
//   - responseSize (uint32_t): 4 bytes (total response size)
//   - responseCode (TPM_RC): 4 bytes (response_code)
// - Body (only if response_code == 0):
//   - certifyInfo (TPM2B_ATTEST):
//     - size (uint16_t): 2 bytes
//     - buffer (bytes): statement.size() bytes
//   - signature (TPMT_SIGNATURE): signature.size() bytes
std::vector<uint8_t> BuildFakeCertifyResponse(
    base::span<const uint8_t> challenge,
    base::span<const uint8_t> signature,
    uint32_t response_code = 0,
    uint32_t magic = kTpmGeneratedValue,
    uint16_t type = kTpmStAttestCertify) {
  std::vector<uint8_t> statement =
      BuildFakeCertifyStatement(challenge, magic, type);

  uint32_t resp_size = 10;
  if (response_code == 0) {
    resp_size += 2 + statement.size() + signature.size();
  }

  std::vector<uint8_t> resp(resp_size);
  base::SpanWriter<uint8_t> writer(resp);
  writer.WriteU16BigEndian(kTpmStNoSessions);
  writer.WriteU32BigEndian(resp_size);
  writer.WriteU32BigEndian(response_code);

  if (response_code == 0) {
    writer.WriteU16BigEndian(statement.size());
    writer.Write(statement);
    writer.Write(signature);
  }

  CHECK_EQ(writer.remaining(), 0u);
  return resp;
}

std::vector<uint8_t> BuildFakeHashResponse(
    base::span<const uint8_t> digest,
    uint16_t ticket_tag,
    uint32_t ticket_hierarchy,
    base::span<const uint8_t> ticket_digest,
    uint32_t response_code = 0) {
  // TPMT_TK_HASHCHECK size: 2 bytes tag + 4 bytes hierarchy + 2 bytes digest
  // size prefix + digest.
  size_t ticket_size = 2 + 4 + 2 + ticket_digest.size();
  size_t body_size = 2 + digest.size() + ticket_size;
  uint32_t resp_size = 10;
  if (response_code == 0) {
    resp_size += body_size;
  }

  std::vector<uint8_t> resp(resp_size);
  base::SpanWriter<uint8_t> writer(resp);
  writer.WriteU16BigEndian(kTpmStNoSessions);
  writer.WriteU32BigEndian(resp_size);
  writer.WriteU32BigEndian(response_code);

  if (response_code == 0) {
    writer.WriteU16BigEndian(digest.size());
    writer.Write(digest);
    writer.WriteU16BigEndian(ticket_tag);
    writer.WriteU32BigEndian(ticket_hierarchy);
    writer.WriteU16BigEndian(ticket_digest.size());
    writer.Write(ticket_digest);
  }

  CHECK_EQ(writer.remaining(), 0u);
  return resp;
}

std::vector<uint8_t> BuildFakeSignResponse(base::span<const uint8_t> signature,
                                           uint32_t response_code = 0,
                                           uint16_t tag = kTpmStSessions) {
  size_t body_size = signature.size();
  size_t session_size = (tag == kTpmStSessions) ? 5 : 0;
  uint32_t resp_size = 10;
  if (response_code == 0) {
    if (tag == kTpmStSessions) {
      resp_size += 4;  // parameterSize
    }
    resp_size += body_size + session_size;
  }

  std::vector<uint8_t> resp(resp_size);
  base::SpanWriter<uint8_t> writer(resp);
  writer.WriteU16BigEndian(tag);
  writer.WriteU32BigEndian(resp_size);
  writer.WriteU32BigEndian(response_code);

  if (response_code == 0) {
    if (tag == kTpmStSessions) {
      writer.WriteU32BigEndian(body_size);
    }
    writer.Write(signature);
    if (tag == kTpmStSessions) {
      writer.WriteU16BigEndian(0);  // nonce size
      writer.WriteU8BigEndian(0);   // sessionAttributes
      writer.WriteU16BigEndian(0);  // HMAC size
    }
  }

  CHECK_EQ(writer.remaining(), 0u);
  return resp;
}

}  // namespace

TEST(TpmCppParserTest, VerifySignature_RsaSha256_Success) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();

  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha256, sig_bytes);
  EXPECT_OK(VerifySignature(spki, kStatement, sig_blob));
}

TEST(TpmCppParserTest, VerifySignature_RsaSha1_Success) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();

  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA1, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha1, sig_bytes);
  EXPECT_OK(VerifySignature(spki, kStatement, sig_blob));
}

TEST(TpmCppParserTest, VerifySignature_EcdsaSha256_Success) {
  auto ec_priv = keypair::PrivateKey::GenerateEcP256();
  auto ec_pub = keypair::PublicKey::FromPrivateKey(ec_priv);
  std::vector<uint8_t> spki = ec_pub.ToSubjectPublicKeyInfo();

  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::ECDSA_SHA256, ec_priv, kStatement);

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> raw_sig,
                       ConvertEcdsaDerSignatureToRaw(ec_pub, sig_bytes));
  ASSERT_EQ(raw_sig.size(), 64u);

  auto [r_span, s_span] = base::span<const uint8_t, 64>(raw_sig).split_at<32>();

  auto sig_blob = BuildTpmEcdsaSignature(kTpmAlgSha256, r_span, s_span);
  EXPECT_OK(VerifySignature(spki, kStatement, sig_blob));
}

TEST(TpmCppParserTest, VerifySignature_UnsupportedSignatureAlgorithm) {
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};

  // 0x1234 is an unsupported signature algorithm
  EXPECT_THAT(VerifySignature(spki, kStatement,
                              std::to_array<uint8_t>({0x12, 0x34, 0x00, 0x0B,
                                                      0x00, 0x04, 1, 2, 3, 4})),
              ErrorIs(SignatureError::kUnsupportedSignatureAlgorithm));
}

TEST(TpmCppParserTest, VerifySignature_UnsupportedHashAlgorithm) {
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};
  static constexpr uint8_t kDummySig[256] = {0};

  EXPECT_THAT(VerifySignature(spki, kStatement,
                              BuildTpmRsaSignature(0x0010, kDummySig)),
              ErrorIs(SignatureError::kUnsupportedHashAlgorithm));
}

TEST(TpmCppParserTest, VerifySignature_BufferTooSmall) {
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};

  std::vector<uint8_t> empty;
  EXPECT_THAT(VerifySignature(spki, kStatement, empty),
              ErrorIs(SignatureError::kBufferTooSmall));
}

TEST(TpmCppParserTest, VerifySignature_TrailingBytes) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha256, sig_bytes);
  sig_blob.push_back(0x99);  // Trailing garbage

  EXPECT_THAT(VerifySignature(spki, kStatement, sig_blob),
              ErrorIs(SignatureError::kTrailingBytes));
}

TEST(TpmCppParserTest, VerifySignature_InvalidPublicKey) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  spki[0] += 1;  // Corrupt the SPKI header

  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha256, sig_bytes);
  EXPECT_THAT(VerifySignature(spki, kStatement, sig_blob),
              ErrorIs(SignatureError::kInvalidPublicKey));
}

TEST(TpmCppParserTest, VerifySignature_InvalidSignature) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();

  static constexpr uint8_t kStatement[] = {1, 2, 3, 4};
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha256, sig_bytes);

  // Verify with a different statement to trigger verification failure
  static constexpr uint8_t kWrongStatement[] = {9, 9, 9, 9};
  EXPECT_THAT(VerifySignature(spki, kWrongStatement, sig_blob),
              ErrorIs(SignatureError::kInvalidSignature));
}

TEST(TpmCppParserTest, GetSignatureAlgorithms_Rsa_Success) {
  static constexpr uint8_t kDummySig[256] = {0};
  EXPECT_THAT(
      GetSignatureAlgorithms(BuildTpmRsaSignature(kTpmAlgSha256, kDummySig)),
      ValueIs(SignatureAlgorithms{
          .sig_alg = kTpmAlgRsaSsa,
          .hash_alg = kTpmAlgSha256,
      }));
}

TEST(TpmCppParserTest, GetSignatureAlgorithms_Ecdsa_Success) {
  static constexpr uint8_t kR[32] = {0};
  static constexpr uint8_t kS[32] = {0};
  EXPECT_THAT(
      GetSignatureAlgorithms(BuildTpmEcdsaSignature(kTpmAlgSha256, kR, kS)),
      ValueIs(SignatureAlgorithms{
          .sig_alg = kTpmAlgEcdsa,
          .hash_alg = kTpmAlgSha256,
      }));
}

TEST(TpmCppParserTest, GetSignatureAlgorithms_UnsupportedSignatureAlgorithm) {
  // 0x1234 is an unsupported signature algorithm
  EXPECT_THAT(GetSignatureAlgorithms(std::to_array<uint8_t>(
                  {0x12, 0x34, 0x00, 0x0B, 0x00, 0x04, 1, 2, 3, 4})),
              ErrorIs(SignatureError::kUnsupportedSignatureAlgorithm));
}

TEST(TpmCppParserTest, GetSignatureAlgorithms_UnsupportedHashAlgorithm) {
  static constexpr uint8_t kDummySig[256] = {0};
  constexpr uint16_t kUnsupportedHashAlg = 0x0010;  // TPM_ALG_NULL
  EXPECT_THAT(GetSignatureAlgorithms(
                  BuildTpmRsaSignature(kUnsupportedHashAlg, kDummySig)),
              ValueIs(SignatureAlgorithms{
                  .sig_alg = kTpmAlgRsaSsa,
                  .hash_alg = kUnsupportedHashAlg,
              }));
}

TEST(TpmCppParserTest, ParseCertifyResponse_Success) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  std::vector<uint8_t> statement = BuildFakeCertifyStatement(
      kChallenge, kTpmGeneratedValue, kTpmStAttestCertify);
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, statement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha256, sig_bytes);
  auto resp = BuildFakeCertifyResponse(kChallenge, sig_blob);

  EXPECT_THAT(ParseCertifyResponse(resp, kChallenge),
              ValueIs(CertifyResponse{
                  .statement = statement,
                  .signature = sig_blob,
              }));
}

TEST(TpmCppParserTest, ParseCertifyResponse_BadMagic) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  std::vector<uint8_t> statement =
      BuildFakeCertifyStatement(kChallenge, 0x11223344, 0x8017);  // Bad magic
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, statement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha256, sig_bytes);
  auto resp = BuildFakeCertifyResponse(kChallenge, sig_blob, 0, 0x11223344);

  EXPECT_THAT(ParseCertifyResponse(resp, kChallenge),
              ErrorIs(TpmParseError(TpmParseError::Type::kBadMagicNumber)));
}

TEST(TpmCppParserTest, ParseCertifyResponse_ChallengeMismatch) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  std::vector<uint8_t> statement = BuildFakeCertifyStatement(
      kChallenge, kTpmGeneratedValue, kTpmStAttestCertify);
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, statement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha256, sig_bytes);
  auto resp = BuildFakeCertifyResponse(kChallenge, sig_blob);

  static constexpr uint8_t kWrongChallenge[] = {9, 9, 9, 9};
  EXPECT_THAT(ParseCertifyResponse(resp, kWrongChallenge),
              ErrorIs(TpmParseError(TpmParseError::Type::kChallengeMismatch)));
}

TEST(TpmCppParserTest, ParseCertifyResponse_TpmError) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  std::vector<uint8_t> statement = BuildFakeCertifyStatement(
      kChallenge, kTpmGeneratedValue, kTpmStAttestCertify);
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, statement);

  auto sig_blob = BuildTpmRsaSignature(kTpmAlgSha256, sig_bytes);
  auto resp = BuildFakeCertifyResponse(kChallenge, sig_blob,
                                       0x100);  // TPM error code 0x100

  EXPECT_THAT(
      ParseCertifyResponse(resp, kChallenge),
      ErrorIs(TpmParseError(TpmParseError::Type::kTpmErrorResponse, 0x100)));
}

TEST(TpmCppParserTest, BuildHashCommand) {
  static constexpr uint8_t kData[] = {1, 2, 3, 4};
  uint16_t hash_alg = kTpmAlgSha256;
  // Note: kTpmRhOwner (0x40000001) is used for standard keys and mock
  // validation tickets in unit tests. By contrast, kTpmRhEndorsement
  // (0x4000000b) MUST be used for Windows Attestation Identity Keys (AIKs) in
  // production.
  uint32_t hierarchy = kTpmRhOwner;

  std::vector<uint8_t> cmd = BuildHashCommand(kData, hash_alg, hierarchy);
  EXPECT_EQ(cmd.size(), 22u);

  base::SpanReader<const uint8_t> reader(cmd);
  uint16_t tag;
  uint32_t size, cc;
  ASSERT_TRUE(reader.ReadU16BigEndian(tag));
  ASSERT_TRUE(reader.ReadU32BigEndian(size));
  ASSERT_TRUE(reader.ReadU32BigEndian(cc));
  EXPECT_EQ(tag, kTpmStNoSessions);
  EXPECT_EQ(size, 22u);
  EXPECT_EQ(cc, kTpmCcHash);

  uint16_t data_len;
  ASSERT_TRUE(reader.ReadU16BigEndian(data_len));
  EXPECT_EQ(data_len, 4u);
  auto data_span = reader.Read<4>();
  ASSERT_TRUE(data_span.has_value());
  EXPECT_THAT(*data_span, ElementsAre(1, 2, 3, 4));

  uint16_t alg;
  ASSERT_TRUE(reader.ReadU16BigEndian(alg));
  EXPECT_EQ(alg, hash_alg);

  uint32_t hier;
  ASSERT_TRUE(reader.ReadU32BigEndian(hier));
  EXPECT_EQ(hier, hierarchy);
}

TEST(TpmCppParserTest, ParseHashResponse_Success) {
  static constexpr uint8_t kDigest[] = {1, 2, 3};
  static constexpr uint8_t kTicketDigest[] = {4, 5, 6};
  std::vector<uint8_t> resp = BuildFakeHashResponse(kDigest, kTpmStHashcheck,
                                                    kTpmRhOwner, kTicketDigest);

  auto parsed_or_error = ParseHashResponse(resp);
  ASSERT_OK_AND_ASSIGN(HashResponse parsed, parsed_or_error);
  EXPECT_THAT(parsed.digest, ElementsAre(1, 2, 3));

  std::vector<uint8_t> expected_ticket = {
      0x80, 0x24,              // tag
      0x40, 0x00, 0x00, 0x01,  // hierarchy
      0x00, 0x03,              // digest size
      4,    5,    6            // digest
  };
  EXPECT_EQ(parsed.validation_ticket, expected_ticket);
}

TEST(TpmCppParserTest, ParseHashResponse_TpmError) {
  static constexpr uint8_t kDigest[] = {1, 2, 3};
  static constexpr uint8_t kTicketDigest[] = {4, 5, 6};
  std::vector<uint8_t> resp = BuildFakeHashResponse(
      kDigest, kTpmStHashcheck, kTpmRhOwner, kTicketDigest, 0x100);

  auto parsed_or_error = ParseHashResponse(resp);
  EXPECT_THAT(
      parsed_or_error,
      ErrorIs(TpmParseError(TpmParseError::Type::kTpmErrorResponse, 0x100)));
}

TEST(TpmCppParserTest, BuildSignCommand) {
  uint32_t key_handle = 0x81000001;
  static constexpr uint8_t kDigest[] = {1, 2, 3};
  uint16_t sig_alg = kTpmAlgEcdsa;
  uint16_t hash_alg = kTpmAlgSha256;
  static constexpr uint8_t kTicket[] = {7, 8, 9, 10};

  std::vector<uint8_t> cmd =
      BuildSignCommand(key_handle, kDigest, sig_alg, hash_alg, kTicket);
  EXPECT_EQ(cmd.size(), 40u);

  base::SpanReader<const uint8_t> reader(cmd);
  uint16_t tag;
  uint32_t size, cc;
  ASSERT_TRUE(reader.ReadU16BigEndian(tag));
  ASSERT_TRUE(reader.ReadU32BigEndian(size));
  ASSERT_TRUE(reader.ReadU32BigEndian(cc));
  EXPECT_EQ(tag, kTpmStSessions);
  EXPECT_EQ(size, 40u);
  EXPECT_EQ(cc, kTpmCcSign);

  uint32_t handle;
  ASSERT_TRUE(reader.ReadU32BigEndian(handle));
  EXPECT_EQ(handle, key_handle);

  uint32_t auth_size;
  ASSERT_TRUE(reader.ReadU32BigEndian(auth_size));
  EXPECT_EQ(auth_size, 9u);
  auto auth_span = reader.Read<9>();
  ASSERT_TRUE(auth_span.has_value());

  uint16_t digest_len;
  ASSERT_TRUE(reader.ReadU16BigEndian(digest_len));
  EXPECT_EQ(digest_len, 3u);
  auto digest_span = reader.Read<3>();
  ASSERT_TRUE(digest_span.has_value());
  EXPECT_THAT(*digest_span, ElementsAre(1, 2, 3));

  uint16_t sig_scheme;
  ASSERT_TRUE(reader.ReadU16BigEndian(sig_scheme));
  EXPECT_EQ(sig_scheme, sig_alg);

  uint16_t hash;
  ASSERT_TRUE(reader.ReadU16BigEndian(hash));
  EXPECT_EQ(hash, hash_alg);

  auto ticket_span = reader.Read<4>();
  ASSERT_TRUE(ticket_span.has_value());
  EXPECT_THAT(*ticket_span, ElementsAre(7, 8, 9, 10));
}

TEST(TpmCppParserTest, ParseSignResponse_Success) {
  static constexpr uint8_t kDummySig[] = {0xAA, 0xBB};
  std::vector<uint8_t> sig_blob =
      BuildTpmRsaSignature(kTpmAlgSha256, kDummySig);
  std::vector<uint8_t> resp = BuildFakeSignResponse(sig_blob);

  auto parsed_or_error = ParseSignResponse(resp);
  ASSERT_OK_AND_ASSIGN(SignResponse parsed, parsed_or_error);
  EXPECT_EQ(parsed.signature, sig_blob);
}

}  // namespace crypto::tpm
