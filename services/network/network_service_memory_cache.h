// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_H_

#include <inttypes.h>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class NetworkIsolationKey;
class URLRequest;
}  // namespace net

namespace network {

class NetworkServiceMemoryCacheWriter;
struct ResourceRequest;

// An in-memory HTTP cache. NetworkContext owns the in-memory cache.
// TODO(https://crbug.com/1339708): Add more descriptions once the network
// service starts serving response from the in-memory cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceMemoryCache {
 public:
  NetworkServiceMemoryCache();
  ~NetworkServiceMemoryCache();

  NetworkServiceMemoryCache(const NetworkServiceMemoryCache&) = delete;
  NetworkServiceMemoryCache& operator=(const NetworkServiceMemoryCache&) =
      delete;

  base::WeakPtr<NetworkServiceMemoryCache> GetWeakPtr();

  // Creates a NetworkServiceMemoryCacheWriter when the response is 200 and it
  // can be cacheable.
  std::unique_ptr<NetworkServiceMemoryCacheWriter> MaybeCreateWriter(
      net::URLRequest* url_request,
      mojom::RequestDestination request_destination,
      const mojom::URLResponseHeadPtr& response);

  // Stores an HTTP response into `this`. Called when a writer finished reading
  // response body.
  void StoreResponse(const std::string& cache_key,
                     const URLLoaderCompletionStatus& status,
                     mojom::URLResponseHeadPtr response_head,
                     std::vector<unsigned char> data);

  // Returns a cache key if `this` has a fresh response for `resource_request`.
  // The returned cache key is valid only for the current call stack. It must be
  // used synchronously.
  absl::optional<std::string> CanServe(
      const ResourceRequest& resource_request,
      const net::NetworkIsolationKey& network_isolation_key);

  void SetCurrentTimeForTesting(base::Time current_time);

  mojom::URLResponseHeadPtr GetResponseHeadForTesting(
      const std::string& cache_key);

 private:
  struct Entry;
  using CacheMap = base::LRUCache<std::string, std::unique_ptr<Entry>>;

  // Returns the current time for cache freshness checks.
  base::Time GetCurrentTime();

  // Used for tracing.
  uint64_t GetNextTraceId();

  // Erases least recently used entries from the in-memory cache until
  // `total_bytes_` becomes less than `max_total_bytes_`.
  void ShrinkToTotalBytes();

  uint32_t next_trace_id_ = 0;

  CacheMap entries_;
  const size_t max_total_bytes_;
  size_t total_bytes_ = 0;

  base::Time current_time_for_testing_;

  base::WeakPtrFactory<NetworkServiceMemoryCache> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_H_
