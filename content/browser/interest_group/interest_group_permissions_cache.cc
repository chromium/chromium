// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_permissions_cache.h"

#include <map>
#include <memory>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/network_isolation_key.h"
#include "url/origin.h"

namespace content {

InterestGroupPermissionsCache::InterestGroupPermissionsCache() = default;
InterestGroupPermissionsCache::~InterestGroupPermissionsCache() = default;

InterestGroupPermissionsCache::Permissions*
InterestGroupPermissionsCache::GetPermissions(
    const url::Origin& frame_origin,
    const url::Origin& interest_group_owner,
    const net::NetworkIsolationKey& network_isolation_key) {
  base::TimeTicks now = base::TimeTicks::Now();
  CacheShard* shard = FindShard(frame_origin, network_isolation_key, now);
  if (!shard)
    return nullptr;

  auto cache_entry = shard->cache->Get(interest_group_owner);
  if (cache_entry == shard->cache->end())
    return nullptr;
  if (cache_entry->second.expiry < now) {
    // Delete the LRU cache entry if it has expired.
    shard->cache->Erase(cache_entry);
    return nullptr;
  }
  return &cache_entry->second.permissions;
}

void InterestGroupPermissionsCache::CachePermissions(
    Permissions permissions,
    const url::Origin& frame_origin,
    const url::Origin& interest_group_owner,
    const net::NetworkIsolationKey& network_isolation_key) {
  base::TimeTicks now = base::TimeTicks::Now();
  // Use FindShard() here to remove shard if it has expired. Reusing expired
  // entries that haven't been cleaned up yet would potentially leak details
  // about when the cleanup expired task has last run, which can be influenced
  // by calls made from cross-origin renderers.
  CacheShard* shard = FindShard(frame_origin, network_isolation_key, now);
  if (!shard) {
    shard = &cache_shards_
                 .emplace(std::make_pair(
                     CacheShardKey{frame_origin, network_isolation_key},
                     CacheShard()))
                 .first->second;
  }
  base::TimeTicks expiry = now + kCacheDuration;
  shard->cache->Put({interest_group_owner, CacheEntry{expiry, permissions}});
  // Update the shard expiry to match that of the newly added CacheEntry, as
  // that should be longest lived entry.
  shard->expiry = expiry;

  MaybeStartDeleteExpiredTimer();
}

void InterestGroupPermissionsCache::Clear() {
  cache_shards_.clear();
}

size_t InterestGroupPermissionsCache::cache_shards_for_testing() const {
  return cache_shards_.size();
}

InterestGroupPermissionsCache::CacheShard::CacheShard()
    : cache(std::make_unique<base::LRUCache<url::Origin, CacheEntry>>(
          kMaxCacheEntriesPerShard)) {}
InterestGroupPermissionsCache::CacheShard::CacheShard(CacheShard&&) = default;
InterestGroupPermissionsCache::CacheShard::~CacheShard() = default;

InterestGroupPermissionsCache::CacheShard*
InterestGroupPermissionsCache::FindShard(
    const url::Origin& frame_origin,
    const net::NetworkIsolationKey& network_isolation_key,
    base::TimeTicks now) {
  auto shard =
      cache_shards_.find(CacheShardKey{frame_origin, network_isolation_key});
  if (shard == cache_shards_.end())
    return nullptr;
  if (shard->second.expiry < now) {
    cache_shards_.erase(shard);
    return nullptr;
  }
  return &shard->second;
}

void InterestGroupPermissionsCache::MaybeStartDeleteExpiredTimer() {
  if (cache_shards_.empty() || delete_expired_timer_.IsRunning())
    return;
  delete_expired_timer_.Start(
      FROM_HERE, kDeleteExpiredTimerDuration,
      base::BindOnce(&InterestGroupPermissionsCache::DeleteExpired,
                     base::Unretained(this)));
}

void InterestGroupPermissionsCache::DeleteExpired() {
  base::TimeTicks now = base::TimeTicks::Now();
  for (auto shard = cache_shards_.begin(); shard != cache_shards_.end();) {
    auto current_shard = shard;
    shard = ++shard;
    if (current_shard->second.expiry < now)
      cache_shards_.erase(current_shard);
  }

  MaybeStartDeleteExpiredTimer();
}

}  // namespace content
