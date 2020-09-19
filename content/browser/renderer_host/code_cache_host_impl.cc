// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/code_cache_host_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/io_buffer.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::CacheStorageError;

namespace content {

namespace {

void NoOpCacheStorageErrorCallback(CacheStorageCacheHandle cache_handle,
                                   CacheStorageError error) {}

// Code caches use two keys: the URL of requested resource |resource_url|
// as the primary key and the origin lock of the renderer that requested this
// resource as secondary key. This function returns the origin lock of the
// renderer that will be used as the secondary key for the code cache.
// The secondary key is:
// Case 1. an empty GURL if the render process is not locked to an origin. In
// this case, code cache uses |resource_url| as the key.
// Case 2. a base::nullopt, if the origin lock is opaque (for ex: browser
// initiated navigation to a data: URL). In these cases, the code should not be
// cached since the serialized value of opaque origins should not be used as a
// key.
// Case 3: origin_lock if the scheme of origin_lock is Http/Https/chrome.
// Case 4. base::nullopt otherwise.
base::Optional<GURL> GetSecondaryKeyForCodeCache(const GURL& resource_url,
                                                 int render_process_id) {
  if (!resource_url.is_valid() || !resource_url.SchemeIsHTTPOrHTTPS())
    return base::nullopt;

  ProcessLock process_lock =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
          render_process_id);

  // Case 1: If process is not locked to a site, it is safe to just use the
  // |resource_url| of the requested resource as the key. Return an empty GURL
  // as the second key.
  if (!process_lock.is_locked_to_site())
    return GURL::EmptyGURL();

  // Case 2: Don't cache the code corresponding to opaque origins. The same
  // origin checks should always fail for opaque origins but the serialized
  // value of opaque origins does not ensure this.
  // NOTE: HasOpaqueOrigin() will return true if the ProcessLock lock url is
  // invalid, leading to a return value of base::nullopt.
  if (process_lock.HasOpaqueOrigin())
    return base::nullopt;

  // Case 3: process_lock_url is used to enfore site-isolation in code caches.
  // Http/https/chrome schemes are safe to be used as a secondary key. Other
  // schemes could be enabled if they are known to be safe and if it is
  // required to cache code from those origins.
  //
  // file:// URLs will have a "file:" process lock and would thus share a
  // cache across all file:// URLs. That would likely be ok for security, but
  // since this case is not performance sensitive we will keep things simple and
  // limit the cache to http/https/chrome processes.
  if (process_lock.matches_scheme(url::kHttpScheme) ||
      process_lock.matches_scheme(url::kHttpsScheme) ||
      process_lock.matches_scheme(content::kChromeUIScheme)) {
    return process_lock.lock_url();
  }

  return base::nullopt;
}

}  // namespace

CodeCacheHostImpl::CodeCacheHostImpl(
    int render_process_id,
    scoped_refptr<CacheStorageContextImpl> cache_storage_context,
    scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context,
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver)
    : render_process_id_(render_process_id),
      cache_storage_context_(std::move(cache_storage_context)),
      generated_code_cache_context_(std::move(generated_code_cache_context)),
      receiver_(this, std::move(receiver)) {}

CodeCacheHostImpl::~CodeCacheHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void CodeCacheHostImpl::SetCacheStorageContextForTesting(
    scoped_refptr<CacheStorageContextImpl> context) {
  cache_storage_context_ = std::move(context);
}

void CodeCacheHostImpl::DidGenerateCacheableMetadata(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url,
    base::Time expected_response_time,
    mojo_base::BigBuffer data) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    mojo::ReportBadMessage("Invalid URL scheme for code cache.");
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
  if (!code_cache)
    return;

  base::Optional<GURL> origin_lock =
      GetSecondaryKeyForCodeCache(url, render_process_id_);
  if (!origin_lock)
    return;

  code_cache->WriteEntry(url, *origin_lock, expected_response_time,
                         std::move(data));
}

void CodeCacheHostImpl::FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                                        const GURL& url,
                                        FetchCachedCodeCallback callback) {
  GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
  if (!code_cache) {
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  base::Optional<GURL> origin_lock =
      GetSecondaryKeyForCodeCache(url, render_process_id_);
  if (!origin_lock) {
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  auto read_callback = base::BindRepeating(
      &CodeCacheHostImpl::OnReceiveCachedCode, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(&callback));
  code_cache->FetchEntry(url, *origin_lock, read_callback);
}

void CodeCacheHostImpl::ClearCodeCacheEntry(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url) {
  GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
  if (!code_cache)
    return;

  base::Optional<GURL> origin_lock =
      GetSecondaryKeyForCodeCache(url, render_process_id_);
  if (!origin_lock)
    return;

  code_cache->DeleteEntry(url, *origin_lock);
}

void CodeCacheHostImpl::DidGenerateCacheableMetadataInCacheStorage(
    const GURL& url,
    base::Time expected_response_time,
    mojo_base::BigBuffer data,
    const url::Origin& cache_storage_origin,
    const std::string& cache_storage_cache_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We cannot trust the renderer to give us the correct origin here.  Validate
  // it against the ChildProcessSecurityPolicy.
  bool origin_allowed =
      ChildProcessSecurityPolicyImpl::GetInstance()->CanAccessDataForOrigin(
          render_process_id_, cache_storage_origin);
  base::UmaHistogramBoolean(
      "ServiceWorkerCache.DidGenerateCacheableMetadataMessageInCacheStorage."
      "OriginAllowed",
      origin_allowed);
  if (!origin_allowed) {
    // TODO(crbug/925035): Report a bad mojo message here.  Currently we just
    // null-route the request since this condition triggers more frequently
    // than we expect.
    return;
  }

  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1(
      "CacheStorage",
      "CodeCacheHostImpl::DidGenerateCacheableMetadataInCacheStorage",
      TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT, "url", url.spec());

  scoped_refptr<CacheStorageManager> manager =
      cache_storage_context_->CacheManager();
  if (!manager)
    return;

  scoped_refptr<net::IOBuffer> buf =
      base::MakeRefCounted<net::IOBuffer>(data.size());
  if (data.size())
    memcpy(buf->data(), data.data(), data.size());

  CacheStorageHandle cache_storage = manager->OpenCacheStorage(
      cache_storage_origin, CacheStorageOwner::kCacheAPI);
  cache_storage.value()->OpenCache(
      cache_storage_cache_name, trace_id,
      base::BindOnce(&CodeCacheHostImpl::OnCacheStorageOpenCallback,
                     weak_ptr_factory_.GetWeakPtr(), url,
                     expected_response_time, trace_id, buf, data.size()));
}

GeneratedCodeCache* CodeCacheHostImpl::GetCodeCache(
    blink::mojom::CodeCacheType cache_type) {
  if (!generated_code_cache_context_)
    return nullptr;

  if (cache_type == blink::mojom::CodeCacheType::kJavascript)
    return generated_code_cache_context_->generated_js_code_cache();

  DCHECK_EQ(blink::mojom::CodeCacheType::kWebAssembly, cache_type);
  return generated_code_cache_context_->generated_wasm_code_cache();
}

void CodeCacheHostImpl::OnReceiveCachedCode(FetchCachedCodeCallback callback,
                                            const base::Time& response_time,
                                            mojo_base::BigBuffer data) {
  std::move(callback).Run(response_time, std::move(data));
}

void CodeCacheHostImpl::OnCacheStorageOpenCallback(
    const GURL& url,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buf,
    int buf_len,
    CacheStorageCacheHandle cache_handle,
    CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW1(
      "CacheStorage", "CodeCacheHostImpl::OnCacheStorageOpenCallback",
      TRACE_ID_GLOBAL(trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", url.spec());
  if (error != CacheStorageError::kSuccess || !cache_handle.value())
    return;
  CacheStorageCache* cache = cache_handle.value();
  cache->WriteSideData(
      base::BindOnce(&NoOpCacheStorageErrorCallback, std::move(cache_handle)),
      url, expected_response_time, trace_id, buf, buf_len);
}

}  // namespace content
