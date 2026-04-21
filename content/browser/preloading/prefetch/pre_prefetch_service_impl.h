// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_SERVICE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/pre_prefetch_handle.h"
#include "content/public/browser/pre_prefetch_service.h"
#include "content/public/browser/prefetch_priority.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "content/public/browser/prefetch_update_headers_params.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

class PrefetchRequest;
class PrePrefetchServiceCore;

// The subset of `PrefetchRequest` members that `PrePrefetchService` has to
// distinguish when comparing the pre-calculated headers. There are other
// `PrefetchRequest` members that can affect header construction generally
// (e.g., `browser_context`, `prefetch_type`, `referring_web_contents`), but
// they are not included here since they are fixed values among the requests
// handled by this `PrePrefetchService. `additional_headers` are not included in
// the pre-calculation (see `MakeInitialResourceRequestForPrePrefetch()`), so it
// is also omitted here.
struct PrePrefetchPreCalculatedHeadersKey {
  url::Origin origin;
  bool javascript_enabled = false;
  bool should_append_variations_header = true;

  bool operator<(const PrePrefetchPreCalculatedHeadersKey& other) const {
    return std::tie(origin, javascript_enabled,
                    should_append_variations_header) <
           std::tie(other.origin, other.javascript_enabled,
                    other.should_append_variations_header);
  }
};

// Responsible for starting PrePrefetches based on an associated
// `BrowserContext` given via ctor.
//
// Thread model:
//
// `PrePrefetchServiceImpl` should be instantiated and destructed on the UI
// thread. Other functions are expected to be called from non UI thread, and are
// thread-safe. It delegates actual operations to **`PrePrefetchServiceCore`
// bound sequence** running on a dedicated `SequencedTaskRunner` to ensure
// sequential accesses.
// WARNING: Currently it is `PrePrefetchServiceImpl` owner's responsibility to
// ensure that the dtor on UI thread and non-main thread calls cannot happen
// simultaneously, otherwise this will lead a data race for `core_`.
class CONTENT_EXPORT PrePrefetchServiceImpl : public PrePrefetchService {
 public:
  PrePrefetchServiceImpl(
      BrowserContext* browser_context,
      std::vector<PrePrefetchUpdateHeadersCallback>
          embedder_non_ui_thread_update_headers_callbacks,
      std::optional<url::Origin> initial_origin_hint,
      std::optional<bool> initial_javascript_enabled_hint,
      std::optional<bool> initial_should_append_variations_header_hint);
  ~PrePrefetchServiceImpl() override;

  // Starts PrePrefetch for the given `url`, for embedder triggers.
  // Expected to be called from a non-UI thread (On UI thread, we can simply
  // start Prefetch instead).
  // Returns `PrePrefetchHandle` synchronously by blocking on the sequenced task
  // runner's execution.
  [[nodiscard]] std::unique_ptr<PrePrefetchHandle> StartPrePrefetchRequest(
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
      bool should_bypass_http_cache) override;

  [[nodiscard]] std::unique_ptr<PrePrefetchHandle>
  StartPrePrefetchRequestForTesting(
      std::unique_ptr<const PrefetchRequest> prefetch_request);

  // Sets the URLLoaderFactory for testing. The caller must keep the ownership
  // of the factory during the test.
  static void SetURLLoaderFactoryForTesting(
      network::SharedURLLoaderFactory* url_loader_factory);

 private:
  [[nodiscard]] std::unique_ptr<PrePrefetchHandle>
  StartPrePrefetchRequestInternal(
      std::unique_ptr<const PrefetchRequest> prefetch_request);

  PrefetchUpdateHeadersParams PreCalculatePrePrefetchHeadersOnUI(
      BrowserContext* browser_context,
      const PrePrefetchPreCalculatedHeadersKey& key) const;

  // This is UI-thread bound, and must not be dereferenced during this
  // `PrePrefetchServiceCore` sequence.
  base::WeakPtr<BrowserContext> browser_context_weak_on_ui_thread_;

  scoped_refptr<base::SequencedTaskRunner> core_task_runner_;
  base::SequenceBound<PrePrefetchServiceCore> core_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_SERVICE_IMPL_H_
