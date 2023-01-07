// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/http_auth_cache_copier.h"

#include "base/logging.h"
#include "net/http/http_auth_cache.h"

namespace network {

HttpAuthCacheCopier::HttpAuthCacheCopier() = default;
HttpAuthCacheCopier::~HttpAuthCacheCopier() = default;

base::UnguessableToken HttpAuthCacheCopier::SaveHttpAuthCache(
    const net::HttpAuthCache& cache) {
  base::UnguessableToken key = base::UnguessableToken::Create();
  auto cache_it = caches_.emplace(
      key, std::make_unique<net::HttpAuthCache>(
               cache.key_server_entries_by_network_anonymization_key()));
  DCHECK(cache_it.second);
  cache_it.first->second->CopyProxyEntriesFrom(cache);
  return key;
}

void HttpAuthCacheCopier::LoadHttpAuthCache(const base::UnguessableToken& key,
                                            net::HttpAuthCache* cache) {
  auto it = caches_.find(key);
  if (it == caches_.end()) {
    DLOG(ERROR) << "Unknown HttpAuthCache key: " << key;
    return;
  }

  // Source and destination caches must have the same configuration.
  DCHECK_EQ(cache->key_server_entries_by_network_anonymization_key(),
            it->second->key_server_entries_by_network_anonymization_key());

  cache->CopyProxyEntriesFrom(*it->second);
  caches_.erase(it);
}

}  // namespace network
