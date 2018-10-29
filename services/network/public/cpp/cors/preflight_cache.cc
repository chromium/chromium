// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_cache.h"

#include "url/gurl.h"

namespace network {

namespace cors {

PreflightCache::PreflightCache() = default;
PreflightCache::~PreflightCache() = default;

void PreflightCache::AppendEntry(
    const std::string& origin,
    const GURL& url,
    std::unique_ptr<PreflightResult> preflight_result) {
  DCHECK(preflight_result);
  cache_[origin][url.spec()] = std::move(preflight_result);
}

bool PreflightCache::CheckIfRequestCanSkipPreflight(
    const std::string& origin,
    const GURL& url,
    mojom::FetchCredentialsMode credentials_mode,
    const std::string& method,
    const net::HttpRequestHeaders& request_headers,
    bool is_revalidating) {
  // Either |origin| or |url| are not in cache.
  auto cache_per_origin = cache_.find(origin);
  if (cache_per_origin == cache_.end())
    return false;

  auto cache_entry = cache_per_origin->second.find(url.spec());
  if (cache_entry == cache_per_origin->second.end())
    return false;

  // Both |origin| and |url| are in cache. Check if the entry is still valid and
  // sufficient to skip CORS-preflight.
  if (cache_entry->second->EnsureAllowedRequest(
          credentials_mode, method, request_headers, is_revalidating)) {
    return true;
  }

  // The cache entry is either stale or not sufficient. Remove the item from the
  // cache.
  cache_per_origin->second.erase(url.spec());
  if (cache_per_origin->second.empty())
    cache_.erase(cache_per_origin);

  return false;
}

size_t PreflightCache::CountOriginsForTesting() const {
  return cache_.size();
}

size_t PreflightCache::CountEntriesForTesting() const {
  size_t entries = 0;
  for (auto const& cache_per_origin : cache_)
    entries += cache_per_origin.second.size();
  return entries;
}

}  // namespace cors

}  // namespace network
