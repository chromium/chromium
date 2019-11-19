// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_client_auth_cache.h"

#include "base/logging.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

SSLClientAuthCache::SSLClientAuthCache() {}

SSLClientAuthCache::~SSLClientAuthCache() {}

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
  cache_[server] =
      std::make_pair(std::move(certificate), std::move(private_key));

  // TODO(wtc): enforce a maximum number of entries.
}

bool SSLClientAuthCache::Remove(const HostPortPair& server) {
  return cache_.erase(server);
}

void SSLClientAuthCache::Clear() {
  cache_.clear();
}

}  // namespace net
