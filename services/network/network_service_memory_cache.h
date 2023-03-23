// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_H_

#include <inttypes.h>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/cookies/cookie_partition_key.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class HttpVaryData;
class NetLogWithSource;
class NetworkIsolationKey;
class URLRequest;
struct TransportInfo;
}  // namespace net

namespace network {

class NetworkContext;
class NetworkServiceMemoryCacheURLLoader;
class NetworkServiceMemoryCacheWriter;
struct CrossOriginEmbedderPolicy;
struct ResourceRequest;

// An in-memory HTTP cache. NetworkContext owns the in-memory cache.
// TODO(https://crbug.com/1339708): Add more descriptions once the network
// service starts serving response from the in-memory cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceMemoryCache {
 public:
  explicit NetworkServiceMemoryCache(NetworkContext* network_context);
  ~NetworkServiceMemoryCache();

  NetworkServiceMemoryCache(const NetworkServiceMemoryCache&) = delete;
  NetworkServiceMemoryCache& operator=(const NetworkServiceMemoryCache&) =
      delete;

  base::WeakPtr<NetworkServiceMemoryCache> GetWeakPtr();

  size_t total_bytes() const { return total_bytes_; }

  // Clears all cache entries.
  void Clear();

  // Creates a NetworkServiceMemoryCacheWriter when the response is 200 and it
  // can be cacheable.
  std::unique_ptr<NetworkServiceMemoryCacheWriter> MaybeCreateWriter(
      net::URLRequest* url_request,
      mojom::RequestDestination request_destination,
      const net::TransportInfo& transport_info,
      const mojom::URLResponseHeadPtr& response);

  // Stores an HTTP response into `this`. Called when a writer finished reading
  // response body.
  void StoreResponse(const std::string& cache_key,
                     const URLLoaderCompletionStatus& status,
                     mojom::RequestDestination request_destination,
                     const net::HttpVaryData& vary_data,
                     const net::TransportInfo& transport_info,
                     mojom::URLResponseHeadPtr response_head,
                     std::vector<unsigned char> data);

  // Returns a cache key if `this` has a fresh response for `resource_request`.
  // `factory_client_security_state` should come from
  // mojom::URLLoaderFactoryParams. The returned cache key is valid only for the
  // current call stack. It must be used synchronously.
  absl::optional<std::string> CanServe(
      uint32_t load_options,
      const ResourceRequest& resource_request,
      const net::NetworkIsolationKey& network_isolation_key,
      const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      const mojom::ClientSecurityState* factory_client_security_state);

  // Creates and starts a custom URLLoader that serves a response from the
  // in-memory cache, instead of creating a network::URLLoader. Must be called
  // immediately after CanServe().
  void CreateLoaderAndStart(
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const std::string& cache_key,
      const ResourceRequest& resource_request,
      const net::NetLogWithSource net_log,
      const absl::optional<net::CookiePartitionKey> cookie_partition_key,
      mojo::PendingRemote<mojom::URLLoaderClient> client);

  // Returns a suitable capacity for a data pipe that is used to serve a
  // response from `this`.
  uint32_t GetDataPipeCapacity(size_t content_length);

  // Called when a custom URLLoader is completed.
  void OnLoaderCompleted(NetworkServiceMemoryCacheURLLoader* loader);

  // Called when a redirect happens for a request.
  void OnRedirect(const net::URLRequest* url_request,
                  mojom::RequestDestination request_destination);

  void SetCurrentTimeForTesting(base::Time current_time);

  mojom::URLResponseHeadPtr GetResponseHeadForTesting(
      const std::string& cache_key);

  void SetDataPipeCapacityForTesting(uint32_t capacity);

 private:
  struct Entry;
  using CacheMap = base::LRUCache<std::string, std::unique_ptr<Entry>>;

  // Returns the current time for cache freshness checks.
  base::Time GetCurrentTime();

  // Used for tracing.
  uint64_t GetNextTraceId();

  // Erases a single entry.
  void EraseEntry(CacheMap::iterator it);

  // Erases least recently used entries from the in-memory cache until
  // `total_bytes_` becomes less than `max_total_bytes_`.
  void ShrinkToTotalBytes();

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // `network_context_` onws `this`.
  const raw_ptr<NetworkContext> network_context_;

  uint32_t next_trace_id_ = 0;

  CacheMap entries_;
  const size_t max_total_bytes_;
  const size_t max_per_entry_bytes_;
  size_t total_bytes_ = 0;

  std::set<std::unique_ptr<NetworkServiceMemoryCacheURLLoader>,
           base::UniquePtrComparator>
      url_loaders_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  base::Time current_time_for_testing_;

  absl::optional<uint32_t> data_pipe_capacity_for_testing_;

  base::WeakPtrFactory<NetworkServiceMemoryCache> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_H_
