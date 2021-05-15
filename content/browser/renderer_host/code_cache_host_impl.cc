// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/code_cache_host_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
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

// Code caches use two keys: the URL of requested resource |resource_url|
// as the primary key and the origin lock of the renderer that requested this
// resource as secondary key. This function returns the origin lock of the
// renderer that will be used as the secondary key for the code cache.
// The secondary key is:
// Case 1. an empty GURL if the render process is not locked to an origin. In
// this case, code cache uses |resource_url| as the key.
// Case 2. a absl::nullopt, if the origin lock is opaque (for ex: browser
// initiated navigation to a data: URL). In these cases, the code should not be
// cached since the serialized value of opaque origins should not be used as a
// key.
// Case 3: origin_lock if the scheme of origin_lock is Http/Https/chrome.
// Case 4. absl::nullopt otherwise.
absl::optional<GURL> GetSecondaryKeyForCodeCache(const GURL& resource_url,
                                                 int render_process_id) {
  if (!resource_url.is_valid() || !resource_url.SchemeIsHTTPOrHTTPS())
    return absl::nullopt;

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
  // invalid, leading to a return value of absl::nullopt.
  if (process_lock.HasOpaqueOrigin())
    return absl::nullopt;

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

  return absl::nullopt;
}

}  // namespace

CodeCacheHostImpl::CodeCacheHostImpl(
    int render_process_id,
    RenderProcessHostImpl* render_process_host_impl,
    scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context,
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver)
    : render_process_id_(render_process_id),
      render_process_host_impl_(render_process_host_impl),
      generated_code_cache_context_(std::move(generated_code_cache_context)),
      receiver_(this, std::move(receiver)) {
  // render_process_host_impl may be null in tests.
}

CodeCacheHostImpl::~CodeCacheHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void CodeCacheHostImpl::SetCacheStorageControlForTesting(
    storage::mojom::CacheStorageControl* cache_storage_control) {
  cache_storage_control_for_testing_ = cache_storage_control;
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

  absl::optional<GURL> origin_lock =
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

  absl::optional<GURL> origin_lock =
      GetSecondaryKeyForCodeCache(url, render_process_id_);
  if (!origin_lock) {
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  auto read_callback =
      base::BindOnce(&CodeCacheHostImpl::OnReceiveCachedCode,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  code_cache->FetchEntry(url, *origin_lock, std::move(read_callback));
}

void CodeCacheHostImpl::ClearCodeCacheEntry(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url) {
  GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
  if (!code_cache)
    return;

  absl::optional<GURL> origin_lock =
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
  if (!origin_allowed) {
    receiver_.ReportBadMessage("Bad cache_storage origin.");
    return;
  }

  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1(
      "CacheStorage",
      "CodeCacheHostImpl::DidGenerateCacheableMetadataInCacheStorage",
      TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT, "url", url.spec());

  mojo::Remote<blink::mojom::CacheStorage> remote;
  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;

  storage::mojom::CacheStorageControl* cache_storage_control =
      cache_storage_control_for_testing_
          ? cache_storage_control_for_testing_
          : render_process_host_impl_->GetStoragePartition()
                ->GetCacheStorageControl();
  cache_storage_control->AddReceiver(
      cross_origin_embedder_policy, mojo::NullRemote(), cache_storage_origin,
      storage::mojom::CacheStorageOwner::kCacheAPI,
      remote.BindNewPipeAndPassReceiver());

  // Call the remote pointer directly so we can pass the remote to the callback
  // itself to preserve its lifetime.
  auto* raw_remote = remote.get();
  raw_remote->Open(
      base::UTF8ToUTF16(cache_storage_cache_name), trace_id,
      base::BindOnce(
          [](const GURL& url, base::Time expected_response_time,
             mojo_base::BigBuffer data, int64_t trace_id,
             mojo::Remote<blink::mojom::CacheStorage> preserve_remote_lifetime,
             blink::mojom::OpenResultPtr result) {
            if (result->is_status()) {
              // Silently ignore errors.
              return;
            }

            mojo::AssociatedRemote<blink::mojom::CacheStorageCache> remote;
            remote.Bind(std::move(result->get_cache()));
            remote->WriteSideData(
                url, expected_response_time, std::move(data), trace_id,
                base::BindOnce(
                    [](mojo::Remote<blink::mojom::CacheStorage>
                           preserve_remote_lifetime,
                       CacheStorageError error) {
                      // Silently ignore errors.
                    },
                    std::move(preserve_remote_lifetime)));
          },
          url, expected_response_time, std::move(data), trace_id,
          std::move(remote)));
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

}  // namespace content
