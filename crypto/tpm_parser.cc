// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/tpm_parser.h"

#include <stdint.h>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_rust.h"
#include "base/containers/to_vector.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "crypto/tpm.rs.h"

namespace crypto::tpm {

namespace {

// Asymmetric signature algorithm families supported by the TPM parser.
enum class Algorithm {
  kRsaSsa,
  kEcdsa,
};

SignatureErrorOr<void> MapSignatureParseResult(SignatureParseResult result) {
  switch (result) {
    case SignatureParseResult::Ok:
      return base::ok();
    case SignatureParseResult::BufferTooSmall:
      return base::unexpected(SignatureError::kBufferTooSmall);
    case SignatureParseResult::TrailingBytes:
      return base::unexpected(SignatureError::kTrailingBytes);
    case SignatureParseResult::UnsupportedSignatureAlgorithm:
      return base::unexpected(SignatureError::kUnsupportedSignatureAlgorithm);
  }
  NOTREACHED();
}

TpmParseErrorOr<void> MapParseResult(ParseResult result,
                                     uint32_t tpm_response_code) {
  switch (result) {
    case ParseResult::Ok:
      return base::ok();
    case ParseResult::BufferTooSmall:
      return base::unexpected(
          TpmParseError(TpmParseError::Type::kBufferTooSmall));
    case ParseResult::TrailingBytes:
      return base::unexpected(
          TpmParseError(TpmParseError::Type::kTrailingBytes));
    case ParseResult::TpmErrorResponse:
      return base::unexpected(TpmParseError(
          TpmParseError::Type::kTpmErrorResponse, tpm_response_code));
    case ParseResult::BadMagicNumber:
      return base::unexpected(
          TpmParseError(TpmParseError::Type::kBadMagicNumber));
    case ParseResult::WrongType:
      return base::unexpected(TpmParseError(TpmParseError::Type::kWrongType));
    case ParseResult::ChallengeMismatch:
      return base::unexpected(
          TpmParseError(TpmParseError::Type::kChallengeMismatch));
  }
  NOTREACHED();
}

SignatureErrorOr<hash::HashKind> MapHashAlgorithm(TpmAlg hash_alg) {
  switch (hash_alg) {
    case TpmAlg::TPM_ALG_SHA256:
      return hash::kSha256;
    case TpmAlg::TPM_ALG_SHA1:
      return hash::kSha1;
    default:
      return base::unexpected(SignatureError::kUnsupportedHashAlgorithm);
  }
}

sign::SignatureKind ToSignatureKind(Algorithm alg, hash::HashKind hash) {
  switch (alg) {
    case Algorithm::kRsaSsa:
      switch (hash) {
        case hash::kSha1:
          return sign::SignatureKind::RSA_PKCS1_SHA1;
        case hash::kSha256:
          return sign::SignatureKind::RSA_PKCS1_SHA256;
        case hash::kSha384:
          return sign::SignatureKind::RSA_PKCS1_SHA384;
        case hash::kSha512:
          return sign::SignatureKind::RSA_PKCS1_SHA512;
      }
    case Algorithm::kEcdsa:
      switch (hash) {
        case hash::kSha1:
          return sign::SignatureKind::ECDSA_SHA1;
        case hash::kSha256:
          return sign::SignatureKind::ECDSA_SHA256;
        case hash::kSha384:
          return sign::SignatureKind::ECDSA_SHA384;
        case hash::kSha512:
          return sign::SignatureKind::ECDSA_SHA512;
      }
  }
  NOTREACHED();
}

SignatureErrorOr<void> VerifyRsaSignature(const keypair::PublicKey& public_key,
                                          base::span<const uint8_t> statement,
                                          hash::HashKind hash_kind,
                                          base::span<const uint8_t> rsa_sig) {
  bool verified = sign::Verify(ToSignatureKind(Algorithm::kRsaSsa, hash_kind),
                               public_key, statement, rsa_sig);
  if (!verified) {
    return base::unexpected(SignatureError::kInvalidSignature);
  }
  return base::ok();
}

SignatureErrorOr<void> VerifyEcdsaSignature(
    const keypair::PublicKey& public_key,
    base::span<const uint8_t> statement,
    hash::HashKind hash_kind,
    base::span<const uint8_t> ecdsa_r,
    base::span<const uint8_t> ecdsa_s) {
  std::optional<std::vector<uint8_t>> der_sig =
      ConvertEcdsaRawComponentsToDer(ecdsa_r, ecdsa_s);
  bool verified = der_sig.has_value() &&
                  sign::Verify(ToSignatureKind(Algorithm::kEcdsa, hash_kind),
                               public_key, statement, *der_sig);
  if (!verified) {
    return base::unexpected(SignatureError::kInvalidSignature);
  }
  return base::ok();
}

}  // namespace

std::vector<uint8_t> BuildCertifyCommand(uint32_t object_handle,
                                         uint32_t sign_handle,
                                         base::span<const uint8_t> challenge) {
  return base::ToVector(build_certify_command(
      object_handle, sign_handle, base::SpanToRustSlice(challenge)));
}

TpmParseErrorOr<CertifyResponse> ParseCertifyResponse(
    base::span<const uint8_t> response_blob,
    base::span<const uint8_t> challenge) {
  RawCertifyResponse raw_response = parse_certify_response(
      base::SpanToRustSlice(response_blob), base::SpanToRustSlice(challenge));

  return MapParseResult(raw_response.result, raw_response.tpm_response_code)
      .transform([&] {
        return CertifyResponse{
            .statement = base::ToVector(raw_response.statement),
            .signature = base::ToVector(raw_response.signature),
        };
      });
}

std::vector<uint8_t> BuildHashCommand(base::span<const uint8_t> data,
                                      uint16_t hash_alg,
                                      uint32_t hierarchy) {
  return base::ToVector(
      build_hash_command(base::SpanToRustSlice(data), hash_alg, hierarchy));
}

TpmParseErrorOr<HashResponse> ParseHashResponse(
    base::span<const uint8_t> response_blob) {
  RawHashResponse raw_response =
      parse_hash_response(base::SpanToRustSlice(response_blob));

  return MapParseResult(raw_response.result, raw_response.tpm_response_code)
      .transform([&] {
        return HashResponse{
            .digest = base::ToVector(raw_response.digest),
            .validation_ticket = base::ToVector(raw_response.validation_ticket),
        };
      });
}

std::vector<uint8_t> BuildSignCommand(
    uint32_t key_handle,
    base::span<const uint8_t> digest,
    uint16_t sig_alg,
    uint16_t hash_alg,
    base::span<const uint8_t> validation_ticket) {
  return base::ToVector(
      build_sign_command(key_handle, base::SpanToRustSlice(digest), sig_alg,
                         hash_alg, base::SpanToRustSlice(validation_ticket)));
}

TpmParseErrorOr<SignResponse> ParseSignResponse(
    base::span<const uint8_t> response_blob) {
  RawSignResponse raw_response =
      parse_sign_response(base::SpanToRustSlice(response_blob));

  return MapParseResult(raw_response.result, raw_response.tpm_response_code)
      .transform([&] {
        return SignResponse{
            .signature = base::ToVector(raw_response.signature),
        };
      });
}

SignatureErrorOr<SignatureAlgorithms> GetSignatureAlgorithms(
    base::span<const uint8_t> signature_blob) {
  RawSignatureComponents raw_sig =
      parse_tpm_signature(base::SpanToRustSlice(signature_blob));

  RETURN_IF_ERROR(MapSignatureParseResult(raw_sig.status));

  return SignatureAlgorithms{
      .sig_alg = std::to_underlying(raw_sig.sig_alg),
      .hash_alg = std::to_underlying(raw_sig.hash_alg),
  };
}

SignatureErrorOr<void> VerifySignature(
    base::span<const uint8_t> spki,
    base::span<const uint8_t> statement,
    base::span<const uint8_t> signature_blob) {
  // 1. Parse the signature using Rust FFI
  RawSignatureComponents raw_sig =
      parse_tpm_signature(base::SpanToRustSlice(signature_blob));

  RETURN_IF_ERROR(MapSignatureParseResult(raw_sig.status));

  ASSIGN_OR_RETURN(hash::HashKind hash_kind,
                   MapHashAlgorithm(raw_sig.hash_alg));

  // 2. Import public key
  ASSIGN_OR_RETURN(auto public_key,
                   keypair::PublicKey::FromSubjectPublicKeyInfo(spki),
                   [] { return SignatureError::kInvalidPublicKey; });

  // 3. Verify signature
  switch (raw_sig.sig_alg) {
    case TpmAlg::TPM_ALG_RSASSA:
      return VerifyRsaSignature(public_key, statement, hash_kind,
                                raw_sig.rsa_sig);
    case TpmAlg::TPM_ALG_ECDSA:
      return VerifyEcdsaSignature(public_key, statement, hash_kind,
                                  raw_sig.ecdsa_r, raw_sig.ecdsa_s);
    default:
      return base::unexpected(SignatureError::kUnsupportedSignatureAlgorithm);
  }
}

}  // namespace crypto::tpm
