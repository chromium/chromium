// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/code_cache_fetcher.h"

#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

bool ShouldUseIsolatedCodeCache(
    const network::mojom::URLResponseHead& response_head,
    const KURL& initial_url,
    const KURL& current_url,
    base::Time code_cache_response_time) {
  // We only support code cache for other service worker provided
  // resources when a direct pass-through fetch handler is used. If the service
  // worker synthesizes a new Response or provides a Response fetched from a
  // different URL, then do not use the code cache.
  // Also, responses coming from cache storage use a separate code cache
  // mechanism.
  if (response_head.was_fetched_via_service_worker) {
    // Do the same check as !ResourceResponse::IsServiceWorkerPassThrough().
    if (!response_head.cache_storage_cache_name.empty()) {
      // Responses was produced by cache_storage
      return false;
    }
    if (response_head.url_list_via_service_worker.empty()) {
      // Response was synthetically constructed.
      return false;
    }
    if (KURL(response_head.url_list_via_service_worker.back()) != current_url) {
      // Response was fetched from different URLs.
      return false;
    }
  }
  if (SchemeRegistry::SchemeSupportsCodeCacheWithHashing(
          initial_url.Protocol())) {
    // This resource should use a source text hash rather than a response time
    // comparison.
    if (!SchemeRegistry::SchemeSupportsCodeCacheWithHashing(
            current_url.Protocol())) {
      // This kind of Resource doesn't support requiring a hash, so we can't
      // send cached code to it.
      return false;
    }
    if (!Platform::Current()->ShouldUseCodeCacheWithHashing(
            WebURL(current_url))) {
      // Do not send cached code if opted-out by the embedder.
      return false;
    }
  } else if (!response_head.should_use_source_hash_for_js_code_cache) {
    // If the timestamps don't match or are null, the code cache data may be
    // for a different response. See https://crbug.com/1099587.
    if (code_cache_response_time.is_null() ||
        response_head.response_time.is_null() ||
        code_cache_response_time != response_head.response_time) {
      return false;
    }
  }
  return true;
}

bool ShouldFetchCodeCache(const network::ResourceRequest& request) {
  // Since code cache requests use a per-frame interface, don't fetch cached
  // code for keep-alive requests. These are only used for beaconing and we
  // don't expect code cache to help there.
  if (request.keepalive) {
    return false;
  }

  // Aside from http and https, the only other supported protocols are those
  // listed in the SchemeRegistry as requiring a content equality check. Do not
  // fetch cached code if opted-out by the embedder.
  bool should_use_source_hash =
      SchemeRegistry::SchemeSupportsCodeCacheWithHashing(
          String(request.url.scheme())) &&
      Platform::Current()->ShouldUseCodeCacheWithHashing(
          WebURL(KURL(request.url)));
  if (!request.url.SchemeIsHTTPOrHTTPS() && !should_use_source_hash) {
    return false;
  }

  // Supports script resource requests and shared storage worklet module
  // requests.
  // TODO(crbug.com/964467): Currently Chrome doesn't support code cache for
  // dedicated worker, shared worker, audio worklet and paint worklet. For
  // the service worker scripts, Blink receives the code cache via
  // URLLoaderClient::OnReceiveResponse() IPC.
  if (request.destination == network::mojom::RequestDestination::kScript ||
      request.destination ==
          network::mojom::RequestDestination::kSharedStorageWorklet) {
    return true;
  }

  // WebAssembly module request have RequestDestination::kEmpty. Note that
  // we always perform a code fetch for all of these requests because:
  //
  // * It is not easy to distinguish WebAssembly modules from other kEmpty
  //   requests
  // * The fetch might be handled by Service Workers, but we can't still know
  //   if the response comes from the CacheStorage (in such cases its own
  //   code cache will be used) or not.
  //
  // These fetches should be cheap, however, requiring one additional IPC and
  // no browser process disk IO since the cache index is in memory and the
  // resource key should not be present.
  //
  // The only case where it's easy to skip a kEmpty request is when a content
  // equality check is required, because only ScriptResource supports that
  // requirement.
  if (request.destination == network::mojom::RequestDestination::kEmpty) {
    return true;
  }
  return false;
}

mojom::blink::CodeCacheType GetCodeCacheType(
    network::mojom::RequestDestination destination) {
  if (destination == network::mojom::RequestDestination::kEmpty) {
    // For requests initiated by the fetch function, we use code cache for
    // WASM compiled code.
    return mojom::blink::CodeCacheType::kWebAssembly;
  } else {
    // Otherwise, we use code cache for scripting.
    return mojom::blink::CodeCacheType::kJavascript;
  }
}

}  // namespace

// static
scoped_refptr<CodeCacheFetcher> CodeCacheFetcher::TryCreateAndStart(
    const network::ResourceRequest& request,
    CodeCacheHost& code_cache_host,
    base::OnceClosure done_closure) {
  if (!ShouldFetchCodeCache(request)) {
    return nullptr;
  }
  auto fetcher = base::MakeRefCounted<CodeCacheFetcher>(
      code_cache_host, GetCodeCacheType(request.destination), KURL(request.url),
      std::move(done_closure));
  fetcher->Start();
  return fetcher;
}

CodeCacheFetcher::CodeCacheFetcher(CodeCacheHost& code_cache_host,
                                   mojom::blink::CodeCacheType code_cache_type,
                                   const KURL& url,
                                   base::OnceClosure done_closure)
    : code_cache_host_(code_cache_host.GetWeakPtr()),
      code_cache_type_(code_cache_type),
      initial_url_(url),
      current_url_(url),
      done_closure_(std::move(done_closure)) {}

void CodeCacheFetcher::Start() {
  CHECK(code_cache_host_);
  (*code_cache_host_)
      ->FetchCachedCode(code_cache_type_, initial_url_,
                        WTF::BindOnce(&CodeCacheFetcher::DidReceiveCachedCode,
                                      base::WrapRefCounted(this)));
}

void CodeCacheFetcher::DidReceiveCachedMetadataFromUrlLoader() {
  did_receive_cached_metadata_from_url_loader_ = true;
  if (!is_waiting_) {
    ClearCodeCacheEntryIfPresent();
  }
}

std::optional<mojo_base::BigBuffer> CodeCacheFetcher::TakeCodeCacheForResponse(
    const network::mojom::URLResponseHead& response_head) {
  CHECK(!is_waiting_);
  if (!ShouldUseIsolatedCodeCache(response_head, initial_url_, current_url_,
                                  code_cache_response_time_)) {
    ClearCodeCacheEntryIfPresent();
    return std::nullopt;
  }
  return std::move(code_cache_data_);
}

void CodeCacheFetcher::DidReceiveCachedCode(base::Time response_time,
                                            mojo_base::BigBuffer data) {
  is_waiting_ = false;
  code_cache_data_ = std::move(data);
  if (did_receive_cached_metadata_from_url_loader_) {
    ClearCodeCacheEntryIfPresent();
    return;
  }
  code_cache_response_time_ = response_time;
  std::move(done_closure_).Run();
}

void CodeCacheFetcher::ClearCodeCacheEntryIfPresent() {
  if (code_cache_host_ && code_cache_data_ && (code_cache_data_->size() > 0)) {
    (*code_cache_host_)->ClearCodeCacheEntry(code_cache_type_, initial_url_);
  }
  code_cache_data_.reset();
}

}  // namespace blink
