// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PREFETCH_CACHE_H_
#define SERVICES_NETWORK_PREFETCH_CACHE_H_

#include <map>
#include <set>
#include <utility>

#include "base/component_export.h"
#include "base/containers/linked_list.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "url/gurl.h"

namespace net {
class NetworkIsolationKey;
}  // namespace net

namespace network {

class PrefetchURLLoaderClient;
struct ResourceRequest;

// PrefetchCache implements caching of PrefetchURLLoaderClient objects for
// NetworkContexts. There is at most one created per NetworkContext.
class COMPONENT_EXPORT(NETWORK_SERVICE) PrefetchCache final {
 public:
  // The maximum age a prefetch is permitted to live for without being claimed
  // by a render process.
  // TODO(crbug.com/342445996): Allow this to be set dynamically by a feature
  // param.
  static constexpr auto kMaxAge = base::Minutes(5);

  // If several prefetches are created close together, it is wasteful to wake up
  // once for each one, so permit prefetches that have reached
  // `kMaxAge` - `kExpirySlack` to also be expired.
  // TODO(crbug.com/342445996): Allow this to be set dynamically by a feature
  // param.
  static constexpr auto kExpirySlack = base::Seconds(1);

  PrefetchCache();

  PrefetchCache(const PrefetchCache&) = delete;
  PrefetchCache& operator=(const PrefetchCache&) = delete;

  ~PrefetchCache();

  // Instantiates and inserts a new PrefetchURLLoaderClient. If a matching
  // request (same NIK and URL) already exists in the cache, returns nullptr.
  // The returned PrefetchURLLoaderClient is owned by this object. The returned
  // pointer is safe to use synchronously as long as nothing calls Erase() on
  // it, but it should not be used again after returning to the run loop as it
  // might be deleted asynchronously.
  PrefetchURLLoaderClient* Emplace(const ResourceRequest& request);

  // Finds a PrefetchURLLoaderClient matching `nik` and `url`. Returns nullptr
  // if nothing is found. As with Emplace(), the returned pointer should only be
  // used synchronously.
  PrefetchURLLoaderClient* Lookup(const net::NetworkIsolationKey& nik,
                                  const GURL& url);

  // Prevents `client` from being returned by future calls to Lookup() and
  // permits a new request with the same key to be created by Emplace().
  // `client` must exist in the cache.
  void Consume(PrefetchURLLoaderClient* client);

  // Removes `client` from the cache and deletes it. `client` must have been
  // created by Emplace() and not already erased.
  void Erase(PrefetchURLLoaderClient* client);

 private:
  // This is not a std::list because we want to be able to remove an item from
  // the cache by pointer.
  using ListType = base::LinkedList<PrefetchURLLoaderClient>;

  // The references in the KeyType point inside the PrefetchURLLoaderClient that
  // is the value of the map, so they are guaranteed to be valid.
  using KeyType = std::pair<const net::NetworkIsolationKey&, const GURL&>;

  // The value_type of the map is const to reduce the risk of accidentally
  // changing it, since the key's references point into the value.
  using MapType = std::map<KeyType, const raw_ptr<PrefetchURLLoaderClient>>;

  using ClientStorage = std::set<std::unique_ptr<PrefetchURLLoaderClient>,
                                 base::UniquePtrComparator>;

  // Deletes any expired cache entries and then restarts the timer if needed.
  void OnTimer();

  // Removes and deletes the oldest entry from the cache.
  void EraseOldest();

  // Removes an entry from the cache without deleting it. `client` must be in
  // the cache.
  void RemoveFromCache(PrefetchURLLoaderClient* client);

  // Removes `client` from `client_storage_`, deleting it. `client` must exist.
  void EraseFromStorage(PrefetchURLLoaderClient* client);

  // Starts the timer to fire when the next cache entry will expire. `now`
  // should be the current time. It is optional because some callers have it
  // handy and some don't.
  void StartTimer(base::TimeTicks now = base::TimeTicks::Now());

  // Storage for all the PrefetchURLLoaderClients created by this object,
  // regardless if Consume() has been called for them or not. `list_` and `map_`
  // contain references into these objects, so must be destroyed first.
  ClientStorage client_storage_;

  // The PrefetchURLLoaderClients are stored in a list to permit deletion and
  // finding the oldest in O(1) time. `list_.head()` is the oldest item and
  // `list_.tail()` is the newest. `list_` does not own the objects. They are
  // owned by `client_storage_`.
  ListType list_;

  // They are referenced from a map, permitting O(log N) lookup.
  MapType map_;

  // Timer. If `list_` is non-empty, it is set to go off when the oldest item in
  // `list_` will expire.
  base::OneShotTimer expiry_timer_;

  // Initialized from kNetworkContextPrefetchMaxLoaders feature flag.
  const size_t max_size_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PREFETCH_CACHE_H_
