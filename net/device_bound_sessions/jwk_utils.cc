// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/jwk_utils.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64url.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"

namespace net::device_bound_sessions {

namespace {
// The format of JSON Web Key (JWK) is specified in the section 4 of RFC 7517:
// https://www.ietf.org/rfc/rfc7517.html#section-4
//
// The parameters of a particular key type are specified by the JWA spec:
// https://www.ietf.org/rfc/rfc7518.html#section-6
constexpr char kKeyTypeParam[] = "kty";
constexpr char kEcKeyType[] = "EC";
constexpr char kEcCurve[] = "crv";
constexpr char kEcCurveP256[] = "P-256";
constexpr char kEcCoordinateX[] = "x";
constexpr char kEcCoordinateY[] = "y";
constexpr char kRsaKeyType[] = "RSA";
constexpr char kRsaModulus[] = "n";
constexpr char kRsaExponent[] = "e";

// An X9.62 uncompressed P-256 point is 0x04 || x || y, with 32-byte
// coordinates.
constexpr uint8_t kX962UncompressedPrefix = 0x04;
constexpr size_t kEcP256FieldElementLength = 32;
constexpr size_t kEcP256UncompressedPointLength =
    1 + 2 * kEcP256FieldElementLength;

std::string Base64UrlEncode(base::span<const uint8_t> input) {
  std::string output;
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

}  // namespace

std::optional<base::DictValue> CreateJwk(
    const crypto::keypair::PublicKey& key) {
  if (key.IsEcP256()) {
    std::vector<uint8_t> uncompressed = key.ToUncompressedX962Point();
    auto point = base::span(uncompressed)
                     .to_fixed_extent<kEcP256UncompressedPointLength>();
    CHECK(point);
    CHECK_EQ(point->front(), kX962UncompressedPrefix);
    auto [x, y] = point->subspan<1>().split_at<kEcP256FieldElementLength>();
    return base::DictValue()
        .Set(kKeyTypeParam, kEcKeyType)
        .Set(kEcCurve, kEcCurveP256)
        .Set(kEcCoordinateX, Base64UrlEncode(x))
        .Set(kEcCoordinateY, Base64UrlEncode(y));
  }

  if (key.IsRsa()) {
    return base::DictValue()
        .Set(kKeyTypeParam, kRsaKeyType)
        .Set(kRsaModulus, Base64UrlEncode(key.GetRsaModulus()))
        .Set(kRsaExponent, Base64UrlEncode(key.GetRsaExponent()));
  }

  return std::nullopt;
}

std::optional<std::string> CreateJwkThumbprint(
    const crypto::keypair::PublicKey& key) {
  // `CreateJwk()` only supports EC P-256 and RSA keys, and
  // populates exclusively the required members from RFC 7638 Section 3.2.
  // `base::DictValue` stores keys in lexicographical order and
  // `base::WriteJson()` produces compact JSON with no whitespace, matching
  // the canonical representation required by RFC 7638 Section 3.1.
  return CreateJwk(key)
      .and_then([](const base::DictValue& jwk) { return base::WriteJson(jwk); })
      .transform([](std::string_view canonical_jwk) {
        return Base64UrlEncode(crypto::hash::Sha256(canonical_jwk));
      });
}

}  // namespace net::device_bound_sessions
