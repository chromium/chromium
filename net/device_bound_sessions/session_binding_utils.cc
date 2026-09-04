// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_binding_utils.h"

#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "crypto/sign.h"
#include "net/base/url_util.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

namespace {

// Source: JSON Web Signature and Encryption Algorithms
// https://www.iana.org/assignments/jose/jose.xhtml,
// RFC 8037 (EdDSA in JOSE), and RFC 9964 (ML-DSA in JOSE).
std::optional<std::string_view> SignatureAlgorithmToString(
    crypto::sign::SignatureKind algorithm) {
  switch (algorithm) {
    case crypto::sign::RSA_PKCS1_SHA1:
      return "RS1";
    case crypto::sign::RSA_PKCS1_SHA256:
      return "RS256";
    case crypto::sign::RSA_PKCS1_SHA384:
      return "RS384";
    case crypto::sign::RSA_PKCS1_SHA512:
      return "RS512";
    case crypto::sign::RSA_PSS_SHA256:
      return "PS256";
    case crypto::sign::RSA_PSS_SHA384:
      return "PS384";
    case crypto::sign::RSA_PSS_SHA512:
      return "PS512";
    case crypto::sign::ECDSA_SHA1:
      // SHA-1 with ECDSA has no standard JWA representation.
      return std::nullopt;
    case crypto::sign::ECDSA_SHA256:
      return "ES256";
    case crypto::sign::ECDSA_SHA384:
      return "ES384";
    case crypto::sign::ECDSA_SHA512:
      return "ES512";
    case crypto::sign::ED25519:
      return "EdDSA";
    case crypto::sign::MLDSA_44:
      return "ML-DSA-44";
    case crypto::sign::MLDSA_65:
      return "ML-DSA-65";
    case crypto::sign::MLDSA_87:
      return "ML-DSA-87";
  }
}

std::string Base64UrlEncode(std::string_view data) {
  std::string output;
  base::Base64UrlEncode(data, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

std::optional<std::string> CombineHeaderAndPayload(
    const base::DictValue& header,
    const base::DictValue& payload) {
  std::optional<std::string> header_serialized = base::WriteJson(header);
  if (!header_serialized) {
    DVLOG(1) << "Unexpected JSONWriter error while serializing a registration "
                "token header";
    return std::nullopt;
  }

  std::optional<std::string> payload_serialized = base::WriteJsonWithOptions(
      payload, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION);
  if (!payload_serialized) {
    DVLOG(1) << "Unexpected JSONWriter error while serializing a registration "
                "token payload";
    return std::nullopt;
  }

  return base::StrCat({Base64UrlEncode(*header_serialized), ".",
                       Base64UrlEncode(*payload_serialized)});
}

// Helper function for the shared functionality of refresh and
// registration JWTs.
std::optional<std::string> CreateHeaderAndPayload(
    std::optional<std::string> challenge,
    crypto::sign::SignatureKind algorithm,
    std::optional<base::DictValue> jwk,
    const std::optional<std::string>& authorization) {
  ASSIGN_OR_RETURN(std::string_view alg, SignatureAlgorithmToString(algorithm));
  auto header = base::DictValue().Set("alg", alg).Set("typ", "dbsc+jwt");
  if (jwk.has_value()) {
    header.Set("jwk", std::move(*jwk));
  }

  auto payload = base::DictValue();
  if (challenge.has_value()) {
    payload.Set("jti", *challenge);
  }
  if (authorization.has_value()) {
    payload.Set("authorization", authorization.value());
  }

  return CombineHeaderAndPayload(header, payload);
}

}  // namespace

base::DictValue CreateAttestationValue(
    const crypto::AttestationStatement& attestation_statement) {
  std::string_view format;
  switch (attestation_statement.format) {
    case crypto::AttestationStatement::Format::kTpm:
      format = "TPM";
      break;
    case crypto::AttestationStatement::Format::kSecureEnclave:
      format = "SECURE_ENCLAVE";
      break;
  }
  return base::DictValue()
      .Set("fmt", format)
      .Set("stmt", Base64UrlEncode(
                       base::as_string_view(attestation_statement.statement)))
      .Set("sig", Base64UrlEncode(
                      base::as_string_view(attestation_statement.signature)));
}

std::optional<std::string> CreateOuterRegistrationHeaderAndPayload(
    std::string_view inner_jws,
    crypto::sign::SignatureKind aik_algorithm,
    base::span<const uint8_t> aik_pubkey_spki,
    std::string_view aud,
    const crypto::AttestationStatement& attestation_stmt) {
  ASSIGN_OR_RETURN(std::string_view alg,
                   SignatureAlgorithmToString(aik_algorithm));
  base::DictValue jwk = ConvertPkeySpkiToJwk(aik_algorithm, aik_pubkey_spki);
  if (jwk.empty()) {
    DVLOG(1) << "Unexpected error when converting the SPKI to a JWK";
    return std::nullopt;
  }

  auto header = base::DictValue()
                    .Set("alg", alg)
                    .Set("typ", "dbsc+aik")
                    .Set("cty", "jwt")
                    .Set("jwk", std::move(jwk));

  auto payload = base::DictValue()
                     .Set("aud", aud)
                     .Set("jti", inner_jws)
                     .Set("att", CreateAttestationValue(attestation_stmt));

  return CombineHeaderAndPayload(header, payload);
}

std::optional<std::string> CreateKeyRegistrationHeaderAndPayload(
    std::optional<std::string> challenge,
    crypto::sign::SignatureKind algorithm,
    base::span<const uint8_t> pubkey_spki,
    std::optional<std::string> authorization) {
  base::DictValue jwk = ConvertPkeySpkiToJwk(algorithm, pubkey_spki);
  if (jwk.empty()) {
    DVLOG(1) << "Unexpected error when converting the SPKI to a JWK";
    return std::nullopt;
  }

  return CreateHeaderAndPayload(challenge, algorithm, std::move(jwk),
                                std::move(authorization));
}

std::optional<std::string> CreateKeyRefreshHeaderAndPayload(
    std::optional<std::string> challenge,
    crypto::sign::SignatureKind algorithm) {
  return CreateHeaderAndPayload(challenge, algorithm, /*jwk=*/std::nullopt,
                                /*authorization=*/std::nullopt);
}

std::optional<std::string> AppendSignatureToHeaderAndPayload(
    std::string_view header_and_payload,
    crypto::sign::SignatureKind algorithm,
    base::span<const uint8_t> pubkey_spki,
    base::span<const uint8_t> signature) {
  std::optional<std::vector<uint8_t>> signature_holder;
  if (algorithm == crypto::sign::ECDSA_SHA256) {
    std::optional<crypto::keypair::PublicKey> public_key =
        crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(pubkey_spki);
    if (!public_key.has_value()) {
      return std::nullopt;
    }
    signature_holder =
        crypto::ConvertEcdsaDerSignatureToRaw(*public_key, signature);
    if (!signature_holder.has_value()) {
      return std::nullopt;
    }
    signature = base::span(*signature_holder);
  }

  return base::StrCat(
      {header_and_payload, ".", Base64UrlEncode(as_string_view(signature))});
}

const char kSecFetchSiteHeaderName[] = "Sec-Fetch-Site";
const char kSecFetchModeHeaderName[] = "Sec-Fetch-Mode";
const char kSecFetchDestHeaderName[] = "Sec-Fetch-Dest";

bool IsSecure(const GURL& url) {
  return url.SchemeIsCryptographic() || IsLocalhost(url);
}

std::string_view SecFetchSiteForReferringOrigin(
    const url::Origin& referring_origin,
    const GURL& target_url) {
  switch (GetOriginRelation(target_url, referring_origin)) {
    case OriginRelation::kSameOrigin:
      return "same-origin";
    case OriginRelation::kSameSite:
      return "same-site";
    case OriginRelation::kCrossSite:
      return "cross-site";
  }
  NOTREACHED();
}

}  // namespace net::device_bound_sessions
