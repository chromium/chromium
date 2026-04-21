// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRE_PREFETCH_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PRE_PREFETCH_SERVICE_H_

#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/pre_prefetch_handle.h"
#include "content/public/browser/prefetch_priority.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "content/public/browser/prefetch_update_headers_params.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {

// Responsible for starting PrePrefetches based on an associated
// `BrowserContext` given via ctor.

// `PrePrefetchService` should be instantiated on the UI thread.
// Other functions are expected to be called from a non-UI thread, and are
// thread-safe. Please see the implementation for more details about how the
// thread model is constructed.
class CONTENT_EXPORT PrePrefetchService {
 public:
  virtual ~PrePrefetchService() = default;

  // Creates `PrePrefetchService` with `BrowserContext`.
  // Accepts embedder callbacks to modify the PrePrefetch initial request
  // headers. Therefore, for example, they can conceptually emulate header
  // modifications performed in
  // `ContentBrowserClient::WillCreateURLLoaderFactory`. Currently we accept
  // `embedder_non_ui_thread_update_headers_callbacks`, which are expected to be
  // run on PrePrefetch non-UI thread/sequence (e.g. `PrePrefetchServiceCore`).
  // It should have no dependency on UI thread, otherwise it can cause a thread
  // hop to the UI thread when called, which nullifies the PrePrefetch benefit.
  // If multiple callbacks are provided, they are run in the order they are
  // provided, and previous `ResourceRequest` update is passed to the next
  // callback.
  // If we need to have UI-dependent embedder callbacks, that should  be
  // considered in a separate way (See
  // `PrePrefetchServiceImpl::PreCalculatePrePrefetchHeadersOnUI()` for
  // example).
  // Callers can also specify hints for the initial PrePrefetch request. These
  // hints are utilized to pre-calculate the UI-thread-dependent parts of the
  // PrePrefetch `ResourceRequest`, which will later be used on a non-UI thread.
  static std::unique_ptr<PrePrefetchService> Create(
      BrowserContext* browser_context,
      std::vector<PrePrefetchUpdateHeadersCallback>
          embedder_non_ui_thread_update_headers_callbacks = {},
      std::optional<url::Origin> initial_origin_hint = std::nullopt,
      std::optional<bool> initial_javascript_enabled_hint = std::nullopt,
      std::optional<bool> initial_should_append_variations_header_hint =
          std::nullopt);

  // Starts PrePrefetch for the given `url`, for embedder triggers.
  // Expected to be called from a non-UI thread. On UI thread, we can simply
  // start Prefetch instead.
  // Returns `PrePrefetchHandle` synchronously.
  [[nodiscard]] virtual std::unique_ptr<PrePrefetchHandle>
  StartPrePrefetchRequest(
      const GURL& url,
      const std::string& embedder_histogram_suffix,
      bool javascript_enabled,
      std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
      std::optional<PrefetchPriority> priority,
      const net::HttpRequestHeaders& additional_headers,
      std::unique_ptr<PrefetchRequestStatusListener> request_status_listener,
      base::TimeDelta ttl,
      bool should_append_variations_header,
      bool should_disable_block_until_head_timeout,
      bool should_bypass_http_cache) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRE_PREFETCH_SERVICE_H_
