// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PERMISSIONS_CACHE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PERMISSIONS_CACHE_H_

#include "content/common/content_export.h"

#include <map>
#include <memory>
#include <tuple>

#include "base/containers/lru_cache.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/network_isolation_key.h"
#include "url/origin.h"

namespace content {

// Cache of per-origin join/leave interest group permissions, partitioned by
// NetworkIsolationKey and initiating frame origin. To minimize leaks across
// first party contexts, uses a separate LRU cache "shard" for each {frame
// origin, NetworkIsolationKey} pair. Each LRU cache has a maximum number of
// entries, but there's no global limit, to prevent leaks. The short entry
// lifetime and delay in creating entries, along with the LRU size cap, should
// keep memory usage low.
//
// In addition to the LRU-ness of the cache, individual entries are passively
// expired (i.e., no longer returned, but not proactively deleted) after
// `kCacheDuration` has passed since they were added to the cache. Cache shards
// are garbage collected on a timer, at some point after all their entries have
// expired. Expired entries and shards are also deleted on access, which isn't
// strictly necessary.
//
// Permissions are typically learned from .well-known fetches, and should be
// cached by the network process, depending on HTTP response headers, but
// caching it in the browser process results in much better performance for
// sites that want to join (or, more likely, leave) large numbers of
// cross-origin interest groups, due to the limit of outstanding joins/leaves in
// the renderer process, and overhead on accessing the network stack's HTTP
// cache.
class CONTENT_EXPORT InterestGroupPermissionsCache {
 public:
  // Cache duration for cache entries. Cache shards expire when all their
  // individual entries expire, so this is also the expiration duration of
  // cache shards, relative to last added entry.
  static constexpr base::TimeDelta kCacheDuration = base::Minutes(1);

  // How often expired cache shards are evicted. Expired but not-yet-evicted
  // entries are not returned.
  static constexpr base::TimeDelta kDeleteExpiredTimerDuration =
      2 * kCacheDuration;

  // The maximum number of cache entries for each cache shard. There is no
  // global maximum, to prevent that from becoming a sidechannel.
  static constexpr int kMaxCacheEntriesPerShard = 50;

  // Permissions associated with an interest group origin.
  struct Permissions {
    bool can_join = false;
    bool can_leave = false;

    // Comparison operators are only useful for testing.

    bool operator==(Permissions permissions) const {
      return can_join == permissions.can_join &&
             can_leave == permissions.can_leave;
    }

    bool operator!=(Permissions permissions) const {
      return !(*this == permissions);
    }
  };

  InterestGroupPermissionsCache();
  InterestGroupPermissionsCache(InterestGroupPermissionsCache&) = delete;
  ~InterestGroupPermissionsCache();

  InterestGroupPermissionsCache& operator=(
      const InterestGroupPermissionsCache&) = delete;

  // Retrieves unexpired cached Permissions that `interest_group_owner` has
  // granted to `frame_origin`, learned in the context of
  // `network_isolation_key`.
  Permissions* GetPermissions(
      const url::Origin& frame_origin,
      const url::Origin& interest_group_owner,
      const net::NetworkIsolationKey& network_isolation_key);

  // Adds `permissions` to the cache as the set of permissions
  // `interest_group_owner` has granted to `frame_origin`, learned in the
  // context of `network_isolation_key`.
  void CachePermissions(Permissions permissions,
                        const url::Origin& frame_origin,
                        const url::Origin& interest_group_owner,
                        const net::NetworkIsolationKey& network_isolation_key);

  // Deletes all cache entries.
  void Clear();

  // Returns the number of non-deleted cache shards, to test cleanup logic. All
  // values in a cache shard may be expired, since expired cache shards are only
  // deleted on access and on a timer.
  size_t cache_shards_for_testing() const;

 private:
  // An entry in the cache.
  struct CacheEntry {
    base::TimeTicks expiry;
    Permissions permissions;
  };

  // An independent shard of the cache. Each shard must have no impact on the
  // others, including when their entries are observably evicted.
  struct CacheShard {
    CacheShard();
    CacheShard(CacheShard&&);
    ~CacheShard();

    // Last expiration time of any of the shard's CacheEntry.
    base::TimeTicks expiry;

    // Map of interest group owner origins to CacheEntries. A std::unique_ptr
    // wrapper is needed to make these movable, so they can be put in a
    // std::map.
    std::unique_ptr<base::LRUCache<url::Origin, CacheEntry>> cache;
  };

  // Key indicating which cache shard to use.
  struct CacheShardKey {
    bool operator<(const CacheShardKey& other) const {
      return std::tie(frame_origin, network_isolation_key) <
             std::tie(other.frame_origin, other.network_isolation_key);
    }

    url::Origin frame_origin;
    net::NetworkIsolationKey network_isolation_key;
  };

  // Returns the specified cache shard, if it exists and is not expired. Deletes
  // the cache shard if it exists but is expired.
  CacheShard* FindShard(const url::Origin& frame_origin,
                        const net::NetworkIsolationKey& network_isolation_key,
                        base::TimeTicks now);

  // Starts a timer to invoke DeleteExpired(), if there are any cache shards and
  // the timer hasn't already been started.
  void MaybeStartDeleteExpiredTimer();

  // Walks through the cache shards, deleting any that have expired, and
  // restarts timer if there are any shards left. Expired LRU entries are not
  // deleted on an individual basis, except when accessed directly.
  void DeleteExpired();

  base::OneShotTimer delete_expired_timer_;
  std::map<CacheShardKey, CacheShard> cache_shards_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PERMISSIONS_CACHE_H_
