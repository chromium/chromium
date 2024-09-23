// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEY_H_
#define NET_COOKIES_COOKIE_PARTITION_KEY_H_

#include <optional>
#include <string>

#include "base/types/expected.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/features.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

#if !BUILDFLAG(CRONET_BUILD)
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#endif

namespace net {

class SiteForCookies;

class NET_EXPORT CookiePartitionKey {
 public:
  class NET_EXPORT SerializedCookiePartitionKey {
   public:
    const std::string& TopLevelSite() const;
    bool has_cross_site_ancestor() const;

    std::string GetDebugString() const;

    // This constructor does not check if the values being serialized are valid.
    // The caller of this function must ensure that only valid values are passed
    // to this method.
    SerializedCookiePartitionKey(base::PassKey<CookiePartitionKey> key,
                                 const std::string& site,
                                 bool has_cross_site_ancestor);

   private:
    std::string top_level_site_;
    bool has_cross_site_ancestor_;
  };

  // An enumerated value representing whether any frame in the PartitionKey's
  // ancestor chain (including the top-level document's site) is cross-site with
  // the current frame. These values are persisted to disk. Entries should not
  // be renumbered and numeric values should never be reused.
  enum class AncestorChainBit {
    // All frames in the ancestor chain are pairwise same-site.
    kSameSite = 0,
    // At least one frame in the ancestor chain is cross-site with
    // the current frame.
    kCrossSite = 1,
  };

  static AncestorChainBit BoolToAncestorChainBit(bool val);

  CookiePartitionKey() = delete;
#if !BUILDFLAG(CRONET_BUILD)
  explicit CookiePartitionKey(mojo::DefaultConstruct::Tag);
#endif
  CookiePartitionKey(const CookiePartitionKey& other);
  CookiePartitionKey(CookiePartitionKey&& other);
  CookiePartitionKey& operator=(const CookiePartitionKey& other);
  CookiePartitionKey& operator=(CookiePartitionKey&& other);
  ~CookiePartitionKey();

  bool operator==(const CookiePartitionKey& other) const;
  bool operator!=(const CookiePartitionKey& other) const;
  bool operator<(const CookiePartitionKey& other) const;

  // Methods for serializing and deserializing a partition key to/from a string.
  // This is currently used for:
  // -  Storing persistent partitioned cookies
  // -  Loading partitioned cookies into Java code
  // -  Sending cookie partition keys as strings in the DevTools protocol
  //
  // This function returns true if the partition key is not opaque and if nonce_
  // is not present. We do not want to serialize cookies with opaque origins or
  // nonce in their partition key to disk, because if the browser session ends
  // we will not be able to attach the saved cookie to any future requests. This
  // is because opaque origins' nonces are only stored in volatile memory.
  //
  // TODO(crbug.com/40188414) Investigate ways to persist partition keys with
  // opaque origins if a browser session is restored.
  [[nodiscard]] static base::expected<SerializedCookiePartitionKey, std::string>
  Serialize(const std::optional<CookiePartitionKey>& in);

  static CookiePartitionKey FromURLForTesting(
      const GURL& url,
      AncestorChainBit ancestor_chain_bit = AncestorChainBit::kCrossSite,
      std::optional<base::UnguessableToken> nonce = std::nullopt) {
    return CookiePartitionKey(SchemefulSite(url), nonce, ancestor_chain_bit);
  }

  // Create a partition key from a network isolation key. Partition key is
  // derived from the key's top-frame site. For scripts, the request_site
  // is the url of the context running the code.
  static std::optional<CookiePartitionKey> FromNetworkIsolationKey(
      const NetworkIsolationKey& network_isolation_key,
      const SiteForCookies& site_for_cookies,
      const SchemefulSite& request_site,
      bool main_frame_navigation);

  // Create a new CookiePartitionKey from the site of an existing
  // CookiePartitionKey. This should only be used for sites of partition keys
  // which were already created using Deserialize or FromNetworkIsolationKey.
  static CookiePartitionKey FromWire(
      const SchemefulSite& site,
      AncestorChainBit ancestor_chain_bit,
      std::optional<base::UnguessableToken> nonce = std::nullopt) {
    return CookiePartitionKey(site, nonce, ancestor_chain_bit);
  }

  // Create a new CookiePartitionKey in a script running in a renderer. We do
  // not trust the renderer to provide us with a cookie partition key, so we let
  // the renderer use this method to indicate the cookie is partitioned but the
  // key still needs to be determined.
  //
  // When the browser is ingesting cookie partition keys from the renderer,
  // either the `from_script_` flag should be set or the cookie partition key
  // should match the browser's. Otherwise the renderer may be compromised.
  //
  // TODO(crbug.com/40188414) Consider removing this factory method and
  // `from_script_` flag when BlinkStorageKey is available in
  // ServiceWorkerGlobalScope.
  static std::optional<CookiePartitionKey> FromScript() {
    return std::make_optional(CookiePartitionKey(true));
  }

  // Create a new CookiePartitionKey from the components of a StorageKey.
  // Forwards to FromWire, but unlike that method in this one the optional nonce
  // argument has no default. It also checks that cookie partitioning is enabled
  // before returning a valid key, which FromWire does not check.
  [[nodiscard]] static std::optional<CookiePartitionKey>
  FromStorageKeyComponents(const SchemefulSite& top_level_site,
                           AncestorChainBit ancestor_chain_bit,
                           const std::optional<base::UnguessableToken>& nonce);

  // FromStorage is a factory method which is meant for creating a new
  // CookiePartitionKey using properties of a previously existing
  // CookiePartitionKey that was already ingested into storage. This should NOT
  // be used to create a new CookiePartitionKey that was not previously saved in
  // storage.
  [[nodiscard]] static base::expected<std::optional<CookiePartitionKey>,
                                      std::string>
  FromStorage(const std::string& top_level_site, bool has_cross_site_ancestor);

  // This method should be used when the data provided is expected to be
  // non-null but might be invalid or comes from a potentially untrustworthy
  // source (such as user-supplied data).
  //
  // This reserves FromStorage to handle cases that can result in a null key
  // (and perfectly validly, like in the case when the top_level_site is empty).
  [[nodiscard]] static base::expected<CookiePartitionKey, std::string>
  FromUntrustedInput(const std::string& top_level_site,
                     bool has_cross_site_ancestor);

  const SchemefulSite& site() const { return site_; }

  bool from_script() const { return from_script_; }

  // Returns true if the current partition key can be serialized to a string.
  // Cookie partition keys whose internal site is opaque cannot be serialized.
  bool IsSerializeable() const;

  const std::optional<base::UnguessableToken>& nonce() const { return nonce_; }

  static bool HasNonce(const std::optional<CookiePartitionKey>& key) {
    return key && key->nonce();
  }

  bool IsThirdParty() const {
    return ancestor_chain_bit_ == AncestorChainBit::kCrossSite;
  }

 private:
  // Used by DeserializeInternal to determine how strict the context should be
  // about inconsistencies in the input.
  enum class ParsingMode {
    // The top_level_site string must be serialized exactly as a SchemefulSite
    // would be.
    // Use this when reading from storage.
    kStrict = 0,
    // The top_level_site string must be coercible to a SchemefulSite.
    // Use this for user input.
    kLoose = 1,
  };

  explicit CookiePartitionKey(const SchemefulSite& site,
                              std::optional<base::UnguessableToken> nonce,
                              AncestorChainBit ancestor_chain_bit);
  explicit CookiePartitionKey(bool from_script);

  // This method holds the deserialization logic for validating input from
  // DeserializeForTesting and FromUntrustedInput which can be used to pass
  // unserializable top_level_site values.
  [[nodiscard]] static base::expected<CookiePartitionKey, std::string>
  DeserializeInternal(
      const std::string& top_level_site,
      CookiePartitionKey::AncestorChainBit has_cross_site_ancestor,
      CookiePartitionKey::ParsingMode parsing_mode);

  AncestorChainBit MaybeAncestorChainBit() const;

  SchemefulSite site_;
  bool from_script_ = false;
  // crbug.com/328043119 remove code associated with
  // kAncestorChainBitEnabledInPartitionedCookies
  //  when feature is no longer needed.
  bool ancestor_chain_enabled_ = base::FeatureList::IsEnabled(
      features::kAncestorChainBitEnabledInPartitionedCookies);

  // Having a nonce is a way to force a transient opaque `CookiePartitionKey`
  // for non-opaque origins.
  std::optional<base::UnguessableToken> nonce_;
  AncestorChainBit ancestor_chain_bit_ = AncestorChainBit::kCrossSite;
};

// Used so that CookiePartitionKeys can be the arguments of DCHECK_EQ.
NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const CookiePartitionKey& cpk);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEY_H_
