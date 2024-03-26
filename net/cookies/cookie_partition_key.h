// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEY_H_
#define NET_COOKIES_COOKIE_PARTITION_KEY_H_

#include <optional>
#include <string>

#include "base/types/expected.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

#if !BUILDFLAG(CRONET_BUILD)
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#endif

namespace net {

class NET_EXPORT CookiePartitionKey {
 public:
  class NET_EXPORT SerializedCookiePartitionKey {
   public:
    const std::string& TopLevelSite() const;

   private:
    friend class CookiePartitionKey;
    // This constructor does not check if the values being serialized are valid.
    // The caller of this function must ensure that only valid values are passed
    // to this method.
    //
    // TODO (crbug.com/41486025) once ancestor chain bit is implemented update
    // update constructor to set the value.
    explicit SerializedCookiePartitionKey(const std::string& site);

    std::string top_level_site_;
  };

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
  // TODO(crbug.com/1225444) Investigate ways to persist partition keys with
  // opaque origins if a browser session is restored.
  [[nodiscard]] static base::expected<SerializedCookiePartitionKey, std::string>
  Serialize(const std::optional<CookiePartitionKey>& in);

  static CookiePartitionKey FromURLForTesting(
      const GURL& url,
      const std::optional<base::UnguessableToken> nonce = std::nullopt) {
    return nonce ? CookiePartitionKey(SchemefulSite(url), nonce)
                 : CookiePartitionKey(url);
  }

  // Create a partition key from a network isolation key. Partition key is
  // derived from the key's top-frame site.
  static std::optional<CookiePartitionKey> FromNetworkIsolationKey(
      const NetworkIsolationKey& network_isolation_key);

  // Create a new CookiePartitionKey from the site of an existing
  // CookiePartitionKey. This should only be used for sites of partition keys
  // which were already created using Deserialize or FromNetworkIsolationKey.
  static CookiePartitionKey FromWire(
      const SchemefulSite& site,
      std::optional<base::UnguessableToken> nonce = std::nullopt) {
    return CookiePartitionKey(site, nonce);
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
  // TODO(crbug.com/1225444) Consider removing this factory method and
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
                           const std::optional<base::UnguessableToken>& nonce);

  // FromStorage is a factory method which is meant for creating a new
  // CookiePartitionKey using properties of a previously existing
  // CookiePartitionKey that was already ingested into storage. This should NOT
  // be used to create a new CookiePartitionKey that was not previously saved in
  // storage.
  [[nodiscard]] static base::expected<std::optional<CookiePartitionKey>,
                                      std::string>
  FromStorage(const std::string& top_level_site);

  // This method should be used when the data provided is expected to be
  // non-null but might be invalid or comes from a potentially untrustworthy
  // source (such as user-supplied data).
  //
  // This reserves FromStorage to handle cases that can result in a null key
  // (and perfectly validly, like in the case when the top_level_site is empty).
  [[nodiscard]] static base::expected<CookiePartitionKey, std::string>
  FromUntrustedInput(const std::string& top_level_site);

  const SchemefulSite& site() const { return site_; }

  bool from_script() const { return from_script_; }

  // Returns true if the current partition key can be serialized to a string.
  // Cookie partition keys whose internal site is opaque cannot be serialized.
  bool IsSerializeable() const;

  const std::optional<base::UnguessableToken>& nonce() const { return nonce_; }

  static bool HasNonce(const std::optional<CookiePartitionKey>& key) {
    return key && key->nonce();
  }

 private:
  explicit CookiePartitionKey(const SchemefulSite& site,
                              std::optional<base::UnguessableToken> nonce);
  explicit CookiePartitionKey(const GURL& url);
  explicit CookiePartitionKey(bool from_script);

  // This method holds the deserialization logic for validating input from
  // DeserializeForTesting and FromUntrustedInput which can be used to pass
  // unserializable top_level_site values.
  [[nodiscard]] static base::expected<CookiePartitionKey, std::string>
  DeserializeInternal(const std::string& top_level_site);

  SchemefulSite site_;
  bool from_script_ = false;

  // Having a nonce is a way to force a transient opaque `CookiePartitionKey`
  // for non-opaque origins.
  std::optional<base::UnguessableToken> nonce_;
};

// Used so that CookiePartitionKeys can be the arguments of DCHECK_EQ.
NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const CookiePartitionKey& cpk);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEY_H_
