// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/http_auth_cache_proxy_copier.h"

#include "base/logging.h"
#include "net/http/http_auth_cache.h"

namespace network {

HttpAuthCacheProxyCopier::HttpAuthCacheProxyCopier() = default;
HttpAuthCacheProxyCopier::~HttpAuthCacheProxyCopier() = default;

base::UnguessableToken HttpAuthCacheProxyCopier::SaveHttpAuthCache(
    const net::HttpAuthCache& cache) {
  base::UnguessableToken key = base::UnguessableToken::Create();
  auto cache_it = caches_.emplace(
      key, std::make_unique<net::HttpAuthCache>(
               // It doesn't matter what value we pass here because these
               // caches are only used for copying proxy entries (not
               // server entries).
               /*key_server_entries_by_network_anonymization_key=*/true));
  DCHECK(cache_it.second);
  cache_it.first->second->CopyProxyEntriesFrom(cache);
  return key;
}

void HttpAuthCacheProxyCopier::LoadHttpAuthCache(
    const base::UnguessableToken& key,
    net::HttpAuthCache* cache) {
  auto it = caches_.find(key);
  if (it == caches_.end()) {
    DLOG(ERROR) << "Unknown HttpAuthCache key: " << key;
    return;
  }
  cache->CopyProxyEntriesFrom(*it->second);
  caches_.erase(it);
}

}  // namespace network
