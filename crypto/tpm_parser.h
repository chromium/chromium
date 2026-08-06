// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes C++ bindings for the low-level TPM operations implemented
// in Rust (see //crypto/tpm.rs). C++ client code should use the APIs defined
// in this file rather than calling the auto-generated Rust bindings directly.

#ifndef CRYPTO_TPM_PARSER_H_
#define CRYPTO_TPM_PARSER_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/types/expected.h"
#include "crypto/crypto_export.h"

namespace crypto::tpm {

// Various errors returned during TPM response parsing.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// NOTE: Since the response parsing happens completely on the Rust side,
// this is a strict subset of the enum defined in tpm.rs. We purposefully drop
// the kOk option, so that it's a true error enum.
struct CRYPTO_EXPORT TpmParseError {
  // LINT.IfChange(TpmCertifyParseResult)
  enum class Type : uint8_t {
    kBufferTooSmall = 1,
    kTrailingBytes = 2,
    kTpmErrorResponse = 3,
    kBadMagicNumber = 4,
    kWrongType = 5,
    kChallengeMismatch = 6,
    kMaxValue = kChallengeMismatch
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:TpmCertifyParseResult)

  const Type type = Type::kBufferTooSmall;
  // Only populated if `type` is `Type::kTpmErrorResponse`.
  const std::optional<uint32_t> tpm_error_code;

  explicit TpmParseError(Type type,
                         std::optional<uint32_t> tpm_error_code = std::nullopt)
      : type(type), tpm_error_code(tpm_error_code) {
    CHECK_EQ(type == Type::kTpmErrorResponse, tpm_error_code.has_value());
  }

  friend bool operator==(const TpmParseError&, const TpmParseError&) = default;
};

template <typename T>
using TpmParseErrorOr = base::expected<T, TpmParseError>;

inline constexpr auto kNoTpmParseErrorForMetrics =
    static_cast<TpmParseError::Type>(0);

// Various errors returned during TPM signature verification.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// NOTE: While signature parsing happens in Rust, signature verification is
// implemented in C++. This means this enum extends the Rust version with
// possible verification errors, but also drops the kOk option to make it a true
// error enum.
//
enum class SignatureError : uint8_t {
  kBufferTooSmall = 1,
  kTrailingBytes = 2,
  kUnsupportedSignatureAlgorithm = 3,
  kUnsupportedHashAlgorithm = 4,
  kInvalidPublicKey = 5,
  kInvalidSignature = 6,
  kMaxValue = kInvalidSignature
};

template <typename T>
using SignatureErrorOr = base::expected<T, SignatureError>;

inline constexpr auto kNoSignatureErrorForMetrics =
    static_cast<SignatureError>(0);

// Response components extracted from a parsed TPM2_Certify response.
struct CRYPTO_EXPORT CertifyResponse {
  std::vector<uint8_t> statement;
  std::vector<uint8_t> signature;

  friend bool operator==(const CertifyResponse&,
                         const CertifyResponse&) = default;
};

// Response components extracted from a parsed TPM2_Hash response.
struct CRYPTO_EXPORT HashResponse {
  std::vector<uint8_t> digest;
  std::vector<uint8_t> validation_ticket;

  friend bool operator==(const HashResponse&, const HashResponse&) = default;
};

// Response components extracted from a parsed TPM2_Sign response.
struct CRYPTO_EXPORT SignResponse {
  std::vector<uint8_t> signature;

  friend bool operator==(const SignResponse&, const SignResponse&) = default;
};

// TPM algorithm IDs returned by the parser, solely for telemetry.
struct CRYPTO_EXPORT SignatureAlgorithms {
  uint16_t sig_alg = 0;
  uint16_t hash_alg = 0;

  friend bool operator==(const SignatureAlgorithms&,
                         const SignatureAlgorithms&) = default;
};

// TPM hierarchy handle constants. See TPM 2.0 Library Part 2, Section 24.
// Used as the `hierarchy` parameter for `BuildHashCommand` and when validating
// tickets.
// kTpmRhOwner (0x40000001) is used for standard keys and mock validation
// tickets in unit tests.
inline constexpr uint32_t kTpmRhOwner = 0x40000001;
// kTpmRhEndorsement (0x4000000b) MUST be used for Windows Attestation Identity
// Keys (AIKs) in production.
inline constexpr uint32_t kTpmRhEndorsement = 0x4000000b;

// Builds a serialized TPM2_Certify command buffer.
//
// * `object_handle` - The TPM handle of the key to be certified.
// * `sign_handle` - The TPM handle of the attestation key used to sign the
// certification.
// * `challenge` - A security challenge/nonce to prevent replay attacks.
CRYPTO_EXPORT std::vector<uint8_t> BuildCertifyCommand(
    uint32_t object_handle,
    uint32_t sign_handle,
    base::span<const uint8_t> challenge);

// Parses a serialized TPM2_Certify response and extracts the certified
// statement and signature.
//
// * `response_blob` - The raw byte response from the TPM2_Certify command.
// * `challenge` - The challenge expected in the attestation's extra data to
// prevent replay.
//
// If the TPM returns an error code, an error of type `kTpmErrorResponse` will
// be returned containing the error code, and no statement or signature will be
// extracted.
CRYPTO_EXPORT TpmParseErrorOr<CertifyResponse> ParseCertifyResponse(
    base::span<const uint8_t> response_blob,
    base::span<const uint8_t> challenge);

// Builds a serialized TPM2_Hash command buffer.
//
// * `data` - The byte buffer to be hashed.
// * `hash_alg` - The TPM algorithm ID of the hash function (e.g. SHA-256).
// * `hierarchy` - The TPM hierarchy handle for the ticket (e.g. kTpmRhOwner
// for storage/test tickets, or kTpmRhEndorsement for AIKs).
CRYPTO_EXPORT std::vector<uint8_t> BuildHashCommand(
    base::span<const uint8_t> data,
    uint16_t hash_alg,
    uint32_t hierarchy);

// Parses a serialized TPM2_Hash response.
//
// If the TPM returns an error code, an error of type `kTpmErrorResponse` will
// be returned containing the error code, and no digest or validation ticket
// will be extracted.
CRYPTO_EXPORT TpmParseErrorOr<HashResponse> ParseHashResponse(
    base::span<const uint8_t> response_blob);

// Builds a serialized TPM2_Sign command buffer.
CRYPTO_EXPORT std::vector<uint8_t> BuildSignCommand(
    uint32_t key_handle,
    base::span<const uint8_t> digest,
    uint16_t sig_alg,
    uint16_t hash_alg,
    base::span<const uint8_t> validation_ticket);

// Parses a serialized TPM2_Sign response.
CRYPTO_EXPORT TpmParseErrorOr<SignResponse> ParseSignResponse(
    base::span<const uint8_t> response_blob);

// Parses a serialized `TPMT_SIGNATURE` and returns the signature and hash
// algorithms used, solely for telemetry.
CRYPTO_EXPORT SignatureErrorOr<SignatureAlgorithms> GetSignatureAlgorithms(
    base::span<const uint8_t> signature_blob);

// Verifies a TPM signature over the given statement using the provided public
// key.
//
// * `spki` - The Subject Public Key Info of the certifying key.
// * `statement` - The attestation statement bytes to verify.
// * `signature_blob` - The serialized TPMT_SIGNATURE bytes returned by the TPM.
CRYPTO_EXPORT SignatureErrorOr<void> VerifySignature(
    base::span<const uint8_t> spki,
    base::span<const uint8_t> statement,
    base::span<const uint8_t> signature_blob);

}  // namespace crypto::tpm

#endif  // CRYPTO_TPM_PARSER_H_
