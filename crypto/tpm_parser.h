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
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/types/expected.h"
#include "crypto/crypto_export.h"
#include "crypto/hash.h"
#include "crypto/sign.h"
#include "crypto/tpm.rs.h"

namespace crypto::tpm {

using enum TpmAlgHash;
using enum TpmAlgPublic;
using enum TpmAlgSigScheme;
using enum TpmCc;
using enum TpmConstant;
using enum TpmRh;
using enum TpmSt;

// LINT.IfChange(TpmCommand)
// Enumerates the TPM 2.0 commands implemented by this module.
enum class TpmCommand {
  kCertify,            // TPM2_Certify
  kCreate,             // TPM2_Create
  kFlushContext,       // TPM2_FlushContext
  kHash,               // TPM2_Hash
  kHashSequenceStart,  // TPM2_HashSequenceStart
  kSequenceComplete,   // TPM2_SequenceComplete
  kSequenceUpdate,     // TPM2_SequenceUpdate
  kSign,               // TPM2_Sign
};

template <typename Sink>
void AbslStringify(Sink& sink, TpmCommand command) {
  switch (command) {
    case TpmCommand::kCertify:
      sink.Append("Certify");
      return;
    case TpmCommand::kCreate:
      sink.Append("Create");
      return;
    case TpmCommand::kFlushContext:
      sink.Append("FlushContext");
      return;
    case TpmCommand::kHash:
      sink.Append("Hash");
      return;
    case TpmCommand::kHashSequenceStart:
      sink.Append("HashSequenceStart");
      return;
    case TpmCommand::kSequenceComplete:
      sink.Append("SequenceComplete");
      return;
    case TpmCommand::kSequenceUpdate:
      sink.Append("SequenceUpdate");
      return;
    case TpmCommand::kSign:
      sink.Append("Sign");
      return;
  }

  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/histograms.xml:TpmCommand)

// Various errors returned during TPM response parsing.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// NOTE: Since the response parsing happens completely on the Rust side,
// this is a strict subset of the enum defined in tpm.rs. We purposefully drop
// the kOk option, so that it's a true error enum.
struct CRYPTO_EXPORT TpmParseError {
  // LINT.IfChange(TpmParseResult)
  enum class Type : uint8_t {
    kBufferTooSmall = 1,
    kTrailingBytes = 2,
    kTpmErrorResponse = 3,
    kBadMagicNumber = 4,
    kWrongType = 5,
    kChallengeMismatch = 6,
    kMaxValue = kChallengeMismatch
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:TpmParseResult)

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
  static constexpr auto kCommand = TpmCommand::kCertify;

  std::vector<uint8_t> statement;
  std::vector<uint8_t> signature;

  friend bool operator==(const CertifyResponse&,
                         const CertifyResponse&) = default;
};

// Response components extracted from a parsed TPM2_Create response.
struct CRYPTO_EXPORT CreateResponse {
  static constexpr auto kCommand = TpmCommand::kCreate;

  // The serialized TPM2B_PRIVATE structure returned by the TPM.
  std::vector<uint8_t> out_private;
  // The serialized TPM2B_PUBLIC structure returned by the TPM.
  std::vector<uint8_t> out_public;

  friend bool operator==(const CreateResponse&,
                         const CreateResponse&) = default;
};

// Response from parsing a TPM2_FlushContext response.
struct CRYPTO_EXPORT FlushContextResponse {
  static constexpr auto kCommand = TpmCommand::kFlushContext;

  friend bool operator==(const FlushContextResponse&,
                         const FlushContextResponse&) = default;
};

// Response components extracted from a parsed TPM2_Hash response.
struct CRYPTO_EXPORT HashResponse {
  static constexpr auto kCommand = TpmCommand::kHash;

  std::vector<uint8_t> digest;
  std::vector<uint8_t> validation_ticket;

  friend bool operator==(const HashResponse&, const HashResponse&) = default;
};

// Response components extracted from a parsed TPM2_HashSequenceStart response.
struct CRYPTO_EXPORT HashSequenceStartResponse {
  static constexpr auto kCommand = TpmCommand::kHashSequenceStart;

  uint32_t sequence_handle = 0;

  friend bool operator==(const HashSequenceStartResponse&,
                         const HashSequenceStartResponse&) = default;
};

// Response components extracted from a parsed TPM2_SequenceComplete response.
struct CRYPTO_EXPORT SequenceCompleteResponse {
  static constexpr auto kCommand = TpmCommand::kSequenceComplete;

  std::vector<uint8_t> digest;
  std::vector<uint8_t> validation_ticket;

  friend bool operator==(const SequenceCompleteResponse&,
                         const SequenceCompleteResponse&) = default;
};

// Response from parsing a TPM2_SequenceUpdate response.
struct CRYPTO_EXPORT SequenceUpdateResponse {
  static constexpr auto kCommand = TpmCommand::kSequenceUpdate;

  friend bool operator==(const SequenceUpdateResponse&,
                         const SequenceUpdateResponse&) = default;
};

// Response components extracted from a parsed TPM2_Sign response.
struct CRYPTO_EXPORT SignResponse {
  static constexpr auto kCommand = TpmCommand::kSign;

  std::vector<uint8_t> signature;

  friend bool operator==(const SignResponse&, const SignResponse&) = default;
};

// TPM algorithm IDs for a given SignatureAlgorithm.
struct CRYPTO_EXPORT SignatureAlgorithms {
  TpmAlgSigScheme sig_alg = TPM_ALG_NULL;
  TpmAlgHash hash_alg = TPM_ALG_SHA256;

  friend bool operator==(const SignatureAlgorithms&,
                         const SignatureAlgorithms&) = default;
};

// Builds a serialized TPM2_Certify command buffer.
//
// TPM2_Certify takes a `TPM2B_DATA qualifyingData` parameter to ensure
// freshness and prevent replay attacks (which for key attestation protocols is
// typically the SHA-256 digest of the challenge).
//
// * `object_handle` - The TPM handle of the key to be certified.
// * `sign_handle` - The TPM handle of the attestation key used to sign the
// certification.
// * `qualifying_data` - Data provided by the caller to ensure freshness (e.g.,
// the SHA-256 digest of the challenge).
CRYPTO_EXPORT std::vector<uint8_t> BuildCertifyCommand(
    uint32_t object_handle,
    uint32_t sign_handle,
    base::span<const uint8_t> qualifying_data);

// Parses a serialized TPM2_Certify response and extracts the certified
// statement and signature.
//
// TPM2_Certify operates on `TPM2B_DATA qualifyingData` (which for key
// attestation protocols is typically the SHA-256 digest of the challenge),
// returned in the `extraData` field of the `TPMS_ATTEST` structure.
//
// * `response_blob` - The raw byte response from the TPM2_Certify command.
// * `expected_extra_data` - The extra data expected in the attestation's
// `extraData` field (e.g., the SHA-256 digest of the challenge) to prevent
// replay attacks.
//
// If the TPM returns an error code, an error of type `kTpmErrorResponse` will
// be returned containing the error code, and no statement or signature will be
// extracted.
CRYPTO_EXPORT TpmParseErrorOr<CertifyResponse> ParseCertifyResponse(
    base::span<const uint8_t> response_blob,
    base::span<const uint8_t> expected_extra_data);

// Builds a serialized TPM2_Create command buffer for an Attestation Identity
// Key (AIK) configured according to the provided `kind` under `parent_handle`.
//
// Returns nullopt if `kind` is not supported for AIK creation.
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> BuildCreateAikCommand(
    uint32_t parent_handle,
    sign::SignatureKind kind);

// Parses a serialized TPM2_Create response and extracts the private area and
// public area.
//
// If the TPM returns an error code, an error of type `kTpmErrorResponse` will
// be returned containing the error code.
CRYPTO_EXPORT TpmParseErrorOr<CreateResponse> ParseCreateResponse(
    base::span<const uint8_t> response_blob);

// Builds a serialized TPM2_FlushContext command buffer.
//
// * `handle` - The handle of the item to flush.
CRYPTO_EXPORT std::vector<uint8_t> BuildFlushContextCommand(uint32_t handle);

// Parses a serialized TPM2_FlushContext response.
CRYPTO_EXPORT TpmParseErrorOr<FlushContextResponse> ParseFlushContextResponse(
    base::span<const uint8_t> response_blob);

// Builds a serialized TPM2_Hash command buffer.
//
// * `data` - The byte buffer to be hashed.
// * `hash_kind` - The hash algorithm to use.
CRYPTO_EXPORT std::vector<uint8_t> BuildHashCommand(
    base::span<const uint8_t> data,
    hash::HashKind hash_kind);

// Parses a serialized TPM2_Hash response.
//
// If the TPM returns an error code, an error of type `kTpmErrorResponse` will
// be returned containing the error code, and no digest or validation ticket
// will be extracted.
CRYPTO_EXPORT TpmParseErrorOr<HashResponse> ParseHashResponse(
    base::span<const uint8_t> response_blob);

// Builds a serialized TPM2_HashSequenceStart command buffer.
//
// * `hash_kind` - The hash algorithm to use for the sequence.
CRYPTO_EXPORT std::vector<uint8_t> BuildHashSequenceStartCommand(
    hash::HashKind hash_kind);

// Parses a serialized TPM2_HashSequenceStart response.
//
// If the TPM returns an error code, an error of type `kTpmErrorResponse` will
// be returned containing the error code, and no sequence handle will be
// extracted.
CRYPTO_EXPORT TpmParseErrorOr<HashSequenceStartResponse>
ParseHashSequenceStartResponse(base::span<const uint8_t> response_blob);

// Builds a serialized TPM2_SequenceComplete command buffer.
//
// * `sequence_handle` - The handle of the sequence to complete.
// * `data` - The final byte buffer to append to the hash sequence.
CRYPTO_EXPORT std::vector<uint8_t> BuildSequenceCompleteCommand(
    uint32_t sequence_handle,
    base::span<const uint8_t> data);

// Parses a serialized TPM2_SequenceComplete response.
//
// If the TPM returns an error code, an error of type `kTpmErrorResponse` will
// be returned containing the error code, and no digest or validation ticket
// will be extracted.
CRYPTO_EXPORT TpmParseErrorOr<SequenceCompleteResponse>
ParseSequenceCompleteResponse(base::span<const uint8_t> response_blob);

// Builds a serialized TPM2_SequenceUpdate command buffer.
//
// * `sequence_handle` - The handle of the sequence to be updated.
// * `data` - The byte buffer to append to the hash sequence.
CRYPTO_EXPORT std::vector<uint8_t> BuildSequenceUpdateCommand(
    uint32_t sequence_handle,
    base::span<const uint8_t> data);

// Parses a serialized TPM2_SequenceUpdate response.
CRYPTO_EXPORT TpmParseErrorOr<SequenceUpdateResponse>
ParseSequenceUpdateResponse(base::span<const uint8_t> response_blob);

// Builds a serialized TPM2_Sign command buffer.
//
// Uses TPM_ALG_NULL for the signing scheme so that the TPM auto-infers
// the scheme configured on `key_handle`.
//
// * `key_handle` - The handle of the signing key.
// * `digest` - The digest to sign.
// * `validation_ticket` - The validation ticket from TPM2_Hash or
//   TPM2_SequenceComplete.
CRYPTO_EXPORT std::vector<uint8_t> BuildSignCommand(
    uint32_t key_handle,
    base::span<const uint8_t> digest,
    base::span<const uint8_t> validation_ticket);

// Parses a serialized TPM2_Sign response.
CRYPTO_EXPORT TpmParseErrorOr<SignResponse> ParseSignResponse(
    base::span<const uint8_t> response_blob);

// Parses a serialized `TPMT_SIGNATURE` and returns the normalized signature
// (DER-encoded for ECDSA, raw bytes for RSA).
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> ParseTpmSignature(
    base::span<const uint8_t> signature_blob);

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
