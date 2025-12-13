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
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "net/base/url_util.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

namespace {

// Source: JSON Web Signature and Encryption Algorithms
// https://www.iana.org/assignments/jose/jose.xhtml
std::string SignatureAlgorithmToString(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return "ES256";
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
      return "RS256";
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return "PS256";
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
      return "RS1";
  }
}

std::string Base64UrlEncode(std::string_view data) {
  std::string output;
  base::Base64UrlEncode(data, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

std::optional<std::string> CombineHeaderAndPayload(
    const base::Value::Dict& header,
    const base::Value::Dict& payload) {
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
    std::string_view challenge,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    std::optional<base::Value::Dict> jwk,
    const std::optional<std::string>& authorization) {
  auto header = base::Value::Dict()
                    .Set("alg", SignatureAlgorithmToString(algorithm))
                    .Set("typ", "dbsc+jwt");
  if (jwk.has_value()) {
    header.Set("jwk", std::move(*jwk));
  }

  auto payload = base::Value::Dict().Set("jti", challenge);
  if (authorization.has_value()) {
    payload.Set("authorization", authorization.value());
  }

  return CombineHeaderAndPayload(header, payload);
}

}  // namespace

std::optional<std::string> CreateKeyRegistrationHeaderAndPayload(
    std::string_view challenge,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey_spki,
    std::optional<std::string> authorization) {
  base::Value::Dict jwk = ConvertPkeySpkiToJwk(algorithm, pubkey_spki);
  if (jwk.empty()) {
    DVLOG(1) << "Unexpected error when converting the SPKI to a JWK";
    return std::nullopt;
  }

  return CreateHeaderAndPayload(challenge, algorithm, std::move(jwk),
                                std::move(authorization));
}

std::optional<std::string> CreateKeyRefreshHeaderAndPayload(
    std::string_view challenge,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  return CreateHeaderAndPayload(challenge, algorithm, /*jwk=*/std::nullopt,
                                /*authorization=*/std::nullopt);
}

std::optional<std::string> AppendSignatureToHeaderAndPayload(
    std::string_view header_and_payload,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pubkey_spki,
    base::span<const uint8_t> signature) {
  std::optional<std::vector<uint8_t>> signature_holder;
  if (algorithm == crypto::SignatureVerifier::ECDSA_SHA256) {
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

bool IsSecure(const GURL& url) {
  return url.SchemeIsCryptographic() || IsLocalhost(url);
}

}  // namespace net::device_bound_sessions
