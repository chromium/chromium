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

TpmParseErrorOr<void> MapResponseStatus(const ResponseStatus& status) {
  switch (status.result) {
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
          TpmParseError::Type::kTpmErrorResponse, status.tpm_response_code));
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

SignatureErrorOr<hash::HashKind> MapHashAlgorithm(TpmAlgHash hash_alg) {
  switch (hash_alg) {
    case TPM_ALG_SHA256:
      return hash::kSha256;
    case TPM_ALG_SHA1:
      return hash::kSha1;
    default:
      return base::unexpected(SignatureError::kUnsupportedHashAlgorithm);
  }
}

constexpr TpmAlgHash ToTpmAlgHash(hash::HashKind hash_kind) {
  switch (hash_kind) {
    case hash::kSha1:
      return TPM_ALG_SHA1;
    case hash::kSha256:
      return TPM_ALG_SHA256;
    case hash::kSha384:
      return TPM_ALG_SHA384;
    case hash::kSha512:
      return TPM_ALG_SHA512;
  }
  NOTREACHED();
}

sign::SignatureKind ToSignatureKind(TpmAlgSigScheme alg, hash::HashKind hash) {
  switch (alg) {
    case TPM_ALG_NULL:
      NOTREACHED();
    case TPM_ALG_RSASSA:
      switch (hash) {
        case hash::kSha1:
          return sign::RSA_PKCS1_SHA1;
        case hash::kSha256:
          return sign::RSA_PKCS1_SHA256;
        case hash::kSha384:
          return sign::RSA_PKCS1_SHA384;
        case hash::kSha512:
          return sign::RSA_PKCS1_SHA512;
      }
    case TPM_ALG_RSAPSS:
      switch (hash) {
        case hash::kSha1:
          NOTREACHED();
        case hash::kSha256:
          return sign::RSA_PSS_SHA256;
        case hash::kSha384:
          return sign::RSA_PSS_SHA384;
        case hash::kSha512:
          return sign::RSA_PSS_SHA512;
      }
    case TPM_ALG_ECDSA:
      switch (hash) {
        case hash::kSha1:
          return sign::ECDSA_SHA1;
        case hash::kSha256:
          return sign::ECDSA_SHA256;
        case hash::kSha384:
          return sign::ECDSA_SHA384;
        case hash::kSha512:
          return sign::ECDSA_SHA512;
      }
  }
  NOTREACHED();
}

SignatureErrorOr<void> VerifyRsaSignature(const keypair::PublicKey& public_key,
                                          base::span<const uint8_t> statement,
                                          hash::HashKind hash_kind,
                                          base::span<const uint8_t> rsa_sig) {
  bool verified = sign::Verify(ToSignatureKind(TPM_ALG_RSASSA, hash_kind),
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
                  sign::Verify(ToSignatureKind(TPM_ALG_ECDSA, hash_kind),
                               public_key, statement, *der_sig);
  if (!verified) {
    return base::unexpected(SignatureError::kInvalidSignature);
  }
  return base::ok();
}

std::optional<SignatureAlgorithms> ToSignatureAlgorithms(
    sign::SignatureKind kind) {
  switch (kind) {
    case sign::SignatureKind::RSA_PKCS1_SHA256:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_RSASSA,
                                 .hash_alg = TPM_ALG_SHA256};
    case sign::SignatureKind::RSA_PKCS1_SHA384:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_RSASSA,
                                 .hash_alg = TPM_ALG_SHA384};
    case sign::SignatureKind::RSA_PKCS1_SHA512:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_RSASSA,
                                 .hash_alg = TPM_ALG_SHA512};
    case sign::SignatureKind::RSA_PSS_SHA256:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_RSAPSS,
                                 .hash_alg = TPM_ALG_SHA256};
    case sign::SignatureKind::RSA_PSS_SHA384:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_RSAPSS,
                                 .hash_alg = TPM_ALG_SHA384};
    case sign::SignatureKind::RSA_PSS_SHA512:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_RSAPSS,
                                 .hash_alg = TPM_ALG_SHA512};
    case sign::SignatureKind::ECDSA_SHA256:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_ECDSA,
                                 .hash_alg = TPM_ALG_SHA256};
    case sign::SignatureKind::ECDSA_SHA384:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_ECDSA,
                                 .hash_alg = TPM_ALG_SHA384};
    case sign::SignatureKind::ECDSA_SHA512:
      return SignatureAlgorithms{.sig_alg = TPM_ALG_ECDSA,
                                 .hash_alg = TPM_ALG_SHA512};
    default:
      return std::nullopt;
  }
}

}  // namespace

std::vector<uint8_t> BuildCertifyCommand(
    uint32_t object_handle,
    uint32_t sign_handle,
    base::span<const uint8_t> qualifying_data) {
  return base::ToVector(build_certify_command(
      object_handle, sign_handle, base::SpanToRustSlice(qualifying_data)));
}

TpmParseErrorOr<CertifyResponse> ParseCertifyResponse(
    base::span<const uint8_t> response_blob,
    base::span<const uint8_t> expected_extra_data) {
  RawCertifyResponse raw_response =
      parse_certify_response(base::SpanToRustSlice(response_blob),
                             base::SpanToRustSlice(expected_extra_data));

  return MapResponseStatus(raw_response.status).transform([&] {
    return CertifyResponse{
        .statement = base::ToVector(raw_response.statement),
        .signature = base::ToVector(raw_response.signature),
    };
  });
}

std::optional<std::vector<uint8_t>> BuildCreateAikCommand(
    uint32_t parent_handle,
    sign::SignatureKind kind) {
  return ToSignatureAlgorithms(kind).transform(
      [parent_handle](const auto& algs) {
        return base::ToVector(build_create_aik_command(
            parent_handle, algs.sig_alg, algs.hash_alg));
      });
}

TpmParseErrorOr<CreateResponse> ParseCreateResponse(
    base::span<const uint8_t> response_blob) {
  RawCreateResponse raw_response =
      parse_create_response(base::SpanToRustSlice(response_blob));

  return MapResponseStatus(raw_response.status).transform([&] {
    return CreateResponse{
        .out_private = base::ToVector(raw_response.out_private),
        .out_public = base::ToVector(raw_response.out_public),
    };
  });
}

std::vector<uint8_t> BuildFlushContextCommand(uint32_t handle) {
  return base::ToVector(build_flush_context_command(handle));
}

TpmParseErrorOr<FlushContextResponse> ParseFlushContextResponse(
    base::span<const uint8_t> response_blob) {
  ResponseStatus status =
      parse_flush_context_response(base::SpanToRustSlice(response_blob));

  return MapResponseStatus(status).transform(
      [] { return FlushContextResponse{}; });
}

std::vector<uint8_t> BuildHashCommand(base::span<const uint8_t> data,
                                      hash::HashKind hash_kind) {
  return base::ToVector(
      build_hash_command(base::SpanToRustSlice(data), ToTpmAlgHash(hash_kind)));
}

TpmParseErrorOr<HashResponse> ParseHashResponse(
    base::span<const uint8_t> response_blob) {
  RawHashResponse raw_response =
      parse_hash_response(base::SpanToRustSlice(response_blob));

  return MapResponseStatus(raw_response.status).transform([&] {
    return HashResponse{
        .digest = base::ToVector(raw_response.digest),
        .validation_ticket = base::ToVector(raw_response.validation_ticket),
    };
  });
}

std::vector<uint8_t> BuildHashSequenceStartCommand(hash::HashKind hash_kind) {
  return base::ToVector(
      build_hash_sequence_start_command(ToTpmAlgHash(hash_kind)));
}

TpmParseErrorOr<HashSequenceStartResponse> ParseHashSequenceStartResponse(
    base::span<const uint8_t> response_blob) {
  RawHashSequenceStartResponse raw_response =
      parse_hash_sequence_start_response(base::SpanToRustSlice(response_blob));

  return MapResponseStatus(raw_response.status).transform([&] {
    return HashSequenceStartResponse{
        .sequence_handle = raw_response.sequence_handle,
    };
  });
}

std::vector<uint8_t> BuildSequenceCompleteCommand(
    uint32_t sequence_handle,
    base::span<const uint8_t> data) {
  return base::ToVector(build_sequence_complete_command(
      sequence_handle, base::SpanToRustSlice(data)));
}

TpmParseErrorOr<SequenceCompleteResponse> ParseSequenceCompleteResponse(
    base::span<const uint8_t> response_blob) {
  RawHashResponse raw_response =
      parse_sequence_complete_response(base::SpanToRustSlice(response_blob));

  return MapResponseStatus(raw_response.status).transform([&] {
    return SequenceCompleteResponse{
        .digest = base::ToVector(raw_response.digest),
        .validation_ticket = base::ToVector(raw_response.validation_ticket),
    };
  });
}

std::vector<uint8_t> BuildSequenceUpdateCommand(
    uint32_t sequence_handle,
    base::span<const uint8_t> data) {
  return base::ToVector(build_sequence_update_command(
      sequence_handle, base::SpanToRustSlice(data)));
}

TpmParseErrorOr<SequenceUpdateResponse> ParseSequenceUpdateResponse(
    base::span<const uint8_t> response_blob) {
  ResponseStatus status =
      parse_sequence_update_response(base::SpanToRustSlice(response_blob));

  return MapResponseStatus(status).transform(
      [] { return SequenceUpdateResponse{}; });
}

std::vector<uint8_t> BuildSignCommand(
    uint32_t key_handle,
    base::span<const uint8_t> digest,
    base::span<const uint8_t> validation_ticket) {
  return base::ToVector(
      build_sign_command(key_handle, base::SpanToRustSlice(digest),
                         base::SpanToRustSlice(validation_ticket)));
}

TpmParseErrorOr<SignResponse> ParseSignResponse(
    base::span<const uint8_t> response_blob) {
  RawSignResponse raw_response =
      parse_sign_response(base::SpanToRustSlice(response_blob));

  return MapResponseStatus(raw_response.status).transform([&] {
    return SignResponse{
        .signature = base::ToVector(raw_response.signature),
    };
  });
}

std::optional<std::vector<uint8_t>> ParseTpmSignature(
    base::span<const uint8_t> signature_blob) {
  RawSignatureComponents raw_sig =
      parse_tpm_signature(base::SpanToRustSlice(signature_blob));

  if (raw_sig.status != SignatureParseResult::Ok) {
    return std::nullopt;
  }

  switch (raw_sig.sig_alg) {
    case TPM_ALG_RSASSA:
      return base::ToVector(raw_sig.rsa_sig);
    case TPM_ALG_ECDSA:
      return ConvertEcdsaRawComponentsToDer(raw_sig.ecdsa_r, raw_sig.ecdsa_s);
    default:
      return std::nullopt;
  }
}

SignatureErrorOr<SignatureAlgorithms> GetSignatureAlgorithms(
    base::span<const uint8_t> signature_blob) {
  RawSignatureComponents raw_sig =
      parse_tpm_signature(base::SpanToRustSlice(signature_blob));

  RETURN_IF_ERROR(MapSignatureParseResult(raw_sig.status));

  return SignatureAlgorithms{
      .sig_alg = raw_sig.sig_alg,
      .hash_alg = raw_sig.hash_alg,
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
    case TPM_ALG_RSASSA:
      return VerifyRsaSignature(public_key, statement, hash_kind,
                                raw_sig.rsa_sig);
    case TPM_ALG_ECDSA:
      return VerifyEcdsaSignature(public_key, statement, hash_kind,
                                  raw_sig.ecdsa_r, raw_sig.ecdsa_s);
    default:
      return base::unexpected(SignatureError::kUnsupportedSignatureAlgorithm);
  }
}

}  // namespace crypto::tpm
