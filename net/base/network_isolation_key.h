// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ISOLATION_KEY_H_
#define NET_BASE_NETWORK_ISOLATION_KEY_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"

namespace network {
namespace mojom {
class NetworkIsolationKeyDataView;
}  // namespace mojom
}  // namespace network

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

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
                      const SchemefulSite& frame_site);

  // Alternative constructor that takes ownership of arguments, to save copies.
  NetworkIsolationKey(SchemefulSite&& top_frame_site,
                      SchemefulSite&& frame_site);

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

  // Creates a non-empty NetworkIsolationKey with an opaque origin that is not
  // considered transient. The returned NetworkIsolationKey will be cross-origin
  // with all other keys and associated data is able to be persisted to disk.
  static NetworkIsolationKey CreateOpaqueAndNonTransient();

  // Creates a new key using |top_frame_site_| and |new_frame_site|.
  NetworkIsolationKey CreateWithNewFrameSite(
      const SchemefulSite& new_frame_site) const;

  // Creates a new key using |top_frame_site_| and |new_frame_origin|.
  // TODO(https://crbug.com/1145294):  Remove this in favor of above method.
  NetworkIsolationKey CreateWithNewFrameOrigin(
      const url::Origin& new_frame_origin) const;

  // Intended for temporary use in locations that should be using a non-empty
  // NetworkIsolationKey(), but are not yet. This both reduces the chance of
  // accidentally copying the lack of a NIK where one should be used, and
  // provides a reasonable way of locating callsites that need to have their
  // NetworkIsolationKey filled in.
  static NetworkIsolationKey Todo() { return NetworkIsolationKey(); }

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
    return std::tie(top_frame_site_, frame_site_, opaque_and_non_transient_) ==
           std::tie(other.top_frame_site_, other.frame_site_,
                    other.opaque_and_non_transient_);
  }

  // Compare keys for inequality, true if any enabled field varies.
  bool operator!=(const NetworkIsolationKey& other) const {
    return !(*this == other);
  }

  // Provide an ordering for keys based on all enabled fields.
  bool operator<(const NetworkIsolationKey& other) const {
    return std::tie(top_frame_site_, frame_site_, opaque_and_non_transient_) <
           std::tie(other.top_frame_site_, other.frame_site_,
                    other.opaque_and_non_transient_);
  }

  // Returns the string representation of the key, which is the string
  // representation of each piece of the key separated by spaces.
  std::string ToString() const;

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
  const base::Optional<SchemefulSite>& GetTopFrameSite() const {
    return top_frame_site_;
  }
  const base::Optional<SchemefulSite>& GetFrameSite() const {
    return frame_site_;
  }

  // Returns true if all parts of the key are empty.
  bool IsEmpty() const;

  // Returns a representation of |this| as a base::Value. Returns false on
  // failure. Succeeds if either IsEmpty() or !IsTransient().
  bool ToValue(base::Value* out_value) const WARN_UNUSED_RESULT;

  // Inverse of ToValue(). Writes the result to |network_isolation_key|. Returns
  // false on failure. Fails on values that could not have been produced by
  // ToValue(), like transient origins. If the value of
  // net::features::kAppendFrameOriginToNetworkIsolationKey has changed between
  // saving and loading the data, fails.
  static bool FromValue(const base::Value& value,
                        NetworkIsolationKey* out_network_isolation_key)
      WARN_UNUSED_RESULT;

 private:
  // These classes need to be able to set |opaque_and_non_transient_|
  friend class IsolationInfo;
  friend struct mojo::StructTraits<network::mojom::NetworkIsolationKeyDataView,
                                   net::NetworkIsolationKey>;

  NetworkIsolationKey(SchemefulSite&& top_frame_site,
                      SchemefulSite&& frame_site,
                      bool opaque_and_non_transient);

  bool IsOpaque() const;

  // SchemefulSite::Serialize() is not const, as it may initialize the nonce.
  // Need this to call it on a const |site|.
  static base::Optional<std::string> SerializeSiteWithNonce(
      const SchemefulSite& site);

  // Whether opaque origins cause the key to be transient. Always false, unless
  // created with |CreateOpaqueAndNonTransient|.
  bool opaque_and_non_transient_ = false;

  // Whether or not to use the |frame_site_| as part of the key.
  bool use_frame_site_;

  // The origin/etld+1 of the top frame of the page making the request.
  base::Optional<SchemefulSite> top_frame_site_;

  // The origin/etld+1 of the frame that initiates the request.
  base::Optional<SchemefulSite> frame_site_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_ISOLATION_KEY_H_
