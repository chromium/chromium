// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span_writer.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
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
    const UnexportableSigningKey& signing_key,
    base::span<const uint8_t> challenge) {
  static constexpr uint32_t kTpmGeneratedValue = 0xFF544347;
  static constexpr uint16_t kTpmStAttestCertify = 0x8017;
  // TPM_ALG_SHA256 + hash
  static constexpr size_t kNameBufSize = 2 + hash::kSha256Size;

  // TPMS_ATTEST structure size without the extraData (challenge) payload:
  // - magic: 4 bytes (TPM_GENERATED)
  // - type: 2 bytes (TPMI_ST_ATTEST)
  // - qualifiedSigner: 2 bytes (TPM2B_NAME header, empty name)
  // - extraData header: 2 bytes (TPM2B_DATA header)
  // - clockInfo: 17 bytes (TPMS_CLOCK_INFO)
  // - firmwareVersion: 8 bytes (uint64_t)
  // - attested (TPMS_CERTIFY_INFO):
  //   - name: 36 bytes (TPM2B_NAME with SHA-256 algorithm ID + 32-byte digest)
  //   - qualifiedName: 2 bytes (TPM2B_NAME header, empty name)
  static constexpr size_t kAttestationStatementFixedSize =
      4 + 2 + 2 + 2 + 17 + 8 + (2 + kNameBufSize) + 2;

  std::vector<uint8_t> attestation_statement(kAttestationStatementFixedSize +
                                             challenge.size());
  base::SpanWriter<uint8_t> attest_writer(attestation_statement);
  attest_writer.WriteU32BigEndian(kTpmGeneratedValue);
  attest_writer.WriteU16BigEndian(kTpmStAttestCertify);
  // qualifiedSigner (empty)
  attest_writer.WriteU16BigEndian(0);

  // extraData
  WriteTpm2b(attest_writer, challenge);

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
  name_writer.WriteU16BigEndian(0x000B);  // TPM_ALG_SHA256
  name_writer.Write(hash::Sha256(signing_key.GetSubjectPublicKeyInfo()));
  CHECK_EQ(name_writer.remaining(), 0u);

  WriteTpm2b(attest_writer, name_buf);

  // qualifiedName: TPM2B_NAME (empty)
  attest_writer.WriteU16BigEndian(0);

  CHECK_EQ(attest_writer.remaining(), 0u);
  return attestation_statement;
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
  sig_writer.WriteU16BigEndian(0x0018);  // TPM_ALG_ECDSA
  sig_writer.WriteU16BigEndian(0x000B);  // TPM_ALG_SHA256

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
  sig_writer.WriteU16BigEndian(0x0014);  // TPM_ALG_RSASSA
  sig_writer.WriteU16BigEndian(0x000B);  // TPM_ALG_SHA256
  WriteTpm2b(sig_writer, der_signature);
  CHECK_EQ(sig_writer.remaining(), 0u);
  return signature;
}

template <typename BaseInterface>
class SoftwareKeyImpl : public BaseInterface {
 public:
  explicit SoftwareKeyImpl(crypto::keypair::PrivateKey key)
      : key_(std::move(key)) {}

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    switch (GetSignatureKind()) {
      case sign::RSA_PKCS1_SHA256:
        return SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
      case sign::ECDSA_SHA256:
        return SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
      default:
        NOTREACHED();
    }
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return key_.ToSubjectPublicKeyInfo();
  }

  std::vector<uint8_t> GetWrappedKey() const override {
    switch (GetSignatureKind()) {
      case sign::RSA_PKCS1_SHA256:
        return key_.ToRSAPrivateKey();
      case sign::ECDSA_SHA256:
        return key_.ToEcP256PrivateKey();
      default:
        NOTREACHED();
    }
  }

#if BUILDFLAG(IS_APPLE)
  SecKeyRef GetSecKeyRef() const override { NOTREACHED(); }
#elif BUILDFLAG(IS_WIN)
  NCRYPT_KEY_HANDLE GetNCryptKeyHandle() const override { NOTREACHED(); }
#endif

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
  explicit SoftwareSigningKey(crypto::keypair::PrivateKey key)
      : SoftwareKeyImpl<UnexportableSigningKey>(std::move(key)) {}

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    return sign::Sign(GetSignatureKind(), key(), data);
  }

#if BUILDFLAG(IS_WIN)
  bool SupportsTls13() override { return true; }
#endif  // BUILDFLAG(IS_WIN)
};

class SoftwareAttestationKey
    : public SoftwareKeyImpl<UnexportableAttestationKey> {
 public:
  explicit SoftwareAttestationKey(crypto::keypair::PrivateKey key)
      : SoftwareKeyImpl<UnexportableAttestationKey>(std::move(key)) {}

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
    std::vector<uint8_t> attestation_statement =
        CreateTpm2bAttestationStatement(signing_key, challenge);

    const std::vector<uint8_t> der_signature =
        sign::Sign(GetSignatureKind(), key(), attestation_statement);

    switch (GetSignatureKind()) {
      case sign::ECDSA_SHA256:
        return AttestationStatement{
            .format = AttestationStatement::kTpm,
            .statement = std::move(attestation_statement),
            .signature = CreateTpmEcdsaSignature(key(), der_signature),
        };
      case sign::RSA_PKCS1_SHA256:
        return AttestationStatement{
            .format = AttestationStatement::kTpm,
            .statement = std::move(attestation_statement),
            .signature = CreateTpmRsaSignature(der_signature),
        };
      default:
        NOTREACHED();
    }
  }
};

class SoftwareProvider : public UnexportableKeyProvider {
 public:
  ~SoftwareProvider() override = default;

  std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    for (auto algo : acceptable_algorithms) {
      switch (algo) {
        case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
          return algo;
        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
        case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
          continue;  // Not supported
      }
    }

    return std::nullopt;
  }

  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    if (!SelectAlgorithm(acceptable_algorithms)) {
      return nullptr;
    }

    for (auto algo : acceptable_algorithms) {
      switch (algo) {
        case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256: {
          return std::make_unique<SoftwareSigningKey>(
              crypto::keypair::PrivateKey::GenerateEcP256());
        }

        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256: {
          return std::make_unique<SoftwareSigningKey>(
              crypto::keypair::PrivateKey::GenerateRsa2048());
        }
        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
        case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
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
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    if (!SelectAlgorithm(acceptable_algorithms)) {
      return nullptr;
    }

    for (auto algo : acceptable_algorithms) {
      switch (algo) {
        case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256: {
          return std::make_unique<SoftwareAttestationKey>(
              crypto::keypair::PrivateKey::GenerateEcP256());
        }

        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256: {
          return std::make_unique<SoftwareAttestationKey>(
              crypto::keypair::PrivateKey::GenerateRsa2048());
        }
        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
        case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
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
