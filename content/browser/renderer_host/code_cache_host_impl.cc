// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/code_cache_host_impl.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/persistent_cache/entry.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/io_buffer.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-data-view.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::CacheStorageError;

namespace content {

namespace {

// The key used for the PersistentCacheCollection when a unique context cannot
// be determined and strict site isolation is disabled. This groups entries in
// the same way as with GeneratedCodeCache under the same conditions.
constexpr const char kSharedContextKeyForRelaxedIsolation[] =
    "_shared_context_for_relaxed_isolation";

GeneratedCodeCache::CodeCacheType MojoCacheTypeToCodeCacheType(
    blink::mojom::CodeCacheType type) {
  switch (type) {
    case blink::mojom::CodeCacheType::kJavascript:
      return GeneratedCodeCache::CodeCacheType::kJavaScript;
    case blink::mojom::CodeCacheType::kWebAssembly:
      return GeneratedCodeCache::CodeCacheType::kWebAssembly;
    default:
      NOTREACHED();
  }
}

// Returns null where there is no usable context key and caching should not be
// used. Returns the key to an isolated cache for locked processes and the key
// to a shared cache for unlocked processes when under partial site isolation.
// `secondary_key` should come from `GetSecondaryKeyForCodeCache` to make sure
// it conforms with the security checks.
std::optional<std::string> GetContextKeyForPersistentCacheCollection(
    const GURL& secondary_key,
    const net::NetworkIsolationKey& nik,
    blink::mojom::CodeCacheType cache_type) {
  std::string context_key = GeneratedCodeCache::GetContextKey(
      secondary_key, nik, MojoCacheTypeToCodeCacheType(cache_type));

  // Here `context_key` will contain a value for locked processes and an empty
  // string for unlocked ones. When sites are isolated per process an empty
  // context key means no access to the cache.
  if (context_key.empty() &&
      content::SiteIsolationPolicy::IsSitePerProcessOrStricter()) {
    return std::nullopt;
  }

  // Alternatively, Android uses partial Site Isolation (i.e., some sites
  // require dedicated processes and others do not).
  //
  // An empty string is not a valid context key for PersistentCacheCollection so
  // a shared context key is used instead. This lets all unlocked processes
  // share a context (and thus a cache) like is achieved when using
  // GeneratedCodeCache through the implementation of `GetCacheKey()` which will
  // construct the full cache key using only the resource URL for requests from
  // unlocked processes.
  //
  // The context key returned by this function needs to enforce the "jail" and
  // "citadel" concepts (see:
  // https://chromium.googlesource.com/chromium/src/+/main/docs/process_model_and_site_isolation.md)
  //
  // 1) Locked processes are "jailed" since they cannot access shared context
  // with their non-empty context key which will never equal
  // `kSharedContextKeyForRelaxedIsolation'.
  // 2) The "citadel" concept is upheld
  // because unlocked processes do not have access to data from locked processes
  // because locked processed store their data using their specific keys and not
  // the shared context key.
  if (context_key.empty() &&
      !content::SiteIsolationPolicy::IsSitePerProcessOrStricter()) {
    return kSharedContextKeyForRelaxedIsolation;
  }

  return context_key;
}

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
    if (!process_lock.IsLockedToSite()) {
      // We can't tell for certain whether this renderer is doing something
      // malicious, but we don't trust it enough to store data.
      return false;
    }
    if (process_lock.MatchesScheme(url::kHttpScheme) ||
        process_lock.MatchesScheme(url::kHttpsScheme)) {
      if (operation == CodeCacheHostImpl::Operation::kWrite) {
        mojo::ReportBadMessage("HTTP(S) pages cannot cache WebUI code");
      }
      return false;
    }
    // Other schemes which might successfully load chrome or chrome-untrusted
    // scripts, such as the PDF viewer, are unsupported but not considered
    // dangerous.
    return process_lock.MatchesScheme(content::kChromeUIScheme) ||
           process_lock.MatchesScheme(content::kChromeUIUntrustedScheme);
  }
  if (resource_url.SchemeIsHTTPOrHTTPS() ||
      blink::CommonSchemeRegistry::IsExtensionScheme(
          resource_url.GetScheme())) {
    if (process_lock.MatchesScheme(content::kChromeUIScheme) ||
        process_lock.MatchesScheme(content::kChromeUIUntrustedScheme)) {
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
      document_isolation_policy, mojo::NullRemote(),
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
             blink::mojom::CacheStorage::OpenResult result) {
            if (!result.has_value()) {
              // Silently ignore errors.
              return;
            }

            mojo::AssociatedRemote<blink::mojom::CacheStorageCache> remote;
            remote.Bind(std::move(result.value()));
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

bool CodeCacheHostImpl::IsPersistentCacheForCodeCacheEnabled(
    blink::mojom::CodeCacheType cache_type) {
  // Serve non-js from existing cache implementation.
  // TODO(crbug.com/377475540): Use another PersistentCacheCollection for
  // WASM.
  if (cache_type != blink::mojom::CodeCacheType::kJavascript) {
    return false;
  }

  ProcessLock process_lock =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
          render_process_id_);

  // Serve ChromeUI from existing cache implementation.
  // TODO(crbug.com/377475540): Use another PersistentCacheCollection for
  // ChromeUI.
  if (process_lock.MatchesScheme(content::kChromeUIScheme) ||
      process_lock.MatchesScheme(content::kChromeUIUntrustedScheme)) {
    return false;
  }

  return blink::features::IsPersistentCacheForCodeCacheEnabled();
}

void CodeCacheHostImpl::DidGenerateCacheableMetadata(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url,
    base::Time expected_response_time,
    mojo_base::BigBuffer data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<GURL> secondary_key =
      GetSecondaryKeyForCodeCache(url, render_process_id_, Operation::kWrite);
  if (!secondary_key) {
    return;
  }

  if (IsPersistentCacheForCodeCacheEnabled(cache_type)) {
    if (!generated_code_cache_context_) {
      return;
    }

    std::string resource_key = GeneratedCodeCache::GetResourceKey(
        url, MojoCacheTypeToCodeCacheType(cache_type));
    std::optional<std::string> context_key =
        GetContextKeyForPersistentCacheCollection(
            secondary_key.value(), network_isolation_key_, cache_type);

    // An empty context key here means the isolation requirements for caching
    // are not met (see `GetContextKeyForPersistentCacheCollection()` for
    // details). In this case, we intentionally do not use the cache.
    if (context_key.has_value()) {
      generated_code_cache_context_->InsertIntoPersistentCacheCollection(
          context_key.value(), resource_key, std::move(data),
          persistent_cache::EntryMetadata{
              .input_signature =
                  expected_response_time.ToDeltaSinceWindowsEpoch()
                      .InMicroseconds()});
    }
  } else {
    GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
    if (!code_cache) {
      return;
    }

    code_cache->WriteEntry(url, *secondary_key, network_isolation_key_,
                           expected_response_time, std::move(data));
  }
}

void CodeCacheHostImpl::FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                                        const GURL& url,
                                        FetchCachedCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<GURL> secondary_key =
      GetSecondaryKeyForCodeCache(url, render_process_id_, Operation::kRead);
  if (!secondary_key) {
    std::move(callback).Run(base::Time(), {});
    return;
  }

  if (IsPersistentCacheForCodeCacheEnabled(cache_type)) {
    if (!generated_code_cache_context_) {
      std::move(callback).Run(base::Time(), {});
      return;
    }

    std::string resource_key = GeneratedCodeCache::GetResourceKey(
        url, MojoCacheTypeToCodeCacheType(cache_type));
    std::optional<std::string> context_key =
        GetContextKeyForPersistentCacheCollection(
            secondary_key.value(), network_isolation_key_, cache_type);

    // An empty context key here means the isolation requirements for caching
    // are not met (see `GetContextKeyForPersistentCacheCollection()` for
    // details). In this case, we intentionally do not use the cache.
    if (!context_key.has_value()) {
      std::move(callback).Run(base::Time(), mojo_base::BigBuffer());
      return;
    }

    std::unique_ptr<persistent_cache::Entry> entry =
        generated_code_cache_context_->FindInPersistentCacheCollection(
            context_key.value(), resource_key);

    if (entry && entry->GetContentSize() > 0) {
      std::move(callback).Run(
          base::Time::FromDeltaSinceWindowsEpoch(
              base::Microseconds(entry->GetMetadata().input_signature)),
          mojo_base::BigBuffer(entry->GetContentSpan()));
    } else {
      std::move(callback).Run(base::Time(), mojo_base::BigBuffer());
    }

  } else {
    GeneratedCodeCache* code_cache = GetCodeCache(cache_type);
    if (!code_cache) {
      std::move(callback).Run(base::Time(), {});
      return;
    }

    auto read_callback = base::BindOnce(
        &CodeCacheHostImpl::OnReceiveCachedCode, weak_ptr_factory_.GetWeakPtr(),
        cache_type, base::TimeTicks::Now(), std::move(callback));
    code_cache->FetchEntry(url, *secondary_key, network_isolation_key_,
                           std::move(read_callback));
  }
}

void CodeCacheHostImpl::ClearCodeCacheEntry(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note:
  // There is no handling under `IsPersistentCacheForCodeCacheEnabled()`
  // here as `PersistentCache` does not expose the ability to delete specific
  // entries. This will lead to entries that are known to be unusable by
  // renderers remaining in the cache. This does not lead to keys being
  // unusable forever since the entries can get overwritten by valid entries.
  // Additionally this does not lead to invalid values being used by renderers
  // since the fact that they are unusable was detected by the clients
  // themselves.
  if (IsPersistentCacheForCodeCacheEnabled(cache_type)) {
    return;
  }

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
  if (process_lock.MatchesScheme(content::kChromeUIScheme) ||
      process_lock.MatchesScheme(content::kChromeUIUntrustedScheme)) {
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

  if (data.size() > 0) {
    base::UmaHistogramCustomCounts("SiteIsolatedCodeCache.DataSize",
                                   data.size(), 1, 10000000, 100);
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
  if (!process_lock.IsLockedToSite()) {
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
  if (process_lock.MatchesScheme(url::kHttpScheme) ||
      process_lock.MatchesScheme(url::kHttpsScheme) ||
      process_lock.MatchesScheme(content::kChromeUIScheme) ||
      process_lock.MatchesScheme(content::kChromeUIUntrustedScheme) ||
      blink::CommonSchemeRegistry::IsExtensionScheme(
          process_lock.GetProcessLockURL().GetScheme())) {
    return process_lock.GetProcessLockURL();
  }

  return std::nullopt;
}

}  // namespace content
