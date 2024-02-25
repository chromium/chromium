// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_cache.h"

#include <iterator>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "services/network/data_remover_util.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "url/gurl.h"

namespace network::cors {

namespace {

constexpr size_t kMaxCacheEntries = 1024u;
constexpr size_t kMaxKeyLength = 1024u;
constexpr size_t kPurgeUnit = 10u;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CacheMetric {
  kHitAndPass = 0,
  kHitAndFail = 1,
  kMiss = 2,
  kStale = 3,

  kMaxValue = kStale,
};

base::Value::Dict NetLogCacheStatusParams(const CacheMetric metric) {
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

  return base::Value::Dict().Set("status", cache_status);
}

void RecordCacheMetricNetLog(CacheMetric metric,
                             const net::NetLogWithSource& net_log) {
  net_log.AddEvent(net::NetLogEventType::CHECK_CORS_PREFLIGHT_CACHE,
                   [&] { return NetLogCacheStatusParams(metric); });
}

}  // namespace

PreflightCache::PreflightCache() = default;
PreflightCache::~PreflightCache() = default;

void PreflightCache::AppendEntry(
    const url::Origin& origin,
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    mojom::IPAddressSpace target_ip_address_space,
    std::unique_ptr<PreflightResult> preflight_result) {
  DCHECK(preflight_result);

  // Do not cache `preflight_result` if `url` is too long.
  const std::string url_spec = url.spec();
  if (url_spec.length() >= kMaxKeyLength) {
    return;
  }

  auto key = std::make_tuple(origin, url_spec, network_isolation_key,
                             target_ip_address_space);
  const auto existing_entry = cache_.find(key);
  if (existing_entry == cache_.end()) {
    // Since one new entry is always added below, let's purge one cache entry
    // if cache size is larger than kMaxCacheEntries - 1 so that the size to be
    // kMaxCacheEntries at maximum.
    MayPurge(kMaxCacheEntries - 1, kPurgeUnit);
  }
  cache_[key] = std::move(preflight_result);
}

bool PreflightCache::CheckIfRequestCanSkipPreflight(
    const url::Origin& origin,
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    mojom::IPAddressSpace target_ip_address_space,
    mojom::CredentialsMode credentials_mode,
    const std::string& method,
    const net::HttpRequestHeaders& request_headers,
    bool is_revalidating,
    const net::NetLogWithSource& net_log,
    bool acam_preflight_spec_conformant) {
  // Check if the entry exists in the cache.
  auto key = std::make_tuple(origin, url.spec(), network_isolation_key,
                             target_ip_address_space);
  auto cache_entry = cache_.find(key);
  if (cache_entry == cache_.end()) {
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
            acam_preflight_spec_conformant)) {
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
  cache_.erase(cache_entry);
  return false;
}

// Clear browsing history time ranges allow for last 1hr, 24hr, 7d, 4w,
// and all time.
// The PreflightCache does not contain a timestamp for when the entry was
// added to the cache, and since Chrome caps the Access-Control-Max-Age header
// value for CORS-preflight responses to 2hrs it doesn't make sense to add the
// granularity to remove only entries created in the last 1hr.
// Always clears the whole PreflightCache regardless the range selected for
// Clear Browsing history
void PreflightCache::ClearCache(mojom::ClearDataFilterPtr url_filter) {
  if (url_filter.is_null()) {
    cache_.clear();
    return;
  }
  if (url_filter->origins.empty() && url_filter->domains.empty()) {
    switch (url_filter->type) {
      case mojom::ClearDataFilter_Type::DELETE_MATCHES:
        return;  // Nothing to do
      case mojom::ClearDataFilter_Type::KEEP_MATCHES:
        cache_.clear();  // Remove all
        return;
    }
  }
  std::set<url::Origin> origins(url_filter->origins.begin(),
                                url_filter->origins.end());
  std::set<std::string> domains(url_filter->domains.begin(),
                                url_filter->domains.end());

  for (auto it = cache_.begin(); it != cache_.end();) {
    auto next_it = std::next(it);
    auto cached_url = std::get<0>(it->first).GetURL();
    if (network::DoesUrlMatchFilter(url_filter->type, origins, domains,
                                    cached_url)) {
      cache_.erase(it);
    }
    it = next_it;
  }
}

size_t PreflightCache::CountEntriesForTesting() const {
  return cache_.size();
}

bool PreflightCache::DoesEntryExistForTesting(
    const url::Origin& origin,
    const std::string& url,
    const net::NetworkIsolationKey& network_isolation_key,
    mojom::IPAddressSpace target_ip_address_space) {
  std::tuple<url::Origin, std::string, net::NetworkIsolationKey,
             mojom::IPAddressSpace>
      entry_key = std::make_tuple(origin, url, network_isolation_key,
                                  target_ip_address_space);
  return cache_.find(entry_key) != cache_.end();
}

void PreflightCache::MayPurgeForTesting(size_t max_entries, size_t purge_unit) {
  MayPurge(max_entries, purge_unit);
}

void PreflightCache::MayPurge(size_t max_entries, size_t purge_unit) {
  if (cache_.size() <= max_entries) {
    return;
  }
  DCHECK_GE(cache_.size(), purge_unit);
  auto purge_begin_entry = cache_.begin();
  std::advance(purge_begin_entry, base::RandInt(0, cache_.size() - purge_unit));
  auto purge_end_entry = purge_begin_entry;
  std::advance(purge_end_entry, purge_unit);
  cache_.erase(purge_begin_entry, purge_end_entry);
}

}  // namespace network::cors
