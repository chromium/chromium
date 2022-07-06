// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_memory_cache.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "services/network/network_context.h"
#include "services/network/network_service_memory_cache_url_loader.h"
#include "services/network/network_service_memory_cache_writer.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

// TODO(https://crbug.com/1339708): Adjust the default size based on stats.
const base::FeatureParam<int> kNetworkServiceMemoryCacheMaxTotalSize{
    &features::kNetworkServiceMemoryCache, "max_total_size", 64 * 1024 * 1024};

std::string GenerateCacheKeyForResourceRequest(
    const ResourceRequest& resource_request,
    const net::NetworkIsolationKey& network_isolation_key) {
  const bool is_subframe_document_resource =
      resource_request.destination == mojom::RequestDestination::kIframe;
  return net::HttpCache::GenerateCacheKey(
      resource_request.url, resource_request.load_flags, network_isolation_key,
      /*upload_data_identifier=*/0, is_subframe_document_resource,
      /*use_single_keyed_cache=*/false, /*single_key_checksum=*/"");
}

}  // namespace

struct NetworkServiceMemoryCache::Entry {
  Entry(mojom::URLResponseHeadPtr response_head,
        scoped_refptr<base::RefCountedBytes> content)
      : response_head(std::move(response_head)), content(std::move(content)) {}
  ~Entry() = default;

  // Movable.
  Entry(Entry&&) = default;
  Entry& operator=(Entry&&) = default;
  Entry(const Entry&) = delete;
  Entry& operator=(const Entry&) = delete;

  mojom::URLResponseHeadPtr response_head;
  scoped_refptr<base::RefCountedBytes> content;
};

NetworkServiceMemoryCache::NetworkServiceMemoryCache()
    : entries_(CacheMap::NO_AUTO_EVICT),
      max_total_bytes_(kNetworkServiceMemoryCacheMaxTotalSize.Get()) {}

NetworkServiceMemoryCache::~NetworkServiceMemoryCache() = default;

void NetworkServiceMemoryCache::Clear() {
  entries_.Clear();
}

base::WeakPtr<NetworkServiceMemoryCache>
NetworkServiceMemoryCache::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<NetworkServiceMemoryCacheWriter>
NetworkServiceMemoryCache::MaybeCreateWriter(
    net::URLRequest* url_request,
    mojom::RequestDestination request_destination,
    const mojom::URLResponseHeadPtr& response) {
  DCHECK(url_request);

  // TODO(https://crbug.com/1339708): Make `this` work with
  // SplitCacheByIncludeCredentials. Currently some tests are failing when
  // the feature is enabled.
  if (base::FeatureList::IsEnabled(
          net::features::kSplitCacheByIncludeCredentials)) {
    return nullptr;
  }

  // TODO(https://crbug.com/1339708): Make `this` work for responses from
  // private network. Currently some tests are failing.
  if (response->response_address_space == mojom::IPAddressSpace::kPrivate)
    return nullptr;

  DCHECK(url_request->url().is_valid());
  if (!url_request->url().SchemeIsHTTPOrHTTPS())
    return nullptr;

  if (url_request->method() != net::HttpRequestHeaders::kGetMethod)
    return nullptr;

  // See the comment in HttpCache::Transaction::ShouldPassThrough().
  if (net::HttpCache::IsSplitCacheEnabled() &&
      url_request->isolation_info().network_isolation_key().IsTransient()) {
    return nullptr;
  }

  if (!response->headers || response->headers->response_code() != net::HTTP_OK)
    return nullptr;

  const int load_flags = url_request->load_flags();
  if (load_flags & net::LOAD_BYPASS_CACHE ||
      load_flags & net::LOAD_DISABLE_CACHE) {
    return nullptr;
  }

  net::ValidationType validation_type = response->headers->RequiresValidation(
      response->request_time, response->response_time, GetCurrentTime());
  if (validation_type != net::VALIDATION_NONE)
    return nullptr;

  bool is_subframe_document_resource =
      request_destination == mojom::RequestDestination::kIframe;
  std::string cache_key = net::HttpCache::GenerateCacheKey(
      url_request->url(), url_request->load_flags(),
      url_request->isolation_info().network_isolation_key(),
      /*upload_data_identifier=*/0, is_subframe_document_resource,
      /*use_single_keyed_cache=*/false,
      /*single_key_checksum=*/"");

  return std::make_unique<NetworkServiceMemoryCacheWriter>(
      weak_ptr_factory_.GetWeakPtr(), GetNextTraceId(), std::move(cache_key),
      url_request, response);
}

void NetworkServiceMemoryCache::StoreResponse(
    const std::string& cache_key,
    const URLLoaderCompletionStatus& status,
    mojom::URLResponseHeadPtr response_head,
    std::vector<unsigned char> data) {
  // TODO(https://crbug.com/1339708): Consider caching a response that doesn't
  // have contents.
  if (status.error_code != net::OK || data.size() == 0)
    return;

  // TODO(https://crbug.com/1339708): Consider not storing a large response to
  // improve cache hit rate.
  if (max_total_bytes_ < data.size())
    return;

  auto prev = entries_.Peek(cache_key);
  if (prev != entries_.end()) {
    DCHECK_GE(total_bytes_, prev->second->content->size());
    total_bytes_ -= prev->second->content->size();
    // The following Put() will remove `prev`.
  }

  DCHECK_GE(std::numeric_limits<size_t>::max() - total_bytes_, data.size());
  total_bytes_ += data.size();

  scoped_refptr<base::RefCountedBytes> content =
      base::RefCountedBytes::TakeVector(&data);
  auto entry =
      std::make_unique<Entry>(std::move(response_head), std::move(content));
  entries_.Put(cache_key, std::move(entry));

  ShrinkToTotalBytes();
}

absl::optional<std::string> NetworkServiceMemoryCache::CanServe(
    const ResourceRequest& resource_request,
    const net::NetworkIsolationKey& network_isolation_key,
    const CrossOriginEmbedderPolicy& cross_origin_embedder_policy) {
  // TODO(https://crbug.com/1339708): Support automatically assigned network
  // isolation key for request from browsers. See comments in
  // CorsURLLoaderFactory::CorsURLLoaderFactory.
  if (!network_isolation_key.IsFullyPopulated())
    return absl::nullopt;

  const GURL& url = resource_request.url;
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return absl::nullopt;

  if (resource_request.method != net::HttpRequestHeaders::kGetMethod)
    return absl::nullopt;

  if (resource_request.load_flags & net::LOAD_BYPASS_CACHE ||
      resource_request.load_flags & net::LOAD_DISABLE_CACHE ||
      resource_request.load_flags & net::LOAD_VALIDATE_CACHE) {
    return absl::nullopt;
  }

  std::string cache_key = GenerateCacheKeyForResourceRequest(
      resource_request, network_isolation_key);

  auto it = entries_.Peek(cache_key);
  if (it == entries_.end())
    return absl::nullopt;

  const mojom::URLResponseHeadPtr& response = it->second->response_head;

  absl::optional<mojom::BlockedByResponseReason> blocked_reason =
      CrossOriginResourcePolicy::IsBlocked(
          /*request_url=*/url, /*original_url=*/url,
          resource_request.request_initiator, *response, resource_request.mode,
          resource_request.destination, cross_origin_embedder_policy,
          /*reporter=*/nullptr);
  if (blocked_reason.has_value())
    return absl::nullopt;

  net::ValidationType validation_type = response->headers->RequiresValidation(
      response->request_time, response->response_time, GetCurrentTime());
  if (validation_type != net::VALIDATION_NONE) {
    // The cached response is stale, erase it from the in-memory cache.
    entries_.Erase(it);
    return absl::nullopt;
  }

  return std::move(cache_key);
}

void NetworkServiceMemoryCache::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const std::string& cache_key,
    const ResourceRequest& resource_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client) {
  auto it = entries_.Get(cache_key);
  CHECK(it != entries_.end());

  auto loader = std::make_unique<NetworkServiceMemoryCacheURLLoader>(
      this, GetNextTraceId(), resource_request.url, std::move(receiver),
      std::move(client), it->second->content);
  NetworkServiceMemoryCacheURLLoader* raw_loader = loader.get();
  url_loaders_.insert(std::move(loader));

  raw_loader->Start(resource_request, it->second->response_head.Clone());
}

uint32_t NetworkServiceMemoryCache::GetDataPipeCapacity(size_t content_length) {
  if (data_pipe_capacity_for_testing_.has_value())
    return *data_pipe_capacity_for_testing_;

  uint32_t default_capacity = features::GetDataPipeDefaultAllocationSize();
  if (content_length > default_capacity)
    return default_capacity;
  return static_cast<size_t>(content_length);
}

void NetworkServiceMemoryCache::OnLoaderCompleted(
    NetworkServiceMemoryCacheURLLoader* loader) {
  DCHECK(loader);
  auto it = url_loaders_.find(loader);
  DCHECK(it != url_loaders_.end());
  url_loaders_.erase(it);
}

void NetworkServiceMemoryCache::SetCurrentTimeForTesting(
    base::Time current_time) {
  current_time_for_testing_ = current_time;
}

mojom::URLResponseHeadPtr NetworkServiceMemoryCache::GetResponseHeadForTesting(
    const std::string& cache_key) {
  auto it = entries_.Peek(cache_key);
  if (it == entries_.end())
    return nullptr;
  return it->second->response_head.Clone();
}

void NetworkServiceMemoryCache::SetDataPipeCapacityForTesting(
    uint32_t capacity) {
  data_pipe_capacity_for_testing_ = capacity;
}

base::Time NetworkServiceMemoryCache::GetCurrentTime() {
  if (!current_time_for_testing_.is_null())
    return current_time_for_testing_;
  return base::Time::Now();
}

uint64_t NetworkServiceMemoryCache::GetNextTraceId() {
  return (reinterpret_cast<uint64_t>(this) << 32) | next_trace_id_++;
}

void NetworkServiceMemoryCache::ShrinkToTotalBytes() {
  while (!entries_.empty() && total_bytes_ > max_total_bytes_) {
    auto it = entries_.rbegin();
    DCHECK_GE(total_bytes_, it->second->content->size());
    total_bytes_ -= it->second->content->size();
    entries_.Erase(it);
  }
}

}  // namespace network
