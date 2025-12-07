// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/isolated_code_cache_fetcher.h"

#include "base/metrics/histogram_functions.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// Validates whether metadata fetched from the isolated code cache should be
// taken by the owning resource request.
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

    // When the cached resource is revalidated and an HTTP 304 ("Not Modified")
    // response is received, the response time changes. However, the code cache
    // is still valid. We use original_response_time (which doesn't change when
    // a HTTP 304 is received) instead of response_time for validating the code
    // cache.
    base::Time response_time = response_head.original_response_time.is_null()
                                   ? response_head.response_time
                                   : response_head.original_response_time;
    if (code_cache_response_time.is_null() || response_time.is_null() ||
        code_cache_response_time != response_time) {
      return false;
    }
  }
  return true;
}

}  // namespace

IsolatedCodeCacheFetcher::IsolatedCodeCacheFetcher(
    CodeCacheHost& code_cache_host,
    mojom::blink::CodeCacheType code_cache_type,
    const KURL& url,
    base::OnceClosure done_closure)
    : code_cache_host_(code_cache_host.GetWeakPtr()),
      code_cache_type_(code_cache_type),
      initial_url_(url),
      current_url_(url),
      done_closure_(std::move(done_closure)) {}

IsolatedCodeCacheFetcher::~IsolatedCodeCacheFetcher() = default;

void IsolatedCodeCacheFetcher::DidReceiveCachedMetadataFromUrlLoader() {
  did_receive_cached_metadata_from_url_loader_ = true;
  if (!is_waiting_) {
    ClearCodeCacheEntryIfPresent();
  }
}

std::optional<mojo_base::BigBuffer>
IsolatedCodeCacheFetcher::TakeCodeCacheForResponse(
    const network::mojom::URLResponseHead& response_head) {
  CHECK(!is_waiting_);
  if (!ShouldUseIsolatedCodeCache(response_head, initial_url_, current_url_,
                                  code_cache_response_time_)) {
    ClearCodeCacheEntryIfPresent();
    return std::nullopt;
  }
  return std::move(code_cache_data_);
}

void IsolatedCodeCacheFetcher::OnReceivedRedirect(const KURL& new_url) {
  current_url_ = new_url;
}

bool IsolatedCodeCacheFetcher::IsWaiting() const {
  return is_waiting_;
}

void IsolatedCodeCacheFetcher::Start() {
  CHECK(code_cache_host_);
  time_of_last_fetch_start_ = base::TimeTicks::Now();
  (*code_cache_host_)
      ->FetchCachedCode(
          code_cache_type_, initial_url_,
          blink::BindOnce(&IsolatedCodeCacheFetcher::DidReceiveCachedCode,
                          base::WrapRefCounted(this)));
}

void IsolatedCodeCacheFetcher::DidReceiveCachedCode(base::Time response_time,
                                                    mojo_base::BigBuffer data) {
  if (!time_of_last_fetch_start_.is_null()) {
    base::UmaHistogramTimes("Blink.ResourceRequest.CodeFetchLatency",
                            base::TimeTicks::Now() - time_of_last_fetch_start_);
    time_of_last_fetch_start_ = base::TimeTicks();
  }

  is_waiting_ = false;
  code_cache_data_ = std::move(data);
  if (did_receive_cached_metadata_from_url_loader_) {
    ClearCodeCacheEntryIfPresent();
    return;
  }
  code_cache_response_time_ = response_time;
  std::move(done_closure_).Run();
}

void IsolatedCodeCacheFetcher::ClearCodeCacheEntryIfPresent() {
  if (code_cache_host_ && code_cache_data_ && (code_cache_data_->size() > 0)) {
    (*code_cache_host_)->ClearCodeCacheEntry(code_cache_type_, initial_url_);
  }
  code_cache_data_.reset();
}

}  // namespace blink
