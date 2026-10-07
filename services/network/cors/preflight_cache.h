// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_PREFLIGHT_CACHE_H_
#define SERVICES_NETWORK_CORS_PREFLIGHT_CACHE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_request_headers.h"
#include "services/network/cors/preflight_result.h"
#include "services/network/public/mojom/clear_data_filter.mojom-forward.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/origin.h"

class GURL;

namespace net {
class NetLogWithSource;
}  // namespace net

namespace network::cors {

// A class to implement CORS-preflight cache that is defined in the fetch spec,
// https://fetch.spec.whatwg.org/#concept-cache.
//
// Cache entries are partitioned by top-frame site (`net::SchemefulSite`) for
// quota enforcement and cache eviction, while lookups strictly enforce exact
// `(origin, url, NetworkIsolationKey)` matching to preserve origin and
// subframe isolation.
//
// Eviction operates in two tiers:
// 1. Local eviction: When the number of entries under a single top-frame site
//    exceeds `kMaxEntriesPerTopFrameSite`, entries are purged locally within
//    that top-frame site without evicting entries from other top-frame sites.
// 2. Global eviction: An LRU cap (`kMaxTopFrameSites`) on active top-frame
//    sites bounds total memory consumption profile-wide.
class COMPONENT_EXPORT(NETWORK_SERVICE) PreflightCache final {
 public:
  // Maximum number of preflight entries allowed per top-frame site partition.
  static constexpr size_t kMaxEntriesPerTopFrameSite = 32u;
  // Number of entries evicted within a top-frame site partition when capacity
  // is exceeded.
  static constexpr size_t kPurgeUnitPerTopFrameSite = 4u;
  // Maximum number of active top-frame site partitions tracked profile-wide.
  static constexpr size_t kMaxTopFrameSites = 128u;

  PreflightCache();

  PreflightCache(const PreflightCache&) = delete;
  PreflightCache& operator=(const PreflightCache&) = delete;

  ~PreflightCache();

  // Appends new `preflight_result` entry to the cache for a specified `origin`,
  // `url`, and `network_isolation_key`.
  void AppendEntry(const url::Origin& origin,
                   const GURL& url,
                   const net::NetworkIsolationKey& network_isolation_key,
                   std::unique_ptr<PreflightResult> preflight_result);

  // Checks if the preflight check can be skipped for given parameters.
  // Returns true if a valid, non-expired cache entry satisfies the request.
  // Returns false and purges stale or insufficient entries (pruning empty
  // top-frame site partitions) on cache miss or authorization failure.
  bool CheckIfRequestCanSkipPreflight(
      const url::Origin& origin,
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      mojom::CredentialsMode credentials_mode,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_revalidating,
      const net::NetLogWithSource& net_log,
      bool acam_preflight_spec_conformant,
      bool is_ad_auction_trusted_signals_request);

  // Clears cached preflight results according to `url_filter`.
  // If `url_filter` is null, all entries across all top-frame sites are
  // cleared. Top-frame site partitions emptied by filtering are pruned from the
  // cache.
  void ClearCache(mojom::ClearDataFilterPtr url_filter);

  size_t CountEntriesForTesting() const;
  size_t CountTopFrameSitesForTesting() const;
  size_t CountEntriesForTopFrameSiteForTesting(
      const std::optional<net::SchemefulSite>& top_frame_site) const;

  bool DoesEntryExistForTesting(
      const url::Origin& origin,
      const std::string& url,
      const net::NetworkIsolationKey& network_isolation_key) const;

  // Purges entries within `top_frame_site`'s partition for testing.
  void MayPurgeForTesting(
      const std::optional<net::SchemefulSite>& top_frame_site,
      size_t max_entries,
      size_t purge_unit);

 private:
  // Key for exact entry lookup within a top-frame site partition.
  using EntryKey = std::tuple<url::Origin /* origin */,
                              std::string /* url */,
                              net::NetworkIsolationKey /* NIK */>;
  using EntryMap = std::map<EntryKey, std::unique_ptr<PreflightResult>>;

  // Randomly evicts `purge_unit` entries from `entries` when exceeding
  // `max_entries`.
  void MayPurge(EntryMap& entries, size_t max_entries, size_t purge_unit);

  // Outer LRU cache keyed by top-frame site (`std::nullopt` for empty NIKs).
  base::LRUCache<std::optional<net::SchemefulSite>, EntryMap> cache_;
};

}  // namespace network::cors

#endif  // SERVICES_NETWORK_CORS_PREFLIGHT_CACHE_H_
