// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ANONYMIZATION_KEY_H_
#define NET_BASE_NETWORK_ANONYMIZATION_KEY_H_

#include <cstddef>
#include <string>
#include <tuple>

#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}

namespace net {

// NetworkAnonymizationKey will be used to partition shared network state based
// on the context on which they were made. This class is an expiremental key
// that contains properties that will be changed via feature flags.

// NetworkAnonymizationKey contains the following properties:

// `top_frame_site` represents the SchemefulSite of the pages top level frame.
// In order to separate first and third party context from each other this field
// will always be populated.

//`is_cross_site` is an expiremental boolean that will be used with the
//`top_frame_site` to create a partition key that separates the
//`top_frame_site`s first party partition from any cross-site iframes. This will
// be used only when `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled.
// When `kEnableCrossSiteFlagNetworkAnonymizationKey` is disabled,
// `is_cross_site_` will be an empty optional.

// The following show how the `is_cross_site` boolean is populated for the
// innermost frame in the chain.
// a->a => is_cross_site = false
// a->b => is_cross_site = true
// a->b->a => is_cross_site = false
// a->(sandboxed a [has nonce]) => is_cross_site = true

// The `nonce` value creates a key for anonymous iframes by giving them a
// temporary `nonce` value which changes per top level navigation. For now, any
// NetworkAnonymizationKey with a nonce will be considered transient. This is
// being considered to possibly change in the future in an effort to allow
// anonymous iframes with the same partition key access to shared resources.
// The nonce value will be empty except for anonymous iframes.

// TODO @brgoldstein, add link to public documentation of key scheme naming
// conventions.

class NET_EXPORT NetworkAnonymizationKey {
 public:
  // TODO(crbug/1372123): Consider having the constructor not pass
  // `is_cross_site` since this may be unnecessary and confusing to consumers.
  NetworkAnonymizationKey(
      const SchemefulSite& top_frame_site,
      const absl::optional<SchemefulSite>& frame_site = absl::nullopt,
      const absl::optional<bool> is_cross_site = absl::nullopt,
      const absl::optional<base::UnguessableToken> nonce = absl::nullopt);

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

  // Creates a NetworkAnonymizationKey from a NetworkIsolationKey. This is
  // possible because a NetworkIsolationKey must always be more granular
  // than a NetworkAnonymizationKey.
  static NetworkAnonymizationKey CreateFromNetworkIsolationKey(
      const net::NetworkIsolationKey& network_isolation_key);

  // TODO(crbug/1372769)
  // Intended for temporary use in locations that should be using main frame and
  // frame origin, but are currently only using frame origin, because the
  // creating object may be shared across main frame objects. Having a special
  // constructor for these methods makes it easier to keep track of locating
  // callsites that need to have their NetworkAnonymizationKey filled in.
  static NetworkAnonymizationKey ToDoUseTopFrameOriginAsWell(
      const url::Origin& incorrectly_used_frame_origin) {
    net::SchemefulSite incorrectly_used_frame_site =
        net::SchemefulSite(incorrectly_used_frame_origin);
    return NetworkAnonymizationKey(incorrectly_used_frame_site,
                                   incorrectly_used_frame_site);
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
  const absl::optional<SchemefulSite>& GetTopFrameSite() const {
    return top_frame_site_;
  }

  absl::optional<bool> GetIsCrossSite() const;

  const absl::optional<base::UnguessableToken>& GetNonce() const {
    return nonce_;
  }

  // Returns true if the NetworkAnonymizationKey has a triple keyed scheme. This
  // means the values of the NetworkAnonymizationKey are as follows:
  // `top_frame_site` -> the schemeful site of the top level page.
  // `frame_site ` -> the schemeful site of the requestor frame
  // `is_cross_site` -> nullopt
  static bool IsFrameSiteEnabled();

  // Returns true if the NetworkAnonymizationKey has a double keyed scheme. This
  // means the values of the NetworkAnonymizationKey are as follows:
  // `top_frame_site` -> the schemeful site of the top level page.
  // `frame_site ` -> nullopt
  // `is_cross_site` -> nullopt
  static bool IsDoubleKeySchemeEnabled();

  // Returns true if the NetworkAnonymizationKey has a <double keyed +
  // is_cross_site> scheme. This means the values of the NetworkAnonymizationKey
  // are as follows:
  // `top_frame_site` -> the schemeful site of the top level page.
  // `frame_site ` -> nullopt
  // `is_cross_site` -> a boolean indicating if the requestor frame site is
  // cross site from the top level site.
  static bool IsCrossSiteFlagSchemeEnabled();

  // Returns a representation of |this| as a base::Value. Returns false on
  // failure. Succeeds if either IsEmpty() or !IsTransient().
  [[nodiscard]] bool ToValue(base::Value* out_value) const;

  // Inverse of ToValue(). Writes the result to |network_anonymization_key|.
  // Returns false on failure. Fails on values that could not have been produced
  // by ToValue(), like transient origins.
  [[nodiscard]] static bool FromValue(
      const base::Value& value,
      NetworkAnonymizationKey* out_network_anonymization_key);

 private:
  std::string GetSiteDebugString(
      const absl::optional<SchemefulSite>& site) const;

  static absl::optional<std::string> SerializeSiteWithNonce(
      const SchemefulSite& site);

  // The origin/etld+1 of the top frame of the page making the request. This
  // will always be populated unless all other fields are also nullopt.
  absl::optional<SchemefulSite> top_frame_site_;

  // True if the frame site is cross site when compared to the top frame site.
  absl::optional<bool> is_cross_site_;

  // for non-opaque origins.
  absl::optional<base::UnguessableToken> nonce_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_ANONYMIZATION_KEY_H_
