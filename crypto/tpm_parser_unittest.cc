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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto::tpm {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Optional;

namespace {

template <size_t N>
using ByteArray = std::array<uint8_t, N>;

template <size_t N>
constexpr ByteArray<N> ToByteArray(const uint8_t (&arr)[N]) {
  return std::to_array(arr);
}

constexpr auto kChallenge = ToByteArray({1, 2, 3, 4});

// Builds a serialized TPMT_SIGNATURE containing an RSASSA signature.
// TPMT_SIGNATURE layout (TPM 2.0 Part 2, Section 11.1.1):
// - sigAlg (TPMI_ALG_SIG_SCHEME): 2 bytes (uint16_t, TPM_ALG_RSASSA)
// - signature (union based on sigAlg):
//   For TPMS_SIGNATURE_RSASSA (Section 11.1.3):
//   - hash (TPMI_ALG_HASH): 2 bytes (uint16_t, hash_alg, e.g. TPM_ALG_SHA256)
//   - sig (TPM2B_PUBLIC_KEY_RSA):
//     - size (uint16_t): 2 bytes
//     - buffer (bytes): sig.size() bytes
std::vector<uint8_t> BuildTpmRsaSignature(TpmAlg hash_alg,
                                          base::span<const uint8_t> sig) {
  size_t size = 2 + 2 + 2 + sig.size();
  std::vector<uint8_t> tpm_sig(size);
  base::SpanWriter<uint8_t> writer(tpm_sig);
  writer.WriteEnumBigEndian(TPM_ALG_RSASSA);
  writer.WriteEnumBigEndian(hash_alg);
  writer.WriteU16BigEndian(sig.size());
  writer.Write(sig);
  CHECK_EQ(writer.remaining(), 0u);
  return tpm_sig;
}

// Builds a serialized TPMT_SIGNATURE containing an ECDSA signature.
// TPMT_SIGNATURE layout (TPM 2.0 Part 2, Section 11.1.1):
// - sigAlg (TPMI_ALG_SIG_SCHEME): 2 bytes (uint16_t, TPM_ALG_ECDSA)
// - signature (union based on sigAlg):
//   For TPMS_SIGNATURE_ECC (Section 11.1.5):
//   - hash (TPMI_ALG_HASH): 2 bytes (uint16_t, hash_alg, e.g. TPM_ALG_SHA256)
//   - signature (TPMS_ECC_POINT containing r and s as TPM2B_ECC_PARAMETERs):
//     - r.size (uint16_t): 2 bytes
//     - r.buffer (bytes): r.size() bytes
//     - s.size (uint16_t): 2 bytes
//     - s.buffer (bytes): s.size() bytes
std::vector<uint8_t> BuildTpmEcdsaSignature(TpmAlg hash_alg,
                                            base::span<const uint8_t> r,
                                            base::span<const uint8_t> s) {
  size_t size = 2 + 2 + 2 + r.size() + 2 + s.size();
  std::vector<uint8_t> tpm_sig(size);
  base::SpanWriter<uint8_t> writer(tpm_sig);
  writer.WriteEnumBigEndian(TPM_ALG_ECDSA);
  writer.WriteEnumBigEndian(hash_alg);
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
    TpmConstant magic = TPM_GENERATED_VALUE,
    TpmSt type = TPM_ST_ATTEST_CERTIFY) {
  size_t size = 4 + 2 + 2 + 2 + challenge.size() + 17 + 8 + 2 + 2;
  std::vector<uint8_t> statement(size);
  base::SpanWriter<uint8_t> writer(statement);
  writer.WriteEnumBigEndian(magic);
  writer.WriteEnumBigEndian(type);
  writer.WriteU16BigEndian(0);  // qualified_signer size = 0
  writer.WriteU16BigEndian(challenge.size());
  writer.Write(challenge);

  ByteArray<17> clock_info = {0};
  writer.Write(clock_info);

  ByteArray<8> firmware_version = {0};
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
    TpmConstant magic = TPM_GENERATED_VALUE,
    TpmSt type = TPM_ST_ATTEST_CERTIFY) {
  std::vector<uint8_t> statement =
      BuildFakeCertifyStatement(challenge, magic, type);

  uint32_t resp_size = 10;
  if (response_code == 0) {
    resp_size += 2 + statement.size() + signature.size();
  }

  std::vector<uint8_t> resp(resp_size);
  base::SpanWriter<uint8_t> writer(resp);
  writer.WriteEnumBigEndian(TPM_ST_NO_SESSIONS);
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
    TpmSt ticket_tag,
    TpmRh ticket_hierarchy,
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
  writer.WriteEnumBigEndian(TPM_ST_NO_SESSIONS);
  writer.WriteU32BigEndian(resp_size);
  writer.WriteU32BigEndian(response_code);

  if (response_code == 0) {
    writer.WriteU16BigEndian(digest.size());
    writer.Write(digest);
    writer.WriteEnumBigEndian(ticket_tag);
    writer.WriteEnumBigEndian(ticket_hierarchy);
    writer.WriteU16BigEndian(ticket_digest.size());
    writer.Write(ticket_digest);
  }

  CHECK_EQ(writer.remaining(), 0u);
  return resp;
}

std::vector<uint8_t> BuildFakeSignResponse(base::span<const uint8_t> signature,
                                           uint32_t response_code = 0,
                                           TpmSt tag = TPM_ST_SESSIONS) {
  size_t body_size = signature.size();
  size_t session_size = (tag == TPM_ST_SESSIONS) ? 5 : 0;
  uint32_t resp_size = 10;
  if (response_code == 0) {
    if (tag == TPM_ST_SESSIONS) {
      resp_size += 4;  // parameterSize
    }
    resp_size += body_size + session_size;
  }

  std::vector<uint8_t> resp(resp_size);
  base::SpanWriter<uint8_t> writer(resp);
  writer.WriteEnumBigEndian(tag);
  writer.WriteU32BigEndian(resp_size);
  writer.WriteU32BigEndian(response_code);

  if (response_code == 0) {
    if (tag == TPM_ST_SESSIONS) {
      writer.WriteU32BigEndian(body_size);
    }
    writer.Write(signature);
    if (tag == TPM_ST_SESSIONS) {
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

  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA256, sig_bytes);
  EXPECT_OK(VerifySignature(spki, kStatement, sig_blob));
}

TEST(TpmCppParserTest, VerifySignature_RsaSha1_Success) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();

  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA1, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA1, sig_bytes);
  EXPECT_OK(VerifySignature(spki, kStatement, sig_blob));
}

TEST(TpmCppParserTest, VerifySignature_EcdsaSha256_Success) {
  auto ec_priv = keypair::PrivateKey::GenerateEcP256();
  auto ec_pub = keypair::PublicKey::FromPrivateKey(ec_priv);
  std::vector<uint8_t> spki = ec_pub.ToSubjectPublicKeyInfo();

  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::ECDSA_SHA256, ec_priv, kStatement);

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> raw_sig,
                       ConvertEcdsaDerSignatureToRaw(ec_pub, sig_bytes));
  ASSERT_EQ(raw_sig.size(), 64u);

  auto [r_span, s_span] = base::span<const uint8_t, 64>(raw_sig).split_at<32>();

  auto sig_blob = BuildTpmEcdsaSignature(TPM_ALG_SHA256, r_span, s_span);
  EXPECT_OK(VerifySignature(spki, kStatement, sig_blob));
}

TEST(TpmCppParserTest, VerifySignature_UnsupportedSignatureAlgorithm) {
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});

  // 0x1234 is an unsupported signature algorithm
  EXPECT_THAT(VerifySignature(spki, kStatement,
                              std::to_array<uint8_t>({0x12, 0x34, 0x00, 0x0B,
                                                      0x00, 0x04, 1, 2, 3, 4})),
              ErrorIs(SignatureError::kUnsupportedSignatureAlgorithm));
}

TEST(TpmCppParserTest, VerifySignature_UnsupportedHashAlgorithm) {
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});
  static constexpr ByteArray<256> kDummySig{};

  EXPECT_THAT(VerifySignature(spki, kStatement,
                              BuildTpmRsaSignature(TPM_ALG_NULL, kDummySig)),
              ErrorIs(SignatureError::kUnsupportedHashAlgorithm));
}

TEST(TpmCppParserTest, VerifySignature_BufferTooSmall) {
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});

  std::vector<uint8_t> empty;
  EXPECT_THAT(VerifySignature(spki, kStatement, empty),
              ErrorIs(SignatureError::kBufferTooSmall));
}

TEST(TpmCppParserTest, VerifySignature_TrailingBytes) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA256, sig_bytes);
  sig_blob.push_back(0x99);  // Trailing garbage

  EXPECT_THAT(VerifySignature(spki, kStatement, sig_blob),
              ErrorIs(SignatureError::kTrailingBytes));
}

TEST(TpmCppParserTest, VerifySignature_InvalidPublicKey) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();
  spki[0] += 1;  // Corrupt the SPKI header

  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA256, sig_bytes);
  EXPECT_THAT(VerifySignature(spki, kStatement, sig_blob),
              ErrorIs(SignatureError::kInvalidPublicKey));
}

TEST(TpmCppParserTest, VerifySignature_InvalidSignature) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = test::FixedRsa2048PublicKeyForTesting();
  std::vector<uint8_t> spki = rsa_pub.ToSubjectPublicKeyInfo();

  static constexpr auto kStatement = ToByteArray({1, 2, 3, 4});
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, kStatement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA256, sig_bytes);

  // Verify with a different statement to trigger verification failure
  static constexpr auto kWrongStatement = ToByteArray({9, 9, 9, 9});
  EXPECT_THAT(VerifySignature(spki, kWrongStatement, sig_blob),
              ErrorIs(SignatureError::kInvalidSignature));
}

TEST(TpmCppParserTest, GetSignatureAlgorithms_Rsa_Success) {
  static constexpr ByteArray<256> kDummySig{};
  EXPECT_THAT(
      GetSignatureAlgorithms(BuildTpmRsaSignature(TPM_ALG_SHA256, kDummySig)),
      ValueIs(SignatureAlgorithms{
          .sig_alg = TPM_ALG_RSASSA,
          .hash_alg = TPM_ALG_SHA256,
      }));
}

TEST(TpmCppParserTest, GetSignatureAlgorithms_Ecdsa_Success) {
  static constexpr ByteArray<32> kR{};
  static constexpr ByteArray<32> kS{};
  EXPECT_THAT(
      GetSignatureAlgorithms(BuildTpmEcdsaSignature(TPM_ALG_SHA256, kR, kS)),
      ValueIs(SignatureAlgorithms{
          .sig_alg = TPM_ALG_ECDSA,
          .hash_alg = TPM_ALG_SHA256,
      }));
}

TEST(TpmCppParserTest, GetSignatureAlgorithms_UnsupportedSignatureAlgorithm) {
  // 0x1234 is an unsupported signature algorithm
  EXPECT_THAT(GetSignatureAlgorithms(std::to_array<uint8_t>(
                  {0x12, 0x34, 0x00, 0x0B, 0x00, 0x04, 1, 2, 3, 4})),
              ErrorIs(SignatureError::kUnsupportedSignatureAlgorithm));
}

TEST(TpmCppParserTest, GetSignatureAlgorithms_UnsupportedHashAlgorithm) {
  static constexpr ByteArray<256> kDummySig{};
  EXPECT_THAT(
      GetSignatureAlgorithms(BuildTpmRsaSignature(TPM_ALG_NULL, kDummySig)),
      ValueIs(SignatureAlgorithms{
          .sig_alg = TPM_ALG_RSASSA,
          .hash_alg = TPM_ALG_NULL,
      }));
}

TEST(TpmCppParserTest, ParseCertifyResponse_Success) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  std::vector<uint8_t> statement = BuildFakeCertifyStatement(kChallenge);
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, statement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA256, sig_bytes);
  auto resp = BuildFakeCertifyResponse(kChallenge, sig_blob);

  EXPECT_THAT(ParseCertifyResponse(resp, kChallenge),
              ValueIs(CertifyResponse{
                  .statement = statement,
                  .signature = sig_blob,
              }));
}

TEST(TpmCppParserTest, ParseCertifyResponse_BadMagic) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  std::vector<uint8_t> statement = BuildFakeCertifyStatement(
      kChallenge, static_cast<TpmConstant>(0x11223344));  // Bad magic
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, statement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA256, sig_bytes);
  auto resp = BuildFakeCertifyResponse(kChallenge, sig_blob, 0,
                                       static_cast<TpmConstant>(0x11223344));

  EXPECT_THAT(ParseCertifyResponse(resp, kChallenge),
              ErrorIs(TpmParseError(TpmParseError::Type::kBadMagicNumber)));
}

TEST(TpmCppParserTest, ParseCertifyResponse_ChallengeMismatch) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  std::vector<uint8_t> statement = BuildFakeCertifyStatement(kChallenge);
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, statement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA256, sig_bytes);
  auto resp = BuildFakeCertifyResponse(kChallenge, sig_blob);

  static constexpr auto kWrongChallenge = ToByteArray({9, 9, 9, 9});
  EXPECT_THAT(ParseCertifyResponse(resp, kWrongChallenge),
              ErrorIs(TpmParseError(TpmParseError::Type::kChallengeMismatch)));
}

TEST(TpmCppParserTest, ParseCertifyResponse_TpmError) {
  auto rsa_priv = test::FixedRsa2048PrivateKeyForTesting();
  std::vector<uint8_t> statement = BuildFakeCertifyStatement(kChallenge);
  auto sig_bytes =
      sign::Sign(sign::SignatureKind::RSA_PKCS1_SHA256, rsa_priv, statement);

  auto sig_blob = BuildTpmRsaSignature(TPM_ALG_SHA256, sig_bytes);
  auto resp = BuildFakeCertifyResponse(kChallenge, sig_blob,
                                       0x100);  // TPM error code 0x100

  EXPECT_THAT(
      ParseCertifyResponse(resp, kChallenge),
      ErrorIs(TpmParseError(TpmParseError::Type::kTpmErrorResponse, 0x100)));
}

TEST(TpmCppParserTest, BuildHashCommand) {
  static constexpr auto kData = ToByteArray({1, 2, 3, 4});
  TpmAlg hash_alg = TPM_ALG_SHA256;
  // Note: TPM_RH_OWNER (0x40000001) is used for standard keys and mock
  // validation tickets in unit tests. By contrast, TPM_RH_ENDORSEMENT
  // (0x4000000b) MUST be used for Windows Attestation Identity Keys (AIKs) in
  // production.
  TpmRh hierarchy = TPM_RH_OWNER;

  std::vector<uint8_t> cmd = BuildHashCommand(kData, hash_alg, hierarchy);
  EXPECT_EQ(cmd.size(), 22u);

  base::SpanReader<const uint8_t> reader(cmd);
  EXPECT_EQ(reader.ReadEnumBigEndian<TpmSt>(), TPM_ST_NO_SESSIONS);
  EXPECT_EQ(reader.ReadU32BigEndian(), 22u);
  EXPECT_EQ(reader.ReadEnumBigEndian<TpmCc>(), TPM_CC_HASH);

  EXPECT_EQ(reader.ReadU16BigEndian(), 4u);
  EXPECT_EQ(reader.Read<4>(), kData);
  EXPECT_EQ(reader.ReadEnumBigEndian<TpmAlg>(), hash_alg);
  EXPECT_EQ(reader.ReadEnumBigEndian<TpmRh>(), hierarchy);
}

TEST(TpmCppParserTest, ParseHashResponse_Success) {
  static constexpr auto kDigest = ToByteArray({1, 2, 3});
  static constexpr auto kTicketDigest = ToByteArray({4, 5, 6});
  std::vector<uint8_t> resp = BuildFakeHashResponse(
      kDigest, TPM_ST_HASHCHECK, TPM_RH_OWNER, kTicketDigest);

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
  static constexpr auto kDigest = ToByteArray({1, 2, 3});
  static constexpr auto kTicketDigest = ToByteArray({4, 5, 6});
  std::vector<uint8_t> resp = BuildFakeHashResponse(
      kDigest, TPM_ST_HASHCHECK, TPM_RH_OWNER, kTicketDigest, 0x100);

  auto parsed_or_error = ParseHashResponse(resp);
  EXPECT_THAT(
      parsed_or_error,
      ErrorIs(TpmParseError(TpmParseError::Type::kTpmErrorResponse, 0x100)));
}

TEST(TpmCppParserTest, BuildSignCommand) {
  uint32_t key_handle = 0x81000001;
  static constexpr auto kDigest = ToByteArray({1, 2, 3});
  TpmAlg sig_alg = TPM_ALG_ECDSA;
  TpmAlg hash_alg = TPM_ALG_SHA256;
  static constexpr auto kTicket = ToByteArray({7, 8, 9, 10});

  std::vector<uint8_t> cmd =
      BuildSignCommand(key_handle, kDigest, sig_alg, hash_alg, kTicket);
  EXPECT_EQ(cmd.size(), 40u);

  base::SpanReader<const uint8_t> reader(cmd);
  EXPECT_EQ(reader.ReadEnumBigEndian<TpmSt>(), TPM_ST_SESSIONS);
  EXPECT_EQ(reader.ReadU32BigEndian(), 40u);
  EXPECT_EQ(reader.ReadEnumBigEndian<TpmCc>(), TPM_CC_SIGN);

  EXPECT_EQ(reader.ReadU32BigEndian(), key_handle);
  EXPECT_EQ(reader.ReadU32BigEndian(), 9u);
  EXPECT_TRUE(reader.Read<9>().has_value());

  EXPECT_EQ(reader.ReadU16BigEndian(), 3u);
  EXPECT_EQ(reader.Read<3>(), kDigest);
  EXPECT_EQ(reader.ReadEnumBigEndian<TpmAlg>(), sig_alg);
  EXPECT_EQ(reader.ReadEnumBigEndian<TpmAlg>(), hash_alg);
  EXPECT_EQ(reader.Read<4>(), kTicket);
}

TEST(TpmCppParserTest, ParseSignResponse_Success) {
  static constexpr auto kDummySig = ToByteArray({0xAA, 0xBB});
  std::vector<uint8_t> sig_blob =
      BuildTpmRsaSignature(TPM_ALG_SHA256, kDummySig);
  std::vector<uint8_t> resp = BuildFakeSignResponse(sig_blob);

  auto parsed_or_error = ParseSignResponse(resp);
  ASSERT_OK_AND_ASSIGN(SignResponse parsed, parsed_or_error);
  EXPECT_EQ(parsed.signature, sig_blob);
}

TEST(TpmCppParserTest, ParseTpmSignature_RsaSuccess) {
  static constexpr auto kDummySig = ToByteArray({0xAA, 0xBB});
  EXPECT_THAT(
      ParseTpmSignature(BuildTpmRsaSignature(TPM_ALG_SHA256, kDummySig)),
      Optional(ElementsAreArray(kDummySig)));
}

TEST(TpmCppParserTest, ParseTpmSignature_EcdsaSuccess) {
  static constexpr auto kValidRawSignature = ToByteArray({
      0x74, 0xa0, 0x6f, 0x6b, 0x2b, 0x0e, 0x82, 0x0e, 0x03, 0x3b, 0x6e,
      0x98, 0xfc, 0x89, 0x9c, 0xf3, 0x30, 0xb5, 0x56, 0xd3, 0x29, 0x89,
      0xb5, 0x82, 0x33, 0x5f, 0x9d, 0x97, 0xfb, 0x65, 0x64, 0x90, 0xbc,
      0xb5, 0xee, 0x42, 0xe2, 0x5a, 0x87, 0xae, 0x21, 0x18, 0xda, 0x7e,
      0x68, 0x65, 0x30, 0xbe, 0xe5, 0x69, 0x3d, 0xc5, 0x5f, 0xd5, 0x62,
      0x45, 0x3e, 0x8d, 0x0b, 0x05, 0x1a, 0x33, 0x79, 0x8d,
  });
  static constexpr auto kR =
      base::span(kValidRawSignature).split_at<32>().first;
  static constexpr auto kS =
      base::span(kValidRawSignature).split_at<32>().second;
  static constexpr auto kValidDerSignature = ToByteArray({
      0x30, 0x45, 0x02, 0x20, 0x74, 0xa0, 0x6f, 0x6b, 0x2b, 0x0e, 0x82, 0x0e,
      0x03, 0x3b, 0x6e, 0x98, 0xfc, 0x89, 0x9c, 0xf3, 0x30, 0xb5, 0x56, 0xd3,
      0x29, 0x89, 0xb5, 0x82, 0x33, 0x5f, 0x9d, 0x97, 0xfb, 0x65, 0x64, 0x90,
      0x02, 0x21, 0x00, 0xbc, 0xb5, 0xee, 0x42, 0xe2, 0x5a, 0x87, 0xae, 0x21,
      0x18, 0xda, 0x7e, 0x68, 0x65, 0x30, 0xbe, 0xe5, 0x69, 0x3d, 0xc5, 0x5f,
      0xd5, 0x62, 0x45, 0x3e, 0x8d, 0x0b, 0x05, 0x1a, 0x33, 0x79, 0x8d,
  });

  EXPECT_THAT(ParseTpmSignature(BuildTpmEcdsaSignature(TPM_ALG_SHA256, kR, kS)),
              Optional(base::ToVector(kValidDerSignature)));
}

TEST(TpmCppParserTest, ParseTpmSignature_InvalidAlgorithm) {
  static constexpr auto kDummySig = ToByteArray({0xAA, 0xBB});
  size_t size = 2 + 2 + 2 + kDummySig.size();
  std::vector<uint8_t> tpm_sig(size);
  base::SpanWriter<uint8_t> writer(tpm_sig);
  writer.WriteEnumBigEndian(TPM_ALG_SHA256);  // Invalid sigAlg.
  writer.WriteEnumBigEndian(TPM_ALG_SHA256);
  writer.WriteU16BigEndian(kDummySig.size());
  writer.Write(kDummySig);

  EXPECT_EQ(ParseTpmSignature(tpm_sig), std::nullopt);
}

TEST(TpmCppParserTest, ParseTpmSignature_MalformedBlob) {
  static constexpr auto kMalformedSig = ToByteArray({1, 2, 3});
  EXPECT_EQ(ParseTpmSignature(kMalformedSig), std::nullopt);
}

}  // namespace crypto::tpm
