// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_cache.h"

#include <iterator>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace network {

namespace cors {

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

void ReportCacheMetric(CacheMetric metric) {
  UMA_HISTOGRAM_ENUMERATION("Net.Cors.PreflightCacheResult", metric);
}

}  // namespace

PreflightCache::PreflightCache() = default;
PreflightCache::~PreflightCache() = default;

void PreflightCache::AppendEntry(
    const std::string& origin,
    const GURL& url,
    std::unique_ptr<PreflightResult> preflight_result) {
  DCHECK(preflight_result);

  // Do not cache |preflight_result| if |url| is too long.
  const std::string url_spec = url.spec();
  if (url_spec.length() >= kMaxKeyLength)
    return;

  auto key = std::make_pair(origin, url_spec);
  const auto existing_entry = cache_.find(key);
  if (existing_entry == cache_.end()) {
    // Since one new entry is always added below, let's purge one cache entry
    // if cache size is larger than kMaxCacheEntries - 1 so that the size to be
    // kMaxCacheEntries at maximum.
    MayPurge(kMaxCacheEntries - 1, kPurgeUnit);
  }
  UMA_HISTOGRAM_COUNTS_10000("Net.Cors.PreflightCacheKeySize",
                             url_spec.length());

  cache_[key] = std::move(preflight_result);
}

bool PreflightCache::CheckIfRequestCanSkipPreflight(
    const std::string& origin,
    const GURL& url,
    mojom::CredentialsMode credentials_mode,
    const std::string& method,
    const net::HttpRequestHeaders& request_headers,
    bool is_revalidating) {
  // Check if the entry exists in the cache.
  auto key = std::make_pair(origin, url.spec());
  auto cache_entry = cache_.find(key);
  if (cache_entry == cache_.end()) {
    ReportCacheMetric(CacheMetric::kMiss);
    return false;
  }

  // Check if the entry is still valid.
  if (!cache_entry->second->IsExpired()) {
    // Both |origin| and |url| are in cache. Check if the entry is sufficient to
    // skip CORS-preflight.
    if (cache_entry->second->EnsureAllowedRequest(
            credentials_mode, method, request_headers, is_revalidating)) {
      ReportCacheMetric(CacheMetric::kHitAndPass);
      return true;
    }
    ReportCacheMetric(CacheMetric::kHitAndFail);
  } else {
    ReportCacheMetric(CacheMetric::kStale);
  }

  // The cache entry is either stale or not sufficient. Remove the item from the
  // cache.
  cache_.erase(cache_entry);
  return false;
}

size_t PreflightCache::CountEntriesForTesting() const {
  return cache_.size();
}

void PreflightCache::MayPurgeForTesting(size_t max_entries, size_t purge_unit) {
  MayPurge(max_entries, purge_unit);
}

void PreflightCache::MayPurge(size_t max_entries, size_t purge_unit) {
  if (cache_.size() <= max_entries)
    return;
  DCHECK_GE(cache_.size(), purge_unit);
  auto purge_begin_entry = cache_.begin();
  std::advance(purge_begin_entry, base::RandInt(0, cache_.size() - purge_unit));
  auto purge_end_entry = purge_begin_entry;
  std::advance(purge_end_entry, purge_unit);
  cache_.erase(purge_begin_entry, purge_end_entry);
}

}  // namespace cors

}  // namespace network
