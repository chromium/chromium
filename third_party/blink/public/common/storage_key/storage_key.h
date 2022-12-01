// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_

#include <iosfwd>
#include <string>

#include "base/strings/string_piece_forward.h"
#include "base/unguessable_token.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "url/origin.h"

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

  // StorageKeys with identical origins and top-level sites are first-party and
  // always kSameSite.
  explicit StorageKey(const url::Origin& origin)
      : StorageKey(origin,
                   net::SchemefulSite(origin),
                   nullptr,
                   blink::mojom::AncestorChainBit::kSameSite) {}

  // This function does not take a top-level site as the nonce makes it globally
  // unique anyway. Implementation wise however, the top-level site is set to
  // the `origin`'s site. The AncestorChainBit is not applicable to StorageKeys
  // with a non-empty nonce so they are initialized to kSameSite.
  static StorageKey CreateWithNonce(const url::Origin& origin,
                                    const base::UnguessableToken& nonce);

  // Callers may specify an optional nonce by passing nullptr.
  static StorageKey CreateWithOptionalNonce(
      const url::Origin& origin,
      const net::SchemefulSite& top_level_site,
      const base::UnguessableToken* nonce,
      blink::mojom::AncestorChainBit ancestor_chain_bit);

  // Takes an origin and populates the rest of the data using |isolation_info|.
  // Note: |frame_origin| from |IsolationInfo| should not be used, as that is
  // not a reliable source to get the origin.
  // Note 2: This probably does not correctly account for extension URLs. See
  // https://crbug.com/1346450 for more context.
  static StorageKey CreateFromOriginAndIsolationInfo(
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info);

  // Creates a StorageKey with the passed in |origin|, and all other information
  // taken from the existing StorageKey instance.
  StorageKey WithOrigin(const url::Origin& origin) const;

  // Copyable and Moveable.
  StorageKey(const StorageKey& other) = default;
  StorageKey& operator=(const StorageKey& other) = default;
  StorageKey(StorageKey&& other) noexcept = default;
  StorageKey& operator=(StorageKey&& other) noexcept = default;

  ~StorageKey() = default;

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

  // Takes in two url::Origin types representing origin and top-level site and
  // returns a StorageKey with a nullptr nonce and an AncestorChainBit set based
  // on whether `origin` and `top_level_site` are schemeful-same-site. NOTE: The
  // approach used by this method for calculating the AncestorChainBit is
  // different than what's done in production code, where the whole frame tree
  // is used. In other words, this method cannot be used to create a StorageKey
  // corresponding to a first-party iframe with a cross-site ancestor (e.g.,
  // "a.com" -> "b.com" -> "a.com"). To create a StorageKey for that scenario,
  // use the StorageKey constructor that has an AncestorChainBit parameter.
  static StorageKey CreateForTesting(const url::Origin& origin,
                                     const url::Origin& top_level_site);

  // Takes in a url::Origin `origin` and a net::SchemefulSite `top_level_site`
  // and returns a StorageKey with a nullptr nonce and an AncestorChainBit set
  // based on whether `origin` and `top_level_site` are schemeful-same-site. See
  // the note in `CreateForTesting()` above regarding how the AncestorChainBit
  // is calculated by this method.
  static StorageKey CreateForTesting(const url::Origin& origin,
                                     const net::SchemefulSite& top_level_site);

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
  // nonce or has an AncestorChainBit value of kCrossSite.
  bool IsThirdPartyContext() const {
    return nonce_ ||
           ancestor_chain_bit_ == blink::mojom::AncestorChainBit::kCrossSite ||
           net::SchemefulSite(origin_) != top_level_site_;
  }
  bool IsFirstPartyContext() const { return !IsThirdPartyContext(); }

  const url::Origin& origin() const { return origin_; }

  const net::SchemefulSite& top_level_site() const { return top_level_site_; }

  const absl::optional<base::UnguessableToken>& nonce() const { return nonce_; }

  blink::mojom::AncestorChainBit ancestor_chain_bit() const {
    return ancestor_chain_bit_;
  }

  std::string GetDebugString() const;

  // Provides a concise string representation suitable for memory dumps.
  // Limits the length to `max_length` chars and strips special characters.
  std::string GetMemoryDumpString(size_t max_length) const;

  // Return the "site for cookies" for the StorageKey's frame (or worker).
  //
  // While the SiteForCookie object returned matches the current default
  // behavior it's important to note that it may not exactly match a
  // SiteForCookies created for the same frame context and could cause
  // behavioral difference for users using the
  // LegacySameSiteCookieBehaviorEnabledForDomainList enterprise policy. The
  // impact is expected to be minimal however.
  //
  // (The difference is due to StorageKey not tracking the same state as
  // SiteForCookies, see see net::SiteForCookies::schemefully_same_ for more
  // info.)
  const net::SiteForCookies ToNetSiteForCookies() const;

  // Returns true if the registration key string is partitioned by top-level
  // site but storage partitioning is currently disabled, otherwise returns
  // false. Also returns false if the key string contains a serialized nonce.
  // Used in
  // components/services/storage/service_worker/service_worker_database.cc
  static bool ShouldSkipKeyDueToPartitioning(const std::string& reg_key_string);

  // Returns a copy of what this storage key would have been if
  // `kThirdPartyStoragePartitioning` were enabled. This is a convenience
  // function for callsites that benefit from future functionality.
  // TODO(crbug.com/1159586): Remove when no longer needed.
  StorageKey CopyWithForceEnabledThirdPartyStoragePartitioning() const {
    StorageKey storage_key = *this;
    storage_key.top_level_site_ =
        storage_key.top_level_site_if_third_party_enabled_;
    storage_key.ancestor_chain_bit_ =
        storage_key.ancestor_chain_bit_if_third_party_enabled_;
    return storage_key;
  }

  // Cast a storage key to a cookie partition key. If cookie partitioning is not
  // enabled, then it will always return nullopt.
  const absl::optional<net::CookiePartitionKey> ToCookiePartitionKey() const;

  // Checks whether this StorageKey matches a given origin for the purposes of
  // clearing site data. This method should only be used in trusted contexts,
  // such as extensions browsingData API or settings UIs, as opposed to the
  // untrusted ones, such as the Clear-Site-Data header (where the entire
  // storage key should be matched exactly).
  // For first-party contexts, this matches on the `origin`; for third-party,
  // this matches on the `top_level_site`. This is done to prevent clearing
  // first-party data for a.example.com when only b.example.com needs to be
  // cleared. The 3P partitioned data for the entire example.com will be cleared
  // in contrast to that.
  bool MatchesOriginForTrustedStorageDeletion(const url::Origin& origin) const;

 private:
  // This enum represents the different type of encodable partitioning
  // attributes.
  enum class EncodedAttribute : uint8_t {
    kTopLevelSite = 0,
    kNonceHigh = 1,
    kNonceLow = 2,
    kAncestorChainBit = 3,
    kMaxValue = kAncestorChainBit,
  };

  StorageKey(const url::Origin& origin,
             const net::SchemefulSite& top_level_site,
             const base::UnguessableToken* nonce,
             blink::mojom::AncestorChainBit ancestor_chain_bit)
      : origin_(origin),
        top_level_site_(IsThirdPartyStoragePartitioningEnabled()
                            ? top_level_site
                            : net::SchemefulSite(origin)),
        top_level_site_if_third_party_enabled_(top_level_site),
        nonce_(nonce ? absl::make_optional(*nonce) : absl::nullopt),
        ancestor_chain_bit_(IsThirdPartyStoragePartitioningEnabled()
                                ? ancestor_chain_bit
                                : blink::mojom::AncestorChainBit::kSameSite),
        ancestor_chain_bit_if_third_party_enabled_(ancestor_chain_bit) {}

  // Converts the attribute type into the separator + uint8_t byte
  // serialization. E.x.: kTopLevelSite becomes "^0"
  static std::string SerializeAttributeSeparator(const EncodedAttribute type);

  // Converts the serialized separator into an EncodedAttribute enum.
  // E.x.: "^0" becomes kTopLevelSite.
  // Expects `in` to have a length of 2.
  static absl::optional<EncodedAttribute> DeserializeAttributeSeparator(
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

  // Stores the value `top_level_site_` would have had if
  // `kThirdPartyStoragePartitioning` were enabled. This isn't used in
  // serialization or comparison.
  // TODO(crbug.com/1159586): Remove when no longer needed.
  net::SchemefulSite top_level_site_if_third_party_enabled_;

  // An optional nonce, forcing a partitioned storage from anything else. Used
  // by anonymous iframes:
  // https://github.com/camillelamy/explainers/blob/master/anonymous_iframes.md
  absl::optional<base::UnguessableToken> nonce_;

  // kCrossSite if any frame in the current frame's ancestor chain is
  // cross-site with the current frame. kSameSite if entire ancestor
  // chain is same-site with the current frame. Used by service workers.
  blink::mojom::AncestorChainBit ancestor_chain_bit_{
      blink::mojom::AncestorChainBit::kSameSite};

  // Stores the value `ancestor_chain_bit_` would have had if
  // `kThirdPartyStoragePartitioning` were enabled. This isn't used in
  // serialization or comparison.
  // TODO(crbug.com/1159586): Remove when no longer needed.
  blink::mojom::AncestorChainBit ancestor_chain_bit_if_third_party_enabled_{
      blink::mojom::AncestorChainBit::kSameSite};
};

BLINK_COMMON_EXPORT
std::ostream& operator<<(std::ostream& ostream, const StorageKey& sk);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
