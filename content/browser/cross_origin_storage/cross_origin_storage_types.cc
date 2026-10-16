// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_origin_storage/cross_origin_storage_types.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "crypto/hash.h"
#include "net/base/is_potentially_trustworthy.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Length of a SHA-256 digest rendered as lowercase hexadecimal.
constexpr size_t kSha256HexLength = 64;

// Returns true if `value` is a lowercase hex string of exactly `length`
// characters. Uppercase is rejected rather than normalized so that the on-disk
// name and the name used for lookups can never disagree.
bool IsLowercaseHexOfLength(std::string_view value, size_t length) {
  if (value.size() != length) {
    return false;
  }
  for (char c : value) {
    const bool is_digit = c >= '0' && c <= '9';
    const bool is_lower_hex_alpha = c >= 'a' && c <= 'f';
    if (!is_digit && !is_lower_hex_alpha) {
      return false;
    }
  }
  return true;
}

}  // namespace

// static
std::optional<CrossOriginStorageHash> CrossOriginStorageHash::Create(
    blink::mojom::CrossOriginStorageAlgorithm algorithm,
    std::string_view value) {
  switch (algorithm) {
    case blink::mojom::CrossOriginStorageAlgorithm::kSha256:
      if (!IsLowercaseHexOfLength(value, kSha256HexLength)) {
        return std::nullopt;
      }
      break;
  }
  return CrossOriginStorageHash(algorithm, std::string(value));
}

CrossOriginStorageHash::CrossOriginStorageHash(
    blink::mojom::CrossOriginStorageAlgorithm algorithm,
    std::string value)
    : algorithm_(algorithm), value_(std::move(value)) {}

CrossOriginStorageHash::CrossOriginStorageHash(const CrossOriginStorageHash&) =
    default;
CrossOriginStorageHash& CrossOriginStorageHash::operator=(
    const CrossOriginStorageHash&) = default;
CrossOriginStorageHash::CrossOriginStorageHash(CrossOriginStorageHash&&) =
    default;
CrossOriginStorageHash& CrossOriginStorageHash::operator=(
    CrossOriginStorageHash&&) = default;
CrossOriginStorageHash::~CrossOriginStorageHash() = default;

bool CrossOriginStorageHash::MatchesDigest(
    base::span<const uint8_t> digest) const {
  switch (algorithm_) {
    case blink::mojom::CrossOriginStorageAlgorithm::kSha256:
      if (digest.size() != crypto::hash::kSha256Size) {
        return false;
      }
      break;
  }
  return base::HexEncodeLower(digest) == value_;
}

CrossOriginStorageAccessSpec::CrossOriginStorageAccessSpec() = default;
CrossOriginStorageAccessSpec::CrossOriginStorageAccessSpec(
    const CrossOriginStorageAccessSpec&) = default;
CrossOriginStorageAccessSpec& CrossOriginStorageAccessSpec::operator=(
    const CrossOriginStorageAccessSpec&) = default;
CrossOriginStorageAccessSpec::CrossOriginStorageAccessSpec(
    CrossOriginStorageAccessSpec&&) = default;
CrossOriginStorageAccessSpec& CrossOriginStorageAccessSpec::operator=(
    CrossOriginStorageAccessSpec&&) = default;
CrossOriginStorageAccessSpec::~CrossOriginStorageAccessSpec() = default;

// static
CrossOriginStorageAccessSpec CrossOriginStorageAccessSpec::AnyOrigin() {
  CrossOriginStorageAccessSpec spec;
  spec.any_origin_ = true;
  return spec;
}

// static
CrossOriginStorageAccessSpec CrossOriginStorageAccessSpec::AllowedOrigins(
    std::vector<url::Origin> origins) {
  CrossOriginStorageAccessSpec spec;
  spec.allowed_origins_ = std::move(origins);
  return spec;
}

// static
std::optional<CrossOriginStorageAccessSpec>
CrossOriginStorageAccessSpec::FromMojo(
    const blink::mojom::CrossOriginStorageOrigins& origins_union) {
  switch (origins_union.which()) {
    case blink::mojom::CrossOriginStorageOrigins::Tag::kAnyOrigin:
      return AnyOrigin();
    case blink::mojom::CrossOriginStorageOrigins::Tag::kOrigins: {
      const std::vector<url::Origin>& origins = origins_union.get_origins();
      if (origins.size() > cos_constants::kMaxTargetOriginsLength) {
        return std::nullopt;
      }
      std::vector<url::Origin> deduplicated;
      deduplicated.reserve(origins.size());
      for (const url::Origin& origin : origins) {
        // Only potentially trustworthy http(s) origins can be named in
        // Cross-Origin Storage sharing grants (per WICG spec § 3.1 `validate a
        // COS request`).
        if ((origin.scheme() != url::kHttpScheme &&
             origin.scheme() != url::kHttpsScheme) ||
            !net::IsOriginPotentiallyTrustworthy(origin)) {
          return std::nullopt;
        }
        if (!std::ranges::contains(deduplicated, origin)) {
          deduplicated.push_back(origin);
        }
      }
      return AllowedOrigins(std::move(deduplicated));
    }
  }
}

}  // namespace content
