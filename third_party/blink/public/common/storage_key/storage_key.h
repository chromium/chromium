// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_

#include <iosfwd>
#include <string>

#include "base/strings/string_piece.h"
#include "base/unguessable_token.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class IsolationInfo;
}

namespace blink {

// A class representing the key that Storage APIs use to key their storage on.
//
// StorageKey contains an origin, a top-level site, and an optional nonce. Using
// the nonce is still unsupported since serialization and deserialization don't
// take it into account. For more details on the overall design, see
// https://docs.google.com/document/d/1xd6MXcUhfnZqIe5dt2CTyCn6gEZ7nOezAEWS0W9hwbQ/edit.
class BLINK_COMMON_EXPORT StorageKey {
 public:
  // This will create a StorageKey with an opaque `origin_` and
  // `top_level_site_`. These two opaque members will not be the same (i.e.,
  // their origin's nonce will be different).
  StorageKey() = default;

  // The following three constructors all create a StorageKey without a nonce;
  // the first of which creates a StorageKey with an implicit top-level site
  // matching the origin. These are currently kept as constructors, rather than
  // as static creation method(s), because of the large number of usages of
  // StorageKey without a top-level site specified. Eventually these will all
  // merge into a static function(s) that will require the caller to explicitly
  // specify that they do not want a top-level site.
  // TODO(https://crbug.com/1271615): Remove or mark as test-only most of these
  // constructors and factory methods.
  explicit StorageKey(const url::Origin& origin)
      : StorageKey(origin, net::SchemefulSite(origin), nullptr) {}

  StorageKey(const url::Origin& origin, const url::Origin& top_level_site)
      : StorageKey(origin, net::SchemefulSite(top_level_site), nullptr) {}

  StorageKey(const url::Origin& origin,
             const net::SchemefulSite& top_level_site)
      : StorageKey(origin, top_level_site, nullptr) {}

  // This function does not take a top-level site as the nonce makes it globally
  // unique anyway. Implementation wise however, the top-level site is set to
  // the `origin`'s site.
  static StorageKey CreateWithNonce(const url::Origin& origin,
                                    const base::UnguessableToken& nonce);

  // Same to the above, but this method does take a top-level site.
  static StorageKey CreateWithOptionalNonce(
      const url::Origin& origin,
      const net::SchemefulSite& top_level_site,
      const base::UnguessableToken* nonce);

  // Copyable and Moveable.
  StorageKey(const StorageKey& other) = default;
  StorageKey& operator=(const StorageKey& other) = default;
  StorageKey(StorageKey&& other) noexcept = default;
  StorageKey& operator=(StorageKey&& other) noexcept = default;

  ~StorageKey() = default;

  // Constructs a StorageKey from a `net::IsolationInfo`.
  static StorageKey FromNetIsolationInfo(
      const net::IsolationInfo& isolation_info);

  // Returns a newly constructed StorageKey from, a previously serialized, `in`.
  // If `in` is invalid then the return value will be nullopt. If this returns a
  // non-nullopt value, it will be a valid, non-opaque StorageKey. A
  // deserialized StorageKey will be equivalent to the StorageKey that was
  // initially serialized.
  //
  // Can be called on the output of either Serialize() or
  // SerializeForLocalStorage(), as it can handle both formats.
  static absl::optional<StorageKey> Deserialize(base::StringPiece in);

  // Transforms a string into a StorageKey if possible (and an opaque StorageKey
  // if not). Currently calls Deserialize, but this may change in future.
  // For use in tests only.
  static StorageKey CreateFromStringForTesting(const std::string& origin);

  // Returns true if ThirdPartyStoragePartitioning feature flag is enabled.
  static bool IsThirdPartyStoragePartitioningEnabled();

  // Serializes the `StorageKey` into a string.
  // Do not call if `this` is opaque.
  std::string Serialize() const;

  // Serializes into a string in the format used for localStorage (without
  // trailing slashes). Prefer Serialize() for uses other than localStorage. Do
  // not call if `this` is opaque.
  std::string SerializeForLocalStorage() const;

  // `IsThirdPartyContext` returns true if the StorageKey is for a context that
  // is "third-party", i.e. the StorageKey's top-level site and origin have
  // different schemes and/or domains.
  //
  // `IsThirdPartyContext` returns true if the StorageKey was created with a
  // nonce.
  //
  // If storage partitioning is disabled, this always returns false.
  bool IsThirdPartyContext() const {
    return nonce_ || net::SchemefulSite(origin_) != top_level_site_;
  }
  bool IsFirstPartyContext() const { return !IsThirdPartyContext(); }

  const url::Origin& origin() const { return origin_; }

  const net::SchemefulSite& top_level_site() const { return top_level_site_; }

  const absl::optional<base::UnguessableToken>& nonce() const { return nonce_; }

  std::string GetDebugString() const;

  // Provides a concise string representation suitable for memory dumps.
  // Limits the length to `max_length` chars and strips special characters.
  std::string GetMemoryDumpString(size_t max_length) const;

  // Return the "site for cookies" for the StorageKey's frame (or worker).
  //
  // Right now this "site for cookies" is not entirely accurate. For example
  // consider if A.com embeds B.com which embeds A.com in a child frame. The
  // site for cookies according to this method will be A.com, but according to
  // the spec it should be an opaque origin.
  const net::SiteForCookies ToNetSiteForCookies() const;

 private:
  // This enum represents the different type of encodable partitioning
  // attributes.
  enum class EncodedAttribute : uint8_t {
    kTopLevelSite = 0,
    kNonceHigh = 1,
    kNonceLow = 2,
    kMax
  };

  StorageKey(const url::Origin& origin,
             const net::SchemefulSite& top_level_site,
             const base::UnguessableToken* nonce)
      : origin_(origin),
        top_level_site_(IsThirdPartyStoragePartitioningEnabled()
                            ? top_level_site
                            : net::SchemefulSite(origin)),
        nonce_(nonce ? absl::make_optional(*nonce) : absl::nullopt) {}

  // Converts the attribute type into the separator + uint8_t byte
  // serialization. E.x.: kTopLevelSite becomes "^0"
  static std::string SerializeAttributeSeparator(const EncodedAttribute type);

  // Converts the serialized separator into an EncodedAttribute enum.
  // E.x.: "^0" becomes kTopLevelSite.
  // Expects `in` to have a length of 2.
  static EncodedAttribute DeserializeAttributeSeparator(
      const base::StringPiece& in);

  BLINK_COMMON_EXPORT
  friend bool operator==(const StorageKey& lhs, const StorageKey& rhs);

  BLINK_COMMON_EXPORT
  friend bool operator!=(const StorageKey& lhs, const StorageKey& rhs);

  // Allows StorageKey to be used as a key in STL (for example, a std::set or
  // std::map).
  BLINK_COMMON_EXPORT
  friend bool operator<(const StorageKey& lhs, const StorageKey& rhs);

  url::Origin origin_;

  // The "top-level site"/"top-level frame"/"main frame" of the context
  // this StorageKey was created for (for storage partitioning purposes).
  //
  // Like everything, this too has exceptions:
  // * For extensions or related enterprise policies this may not represent the
  // top-level site.
  //
  // Note that this value is populated with `origin_`'s site unless the feature
  // flag `kThirdPartyStoragePartitioning` is enabled.
  net::SchemefulSite top_level_site_;

  // An optional nonce, forcing a partitioned storage from anything else. Used
  // by anonymous iframes:
  // https://github.com/camillelamy/explainers/blob/master/anonymous_iframes.md
  absl::optional<base::UnguessableToken> nonce_;
};

BLINK_COMMON_EXPORT
std::ostream& operator<<(std::ostream& ostream, const StorageKey& sk);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
