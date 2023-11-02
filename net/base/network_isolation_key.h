// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ISOLATION_KEY_H_
#define NET_BASE_NETWORK_ISOLATION_KEY_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}

namespace url {
class Origin;
}

namespace net {

// Key used to isolate shared network stack resources used by requests based on
// the context on which they were made.
class NET_EXPORT NetworkIsolationKey {
 public:
  // Full constructor.  When a request is initiated by the top frame, it must
  // also populate the |frame_site| parameter when calling this constructor.
  NetworkIsolationKey(const SchemefulSite& top_frame_site,
                      const SchemefulSite& frame_site,
                      const base::UnguessableToken* nonce = nullptr);

  // Alternative constructor that takes ownership of arguments, to save copies.
  NetworkIsolationKey(SchemefulSite&& top_frame_site,
                      SchemefulSite&& frame_site,
                      const base::UnguessableToken* nonce = nullptr);

  // Legacy constructor.
  // TODO(https://crbug.com/1145294):  Remove this in favor of above
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
  static NetworkIsolationKey CreateTransient();

  // Creates a new key using |top_frame_site_| and |new_frame_site|.
  NetworkIsolationKey CreateWithNewFrameSite(
      const SchemefulSite& new_frame_site) const;

  // Intended for temporary use in locations that should be using main frame and
  // frame origin, but are currently only using frame origin, because the
  // creating object may be shared across main frame objects. Having a special
  // constructor for these methods makes it easier to keep track of locating
  // callsites that need to have their NetworkIsolationKey filled in.
  static NetworkIsolationKey ToDoUseTopFrameOriginAsWell(
      const url::Origin& incorrectly_used_frame_origin) {
    return NetworkIsolationKey(incorrectly_used_frame_origin,
                               incorrectly_used_frame_origin);
  }

  // Compare keys for equality, true if all enabled fields are equal.
  bool operator==(const NetworkIsolationKey& other) const {
    return std::tie(top_frame_site_, frame_site_, nonce_) ==
           std::tie(other.top_frame_site_, other.frame_site_, other.nonce_);
  }

  // Compare keys for inequality, true if any enabled field varies.
  bool operator!=(const NetworkIsolationKey& other) const {
    return !(*this == other);
  }

  // Provide an ordering for keys based on all enabled fields.
  bool operator<(const NetworkIsolationKey& other) const {
    return std::tie(top_frame_site_, frame_site_, nonce_) <
           std::tie(other.top_frame_site_, other.frame_site_, other.nonce_);
  }

  // Returns the string representation of the key for use in string-keyed disk
  // cache. This is the string representation of each piece of the key separated
  // by spaces. Returns nullopt if the network isolation key is transient, in
  // which case, nothing should typically be saved to disk using the key.
  absl::optional<std::string> ToCacheKeyString() const;

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
  const absl::optional<SchemefulSite>& GetTopFrameSite() const {
    return top_frame_site_;
  }

  const absl::optional<SchemefulSite>& GetFrameSite() const;

  // Do not use outside of testing. Returns the `frame_site_` if
  // `kForceIsolationInfoFrameOriginToTopLevelFrame` is disabled. Else it
  // returns nullopt.
  const absl::optional<SchemefulSite>& GetFrameSiteForTesting() const {
    return frame_site_;
  }

  // Getter for the nonce.
  const absl::optional<base::UnguessableToken>& GetNonce() const {
    return nonce_;
  }

  // Returns true if all parts of the key are empty.
  bool IsEmpty() const;

  // Returns true if the NetworkIsolationKey has a triple keyed scheme. This
  // means both `frame_site_` and `top_frame_site_` are populated.
  static bool IsFrameSiteEnabled();

  // Returns a representation of |this| as a base::Value. Returns false on
  // failure. Succeeds if either IsEmpty() or !IsTransient().
  [[nodiscard]] bool ToValue(base::Value* out_value) const;

  // Inverse of ToValue(). Writes the result to |network_isolation_key|. Returns
  // false on failure. Fails on values that could not have been produced by
  // ToValue(), like transient origins. If the value of
  // net::features::kAppendFrameOriginToNetworkIsolationKey has changed between
  // saving and loading the data, fails.
  [[nodiscard]] static bool FromValue(
      const base::Value& value,
      NetworkIsolationKey* out_network_isolation_key);

 private:
  // Whether this key has opaque origins or a nonce.
  bool IsOpaque() const;

  // SchemefulSite::Serialize() is not const, as it may initialize the nonce.
  // Need this to call it on a const |site|.
  static absl::optional<std::string> SerializeSiteWithNonce(
      const SchemefulSite& site);

  // The origin/etld+1 of the top frame of the page making the request.
  absl::optional<SchemefulSite> top_frame_site_;

  // The origin/etld+1 of the frame that initiates the request.
  absl::optional<SchemefulSite> frame_site_;

  // Having a nonce is a way to force a transient opaque `NetworkIsolationKey`
  // for non-opaque origins.
  absl::optional<base::UnguessableToken> nonce_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_ISOLATION_KEY_H_
