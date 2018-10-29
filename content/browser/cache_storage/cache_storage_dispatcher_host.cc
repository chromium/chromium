// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseError;

const int32_t kCachePreservationSeconds = 5;

// TODO(lucmult): Check this before binding.
bool OriginCanAccessCacheStorage(const url::Origin& origin) {
  return !origin.opaque() && IsOriginSecure(origin.GetURL());
}

void StopPreservingCache(CacheStorageCacheHandle cache_handle) {}

}  // namespace

// Implements the mojom interface CacheStorageCache. It's owned by
// CacheStorageDispatcherHost and it's destroyed when client drops the mojo ptr
// which in turn removes from StrongBindingSet in CacheStorageDispatcherHost.
class CacheStorageDispatcherHost::CacheImpl
    : public blink::mojom::CacheStorageCache {
 public:
  CacheImpl(CacheStorageCacheHandle cache_handle,
            CacheStorageDispatcherHost* dispatcher_host)
      : cache_handle_(std::move(cache_handle)),
        owner_(dispatcher_host),
        weak_factory_(this) {}

  ~CacheImpl() override = default;

  // blink::mojom::CacheStorageCache implementation:
  void Match(const ServiceWorkerFetchRequest& request,
             blink::mojom::QueryParamsPtr match_params,
             MatchCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    auto scoped_request = std::make_unique<ServiceWorkerFetchRequest>(
        request.url, request.method, request.headers, request.referrer,
        request.is_reload);

    cache->Match(
        std::move(scoped_request), std::move(match_params),
        base::BindOnce(&CacheImpl::OnCacheMatchCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnCacheMatchCallback(
      blink::mojom::CacheStorageCache::MatchCallback callback,
      blink::mojom::CacheStorageError error,
      blink::mojom::FetchAPIResponsePtr response) {
    if (error != CacheStorageError::kSuccess) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(error));
      return;
    }

    std::move(callback).Run(
        blink::mojom::MatchResult::NewResponse(std::move(response)));
  }

  void MatchAll(const base::Optional<ServiceWorkerFetchRequest>& request,
                blink::mojom::QueryParamsPtr match_params,
                MatchAllCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::MatchAllResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    std::unique_ptr<ServiceWorkerFetchRequest> request_ptr;

    if (request && !request->url.is_empty()) {
      request_ptr = std::make_unique<ServiceWorkerFetchRequest>(
          request->url, request->method, request->headers, request->referrer,
          request->is_reload);
    }

    cache->MatchAll(
        std::move(request_ptr), std::move(match_params),
        base::BindOnce(&CacheImpl::OnCacheMatchAllCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnCacheMatchAllCallback(
      blink::mojom::CacheStorageCache::MatchAllCallback callback,
      blink::mojom::CacheStorageError error,
      std::vector<blink::mojom::FetchAPIResponsePtr> responses) {
    if (error != CacheStorageError::kSuccess &&
        error != CacheStorageError::kErrorNotFound) {
      std::move(callback).Run(blink::mojom::MatchAllResult::NewStatus(error));
      return;
    }

    std::move(callback).Run(
        blink::mojom::MatchAllResult::NewResponses(std::move(responses)));
  }

  void Keys(const base::Optional<ServiceWorkerFetchRequest>& request,
            blink::mojom::QueryParamsPtr match_params,
            KeysCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::CacheKeysResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    std::unique_ptr<ServiceWorkerFetchRequest> request_ptr;

    if (request) {
      request_ptr = std::make_unique<ServiceWorkerFetchRequest>(
          request->url, request->method, request->headers, request->referrer,
          request->is_reload);
    }
    cache->Keys(
        std::move(request_ptr), std::move(match_params),
        base::BindOnce(&CacheImpl::OnCacheKeysCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnCacheKeysCallback(
      blink::mojom::CacheStorageCache::KeysCallback callback,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<content::CacheStorageCache::Requests> requests) {
    if (error != CacheStorageError::kSuccess) {
      std::move(callback).Run(blink::mojom::CacheKeysResult::NewStatus(error));
      return;
    }

    std::move(callback).Run(blink::mojom::CacheKeysResult::NewKeys(*requests));
  }

  void Batch(std::vector<blink::mojom::BatchOperationPtr> batch_operations,
             bool fail_on_duplicates,
             BatchCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(CacheStorageVerboseError::New(
          CacheStorageError::kErrorNotFound, base::nullopt));
      return;
    }
    cache->BatchOperation(
        std::move(batch_operations), fail_on_duplicates,
        base::BindOnce(&CacheImpl::OnCacheBatchCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&CacheImpl::OnBadMessage, weak_factory_.GetWeakPtr(),
                       mojo::GetBadMessageCallback()));
  }

  void OnCacheBatchCallback(
      blink::mojom::CacheStorageCache::BatchCallback callback,
      blink::mojom::CacheStorageVerboseErrorPtr error) {
    std::move(callback).Run(std::move(error));
  }

  void OnBadMessage(mojo::ReportBadMessageCallback bad_message_callback) {
    std::move(bad_message_callback).Run("CSDH_UNEXPECTED_OPERATION");
  }

  CacheStorageCacheHandle cache_handle_;

  // Owns this.
  CacheStorageDispatcherHost* const owner_;

  base::WeakPtrFactory<CacheImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheImpl);
};

CacheStorageDispatcherHost::CacheStorageDispatcherHost() = default;

CacheStorageDispatcherHost::~CacheStorageDispatcherHost() = default;

void CacheStorageDispatcherHost::Init(CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CacheStorageDispatcherHost::CreateCacheListener,
                     base::RetainedRef(this), base::RetainedRef(context)));
}

void CacheStorageDispatcherHost::CreateCacheListener(
    CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_ = context;
}

void CacheStorageDispatcherHost::Has(
    const base::string16& cache_name,
    blink::mojom::CacheStorage::HasCallback callback) {
  TRACE_EVENT0("CacheStorage", "CacheStorageDispatcherHost::OnCacheStorageHas");
  url::Origin origin = bindings_.dispatch_context();
  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  context_->cache_manager()->HasCache(
      origin, CacheStorageOwner::kCacheAPI, base::UTF16ToUTF8(cache_name),
      base::BindOnce(&CacheStorageDispatcherHost::OnHasCallback, this,
                     std::move(callback)));
}

void CacheStorageDispatcherHost::Open(
    const base::string16& cache_name,
    blink::mojom::CacheStorage::OpenCallback callback) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageOpen");
  url::Origin origin = bindings_.dispatch_context();
  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  context_->cache_manager()->OpenCache(
      origin, CacheStorageOwner::kCacheAPI, base::UTF16ToUTF8(cache_name),
      base::BindOnce(&CacheStorageDispatcherHost::OnOpenCallback, this, origin,
                     std::move(callback)));
}

void CacheStorageDispatcherHost::Delete(
    const base::string16& cache_name,
    blink::mojom::CacheStorage::DeleteCallback callback) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageDelete");
  url::Origin origin = bindings_.dispatch_context();
  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  context_->cache_manager()->DeleteCache(origin, CacheStorageOwner::kCacheAPI,
                                         base::UTF16ToUTF8(cache_name),

                                         std::move(callback));
}

void CacheStorageDispatcherHost::Keys(
    blink::mojom::CacheStorage::KeysCallback callback) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageKeys");
  url::Origin origin = bindings_.dispatch_context();

  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  context_->cache_manager()->EnumerateCaches(
      origin, CacheStorageOwner::kCacheAPI,
      base::BindOnce(&CacheStorageDispatcherHost::OnKeysCallback, this,
                     std::move(callback)));
}

void CacheStorageDispatcherHost::Match(
    const content::ServiceWorkerFetchRequest& request,
    blink::mojom::QueryParamsPtr match_params,
    blink::mojom::CacheStorage::MatchCallback callback) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageMatch");
  url::Origin origin = bindings_.dispatch_context();
  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  auto scoped_request = std::make_unique<ServiceWorkerFetchRequest>(
      request.url, request.method, request.headers, request.referrer,
      request.is_reload);

  if (!match_params->cache_name) {
    context_->cache_manager()->MatchAllCaches(
        origin, CacheStorageOwner::kCacheAPI, std::move(scoped_request),
        std::move(match_params),
        base::BindOnce(&CacheStorageDispatcherHost::OnMatchCallback, this,
                       std::move(callback)));
    return;
  }
  std::string cache_name = base::UTF16ToUTF8(*match_params->cache_name);
  context_->cache_manager()->MatchCache(
      origin, CacheStorageOwner::kCacheAPI, std::move(cache_name),
      std::move(scoped_request), std::move(match_params),
      base::BindOnce(&CacheStorageDispatcherHost::OnMatchCallback, this,
                     std::move(callback)));
}

void CacheStorageDispatcherHost::OnHasCallback(
    blink::mojom::CacheStorage::HasCallback callback,
    bool has_cache,
    CacheStorageError error) {
  if (!has_cache)
    error = CacheStorageError::kErrorNotFound;
  std::move(callback).Run(error);
}

void CacheStorageDispatcherHost::OnOpenCallback(
    url::Origin origin,
    blink::mojom::CacheStorage::OpenCallback callback,
    CacheStorageCacheHandle cache_handle,
    CacheStorageError error) {
  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(blink::mojom::OpenResult::NewStatus(error));
    return;
  }

  // Hang on to the cache for a few seconds. This way if the user quickly closes
  // and reopens it the cache backend won't have to be reinitialized.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&StopPreservingCache, cache_handle.Clone()),
      base::TimeDelta::FromSeconds(kCachePreservationSeconds));

  blink::mojom::CacheStorageCacheAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  auto cache_impl = std::make_unique<CacheImpl>(std::move(cache_handle), this);
  cache_bindings_.AddBinding(std::move(cache_impl), std::move(request));

  std::move(callback).Run(
      blink::mojom::OpenResult::NewCache(std::move(ptr_info)));
}

void CacheStorageDispatcherHost::OnKeysCallback(
    blink::mojom::CacheStorage::KeysCallback callback,
    const CacheStorageIndex& cache_index) {
  std::vector<base::string16> string16s;
  for (const auto& metadata : cache_index.ordered_cache_metadata()) {
    string16s.push_back(base::UTF8ToUTF16(metadata.name));
  }

  std::move(callback).Run(string16s);
}

void CacheStorageDispatcherHost::OnMatchCallback(
    blink::mojom::CacheStorage::MatchCallback callback,
    CacheStorageError error,
    blink::mojom::FetchAPIResponsePtr response) {
  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(blink::mojom::MatchResult::NewStatus(error));
    return;
  }

  std::move(callback).Run(
      blink::mojom::MatchResult::NewResponse(std::move(response)));
}

void CacheStorageDispatcherHost::AddBinding(
    blink::mojom::CacheStorageRequest request,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bindings_.AddBinding(this, std::move(request), origin);
}

bool CacheStorageDispatcherHost::ValidState() {
  // cache_manager() can return nullptr when process is shutting down.
  if (!(context_ && context_->cache_manager())) {
    bindings_.CloseAllBindings();
    return false;
  }
  return true;
}

}  // namespace content
