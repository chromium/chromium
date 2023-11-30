// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_memory_cache.h"

#include <algorithm>
#include <limits>
#include <string_view>

#include "base/bit_cast.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/network_isolation_key.h"
#include "net/base/transport_info.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/http/http_vary_data.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service_memory_cache_url_loader.h"
#include "services/network/network_service_memory_cache_writer.h"
#include "services/network/private_network_access_checker.h"
#include "services/network/public/cpp/corb/corb_api.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/private_network_access_check_result.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BlockedByRequestHeaderReason {
  kIfUnmodifiedSince = 0,
  kIfMatch = 1,
  kIfRange = 2,
  kIfModifiedSince = 3,
  kIfNoneMatch = 4,
  kCacheControlNoCache = 5,
  kPragmaNoCache = 6,
  kCacheControlMaxAgeZero = 7,
  kRange = 8,
  kMaxValue = kRange,
};

struct HeaderNameAndValue {
  const char* name;
  const char* value;
  BlockedByRequestHeaderReason reason;
};

// Collected from kPassThroughHeaders, kValidationHeaders, kForceFetchHeaders,
// kForceValidateHeaders, in //net/http/http_cache_transaction.cc.
// TODO(https://crbug.com/1339708): It'd be worthwhile to remove the
// duplication.
constexpr HeaderNameAndValue kSpecialHeaders[] = {
    {"if-unmodified-since", nullptr,
     BlockedByRequestHeaderReason::kIfUnmodifiedSince},
    {"if-match", nullptr, BlockedByRequestHeaderReason::kIfMatch},
    {"if-range", nullptr, BlockedByRequestHeaderReason::kIfRange},
    {"if-modified-since", nullptr,
     BlockedByRequestHeaderReason::kIfModifiedSince},
    {"if-none-match", nullptr, BlockedByRequestHeaderReason::kIfNoneMatch},
    {"cache-control", "no-cache",
     BlockedByRequestHeaderReason::kCacheControlNoCache},
    {"pragma", "no-cache", BlockedByRequestHeaderReason::kPragmaNoCache},
    {"cache-control", "max-age=0",
     BlockedByRequestHeaderReason::kCacheControlMaxAgeZero},
    // The in-memory cache doesn't support range requests.
    {"range", nullptr, BlockedByRequestHeaderReason::kRange},
};

// TODO(https://crbug.com/1339708): Adjust these parameters based on stats.
const base::FeatureParam<int> kNetworkServiceMemoryCacheMaxTotalSize{
    &features::kNetworkServiceMemoryCache, "max_total_size", 64 * 1024 * 1024};
const base::FeatureParam<int> kNetworkServiceMemoryCacheMaxPerEntrySize{
    &features::kNetworkServiceMemoryCache, "max_per_entry_size",
    4 * 1024 * 1024};

absl::optional<std::string> GenerateCacheKeyForResourceRequest(
    const ResourceRequest& resource_request,
    const net::NetworkIsolationKey& network_isolation_key) {
  const bool is_subframe_document_resource =
      resource_request.destination == mojom::RequestDestination::kIframe;
  return net::HttpCache::GenerateCacheKey(
      resource_request.url, resource_request.load_flags, network_isolation_key,
      /*upload_data_identifier=*/0, is_subframe_document_resource);
}

absl::optional<std::string> GenerateCacheKeyForURLRequest(
    const net::URLRequest& url_request,
    mojom::RequestDestination request_destination) {
  bool is_subframe_document_resource =
      request_destination == mojom::RequestDestination::kIframe;
  return net::HttpCache::GenerateCacheKey(
      url_request.url(), url_request.load_flags(),
      url_request.isolation_info().network_isolation_key(),
      /*upload_data_identifier=*/0, is_subframe_document_resource);
}

bool CheckCrossOriginReadBlocking(const ResourceRequest& resource_request,
                                  const mojom::URLResponseHead& response,
                                  const base::RefCountedBytes& content) {
  // Using an empty per-URLLoaderFactory state may result in blocking the stored
  // response unnecessarily. Such a false-positive result is fine because when
  // the stored response is blocked, CorsURLLoader falls back to a URLLoader and
  // the URLLoader performs appropriate Opaque Resource Blocking checks.
  //
  // TODO(https://crbug.com/1339708): Consider moving CORB/ORB handling from
  // URLLoader to CorsURLLoader. It will eliminate the need for CORB/ORB checks
  // here.
  corb::PerFactoryState state;
  auto analyzer = corb::ResponseAnalyzer::Create(state);
  corb::ResponseAnalyzer::Decision decision = analyzer->Init(
      resource_request.url, resource_request.request_initiator,
      resource_request.mode, resource_request.destination, response);

  if (decision == corb::ResponseAnalyzer::Decision::kSniffMore) {
    const size_t size =
        std::min(static_cast<size_t>(net::kMaxBytesToSniff), content.size());
    decision = analyzer->Sniff(
        std::string_view(base::bit_cast<const char*>(content.front()), size));
    if (decision == corb::ResponseAnalyzer::Decision::kSniffMore)
      decision = analyzer->HandleEndOfSniffableResponseBody();
    DCHECK_NE(decision, corb::ResponseAnalyzer::Decision::kSniffMore);
  }

  return decision == corb::ResponseAnalyzer::Decision::kAllow;
}

bool CheckPrivateNetworkAccess(
    uint32_t load_options,
    const ResourceRequest& resource_request,
    const mojom::ClientSecurityState* factory_client_security_state,
    const net::TransportInfo& transport_info) {
  PrivateNetworkAccessChecker checker(
      resource_request, factory_client_security_state, load_options);
  PrivateNetworkAccessCheckResult result = checker.Check(transport_info);
  return !PrivateNetworkAccessCheckResultToCorsError(result).has_value();
}

// Checks whether Vary header in the cached response only has headers that the
// in-memory cache can handle.
bool VaryHasSupportedHeadersOnly(
    const net::HttpResponseHeaders& cached_response_headers) {
  size_t iter = 0;
  std::string value;
  while (cached_response_headers.EnumerateHeader(
      &iter, net::HttpResponseHeaders::kVary, &value)) {
    // Accept-Encoding will be set if missing.
    if (value == net::HttpRequestHeaders::kAcceptEncoding)
      continue;
    // Origin header might be missing or already be specified by the client
    // side. The underlying layer (URLLoader and //net) didn't set/update Origin
    // header unless cross-origin redirects happened. The in-memory cache
    // doesn't store response when redirects happened.
    if (value == net::HttpRequestHeaders::kOrigin)
      continue;

    // TODO(https://crbug.com/1339708): Support more headers. We need to extract
    // some header calculations from net::URLRequestHttpJob.
    return false;
  }

  return true;
}

absl::optional<BlockedByRequestHeaderReason> CheckSpecialRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  for (const auto& [name, value, reason] : kSpecialHeaders) {
    std::string header_value;
    if (!headers.GetHeader(name, &header_value))
      continue;
    // `nullptr` means `header_value` doesn't matter.
    if (value == nullptr)
      return reason;
    net::HttpUtil::ValuesIterator v(header_value.begin(), header_value.end(),
                                    ',');
    while (v.GetNext()) {
      if (base::EqualsCaseInsensitiveASCII(v.value_piece(), value))
        return reason;
    }
  }
  return absl::nullopt;
}

bool MatchVaryHeader(const ResourceRequest& resource_request,
                     const net::HttpVaryData& vary_data,
                     const net::HttpResponseHeaders& cached_response_headers,
                     bool enable_brotli,
                     bool enable_zstd) {
  if ((resource_request.load_flags & net::LOAD_SKIP_VARY_CHECK) ||
      !vary_data.is_valid()) {
    return true;
  }

  if (!VaryHasSupportedHeadersOnly(cached_response_headers))
    return false;

  net::HttpRequestInfo request_info;
  request_info.extra_headers = resource_request.headers;
  request_info.extra_headers.SetAcceptEncodingIfMissing(
      resource_request.url, resource_request.devtools_accepted_stream_types,
      enable_brotli, enable_zstd);
  return vary_data.MatchesRequest(request_info, cached_response_headers);
}

}  // namespace

struct NetworkServiceMemoryCache::Entry {
  Entry(const net::HttpVaryData& vary_data,
        const net::TransportInfo& transport_info,
        mojom::URLResponseHeadPtr response_head,
        scoped_refptr<base::RefCountedBytes> content,
        int64_t encoded_body_length)
      : vary_data(vary_data),
        transport_info(transport_info),
        response_head(std::move(response_head)),
        content(std::move(content)),
        encoded_body_length(encoded_body_length) {}
  ~Entry() = default;

  // Movable.
  Entry(Entry&&) = default;
  Entry& operator=(Entry&&) = default;
  Entry(const Entry&) = delete;
  Entry& operator=(const Entry&) = delete;

  net::HttpVaryData vary_data;
  net::TransportInfo transport_info;
  mojom::URLResponseHeadPtr response_head;
  scoped_refptr<base::RefCountedBytes> content;
  int64_t encoded_body_length;
};

NetworkServiceMemoryCache::NetworkServiceMemoryCache(
    NetworkContext* network_context)
    : network_context_(network_context),
      entries_(CacheMap::NO_AUTO_EVICT),
      max_total_bytes_(kNetworkServiceMemoryCacheMaxTotalSize.Get()),
      max_per_entry_bytes_(kNetworkServiceMemoryCacheMaxPerEntrySize.Get()) {
  DCHECK(network_context_);
  DCHECK_GE(max_total_bytes_, max_per_entry_bytes_);
  DCHECK_GE(static_cast<size_t>(std::numeric_limits<int>::max()),
            max_per_entry_bytes_);

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&NetworkServiceMemoryCache::OnMemoryPressure,
                          base::Unretained(this)));
}

NetworkServiceMemoryCache::~NetworkServiceMemoryCache() = default;

void NetworkServiceMemoryCache::Clear() {
  entries_.Clear();
  total_bytes_ = 0;
}

base::WeakPtr<NetworkServiceMemoryCache>
NetworkServiceMemoryCache::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<NetworkServiceMemoryCacheWriter>
NetworkServiceMemoryCache::MaybeCreateWriter(
    net::URLRequest* url_request,
    mojom::RequestDestination request_destination,
    const net::TransportInfo& transport_info,
    const mojom::URLResponseHeadPtr& response) {
  DCHECK(url_request);

  // TODO(https://crbug.com/1339708): Make `this` work with
  // SplitCacheByIncludeCredentials. Currently some tests are failing when
  // the feature is enabled.
  if (base::FeatureList::IsEnabled(
          net::features::kSplitCacheByIncludeCredentials)) {
    return nullptr;
  }

  DCHECK(url_request->url().is_valid());
  if (!url_request->url().SchemeIsHTTPOrHTTPS())
    return nullptr;

  const absl::optional<std::string> cache_key =
      GenerateCacheKeyForURLRequest(*url_request, request_destination);
  if (!cache_key.has_value())
    return nullptr;

  if (url_request->method() != net::HttpRequestHeaders::kGetMethod) {
    // Invalidate the entry when the method is unsafe, as specified at
    // https://fetch.spec.whatwg.org/#http-network-or-cache-fetch.
    // This is a bit overkilling (for example, we don't see the response
    // status), for the ease of implementation.
    auto it = entries_.Peek(*cache_key);
    if (it != entries_.end()) {
      EraseEntry(it);
    }
    return nullptr;
  }

  // TODO(https://crbug.com/1339708): Make `this` work for responses from
  // private network. Currently some tests are failing.
  if (response->response_address_space == mojom::IPAddressSpace::kPrivate) {
    return nullptr;
  }

  if (!response->headers || response->headers->response_code() != net::HTTP_OK)
    return nullptr;

  const int load_flags = url_request->load_flags();
  if (load_flags & net::LOAD_BYPASS_CACHE ||
      load_flags & net::LOAD_DISABLE_CACHE) {
    return nullptr;
  }

  // See comments in net::HttpCache::Transaction::WriteResponseInfoToEntry().
  if (net::IsCertStatusError(url_request->ssl_info().cert_status))
    return nullptr;

  if (CheckSpecialRequestHeaders(url_request->extra_request_headers())
          .has_value()) {
    return nullptr;
  }

  if (response->content_length > static_cast<int>(max_per_entry_bytes_))
    return nullptr;

  net::ValidationType validation_type = response->headers->RequiresValidation(
      response->request_time, response->response_time, GetCurrentTime());
  if (validation_type != net::VALIDATION_NONE)
    return nullptr;

  return std::make_unique<NetworkServiceMemoryCacheWriter>(
      weak_ptr_factory_.GetWeakPtr(), GetNextTraceId(), max_per_entry_bytes_,
      std::move(*cache_key), url_request, request_destination, transport_info,
      response);
}

void NetworkServiceMemoryCache::StoreResponse(
    const std::string& cache_key,
    const URLLoaderCompletionStatus& status,
    mojom::RequestDestination request_destination,
    const net::HttpVaryData& vary_data,
    const net::TransportInfo& transport_info,
    mojom::URLResponseHeadPtr response_head,
    std::vector<unsigned char> data) {
  DCHECK_GE(max_per_entry_bytes_, data.size());

  // TODO(https://crbug.com/1339708): Consider caching a response that doesn't
  // have contents.
  if (status.error_code != net::OK || data.size() == 0)
    return;

  net::HttpResponseHeaders::FreshnessLifetimes lifetimes =
      response_head->headers->GetFreshnessLifetimes(
          response_head->response_time);
  if (lifetimes.freshness.is_zero()) {
    // The corresponding URLRequest was cancelled.
    return;
  }

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
  auto entry = std::make_unique<Entry>(
      vary_data, transport_info, std::move(response_head), std::move(content),
      status.encoded_body_length);
  entries_.Put(cache_key, std::move(entry));

  ShrinkToTotalBytes();
}

absl::optional<std::string> NetworkServiceMemoryCache::CanServe(
    uint32_t load_options,
    const ResourceRequest& resource_request,
    const net::NetworkIsolationKey& network_isolation_key,
    const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    const mojom::ClientSecurityState* factory_client_security_state) {
  // TODO(https://crbug.com/1339708): Support automatically assigned network
  // isolation key for request from browsers. See comments in
  // CorsURLLoaderFactory::CorsURLLoaderFactory.

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

  // We hit a DCHECK failure without this early return. Let's have this
  // workaround for now.
  // TODO(crbug.com/1360815): Remove this, and handle this request correctly.
  if (resource_request.trusted_params &&
      !resource_request.trusted_params->isolation_info.IsEmpty()) {
    return absl::nullopt;
  }

  absl::optional<std::string> cache_key = GenerateCacheKeyForResourceRequest(
      resource_request, network_isolation_key);
  if (!cache_key.has_value())
    return absl::nullopt;

  auto it = entries_.Peek(*cache_key);
  if (it == entries_.end()) {
    return absl::nullopt;
  }

  absl::optional<BlockedByRequestHeaderReason> blocked_by_headers =
      CheckSpecialRequestHeaders(resource_request.headers);
  if (blocked_by_headers.has_value()) {
    return absl::nullopt;
  }

  if (!CheckPrivateNetworkAccess(load_options, resource_request,
                                 factory_client_security_state,
                                 it->second->transport_info)) {
    return absl::nullopt;
  }

  const mojom::URLResponseHeadPtr& response = it->second->response_head;

  absl::optional<mojom::BlockedByResponseReason> blocked_reason =
      CrossOriginResourcePolicy::IsBlocked(
          /*request_url=*/url, /*original_url=*/url,
          resource_request.request_initiator, *response, resource_request.mode,
          resource_request.destination, cross_origin_embedder_policy,
          /*reporter=*/nullptr);
  if (blocked_reason.has_value())
    return absl::nullopt;

  if (!CheckCrossOriginReadBlocking(resource_request, *response,
                                    *it->second->content)) {
    return absl::nullopt;
  }

  if (!MatchVaryHeader(
          resource_request, it->second->vary_data, *response->headers,
          network_context_->url_request_context()->enable_brotli(),
          network_context_->url_request_context()->enable_zstd())) {
    return absl::nullopt;
  }

  net::ValidationType validation_type = response->headers->RequiresValidation(
      response->request_time, response->response_time, GetCurrentTime());
  if (validation_type != net::VALIDATION_NONE) {
    // The cached response is stale, erase it from the in-memory cache.
    EraseEntry(it);
    return absl::nullopt;
  }

  return std::move(*cache_key);
}

void NetworkServiceMemoryCache::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const std::string& cache_key,
    const ResourceRequest& resource_request,
    const net::NetLogWithSource net_log,
    const absl::optional<net::CookiePartitionKey> cookie_partition_key,
    mojo::PendingRemote<mojom::URLLoaderClient> client) {
  auto it = entries_.Get(cache_key);
  CHECK(it != entries_.end());

  auto loader = std::make_unique<NetworkServiceMemoryCacheURLLoader>(
      this, GetNextTraceId(), resource_request, net_log, std::move(receiver),
      std::move(client), it->second->content, it->second->encoded_body_length,
      std::move(cookie_partition_key));
  NetworkServiceMemoryCacheURLLoader* raw_loader = loader.get();
  url_loaders_.insert(std::move(loader));

  raw_loader->Start(resource_request, it->second->response_head.Clone());
}

uint32_t NetworkServiceMemoryCache::GetDataPipeCapacity(size_t content_length) {
  if (data_pipe_capacity_for_testing_.has_value())
    return *data_pipe_capacity_for_testing_;

  uint32_t default_capacity = features::GetDataPipeDefaultAllocationSize(
      features::DataPipeAllocationSize::kLargerSizeIfPossible);
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

void NetworkServiceMemoryCache::OnRedirect(
    const net::URLRequest* url_request,
    mojom::RequestDestination request_destination) {
  DCHECK(url_request);

  if (url_request->method() != net::HttpRequestHeaders::kGetMethod)
    return;

  absl::optional<std::string> cache_key =
      GenerateCacheKeyForURLRequest(*url_request, request_destination);
  if (!cache_key.has_value())
    return;

  auto it = entries_.Peek(*cache_key);
  if (it != entries_.end())
    EraseEntry(it);
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

void NetworkServiceMemoryCache::EraseEntry(CacheMap::iterator it) {
  DCHECK(it != entries_.end());
  DCHECK_GE(total_bytes_, it->second->content->size());
  total_bytes_ -= it->second->content->size();
  entries_.Erase(it);
}

void NetworkServiceMemoryCache::ShrinkToTotalBytes() {
  while (!entries_.empty() && total_bytes_ > max_total_bytes_) {
    auto it = --entries_.end();
    EraseEntry(it);
  }
}

void NetworkServiceMemoryCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MemoryPressureLevel::
                   MEMORY_PRESSURE_LEVEL_CRITICAL) {
    Clear();
  }
}

}  // namespace network
