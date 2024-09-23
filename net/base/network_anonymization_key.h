// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ANONYMIZATION_KEY_H_
#define NET_BASE_NETWORK_ANONYMIZATION_KEY_H_

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>

#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"

namespace base {
class Value;
}

namespace net {

// NetworkAnonymizationKey (NAK) is used to partition shared network state based
// on the context in which requests were made. Most network state is divided
// by NAK, with some instead using NetworkIsolationKey.
//
// NetworkAnonymizationKey contains the following properties:
//
// `top_frame_site` represents the SchemefulSite of the pages top level frame.
// In order to separate first and third party context from each other this field
// will always be populated.
//
// `is_cross_site` indicates whether the key is cross-site or same-site. A
// same-site key indicates that he schemeful site of the top frame and the frame
// are the same. Intermediary frames between the two may be cross-site to them.
// The effect of this property is to partition first-party and third-party
// resources within a given `top_frame_site`.
//
// The following show how the `is_cross_site` boolean is populated for the
// innermost frame in the chain.
// a->a => is_cross_site = false
// a->b => is_cross_site = true
// a->b->a => is_cross_site = false
// a->(sandboxed a [has nonce]) => is_cross_site = true
//
// The `nonce` value creates a key for anonymous iframes by giving them a
// temporary `nonce` value which changes per top level navigation. For now, any
// NetworkAnonymizationKey with a nonce will be considered transient. This is
// being considered to possibly change in the future in an effort to allow
// anonymous iframes with the same partition key access to shared resources.
// The nonce value will be empty except for anonymous iframes.
//
// This is referred to as "2.5-keyed", to contrast with "double key" (top frame
// site, URL) and "triple key" (top frame site, frame site, and URL). The
// `is_cross_site` bit carries more information than a double key, but less than
// a triple key.
class NET_EXPORT NetworkAnonymizationKey {
 public:
  // Construct an empty key.
  NetworkAnonymizationKey();

  NetworkAnonymizationKey(
      const NetworkAnonymizationKey& network_anonymization_key);
  NetworkAnonymizationKey(NetworkAnonymizationKey&& network_anonymization_key);

  ~NetworkAnonymizationKey();

  NetworkAnonymizationKey& operator=(
      const NetworkAnonymizationKey& network_anonymization_key);
  NetworkAnonymizationKey& operator=(
      NetworkAnonymizationKey&& network_anonymization_key);

  // Compare keys for equality, true if all enabled fields are equal.
  bool operator==(const NetworkAnonymizationKey& other) const {
    return std::tie(top_frame_site_, is_cross_site_, nonce_) ==
           std::tie(other.top_frame_site_, other.is_cross_site_, other.nonce_);
  }

  // Compare keys for inequality, true if any enabled field varies.
  bool operator!=(const NetworkAnonymizationKey& other) const {
    return !(*this == other);
  }

  // Provide an ordering for keys based on all enabled fields.
  bool operator<(const NetworkAnonymizationKey& other) const {
    return std::tie(top_frame_site_, is_cross_site_, nonce_) <
           std::tie(other.top_frame_site_, other.is_cross_site_, other.nonce_);
  }

  // Create a `NetworkAnonymizationKey` from a `top_frame_site`, assuming it is
  // same-site (see comment on the class, above) and has no nonce.
  static NetworkAnonymizationKey CreateSameSite(
      const SchemefulSite& top_frame_site) {
    return NetworkAnonymizationKey(top_frame_site, false, std::nullopt);
  }

  // Create a `NetworkAnonymizationKey` from a `top_frame_site`, assuming it is
  // cross-site (see comment on the class, above) and has no nonce.
  static NetworkAnonymizationKey CreateCrossSite(
      const SchemefulSite& top_frame_site) {
    return NetworkAnonymizationKey(top_frame_site, true, std::nullopt);
  }

  // Create a `NetworkAnonymizationKey` from a `top_frame_site` and
  // `frame_site`. This calculates is_cross_site on the basis of those two
  // sites.
  static NetworkAnonymizationKey CreateFromFrameSite(
      const SchemefulSite& top_frame_site,
      const SchemefulSite& frame_site,
      std::optional<base::UnguessableToken> nonce = std::nullopt);

  // Creates a `NetworkAnonymizationKey` from a `NetworkIsolationKey`. This is
  // possible because a `NetworkIsolationKey` must always be more granular
  // than a `NetworkAnonymizationKey`.
  static NetworkAnonymizationKey CreateFromNetworkIsolationKey(
      const net::NetworkIsolationKey& network_isolation_key);

  // Creates a `NetworkAnonymizationKey` from its constituent parts. This
  // is intended to be used to build a NAK from Mojo, and for tests.
  static NetworkAnonymizationKey CreateFromParts(
      const SchemefulSite& top_frame_site,
      bool is_cross_site,
      std::optional<base::UnguessableToken> nonce = std::nullopt) {
    return NetworkAnonymizationKey(top_frame_site, is_cross_site, nonce);
  }

  // Creates a transient non-empty NetworkAnonymizationKey by creating an opaque
  // origin. This prevents the NetworkAnonymizationKey from sharing data with
  // other NetworkAnonymizationKey.
  static NetworkAnonymizationKey CreateTransient();

  // Returns the string representation of the key.
  std::string ToDebugString() const;

  // Returns true if all parts of the key are empty.
  bool IsEmpty() const;

  // Returns true if `top_frame_site_` is non-empty.
  bool IsFullyPopulated() const;

  // Returns true if this key's lifetime is short-lived. It may not make sense
  // to persist state to disk related to it (e.g., disk cache).
  // A NetworkAnonymizationKey will be considered transient if
  // `top_frame_site_` is empty or opaque or if the key has a `nonce_`.
  bool IsTransient() const;

  // Getters for the top frame, frame site, nonce and is cross site flag.
  const std::optional<SchemefulSite>& GetTopFrameSite() const {
    return top_frame_site_;
  }

  bool IsCrossSite() const { return is_cross_site_; }

  bool IsSameSite() const { return !IsCrossSite(); }

  const std::optional<base::UnguessableToken>& GetNonce() const {
    return nonce_;
  }

  // Returns a representation of |this| as a base::Value. Returns false on
  // failure. Succeeds if either IsEmpty() or !IsTransient().
  [[nodiscard]] bool ToValue(base::Value* out_value) const;

  // Inverse of ToValue(). Writes the result to |network_anonymization_key|.
  // Returns false on failure. Fails on values that could not have been produced
  // by ToValue(), like transient origins.
  [[nodiscard]] static bool FromValue(
      const base::Value& value,
      NetworkAnonymizationKey* out_network_anonymization_key);

  // Determine whether network state partitioning is enabled. This is true if
  // the `PartitionConnectionsByNetworkIsolationKey` feature is enabled, or if
  // `PartitionByDefault()` has been called.
  static bool IsPartitioningEnabled();

  // Default partitioning to enabled, regardless of feature settings. This must
  // be called before any calls to `IsPartitioningEnabled()`.
  static void PartitionByDefault();

  // Clear partitioning-related globals.
  static void ClearGlobalsForTesting();

 private:
  NetworkAnonymizationKey(
      const SchemefulSite& top_frame_site,
      bool is_cross_site,
      std::optional<base::UnguessableToken> nonce = std::nullopt);

  std::string GetSiteDebugString(
      const std::optional<SchemefulSite>& site) const;

  static std::optional<std::string> SerializeSiteWithNonce(
      const SchemefulSite& site);

  // The origin/etld+1 of the top frame of the page making the request. This
  // will always be populated unless all other fields are also nullopt.
  std::optional<SchemefulSite> top_frame_site_;

  // True if the frame site is cross site when compared to the top frame site.
  // This is always false for a non-fully-populated NAK.
  bool is_cross_site_;

  // for non-opaque origins.
  std::optional<base::UnguessableToken> nonce_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const NetworkAnonymizationKey& nak);

}  // namespace net

#endif  // NET_BASE_NETWORK_ANONYMIZATION_KEY_H_
