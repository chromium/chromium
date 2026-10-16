// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_TYPES_H_
#define CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/cross_origin_storage/cross_origin_storage.mojom.h"
#include "url/origin.h"

namespace content {

namespace cos_constants {

// Upper bound on the number of origins named in
// `CrossOriginStorageOrigins::origins` or stored per entry.
inline constexpr size_t kMaxTargetOriginsLength = 100;

// Fraction of total device disk space allowed per resource, matching Chrome's
// Cache API per-StorageKey quota (`0.8` pool ratio * `0.75` per-key ratio =
// `0.6`, see `storage/browser/quota/quota_settings.cc`).
// TODO(crbug.com/547302825): Implement cumulative per-origin quota accounting,
// free-space checks, and eviction (per WICG § 5.1). Currently this ratio caps
// each single staging resource rather than cumulative usage per origin.
inline constexpr double kPerOriginDiskSpaceRatio = 0.6;

// Fallback upper bound on the size of a single Cross-Origin Storage resource
// (64 GiB), used when `base::SysInfo::AmountOfTotalDiskSpace()` is unavailable.
inline constexpr int64_t kMaxResourceSizeBytes = 64LL * 1024 * 1024 * 1024;

}  // namespace cos_constants

enum class CrossOriginStorageStatus {
  kSuccess,
  kNotFound,
  kDataError,
  kQuotaExceeded,
  kInvalidArgument,
  kOperationFailed,
  kOperationAborted,
};

// A validated Cross-Origin Storage content identifier. Instances can only be
// created through Create(), which enforces that the digest is well formed, so
// holding one is proof that the value is safe to use as a file name component.
class CONTENT_EXPORT CrossOriginStorageHash {
 public:
  // Returns std::nullopt unless `value` is exactly 64 lowercase hexadecimal
  // characters and `algorithm` is a supported algorithm.
  static std::optional<CrossOriginStorageHash> Create(
      blink::mojom::CrossOriginStorageAlgorithm algorithm,
      std::string_view value);

  CrossOriginStorageHash(const CrossOriginStorageHash&);
  CrossOriginStorageHash& operator=(const CrossOriginStorageHash&);
  CrossOriginStorageHash(CrossOriginStorageHash&&);
  CrossOriginStorageHash& operator=(CrossOriginStorageHash&&);
  ~CrossOriginStorageHash();

  blink::mojom::CrossOriginStorageAlgorithm algorithm() const {
    return algorithm_;
  }

  // The lowercase hexadecimal digest. Guaranteed to be a safe path component.
  const std::string& value() const { return value_; }

  // Returns whether `digest` matches this hash. Used to verify ingested bytes
  // before they are committed to the shared cache.
  bool MatchesDigest(base::span<const uint8_t> digest) const;

  bool operator==(const CrossOriginStorageHash& other) const = default;
  auto operator<=>(const CrossOriginStorageHash& other) const = default;

 private:
  CrossOriginStorageHash(blink::mojom::CrossOriginStorageAlgorithm algorithm,
                         std::string value);

  blink::mojom::CrossOriginStorageAlgorithm algorithm_;
  std::string value_;
};

// The set of origins an entry is shared with, as declared by a storing origin.
// Enforces that a single write request cannot simultaneously specify both
// `any_origin` and a non-empty `allowed_origins` list.
class CONTENT_EXPORT CrossOriginStorageAccessSpec {
 public:
  CrossOriginStorageAccessSpec();
  CrossOriginStorageAccessSpec(const CrossOriginStorageAccessSpec&);
  CrossOriginStorageAccessSpec& operator=(const CrossOriginStorageAccessSpec&);
  CrossOriginStorageAccessSpec(CrossOriginStorageAccessSpec&&);
  CrossOriginStorageAccessSpec& operator=(CrossOriginStorageAccessSpec&&);
  ~CrossOriginStorageAccessSpec();

  static CrossOriginStorageAccessSpec AnyOrigin();
  static CrossOriginStorageAccessSpec AllowedOrigins(
      std::vector<url::Origin> origins);

  // Translates mojom sharing policy. Returns std::nullopt if the policy names
  // too many origins or contains an origin that is not a potentially
  // trustworthy HTTP(S) origin.
  static std::optional<CrossOriginStorageAccessSpec> FromMojo(
      const blink::mojom::CrossOriginStorageOrigins& origins_union);

  // Whether the entry is readable by any origin (subject to the Public Hash
  // List).
  bool any_origin() const { return any_origin_; }

  // Origins explicitly allowed to read the entry (even when not on the Public
  // Hash List). Always empty when `any_origin()` is true.
  const std::vector<url::Origin>& allowed_origins() const {
    return allowed_origins_;
  }

 private:
  bool any_origin_ = false;
  std::vector<url::Origin> allowed_origins_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_TYPES_H_
