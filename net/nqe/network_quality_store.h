// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_QUALITY_STORE_H_
#define NET_NQE_NETWORK_QUALITY_STORE_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "net/base/net_export.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_id.h"

namespace net {

namespace nqe {

namespace internal {

// NetworkQualityStore holds the network qualities of different networks in
// memory. Entries are stored in LRU order, and older entries may be evicted.
class NET_EXPORT_PRIVATE NetworkQualityStore {
 public:
  // Observes changes in the cached network qualities.
  class NET_EXPORT_PRIVATE NetworkQualitiesCacheObserver {
   public:
    // Notifies the observer of a change in the cached network quality. The
    // observer must register and unregister itself on the IO thread. All the
    // observers would be notified on the IO thread. |network_id| is the ID of
    // the network whose cached quality is being reported.
    virtual void OnChangeInCachedNetworkQuality(
        const nqe::internal::NetworkID& network_id,
        const nqe::internal::CachedNetworkQuality& cached_network_quality) = 0;

   protected:
    NetworkQualitiesCacheObserver() {}
    virtual ~NetworkQualitiesCacheObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(NetworkQualitiesCacheObserver);
  };

  NetworkQualityStore();
  ~NetworkQualityStore();

  // Stores the network quality |cached_network_quality| of network with ID
  // |network_id|.
  void Add(const nqe::internal::NetworkID& network_id,
           const nqe::internal::CachedNetworkQuality& cached_network_quality);

  // Returns true if the network quality estimate was successfully read
  // for a network with ID |network_id|, and sets |cached_network_quality| to
  // the estimate read.
  bool GetById(
      const nqe::internal::NetworkID& network_id,
      nqe::internal::CachedNetworkQuality* cached_network_quality) const;

  // Adds and removes |observer| from the list of cache observers. The
  // observers are notified on the same thread on which it was added. Addition
  // and removal of the observer must happen on the same thread.
  void AddNetworkQualitiesCacheObserver(
      NetworkQualitiesCacheObserver* observer);
  void RemoveNetworkQualitiesCacheObserver(
      NetworkQualitiesCacheObserver* observer);

  // If |disable_offline_check| is set to true, the offline check is disabled
  // when storing the network quality.
  void DisableOfflineCheckForTesting(bool disable_offline_check);

 private:
  // Maximum size of the store that holds network quality estimates.
  // A smaller size may reduce the cache hit rate due to frequent evictions.
  // A larger size may affect performance.
  static const size_t kMaximumNetworkQualityCacheSize = 20;

  // Notifies |observer| of the current effective connection type if |observer|
  // is still registered as an observer.
  void NotifyCacheObserverIfPresent(
      NetworkQualitiesCacheObserver* observer) const;

  // This does not use an unordered_map or hash_map for code simplicity (the key
  // just implements operator<, rather than hash and equality) and because the
  // map is tiny.
  typedef std::map<nqe::internal::NetworkID,
                   nqe::internal::CachedNetworkQuality>
      CachedNetworkQualities;

  // Data structure that stores the qualities of networks.
  CachedNetworkQualities cached_network_qualities_;

  // Observer list for changes in the cached network quality.
  base::ObserverList<NetworkQualitiesCacheObserver>::Unchecked
      network_qualities_cache_observer_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NetworkQualityStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityStore);
};

}  // namespace internal

}  // namespace nqe

}  // namespace net

#endif  // NET_NQE_NETWORK_QUALITY_STORE_H_
