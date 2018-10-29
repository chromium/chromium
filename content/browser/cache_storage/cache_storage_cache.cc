// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache.h"

#include <stddef.h>
#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_blob_to_disk_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_cache_observer.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "storage/common/storage_histograms.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseError;

namespace content {

namespace {

using ResponseHeaderMap = base::flat_map<std::string, std::string>;

const size_t kMaxQueryCacheResultBytes =
    1024 * 1024 * 10;  // 10MB query cache limit

const char kRecordBytesLabel[] = "DiskCache.CacheStorage";

// The range of the padding added to response sizes for opaque resources.
// Increment padding version if changed.
const uint64_t kPaddingRange = 14431 * 1024;

// If the way that a cache's padding is calculated changes increment this
// version.
//
// History:
//
//   1: Uniform random 400K.
//   2: Uniform random 14,431K.
const int32_t kCachePaddingAlgorithmVersion = 2;

using MetadataCallback =
    base::OnceCallback<void(std::unique_ptr<proto::CacheMetadata>)>;

network::mojom::FetchResponseType ProtoResponseTypeToFetchResponseType(
    proto::CacheResponse::ResponseType response_type) {
  switch (response_type) {
    case proto::CacheResponse::BASIC_TYPE:
      return network::mojom::FetchResponseType::kBasic;
    case proto::CacheResponse::CORS_TYPE:
      return network::mojom::FetchResponseType::kCORS;
    case proto::CacheResponse::DEFAULT_TYPE:
      return network::mojom::FetchResponseType::kDefault;
    case proto::CacheResponse::ERROR_TYPE:
      return network::mojom::FetchResponseType::kError;
    case proto::CacheResponse::OPAQUE_TYPE:
      return network::mojom::FetchResponseType::kOpaque;
    case proto::CacheResponse::OPAQUE_REDIRECT_TYPE:
      return network::mojom::FetchResponseType::kOpaqueRedirect;
  }
  NOTREACHED();
  return network::mojom::FetchResponseType::kOpaque;
}

proto::CacheResponse::ResponseType FetchResponseTypeToProtoResponseType(
    network::mojom::FetchResponseType response_type) {
  switch (response_type) {
    case network::mojom::FetchResponseType::kBasic:
      return proto::CacheResponse::BASIC_TYPE;
    case network::mojom::FetchResponseType::kCORS:
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
  NOTREACHED();
  return proto::CacheResponse::OPAQUE_TYPE;
}

// Copy headers out of a cache entry and into a protobuf. The callback is
// guaranteed to be run.
void ReadMetadata(disk_cache::Entry* entry, MetadataCallback callback);
void ReadMetadataDidReadMetadata(disk_cache::Entry* entry,
                                 MetadataCallback callback,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 int rv);

bool VaryMatches(const ServiceWorkerHeaderMap& request,
                 const ServiceWorkerHeaderMap& cached_request,
                 const ResponseHeaderMap& response) {
  auto vary_iter = std::find_if(
      response.begin(), response.end(),
      [](const ResponseHeaderMap::value_type& pair) -> bool {
        return base::CompareCaseInsensitiveASCII(pair.first, "vary") == 0;
      });
  if (vary_iter == response.end())
    return true;

  for (const std::string& trimmed :
       base::SplitString(vary_iter->second, ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
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

// Check a batch operation list for duplicate entries.  A StackVector
// must be passed to store any resulting duplicate URL strings.  Returns
// true if any duplicates were found.
bool FindDuplicateOperations(
    const std::vector<blink::mojom::BatchOperationPtr>& operations,
    std::vector<std::string>* duplicate_url_list_out) {
  using blink::mojom::BatchOperation;
  DCHECK(duplicate_url_list_out);

  if (operations.size() < 2) {
    return false;
  }

  // Create a temporary sorted vector of the operations to support quickly
  // finding potentially duplicate entries.  Multiple entries may have the
  // same URL, but differ by VARY header, so a sorted list is easier to
  // work with than a map.
  //
  // Note, this will use 512 bytes of stack space on 64-bit devices.  The
  // static size attempts to accommodate most typical Cache.addAll() uses in
  // service worker install events while not blowing up the stack too much.
  base::StackVector<BatchOperation*, 64> sorted;
  sorted->reserve(operations.size());
  for (const auto& op : operations) {
    sorted->push_back(op.get());
  }
  std::sort(sorted->begin(), sorted->end(),
            [](BatchOperation* left, BatchOperation* right) {
              return left->request.url < right->request.url;
            });

  // Check each entry in the sorted vector for any duplicates.  Since the
  // list is sorted we only need to inspect the immediate neighbors that
  // have the same URL.  This results in an average complexity of O(n log n).
  // If the entire list has entries with the same URL and different VARY
  // headers then this devolves into O(n^2).
  for (auto outer = sorted->cbegin(); outer != sorted->cend(); ++outer) {
    const BatchOperation* outer_op = *outer;

    // Note, the spec checks CacheQueryOptions like ignoreSearch, etc, but
    // currently there is no way for script to trigger a batch operation with
    // multiple entries and non-default options.  The only exposed API that
    // supports multiple operations is addAll() and it does not allow options
    // to be passed.  Therefore we assume we do not need to take any options
    // into account here.
    DCHECK(!outer_op->match_params);

    // If this entry already matches a duplicate we found, then just skip
    // ahead to find any remaining duplicates.
    if (!duplicate_url_list_out->empty() &&
        outer_op->request.url.spec() == duplicate_url_list_out->back()) {
      continue;
    }

    for (auto inner = std::next(outer); inner != sorted->cend(); ++inner) {
      const BatchOperation* inner_op = *inner;
      // Since the list is sorted we can stop looking at neighbors after
      // the first different URL.
      if (outer_op->request.url != inner_op->request.url) {
        break;
      }
      // VaryMatches() is asymmetric since the operation depends on the VARY
      // header in the target response.  Since we only visit each pair of
      // entries once we need to perform the VaryMatches() call in both
      // directions.
      if (VaryMatches(outer_op->request.headers, inner_op->request.headers,
                      inner_op->response->headers) ||
          VaryMatches(outer_op->request.headers, inner_op->request.headers,
                      outer_op->response->headers)) {
        duplicate_url_list_out->push_back(inner_op->request.url.spec());
        break;
      }
    }
  }

  return !duplicate_url_list_out->empty();
}

GURL RemoveQueryParam(const GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

void ReadMetadata(disk_cache::Entry* entry, MetadataCallback callback) {
  DCHECK(entry);

  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(
          entry->GetDataSize(CacheStorageCache::INDEX_HEADERS));

  net::CompletionCallback read_header_callback =
      base::AdaptCallbackForRepeating(base::BindOnce(
          ReadMetadataDidReadMetadata, entry, std::move(callback), buffer));

  int read_rv =
      entry->ReadData(CacheStorageCache::INDEX_HEADERS, 0, buffer.get(),
                      buffer->size(), read_header_callback);

  if (read_rv != net::ERR_IO_PENDING)
    std::move(read_header_callback).Run(read_rv);
}

void ReadMetadataDidReadMetadata(disk_cache::Entry* entry,
                                 MetadataCallback callback,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 int rv) {
  if (rv != buffer->size()) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (rv > 0)
    storage::RecordBytesRead(kRecordBytesLabel, rv);

  std::unique_ptr<proto::CacheMetadata> metadata(new proto::CacheMetadata());

  if (!metadata->ParseFromArray(buffer->data(), buffer->size())) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(metadata));
}

std::unique_ptr<ServiceWorkerFetchRequest> CreateRequest(
    const proto::CacheMetadata& metadata,
    const GURL& request_url) {
  auto request = std::make_unique<ServiceWorkerFetchRequest>(
      request_url, metadata.request().method(), ServiceWorkerHeaderMap(),
      Referrer(), false);

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
  std::vector<GURL> url_list;
  // From Chrome 57, proto::CacheMetadata's url field was deprecated.
  UMA_HISTOGRAM_BOOLEAN("ServiceWorkerCache.Response.HasDeprecatedURL",
                        metadata.response().has_url());
  if (metadata.response().has_url()) {
    url_list.push_back(GURL(metadata.response().url()));
  } else {
    url_list.reserve(metadata.response().url_list_size());
    for (int i = 0; i < metadata.response().url_list_size(); ++i)
      url_list.push_back(GURL(metadata.response().url_list(i)));
  }

  ResponseHeaderMap headers;
  for (int i = 0; i < metadata.response().headers_size(); ++i) {
    const proto::CacheHeaderMap header = metadata.response().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    headers.insert(std::make_pair(header.name(), header.value()));
  }

  return blink::mojom::FetchAPIResponse::New(
      url_list, metadata.response().status_code(),
      metadata.response().status_text(),
      ProtoResponseTypeToFetchResponseType(metadata.response().response_type()),
      headers, nullptr /* blob */,
      blink::mojom::ServiceWorkerResponseError::kUnknown,
      base::Time::FromInternalValue(metadata.response().response_time()),
      cache_name,
      std::vector<std::string>(
          metadata.response().cors_exposed_header_names().begin(),
          metadata.response().cors_exposed_header_names().end()),
      true /* is_in_cache_storage */, nullptr /* side_data_blob */);
}

// The size of opaque (non-cors) resource responses are padded in order
// to obfuscate their actual size.
bool ShouldPadResponseType(network::mojom::FetchResponseType response_type,
                           bool has_urls) {
  switch (response_type) {
    case network::mojom::FetchResponseType::kBasic:
    case network::mojom::FetchResponseType::kCORS:
    case network::mojom::FetchResponseType::kDefault:
    case network::mojom::FetchResponseType::kError:
      return false;
    case network::mojom::FetchResponseType::kOpaque:
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      return has_urls;
  }
  NOTREACHED();
  return false;
}

bool ShouldPadResourceSize(const content::proto::CacheResponse* response) {
  return ShouldPadResponseType(
      ProtoResponseTypeToFetchResponseType(response->response_type()),
      response->url_list_size());
}

bool ShouldPadResourceSize(const blink::mojom::FetchAPIResponse& response) {
  return ShouldPadResponseType(response.response_type,
                               !response.url_list.empty());
}

int64_t CalculateResponsePaddingInternal(
    const std::string& response_url,
    const crypto::SymmetricKey* padding_key,
    int side_data_size) {
  DCHECK(!response_url.empty());

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(padding_key))
    LOG(FATAL) << "Failed to init HMAC.";

  std::vector<uint8_t> digest(hmac.DigestLength());
  bool success;
  if (side_data_size)
    success = hmac.Sign(response_url + "METADATA", &digest[0], digest.size());
  else
    success = hmac.Sign(response_url, &digest[0], digest.size());
  if (!success)
    LOG(FATAL) << "Failed to sign URL.";

  DCHECK_GE(digest.size(), sizeof(uint64_t));
  uint64_t val = *(reinterpret_cast<uint64_t*>(&digest[0]));
  return val % kPaddingRange;
}

int64_t CalculateResponsePaddingInternal(
    const ::content::proto::CacheResponse* response,
    const crypto::SymmetricKey* padding_key,
    int side_data_size) {
  DCHECK(ShouldPadResourceSize(response));
  const std::string& url = response->url_list(response->url_list_size() - 1);
  return CalculateResponsePaddingInternal(url, padding_key, side_data_size);
}

}  // namespace

// This class ensures that the cache and the entry have a lifetime as long as
// the blob that is created to contain them.
class CacheStorageCache::BlobDataHandle
    : public storage::BlobDataBuilder::DataHandle {
 public:
  BlobDataHandle(CacheStorageCacheHandle cache_handle,
                 disk_cache::ScopedEntryPtr entry)
      : cache_handle_(std::move(cache_handle)), entry_(std::move(entry)) {}

  bool IsValid() override { return bool{entry_}; }

  void Invalidate() {
    cache_handle_ = base::nullopt;
    entry_ = nullptr;
  }

 private:
  ~BlobDataHandle() override {
    if (cache_handle_ && cache_handle_->value()) {
      cache_handle_->value()->blob_data_handles_.erase(this);
    }
  }

  base::Optional<CacheStorageCacheHandle> cache_handle_;
  disk_cache::ScopedEntryPtr entry_;

  DISALLOW_COPY_AND_ASSIGN(BlobDataHandle);
};

// The state needed to pass between CacheStorageCache::Put callbacks.
struct CacheStorageCache::PutContext {
  PutContext(std::unique_ptr<ServiceWorkerFetchRequest> request,
             blink::mojom::FetchAPIResponsePtr response,
             blink::mojom::BlobPtr blob,
             uint64_t blob_size,
             blink::mojom::BlobPtr side_data_blob,
             uint64_t side_data_blob_size,
             CacheStorageCache::ErrorCallback callback)
      : request(std::move(request)),
        response(std::move(response)),
        blob(std::move(blob)),
        blob_size(blob_size),
        side_data_blob(std::move(side_data_blob)),
        side_data_blob_size(side_data_blob_size),
        callback(std::move(callback)) {}

  // Input parameters to the Put function.
  std::unique_ptr<ServiceWorkerFetchRequest> request;
  blink::mojom::FetchAPIResponsePtr response;
  blink::mojom::BlobPtr blob;
  uint64_t blob_size;
  blink::mojom::BlobPtr side_data_blob;
  uint64_t side_data_blob_size;

  CacheStorageCache::ErrorCallback callback;
  disk_cache::ScopedEntryPtr cache_entry;

 private:
  DISALLOW_COPY_AND_ASSIGN(PutContext);
};

struct CacheStorageCache::QueryCacheResult {
  explicit QueryCacheResult(base::Time entry_time) : entry_time(entry_time) {}

  std::unique_ptr<ServiceWorkerFetchRequest> request;
  blink::mojom::FetchAPIResponsePtr response;
  disk_cache::ScopedEntryPtr entry;
  base::Time entry_time;
};

struct CacheStorageCache::QueryCacheContext {
  QueryCacheContext(std::unique_ptr<ServiceWorkerFetchRequest> request,
                    blink::mojom::QueryParamsPtr options,
                    QueryCacheCallback callback,
                    QueryTypes query_types)
      : request(std::move(request)),
        options(std::move(options)),
        callback(std::move(callback)),
        query_types(query_types),
        matches(std::make_unique<QueryCacheResults>()) {}

  ~QueryCacheContext() {
    // If the CacheStorageCache is deleted before a backend operation to open
    // an entry completes, the callback won't be run and the resulting entry
    // will be leaked unless we close it here.
    if (enumerated_entry) {
      enumerated_entry->Close();
      enumerated_entry = nullptr;
    }
  }

  // Input to QueryCache
  std::unique_ptr<ServiceWorkerFetchRequest> request;
  blink::mojom::QueryParamsPtr options;
  QueryCacheCallback callback;
  QueryTypes query_types = 0;
  size_t estimated_out_bytes = 0;

  // Iteration state
  std::unique_ptr<disk_cache::Backend::Iterator> backend_iterator;
  disk_cache::Entry* enumerated_entry = nullptr;

  // Output of QueryCache
  std::unique_ptr<std::vector<QueryCacheResult>> matches;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryCacheContext);
};

// static
std::unique_ptr<CacheStorageCache> CacheStorageCache::CreateMemoryCache(
    const url::Origin& origin,
    CacheStorageOwner owner,
    const std::string& cache_name,
    CacheStorage* cache_storage,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context,
    std::unique_ptr<crypto::SymmetricKey> cache_padding_key) {
  CacheStorageCache* cache = new CacheStorageCache(
      origin, owner, cache_name, base::FilePath(), cache_storage,
      std::move(request_context_getter), std::move(quota_manager_proxy),
      blob_context, 0 /* cache_size */, 0 /* cache_padding */,
      std::move(cache_padding_key));
  cache->SetObserver(cache_storage);
  cache->InitBackend();
  return base::WrapUnique(cache);
}

// static
std::unique_ptr<CacheStorageCache> CacheStorageCache::CreatePersistentCache(
    const url::Origin& origin,
    CacheStorageOwner owner,
    const std::string& cache_name,
    CacheStorage* cache_storage,
    const base::FilePath& path,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context,
    int64_t cache_size,
    int64_t cache_padding,
    std::unique_ptr<crypto::SymmetricKey> cache_padding_key) {
  CacheStorageCache* cache = new CacheStorageCache(
      origin, owner, cache_name, path, cache_storage,
      std::move(request_context_getter), std::move(quota_manager_proxy),
      blob_context, cache_size, cache_padding, std::move(cache_padding_key));
  cache->SetObserver(cache_storage);
  cache->InitBackend();
  return base::WrapUnique(cache);
}

base::WeakPtr<CacheStorageCache> CacheStorageCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CacheStorageCache::Match(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr match_params,
    ResponseCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kMatchBackendClosed), nullptr);
    return;
  }

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::MatchImpl, weak_ptr_factory_.GetWeakPtr(),
      std::move(request), std::move(match_params),
      scheduler_->WrapCallbackToRunNext(std::move(callback))));
}

void CacheStorageCache::MatchAll(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr match_params,
    ResponsesCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kMatchAllBackendClosed),
        std::vector<blink::mojom::FetchAPIResponsePtr>());
    return;
  }

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::MatchAllImpl, weak_ptr_factory_.GetWeakPtr(),
      std::move(request), std::move(match_params),
      scheduler_->WrapCallbackToRunNext(std::move(callback))));
}

void CacheStorageCache::WriteSideData(ErrorCallback callback,
                                      const GURL& url,
                                      base::Time expected_response_time,
                                      scoped_refptr<net::IOBuffer> buffer,
                                      int buf_len) {
  if (backend_state_ == BACKEND_CLOSED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            MakeErrorStorage(ErrorStorageType::kWriteSideDataBackendClosed)));
    return;
  }

  // GetUsageAndQuota is called before entering a scheduled operation since it
  // can call Size, another scheduled operation.
  quota_manager_proxy_->GetUsageAndQuota(
      base::ThreadTaskRunnerHandle::Get().get(), origin_,
      blink::mojom::StorageType::kTemporary,
      base::BindOnce(&CacheStorageCache::WriteSideDataDidGetQuota,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), url,
                     expected_response_time, buffer, buf_len));
}

void CacheStorageCache::BatchOperation(
    std::vector<blink::mojom::BatchOperationPtr> operations,
    bool fail_on_duplicates,
    VerboseErrorCallback callback,
    BadMessageCallback bad_message_callback) {
  // This method may produce a warning message that should be returned in the
  // final VerboseErrorCallback.  A message may be present in both the failure
  // and success paths.
  base::Optional<std::string> message;

  if (backend_state_ == BACKEND_CLOSED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  //
  // Note, we are currently only rejecting on duplicates based on a feature
  // flag while web compat is assessed.  (https://crbug.com/720919)
  std::vector<std::string> duplicate_url_list;
  if (FindDuplicateOperations(operations, &duplicate_url_list)) {
    // If we found any duplicates we need to at least warn the user.  Format
    // the URL list into a comma-separated list.
    std::string url_list_string = base::JoinString(duplicate_url_list, ", ");

    // Place the duplicate list into an error message.
    // NOTE: This must use kDuplicateOperationsBaseMessage in the string so
    // that the renderer can identify successes with duplicates and log the
    // appropriate use counter.
    // TODO(crbug.com/877737): Remove this note once the cache.addAll()
    // duplicate rejection finally ships.
    message.emplace(base::StringPrintf(
        "%s (%s)", blink::cache_storage::kDuplicateOperationBaseMessage,
        url_list_string.c_str()));

    // Depending on the feature flag, we may treat this as an error or allow
    // the batch operation to continue.
    if (fail_on_duplicates) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         CacheStorageVerboseError::New(
                             CacheStorageError::kErrorDuplicateOperation,
                             std::move(message))));
      return;
    }
  }

  // Estimate the required size of the put operations. The size of the deletes
  // is unknown and not considered.
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  base::CheckedNumeric<uint64_t> safe_side_data_size = 0;
  for (const auto& operation : operations) {
    if (operation->operation_type == blink::mojom::OperationType::kPut) {
      safe_space_required +=
          (operation->response->blob ? operation->response->blob->size : 0);
      safe_side_data_size += (operation->response->side_data_blob
                                  ? operation->response->side_data_blob->size
                                  : 0);
    }
  }
  if (!safe_space_required.IsValid() || !safe_side_data_size.IsValid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(bad_message_callback));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
    // GetUsageAndQuota is called before entering a scheduled operation since it
    // can call Size, another scheduled operation. This is racy. The decision
    // to commit is made before the scheduled Put operation runs. By the time
    // Put runs, the cache might already be full and the origin will be larger
    // than it's supposed to be.
    quota_manager_proxy_->GetUsageAndQuota(
        base::ThreadTaskRunnerHandle::Get().get(), origin_,
        blink::mojom::StorageType::kTemporary,
        base::BindOnce(&CacheStorageCache::BatchDidGetUsageAndQuota,
                       weak_ptr_factory_.GetWeakPtr(), std::move(operations),
                       std::move(callback), std::move(bad_message_callback),
                       std::move(message), space_required, side_data_size));
    return;
  }

  BatchDidGetUsageAndQuota(std::move(operations), std::move(callback),
                           std::move(bad_message_callback), std::move(message),
                           0 /* space_required */, 0 /* side_data_size */,
                           blink::mojom::QuotaStatusCode::kOk, 0 /* usage */,
                           0 /* quota */);
}

void CacheStorageCache::BatchDidGetUsageAndQuota(
    std::vector<blink::mojom::BatchOperationPtr> operations,
    VerboseErrorCallback callback,
    BadMessageCallback bad_message_callback,
    base::Optional<std::string> message,
    uint64_t space_required,
    uint64_t side_data_size,
    blink::mojom::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  base::CheckedNumeric<uint64_t> safe_space_required = space_required;
  base::CheckedNumeric<uint64_t> safe_space_required_with_side_data;
  safe_space_required += usage;
  safe_space_required_with_side_data = safe_space_required + side_data_size;
  if (!safe_space_required.IsValid() ||
      !safe_space_required_with_side_data.IsValid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(bad_message_callback));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            CacheStorageVerboseError::New(
                MakeErrorStorage(
                    ErrorStorageType::kBatchDidGetUsageAndQuotaInvalidSpace),
                std::move(message))));
    return;
  }
  if (status_code != blink::mojom::QuotaStatusCode::kOk ||
      safe_space_required.ValueOrDie() > quota) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  CacheStorageVerboseError::New(
                                      CacheStorageError::kErrorQuotaExceeded,
                                      std::move(message))));
    return;
  }
  bool skip_side_data = safe_space_required_with_side_data.ValueOrDie() > quota;

  // The following relies on the guarantee that the RepeatingCallback returned
  // from AdaptCallbackForRepeating invokes the original callback on the first
  // invocation, and (critically) that subsequent invocations are ignored.
  // TODO(jsbell): Replace AdaptCallbackForRepeating with ...? crbug.com/730593
  auto callback_copy = base::AdaptCallbackForRepeating(std::move(callback));
  auto barrier_closure = base::BarrierClosure(
      operations.size(),
      base::BindOnce(&CacheStorageCache::BatchDidAllOperations,
                     weak_ptr_factory_.GetWeakPtr(), callback_copy, message));
  auto completion_callback = base::BindRepeating(
      &CacheStorageCache::BatchDidOneOperation, weak_ptr_factory_.GetWeakPtr(),
      std::move(barrier_closure), std::move(callback_copy), std::move(message));

  // Operations may synchronously invoke |callback| which could release the
  // last reference to this instance. Hold a handle for the duration of this
  // loop. (Asynchronous tasks scheduled by the operations use weak ptrs which
  // will no-op automatically.)
  CacheStorageCacheHandle handle = CreateCacheHandle();

  for (auto& operation : operations) {
    switch (operation->operation_type) {
      case blink::mojom::OperationType::kPut:
        if (skip_side_data) {
          operation->response->side_data_blob = nullptr;
          Put(std::move(operation), completion_callback);
        } else {
          Put(std::move(operation), completion_callback);
        }
        break;
      case blink::mojom::OperationType::kDelete:
        DCHECK_EQ(1u, operations.size());
        Delete(std::move(operation), completion_callback);
        break;
      case blink::mojom::OperationType::kUndefined:
        NOTREACHED();
        // TODO(nhiroki): This should return "TypeError".
        // http://crbug.com/425505
        completion_callback.Run(MakeErrorStorage(
            ErrorStorageType::kBatchDidGetUsageAndQuotaUndefinedOp));
        break;
    }
  }
}

void CacheStorageCache::BatchDidOneOperation(
    base::OnceClosure completion_closure,
    VerboseErrorCallback error_callback,
    base::Optional<std::string> message,
    CacheStorageError error) {
  if (error != CacheStorageError::kSuccess) {
    // This relies on |callback| being created by AdaptCallbackForRepeating
    // and ignoring anything but the first invocation.
    std::move(error_callback)
        .Run(CacheStorageVerboseError::New(error, std::move(message)));
  }

  std::move(completion_closure).Run();
}

void CacheStorageCache::BatchDidAllOperations(
    VerboseErrorCallback callback,
    base::Optional<std::string> message) {
  // This relies on |callback| being created by AdaptCallbackForRepeating
  // and ignoring anything but the first invocation.
  std::move(callback).Run(CacheStorageVerboseError::New(
      CacheStorageError::kSuccess, std::move(message)));
}

void CacheStorageCache::Keys(std::unique_ptr<ServiceWorkerFetchRequest> request,
                             blink::mojom::QueryParamsPtr options,
                             RequestsCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysBackendClosed), nullptr);
    return;
  }

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::KeysImpl, weak_ptr_factory_.GetWeakPtr(),
      std::move(request), std::move(options),
      scheduler_->WrapCallbackToRunNext(std::move(callback))));
}

void CacheStorageCache::Close(base::OnceClosure callback) {
  DCHECK_NE(BACKEND_CLOSED, backend_state_)
      << "Was CacheStorageCache::Close() called twice?";

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::CloseImpl, weak_ptr_factory_.GetWeakPtr(),
      scheduler_->WrapCallbackToRunNext(std::move(callback))));
}

void CacheStorageCache::Size(SizeCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    // TODO(jkarlin): Delete caches that can't be initialized.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0));
    return;
  }

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::SizeImpl, weak_ptr_factory_.GetWeakPtr(),
      scheduler_->WrapCallbackToRunNext(std::move(callback))));
}

void CacheStorageCache::GetSizeThenClose(SizeCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0));
    return;
  }

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::SizeImpl, weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(&CacheStorageCache::GetSizeThenCloseDidGetSize,
                     weak_ptr_factory_.GetWeakPtr(),
                     scheduler_->WrapCallbackToRunNext(std::move(callback)))));
}

void CacheStorageCache::SetObserver(CacheStorageCacheObserver* observer) {
  DCHECK((observer == nullptr) ^ (cache_observer_ == nullptr));
  cache_observer_ = observer;
}

CacheStorageCache::~CacheStorageCache() {
  quota_manager_proxy_->NotifyOriginNoLongerInUse(origin_);
}

CacheStorageCache::CacheStorageCache(
    const url::Origin& origin,
    CacheStorageOwner owner,
    const std::string& cache_name,
    const base::FilePath& path,
    CacheStorage* cache_storage,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context,
    int64_t cache_size,
    int64_t cache_padding,
    std::unique_ptr<crypto::SymmetricKey> cache_padding_key)
    : origin_(origin),
      owner_(owner),
      cache_name_(cache_name),
      path_(path),
      cache_storage_(cache_storage),
      request_context_getter_(std::move(request_context_getter)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      blob_storage_context_(blob_context),
      scheduler_(
          new CacheStorageScheduler(CacheStorageSchedulerClient::CLIENT_CACHE)),
      cache_size_(cache_size),
      cache_padding_(cache_padding),
      cache_padding_key_(std::move(cache_padding_key)),
      max_query_size_bytes_(kMaxQueryCacheResultBytes),
      cache_observer_(nullptr),
      memory_only_(path.empty()),
      weak_ptr_factory_(this) {
  DCHECK(!origin_.opaque());
  DCHECK(quota_manager_proxy_.get());
  DCHECK(cache_padding_key_.get());

  if (cache_size_ != CacheStorage::kSizeUnknown &&
      cache_padding_ != CacheStorage::kSizeUnknown) {
    // The size of this cache has already been reported to the QuotaManager.
    last_reported_size_ = cache_size_ + cache_padding_;
  }

  quota_manager_proxy_->NotifyOriginInUse(origin_);
}

void CacheStorageCache::QueryCache(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr options,
    QueryTypes query_types,
    QueryCacheCallback callback) {
  DCHECK_NE(
      QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_WITH_BODIES,
      query_types & (QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_WITH_BODIES));
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kQueryCacheBackendClosed), nullptr);
    return;
  }

  if (owner_ != CacheStorageOwner::kBackgroundFetch &&
      (!options || !options->ignore_method) && request &&
      !request->method.empty() && request->method != "GET") {
    std::move(callback).Run(CacheStorageError::kSuccess,
                            std::make_unique<QueryCacheResults>());
    return;
  }

  ServiceWorkerFetchRequest* request_ptr = request.get();
  std::unique_ptr<QueryCacheContext> query_cache_context(
      new QueryCacheContext(std::move(request), std::move(options),
                            std::move(callback), query_types));

  if (query_cache_context->request &&
      !query_cache_context->request->url.is_empty() &&
      (!query_cache_context->options ||
       !query_cache_context->options->ignore_search)) {
    // There is no need to scan the entire backend, just open the exact
    // URL.
    disk_cache::Entry** entry_ptr = &query_cache_context->enumerated_entry;
    net::CompletionCallback open_entry_callback =
        base::AdaptCallbackForRepeating(base::BindOnce(
            &CacheStorageCache::QueryCacheDidOpenFastPath,
            weak_ptr_factory_.GetWeakPtr(), std::move(query_cache_context)));
    int rv = backend_->OpenEntry(request_ptr->url.spec(), net::HIGHEST,
                                 entry_ptr, open_entry_callback);
    if (rv != net::ERR_IO_PENDING)
      std::move(open_entry_callback).Run(rv);
    return;
  }

  query_cache_context->backend_iterator = backend_->CreateIterator();
  QueryCacheOpenNextEntry(std::move(query_cache_context));
}

void CacheStorageCache::QueryCacheDidOpenFastPath(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    int rv) {
  if (rv != net::OK) {
    QueryCacheContext* results = query_cache_context.get();
    std::move(results->callback)
        .Run(CacheStorageError::kSuccess,
             std::move(query_cache_context->matches));
    return;
  }
  QueryCacheFilterEntry(std::move(query_cache_context), rv);
}

void CacheStorageCache::QueryCacheOpenNextEntry(
    std::unique_ptr<QueryCacheContext> query_cache_context) {
  DCHECK_EQ(nullptr, query_cache_context->enumerated_entry);

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
  disk_cache::Entry** enumerated_entry = &query_cache_context->enumerated_entry;
  net::CompletionCallback open_entry_callback =
      base::AdaptCallbackForRepeating(base::BindOnce(
          &CacheStorageCache::QueryCacheFilterEntry,
          weak_ptr_factory_.GetWeakPtr(), std::move(query_cache_context)));

  int rv = iterator.OpenNextEntry(enumerated_entry, open_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    std::move(open_entry_callback).Run(rv);
}

void CacheStorageCache::QueryCacheFilterEntry(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    int rv) {
  if (rv == net::ERR_FAILED) {
    // This is the indicator that iteration is complete.
    query_cache_context->backend_iterator.reset();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  if (rv < 0) {
    std::move(query_cache_context->callback)
        .Run(MakeErrorStorage(ErrorStorageType::kQueryCacheFilterEntryFailed),
             std::move(query_cache_context->matches));
    return;
  }

  disk_cache::ScopedEntryPtr entry(query_cache_context->enumerated_entry);
  query_cache_context->enumerated_entry = nullptr;

  if (backend_state_ == BACKEND_CLOSED) {
    std::move(query_cache_context->callback)
        .Run(CacheStorageError::kErrorNotFound,
             std::move(query_cache_context->matches));
    return;
  }

  if (query_cache_context->request &&
      !query_cache_context->request->url.is_empty()) {
    GURL requestURL = query_cache_context->request->url;
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

  // If the entry was created before we started adding entry times, then
  // default to using the Response object's time for sorting purposes.
  int64_t entry_time = metadata->has_entry_time()
                           ? metadata->entry_time()
                           : metadata->response().response_time();

  query_cache_context->matches->push_back(
      QueryCacheResult(base::Time::FromInternalValue(entry_time)));
  QueryCacheResult* match = &query_cache_context->matches->back();
  match->request = CreateRequest(*metadata, GURL(entry->GetKey()));
  match->response = CreateResponse(*metadata, cache_name_);

  if (query_cache_context->request &&
      (!query_cache_context->options ||
       !query_cache_context->options->ignore_vary) &&
      !VaryMatches(query_cache_context->request->headers,
                   match->request->headers, match->response->headers)) {
    query_cache_context->matches->pop_back();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  if (query_cache_context->query_types & QUERY_CACHE_ENTRIES)
    match->entry = std::move(entry);

  if (query_cache_context->query_types & QUERY_CACHE_REQUESTS) {
    query_cache_context->estimated_out_bytes +=
        match->request->EstimatedStructSize();
    if (query_cache_context->estimated_out_bytes > max_query_size_bytes_) {
      std::move(query_cache_context->callback)
          .Run(CacheStorageError::kErrorQueryTooLarge, nullptr);
      return;
    }
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
    if (entry->GetDataSize(INDEX_RESPONSE_BODY) == 0) {
      QueryCacheOpenNextEntry(std::move(query_cache_context));
      return;
    }

    if (!blob_storage_context_) {
      std::move(query_cache_context->callback)
          .Run(MakeErrorStorage(
                   ErrorStorageType::kQueryCacheDidReadMetadataNullBlobContext),
               nullptr);
      return;
    }

    PopulateResponseBody(std::move(entry), match->response.get());
  } else if (!(query_cache_context->query_types &
               QUERY_CACHE_RESPONSES_NO_BODIES)) {
    match->response.reset();
  }

  QueryCacheOpenNextEntry(std::move(query_cache_context));
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
int64_t CacheStorageCache::CalculateResponsePadding(
    const blink::mojom::FetchAPIResponse& response,
    const crypto::SymmetricKey* padding_key,
    int side_data_size) {
  if (!ShouldPadResourceSize(response))
    return 0;
  return CalculateResponsePaddingInternal(response.url_list.back().spec(),
                                          padding_key, side_data_size);
}

// static
int32_t CacheStorageCache::GetResponsePaddingVersion() {
  return kCachePaddingAlgorithmVersion;
}

void CacheStorageCache::MatchImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr match_params,
    ResponseCallback callback) {
  MatchAllImpl(
      std::move(request), std::move(match_params),
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

void CacheStorageCache::MatchAllImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr options,
    ResponsesCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kStorageMatchAllBackendClosed),
        std::vector<blink::mojom::FetchAPIResponsePtr>());
    return;
  }

  QueryCache(
      std::move(request), std::move(options),
      QUERY_CACHE_REQUESTS | QUERY_CACHE_RESPONSES_WITH_BODIES,
      base::BindOnce(&CacheStorageCache::MatchAllDidQueryCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CacheStorageCache::MatchAllDidQueryCache(
    ResponsesCallback callback,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
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

void CacheStorageCache::WriteSideDataDidGetQuota(
    ErrorCallback callback,
    const GURL& url,
    base::Time expected_response_time,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    blink::mojom::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  if (status_code != blink::mojom::QuotaStatusCode::kOk ||
      (buf_len > quota - usage)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  CacheStorageError::kErrorQuotaExceeded));
    return;
  }

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::WriteSideDataImpl, weak_ptr_factory_.GetWeakPtr(),
      scheduler_->WrapCallbackToRunNext(std::move(callback)), url,
      expected_response_time, buffer, buf_len));
}

void CacheStorageCache::WriteSideDataImpl(ErrorCallback callback,
                                          const GURL& url,
                                          base::Time expected_response_time,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          int buf_len) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kWriteSideDataImplBackendClosed));
    return;
  }

  std::unique_ptr<disk_cache::Entry*> scoped_entry_ptr(
      new disk_cache::Entry*());
  disk_cache::Entry** entry_ptr = scoped_entry_ptr.get();
  net::CompletionCallback open_entry_callback = base::AdaptCallbackForRepeating(
      base::BindOnce(&CacheStorageCache::WriteSideDataDidOpenEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     expected_response_time, buffer, buf_len,
                     std::move(scoped_entry_ptr)));


  // Use LOWEST priority here as writing side data is less important than
  // loading resources on the page.
  int rv = backend_->OpenEntry(url.spec(), net::LOWEST, entry_ptr,
                               open_entry_callback);
  if (rv != net::ERR_IO_PENDING)
    std::move(open_entry_callback).Run(rv);
}

void CacheStorageCache::WriteSideDataDidOpenEntry(
    ErrorCallback callback,
    base::Time expected_response_time,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    std::unique_ptr<disk_cache::Entry*> entry_ptr,
    int rv) {
  if (rv != net::OK) {
    std::move(callback).Run(CacheStorageError::kErrorNotFound);
    return;
  }
  disk_cache::ScopedEntryPtr entry(*entry_ptr);

  ReadMetadata(*entry_ptr,
               base::BindOnce(&CacheStorageCache::WriteSideDataDidReadMetaData,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), expected_response_time,
                              buffer, buf_len, std::move(entry)));
}

void CacheStorageCache::WriteSideDataDidReadMetaData(
    ErrorCallback callback,
    base::Time expected_response_time,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    disk_cache::ScopedEntryPtr entry,
    std::unique_ptr<proto::CacheMetadata> headers) {
  if (!headers ||
      headers->response().response_time() !=
          expected_response_time.ToInternalValue()) {
    std::move(callback).Run(CacheStorageError::kErrorNotFound);
    return;
  }
  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = entry.get();

  std::unique_ptr<content::proto::CacheResponse> response(
      headers->release_response());

  int side_data_size_before_write = 0;
  if (ShouldPadResourceSize(response.get()))
    side_data_size_before_write = entry->GetDataSize(INDEX_SIDE_DATA);

  net::CompletionCallback write_side_data_callback =
      base::AdaptCallbackForRepeating(base::BindOnce(
          &CacheStorageCache::WriteSideDataDidWrite,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(entry),
          buf_len, std::move(response), side_data_size_before_write));

  int rv = temp_entry_ptr->WriteData(
      INDEX_SIDE_DATA, 0 /* offset */, buffer.get(), buf_len,
      write_side_data_callback, true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    std::move(write_side_data_callback).Run(rv);
}

void CacheStorageCache::WriteSideDataDidWrite(
    ErrorCallback callback,
    disk_cache::ScopedEntryPtr entry,
    int expected_bytes,
    std::unique_ptr<::content::proto::CacheResponse> response,
    int side_data_size_before_write,
    int rv) {
  if (rv != expected_bytes) {
    entry->Doom();
    UpdateCacheSize(
        base::BindOnce(std::move(callback), CacheStorageError::kErrorNotFound));
    return;
  }

  if (rv > 0)
    storage::RecordBytesWritten(kRecordBytesLabel, rv);

  if (ShouldPadResourceSize(response.get())) {
    cache_padding_ -= CalculateResponsePaddingInternal(
        response.get(), cache_padding_key_.get(), side_data_size_before_write);

    cache_padding_ += CalculateResponsePaddingInternal(
        response.get(), cache_padding_key_.get(), rv);
  }

  UpdateCacheSize(
      base::BindOnce(std::move(callback), CacheStorageError::kSuccess));
}

void CacheStorageCache::Put(blink::mojom::BatchOperationPtr operation,
                            ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(blink::mojom::OperationType::kPut, operation->operation_type);

  std::unique_ptr<ServiceWorkerFetchRequest> request(
      new ServiceWorkerFetchRequest(
          operation->request.url, operation->request.method,
          operation->request.headers, operation->request.referrer,
          operation->request.is_reload));

  Put(std::move(request), std::move(operation->response), std::move(callback));
}

void CacheStorageCache::Put(std::unique_ptr<ServiceWorkerFetchRequest> request,
                            blink::mojom::FetchAPIResponsePtr response,
                            ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);

  blink::mojom::BlobPtr blob;
  uint64_t blob_size = blink::BlobUtils::kUnknownSize;
  blink::mojom::BlobPtr side_data_blob;
  uint64_t side_data_blob_size = blink::BlobUtils::kUnknownSize;

  if (response->blob) {
    blob.Bind(std::move(response->blob->blob));
    blob_size = response->blob->size;
  }
  if (response->side_data_blob) {
    side_data_blob.Bind(std::move(response->side_data_blob->blob));
    side_data_blob_size = response->side_data_blob->size;
  }

  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.Cache.AllWritesResponseType",
                            response->response_type);

  auto put_context = std::make_unique<PutContext>(
      std::move(request), std::move(response), std::move(blob), blob_size,
      std::move(side_data_blob), side_data_blob_size,
      scheduler_->WrapCallbackToRunNext(std::move(callback)));

  scheduler_->ScheduleOperation(base::BindOnce(&CacheStorageCache::PutImpl,
                                               weak_ptr_factory_.GetWeakPtr(),
                                               std::move(put_context)));
}

void CacheStorageCache::PutImpl(std::unique_ptr<PutContext> put_context) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    std::move(put_context->callback)
        .Run(MakeErrorStorage(ErrorStorageType::kPutImplBackendClosed));
    return;
  }

  // Explicitly delete the incumbent resource (which may not exist). This is
  // only done so that it's padding will be decremented from the calculated
  // cache padding.
  // TODO(cmumford): Research alternatives to this explicit delete as it
  // seriously impacts put performance.
  auto delete_request = std::make_unique<ServiceWorkerFetchRequest>(
      put_context->request->url, "", ServiceWorkerHeaderMap(), Referrer(),
      false);

  blink::mojom::QueryParamsPtr query_options = blink::mojom::QueryParams::New();
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
  if (backend_state_ != BACKEND_OPEN) {
    std::move(put_context->callback)
        .Run(MakeErrorStorage(
            ErrorStorageType::kPutDidDeleteEntryBackendClosed));
    return;
  }

  if (error != CacheStorageError::kSuccess &&
      error != CacheStorageError::kErrorNotFound) {
    std::move(put_context->callback).Run(error);
    return;
  }

  std::unique_ptr<disk_cache::Entry*> scoped_entry_ptr(
      new disk_cache::Entry*());
  disk_cache::Entry** entry_ptr = scoped_entry_ptr.get();
  ServiceWorkerFetchRequest* request_ptr = put_context->request.get();
  disk_cache::Backend* backend_ptr = backend_.get();

  net::CompletionCallback create_entry_callback =
      base::AdaptCallbackForRepeating(base::BindOnce(
          &CacheStorageCache::PutDidCreateEntry, weak_ptr_factory_.GetWeakPtr(),
          std::move(scoped_entry_ptr), std::move(put_context)));

  int rv = backend_ptr->CreateEntry(request_ptr->url.spec(), net::HIGHEST,
                                    entry_ptr, create_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    std::move(create_entry_callback).Run(rv);
}

void CacheStorageCache::PutDidCreateEntry(
    std::unique_ptr<disk_cache::Entry*> entry_ptr,
    std::unique_ptr<PutContext> put_context,
    int rv) {
  put_context->cache_entry.reset(*entry_ptr);

  if (rv != net::OK) {
    std::move(put_context->callback).Run(CacheStorageError::kErrorExists);
    return;
  }

  proto::CacheMetadata metadata;
  metadata.set_entry_time(base::Time::Now().ToInternalValue());
  proto::CacheRequest* request_metadata = metadata.mutable_request();
  request_metadata->set_method(put_context->request->method);
  for (ServiceWorkerHeaderMap::const_iterator it =
           put_context->request->headers.begin();
       it != put_context->request->headers.end(); ++it) {
    DCHECK_EQ(std::string::npos, it->first.find('\0'));
    DCHECK_EQ(std::string::npos, it->second.find('\0'));
    proto::CacheHeaderMap* header_map = request_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }

  proto::CacheResponse* response_metadata = metadata.mutable_response();
  response_metadata->set_status_code(put_context->response->status_code);
  response_metadata->set_status_text(put_context->response->status_text);
  response_metadata->set_response_type(FetchResponseTypeToProtoResponseType(
      put_context->response->response_type));
  for (const auto& url : put_context->response->url_list)
    response_metadata->add_url_list(url.spec());
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

  std::unique_ptr<std::string> serialized(new std::string());
  if (!metadata.SerializeToString(serialized.get())) {
    std::move(put_context->callback)
        .Run(MakeErrorStorage(ErrorStorageType::kMetadataSerializationFailed));
    return;
  }

  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(std::move(serialized));

  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = put_context->cache_entry.get();

  net::CompletionCallback write_headers_callback =
      base::AdaptCallbackForRepeating(
          base::BindOnce(&CacheStorageCache::PutDidWriteHeaders,
                         weak_ptr_factory_.GetWeakPtr(), std::move(put_context),
                         buffer->size()));

  rv = temp_entry_ptr->WriteData(INDEX_HEADERS, 0 /* offset */, buffer.get(),
                                 buffer->size(), write_headers_callback,
                                 true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    std::move(write_headers_callback).Run(rv);
}

void CacheStorageCache::PutDidWriteHeaders(
    std::unique_ptr<PutContext> put_context,
    int expected_bytes,
    int rv) {
  if (rv != expected_bytes) {
    put_context->cache_entry->Doom();
    std::move(put_context->callback)
        .Run(MakeErrorStorage(ErrorStorageType::kPutDidWriteHeadersWrongBytes));
    return;
  }

  if (rv > 0)
    storage::RecordBytesWritten(kRecordBytesLabel, rv);
  if (ShouldPadResourceSize(*put_context->response)) {
    cache_padding_ += CalculateResponsePadding(*put_context->response,
                                               cache_padding_key_.get(),
                                               0 /* side_data_size */);
  }

  // The metadata is written, now for the response content. The data is streamed
  // from the blob into the cache entry.
  if (!put_context->response->blob ||
      put_context->response->blob->uuid.empty()) {
    UpdateCacheSize(base::BindOnce(std::move(put_context->callback),
                                   CacheStorageError::kSuccess));
    return;
  }
  PutWriteBlobToCache(std::move(put_context), INDEX_RESPONSE_BODY);
}

void CacheStorageCache::PutWriteBlobToCache(
    std::unique_ptr<PutContext> put_context,
    int disk_cache_body_index) {
  DCHECK(disk_cache_body_index == INDEX_RESPONSE_BODY ||
         disk_cache_body_index == INDEX_SIDE_DATA);

  blink::mojom::BlobPtr blob = disk_cache_body_index == INDEX_RESPONSE_BODY
                                   ? std::move(put_context->blob)
                                   : std::move(put_context->side_data_blob);
  DCHECK(blob);

  int64_t blob_size = disk_cache_body_index == INDEX_RESPONSE_BODY
                          ? put_context->blob_size
                          : put_context->side_data_blob_size;

  disk_cache::ScopedEntryPtr entry(std::move(put_context->cache_entry));
  put_context->cache_entry = nullptr;

  auto blob_to_cache = std::make_unique<CacheStorageBlobToDiskCache>();
  CacheStorageBlobToDiskCache* blob_to_cache_raw = blob_to_cache.get();
  BlobToDiskCacheIDMap::KeyType blob_to_cache_key =
      active_blob_to_disk_cache_writers_.Add(std::move(blob_to_cache));

  blob_to_cache_raw->StreamBlobToCache(
      std::move(entry), disk_cache_body_index, std::move(blob), blob_size,
      base::BindOnce(&CacheStorageCache::PutDidWriteBlobToCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context),
                     blob_to_cache_key));
}

void CacheStorageCache::PutDidWriteBlobToCache(
    std::unique_ptr<PutContext> put_context,
    BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
    disk_cache::ScopedEntryPtr entry,
    bool success) {
  DCHECK(entry);
  put_context->cache_entry = std::move(entry);

  active_blob_to_disk_cache_writers_.Remove(blob_to_cache_key);

  if (!success) {
    put_context->cache_entry->Doom();
    std::move(put_context->callback)
        .Run(MakeErrorStorage(ErrorStorageType::kPutDidWriteBlobToCacheFailed));
    return;
  }

  if (put_context->side_data_blob) {
    PutWriteBlobToCache(std::move(put_context), INDEX_SIDE_DATA);
    return;
  }

  UpdateCacheSize(base::BindOnce(std::move(put_context->callback),
                                 CacheStorageError::kSuccess));
}

void CacheStorageCache::CalculateCacheSizePadding(
    SizePaddingCallback got_sizes_callback) {
  net::Int64CompletionCallback got_size_callback =
      base::AdaptCallbackForRepeating(base::BindOnce(
          &CacheStorageCache::CalculateCacheSizePaddingGotSize,
          weak_ptr_factory_.GetWeakPtr(), std::move(got_sizes_callback)));

  int64_t rv = backend_->CalculateSizeOfAllEntries(got_size_callback);
  if (rv != net::ERR_IO_PENDING)
    std::move(got_size_callback).Run(rv);
}

void CacheStorageCache::CalculateCacheSizePaddingGotSize(
    SizePaddingCallback callback,
    int64_t cache_size) {
  // Enumerating entries is only done during cache initialization and only if
  // necessary.
  DCHECK_EQ(backend_state_, BACKEND_UNINITIALIZED);
  std::unique_ptr<ServiceWorkerFetchRequest> request;
  blink::mojom::QueryParamsPtr options = blink::mojom::QueryParams::New();
  options->ignore_search = true;
  QueryCache(std::move(request), std::move(options),
             QUERY_CACHE_RESPONSES_NO_BODIES,
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
      if (ShouldPadResourceSize(*result.response)) {
        int32_t side_data_size =
            result.entry ? result.entry->GetDataSize(INDEX_SIDE_DATA) : 0;
        cache_padding += CalculateResponsePadding(
            *result.response, cache_padding_key_.get(), side_data_size);
      }
    }
  }

  std::move(callback).Run(cache_size, cache_padding);
}

void CacheStorageCache::CalculateCacheSize(
    const net::Int64CompletionCallback& callback) {
  int64_t rv = backend_->CalculateSizeOfAllEntries(callback);
  if (rv != net::ERR_IO_PENDING)
    callback.Run(rv);
}

void CacheStorageCache::UpdateCacheSize(base::OnceClosure callback) {
  if (backend_state_ != BACKEND_OPEN)
    return;

  // Note that the callback holds a cache handle to keep the cache alive during
  // the operation since this UpdateCacheSize is often run after an operation
  // completes and runs its callback.
  CalculateCacheSize(base::AdaptCallbackForRepeating(
      base::BindOnce(&CacheStorageCache::UpdateCacheSizeGotSize,
                     weak_ptr_factory_.GetWeakPtr(), CreateCacheHandle(),
                     std::move(callback))));
}

void CacheStorageCache::UpdateCacheSizeGotSize(
    CacheStorageCacheHandle cache_handle,
    base::OnceClosure callback,
    int64_t current_cache_size) {
  DCHECK_NE(current_cache_size, CacheStorage::kSizeUnknown);
  cache_size_ = current_cache_size;
  int64_t size_delta = PaddedCacheSize() - last_reported_size_;
  last_reported_size_ = PaddedCacheSize();

  quota_manager_proxy_->NotifyStorageModified(
      CacheStorageQuotaClient::GetIDFromOwner(owner_), origin_,
      blink::mojom::StorageType::kTemporary, size_delta);

  if (cache_storage_)
    cache_storage_->NotifyCacheContentChanged(cache_name_);

  if (cache_observer_)
    cache_observer_->CacheSizeUpdated(this);

  std::move(callback).Run();
}

void CacheStorageCache::GetAllMatchedEntries(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr options,
    CacheEntriesCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysBackendClosed), {});
    return;
  }

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::GetAllMatchedEntriesImpl,
      weak_ptr_factory_.GetWeakPtr(), std::move(request), std::move(options),
      scheduler_->WrapCallbackToRunNext(std::move(callback))));
}

void CacheStorageCache::GetAllMatchedEntriesImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr options,
    CacheEntriesCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(
            ErrorStorageType::kStorageGetAllMatchedEntriesBackendClosed),
        {});
    return;
  }

  QueryCache(
      std::move(request), std::move(options),
      QUERY_CACHE_REQUESTS | QUERY_CACHE_RESPONSES_WITH_BODIES,
      base::BindOnce(&CacheStorageCache::GetAllMatchedEntriesDidQueryCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CacheStorageCache::GetAllMatchedEntriesDidQueryCache(
    CacheEntriesCallback callback,
    blink::mojom::CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error, {});
    return;
  }

  std::vector<CacheEntry> entries;
  entries.reserve(query_cache_results->size());

  for (auto& result : *query_cache_results) {
    entries.emplace_back(std::move(result.request), std::move(result.response));
  }

  std::move(callback).Run(CacheStorageError::kSuccess, std::move(entries));
}

void CacheStorageCache::Delete(blink::mojom::BatchOperationPtr operation,
                               ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(blink::mojom::OperationType::kDelete, operation->operation_type);

  std::unique_ptr<ServiceWorkerFetchRequest> request(
      new ServiceWorkerFetchRequest(
          operation->request.url, operation->request.method,
          operation->request.headers, operation->request.referrer,
          operation->request.is_reload));

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::DeleteImpl, weak_ptr_factory_.GetWeakPtr(),
      std::move(request), std::move(operation->match_params),
      scheduler_->WrapCallbackToRunNext(std::move(callback))));
}

void CacheStorageCache::DeleteImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr match_params,
    ErrorCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kDeleteImplBackendClosed));
    return;
  }

  QueryCache(
      std::move(request), std::move(match_params),
      QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_NO_BODIES,
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

  for (auto& result : *query_cache_results) {
    disk_cache::ScopedEntryPtr entry = std::move(result.entry);
    if (ShouldPadResourceSize(*result.response)) {
      cache_padding_ -=
          CalculateResponsePadding(*result.response, cache_padding_key_.get(),
                                   entry->GetDataSize(INDEX_SIDE_DATA));
    }
    entry->Doom();
  }

  UpdateCacheSize(
      base::BindOnce(std::move(callback), CacheStorageError::kSuccess));
}

void CacheStorageCache::KeysImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    blink::mojom::QueryParamsPtr options,
    RequestsCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysImplBackendClosed), nullptr);
    return;
  }

  QueryCache(
      std::move(request), std::move(options), QUERY_CACHE_REQUESTS,
      base::BindOnce(&CacheStorageCache::KeysDidQueryCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CacheStorageCache::KeysDidQueryCache(
    RequestsCallback callback,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error, nullptr);
    return;
  }

  std::unique_ptr<Requests> out_requests = std::make_unique<Requests>();
  out_requests->reserve(query_cache_results->size());
  for (const auto& result : *query_cache_results)
    out_requests->push_back(*result.request);

  std::move(callback).Run(CacheStorageError::kSuccess, std::move(out_requests));
}

void CacheStorageCache::CloseImpl(base::OnceClosure callback) {
  DCHECK_EQ(BACKEND_OPEN, backend_state_);

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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0));
    return;
  }

  int64_t size = backend_state_ == BACKEND_OPEN ? PaddedCacheSize() : 0;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), size));
}

void CacheStorageCache::GetSizeThenCloseDidGetSize(SizeCallback callback,
                                                   int64_t cache_size) {
  for (auto* handle : blob_data_handles_)
    handle->Invalidate();
  CloseImpl(base::BindOnce(std::move(callback), cache_size));
}

void CacheStorageCache::CreateBackend(ErrorCallback callback) {
  DCHECK(!backend_);

  // Use APP_CACHE as opposed to DISK_CACHE to prevent cache eviction.
  net::CacheType cache_type = memory_only_ ? net::MEMORY_CACHE : net::APP_CACHE;

  // The maximum size of each cache. Ultimately, cache size
  // is controlled per-origin by the QuotaManager.
  uint64_t max_bytes = memory_only_ ? std::numeric_limits<int>::max()
                                    : std::numeric_limits<int64_t>::max();

  std::unique_ptr<ScopedBackendPtr> backend_ptr(new ScopedBackendPtr());

  // Temporary pointer so that backend_ptr can be Pass()'d in Bind below.
  ScopedBackendPtr* backend = backend_ptr.get();

  net::CompletionCallback create_cache_callback =
      base::AdaptCallbackForRepeating(
          base::BindOnce(&CacheStorageCache::CreateBackendDidCreate,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(backend_ptr)));

  int rv = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_SIMPLE, path_, max_bytes,
      false, /* force */
      nullptr, backend,
      base::BindOnce(&CacheStorageCache::DeleteBackendCompletedIO,
                     weak_ptr_factory_.GetWeakPtr()),
      create_cache_callback);
  if (rv != net::ERR_IO_PENDING)
    std::move(create_cache_callback).Run(rv);
}

void CacheStorageCache::CreateBackendDidCreate(
    CacheStorageCache::ErrorCallback callback,
    std::unique_ptr<ScopedBackendPtr> backend_ptr,
    int rv) {
  if (rv != net::OK) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kCreateBackendDidCreateFailed));
    return;
  }

  backend_ = std::move(*backend_ptr);
  std::move(callback).Run(CacheStorageError::kSuccess);
}

void CacheStorageCache::InitBackend() {
  DCHECK_EQ(BACKEND_UNINITIALIZED, backend_state_);
  DCHECK(!initializing_);
  DCHECK(!scheduler_->ScheduledOperations());
  initializing_ = true;

  scheduler_->ScheduleOperation(base::BindOnce(
      &CacheStorageCache::CreateBackend, weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(
          &CacheStorageCache::InitDidCreateBackend,
          weak_ptr_factory_.GetWeakPtr(),
          scheduler_->WrapCallbackToRunNext(base::DoNothing::Once()))));
}

void CacheStorageCache::InitDidCreateBackend(
    base::OnceClosure callback,
    CacheStorageError cache_create_error) {
  if (cache_create_error != CacheStorageError::kSuccess) {
    InitGotCacheSize(std::move(callback), cache_create_error, 0);
    return;
  }

  auto calculate_size_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  int64_t rv = backend_->CalculateSizeOfAllEntries(base::BindOnce(
      &CacheStorageCache::InitGotCacheSize, weak_ptr_factory_.GetWeakPtr(),
      calculate_size_callback, cache_create_error));

  if (rv != net::ERR_IO_PENDING)
    InitGotCacheSize(std::move(calculate_size_callback), cache_create_error,
                     rv);
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
    UMA_HISTOGRAM_COUNTS_10M("ServiceWorkerCache.IndexSizeDifference",
                             std::abs(cache_size_ - cache_size));
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
  cache_size_ = cache_size;
  cache_padding_ = cache_padding;

  initializing_ = false;
  backend_state_ = (cache_create_error == CacheStorageError::kSuccess &&
                    backend_ && backend_state_ == BACKEND_UNINITIALIZED)
                       ? BACKEND_OPEN
                       : BACKEND_CLOSED;

  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.InitBackendResult",
                            cache_create_error);

  if (cache_observer_)
    cache_observer_->CacheSizeUpdated(this);

  std::move(callback).Run();
}

void CacheStorageCache::PopulateResponseBody(
    disk_cache::ScopedEntryPtr entry,
    blink::mojom::FetchAPIResponse* response) {
  DCHECK(blob_storage_context_);

  // Create a blob with the response body data.
  response->blob = blink::mojom::SerializedBlob::New();
  response->blob->size = entry->GetDataSize(INDEX_RESPONSE_BODY);
  response->blob->uuid = base::GenerateGUID();
  auto blob_data =
      std::make_unique<storage::BlobDataBuilder>(response->blob->uuid);

  disk_cache::Entry* temp_entry = entry.get();
  auto data_handle = base::MakeRefCounted<BlobDataHandle>(CreateCacheHandle(),
                                                          std::move(entry));
  blob_data_handles_.insert(data_handle.get());
  blob_data->AppendDiskCacheEntryWithSideData(
      std::move(data_handle), temp_entry, INDEX_RESPONSE_BODY, INDEX_SIDE_DATA);
  auto blob_handle =
      blob_storage_context_->AddFinishedBlob(std::move(blob_data));

  storage::BlobImpl::Create(std::move(blob_handle),
                            MakeRequest(&response->blob->blob));
}

CacheStorageCacheHandle CacheStorageCache::CreateCacheHandle() {
  return cache_storage_->CreateCacheHandle(this);
}

int64_t CacheStorageCache::PaddedCacheSize() const {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (cache_size_ == CacheStorage::kSizeUnknown ||
      cache_padding_ == CacheStorage::kSizeUnknown) {
    return CacheStorage::kSizeUnknown;
  }
  return cache_size_ + cache_padding_;
}

}  // namespace content
