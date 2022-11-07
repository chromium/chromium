// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEY_H_
#define NET_COOKIES_COOKIE_PARTITION_KEY_H_

#include <string>

#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {

class NET_EXPORT CookiePartitionKey {
 public:
  CookiePartitionKey();
  CookiePartitionKey(const CookiePartitionKey& other);
  CookiePartitionKey(CookiePartitionKey&& other);
  CookiePartitionKey& operator=(const CookiePartitionKey& other);
  CookiePartitionKey& operator=(CookiePartitionKey&& other);
  ~CookiePartitionKey();

  bool operator==(const CookiePartitionKey& other) const;
  bool operator!=(const CookiePartitionKey& other) const;
  bool operator<(const CookiePartitionKey& other) const;

  // Methods for serializing and deserializing a partition key to/from a string.
  // This will be used for Android, storing persistent partitioned cookies, and
  // loading partitioned cookies into Java code.
  //
  // This function returns true if the partition key is not opaque and if nonce_
  // is not present. We do not want to serialize cookies with opaque origins or
  // nonce in their partition key to disk, because if the browser session ends
  // we will not be able to attach the saved cookie to any future requests. This
  // is because opaque origins' nonces are only stored in volatile memory.
  //
  // TODO(crbug.com/1225444) Investigate ways to persist partition keys with
  // opaque origins if a browser session is restored.
  [[nodiscard]] static bool Serialize(
      const absl::optional<CookiePartitionKey>& in,
      std::string& out);
  // Deserializes the result of the method above.
  // If the result is absl::nullopt, the resulting cookie is not partitioned.
  //
  // Returns if the resulting partition key is valid.
  [[nodiscard]] static bool Deserialize(
      const std::string& in,
      absl::optional<CookiePartitionKey>& out);

  static CookiePartitionKey FromURLForTesting(
      const GURL& url,
      const absl::optional<base::UnguessableToken> nonce = absl::nullopt) {
    return nonce ? CookiePartitionKey(SchemefulSite(url), nonce)
                 : CookiePartitionKey(url);
  }

  // Create a partition key from a network isolation key. Partition key is
  // derived from the key's top-frame site.
  static absl::optional<CookiePartitionKey> FromNetworkIsolationKey(
      const NetworkIsolationKey& network_isolation_key);

  // Create a new CookiePartitionKey from the site of an existing
  // CookiePartitionKey. This should only be used for sites of partition keys
  // which were already created using Deserialize or FromNetworkIsolationKey.
  static CookiePartitionKey FromWire(
      const SchemefulSite& site,
      absl::optional<base::UnguessableToken> nonce = absl::nullopt) {
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
  static absl::optional<CookiePartitionKey> FromScript() {
    return absl::make_optional(CookiePartitionKey(true));
  }

  // Create a new CookiePartitionKey from the components of a StorageKey.
  // Forwards to FromWire, but unlike that method in this one the optional nonce
  // argument has no default. It also checks that cookie partitioning is enabled
  // before returning a valid key, which FromWire does not check.
  static absl::optional<CookiePartitionKey> FromStorageKeyComponents(
      const SchemefulSite& top_level_site,
      const absl::optional<base::UnguessableToken>& nonce);

  const SchemefulSite& site() const { return site_; }

  bool from_script() const { return from_script_; }

  // Returns true if the current partition key can be serialized to a string.
  // Cookie partition keys whose internal site is opaque cannot be serialized.
  bool IsSerializeable() const;

  const absl::optional<base::UnguessableToken>& nonce() const { return nonce_; }

  static bool HasNonce(const absl::optional<CookiePartitionKey>& key) {
    return key && key->nonce();
  }

 private:
  explicit CookiePartitionKey(const SchemefulSite& site,
                              absl::optional<base::UnguessableToken> nonce);
  explicit CookiePartitionKey(const GURL& url);
  explicit CookiePartitionKey(bool from_script);

  SchemefulSite site_;
  bool from_script_ = false;

  // Having a nonce is a way to force a transient opaque `CookiePartitionKey`
  // for non-opaque origins.
  absl::optional<base::UnguessableToken> nonce_;
};

// Used so that CookiePartitionKeys can be the arguments of DCHECK_EQ.
NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const CookiePartitionKey& cpk);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEY_H_
