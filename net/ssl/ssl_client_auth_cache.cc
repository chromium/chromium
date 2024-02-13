// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_client_auth_cache.h"

#include "base/check.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

SSLClientAuthCache::SSLClientAuthCache() = default;

SSLClientAuthCache::~SSLClientAuthCache() = default;

bool SSLClientAuthCache::Lookup(const HostPortPair& server,
                                scoped_refptr<X509Certificate>* certificate,
                                scoped_refptr<SSLPrivateKey>* private_key) {
  DCHECK(certificate);

  auto iter = cache_.find(server);
  if (iter == cache_.end())
    return false;

  *certificate = iter->second.first;
  *private_key = iter->second.second;
  return true;
}

void SSLClientAuthCache::Add(const HostPortPair& server,
                             scoped_refptr<X509Certificate> certificate,
                             scoped_refptr<SSLPrivateKey> private_key) {
  cache_[server] = std::pair(std::move(certificate), std::move(private_key));

  // TODO(wtc): enforce a maximum number of entries.
}

bool SSLClientAuthCache::Remove(const HostPortPair& server) {
  return cache_.erase(server);
}

void SSLClientAuthCache::Clear() {
  cache_.clear();
}

base::flat_set<HostPortPair> SSLClientAuthCache::GetCachedServers() const {
  // TODO(mattm): If views become permitted by Chromium style maybe we could
  // avoid the intermediate vector by using:
  // auto keys = std::views::keys(m);
  // base::flat_set<HostPortPair>(base::sorted_unique, keys.begin(),
  //                              keys.end());

  // Use the flat_set underlying container type (currently a std::vector), so we
  // can move the keys into the set instead of copying them.
  base::flat_set<HostPortPair>::container_type keys;
  keys.reserve(cache_.size());
  for (const auto& [key, _] : cache_) {
    keys.push_back(key);
  }
  // `cache_` is a std::map, so the keys are already sorted.
  return base::flat_set<HostPortPair>(base::sorted_unique, std::move(keys));
}

}  // namespace net
