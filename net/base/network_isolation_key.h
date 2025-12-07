// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ISOLATION_KEY_H_
#define NET_BASE_NETWORK_ISOLATION_KEY_H_

#include <optional>
#include <string>
#include <tuple>

#include "base/memory/ref_counted.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_partition.h"
#include "net/base/schemeful_site.h"

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
// The NetworkIsolationKey uses the "triple-key" scheme to partition the HTTP
// cache. The key has the following properties:
// `top_frame_site` -> the schemeful site of the top level page.
// `frame_site ` -> the schemeful site of the frame.
// `network_isolation_partition` -> an extra partition for the HTTP cache for
// special use cases.
class NET_EXPORT NetworkIsolationKey {
 public:
  // Full constructor.  When a request is initiated by the top frame, it must
  // also populate the |frame_site| parameter when calling this constructor.
  NetworkIsolationKey(
      const SchemefulSite& top_frame_site,
      const SchemefulSite& frame_site,
      const std::optional<base::UnguessableToken>& nonce = std::nullopt,
      NetworkIsolationPartition network_isolation_partition =
          NetworkIsolationPartition::kGeneral);

  // Alternative constructor that takes ownership of arguments, to save copies.
  NetworkIsolationKey(
      SchemefulSite&& top_frame_site,
      SchemefulSite&& frame_site,
      std::optional<base::UnguessableToken>&& nonce = std::nullopt,
      NetworkIsolationPartition network_isolation_partition =
          NetworkIsolationPartition::kGeneral);

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
    return data_->top_frame_site();
  }

  // Do not use outside of testing. Returns the `frame_site_`.
  const std::optional<SchemefulSite> GetFrameSiteForTesting() const {
    return GetFrameSite();
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
    return data_->frame_site();
  }
  // We also need to access the frame site directly when constructing
  // CookiePartitionKey for nonced partitions. We also use a passkey for this
  // case.
  using CookiePartitionKeyPassKey = base::PassKey<CookiePartitionKey>;
  const std::optional<SchemefulSite>& GetFrameSiteForCookiePartitionKey(
      CookiePartitionKeyPassKey) const {
    CHECK(!IsEmpty());
    return data_->frame_site();
  }
  // Same as above but for constructing a `NetworkAnonymizationKey()` from this
  // NIK.
  using NetworkAnonymizationKeyPassKey = base::PassKey<NetworkAnonymizationKey>;
  const std::optional<SchemefulSite>& GetFrameSiteForNetworkAnonymizationKey(
      NetworkAnonymizationKeyPassKey) const {
    return data_->frame_site();
  }

  // Getter for the nonce.
  const std::optional<base::UnguessableToken>& GetNonce() const {
    return data_->nonce();
  }

  NetworkIsolationPartition GetNetworkIsolationPartition() const {
    return data_->network_isolation_partition();
  }

  // Returns true if all parts of the key are empty.
  bool IsEmpty() const;

 private:
  // Holds all the data of a NetworkIsolationKey. This is ref-counted to make
  // copying NetworkIsolationKey objects cheaper.
  class Data : public base::RefCountedThreadSafe<Data> {
   public:
    static scoped_refptr<Data> GetEmptyData();

    // Conctruct an empty data.
    explicit Data(base::PassKey<Data>);

    Data(SchemefulSite&& top_frame_site,
         SchemefulSite&& frame_site,
         std::optional<base::UnguessableToken>&& nonce,
         NetworkIsolationPartition network_isolation_partition);

    // The origin/etld+1 of the top frame of the page making the request.
    const std::optional<SchemefulSite>& top_frame_site() const {
      return top_frame_site_;
    }

    // The origin/etld+1 of the frame that initiates the request.
    const std::optional<SchemefulSite>& frame_site() const {
      return frame_site_;
    }

    // Having a nonce is a way to force a transient opaque `NetworkIsolationKey`
    // for non-opaque origins.
    const std::optional<base::UnguessableToken>& nonce() const {
      return nonce_;
    }

    // The network isolation partition for this NIK. This will be kGeneral
    // except for specific use cases that require isolation from all other
    // use cases.
    NetworkIsolationPartition network_isolation_partition() const {
      return network_isolation_partition_;
    }

    bool is_empty() const { return !top_frame_site_.has_value(); }

    // Compare keys for equality, true if all enabled fields are equal.
    friend bool operator==(const Data& a, const Data& b) {
      return std::tie(a.top_frame_site_, a.frame_site_, a.nonce_,
                      a.network_isolation_partition_) ==
             std::tie(b.top_frame_site_, b.frame_site_, b.nonce_,
                      b.network_isolation_partition_);
    }

    // Provide an ordering for keys based on all enabled fields.
    friend auto operator<=>(const Data& a, const Data& b) {
      return std::tie(a.top_frame_site_, a.frame_site_, a.nonce_,
                      a.network_isolation_partition_) <=>
             std::tie(b.top_frame_site_, b.frame_site_, b.nonce_,
                      b.network_isolation_partition_);
    }

   private:
    friend class base::RefCountedThreadSafe<Data>;
    ~Data();

    const std::optional<SchemefulSite> top_frame_site_;
    const std::optional<SchemefulSite> frame_site_;
    const std::optional<base::UnguessableToken> nonce_;
    const NetworkIsolationPartition network_isolation_partition_;
  };

 public:
  // Compare keys for equality, true if all enabled fields are equal.
  friend bool operator==(const NetworkIsolationKey& a,
                         const NetworkIsolationKey& b) {
    return *a.data_ == *b.data_;
  }

  // Provide an ordering for keys based on all enabled fields.
  friend auto operator<=>(const NetworkIsolationKey& a,
                          const NetworkIsolationKey& b) {
    return *a.data_ <=> *b.data_;
  }

 private:
  // Construct a key using the NetworkIsolationKey::Data.
  explicit NetworkIsolationKey(const scoped_refptr<const Data>& data);

  template <typename H>
  friend H AbslHashValue(H h, const NetworkIsolationKey& key) {
    return H::combine(std::move(h), key.GetTopFrameSite(), key.GetFrameSite(),
                      key.GetNonce(), key.GetNetworkIsolationPartition());
  }

  // Whether this key has opaque origins or a nonce.
  bool IsOpaque() const;

  const std::optional<SchemefulSite>& GetFrameSite() const {
    return data_->frame_site();
  }

  // A non-null Data.
  scoped_refptr<const Data> data_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const NetworkIsolationKey& nak);

}  // namespace net

#endif  // NET_BASE_NETWORK_ISOLATION_KEY_H_
