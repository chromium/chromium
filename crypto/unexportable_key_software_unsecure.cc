// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "crypto/tpm_parser.h"
#include "crypto/unexportable_key.h"

namespace crypto {

namespace {

// Small helper to write a TPM2B sized buffer. Consisting of a uint16_t size and
// payload.
void WriteTpm2b(base::SpanWriter<uint8_t>& writer,
                base::span<const uint8_t> data) {
  CHECK_LE(data.size(), std::numeric_limits<uint16_t>::max());
  CHECK(writer.WriteU16BigEndian(data.size()));
  CHECK(writer.Write(data));
}

// Generates a fake TPM 2.0 certification statement (TPMS_ATTEST) for the
// given signing key and challenge.
// See TCG TPM 2.0 Library Specification, Part 2: Structures
// (https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf).
std::vector<uint8_t> CreateTpm2bAttestationStatement(
    base::span<const uint8_t> subject_key,
    base::span<const uint8_t> challenge) {
  // TPM_ALG_SHA256 + hash
  static constexpr size_t kNameBufSize = 2 + hash::kSha256Size;
  static constexpr size_t kExtraDataBufSize = 2 + hash::kSha256Size;

  // TPMS_ATTEST structure size:
  // - magic: 4 bytes (TPM_GENERATED)
  // - type: 2 bytes (TPMI_ST_ATTEST)
  // - qualifiedSigner: 2 bytes (TPM2B_NAME header, empty name)
  // - extraData: 34 bytes (TPM2B_DATA with 32-byte SHA-256 digest of challenge)
  // - clockInfo: 17 bytes (TPMS_CLOCK_INFO)
  // - firmwareVersion: 8 bytes (uint64_t)
  // - attested (TPMS_CERTIFY_INFO):
  //   - name: 36 bytes (TPM2B_NAME with SHA-256 algorithm ID + 32-byte digest)
  //   - qualifiedName: 2 bytes (TPM2B_NAME header, empty name)
  static constexpr size_t kAttestationStatementFixedSize =
      4 + 2 + 2 + kExtraDataBufSize + 17 + 8 + (2 + kNameBufSize) + 2;

  std::vector<uint8_t> attestation_statement(kAttestationStatementFixedSize);
  base::SpanWriter<uint8_t> attest_writer(attestation_statement);
  attest_writer.WriteEnumBigEndian(tpm::TPM_GENERATED_VALUE);
  attest_writer.WriteEnumBigEndian(tpm::TPM_ST_ATTEST_CERTIFY);
  // qualifiedSigner (empty)
  attest_writer.WriteU16BigEndian(0);

  // extraData
  WriteTpm2b(attest_writer, hash::Sha256(challenge));

  // TPMS_CLOCK_INFO (17 bytes)
  attest_writer.WriteU64BigEndian(0);  // clock
  attest_writer.WriteU32BigEndian(0);  // resetCount
  attest_writer.WriteU32BigEndian(0);  // restartCount
  attest_writer.WriteU8BigEndian(1);   // safe (YES)

  // firmwareVersion
  attest_writer.WriteU64BigEndian(0);

  // TPMS_CERTIFY_INFO
  // name: TPM2B_NAME
  std::array<uint8_t, kNameBufSize> name_buf;
  base::SpanWriter<uint8_t> name_writer(name_buf);
  name_writer.WriteEnumBigEndian(tpm::TPM_ALG_SHA256);
  name_writer.Write(hash::Sha256(subject_key));
  CHECK_EQ(name_writer.remaining(), 0u);

  WriteTpm2b(attest_writer, name_buf);

  // qualifiedName: TPM2B_NAME (empty)
  attest_writer.WriteU16BigEndian(0);

  CHECK_EQ(attest_writer.remaining(), 0u);
  return attestation_statement;
}

// Constructs a TPM 2.0 TPMT_PUBLIC structure for the given public key.
// See TCG TPM 2.0 Library Specification, Part 2: Structures, Section 12.2.4
// (https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=197).
std::vector<uint8_t> CreateTpmtPublic(const keypair::PublicKey& public_key) {
  static constexpr uint32_t kObjectAttributes = 0x00040072;

  if (public_key.IsEcP256()) {
    std::vector<uint8_t> ec_point = public_key.ToUncompressedX962Point();
    CHECK_EQ(ec_point.size(), 65u);
    auto [x, y] = base::span(ec_point).subspan<1, 64>().split_at<32>();

    constexpr size_t kEccTpmtPublicSize =
        2 + 2 + 4 + 2 + 2 + 2 + 2 + 2 + (2 + 32) + (2 + 32);
    std::vector<uint8_t> tpmt_public(kEccTpmtPublicSize);
    base::SpanWriter<uint8_t> writer(tpmt_public);
    writer.WriteEnumBigEndian(tpm::TPM_ALG_ECC);
    writer.WriteEnumBigEndian(tpm::TPM_ALG_SHA256);
    writer.WriteU32BigEndian(kObjectAttributes);
    writer.WriteU16BigEndian(0u);  // authPolicy size (empty)

    // TPMS_ECC_PARMS
    writer.WriteEnumBigEndian(tpm::TPM_ALG_NULL);  // symmetric
    writer.WriteEnumBigEndian(tpm::TPM_ALG_NULL);  // scheme
    writer.WriteEnumBigEndian(tpm::TPM_ECC_NIST_P256);
    writer.WriteEnumBigEndian(tpm::TPM_ALG_NULL);  // kdf

    // TPMS_ECC_POINT (unique)
    WriteTpm2b(writer, x);
    WriteTpm2b(writer, y);
    CHECK_EQ(writer.remaining(), 0u);
    return tpmt_public;
  }

  if (public_key.IsRsa()) {
    std::vector<uint8_t> modulus = public_key.GetRsaModulus();
    CHECK_EQ(modulus.size(), 256u);

    constexpr size_t kRsaTpmtPublicSize =
        2 + 2 + 4 + 2 + 2 + 2 + 2 + 4 + (2 + 256);
    std::vector<uint8_t> tpmt_public(kRsaTpmtPublicSize);
    base::SpanWriter<uint8_t> writer(tpmt_public);
    writer.WriteEnumBigEndian(tpm::TPM_ALG_RSA);
    writer.WriteEnumBigEndian(tpm::TPM_ALG_SHA256);
    writer.WriteU32BigEndian(kObjectAttributes);
    writer.WriteU16BigEndian(0u);  // authPolicy size (empty)

    // TPMS_RSA_PARMS
    writer.WriteEnumBigEndian(tpm::TPM_ALG_NULL);  // symmetric
    writer.WriteEnumBigEndian(tpm::TPM_ALG_NULL);  // scheme
    writer.WriteU16BigEndian(2048u);               // keyBits
    writer.WriteU32BigEndian(0u);                  // exponent (default 65537)

    // TPM2B_PUBLIC_KEY_RSA (unique)
    WriteTpm2b(writer, modulus);
    CHECK_EQ(writer.remaining(), 0u);
    return tpmt_public;
  }

  NOTREACHED();
}

// Converts a DER-encoded ECDSA signature to a TPM-compatible signature
// (TPMT_SIGNATURE) format for P-256 keys.
// See TPMT_SIGNATURE specification in TCG TPM 2.0 Library Specification,
// Part 2: Structures
// (https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=187).
std::vector<uint8_t> CreateTpmEcdsaSignature(
    const keypair::PrivateKey& key,
    base::span<const uint8_t> der_signature) {
  // For P-256, R and S are 32 bytes each.
  static constexpr size_t kPrimeSize = 32;

  std::optional<std::vector<uint8_t>> raw_sig = ConvertEcdsaDerSignatureToRaw(
      keypair::PublicKey::FromPrivateKey(key), der_signature);
  CHECK(raw_sig.has_value());
  CHECK_EQ(raw_sig->size(), kPrimeSize * 2);

  base::span<const uint8_t, kPrimeSize * 2> sig_span(*raw_sig);
  auto [r_bytes, s_bytes] = sig_span.split_at<kPrimeSize>();

  constexpr size_t kEcdsaTpmSigSize = 2 + 2 + 2 * (2 + kPrimeSize);

  std::vector<uint8_t> signature(kEcdsaTpmSigSize);
  base::SpanWriter<uint8_t> sig_writer(signature);
  sig_writer.WriteEnumBigEndian(tpm::TPM_ALG_ECDSA);
  sig_writer.WriteEnumBigEndian(tpm::TPM_ALG_SHA256);

  WriteTpm2b(sig_writer, r_bytes);
  WriteTpm2b(sig_writer, s_bytes);
  CHECK_EQ(sig_writer.remaining(), 0u);
  return signature;
}

// Formats a DER-encoded RSA signature into a TPM-compatible signature
// (TPMT_SIGNATURE) format.
// See TPMT_SIGNATURE specification in TCG TPM 2.0 Library Specification,
// Part 2: Structures
// (https://trustedcomputinggroup.org/wp-content/uploads/Trusted-Platform-Module-2.0-Library-Part-2-Structures_Version-185_pub.pdf#page=187).
std::vector<uint8_t> CreateTpmRsaSignature(
    base::span<const uint8_t> der_signature) {
  // For RSA-2048, the signature size is always 256 bytes.
  constexpr size_t kRsa2048SigSize = 256;
  CHECK_EQ(der_signature.size(), kRsa2048SigSize);
  std::vector<uint8_t> signature(2 + 2 + 2 + kRsa2048SigSize);
  base::SpanWriter<uint8_t> sig_writer(signature);
  sig_writer.WriteEnumBigEndian(tpm::TPM_ALG_RSASSA);
  sig_writer.WriteEnumBigEndian(tpm::TPM_ALG_SHA256);
  WriteTpm2b(sig_writer, der_signature);
  CHECK_EQ(sig_writer.remaining(), 0u);
  return signature;
}

template <typename BaseInterface>
class SoftwareKeyImpl : public BaseInterface {
 public:
  explicit SoftwareKeyImpl(crypto::keypair::PrivateKey key)
      : key_(std::move(key)) {}

  sign::SignatureKind Algorithm() const override { return GetSignatureKind(); }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return key_.ToSubjectPublicKeyInfo();
  }

  std::vector<uint8_t> GetWrappedKey() const override {
    switch (GetSignatureKind()) {
      case sign::RSA_PKCS1_SHA256:
        return key_.ToRSAPrivateKey();
      case sign::ECDSA_SHA256:
        return key_.ToEcP256PrivateKey();
      case sign::RSA_PKCS1_SHA1:
      case sign::RSA_PKCS1_SHA384:
      case sign::RSA_PKCS1_SHA512:
      case sign::RSA_PSS_SHA256:
      case sign::RSA_PSS_SHA384:
      case sign::RSA_PSS_SHA512:
      case sign::ECDSA_SHA1:
      case sign::ECDSA_SHA384:
      case sign::ECDSA_SHA512:
      case sign::ED25519:
      case sign::MLDSA_44:
      case sign::MLDSA_65:
      case sign::MLDSA_87:
        NOTREACHED();
    }
  }

#if BUILDFLAG(IS_APPLE)
  SecKeyRef GetSecKeyRef() const override { NOTREACHED(); }
#elif BUILDFLAG(IS_WIN)
  NCRYPT_KEY_HANDLE GetNCryptKeyHandle() const override { NOTREACHED(); }
#endif

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    return sign::Sign(GetSignatureKind(), key(), data);
  }

#if BUILDFLAG(IS_WIN)
  bool SupportsTls13() override { return true; }
#endif  // BUILDFLAG(IS_WIN)

 protected:
  const crypto::keypair::PrivateKey& key() const { return key_; }

  sign::SignatureKind GetSignatureKind() const {
    if (key_.IsRsa()) {
      return sign::RSA_PKCS1_SHA256;
    }
    if (key_.IsEcP256()) {
      return sign::ECDSA_SHA256;
    }
    NOTREACHED();
  }

 private:
  crypto::keypair::PrivateKey key_;
};

class SoftwareSigningKey : public SoftwareKeyImpl<UnexportableSigningKey> {
 public:
  using Base = SoftwareKeyImpl<UnexportableSigningKey>;

  explicit SoftwareSigningKey(crypto::keypair::PrivateKey key)
      : Base(std::move(key)) {}
};

class SoftwareAttestationKey
    : public SoftwareKeyImpl<UnexportableAttestationKey> {
 public:
  using Base = SoftwareKeyImpl<UnexportableAttestationKey>;

  explicit SoftwareAttestationKey(crypto::keypair::PrivateKey key)
      : Base(std::move(key)) {}

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    // Emulate TPM 2.0 restricted signing key behavior: hardware TPMs refuse to
    // sign external data starting with `TPM_GENERATED_VALUE` (0xFF544347) via
    // TPM2_Hash/TPM2_Sign to prevent forging TPM-generated attestation
    // structures (e.g., TPMS_ATTEST).
    return std::ranges::starts_with(
               data, base::EnumToBigEndian(tpm::TPM_GENERATED_VALUE))
               ? std::nullopt
               : Base::SignSlowly(data);
  }

  // Certifies the signing key by generating a fake TPM 2.0 certification
  // output. This emulates the behavior of a TPM-backed key provider for
  // testing.
  //
  // The returned AttestationStatement contains a TPMS_ATTEST and TPMT_SIGNATURE
  // structure.
  //
  // See https://github.com/WICG/dbsc-sso for details.
  std::optional<AttestationStatement> CertifySlowly(
      const UnexportableSigningKey& signing_key,
      base::span<const uint8_t> challenge) override {
    ASSIGN_OR_RETURN(auto public_key,
                     keypair::PublicKey::FromSubjectPublicKeyInfo(
                         signing_key.GetSubjectPublicKeyInfo()));

    std::vector<uint8_t> tpmt_public = CreateTpmtPublic(public_key);
    std::vector<uint8_t> attestation_statement =
        CreateTpm2bAttestationStatement(tpmt_public, challenge);

    const std::vector<uint8_t> der_signature =
        sign::Sign(GetSignatureKind(), key(), attestation_statement);

    std::vector<uint8_t> tpm_signature = [&]() {
      switch (GetSignatureKind()) {
        case sign::ECDSA_SHA256:
          return CreateTpmEcdsaSignature(key(), der_signature);
        case sign::RSA_PKCS1_SHA256:
          return CreateTpmRsaSignature(der_signature);
        default:
          NOTREACHED();
      }
    }();

    return AttestationStatement{
        .format = AttestationStatement::kTpm,
        .statement = std::move(attestation_statement),
        .signature = std::move(tpm_signature),
        .subject_key = std::move(tpmt_public),
    };
  }
};

class SoftwareProvider : public UnexportableKeyProvider {
 public:
  ~SoftwareProvider() override = default;

  std::optional<sign::SignatureKind> SelectAlgorithm(
      base::span<const sign::SignatureKind> acceptable_algorithms) override {
    for (auto algo : acceptable_algorithms) {
      switch (algo) {
        case sign::ECDSA_SHA256:
        case sign::RSA_PKCS1_SHA256:
          return algo;
        case sign::RSA_PKCS1_SHA1:
        case sign::RSA_PKCS1_SHA384:
        case sign::RSA_PKCS1_SHA512:
        case sign::RSA_PSS_SHA256:
        case sign::RSA_PSS_SHA384:
        case sign::RSA_PSS_SHA512:
        case sign::ECDSA_SHA1:
        case sign::ECDSA_SHA384:
        case sign::ECDSA_SHA512:
        case sign::ED25519:
        case sign::MLDSA_44:
        case sign::MLDSA_65:
        case sign::MLDSA_87:
          continue;  // Not supported
      }
    }

    return std::nullopt;
  }

  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const sign::SignatureKind> acceptable_algorithms) override {
    if (!SelectAlgorithm(acceptable_algorithms)) {
      return nullptr;
    }

    for (auto algo : acceptable_algorithms) {
      switch (algo) {
        case sign::ECDSA_SHA256: {
          return std::make_unique<SoftwareSigningKey>(
              crypto::keypair::PrivateKey::GenerateEcP256());
        }

        case sign::RSA_PKCS1_SHA256: {
          return std::make_unique<SoftwareSigningKey>(
              crypto::keypair::PrivateKey::GenerateRsa2048());
        }
        case sign::RSA_PKCS1_SHA1:
        case sign::RSA_PKCS1_SHA384:
        case sign::RSA_PKCS1_SHA512:
        case sign::RSA_PSS_SHA256:
        case sign::RSA_PSS_SHA384:
        case sign::RSA_PSS_SHA512:
        case sign::ECDSA_SHA1:
        case sign::ECDSA_SHA384:
        case sign::ECDSA_SHA512:
        case sign::ED25519:
        case sign::MLDSA_44:
        case sign::MLDSA_65:
        case sign::MLDSA_87:
          continue;  // Not supported
      }
    }

    return nullptr;
  }

  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    if (auto key =
            crypto::keypair::PrivateKey::FromEcP256PrivateKey(wrapped_key)) {
      return std::make_unique<SoftwareSigningKey>(std::move(*key));
    }

    if (auto key =
            crypto::keypair::PrivateKey::FromRSAPrivateKey(wrapped_key)) {
      return std::make_unique<SoftwareSigningKey>(std::move(*key));
    }

    return nullptr;
  }

  std::unique_ptr<UnexportableAttestationKey> GenerateAttestationKeySlowly(
      base::span<const sign::SignatureKind> acceptable_algorithms) override {
    if (!SelectAlgorithm(acceptable_algorithms)) {
      return nullptr;
    }

    for (auto algo : acceptable_algorithms) {
      switch (algo) {
        case sign::ECDSA_SHA256: {
          return std::make_unique<SoftwareAttestationKey>(
              crypto::keypair::PrivateKey::GenerateEcP256());
        }

        case sign::RSA_PKCS1_SHA256: {
          return std::make_unique<SoftwareAttestationKey>(
              crypto::keypair::PrivateKey::GenerateRsa2048());
        }
        case sign::RSA_PKCS1_SHA1:
        case sign::RSA_PKCS1_SHA384:
        case sign::RSA_PKCS1_SHA512:
        case sign::RSA_PSS_SHA256:
        case sign::RSA_PSS_SHA384:
        case sign::RSA_PSS_SHA512:
        case sign::ECDSA_SHA1:
        case sign::ECDSA_SHA384:
        case sign::ECDSA_SHA512:
        case sign::ED25519:
        case sign::MLDSA_44:
        case sign::MLDSA_65:
        case sign::MLDSA_87:
          continue;  // Not supported
      }
    }

    return nullptr;
  }

  std::unique_ptr<UnexportableAttestationKey> FromWrappedAttestationKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    if (auto key =
            crypto::keypair::PrivateKey::FromEcP256PrivateKey(wrapped_key)) {
      return std::make_unique<SoftwareAttestationKey>(std::move(*key));
    }

    if (auto key =
            crypto::keypair::PrivateKey::FromRSAPrivateKey(wrapped_key)) {
      return std::make_unique<SoftwareAttestationKey>(std::move(*key));
    }

    return nullptr;
  }

  StatefulUnexportableKeyProvider* AsStatefulUnexportableKeyProvider()
      override {
    // Unexportable software keys are stateless.
    return nullptr;
  }
};

}  // namespace

std::unique_ptr<UnexportableKeyProvider>
GetSoftwareUnsecureUnexportableKeyProvider() {
  return std::make_unique<SoftwareProvider>();
}

}  // namespace crypto
