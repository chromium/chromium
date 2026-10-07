// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_cache.h"

#include <stddef.h>

#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "net/base/does_url_match_filter.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "services/network/data_remover_util.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "url/gurl.h"

namespace network::cors {

namespace {

constexpr size_t kMaxKeyLength = 1024u;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CacheMetric {
  kHitAndPass = 0,
  kHitAndFail = 1,
  kMiss = 2,
  kStale = 3,

  kMaxValue = kStale,
};

base::DictValue NetLogCacheStatusParams(const CacheMetric metric) {
  std::string cache_status;
  switch (metric) {
    case CacheMetric::kHitAndPass:
      cache_status = "hit-and-pass";
      break;
    case CacheMetric::kHitAndFail:
      cache_status = "hit-and-fail";
      break;
    case CacheMetric::kMiss:
      cache_status = "miss";
      break;
    case CacheMetric::kStale:
      cache_status = "stale";
      break;
  }

  return base::DictValue().Set("status", cache_status);
}

void RecordCacheMetricNetLog(CacheMetric metric,
                             const net::NetLogWithSource& net_log) {
  net_log.AddEvent(net::NetLogEventType::CHECK_CORS_PREFLIGHT_CACHE,
                   [&] { return NetLogCacheStatusParams(metric); });
}

}  // namespace

PreflightCache::PreflightCache() : cache_(kMaxTopFrameSites) {}

PreflightCache::~PreflightCache() = default;

void PreflightCache::AppendEntry(
    const url::Origin& origin,
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    std::unique_ptr<PreflightResult> preflight_result) {
  DCHECK(preflight_result);

  // Do not cache `preflight_result` if `url` is too long.
  const std::string url_spec = url.spec();
  if (url_spec.length() >= kMaxKeyLength) {
    return;
  }

  const std::optional<net::SchemefulSite>& top_frame_site =
      network_isolation_key.GetTopFrameSite();
  auto cache_it = cache_.Get(top_frame_site);
  if (cache_it == cache_.end()) {
    cache_it = cache_.Put(top_frame_site, EntryMap());
  }

  EntryMap& entries = cache_it->second;
  EntryKey key(origin, url_spec, network_isolation_key);
  auto it = entries.find(key);
  if (it != entries.end()) {
    it->second = std::move(preflight_result);
    return;
  }
  // Purge entries in this top-frame site partition if its size reaches
  // `kMaxEntriesPerTopFrameSite` so the per-site partition size stays
  // bounded.
  MayPurge(entries, kMaxEntriesPerTopFrameSite - 1, kPurgeUnitPerTopFrameSite);
  entries.emplace(std::move(key), std::move(preflight_result));
}

bool PreflightCache::CheckIfRequestCanSkipPreflight(
    const url::Origin& origin,
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    mojom::CredentialsMode credentials_mode,
    const std::string& method,
    const net::HttpRequestHeaders& request_headers,
    bool is_revalidating,
    const net::NetLogWithSource& net_log,
    bool acam_preflight_spec_conformant,
    bool is_ad_auction_trusted_signals_request) {
  const std::optional<net::SchemefulSite>& top_frame_site =
      network_isolation_key.GetTopFrameSite();
  auto cache_it = cache_.Peek(top_frame_site);
  if (cache_it == cache_.end()) {
    RecordCacheMetricNetLog(CacheMetric::kMiss, net_log);
    return false;
  }

  EntryMap& entries = cache_it->second;
  EntryKey key(origin, url.spec(), network_isolation_key);
  auto cache_entry = entries.find(key);
  if (cache_entry == entries.end()) {
    RecordCacheMetricNetLog(CacheMetric::kMiss, net_log);
    return false;
  }

  // Check if the entry is still valid.
  if (!cache_entry->second->IsExpired()) {
    // Both `origin` and `url` are in cache. Check if the entry is sufficient to
    // skip CORS-preflight.
    if (cache_entry->second->EnsureAllowedRequest(
            credentials_mode, method, request_headers, is_revalidating,
            NonWildcardRequestHeadersSupport(true),
            acam_preflight_spec_conformant,
            is_ad_auction_trusted_signals_request)) {
      cache_.Get(top_frame_site);
      // Note that we always use the "with non-wildcard request headers"
      // variant, because it is hard to generate the correct error information
      // from here, and cache miss is in most case recoverable.
      RecordCacheMetricNetLog(CacheMetric::kHitAndPass, net_log);
      net_log.AddEvent(
          net::NetLogEventType::CORS_PREFLIGHT_CACHED_RESULT,
          [&cache_entry] { return cache_entry->second->NetLogParams(); });
      return true;
    }
    RecordCacheMetricNetLog(CacheMetric::kHitAndFail, net_log);
  } else {
    RecordCacheMetricNetLog(CacheMetric::kStale, net_log);
  }

  // The cache entry is either stale or not sufficient. Remove the item from the
  // cache.
  entries.erase(cache_entry);
  if (entries.empty()) {
    cache_.Erase(cache_it);
  }
  return false;
}

// Clear browsing history time ranges allow for last 1hr, 24hr, 7d, 4w,
// and all time.
// The PreflightCache does not contain a timestamp for when the entry was
// added to the cache, and since Chrome caps the Access-Control-Max-Age header
// value for CORS-preflight responses to 2hrs it doesn't make sense to add the
// granularity to remove only entries created in the last 1hr.
// Always clears the whole PreflightCache regardless the range selected for
// Clear Browsing history.
void PreflightCache::ClearCache(mojom::ClearDataFilterPtr url_filter) {
  if (url_filter.is_null()) {
    cache_.Clear();
    return;
  }
  if (url_filter->origins.empty() && url_filter->domains.empty()) {
    switch (url_filter->type) {
      case mojom::ClearDataFilter_Type::DELETE_MATCHES:
        return;  // Nothing to do
      case mojom::ClearDataFilter_Type::KEEP_MATCHES:
        cache_.Clear();  // Remove all
        return;
    }
  }
  const net::UrlFilterType url_filter_type =
      ConvertClearDataFilterType(url_filter->type);
  const base::flat_set<url::Origin> origins(url_filter->origins.begin(),
                                            url_filter->origins.end());
  const base::flat_set<std::string> domains(url_filter->domains.begin(),
                                            url_filter->domains.end());

  for (auto site_it = cache_.begin(); site_it != cache_.end();) {
    EntryMap& entries = site_it->second;
    std::erase_if(entries, [&](const auto& entry) {
      return net::DoesUrlMatchFilter(url_filter_type, origins, domains,
                                     std::get<0>(entry.first).GetURL());
    });
    if (entries.empty()) {
      site_it = cache_.Erase(site_it);
    } else {
      ++site_it;
    }
  }
}

size_t PreflightCache::CountEntriesForTesting() const {
  size_t total = 0;
  for (const auto& [site, entries] : cache_) {
    total += entries.size();
  }
  return total;
}

size_t PreflightCache::CountTopFrameSitesForTesting() const {
  return cache_.size();
}

size_t PreflightCache::CountEntriesForTopFrameSiteForTesting(
    const std::optional<net::SchemefulSite>& top_frame_site) const {
  auto it = cache_.Peek(top_frame_site);
  return it != cache_.end() ? it->second.size() : 0u;
}

bool PreflightCache::DoesEntryExistForTesting(
    const url::Origin& origin,
    const std::string& url,
    const net::NetworkIsolationKey& network_isolation_key) const {
  auto it = cache_.Peek(network_isolation_key.GetTopFrameSite());
  if (it == cache_.end()) {
    return false;
  }
  return it->second.contains(EntryKey(origin, url, network_isolation_key));
}

void PreflightCache::MayPurgeForTesting(
    const std::optional<net::SchemefulSite>& top_frame_site,
    size_t max_entries,
    size_t purge_unit) {
  auto it = cache_.Peek(top_frame_site);
  if (it == cache_.end()) {
    return;
  }
  MayPurge(it->second, max_entries, purge_unit);
  if (it->second.empty()) {
    cache_.Erase(it);
  }
}

void PreflightCache::MayPurge(EntryMap& entries,
                              size_t max_entries,
                              size_t purge_unit) {
  if (entries.size() <= max_entries) {
    return;
  }
  DCHECK_GE(entries.size(), purge_unit);
  auto purge_begin_entry = entries.begin();
  std::advance(purge_begin_entry,
               base::RandIntInclusive(0, entries.size() - purge_unit));
  auto purge_end_entry = purge_begin_entry;
  std::advance(purge_end_entry, purge_unit);
  entries.erase(purge_begin_entry, purge_end_entry);
}

}  // namespace network::cors
