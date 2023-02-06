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

// A class used by Storage APIs as a key for storage. An entity with a given
// storage key may not access data keyed with any other storage key.
//
// When third party storage partitioning is disabled, a StorageKey is equivalent
// to an origin, which is how storage has historically been partitioned.
//
// When third party storage partitioning is enabled, a storage key additionally
// contains a top-level site and an ancestor chain bit (see below). This
// achieves partitioning of an origin by the top-level site that it is embedded
// in. For example, https://chat.example.net embedded in
// https://social-example.org is a distinct key from https://chat.example.net
// embedded in https://news-example.org.
//
// A key is a third-party key if its origin is not in its top-level site (or if
// its ancestor chain bit is `kCrossSite`; see below); otherwise it is a
// first-party key.
//
// A corner-case is a first-party origin embedded in a third-party origin, such
// as https://a.com embedded in https://b.com in https://a.com. The inner
// `a.com` frame can be controlled by `b.com`, and is thus considered
// third-party. The ancestor chain bit tracks this status.
//
// TODO(https://crbug.com/1410254): Use kCrossSite for this case.
// Storage keys can also optionally have a nonce. Keys with different nonces are
// considered distinct, and distinct from a key with no nonce. This is used to
// implement iframe credentialless and other forms of storage partitioning.
// Keys with a nonce disregard the top level site and ancestor chain bit. For
// consistency we set them to the origin's site and `kSameSite` respectively.
//
// TODO(https://crbug.com/1410254): Use kCrossSite for this case.
// Storage keys might have an opaque top level site (for example, if an
// iframe is embedded in a data url). These storage keys always have a
// `kSameSite` ancestor chain bit as it provides no additional distinctiveness.
//
// Storage keys might have a top level site and origin that don't match. These
// storage keys always have a `kCrossSite` ancestor chain bit.
//
// For more details on the overall design, see
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
  static StorageKey CreateWithNonceForTesting(
      const url::Origin& origin,
      const base::UnguessableToken& nonce);

  // Callers may specify an optional `nonce` by passing nullptr.
  // If the `nonce` isn't null, `top_level_site` must be the same as `origin`
  // and `ancestor_chain_bit` must be kSameSite. If `top_level_site` is opaque,
  // `ancestor_chain_bit` must be `kSameSite`, otherwise if `top_level_site`
  // doesn't match `origin` `ancestor_chain_bit` must be `kCrossSite`.
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
  // Only supports the output of Serialize().
  static absl::optional<StorageKey> Deserialize(base::StringPiece in);

  // Transforms a string in the format used for localStorage (without trailing
  // slashes) into a StorageKey if possible.
  // Prefer Deserialize() for uses other than localStorage.
  // TODO(https://crbug.com/1410254): Move this to LocalStorage code.
  static absl::optional<StorageKey> DeserializeForLocalStorage(
      base::StringPiece in);

  // Transforms a string into a first-party StorageKey by interpreting it as an
  // origin. For use in tests only.
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

  // Tries to construct an instance from (potentially
  // untrusted) values that got received over Mojo.
  //
  // Returns whether successful or not. Doesn't touch
  // `out` if false is returned.  This returning true does
  // not mean that whoever sent the values did not lie,
  // merely that they are well-formed.
  //
  // This function should only be used for serializing from Mojo or
  // testing.
  //
  // TODO(crbug.com/1159586): This function can be removed (or greatly
  // simplified) once the
  // `*_if_third_party_enabled_` members are removed.
  static bool FromWire(
      const url::Origin& origin,
      const net::SchemefulSite& top_level_site,
      const net::SchemefulSite& top_level_site_if_third_party_enabled,
      const absl::optional<base::UnguessableToken>& nonce,
      blink::mojom::AncestorChainBit ancestor_chain_bit,
      blink::mojom::AncestorChainBit ancestor_chain_bit_if_third_party_enabled,
      StorageKey& out);

  // Returns true if ThirdPartyStoragePartitioning feature flag is enabled.
  static bool IsThirdPartyStoragePartitioningEnabled();

  // Serializes the `StorageKey` into a string.
  // Do not call if `origin_` is opaque.
  std::string Serialize() const;

  // Serializes into a string in the format used for localStorage (without
  // trailing slashes). Prefer Serialize() for uses other than localStorage. Do
  // not call if `origin_` is opaque.
  // TODO(https://crbug.com/1410254): Move this to LocalStorage code.
  std::string SerializeForLocalStorage() const;

  // `IsThirdPartyContext` returns true if the StorageKey is for a context that
  // is "third-party", i.e. the StorageKey's top-level site and origin have
  // different schemes and/or domains, or an intervening frame in the frame
  // tree is third-party.
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

  // Checks if every single member in a StorageKey matches those in `other`.
  // Since the *_if_third_party_enabled_ fields aren't used normally this
  // function is only useful for testing purposes.
  // This function can be removed when  the *_if_third_party_enabled_ fields are
  // removed.
  bool ExactMatchForTesting(const StorageKey& other) const;

 private:
  // This enum represents the different type of encodable partitioning
  // attributes.
  enum class EncodedAttribute : uint8_t {
    kTopLevelSite = 0,
    kNonceHigh = 1,
    kNonceLow = 2,
    kAncestorChainBit = 3,
    kTopLevelSiteOpaqueNonceHigh = 4,
    kTopLevelSiteOpaqueNonceLow = 5,
    kTopLevelSiteOpaquePrecursor = 6,
    kMaxValue = kTopLevelSiteOpaquePrecursor,
  };

  StorageKey(const url::Origin& origin,
             const net::SchemefulSite& top_level_site,
             const base::UnguessableToken* nonce,
             blink::mojom::AncestorChainBit ancestor_chain_bit);

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
  net::SchemefulSite top_level_site_if_third_party_enabled_ = top_level_site_;

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
  blink::mojom::AncestorChainBit ancestor_chain_bit_if_third_party_enabled_ =
      ancestor_chain_bit_;
};

BLINK_COMMON_EXPORT
std::ostream& operator<<(std::ostream& ostream, const StorageKey& sk);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
