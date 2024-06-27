// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/code_cache_host_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/io_buffer.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::CacheStorageError;

namespace content {

namespace {

bool CheckSecurityForAccessingCodeCacheData(
    const GURL& resource_url,
    int render_process_id,
    CodeCacheHostImpl::Operation operation) {
  ProcessLock process_lock =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
          render_process_id);

  // Code caching is only allowed for http(s) and chrome/chrome-untrusted
  // scripts. Furthermore, there is no way for http(s) pages to load chrome or
  // chrome-untrusted scripts, so any http(s) page attempting to store data
  // about a chrome or chrome-untrusted script would be an indication of
  // suspicious activity.
  if (resource_url.SchemeIs(content::kChromeUIScheme) ||
      resource_url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    if (!process_lock.is_locked_to_site()) {
      // We can't tell for certain whether this renderer is doing something
      // malicious, but we don't trust it enough to store data.
      return false;
    }
    if (process_lock.matches_scheme(url::kHttpScheme) ||
        process_lock.matches_scheme(url::kHttpsScheme)) {
      if (operation == CodeCacheHostImpl::Operation::kWrite) {
        mojo::ReportBadMessage("HTTP(S) pages cannot cache WebUI code");
      }
      return false;
    }
    // Other schemes which might successfully load chrome or chrome-untrusted
    // scripts, such as the PDF viewer, are unsupported but not considered
    // dangerous.
    return process_lock.matches_scheme(content::kChromeUIScheme) ||
           process_lock.matches_scheme(content::kChromeUIUntrustedScheme);
  }
  if (resource_url.SchemeIsHTTPOrHTTPS() ||
      blink::CommonSchemeRegistry::IsExtensionScheme(resource_url.scheme())) {
    if (process_lock.matches_scheme(content::kChromeUIScheme) ||
        process_lock.matches_scheme(content::kChromeUIUntrustedScheme)) {
      // It is possible for WebUI pages to include open-web content, but such
      // usage is rare and we've decided that reasoning about security is easier
      // if the WebUI code cache includes only WebUI scripts.
      return false;
    }
    return true;
  }

  if (operation == CodeCacheHostImpl::Operation::kWrite) {
    mojo::ReportBadMessage("Invalid URL scheme for code cache.");
  }
  return false;
}

void DidGenerateCacheableMetadataInCacheStorageOnUI(
    const GURL& url,
    base::Time expected_response_time,
    mojo_base::BigBuffer data,
    const std::string& cache_storage_cache_name,
    int render_process_id,
    const blink::StorageKey& code_cache_storage_key,
    storage::mojom::CacheStorageControl* cache_storage_control_for_testing,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* render_process_host = RenderProcessHost::FromID(render_process_id);
  if (!render_process_host)
    return;

  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1(
      "CacheStorage",
      "CodeCacheHostImpl::DidGenerateCacheableMetadataInCacheStorage",
      TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT, "url", url.spec());

  mojo::Remote<blink::mojom::CacheStorage> remote;
  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;
  network::DocumentIsolationPolicy document_isolation_policy;

  storage::mojom::CacheStorageControl* cache_storage_control =
      cache_storage_control_for_testing
          ? cache_storage_control_for_testing
          : render_process_host->GetStoragePartition()
                ->GetCacheStorageControl();

  cache_storage_control->AddReceiver(
      cross_origin_embedder_policy, mojo::NullRemote(),
      document_isolation_policy,
      storage::BucketLocator::ForDefaultBucket(code_cache_storage_key),
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

void AddCodeCacheReceiver(
    mojo::UniqueReceiverSet<blink::mojom::CodeCacheHost>* receiver_set,
    scoped_refptr<GeneratedCodeCacheContext> context,
    int render_process_id,
    const net::NetworkIsolationKey& nik,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver,
    CodeCacheHostImpl::ReceiverSet::CodeCacheHostReceiverHandler handler) {
  auto host = std::make_unique<CodeCacheHostImpl>(render_process_id, context,
                                                  nik, storage_key);
  auto* raw_host = host.get();
  auto id = receiver_set->Add(std::move(host), std::move(receiver));
  if (handler)
    std::move(handler).Run(raw_host, id, *receiver_set);
}

}  // namespace

bool CodeCacheHostImpl::use_empty_secondary_key_for_testing_ = false;

CodeCacheHostImpl::ReceiverSet::ReceiverSet(
    scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context)
    : generated_code_cache_context_(generated_code_cache_context),
      receiver_set_(
          new mojo::UniqueReceiverSet<blink::mojom::CodeCacheHost>(),
          base::OnTaskRunnerDeleter(GeneratedCodeCacheContext::GetTaskRunner(
              generated_code_cache_context))) {}

CodeCacheHostImpl::ReceiverSet::~ReceiverSet() = default;

void CodeCacheHostImpl::ReceiverSet::Add(
    int render_process_id,
    const net::NetworkIsolationKey& nik,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver,
    CodeCacheHostReceiverHandler handler) {
  if (!receiver_set_) {
    receiver_set_ = {
        new mojo::UniqueReceiverSet<blink::mojom::CodeCacheHost>(),
        base::OnTaskRunnerDeleter(GeneratedCodeCacheContext::GetTaskRunner(
            generated_code_cache_context_))};
  }
  // |receiver_set_| will be deleted on the code cache thread, so it is safe to
  // post a task to the code cache thread with the raw pointer.
  GeneratedCodeCacheContext::RunOrPostTask(
      generated_code_cache_context_, FROM_HERE,
      base::BindOnce(&AddCodeCacheReceiver, receiver_set_.get(),
                     generated_code_cache_context_, render_process_id, nik,
                     storage_key, std::move(receiver), std::move(handler)));
}

void CodeCacheHostImpl::ReceiverSet::Add(
    int render_process_id,
    const net::NetworkIsolationKey& nik,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver) {
  Add(render_process_id, nik, storage_key, std::move(receiver),
      CodeCacheHostReceiverHandler());
}

void CodeCacheHostImpl::ReceiverSet::Clear() {
  receiver_set_.reset();
}

CodeCacheHostImpl::CodeCacheHostImpl(
    int render_process_id,
    scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context,
    const net::NetworkIsolationKey& nik,
    const blink::StorageKey& storage_key)
    : render_process_id_(render_process_id),
      generated_code_cache_context_(std::move(generated_code_cache_context)),
      network_isolation_key_(nik),
      storage_key_(storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CodeCacheHostImpl::~CodeCacheHostImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
  if (!code_cache)
    return;

  std::optional<GURL> secondary_key =
      GetSecondaryKeyForCodeCache(url, render_process_id_, Operation::kWrite);
  if (!secondary_key) {
    return;
  }

  code_cache->WriteEntry(url, *secondary_key, network_isolation_key_,
                         expected_response_time, std::move(data));
}

void CodeCacheHostImpl::FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                                        const GURL& url,
                                        FetchCachedCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
  if (!code_cache) {
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  std::optional<GURL> secondary_key =
      GetSecondaryKeyForCodeCache(url, render_process_id_, Operation::kRead);
  if (!secondary_key) {
    std::move(callback).Run(base::Time(), std::vector<uint8_t>());
    return;
  }

  auto read_callback = base::BindOnce(
      &CodeCacheHostImpl::OnReceiveCachedCode, weak_ptr_factory_.GetWeakPtr(),
      cache_type, base::TimeTicks::Now(), std::move(callback));
  code_cache->FetchEntry(url, *secondary_key, network_isolation_key_,
                         std::move(read_callback));
}

void CodeCacheHostImpl::ClearCodeCacheEntry(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
  if (!code_cache)
    return;

  std::optional<GURL> secondary_key =
      GetSecondaryKeyForCodeCache(url, render_process_id_, Operation::kRead);
  if (!secondary_key) {
    return;
  }

  code_cache->DeleteEntry(url, *secondary_key, network_isolation_key_);
}

void CodeCacheHostImpl::DidGenerateCacheableMetadataInCacheStorage(
    const GURL& url,
    base::Time expected_response_time,
    mojo_base::BigBuffer data,
    const std::string& cache_storage_cache_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DidGenerateCacheableMetadataInCacheStorageOnUI, url,
                     expected_response_time, std::move(data),
                     cache_storage_cache_name, render_process_id_, storage_key_,
                     cache_storage_control_for_testing_,
                     mojo::GetBadMessageCallback()));
}

GeneratedCodeCache* CodeCacheHostImpl::GetCodeCache(
    blink::mojom::CodeCacheType cache_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!generated_code_cache_context_)
    return nullptr;

  ProcessLock process_lock =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
          render_process_id_);

  // To minimize the chance of any cache bug resulting in privilege escalation
  // from an ordinary web page to trusted WebUI, we use a completely separate
  // GeneratedCodeCache instance for WebUI pages.
  if (process_lock.matches_scheme(content::kChromeUIScheme) ||
      process_lock.matches_scheme(content::kChromeUIUntrustedScheme)) {
    if (cache_type == blink::mojom::CodeCacheType::kJavascript) {
      return generated_code_cache_context_->generated_webui_js_code_cache();
    }

    // WebAssembly in WebUI pages is not supported due to no current usage.
    return nullptr;
  }

  if (cache_type == blink::mojom::CodeCacheType::kJavascript)
    return generated_code_cache_context_->generated_js_code_cache();

  DCHECK_EQ(blink::mojom::CodeCacheType::kWebAssembly, cache_type);
  return generated_code_cache_context_->generated_wasm_code_cache();
}

void CodeCacheHostImpl::OnReceiveCachedCode(
    blink::mojom::CodeCacheType cache_type,
    base::TimeTicks start_time,
    FetchCachedCodeCallback callback,
    const base::Time& response_time,
    mojo_base::BigBuffer data) {
  if (cache_type == blink::mojom::CodeCacheType::kJavascript &&
      data.size() > 0) {
    base::UmaHistogramTimes("SiteIsolatedCodeCache.JS.FetchCodeCache",
                            base::TimeTicks::Now() - start_time);
  }
  std::move(callback).Run(response_time, std::move(data));
}

// Code caches use two keys: the URL of requested resource |resource_url|
// as the primary key and the origin lock of the renderer that requested this
// resource as secondary key. This function returns the origin lock of the
// renderer that will be used as the secondary key for the code cache.
// The secondary key is:
// Case 0. std::nullopt if the resource URL or origin lock have unsupported
// schemes, or if they represent potentially dangerous combinations such as
// WebUI code in an open-web page.
// Case 1. an empty GURL if the render process is not locked to an origin. In
// this case, code cache uses |resource_url| as the key.
// Case 2. a std::nullopt, if the origin lock is opaque (for ex: browser
// initiated navigation to a data: URL). In these cases, the code should not be
// cached since the serialized value of opaque origins should not be used as a
// key.
// Case 3: origin_lock if the scheme of origin_lock is
// Http/Https/chrome/chrome-untrusted.
// Case 4. std::nullopt otherwise.
std::optional<GURL> CodeCacheHostImpl::GetSecondaryKeyForCodeCache(
    const GURL& resource_url,
    int render_process_id,
    CodeCacheHostImpl::Operation operation) {
  if (use_empty_secondary_key_for_testing_) {
    return GURL();
  }
  // Case 0: check for invalid schemes.
  if (!CheckSecurityForAccessingCodeCacheData(resource_url, render_process_id,
                                              operation)) {
    return std::nullopt;
  }
  if (!resource_url.is_valid()) {
    return std::nullopt;
  }

  ProcessLock process_lock =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
          render_process_id);

  // Case 1: If process is not locked to a site, it is safe to just use the
  // |resource_url| of the requested resource as the key. Return an empty GURL
  // as the second key.
  if (!process_lock.is_locked_to_site()) {
    return GURL();
  }

  // Case 2: Don't cache the code corresponding to opaque origins. The same
  // origin checks should always fail for opaque origins but the serialized
  // value of opaque origins does not ensure this.
  // NOTE: HasOpaqueOrigin() will return true if the ProcessLock lock url is
  // invalid, leading to a return value of std::nullopt.
  if (process_lock.HasOpaqueOrigin()) {
    return std::nullopt;
  }

  // Case 3: process_lock_url is used to enfore site-isolation in code caches.
  // Http/https/chrome schemes are safe to be used as a secondary key. Other
  // schemes could be enabled if they are known to be safe and if it is
  // required to cache code from those origins.
  //
  // file:// URLs will have a "file:" process lock and would thus share a
  // cache across all file:// URLs. That would likely be ok for security, but
  // since this case is not performance sensitive we will keep things simple and
  // limit the cache to http/https/chrome/chrome-untrusted processes.
  if (process_lock.matches_scheme(url::kHttpScheme) ||
      process_lock.matches_scheme(url::kHttpsScheme) ||
      process_lock.matches_scheme(content::kChromeUIScheme) ||
      process_lock.matches_scheme(content::kChromeUIUntrustedScheme) ||
      blink::CommonSchemeRegistry::IsExtensionScheme(
          process_lock.lock_url().scheme())) {
    return process_lock.lock_url();
  }

  return std::nullopt;
}

}  // namespace content
