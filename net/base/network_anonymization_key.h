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
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_partition.h"
#include "net/base/schemeful_site.h"

namespace base {
class Value;
}

namespace net {

class NetworkIsolationKey;

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

  // Create a `NetworkAnonymizationKey` from a `top_frame_site`, assuming it is
  // same-site (see comment on the class, above) and has no nonce.
  static NetworkAnonymizationKey CreateSameSite(
      const SchemefulSite& top_frame_site) {
    return NetworkAnonymizationKey(top_frame_site, false, std::nullopt,
                                   NetworkIsolationPartition::kGeneral);
  }

  // Create a `NetworkAnonymizationKey` from a `top_frame_site`, assuming it is
  // cross-site (see comment on the class, above) and has no nonce.
  static NetworkAnonymizationKey CreateCrossSite(
      const SchemefulSite& top_frame_site) {
    return NetworkAnonymizationKey(top_frame_site, true, std::nullopt,
                                   NetworkIsolationPartition::kGeneral);
  }

  // Create a `NetworkAnonymizationKey` from a `top_frame_site` and
  // `frame_site`. This calculates is_cross_site on the basis of those two
  // sites.
  static NetworkAnonymizationKey CreateFromFrameSite(
      const SchemefulSite& top_frame_site,
      const SchemefulSite& frame_site,
      std::optional<base::UnguessableToken> nonce = std::nullopt,
      NetworkIsolationPartition network_isolation_partition =
          NetworkIsolationPartition::kGeneral);

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
      std::optional<base::UnguessableToken> nonce = std::nullopt,
      NetworkIsolationPartition network_isolation_partition =
          NetworkIsolationPartition::kGeneral) {
    return NetworkAnonymizationKey(top_frame_site, is_cross_site, nonce,
                                   network_isolation_partition);
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
    return data_->top_frame_site();
  }

  bool IsCrossSite() const { return data_->is_cross_site(); }

  bool IsSameSite() const { return !IsCrossSite(); }

  const std::optional<base::UnguessableToken>& GetNonce() const {
    return data_->nonce();
  }

  net::NetworkIsolationPartition network_isolation_partition() const {
    return data_->network_isolation_partition();
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
  // Holds all the data of a NetworkAnonymizationKey. This is ref-counted to
  // make copying NetworkAnonymizationKey objects cheaper.
  class Data : public base::RefCountedThreadSafe<Data> {
   public:
    static scoped_refptr<Data> GetEmptyData();

    // Conctruct an empty data.
    explicit Data(base::PassKey<Data>);

    Data(const std::optional<SchemefulSite>& top_frame_site,
         bool is_cross_site,
         std::optional<base::UnguessableToken> nonce,
         NetworkIsolationPartition network_isolation_partition);

    // The origin/etld+1 of the top frame of the page making the request.
    const std::optional<SchemefulSite>& top_frame_site() const {
      return top_frame_site_;
    }

    // True if the frame site is cross site when compared to the top frame site.
    bool is_cross_site() const { return is_cross_site_; }

    // for non-opaque origins.
    const std::optional<base::UnguessableToken>& nonce() const {
      return nonce_;
    }

    NetworkIsolationPartition network_isolation_partition() const {
      return network_isolation_partition_;
    }

    bool is_empty() const { return !top_frame_site_.has_value(); }

    friend bool operator==(const Data& a, const Data& b) {
      return std::tie(a.top_frame_site_, a.is_cross_site_, a.nonce_,
                      a.network_isolation_partition_) ==
             std::tie(b.top_frame_site_, b.is_cross_site_, b.nonce_,
                      b.network_isolation_partition_);
    }

    friend auto operator<=>(const Data& a, const Data& b) {
      return std::tie(a.top_frame_site_, a.is_cross_site_, a.nonce_,
                      a.network_isolation_partition_) <=>
             std::tie(b.top_frame_site_, b.is_cross_site_, b.nonce_,
                      b.network_isolation_partition_);
    }

    template <typename H>
    friend H AbslHashValue(H h, const Data& data) {
      return H::combine(std::move(h), data.top_frame_site_, data.is_cross_site_,
                        data.nonce_, data.network_isolation_partition_);
    }

   private:
    friend class base::RefCountedThreadSafe<Data>;
    ~Data();

    const std::optional<SchemefulSite> top_frame_site_;
    const bool is_cross_site_;
    const std::optional<base::UnguessableToken> nonce_;
    const NetworkIsolationPartition network_isolation_partition_;
  };

 public:
  // Compare keys for equality, true if all enabled fields are equal.
  friend bool operator==(const NetworkAnonymizationKey& a,
                         const NetworkAnonymizationKey& b) {
    return *a.data_ == *b.data_;
  }

  // Provide an ordering for keys based on all enabled fields.
  friend auto operator<=>(const NetworkAnonymizationKey& a,
                          const NetworkAnonymizationKey& b) {
    return *a.data_ <=> *b.data_;
  }

  template <typename H>
  friend H AbslHashValue(
      H h,
      const NetworkAnonymizationKey& network_anonymization_key) {
    return H::combine(std::move(h), *network_anonymization_key.data_);
  }

 private:
  NetworkAnonymizationKey(
      const std::optional<SchemefulSite>& top_frame_site,
      bool is_cross_site,
      std::optional<base::UnguessableToken> nonce = std::nullopt,
      NetworkIsolationPartition network_isolation_partition =
          NetworkIsolationPartition::kGeneral);

  std::string GetSiteDebugString(
      const std::optional<SchemefulSite>& site) const;

  static std::optional<std::string> SerializeSiteWithNonce(
      const SchemefulSite& site);

  // A non-null Data.
  scoped_refptr<const Data> data_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const NetworkAnonymizationKey& nak);

}  // namespace net

#endif  // NET_BASE_NETWORK_ANONYMIZATION_KEY_H_
