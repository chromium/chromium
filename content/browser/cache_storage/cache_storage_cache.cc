// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/cache_storage/cache_storage_cache.h"

#include <stddef.h>

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_blob_to_disk_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_entry_handler.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_cache_observer.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/browser/cache_storage/cache_storage_trace_utils.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/quota/padding_key.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/fetch/fetch_api_request_headers_map.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseError;

namespace content {

namespace {

using ResponseHeaderMap = base::flat_map<std::string, std::string>;

const size_t kMaxQueryCacheResultBytes =
    1024 * 1024 * 10;  // 10MB query cache limit

// If the way that a cache's padding is calculated changes increment this
// version.
//
// History:
//
//   1: Uniform random 400K.
//   2: Uniform random 14,431K.
//   3: FetchAPIResponse.padding and separate side data padding.
const int32_t kCachePaddingAlgorithmVersion = 3;

// Maximum number of recursive QueryCacheOpenNextEntry() calls we permit
// before forcing an asynchronous task.
const int kMaxQueryCacheRecursiveDepth = 20;

using MetadataCallback =
    base::OnceCallback<void(std::unique_ptr<proto::CacheMetadata>)>;

network::mojom::FetchResponseType ProtoResponseTypeToFetchResponseType(
    proto::CacheResponse::ResponseType response_type) {
  switch (response_type) {
    case proto::CacheResponse::BASIC_TYPE:
      return network::mojom::FetchResponseType::kBasic;
    case proto::CacheResponse::CORS_TYPE:
      return network::mojom::FetchResponseType::kCors;
    case proto::CacheResponse::DEFAULT_TYPE:
      return network::mojom::FetchResponseType::kDefault;
    case proto::CacheResponse::ERROR_TYPE:
      return network::mojom::FetchResponseType::kError;
    case proto::CacheResponse::OPAQUE_TYPE:
      return network::mojom::FetchResponseType::kOpaque;
    case proto::CacheResponse::OPAQUE_REDIRECT_TYPE:
      return network::mojom::FetchResponseType::kOpaqueRedirect;
  }
  NOTREACHED_IN_MIGRATION();
  return network::mojom::FetchResponseType::kOpaque;
}

proto::CacheResponse::ResponseType FetchResponseTypeToProtoResponseType(
    network::mojom::FetchResponseType response_type) {
  switch (response_type) {
    case network::mojom::FetchResponseType::kBasic:
      return proto::CacheResponse::BASIC_TYPE;
    case network::mojom::FetchResponseType::kCors:
      return proto::CacheResponse::CORS_TYPE;
    case network::mojom::FetchResponseType::kDefault:
      return proto::CacheResponse::DEFAULT_TYPE;
    case network::mojom::FetchResponseType::kError:
      return proto::CacheResponse::ERROR_TYPE;
    case network::mojom::FetchResponseType::kOpaque:
      return proto::CacheResponse::OPAQUE_TYPE;
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      return proto::CacheResponse::OPAQUE_REDIRECT_TYPE;
  }
  NOTREACHED_IN_MIGRATION();
  return proto::CacheResponse::OPAQUE_TYPE;
}

// Assert that ConnectionInfo does not change since we cast it to
// an integer in order to serialize it to disk.
static_assert(static_cast<int>(net::HttpConnectionInfo::kUNKNOWN) == 0,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kHTTP1_1) == 1,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kDEPRECATED_SPDY2) == 2,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kDEPRECATED_SPDY3) == 3,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kHTTP2) == 4,
              "ConnectionInfo enum is stable");
static_assert(
    static_cast<int>(net::HttpConnectionInfo::kQUIC_UNKNOWN_VERSION) == 5,
    "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kDEPRECATED_HTTP2_14) ==
                  6,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kDEPRECATED_HTTP2_15) ==
                  7,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kHTTP0_9) == 8,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kHTTP1_0) == 9,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_32) == 10,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_33) == 11,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_34) == 12,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_35) == 13,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_36) == 14,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_37) == 15,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_38) == 16,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_39) == 17,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_40) == 18,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_41) == 19,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_42) == 20,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_43) == 21,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_Q099) == 22,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_44) == 23,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_45) == 24,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_46) == 25,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_47) == 26,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_999) == 27,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_Q048) == 28,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_Q049) == 29,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_Q050) == 30,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_T048) == 31,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_T049) == 32,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_T050) == 33,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_T099) == 34,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_DRAFT_25) == 35,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_DRAFT_27) == 36,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_DRAFT_28) == 37,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_DRAFT_29) == 38,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_T051) == 39,
              "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_RFC_V1) == 40,
              "ConnectionInfo enum is stable");
static_assert(
    static_cast<int>(net::HttpConnectionInfo::kDEPRECATED_QUIC_2_DRAFT_1) == 41,
    "ConnectionInfo enum is stable");
static_assert(static_cast<int>(net::HttpConnectionInfo::kQUIC_2_DRAFT_8) == 42,
              "ConnectionInfo enum is stable");
// The following assert needs to be changed every time a new value is added.
// It exists to prevent us from forgetting to add new values above.
static_assert(static_cast<int>(net::HttpConnectionInfo::kMaxValue) == 42,
              "Please add new values above and update this assert");

// Copy headers out of a cache entry and into a protobuf. The callback is
// guaranteed to be run.
void ReadMetadata(disk_cache::Entry* entry, MetadataCallback callback);
void ReadMetadataDidReadMetadata(disk_cache::Entry* entry,
                                 MetadataCallback callback,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 int rv);

// This method will return a normalized Cache URL. In this case, the only
// normalization being done is to remove the fragment (ref is a synonym
// for fragment).
GURL NormalizeCacheUrl(const GURL& url) {
  if (url.has_ref())
    return url.GetWithoutRef();

  return url;
}

bool VaryMatches(const blink::FetchAPIRequestHeadersMap& request,
                 const blink::FetchAPIRequestHeadersMap& cached_request,
                 network::mojom::FetchResponseType response_type,
                 const ResponseHeaderMap& response) {
  if (response_type == network::mojom::FetchResponseType::kOpaque)
    return true;

  auto vary_iter = base::ranges::find_if(
      response, [](const ResponseHeaderMap::value_type& pair) {
        return base::CompareCaseInsensitiveASCII(pair.first, "vary") == 0;
      });
  if (vary_iter == response.end())
    return true;

  for (const std::string& trimmed :
       base::SplitString(vary_iter->second, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    if (trimmed == "*")
      return false;

    auto request_iter = request.find(trimmed);
    auto cached_request_iter = cached_request.find(trimmed);

    // If the header exists in one but not the other, no match.
    if ((request_iter == request.end()) !=
        (cached_request_iter == cached_request.end()))
      return false;

    // If the header exists in one, it exists in both. Verify that the values
    // are equal.
    if (request_iter != request.end() &&
        request_iter->second != cached_request_iter->second)
      return false;
  }

  return true;
}

// Checks a batch operation list for duplicate entries. Returns any duplicate
// URL strings that were found. If the return value is empty, then there were no
// duplicates.
std::vector<std::string> FindDuplicateOperations(
    const std::vector<blink::mojom::BatchOperationPtr>& operations) {
  using blink::mojom::BatchOperation;

  std::vector<std::string> duplicate_url_list;

  if (operations.size() < 2) {
    return duplicate_url_list;
  }

  // Create a temporary sorted vector of the operations to support quickly
  // finding potentially duplicate entries.  Multiple entries may have the
  // same URL, but differ by VARY header, so a sorted list is easier to
  // work with than a map.
  //
  // Note, this will use 512 bytes of stack space on 64-bit devices.  The
  // static size attempts to accommodate most typical Cache.addAll() uses in
  // service worker install events while not blowing up the stack too much.
  absl::InlinedVector<BatchOperation*, 64> sorted;
  sorted.reserve(operations.size());
  for (const auto& op : operations) {
    sorted.push_back(op.get());
  }
  std::sort(sorted.begin(), sorted.end(),
            [](BatchOperation* left, BatchOperation* right) {
              return left->request->url < right->request->url;
            });

  // Check each entry in the sorted vector for any duplicates.  Since the
  // list is sorted we only need to inspect the immediate neighbors that
  // have the same URL.  This results in an average complexity of O(n log n).
  // If the entire list has entries with the same URL and different VARY
  // headers then this devolves into O(n^2).
  for (BatchOperation* const* outer = sorted.cbegin(); outer != sorted.cend();
       ++outer) {
    const BatchOperation* outer_op = *outer;

    // Note, the spec checks CacheQueryOptions like ignoreSearch, etc, but
    // currently there is no way for script to trigger a batch operation with
    // multiple entries and non-default options.  The only exposed API that
    // supports multiple operations is addAll() and it does not allow options
    // to be passed.  Therefore we assume we do not need to take any options
    // into account here.
    DCHECK(!outer_op->match_options);

    // If this entry already matches a duplicate we found, then just skip
    // ahead to find any remaining duplicates.
    if (!duplicate_url_list.empty() &&
        outer_op->request->url.spec() == duplicate_url_list.back()) {
      continue;
    }

    for (BatchOperation* const* inner = std::next(outer);
         inner != sorted.cend(); ++inner) {
      const BatchOperation* inner_op = *inner;
      // Since the list is sorted we can stop looking at neighbors after
      // the first different URL.
      if (outer_op->request->url != inner_op->request->url) {
        break;
      }

      // VaryMatches() is asymmetric since the operation depends on the VARY
      // header in the target response.  Since we only visit each pair of
      // entries once we need to perform the VaryMatches() call in both
      // directions.
      if (VaryMatches(outer_op->request->headers, inner_op->request->headers,
                      inner_op->response->response_type,
                      inner_op->response->headers) ||
          VaryMatches(outer_op->request->headers, inner_op->request->headers,
                      outer_op->response->response_type,
                      outer_op->response->headers)) {
        duplicate_url_list.push_back(inner_op->request->url.spec());
        break;
      }
    }
  }

  return duplicate_url_list;
}

GURL RemoveQueryParam(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

void ReadMetadata(disk_cache::Entry* entry, MetadataCallback callback) {
  DCHECK(entry);

  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(
          entry->GetDataSize(CacheStorageCache::INDEX_HEADERS));

  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      ReadMetadataDidReadMetadata, entry, std::move(callback), buffer));

  int read_rv =
      entry->ReadData(CacheStorageCache::INDEX_HEADERS, 0, buffer.get(),
                      buffer->size(), std::move(split_callback.first));

  if (read_rv != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(read_rv);
}

void ReadMetadataDidReadMetadata(disk_cache::Entry* entry,
                                 MetadataCallback callback,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 int rv) {
  if (rv != buffer->size()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<proto::CacheMetadata> metadata(new proto::CacheMetadata());

  if (!metadata->ParseFromArray(buffer->data(), buffer->size())) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(metadata));
}

bool ShouldPadResourceSize(const content::proto::CacheResponse* response) {
  return storage::ShouldPadResponseType(
      ProtoResponseTypeToFetchResponseType(response->response_type()));
}

bool ShouldPadResourceSize(const blink::mojom::FetchAPIResponse& response) {
  return storage::ShouldPadResponseType(response.response_type);
}

blink::mojom::FetchAPIRequestPtr CreateRequest(
    const proto::CacheMetadata& metadata,
    const GURL& request_url) {
  auto request = blink::mojom::FetchAPIRequest::New();
  request->url =
      metadata.request().has_fragment()
          ? net::AppendOrReplaceRef(request_url, metadata.request().fragment())
          : request_url;
  request->method = metadata.request().method();
  request->is_reload = false;
  request->referrer = blink::mojom::Referrer::New();
  request->headers = {};

  for (int i = 0; i < metadata.request().headers_size(); ++i) {
    const proto::CacheHeaderMap header = metadata.request().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    request->headers.insert(std::make_pair(header.name(), header.value()));
  }
  return request;
}

blink::mojom::FetchAPIResponsePtr CreateResponse(
    const proto::CacheMetadata& metadata,
    const std::string& cache_name) {
  // We no longer support Responses with only a single URL entry.  This field
  // was deprecated in M57.
  if (metadata.response().has_url())
    return nullptr;

  std::vector<GURL> url_list;
  url_list.reserve(metadata.response().url_list_size());
  for (int i = 0; i < metadata.response().url_list_size(); ++i)
    url_list.push_back(GURL(metadata.response().url_list(i)));

  ResponseHeaderMap headers;
  for (int i = 0; i < metadata.response().headers_size(); ++i) {
    const proto::CacheHeaderMap header = metadata.response().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    headers.insert(std::make_pair(header.name(), header.value()));
  }

  std::string alpn_negotiated_protocol =
      metadata.response().has_alpn_negotiated_protocol()
          ? metadata.response().alpn_negotiated_protocol()
          : "unknown";

  std::optional<std::string> mime_type;
  if (metadata.response().has_mime_type())
    mime_type = metadata.response().mime_type();

  std::optional<std::string> request_method;
  if (metadata.response().has_request_method())
    request_method = metadata.response().request_method();

  auto response_time =
      base::Time::FromInternalValue(metadata.response().response_time());

  int64_t padding = 0;
  if (metadata.response().has_padding()) {
    padding = metadata.response().padding();
  } else if (ShouldPadResourceSize(&metadata.response())) {
    padding = storage::ComputeRandomResponsePadding();
  }

  bool request_include_credentials =
      metadata.response().has_request_include_credentials()
          ? metadata.response().request_include_credentials()
          : true;

  // While we block most partial responses from being stored, we can have
  // partial responses for bgfetch or opaque responses.
  bool has_range_requested = headers.contains(net::HttpRequestHeaders::kRange);

  return blink::mojom::FetchAPIResponse::New(
      url_list, metadata.response().status_code(),
      metadata.response().status_text(),
      ProtoResponseTypeToFetchResponseType(metadata.response().response_type()),
      padding, network::mojom::FetchResponseSource::kCacheStorage, headers,
      mime_type, request_method, /*blob=*/nullptr,
      blink::mojom::ServiceWorkerResponseError::kUnknown, response_time,
      cache_name,
      std::vector<std::string>(
          metadata.response().cors_exposed_header_names().begin(),
          metadata.response().cors_exposed_header_names().end()),
      /*side_data_blob=*/nullptr, /*side_data_blob_for_cache_put=*/nullptr,
      network::mojom::ParsedHeaders::New(),
      // Default proto value of 0 maps to HttpConnectionInfo::kUNKNOWN.
      static_cast<net::HttpConnectionInfo>(
          metadata.response().connection_info()),
      alpn_negotiated_protocol, metadata.response().was_fetched_via_spdy(),
      has_range_requested, /*auth_challenge_info=*/std::nullopt,
      request_include_credentials);
}

int64_t CalculateSideDataPadding(
    const storage::BucketLocator& bucket_locator,
    const ::content::proto::CacheResponse* response,
    int side_data_size) {
  DCHECK(ShouldPadResourceSize(response));
  DCHECK_GE(side_data_size, 0);

  if (!side_data_size)
    return 0;

  // Fallback to random padding if this is for an older entry without
  // a url list or request method.
  if (response->url_list_size() == 0 || !response->has_request_method())
    return storage::ComputeRandomResponsePadding();

  const std::string& url = response->url_list(response->url_list_size() - 1);
  const base::Time response_time =
      base::Time::FromInternalValue(response->response_time());

  return storage::ComputeStableResponsePadding(
      bucket_locator.storage_key, url, response_time,
      response->request_method(), side_data_size);
}

net::RequestPriority GetDiskCachePriority(
    CacheStorageSchedulerPriority priority) {
  return priority == CacheStorageSchedulerPriority::kHigh ? net::HIGHEST
                                                          : net::MEDIUM;
}

}  // namespace

struct CacheStorageCache::QueryCacheResult {
  QueryCacheResult(base::Time entry_time,
                   int64_t padding,
                   int64_t side_data_padding)
      : entry_time(entry_time),
        padding(padding),
        side_data_padding(side_data_padding) {}

  blink::mojom::FetchAPIRequestPtr request;
  blink::mojom::FetchAPIResponsePtr response;
  disk_cache::ScopedEntryPtr entry;
  base::Time entry_time;
  int64_t padding = 0;
  int64_t side_data_padding = 0;
};

struct CacheStorageCache::QueryCacheContext {
  QueryCacheContext(blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::CacheQueryOptionsPtr options,
                    QueryCacheCallback callback,
                    QueryTypes query_types)
      : request(std::move(request)),
        options(std::move(options)),
        callback(std::move(callback)),
        query_types(query_types),
        matches(std::make_unique<QueryCacheResults>()) {}

  QueryCacheContext(const QueryCacheContext&) = delete;
  QueryCacheContext& operator=(const QueryCacheContext&) = delete;

  ~QueryCacheContext() = default;

  // Input to QueryCache
  blink::mojom::FetchAPIRequestPtr request;
  blink::mojom::CacheQueryOptionsPtr options;
  QueryCacheCallback callback;
  QueryTypes query_types = 0;
  size_t estimated_out_bytes = 0;

  // Iteration state
  std::unique_ptr<disk_cache::Backend::Iterator> backend_iterator;

  // Output of QueryCache
  std::unique_ptr<std::vector<QueryCacheResult>> matches;
};

struct CacheStorageCache::BatchInfo {
  size_t remaining_operations = 0;
  VerboseErrorCallback callback;
  std::optional<std::string> message;
  const int64_t trace_id = 0;
};

// static
std::unique_ptr<CacheStorageCache> CacheStorageCache::CreateMemoryCache(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner,
    const std::string& cache_name,
    CacheStorage* cache_storage,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context) {
  CacheStorageCache* cache = new CacheStorageCache(
      bucket_locator, owner, cache_name, base::FilePath(), cache_storage,
      std::move(scheduler_task_runner), std::move(quota_manager_proxy),
      std::move(blob_storage_context), /*cache_size=*/0,
      /*cache_padding=*/0);
  cache->SetObserver(cache_storage);
  cache->InitBackend();
  return base::WrapUnique(cache);
}

// static
std::unique_ptr<CacheStorageCache> CacheStorageCache::CreatePersistentCache(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner,
    const std::string& cache_name,
    CacheStorage* cache_storage,
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
    int64_t cache_size,
    int64_t cache_padding) {
  CacheStorageCache* cache = new CacheStorageCache(
      bucket_locator, owner, cache_name, path, cache_storage,
      std::move(scheduler_task_runner), std::move(quota_manager_proxy),
      std::move(blob_storage_context), cache_size, cache_padding);
  cache->SetObserver(cache_storage);
  cache->InitBackend();
  return base::WrapUnique(cache);
}

base::WeakPtr<CacheStorageCache> CacheStorageCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CacheStorageCacheHandle CacheStorageCache::CreateHandle() {
  return CacheStorageCacheHandle(weak_ptr_factory_.GetWeakPtr());
}

void CacheStorageCache::AddHandleRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handle_ref_count_ += 1;
  // Reference the parent CacheStorage while the Cache is referenced.  Some
  // code may only directly reference the Cache and we don't want to let the
  // CacheStorage cleanup if it becomes unreferenced in these cases.
  if (handle_ref_count_ == 1 && cache_storage_)
    cache_storage_handle_ = cache_storage_->CreateHandle();
}

void CacheStorageCache::DropHandleRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(handle_ref_count_, 0U);
  handle_ref_count_ -= 1;
  // Dropping the last reference may result in the parent CacheStorage
  // deleting itself or this Cache object.  Be careful not to touch the
  // `this` pointer in this method after the following code.
  if (handle_ref_count_ == 0 && cache_storage_) {
    CacheStorageHandle handle = std::move(cache_storage_handle_);
    cache_storage_->CacheUnreferenced(this);
  }
}

bool CacheStorageCache::IsUnreferenced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !handle_ref_count_;
}

void CacheStorageCache::Match(blink::mojom::FetchAPIRequestPtr request,
                              blink::mojom::CacheQueryOptionsPtr match_options,
                              CacheStorageSchedulerPriority priority,
                              int64_t trace_id,
                              ResponseCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kMatchBackendClosed), nullptr);
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared, CacheStorageSchedulerOp::kMatch,
      priority,
      base::BindOnce(
          &CacheStorageCache::MatchImpl, weak_ptr_factory_.GetWeakPtr(),
          std::move(request), std::move(match_options), trace_id, priority,
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void CacheStorageCache::MatchAll(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    int64_t trace_id,
    ResponsesCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kMatchAllBackendClosed),
        std::vector<blink::mojom::FetchAPIResponsePtr>());
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kMatchAll,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &CacheStorageCache::MatchAllImpl, weak_ptr_factory_.GetWeakPtr(),
          std::move(request), std::move(match_options), trace_id,
          CacheStorageSchedulerPriority::kNormal,
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void CacheStorageCache::WriteSideData(ErrorCallback callback,
                                      const GURL& url,
                                      base::Time expected_response_time,
                                      int64_t trace_id,
                                      scoped_refptr<net::IOBuffer> buffer,
                                      int buf_len) {
  if (backend_state_ == BACKEND_CLOSED) {
    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            MakeErrorStorage(ErrorStorageType::kWriteSideDataBackendClosed)));
    return;
  }

  // GetBucketSpaceRemaining is called before entering a scheduled operation
  // since it can call Size, another scheduled operation.
  quota_manager_proxy_->GetBucketSpaceRemaining(
      bucket_locator_, scheduler_task_runner_,
      base::BindOnce(
          &CacheStorageCache::WriteSideDataDidGetBucketSpaceRemaining,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), url,
          expected_response_time, trace_id, buffer, buf_len));
}

void CacheStorageCache::BatchOperation(
    std::vector<blink::mojom::BatchOperationPtr> operations,
    int64_t trace_id,
    VerboseErrorCallback callback,
    BadMessageCallback bad_message_callback) {
  // This method may produce a warning message that should be returned in the
  // final VerboseErrorCallback.  A message may be present in both the failure
  // and success paths.
  std::optional<std::string> message;

  if (backend_state_ == BACKEND_CLOSED) {
    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            CacheStorageVerboseError::New(
                MakeErrorStorage(ErrorStorageType::kBatchBackendClosed),
                std::move(message))));
    return;
  }

  // From BatchCacheOperations:
  //
  //   https://w3c.github.io/ServiceWorker/#batch-cache-operations-algorithm
  //
  // "If the result of running Query Cache with operation’s request,
  //  operation’s options, and addedItems is not empty, throw an
  //  InvalidStateError DOMException."

  if (const auto duplicate_url_list = FindDuplicateOperations(operations);
      !duplicate_url_list.empty()) {
    // If we found any duplicates we need to at least warn the user.  Format
    // the URL list into a comma-separated list.
    const std::string url_list_string =
        base::JoinString(duplicate_url_list, ", ");

    // Place the duplicate list into an error message.
    message.emplace(
        base::StringPrintf("duplicate requests (%s)", url_list_string.c_str()));

    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       CacheStorageVerboseError::New(
                           CacheStorageError::kErrorDuplicateOperation,
                           std::move(message))));
    return;
  }

  // Estimate the required size of the put operations. The size of the deletes
  // is unknown and not considered.
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  base::CheckedNumeric<uint64_t> safe_side_data_size = 0;
  for (const auto& operation : operations) {
    if (operation->operation_type == blink::mojom::OperationType::kPut) {
      safe_space_required += CalculateRequiredSafeSpaceForPut(operation);
      safe_side_data_size +=
          (operation->response->side_data_blob_for_cache_put
               ? operation->response->side_data_blob_for_cache_put->size
               : 0);
    }
  }
  if (!safe_space_required.IsValid() || !safe_side_data_size.IsValid()) {
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     std::move(bad_message_callback));
    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            CacheStorageVerboseError::New(
                MakeErrorStorage(ErrorStorageType::kBatchInvalidSpace),
                std::move(message))));
    return;
  }
  uint64_t space_required = safe_space_required.ValueOrDie();
  uint64_t side_data_size = safe_side_data_size.ValueOrDie();
  if (space_required || side_data_size) {
    quota_manager_proxy_->GetBucketSpaceRemaining(
        bucket_locator_, scheduler_task_runner_,
        base::BindOnce(&CacheStorageCache::BatchDidGetBucketSpaceRemaining,
                       weak_ptr_factory_.GetWeakPtr(), std::move(operations),
                       trace_id, std::move(callback),
                       std::move(bad_message_callback), std::move(message),
                       space_required, side_data_size));
    return;
  }

  BatchDidGetBucketSpaceRemaining(
      std::move(operations), trace_id, std::move(callback),
      std::move(bad_message_callback), std::move(message),
      0 /* space_required */, 0 /* side_data_size */, 0 /* space_remaining */);
}

void CacheStorageCache::BatchDidGetBucketSpaceRemaining(
    std::vector<blink::mojom::BatchOperationPtr> operations,
    int64_t trace_id,
    VerboseErrorCallback callback,
    BadMessageCallback bad_message_callback,
    std::optional<std::string> message,
    uint64_t space_required,
    uint64_t side_data_size,
    storage::QuotaErrorOr<int64_t> space_remaining) {
  TRACE_EVENT_WITH_FLOW1("CacheStorage",
                         "CacheStorageCache::BatchDidGetBucketSpaceRemaining",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "operations", CacheStorageTracedValue(operations));

  base::CheckedNumeric<uint64_t> safe_space_required = space_required;
  base::CheckedNumeric<uint64_t> safe_space_required_with_side_data;
  safe_space_required_with_side_data = safe_space_required + side_data_size;
  if (!safe_space_required.IsValid() ||
      !safe_space_required_with_side_data.IsValid()) {
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     std::move(bad_message_callback));
    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            CacheStorageVerboseError::New(
                MakeErrorStorage(
                    ErrorStorageType::kBatchDidGetUsageAndQuotaInvalidSpace),
                std::move(message))));
    return;
  }
  if (!space_remaining.has_value() ||
      safe_space_required.ValueOrDie() > space_remaining.value()) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  CacheStorageVerboseError::New(
                                      CacheStorageError::kErrorQuotaExceeded,
                                      std::move(message))));
    return;
  }
  bool skip_side_data = safe_space_required_with_side_data.ValueOrDie() >
                        static_cast<uint64_t>(space_remaining.value());

  auto completion_callback = base::BindRepeating(
      &CacheStorageCache::BatchDidOneOperation, weak_ptr_factory_.GetWeakPtr(),
      base::OwnedRef(BatchInfo{operations.size(), std::move(callback),
                               std::move(message), trace_id}));

  // Operations may synchronously invoke |callback| which could release the
  // last reference to this instance. Hold a handle for the duration of this
  // loop. (Asynchronous tasks scheduled by the operations use weak ptrs which
  // will no-op automatically.)
  CacheStorageCacheHandle handle = CreateHandle();

  for (auto& operation : operations) {
    switch (operation->operation_type) {
      case blink::mojom::OperationType::kPut:
        if (skip_side_data) {
          operation->response->side_data_blob_for_cache_put = nullptr;
          Put(std::move(operation), trace_id, completion_callback);
        } else {
          Put(std::move(operation), trace_id, completion_callback);
        }
        break;
      case blink::mojom::OperationType::kDelete:
        DCHECK_EQ(1u, operations.size());
        Delete(std::move(operation), completion_callback);
        break;
      case blink::mojom::OperationType::kUndefined:
        NOTREACHED_IN_MIGRATION();
        // TODO(nhiroki): This should return "TypeError".
        // http://crbug.com/425505
        completion_callback.Run(MakeErrorStorage(
            ErrorStorageType::kBatchDidGetUsageAndQuotaUndefinedOp));
        break;
    }
  }
}

void CacheStorageCache::BatchDidOneOperation(BatchInfo& batch_status,
                                             CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::BatchDidOneOperation",
                         TRACE_ID_GLOBAL(batch_status.trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // Nothing further to report after the callback is called.
  if (!batch_status.callback)
    return;

  DCHECK_GT(batch_status.remaining_operations, 0u);
  batch_status.remaining_operations--;

  if (error != CacheStorageError::kSuccess) {
    std::move(batch_status.callback)
        .Run(CacheStorageVerboseError::New(error,
                                           std::move(batch_status.message)));
  } else if (batch_status.remaining_operations == 0) {
    TRACE_EVENT_WITH_FLOW0(
        "CacheStorage", "CacheStorageCache::BatchDidAllOperations",
        TRACE_ID_GLOBAL(batch_status.trace_id),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
    std::move(batch_status.callback)
        .Run(CacheStorageVerboseError::New(CacheStorageError::kSuccess,
                                           batch_status.message));
  }
}

void CacheStorageCache::Keys(blink::mojom::FetchAPIRequestPtr request,
                             blink::mojom::CacheQueryOptionsPtr options,
                             int64_t trace_id,
                             RequestsCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysBackendClosed), nullptr);
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared, CacheStorageSchedulerOp::kKeys,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &CacheStorageCache::KeysImpl, weak_ptr_factory_.GetWeakPtr(),
          std::move(request), std::move(options), trace_id,
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void CacheStorageCache::Close(base::OnceClosure callback) {
  DCHECK_NE(BACKEND_CLOSED, backend_state_)
      << "Was CacheStorageCache::Close() called twice?";

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kClose, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &CacheStorageCache::CloseImpl, weak_ptr_factory_.GetWeakPtr(),
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void CacheStorageCache::Size(SizeCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    // TODO(jkarlin): Delete caches that can't be initialized.
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback), 0));
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared, CacheStorageSchedulerOp::kSize,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &CacheStorageCache::SizeImpl, weak_ptr_factory_.GetWeakPtr(),
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void CacheStorageCache::GetSizeThenClose(SizeCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback), 0));
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kSizeThenClose,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &CacheStorageCache::SizeImpl, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &CacheStorageCache::GetSizeThenCloseDidGetSize,
              weak_ptr_factory_.GetWeakPtr(),
              scheduler_->WrapCallbackToRunNext(id, std::move(callback)))));
}

void CacheStorageCache::SetObserver(CacheStorageCacheObserver* observer) {
  DCHECK((observer == nullptr) ^ (cache_observer_ == nullptr));
  cache_observer_ = observer;
}

// static
size_t CacheStorageCache::EstimatedStructSize(
    const blink::mojom::FetchAPIRequestPtr& request) {
  size_t size = sizeof(*request);
  size += request->url.spec().size();

  for (const auto& key_and_value : request->headers) {
    size += key_and_value.first.size();
    size += key_and_value.second.size();
  }

  return size;
}

CacheStorageCache::~CacheStorageCache() = default;

void CacheStorageCache::SetSchedulerForTesting(
    std::unique_ptr<CacheStorageScheduler> scheduler) {
  DCHECK(!scheduler_->ScheduledOperations());
  scheduler_ = std::move(scheduler);
}

CacheStorageCache::CacheStorageCache(
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner,
    const std::string& cache_name,
    const base::FilePath& path,
    CacheStorage* cache_storage,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
    int64_t cache_size,
    int64_t cache_padding)
    : bucket_locator_(bucket_locator),
      owner_(owner),
      cache_name_(cache_name),
      path_(path),
      cache_storage_(cache_storage),
      scheduler_task_runner_(std::move(scheduler_task_runner)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      scheduler_(new CacheStorageScheduler(CacheStorageSchedulerClient::kCache,
                                           scheduler_task_runner_)),
      cache_size_(cache_size),
      cache_padding_(cache_padding),
      max_query_size_bytes_(kMaxQueryCacheResultBytes),
      cache_observer_(nullptr),
      cache_entry_handler_(
          CacheStorageCacheEntryHandler::CreateCacheEntryHandler(
              owner,
              std::move(blob_storage_context))),
      memory_only_(path.empty()) {
  DCHECK(!bucket_locator_.storage_key.origin().opaque());
  DCHECK(quota_manager_proxy_.get());

  if (cache_size_ != CacheStorage::kSizeUnknown &&
      cache_padding_ != CacheStorage::kSizeUnknown) {
    // The size of this cache has already been reported to the QuotaManager.
    last_reported_size_ = cache_size_ + cache_padding_;
  }
}

void CacheStorageCache::QueryCache(blink::mojom::FetchAPIRequestPtr request,
                                   blink::mojom::CacheQueryOptionsPtr options,
                                   QueryTypes query_types,
                                   CacheStorageSchedulerPriority priority,
                                   QueryCacheCallback callback) {
  DCHECK_NE(
      QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_WITH_BODIES,
      query_types & (QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_WITH_BODIES));
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kQueryCacheBackendClosed), nullptr);
    return;
  }

  if (owner_ != storage::mojom::CacheStorageOwner::kBackgroundFetch &&
      (!options || !options->ignore_method) && request &&
      !request->method.empty() &&
      request->method != net::HttpRequestHeaders::kGetMethod) {
    std::move(callback).Run(CacheStorageError::kSuccess,
                            std::make_unique<QueryCacheResults>());
    return;
  }

  std::string request_url;
  if (request)
    request_url = NormalizeCacheUrl(request->url).spec();

  std::unique_ptr<QueryCacheContext> query_cache_context(
      new QueryCacheContext(std::move(request), std::move(options),
                            std::move(callback), query_types));
  if (query_cache_context->request &&
      !query_cache_context->request->url.is_empty() &&
      (!query_cache_context->options ||
       !query_cache_context->options->ignore_search)) {
    // There is no need to scan the entire backend, just open the exact
    // URL.

    auto split_callback = base::SplitOnceCallback(base::BindOnce(
        &CacheStorageCache::QueryCacheDidOpenFastPath,
        weak_ptr_factory_.GetWeakPtr(), std::move(query_cache_context)));

    disk_cache::EntryResult result =
        backend_->OpenEntry(request_url, GetDiskCachePriority(priority),
                            std::move(split_callback.first));
    if (result.net_error() != net::ERR_IO_PENDING)
      std::move(split_callback.second).Run(std::move(result));
    return;
  }

  query_cache_context->backend_iterator = backend_->CreateIterator();
  QueryCacheOpenNextEntry(std::move(query_cache_context));
}

void CacheStorageCache::QueryCacheDidOpenFastPath(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    disk_cache::EntryResult result) {
  if (result.net_error() != net::OK) {
    QueryCacheContext* results = query_cache_context.get();
    std::move(results->callback)
        .Run(CacheStorageError::kSuccess,
             std::move(query_cache_context->matches));
    return;
  }
  QueryCacheFilterEntry(std::move(query_cache_context), std::move(result));
}

void CacheStorageCache::QueryCacheOpenNextEntry(
    std::unique_ptr<QueryCacheContext> query_cache_context) {
  query_cache_recursive_depth_ += 1;
  auto cleanup = base::ScopedClosureRunner(base::BindOnce(
      [](CacheStorageCacheHandle handle) {
        CacheStorageCache* self = From(handle);
        if (!self)
          return;
        DCHECK_GT(self->query_cache_recursive_depth_, 0);
        self->query_cache_recursive_depth_ -= 1;
      },
      CreateHandle()));

  if (!query_cache_context->backend_iterator) {
    // Iteration is complete.
    std::sort(query_cache_context->matches->begin(),
              query_cache_context->matches->end(), QueryCacheResultCompare);

    std::move(query_cache_context->callback)
        .Run(CacheStorageError::kSuccess,
             std::move(query_cache_context->matches));
    return;
  }

  disk_cache::Backend::Iterator& iterator =
      *query_cache_context->backend_iterator;

  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &CacheStorageCache::QueryCacheFilterEntry, weak_ptr_factory_.GetWeakPtr(),
      std::move(query_cache_context)));

  disk_cache::EntryResult result =
      iterator.OpenNextEntry(std::move(split_callback.first));

  if (result.net_error() == net::ERR_IO_PENDING)
    return;

  // In most cases we can immediately invoke the callback when there is no
  // pending IO.  We must be careful, however, to avoid blowing out the stack
  // when iterating a large cache.  Only invoke the callback synchronously
  // if we have not recursed past a threshold depth.
  if (query_cache_recursive_depth_ <= kMaxQueryCacheRecursiveDepth) {
    std::move(split_callback.second).Run(std::move(result));
    return;
  }

  scheduler_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(split_callback.second), std::move(result)));
}

void CacheStorageCache::QueryCacheFilterEntry(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    disk_cache::EntryResult result) {
  if (result.net_error() == net::ERR_FAILED) {
    // This is the indicator that iteration is complete.
    query_cache_context->backend_iterator.reset();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  if (result.net_error() < 0) {
    std::move(query_cache_context->callback)
        .Run(MakeErrorStorage(ErrorStorageType::kQueryCacheFilterEntryFailed),
             std::move(query_cache_context->matches));
    return;
  }

  disk_cache::ScopedEntryPtr entry(result.ReleaseEntry());

  if (backend_state_ == BACKEND_CLOSED) {
    std::move(query_cache_context->callback)
        .Run(CacheStorageError::kErrorNotFound,
             std::move(query_cache_context->matches));
    return;
  }

  if (query_cache_context->request &&
      !query_cache_context->request->url.is_empty()) {
    GURL requestURL = NormalizeCacheUrl(query_cache_context->request->url);
    GURL cachedURL = GURL(entry->GetKey());

    if (query_cache_context->options &&
        query_cache_context->options->ignore_search) {
      requestURL = RemoveQueryParam(requestURL);
      cachedURL = RemoveQueryParam(cachedURL);
    }

    if (cachedURL != requestURL) {
      QueryCacheOpenNextEntry(std::move(query_cache_context));
      return;
    }
  }

  disk_cache::Entry* entry_ptr = entry.get();
  ReadMetadata(
      entry_ptr,
      base::BindOnce(&CacheStorageCache::QueryCacheDidReadMetadata,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(query_cache_context), std::move(entry)));
}

void CacheStorageCache::QueryCacheDidReadMetadata(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    disk_cache::ScopedEntryPtr entry,
    std::unique_ptr<proto::CacheMetadata> metadata) {
  if (!metadata) {
    entry->Doom();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  // Check for older cache entries that need to be padded, but don't
  // have any padding stored in the entry.  Upgrade these entries
  // as we encounter them.  This method will be re-entered once the
  // new paddings are written back to disk.
  if (ShouldPadResourceSize(&metadata->response()) &&
      !metadata->response().has_padding()) {
    QueryCacheUpgradePadding(std::move(query_cache_context), std::move(entry),
                             std::move(metadata));
    return;
  }

  // If the entry was created before we started adding entry times, then
  // default to using the Response object's time for sorting purposes.
  int64_t entry_time = metadata->has_entry_time()
                           ? metadata->entry_time()
                           : metadata->response().response_time();

  // Note, older entries that don't require padding may still not have
  // a padding value since we don't pay the cost to upgrade these entries.
  // Treat these as a zero padding.
  int64_t padding =
      metadata->response().has_padding() ? metadata->response().padding() : 0;
  int64_t side_data_padding = metadata->response().has_side_data_padding()
                                  ? metadata->response().side_data_padding()
                                  : 0;

  DCHECK(!ShouldPadResourceSize(&metadata->response()) ||
         (padding + side_data_padding));

  query_cache_context->matches->push_back(QueryCacheResult(
      base::Time::FromInternalValue(entry_time), padding, side_data_padding));
  QueryCacheResult* match = &query_cache_context->matches->back();
  match->request = CreateRequest(*metadata, GURL(entry->GetKey()));
  match->response = CreateResponse(*metadata, cache_name_);

  if (!match->response) {
    entry->Doom();
    query_cache_context->matches->pop_back();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  if (query_cache_context->request &&
      (!query_cache_context->options ||
       !query_cache_context->options->ignore_vary) &&
      !VaryMatches(query_cache_context->request->headers,
                   match->request->headers, match->response->response_type,
                   match->response->headers)) {
    query_cache_context->matches->pop_back();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  auto blob_entry = cache_entry_handler_->CreateDiskCacheBlobEntry(
      CreateHandle(), std::move(entry));

  if (query_cache_context->query_types & QUERY_CACHE_ENTRIES)
    match->entry = std::move(blob_entry->disk_cache_entry());

  if (query_cache_context->query_types & QUERY_CACHE_REQUESTS) {
    query_cache_context->estimated_out_bytes +=
        EstimatedStructSize(match->request);
    if (query_cache_context->estimated_out_bytes > max_query_size_bytes_) {
      std::move(query_cache_context->callback)
          .Run(CacheStorageError::kErrorQueryTooLarge, nullptr);
      return;
    }

    cache_entry_handler_->PopulateRequestBody(blob_entry, match->request.get());
  } else {
    match->request.reset();
  }

  if (query_cache_context->query_types & QUERY_CACHE_RESPONSES_WITH_BODIES) {
    query_cache_context->estimated_out_bytes +=
        EstimatedResponseSizeWithoutBlob(*match->response);
    if (query_cache_context->estimated_out_bytes > max_query_size_bytes_) {
      std::move(query_cache_context->callback)
          .Run(CacheStorageError::kErrorQueryTooLarge, nullptr);
      return;
    }
    if (blob_entry->disk_cache_entry()->GetDataSize(INDEX_RESPONSE_BODY) == 0) {
      QueryCacheOpenNextEntry(std::move(query_cache_context));
      return;
    }

    cache_entry_handler_->PopulateResponseBody(blob_entry,
                                               match->response.get());
  } else if (!(query_cache_context->query_types &
               QUERY_CACHE_RESPONSES_NO_BODIES)) {
    match->response.reset();
  }

  QueryCacheOpenNextEntry(std::move(query_cache_context));
}

void CacheStorageCache::QueryCacheUpgradePadding(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    disk_cache::ScopedEntryPtr entry,
    std::unique_ptr<proto::CacheMetadata> metadata) {
  DCHECK(ShouldPadResourceSize(&metadata->response()));

  // This should only be called while initializing because the padding
  // version change should trigger an immediate query of all resources
  // to recompute padding.
  DCHECK(initializing_);

  auto* response = metadata->mutable_response();
  response->set_padding(storage::ComputeRandomResponsePadding());
  response->set_side_data_padding(CalculateSideDataPadding(
      bucket_locator_, response, entry->GetDataSize(INDEX_SIDE_DATA)));

  // Get a temporary copy of the entry and metadata pointers before moving them
  // into base::BindOnce.
  disk_cache::Entry* temp_entry_ptr = entry.get();
  auto* temp_metadata_ptr = metadata.get();

  WriteMetadata(
      temp_entry_ptr, *temp_metadata_ptr,
      base::BindOnce(
          [](base::WeakPtr<CacheStorageCache> self,
             std::unique_ptr<QueryCacheContext> query_cache_context,
             disk_cache::ScopedEntryPtr entry,
             std::unique_ptr<proto::CacheMetadata> metadata, int expected_bytes,
             int rv) {
            if (!self)
              return;
            if (expected_bytes != rv) {
              entry->Doom();
              self->QueryCacheOpenNextEntry(std::move(query_cache_context));
              return;
            }
            // We must have a padding here in order to avoid infinite
            // recursion.
            DCHECK(metadata->response().has_padding());
            self->QueryCacheDidReadMetadata(std::move(query_cache_context),
                                            std::move(entry),
                                            std::move(metadata));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(query_cache_context),
          std::move(entry), std::move(metadata)));
}

// static
bool CacheStorageCache::QueryCacheResultCompare(const QueryCacheResult& lhs,
                                                const QueryCacheResult& rhs) {
  return lhs.entry_time < rhs.entry_time;
}

// static
size_t CacheStorageCache::EstimatedResponseSizeWithoutBlob(
    const blink::mojom::FetchAPIResponse& response) {
  size_t size = sizeof(blink::mojom::FetchAPIResponse);
  for (const auto& url : response.url_list)
    size += url.spec().size();
  size += response.status_text.size();
  if (response.cache_storage_cache_name)
    size += response.cache_storage_cache_name->size();
  for (const auto& key_and_value : response.headers) {
    size += key_and_value.first.size();
    size += key_and_value.second.size();
  }
  for (const auto& header : response.cors_exposed_header_names)
    size += header.size();
  return size;
}

// static
int32_t CacheStorageCache::GetResponsePaddingVersion() {
  return kCachePaddingAlgorithmVersion;
}

void CacheStorageCache::MatchImpl(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    int64_t trace_id,
    CacheStorageSchedulerPriority priority,
    ResponseCallback callback) {
  MatchAllImpl(
      std::move(request), std::move(match_options), trace_id, priority,
      base::BindOnce(&CacheStorageCache::MatchDidMatchAll,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CacheStorageCache::MatchDidMatchAll(
    ResponseCallback callback,
    CacheStorageError match_all_error,
    std::vector<blink::mojom::FetchAPIResponsePtr> match_all_responses) {
  if (match_all_error != CacheStorageError::kSuccess) {
    std::move(callback).Run(match_all_error, nullptr);
    return;
  }

  if (match_all_responses.empty()) {
    std::move(callback).Run(CacheStorageError::kErrorNotFound, nullptr);
    return;
  }

  std::move(callback).Run(CacheStorageError::kSuccess,
                          std::move(match_all_responses[0]));
}

void CacheStorageCache::MatchAllImpl(blink::mojom::FetchAPIRequestPtr request,
                                     blink::mojom::CacheQueryOptionsPtr options,
                                     int64_t trace_id,
                                     CacheStorageSchedulerPriority priority,
                                     ResponsesCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "CacheStorageCache::MatchAllImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(request), "options",
                         CacheStorageTracedValue(options));
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kStorageMatchAllBackendClosed),
        std::vector<blink::mojom::FetchAPIResponsePtr>());
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  QueryCache(std::move(request), std::move(options),
             QUERY_CACHE_REQUESTS | QUERY_CACHE_RESPONSES_WITH_BODIES, priority,
             base::BindOnce(&CacheStorageCache::MatchAllDidQueryCache,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            trace_id));
}

void CacheStorageCache::MatchAllDidQueryCache(
    ResponsesCallback callback,
    int64_t trace_id,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::MatchAllDidQueryCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error,
                            std::vector<blink::mojom::FetchAPIResponsePtr>());
    return;
  }

  std::vector<blink::mojom::FetchAPIResponsePtr> out_responses;
  out_responses.reserve(query_cache_results->size());

  for (auto& result : *query_cache_results) {
    out_responses.push_back(std::move(result.response));
  }

  std::move(callback).Run(CacheStorageError::kSuccess,
                          std::move(out_responses));
}

void CacheStorageCache::WriteMetadata(disk_cache::Entry* entry,
                                      const proto::CacheMetadata& metadata,
                                      WriteMetadataCallback callback) {
  std::string serialized;
  if (!metadata.SerializeToString(&serialized)) {
    std::move(callback).Run(0, -1);
    return;
  }

  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(std::move(serialized));

  auto callback_with_expected_bytes = base::BindOnce(
      [](WriteMetadataCallback callback, int expected_bytes, int rv) {
        std::move(callback).Run(expected_bytes, rv);
      },
      std::move(callback), buffer->size());

  auto split_callback =
      base::SplitOnceCallback(std::move(callback_with_expected_bytes));

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  int rv = entry->WriteData(INDEX_HEADERS, /*offset=*/0, buffer.get(),
                            buffer->size(), std::move(split_callback.first),
                            /*truncate=*/true);

  if (rv != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(rv);
}

void CacheStorageCache::WriteSideDataDidGetBucketSpaceRemaining(
    ErrorCallback callback,
    const GURL& url,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    storage::QuotaErrorOr<int64_t> space_remaining) {
  TRACE_EVENT_WITH_FLOW0(
      "CacheStorage",
      "CacheStorageCache::WriteSideDataDidGetBucketSpaceRemaining",
      TRACE_ID_GLOBAL(trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (!space_remaining.has_value() || space_remaining.value() < buf_len) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  CacheStorageError::kErrorQuotaExceeded));
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kWriteSideData,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&CacheStorageCache::WriteSideDataImpl,
                     weak_ptr_factory_.GetWeakPtr(),
                     scheduler_->WrapCallbackToRunNext(id, std::move(callback)),
                     url, expected_response_time, trace_id, buffer, buf_len));
}

void CacheStorageCache::WriteSideDataImpl(ErrorCallback callback,
                                          const GURL& url,
                                          base::Time expected_response_time,
                                          int64_t trace_id,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          int buf_len) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorageCache::WriteSideDataImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "url", url.spec());
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kWriteSideDataImplBackendClosed));
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageCache::WriteSideDataDidOpenEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     expected_response_time, trace_id, buffer, buf_len));

  // Note, the simple disk_cache priority is not important here because we
  // only allow one write operation at a time.  Therefore there will be no
  // competing operations in the disk_cache queue.
  disk_cache::EntryResult result =
      backend_->OpenEntry(NormalizeCacheUrl(url).spec(), net::MEDIUM,
                          std::move(split_callback.first));
  if (result.net_error() != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(std::move(result));
}

void CacheStorageCache::WriteSideDataDidOpenEntry(
    ErrorCallback callback,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    disk_cache::EntryResult result) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::WriteSideDataDidOpenEntry",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (result.net_error() != net::OK) {
    std::move(callback).Run(CacheStorageError::kErrorNotFound);
    return;
  }

  // Give the ownership of entry to a ScopedWritableEntry which will doom the
  // entry before closing unless we tell it that writing has successfully
  // completed via WritingCompleted.
  ScopedWritableEntry entry(result.ReleaseEntry());
  disk_cache::Entry* entry_ptr = entry.get();

  ReadMetadata(entry_ptr,
               base::BindOnce(&CacheStorageCache::WriteSideDataDidReadMetaData,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), expected_response_time,
                              trace_id, buffer, buf_len, std::move(entry)));
}

void CacheStorageCache::WriteSideDataDidReadMetaData(
    ErrorCallback callback,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    ScopedWritableEntry entry,
    std::unique_ptr<proto::CacheMetadata> headers) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::WriteSideDataDidReadMetaData",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (!headers || headers->response().response_time() !=
                      expected_response_time.ToInternalValue()) {
    WriteSideDataComplete(std::move(callback), std::move(entry),
                          /*padding=*/0, /*side_data_padding=*/0,
                          CacheStorageError::kErrorNotFound);
    return;
  }
  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = entry.get();

  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |callback|, |entry| and
  // |response| are not copyable.
  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageCache::WriteSideDataDidWrite,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(entry), buf_len, std::move(headers), trace_id));

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  int rv = temp_entry_ptr->WriteData(
      INDEX_SIDE_DATA, 0 /* offset */, buffer.get(), buf_len,
      std::move(split_callback.first), true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(rv);
}

void CacheStorageCache::WriteSideDataDidWrite(
    ErrorCallback callback,
    ScopedWritableEntry entry,
    int expected_bytes,
    std::unique_ptr<::content::proto::CacheMetadata> metadata,
    int64_t trace_id,
    int rv) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::WriteSideDataDidWrite",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN);
  if (rv != expected_bytes) {
    WriteSideDataComplete(std::move(callback), std::move(entry),
                          /*padding=*/0, /*side_data_padding=*/0,
                          CacheStorageError::kErrorStorage);
    return;
  }

  auto* response = metadata->mutable_response();

  if (ShouldPadResourceSize(response)) {
    cache_padding_ -= response->side_data_padding();

    response->set_side_data_padding(
        CalculateSideDataPadding(bucket_locator_, response, rv));
    cache_padding_ += response->side_data_padding();

    // Get a temporary copy of the entry pointer before passing it in
    // base::Bind.
    disk_cache::Entry* temp_entry_ptr = entry.get();

    WriteMetadata(
        temp_entry_ptr, *metadata,
        base::BindOnce(&CacheStorageCache::WriteSideDataDidWriteMetadata,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(entry), response->padding(),
                       response->side_data_padding()));
    return;
  }

  WriteSideDataComplete(std::move(callback), std::move(entry),
                        response->padding(), response->side_data_padding(),
                        CacheStorageError::kSuccess);
}

void CacheStorageCache::WriteSideDataDidWriteMetadata(ErrorCallback callback,
                                                      ScopedWritableEntry entry,
                                                      int64_t padding,
                                                      int64_t side_data_padding,
                                                      int expected_bytes,
                                                      int rv) {
  auto result = blink::mojom::CacheStorageError::kSuccess;
  if (rv != expected_bytes) {
    result = MakeErrorStorage(
        ErrorStorageType::kWriteSideDataDidWriteMetadataWrongBytes);
  }
  WriteSideDataComplete(std::move(callback), std::move(entry), padding,
                        side_data_padding, result);
}

void CacheStorageCache::WriteSideDataComplete(
    ErrorCallback callback,
    ScopedWritableEntry entry,
    int64_t padding,
    int64_t side_data_padding,
    blink::mojom::CacheStorageError error) {
  if (error != CacheStorageError::kSuccess) {
    // If we found the entry, then we possibly wrote something and now we're
    // dooming the entry, causing a change in size, so update the size before
    // returning.
    if (error != CacheStorageError::kErrorNotFound) {
      entry.reset();
      cache_padding_ -= (padding + side_data_padding);
      UpdateCacheSize(base::BindOnce(std::move(callback), error));
      return;
    }

    entry.get_deleter()
        .WritingCompleted();  // Since we didn't change the entry.
    std::move(callback).Run(error);
    return;
  }

  entry.get_deleter().WritingCompleted();  // Since we didn't change the entry.
  UpdateCacheSize(base::BindOnce(std::move(callback), error));
}

void CacheStorageCache::Put(blink::mojom::BatchOperationPtr operation,
                            int64_t trace_id,
                            ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(blink::mojom::OperationType::kPut, operation->operation_type);
  Put(std::move(operation->request), std::move(operation->response), trace_id,
      std::move(callback));
}

void CacheStorageCache::Put(blink::mojom::FetchAPIRequestPtr request,
                            blink::mojom::FetchAPIResponsePtr response,
                            int64_t trace_id,
                            ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);

  auto put_context = cache_entry_handler_->CreatePutContext(
      std::move(request), std::move(response), trace_id);
  auto id = scheduler_->CreateId();
  put_context->callback =
      scheduler_->WrapCallbackToRunNext(id, std::move(callback));

  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive, CacheStorageSchedulerOp::kPut,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&CacheStorageCache::PutImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context)));
}

void CacheStorageCache::PutImpl(std::unique_ptr<PutContext> put_context) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW2(
      "CacheStorage", "CacheStorageCache::PutImpl",
      TRACE_ID_GLOBAL(put_context->trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "request",
      CacheStorageTracedValue(put_context->request), "response",
      CacheStorageTracedValue(put_context->response));
  if (backend_state_ != BACKEND_OPEN) {
    PutComplete(std::move(put_context),
                MakeErrorStorage(ErrorStorageType::kPutImplBackendClosed));
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  put_context->callback =
      WrapCallbackWithHandle(std::move(put_context->callback));

  // Explicitly delete the incumbent resource (which may not exist). This is
  // only done so that it's padding will be decremented from the calculated
  // cache padding.
  // TODO(cmumford): Research alternatives to this explicit delete as it
  // seriously impacts put performance.
  auto delete_request = blink::mojom::FetchAPIRequest::New();
  delete_request->url = put_context->request->url;
  delete_request->method = "";
  delete_request->is_reload = false;
  delete_request->referrer = blink::mojom::Referrer::New();
  delete_request->headers = {};

  blink::mojom::CacheQueryOptionsPtr query_options =
      blink::mojom::CacheQueryOptions::New();
  query_options->ignore_method = true;
  query_options->ignore_vary = true;
  DeleteImpl(
      std::move(delete_request), std::move(query_options),
      base::BindOnce(&CacheStorageCache::PutDidDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context)));
}

void CacheStorageCache::PutDidDeleteEntry(
    std::unique_ptr<PutContext> put_context,
    CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "CacheStorageCache::PutDidDeleteEntry",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (backend_state_ != BACKEND_OPEN) {
    PutComplete(
        std::move(put_context),
        MakeErrorStorage(ErrorStorageType::kPutDidDeleteEntryBackendClosed));
    return;
  }

  if (error != CacheStorageError::kSuccess &&
      error != CacheStorageError::kErrorNotFound) {
    PutComplete(std::move(put_context), error);
    return;
  }

  const blink::mojom::FetchAPIRequest& request_ = *(put_context->request);
  disk_cache::Backend* backend_ptr = backend_.get();

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageCache::PutDidCreateEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context)));

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  disk_cache::EntryResult result = backend_ptr->OpenOrCreateEntry(
      NormalizeCacheUrl(request_.url).spec(), net::MEDIUM,
      std::move(split_callback.first));

  if (result.net_error() != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(std::move(result));
}

void CacheStorageCache::PutDidCreateEntry(
    std::unique_ptr<PutContext> put_context,
    disk_cache::EntryResult result) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "CacheStorageCache::PutDidCreateEntry",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  int rv = result.net_error();

  // Moving the entry into a ScopedWritableEntry which will doom the entry
  // before closing unless we tell it that writing has successfully completed
  // via WritingCompleted.
  put_context->cache_entry.reset(result.ReleaseEntry());

  if (rv != net::OK) {
    quota_manager_proxy_->OnClientWriteFailed(bucket_locator_.storage_key);
    PutComplete(std::move(put_context), CacheStorageError::kErrorExists);
    return;
  }

  proto::CacheMetadata metadata;
  metadata.set_entry_time(base::Time::Now().ToInternalValue());
  proto::CacheRequest* request_metadata = metadata.mutable_request();
  request_metadata->set_method(put_context->request->method);
  if (put_context->request->url.has_ref())
    request_metadata->set_fragment(put_context->request->url.ref());

  for (const auto& header : put_context->request->headers) {
    DCHECK_EQ(std::string::npos, header.first.find('\0'));
    DCHECK_EQ(std::string::npos, header.second.find('\0'));
    proto::CacheHeaderMap* header_map = request_metadata->add_headers();
    header_map->set_name(header.first);
    header_map->set_value(header.second);
  }

  proto::CacheResponse* response_metadata = metadata.mutable_response();
  if (owner_ != storage::mojom::CacheStorageOwner::kBackgroundFetch &&
      put_context->response->response_type !=
          network::mojom::FetchResponseType::kOpaque &&
      put_context->response->response_type !=
          network::mojom::FetchResponseType::kOpaqueRedirect) {
    DCHECK_NE(put_context->response->status_code, net::HTTP_PARTIAL_CONTENT);
  }
  response_metadata->set_status_code(put_context->response->status_code);
  response_metadata->set_status_text(put_context->response->status_text);
  response_metadata->set_response_type(FetchResponseTypeToProtoResponseType(
      put_context->response->response_type));
  for (const auto& url : put_context->response->url_list)
    response_metadata->add_url_list(url.spec());
  response_metadata->set_connection_info(
      static_cast<int32_t>(put_context->response->connection_info));
  response_metadata->set_alpn_negotiated_protocol(
      put_context->response->alpn_negotiated_protocol);
  response_metadata->set_was_fetched_via_spdy(
      put_context->response->was_fetched_via_spdy);
  if (put_context->response->mime_type.has_value())
    response_metadata->set_mime_type(put_context->response->mime_type.value());
  if (put_context->response->request_method.has_value()) {
    response_metadata->set_request_method(
        put_context->response->request_method.value());
  }
  response_metadata->set_response_time(
      put_context->response->response_time.ToInternalValue());
  for (ResponseHeaderMap::const_iterator it =
           put_context->response->headers.begin();
       it != put_context->response->headers.end(); ++it) {
    DCHECK_EQ(std::string::npos, it->first.find('\0'));
    DCHECK_EQ(std::string::npos, it->second.find('\0'));
    proto::CacheHeaderMap* header_map = response_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }
  for (const auto& header : put_context->response->cors_exposed_header_names)
    response_metadata->add_cors_exposed_header_names(header);

  DCHECK(!ShouldPadResourceSize(*put_context->response) ||
         put_context->response->padding);
  response_metadata->set_padding(put_context->response->padding);

  int64_t side_data_padding = 0;
  if (ShouldPadResourceSize(*put_context->response) &&
      put_context->side_data_blob) {
    side_data_padding = CalculateSideDataPadding(
        bucket_locator_, response_metadata, put_context->side_data_blob_size);
  }
  response_metadata->set_side_data_padding(side_data_padding);
  response_metadata->set_request_include_credentials(
      put_context->response->request_include_credentials);

  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = put_context->cache_entry.get();

  WriteMetadata(
      temp_entry_ptr, metadata,
      base::BindOnce(&CacheStorageCache::PutDidWriteHeaders,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context),
                     response_metadata->padding(),
                     response_metadata->side_data_padding()));
}

void CacheStorageCache::PutDidWriteHeaders(
    std::unique_ptr<PutContext> put_context,
    int64_t padding,
    int64_t side_data_padding,
    int expected_bytes,
    int rv) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::PutDidWriteHeaders",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (rv != expected_bytes) {
    quota_manager_proxy_->OnClientWriteFailed(bucket_locator_.storage_key);
    PutComplete(
        std::move(put_context),
        MakeErrorStorage(ErrorStorageType::kPutDidWriteHeadersWrongBytes));
    return;
  }

  DCHECK(!ShouldPadResourceSize(*put_context->response) ||
         (padding + side_data_padding));
  cache_padding_ += padding + side_data_padding;

  PutWriteBlobToCache(std::move(put_context), INDEX_RESPONSE_BODY);
}

void CacheStorageCache::PutWriteBlobToCache(
    std::unique_ptr<PutContext> put_context,
    int disk_cache_body_index) {
  DCHECK(disk_cache_body_index == INDEX_RESPONSE_BODY ||
         disk_cache_body_index == INDEX_SIDE_DATA);

  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::PutWriteBlobToCache",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  mojo::PendingRemote<blink::mojom::Blob> blob;
  int64_t blob_size = 0;

  switch (disk_cache_body_index) {
    case INDEX_RESPONSE_BODY: {
      blob = std::move(put_context->blob);
      put_context->blob.reset();
      blob_size = put_context->blob_size;
      break;
    }
    case INDEX_SIDE_DATA: {
      blob = std::move(put_context->side_data_blob);
      put_context->side_data_blob.reset();
      blob_size = put_context->side_data_blob_size;
      break;
    }
    case INDEX_HEADERS:
      NOTREACHED_IN_MIGRATION();
  }

  ScopedWritableEntry entry(put_context->cache_entry.release());

  // If there isn't blob data for this index, then we may need to clear any
  // pre-existing data.  This can happen under rare circumstances if a stale
  // file is present and accepted by OpenOrCreateEntry().
  if (!blob) {
    disk_cache::Entry* temp_entry_ptr = entry.get();

    auto clear_callback =
        base::BindOnce(&CacheStorageCache::PutWriteBlobToCacheComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(put_context),
                       disk_cache_body_index, std::move(entry));

    // If there is no pre-existing data, then proceed to the next
    // step immediately.
    if (temp_entry_ptr->GetDataSize(disk_cache_body_index) != 0) {
      std::move(clear_callback).Run(net::OK);
      return;
    }

    auto split_callback = base::SplitOnceCallback(std::move(clear_callback));

    // There is pre-existing data and we need to truncate it.
    int rv = temp_entry_ptr->WriteData(
        disk_cache_body_index, /* offset = */ 0, /* buf = */ nullptr,
        /* buf_len = */ 0, std::move(split_callback.first),
        /* truncate = */ true);

    if (rv != net::ERR_IO_PENDING)
      std::move(split_callback.second).Run(rv);

    return;
  }

  // We have real data, so stream it into the entry.  This will overwrite
  // any existing data.
  auto blob_to_cache = std::make_unique<CacheStorageBlobToDiskCache>(
      quota_manager_proxy_, bucket_locator_.storage_key);
  CacheStorageBlobToDiskCache* blob_to_cache_raw = blob_to_cache.get();
  BlobToDiskCacheIDMap::KeyType blob_to_cache_key =
      active_blob_to_disk_cache_writers_.Add(std::move(blob_to_cache));

  blob_to_cache_raw->StreamBlobToCache(
      std::move(entry), disk_cache_body_index, std::move(blob), blob_size,
      base::BindOnce(&CacheStorageCache::PutDidWriteBlobToCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context),
                     blob_to_cache_key, disk_cache_body_index));
}

void CacheStorageCache::PutDidWriteBlobToCache(
    std::unique_ptr<PutContext> put_context,
    BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
    int disk_cache_body_index,
    ScopedWritableEntry entry,
    bool success) {
  DCHECK(entry);
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::PutDidWriteBlobToCache",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  active_blob_to_disk_cache_writers_.Remove(blob_to_cache_key);

  PutWriteBlobToCacheComplete(std::move(put_context), disk_cache_body_index,
                              std::move(entry),
                              (success ? net::OK : net::ERR_FAILED));
}

void CacheStorageCache::PutWriteBlobToCacheComplete(
    std::unique_ptr<PutContext> put_context,
    int disk_cache_body_index,
    ScopedWritableEntry entry,
    int rv) {
  DCHECK(entry);

  put_context->cache_entry = std::move(entry);

  if (rv != net::OK) {
    PutComplete(
        std::move(put_context),
        MakeErrorStorage(ErrorStorageType::kPutDidWriteBlobToCacheFailed));
    return;
  }

  if (disk_cache_body_index == INDEX_RESPONSE_BODY) {
    PutWriteBlobToCache(std::move(put_context), INDEX_SIDE_DATA);
    return;
  }

  PutComplete(std::move(put_context), CacheStorageError::kSuccess);
}

void CacheStorageCache::PutComplete(std::unique_ptr<PutContext> put_context,
                                    blink::mojom::CacheStorageError error) {
  if (error == CacheStorageError::kSuccess) {
    // Make sure we've written everything.
    DCHECK(put_context->cache_entry);
    DCHECK(!put_context->blob);
    DCHECK(!put_context->side_data_blob);

    // Tell the WritableScopedEntry not to doom the entry since it was a
    // successful operation.
    put_context->cache_entry.get_deleter().WritingCompleted();
  }

  UpdateCacheSize(base::BindOnce(std::move(put_context->callback), error));
}

void CacheStorageCache::CalculateCacheSizePadding(
    SizePaddingCallback got_sizes_callback) {
  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &CacheStorageCache::CalculateCacheSizePaddingGotSize,
      weak_ptr_factory_.GetWeakPtr(), std::move(got_sizes_callback)));

  int64_t rv =
      backend_->CalculateSizeOfAllEntries(std::move(split_callback.first));
  if (rv != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(rv);
}

void CacheStorageCache::CalculateCacheSizePaddingGotSize(
    SizePaddingCallback callback,
    int64_t cache_size) {
  // Enumerating entries is only done during cache initialization and only if
  // necessary.
  DCHECK_EQ(backend_state_, BACKEND_UNINITIALIZED);
  auto request = blink::mojom::FetchAPIRequest::New();
  blink::mojom::CacheQueryOptionsPtr options =
      blink::mojom::CacheQueryOptions::New();
  options->ignore_search = true;
  QueryCache(std::move(request), std::move(options),
             QUERY_CACHE_RESPONSES_NO_BODIES,
             CacheStorageSchedulerPriority::kNormal,
             base::BindOnce(&CacheStorageCache::PaddingDidQueryCache,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            cache_size));
}

void CacheStorageCache::PaddingDidQueryCache(
    SizePaddingCallback callback,
    int64_t cache_size,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  int64_t cache_padding = 0;
  if (error == CacheStorageError::kSuccess) {
    for (const auto& result : *query_cache_results) {
      DCHECK(!ShouldPadResourceSize(*result.response) ||
             (result.padding + result.side_data_padding));
      cache_padding += result.padding + result.side_data_padding;
    }
  }

  std::move(callback).Run(cache_size, cache_padding);
}

void CacheStorageCache::CalculateCacheSize(
    net::Int64CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int64_t rv =
      backend_->CalculateSizeOfAllEntries(std::move(split_callback.first));
  if (rv != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(rv);
}

void CacheStorageCache::UpdateCacheSize(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_state_ != BACKEND_OPEN)
    return;

  // Note that the callback holds a cache handle to keep the cache alive during
  // the operation since this UpdateCacheSize is often run after an operation
  // completes and runs its callback.
  CalculateCacheSize(base::BindOnce(&CacheStorageCache::UpdateCacheSizeGotSize,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    CreateHandle(), std::move(callback)));
}

void CacheStorageCache::UpdateCacheSizeGotSize(
    CacheStorageCacheHandle cache_handle,
    base::OnceClosure callback,
    int64_t current_cache_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(current_cache_size, CacheStorage::kSizeUnknown);
  cache_size_ = current_cache_size;
  int64_t size_delta = PaddedCacheSize() - last_reported_size_;
  last_reported_size_ = PaddedCacheSize();

  quota_manager_proxy_->NotifyBucketModified(
      CacheStorageQuotaClient::GetClientTypeFromOwner(owner_), bucket_locator_,
      size_delta, base::Time::Now(), scheduler_task_runner_,
      base::BindOnce(&CacheStorageCache::UpdateCacheSizeNotifiedStorageModified,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CacheStorageCache::UpdateCacheSizeNotifiedStorageModified(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cache_storage_)
    cache_storage_->NotifyCacheContentChanged(cache_name_);

  if (cache_observer_)
    cache_observer_->CacheSizeUpdated(this);

  std::move(callback).Run();
}

void CacheStorageCache::GetAllMatchedEntries(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr options,
    int64_t trace_id,
    CacheEntriesCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysBackendClosed), {});
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kGetAllMatched,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &CacheStorageCache::GetAllMatchedEntriesImpl,
          weak_ptr_factory_.GetWeakPtr(), std::move(request),
          std::move(options), trace_id,
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void CacheStorageCache::GetAllMatchedEntriesImpl(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr options,
    int64_t trace_id,
    CacheEntriesCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW2("CacheStorage",
                         "CacheStorageCache::GetAllMatchedEntriesImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(request), "options",
                         CacheStorageTracedValue(options));
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(
            ErrorStorageType::kStorageGetAllMatchedEntriesBackendClosed),
        {});
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  QueryCache(
      std::move(request), std::move(options),
      QUERY_CACHE_REQUESTS | QUERY_CACHE_RESPONSES_WITH_BODIES,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&CacheStorageCache::GetAllMatchedEntriesDidQueryCache,
                     weak_ptr_factory_.GetWeakPtr(), trace_id,
                     std::move(callback)));
}

void CacheStorageCache::GetAllMatchedEntriesDidQueryCache(
    int64_t trace_id,
    CacheEntriesCallback callback,
    blink::mojom::CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "CacheStorageCache::GetAllMatchedEntriesDidQueryCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error, {});
    return;
  }

  std::vector<blink::mojom::CacheEntryPtr> entries;
  entries.reserve(query_cache_results->size());

  for (auto& result : *query_cache_results) {
    auto entry = blink::mojom::CacheEntry::New();
    entry->request = std::move(result.request);
    entry->response = std::move(result.response);
    entries.push_back(std::move(entry));
  }

  std::move(callback).Run(CacheStorageError::kSuccess, std::move(entries));
}

CacheStorageCache::InitState CacheStorageCache::GetInitState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return initializing_ ? InitState::Initializing : InitState::Initialized;
}

void CacheStorageCache::Delete(blink::mojom::BatchOperationPtr operation,
                               ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(blink::mojom::OperationType::kDelete, operation->operation_type);

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = operation->request->url;
  request->method = operation->request->method;
  request->is_reload = operation->request->is_reload;
  request->referrer = operation->request->referrer.Clone();
  request->headers = operation->request->headers;

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kDelete, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &CacheStorageCache::DeleteImpl, weak_ptr_factory_.GetWeakPtr(),
          std::move(request), std::move(operation->match_options),
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void CacheStorageCache::DeleteImpl(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    ErrorCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kDeleteImplBackendClosed));
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  QueryCache(
      std::move(request), std::move(match_options),
      QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_NO_BODIES,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&CacheStorageCache::DeleteDidQueryCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CacheStorageCache::DeleteDidQueryCache(
    ErrorCallback callback,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error);
    return;
  }

  if (query_cache_results->empty()) {
    std::move(callback).Run(CacheStorageError::kErrorNotFound);
    return;
  }

  DCHECK(scheduler_->IsRunningExclusiveOperation());

  for (auto& result : *query_cache_results) {
    disk_cache::ScopedEntryPtr entry = std::move(result.entry);
    if (ShouldPadResourceSize(*result.response)) {
      DCHECK(!ShouldPadResourceSize(*result.response) ||
             (result.padding + result.side_data_padding));
      cache_padding_ -= (result.padding + result.side_data_padding);
    }
    entry->Doom();
  }

  UpdateCacheSize(
      base::BindOnce(std::move(callback), CacheStorageError::kSuccess));
}

void CacheStorageCache::KeysImpl(blink::mojom::FetchAPIRequestPtr request,
                                 blink::mojom::CacheQueryOptionsPtr options,
                                 int64_t trace_id,
                                 RequestsCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "CacheStorageCache::KeysImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(request), "options",
                         CacheStorageTracedValue(options));

  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysImplBackendClosed), nullptr);
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  QueryCache(std::move(request), std::move(options), QUERY_CACHE_REQUESTS,
             CacheStorageSchedulerPriority::kNormal,
             base::BindOnce(&CacheStorageCache::KeysDidQueryCache,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            trace_id));
}

void CacheStorageCache::KeysDidQueryCache(
    RequestsCallback callback,
    int64_t trace_id,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "CacheStorageCache::KeysDidQueryCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error, nullptr);
    return;
  }

  std::unique_ptr<Requests> out_requests = std::make_unique<Requests>();
  out_requests->reserve(query_cache_results->size());
  for (auto& result : *query_cache_results)
    out_requests->push_back(std::move(result.request));
  std::move(callback).Run(CacheStorageError::kSuccess, std::move(out_requests));
}

void CacheStorageCache::CloseImpl(base::OnceClosure callback) {
  DCHECK_EQ(BACKEND_OPEN, backend_state_);

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  backend_.reset();
  post_backend_closed_callback_ = std::move(callback);
}

void CacheStorageCache::DeleteBackendCompletedIO() {
  if (!post_backend_closed_callback_.is_null()) {
    DCHECK_NE(BACKEND_CLOSED, backend_state_);
    backend_state_ = BACKEND_CLOSED;
    std::move(post_backend_closed_callback_).Run();
  }
}

void CacheStorageCache::SizeImpl(SizeCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);

  // TODO(cmumford): Can CacheStorage::kSizeUnknown be returned instead of zero?
  if (backend_state_ != BACKEND_OPEN) {
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback), 0));
    return;
  }

  int64_t size = backend_state_ == BACKEND_OPEN ? PaddedCacheSize() : 0;
  scheduler_task_runner_->PostTask(FROM_HERE,
                                   base::BindOnce(std::move(callback), size));
}

void CacheStorageCache::GetSizeThenCloseDidGetSize(SizeCallback callback,
                                                   int64_t cache_size) {
  cache_entry_handler_->InvalidateDiskCacheBlobEntrys();
  CloseImpl(base::BindOnce(std::move(callback), cache_size));
}

void CacheStorageCache::CreateBackend(ErrorCallback callback) {
  DCHECK(!backend_);

  // Use APP_CACHE as opposed to DISK_CACHE to prevent cache eviction.
  net::CacheType cache_type = memory_only_ ? net::MEMORY_CACHE : net::APP_CACHE;

  // The maximum size of each cache. Ultimately, cache size
  // is controlled per storage key by the QuotaManager.
  int64_t max_bytes = memory_only_ ? std::numeric_limits<int>::max()
                                   : std::numeric_limits<int64_t>::max();

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageCache::CreateBackendDidCreate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  disk_cache::BackendResult result = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr, path_,
      max_bytes, disk_cache::ResetHandling::kNeverReset, /*net_log=*/nullptr,
      base::BindOnce(&CacheStorageCache::DeleteBackendCompletedIO,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(split_callback.first));
  if (result.net_error != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(std::move(result));
}

void CacheStorageCache::CreateBackendDidCreate(
    CacheStorageCache::ErrorCallback callback,
    disk_cache::BackendResult result) {
  if (result.net_error != net::OK) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kCreateBackendDidCreateFailed));
    return;
  }

  backend_ = std::move(result.backend);
  std::move(callback).Run(CacheStorageError::kSuccess);
}

void CacheStorageCache::InitBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(BACKEND_UNINITIALIZED, backend_state_);
  DCHECK(!initializing_);
  DCHECK(!scheduler_->ScheduledOperations());
  initializing_ = true;

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive, CacheStorageSchedulerOp::kInit,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &CacheStorageCache::CreateBackend, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &CacheStorageCache::InitDidCreateBackend,
              weak_ptr_factory_.GetWeakPtr(),
              scheduler_->WrapCallbackToRunNext(id, base::BindOnce([] {})))));
}

void CacheStorageCache::InitDidCreateBackend(
    base::OnceClosure callback,
    CacheStorageError cache_create_error) {
  if (cache_create_error != CacheStorageError::kSuccess) {
    InitGotCacheSize(std::move(callback), cache_create_error, 0);
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int64_t rv = backend_->CalculateSizeOfAllEntries(base::BindOnce(
      &CacheStorageCache::InitGotCacheSize, weak_ptr_factory_.GetWeakPtr(),
      std::move(split_callback.first), cache_create_error));

  if (rv != net::ERR_IO_PENDING)
    InitGotCacheSize(std::move(split_callback.second), cache_create_error, rv);
}

void CacheStorageCache::InitGotCacheSize(base::OnceClosure callback,
                                         CacheStorageError cache_create_error,
                                         int64_t cache_size) {
  if (cache_create_error != CacheStorageError::kSuccess) {
    InitGotCacheSizeAndPadding(std::move(callback), cache_create_error, 0, 0);
    return;
  }

  // Now that we know the cache size either 1) the cache size should be unknown
  // (which is why the size was calculated), or 2) it must match the current
  // size. If the sizes aren't equal then there is a bug in how the cache size
  // is saved in the store's index.
  if (cache_size_ != CacheStorage::kSizeUnknown) {
    DLOG_IF(ERROR, cache_size_ != cache_size)
        << "Cache size: " << cache_size
        << " does not match size from index: " << cache_size_;
    if (cache_size_ != cache_size) {
      // We assume that if the sizes match then then cached padding is still
      // correct. If not then we recalculate the padding.
      CalculateCacheSizePaddingGotSize(
          base::BindOnce(&CacheStorageCache::InitGotCacheSizeAndPadding,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         cache_create_error),
          cache_size);
      return;
    }
  }

  if (cache_padding_ == CacheStorage::kSizeUnknown || cache_padding_ < 0) {
    CalculateCacheSizePaddingGotSize(
        base::BindOnce(&CacheStorageCache::InitGotCacheSizeAndPadding,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       cache_create_error),
        cache_size);
    return;
  }

  // If cached size matches actual size then assume cached padding is still
  // correct.
  InitGotCacheSizeAndPadding(std::move(callback), cache_create_error,
                             cache_size, cache_padding_);
}

void CacheStorageCache::InitGotCacheSizeAndPadding(
    base::OnceClosure callback,
    CacheStorageError cache_create_error,
    int64_t cache_size,
    int64_t cache_padding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_size_ = cache_size;
  cache_padding_ = cache_padding;

  initializing_ = false;
  backend_state_ = (cache_create_error == CacheStorageError::kSuccess &&
                    backend_ && backend_state_ == BACKEND_UNINITIALIZED)
                       ? BACKEND_OPEN
                       : BACKEND_CLOSED;

  if (cache_observer_)
    cache_observer_->CacheSizeUpdated(this);

  std::move(callback).Run();
}

int64_t CacheStorageCache::PaddedCacheSize() const {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (cache_size_ == CacheStorage::kSizeUnknown ||
      cache_padding_ == CacheStorage::kSizeUnknown) {
    return CacheStorage::kSizeUnknown;
  }
  return cache_size_ + cache_padding_;
}

base::CheckedNumeric<uint64_t>
CacheStorageCache::CalculateRequiredSafeSpaceForPut(
    const blink::mojom::BatchOperationPtr& operation) {
  DCHECK_EQ(blink::mojom::OperationType::kPut, operation->operation_type);
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  safe_space_required +=
      CalculateRequiredSafeSpaceForResponse(operation->response);
  safe_space_required +=
      CalculateRequiredSafeSpaceForRequest(operation->request);

  return safe_space_required;
}

base::CheckedNumeric<uint64_t>
CacheStorageCache::CalculateRequiredSafeSpaceForRequest(
    const blink::mojom::FetchAPIRequestPtr& request) {
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  safe_space_required += request->method.size();

  safe_space_required += request->url.spec().size();

  for (const auto& header : request->headers) {
    safe_space_required += header.first.size();
    safe_space_required += header.second.size();
  }

  return safe_space_required;
}

base::CheckedNumeric<uint64_t>
CacheStorageCache::CalculateRequiredSafeSpaceForResponse(
    const blink::mojom::FetchAPIResponsePtr& response) {
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  safe_space_required += (response->blob ? response->blob->size : 0);
  safe_space_required += response->status_text.size();

  for (const auto& header : response->headers) {
    safe_space_required += header.first.size();
    safe_space_required += header.second.size();
  }

  for (const auto& header : response->cors_exposed_header_names) {
    safe_space_required += header.size();
  }

  for (const auto& url : response->url_list) {
    safe_space_required += url.spec().size();
  }

  return safe_space_required;
}

}  // namespace content
