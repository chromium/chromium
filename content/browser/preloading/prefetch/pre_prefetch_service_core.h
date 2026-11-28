// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_SERVICE_CORE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_SERVICE_CORE_H_

#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "content/public/browser/prefetch_headers_update_params.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class PrefetchRequest;
class PrePrefetchHandle;
class PrePrefetchServiceImpl;

// The subset of `PrefetchRequest` members that `PrePrefetchService` has to
// distinguish when comparing the pre-calculated headers. There are other
// `PrefetchRequest` members that can affect header construction generally
// (e.g., `browser_context`, `prefetch_type`, `referring_web_contents`), but
// they are not included here since they are fixed values among the requests
// handled by this `PrePrefetchService`. `additional_headers` are not included
// in the pre-calculation (see `MakeInitialResourceRequestForPrePrefetch()`), so
// it is also omitted here.
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PrePrefetchStartResult)
enum class PrePrefetchStartResult {
  // PrePrefetch has been successfully started.
  kStarted = 0,
  // PrePrefetch failed because the `URLLoaderFactory` was disconnected.
  kFailedURLLoaderFactoryDisconnected = 1,
  // PrePrefetch failed because the requested origin did not match the
  // pre-calculated headers cache.
  kFailedPreCalculatedHeadersNotMatched = 2,
  kMaxValue = kFailedPreCalculatedHeadersNotMatched,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrePrefetchStartResult)

// Pre-calculates the UI-thread-dependent PrePrefetch request headers for
// `key`.
network::HttpRequestHeadersUpdateParams
PreCalculatePrePrefetchHeadersOnUIThread(
    BrowserContext* browser_context,
    const PrePrefetchPreCalculatedHeadersKey& key);

// The internal class owned by `PrePrefetchServiceImpl` to run the substantial
// tasks of it on `core_task_runner_` (`SequencedTaskRunner`) ensuring
// sequential accesses. Please see `PrePrefetchServiceImpl` class comment for
// more details.
class PrePrefetchServiceCore {
 public:
  PrePrefetchServiceCore(
      base::WeakPtr<BrowserContext> browser_context,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory,
      std::map<PrePrefetchPreCalculatedHeadersKey,
               network::HttpRequestHeadersUpdateParams>
          ui_thread_pre_calculated_headers_map,
      std::vector<PrePrefetchUpdateHeadersCallback>
          non_ui_thread_update_headers_callbacks);
  ~PrePrefetchServiceCore();

  void StartPrePrefetchRequest(
      base::PassKey<PrePrefetchServiceImpl> pass_key,
      std::unique_ptr<const PrefetchRequest> prefetch_request,
      std::unique_ptr<PrePrefetchHandle>* out_handle,
      base::ScopedClosureRunner on_done_runner);

  // Prohibits `URLLoaderFactory` refresh for testing.
  static void SetShouldProhibitURLLoaderFactoryRefreshForTesting(
      bool should_prohibit);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void PostGetNewURLLoaderFactoryToUIThread();

  void UpdateURLLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory);

  void PostPreCalculateHeadersToUIThread(
      const PrePrefetchPreCalculatedHeadersKey& key);

  void UpdatePreCalculatedHeaders(
      PrePrefetchPreCalculatedHeadersKey key,
      std::optional<network::HttpRequestHeadersUpdateParams>
          headers_update_params);

  base::WeakPtr<BrowserContext> browser_context_weak_on_ui_thread_;
  mojo::Remote<network::mojom::URLLoaderFactory> factory_;

  // Pre-calculated UI-thread headers per
  // `PrePrefetchPreCalculatedHeadersKey`,
  // which is utilized for saving a thread hop to the UI thread if the upcoming
  // PrePrefetch hits this.
  // TODO(crbug.com/452389538): Consider how to refresh these.
  std::map<PrePrefetchPreCalculatedHeadersKey,
           network::HttpRequestHeadersUpdateParams>
      ui_thread_pre_calculated_headers_map_;

  // Tracks in-flight header pre-calculation requests to avoid posting duplicate
  // UI tasks for the exact same key parameters.
  base::flat_set<PrePrefetchPreCalculatedHeadersKey>
      pending_pre_calculate_headers_keys_;

  // Callbacks that can be called on `PrePrefetchContainer::Start()`
  // (`PrePrefetchServiceCore` sequence) to modify the PrePrefetch initial
  // request headers. Therefore, for example, this can conceptually emulate
  // header modifications performed in
  // `ContentBrowserClient::WillCreateURLLoaderFactory`.
  std::vector<PrePrefetchUpdateHeadersCallback>
      non_ui_thread_update_headers_callbacks_;

  base::WeakPtrFactory<PrePrefetchServiceCore> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PRE_PREFETCH_SERVICE_CORE_H_
