// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_cache.h"

#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/types/pass_key.h"
#include "net/base/network_isolation_key.h"
#include "services/network/prefetch_url_loader_client.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {

size_t GetMaxSize() {
  int supplied_size = features::kNetworkContextPrefetchMaxLoaders.Get();
  return static_cast<size_t>(std::max(supplied_size, 1));
}

}  // namespace

PrefetchCache::PrefetchCache() : max_size_(GetMaxSize()) {}

PrefetchCache::~PrefetchCache() = default;

PrefetchURLLoaderClient* PrefetchCache::Emplace(
    const ResourceRequest& request) {
  if (!request.trusted_params.has_value()) {
    DLOG(WARNING)
        << "NetworkContext::Emplace() was called with a request with no "
           "NetworkIsolationKey. This is not going to work.";
    return nullptr;
  }

  const net::NetworkIsolationKey& nik =
      request.trusted_params->isolation_info.network_isolation_key();

  if (nik.IsTransient()) {
    DLOG(WARNING) << "NetworkContext::Emplace() was called with a request with "
                     "a transient NetworkIsolationKey. This won't match "
                     "anything, so ignoring.";
    return nullptr;
  }

  if (!request.url.SchemeIsHTTPOrHTTPS()) {
    DLOG(WARNING) << "NetworkContext::Emplace() was called with a scheme that "
                     "is not http or https. This is not going to work.";
    return nullptr;
  }

  if (map_.contains(KeyType(nik, request.url))) {
    return nullptr;
  }

  while (map_.size() >= max_size_) {
    EraseOldest();
  }

  auto [it, insert_ok] =
      client_storage_.insert(std::make_unique<PrefetchURLLoaderClient>(
          base::PassKey<PrefetchCache>(), nik, request,
          /*expiry_time=*/base::TimeTicks::Now() + kMaxAge, this));
  CHECK(insert_ok);
  auto* client = it->get();

  list_.Append(client);
  auto [_, is_not_duplicated] = map_.emplace(
      KeyType(client->network_isolation_key(), client->url()), client);
  CHECK(is_not_duplicated);

  if (!expiry_timer_.IsRunning()) {
    StartTimer();
  }

  return client;
}

PrefetchURLLoaderClient* PrefetchCache::Lookup(
    const net::NetworkIsolationKey& nik,
    const GURL& url) {
  const auto it = map_.find(KeyType(nik, url));
  return it == map_.end() ? nullptr : it->second;
}

void PrefetchCache::Consume(PrefetchURLLoaderClient* client) {
  const bool was_oldest = client == list_.head();
  RemoveFromCache(client);
  if (was_oldest) {
    if (list_.empty()) {
      expiry_timer_.Stop();
    } else {
      // Automatically resets the timer.
      StartTimer();
    }
  }
}

void PrefetchCache::Erase(PrefetchURLLoaderClient* client) {
  const auto it =
      map_.find(KeyType(client->network_isolation_key(), client->url()));
  if (it != map_.end()) {
    CHECK_EQ(it->second, client);
    map_.erase(it);
    client->RemoveFromList();
  }
  EraseFromStorage(client);
}

void PrefetchCache::OnTimer() {
  auto now = base::TimeTicks::Now();
  while (!list_.empty() &&
         list_.head()->value()->expiry_time() <= now + kExpirySlack) {
    EraseOldest();
  }
  if (!list_.empty()) {
    StartTimer(now);
  }
}

void PrefetchCache::EraseOldest() {
  CHECK(!list_.empty());
  PrefetchURLLoaderClient* oldest = list_.head()->value();
  RemoveFromCache(oldest);
  // Make sure we actually removed the right one.
  CHECK_NE(oldest, list_.head());
  EraseFromStorage(oldest);
}

void PrefetchCache::RemoveFromCache(PrefetchURLLoaderClient* client) {
  const auto it =
      map_.find(KeyType(client->network_isolation_key(), client->url()));
  CHECK(it != map_.end());
  CHECK_EQ(it->second, client);
  map_.erase(it);
  client->RemoveFromList();
}

void PrefetchCache::EraseFromStorage(PrefetchURLLoaderClient* client) {
  // In C++20, std::set::erase() doesn't support transparent comparisons, so
  // it's necessary to use find() first.
  auto it = client_storage_.find(client);
  CHECK(it != client_storage_.end());
  client_storage_.erase(it);
}

void PrefetchCache::StartTimer(base::TimeTicks now) {
  auto next_expiry = list_.head()->value()->expiry_time();
  // It's safe to use base::Unretained() as destroying `this` will destroy
  // `expiry_timer_`, preventing the callback being called.
  expiry_timer_.Start(
      FROM_HERE, std::max(base::Seconds(0), next_expiry - now),
      base::BindOnce(&PrefetchCache::OnTimer, base::Unretained(this)));
}

}  // namespace network
