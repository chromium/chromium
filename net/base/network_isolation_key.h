// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ISOLATION_KEY_H_
#define NET_BASE_NETWORK_ISOLATION_KEY_H_

#include <optional>
#include <string>

#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"

namespace url {
class Origin;
}

namespace network::mojom {
class NonEmptyNetworkIsolationKeyDataView;
}

namespace net {
class CookiePartitionKey;
class NetworkAnonymizationKey;
}

namespace net {

// NetworkIsolationKey (NIK) is used to partition shared network state based on
// the context in which the requests were made. It is used to divide the HTTP
// cache, while the NetworkAnonymizationKey is used for most other network
// state.
//
// The precise form of the NIK is still subject to experimentation. See
// `network_anonymization_key.h` for details on keying terminology.
class NET_EXPORT NetworkIsolationKey {
 public:
  // Full constructor.  When a request is initiated by the top frame, it must
  // also populate the |frame_site| parameter when calling this constructor.
  NetworkIsolationKey(
      const SchemefulSite& top_frame_site,
      const SchemefulSite& frame_site,
      const std::optional<base::UnguessableToken>& nonce = std::nullopt);

  // Alternative constructor that takes ownership of arguments, to save copies.
  NetworkIsolationKey(
      SchemefulSite&& top_frame_site,
      SchemefulSite&& frame_site,
      std::optional<base::UnguessableToken>&& nonce = std::nullopt);

  // Legacy constructor.
  // TODO(crbug.com/40729378):  Remove this in favor of above
  // constructor.
  NetworkIsolationKey(const url::Origin& top_frame_origin,
                      const url::Origin& frame_origin);

  // Construct an empty key.
  NetworkIsolationKey();

  NetworkIsolationKey(const NetworkIsolationKey& network_isolation_key);
  NetworkIsolationKey(NetworkIsolationKey&& network_isolation_key);

  ~NetworkIsolationKey();

  NetworkIsolationKey& operator=(
      const NetworkIsolationKey& network_isolation_key);
  NetworkIsolationKey& operator=(NetworkIsolationKey&& network_isolation_key);

  // Creates a transient non-empty NetworkIsolationKey by creating an opaque
  // origin. This prevents the NetworkIsolationKey from sharing data with other
  // NetworkIsolationKeys. Data for transient NetworkIsolationKeys is not
  // persisted to disk.
  static NetworkIsolationKey CreateTransientForTesting();

  // Creates a new key using |top_frame_site_| and |new_frame_site|.
  NetworkIsolationKey CreateWithNewFrameSite(
      const SchemefulSite& new_frame_site) const;

  // Compare keys for equality, true if all enabled fields are equal.
  bool operator==(const NetworkIsolationKey& other) const {
    switch (GetMode()) {
      case Mode::kFrameSiteWithSharedOpaqueEnabled:
        if ((frame_site_ && frame_site_->opaque()) &&
            (other.frame_site_ && other.frame_site_->opaque())) {
          return std::tie(top_frame_site_, nonce_) ==
                 std::tie(other.top_frame_site_, other.nonce_);
        }
        [[fallthrough]];
      case Mode::kFrameSiteEnabled:
        return std::tie(top_frame_site_, frame_site_, nonce_) ==
               std::tie(other.top_frame_site_, other.frame_site_, other.nonce_);
      case Mode::kCrossSiteFlagEnabled:
        return std::tie(top_frame_site_, is_cross_site_, nonce_) ==
               std::tie(other.top_frame_site_, other.is_cross_site_,
                        other.nonce_);
    }
  }

  // Compare keys for inequality, true if any enabled field varies.
  bool operator!=(const NetworkIsolationKey& other) const {
    return !(*this == other);
  }

  // Provide an ordering for keys based on all enabled fields.
  bool operator<(const NetworkIsolationKey& other) const {
    switch (GetMode()) {
      case Mode::kFrameSiteWithSharedOpaqueEnabled:
        if ((frame_site_ && frame_site_->opaque()) &&
            (other.frame_site_ && other.frame_site_->opaque())) {
          return std::tie(top_frame_site_, nonce_) <
                 std::tie(other.top_frame_site_, other.nonce_);
        }
        [[fallthrough]];
      case Mode::kFrameSiteEnabled:
        return std::tie(top_frame_site_, frame_site_, nonce_) <
               std::tie(other.top_frame_site_, other.frame_site_, other.nonce_);
      case Mode::kCrossSiteFlagEnabled:
        return std::tie(top_frame_site_, is_cross_site_, nonce_) <
               std::tie(other.top_frame_site_, other.is_cross_site_,
                        other.nonce_);
    }
  }

  // Returns the string representation of the key for use in string-keyed disk
  // cache. This is the string representation of each piece of the key separated
  // by spaces. Returns nullopt if the network isolation key is transient, in
  // which case, nothing should typically be saved to disk using the key.
  std::optional<std::string> ToCacheKeyString() const;

  // Returns string for debugging. Difference from ToString() is that transient
  // entries may be distinguishable from each other.
  std::string ToDebugString() const;

  // Returns true if all parts of the key are non-empty.
  bool IsFullyPopulated() const;

  // Returns true if this key's lifetime is short-lived, or if
  // IsFullyPopulated() returns true. It may not make sense to persist state to
  // disk related to it (e.g., disk cache).
  bool IsTransient() const;

  // Getters for the top frame and frame sites. These accessors are primarily
  // intended for IPC calls, and to be able to create an IsolationInfo from a
  // NetworkIsolationKey.
  const std::optional<SchemefulSite>& GetTopFrameSite() const {
    return top_frame_site_;
  }

  enum class Mode {
    // This scheme indicates that "triple-key" NetworkIsolationKeys are used to
    // partition the HTTP cache. This key will have the following properties:
    // `top_frame_site` -> the schemeful site of the top level page.
    // `frame_site ` -> the schemeful site of the frame.
    // `is_cross_site` -> std::nullopt.
    kFrameSiteEnabled,
    // This scheme indicates that "2.5-key" NetworkIsolationKeys are used to
    // partition the HTTP cache. This key will have the following properties:
    // `top_frame_site_` -> the schemeful site of the top level page.
    // `frame_site_` -> should only be accessed for serialization or building
    // nonced CookiePartitionKeys.
    // `is_cross_site_` -> a boolean indicating whether the frame site is
    // schemefully cross-site from the top-level site.
    kCrossSiteFlagEnabled,
    // This scheme indicates that "triple-key" NetworkIsolationKeys are used to
    // partition the HTTP cache except when the frame site has an opaque origin.
    // In that case, a fixed value will be used instead of the frame site such
    // that all opaque origin frames under a given top-level site share a cache
    // partition (unless the NIK is explicitly provided a nonce).
    kFrameSiteWithSharedOpaqueEnabled,
  };

  // Returns the cache key scheme currently in use.
  static Mode GetMode();

  // Do not use outside of testing. Returns the `frame_site_`.
  const std::optional<SchemefulSite> GetFrameSiteForTesting() const {
    if (GetMode() == Mode::kFrameSiteEnabled ||
        GetMode() == Mode::kFrameSiteWithSharedOpaqueEnabled) {
      return frame_site_;
    }
    return std::nullopt;
  }

  // Do not use outside of testing. Returns `is_cross_site_`.
  const std::optional<bool> GetIsCrossSiteForTesting() const {
    if (GetMode() == Mode::kCrossSiteFlagEnabled) {
      return is_cross_site_;
    }
    return std::nullopt;
  }

  // When serializing a NIK for sending via mojo we want to access the frame
  // site directly. We don't want to expose this broadly, though, hence the
  // passkey.
  using SerializationPassKey = base::PassKey<struct mojo::StructTraits<
      network::mojom::NonEmptyNetworkIsolationKeyDataView,
      NetworkIsolationKey>>;
  const std::optional<SchemefulSite>& GetFrameSiteForSerialization(
      SerializationPassKey) const {
    CHECK(!IsEmpty());
    return frame_site_;
  }
  // We also need to access the frame site directly when constructing
  // CookiePartitionKey for nonced partitions. We also use a passkey for this
  // case.
  using CookiePartitionKeyPassKey = base::PassKey<CookiePartitionKey>;
  const std::optional<SchemefulSite>& GetFrameSiteForCookiePartitionKey(
      CookiePartitionKeyPassKey) const {
    CHECK(!IsEmpty());
    return frame_site_;
  }
  // Same as above but for constructing a `NetworkAnonymizationKey()` from this
  // NIK.
  using NetworkAnonymizationKeyPassKey = base::PassKey<NetworkAnonymizationKey>;
  const std::optional<SchemefulSite>& GetFrameSiteForNetworkAnonymizationKey(
      NetworkAnonymizationKeyPassKey) const {
    CHECK(!IsEmpty());
    return frame_site_;
  }

  // Getter for the nonce.
  const std::optional<base::UnguessableToken>& GetNonce() const {
    return nonce_;
  }

  // Returns true if all parts of the key are empty.
  bool IsEmpty() const;

 private:
  // Whether this key has opaque origins or a nonce.
  bool IsOpaque() const;

  // The origin/etld+1 of the top frame of the page making the request.
  std::optional<SchemefulSite> top_frame_site_;

  // The origin/etld+1 of the frame that initiates the request.
  std::optional<SchemefulSite> frame_site_;

  // A boolean indicating whether the frame origin is cross-site from the
  // top-level origin. This will be used for experiments to determine the
  // the difference in performance between partitioning the HTTP cache using the
  // top-level origin and frame origin ("triple-keying") vs. the top-level
  // origin and an is-cross-site bit ("2.5-keying") like the
  // `NetworkAnonymizationKey` uses for network state partitioning. This will be
  // std::nullopt when `GetMode()` returns `Mode::kFrameSiteEnabled` or
  // `Mode::kFrameSiteWithSharedOpaqueEnabled`, or for an empty
  // `NetworkIsolationKey`.
  std::optional<bool> is_cross_site_;

  // Having a nonce is a way to force a transient opaque `NetworkIsolationKey`
  // for non-opaque origins.
  std::optional<base::UnguessableToken> nonce_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const NetworkIsolationKey& nak);

}  // namespace net

#endif  // NET_BASE_NETWORK_ISOLATION_KEY_H_
