// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

class PrefetchContainer;
class PrefetchServingPageMetricsContainer;
class ServiceWorkerMainResourceHandle;

using PrefetchCompleteCallbackForTesting =
    base::RepeatingCallback<void(PrefetchContainer*)>;

// Intercepts navigations that can use prefetched resources.
class CONTENT_EXPORT PrefetchURLLoaderInterceptor final
    : public NavigationLoaderInterceptor {
 public:
  PrefetchURLLoaderInterceptor(
      PrefetchServiceWorkerState expected_service_worker_state,
      base::WeakPtr<ServiceWorkerMainResourceHandle>
          service_worker_handle_for_navigation,
      FrameTreeNodeId frame_tree_node_id,
      std::optional<blink::DocumentToken> initiator_document_token,
      base::WeakPtr<PrefetchServingPageMetricsContainer>
          serving_page_metrics_container);
  ~PrefetchURLLoaderInterceptor() override;

  PrefetchURLLoaderInterceptor(const PrefetchURLLoaderInterceptor&) = delete;
  PrefetchURLLoaderInterceptor& operator=(const PrefetchURLLoaderInterceptor&) =
      delete;

  // NavigationLoaderInterceptor
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      NavigationLoaderInterceptor::LoaderCallback callback,
      NavigationLoaderInterceptor::FallbackCallback fallback_callback) override;

  // Sets a callback to be called on |OnGetPrefetchComplete| to inform whether
  // the prefetch is served, used only for test purpose.
  static void SetPrefetchCompleteCallbackForTesting(
      PrefetchCompleteCallbackForTesting callback);

 protected:
  FrameTreeNodeId GetFrameTreeNodeId() const { return frame_tree_node_id_; }

 private:
  // Gets the `PrefetchContainer` (if any) to be used for
  // `url`. The `PrefetchContainer` is first obtained
  // from `PrefetchService` and then goes through other checks in
  // `PrefetchUrlLoaderHelper`.
  // The |get_prefetch_callback| is called with this associated prefetch.
  void GetPrefetch(const GURL& url,
                   base::OnceCallback<void(PrefetchServingHandle)>
                       get_prefetch_callback) const;

  void OnGetPrefetchComplete(const GURL& url,
                             const std::optional<url::Origin>& top_frame_origin,
                             PrefetchServingHandle serving_handle);

  // Matches prefetches only if its final PrefetchServiceWorkerState is
  // `expected_service_worker_state_`, either `kControlled` or `kDisallowed`.
  const PrefetchServiceWorkerState expected_service_worker_state_;

  // `ServiceWorkerMainResourceHandle` used for the navigation to be intercepted
  // (i.e. NOT the handle used for prefetch). This is used only for the
  // `kControlled` case and can be null for `kDisallowed` case.
  const base::WeakPtr<ServiceWorkerMainResourceHandle>
      service_worker_handle_for_navigation_;

  // The frame tree node |this| is associated with, used to retrieve
  // |PrefetchService|.
  const FrameTreeNodeId frame_tree_node_id_;

  // Corresponds to the ID of "navigable's active document" used for "finding a
  // matching prefetch record" in the spec. This is used as a part of
  // `PrefetchKey` to make prefetches per-Document.
  // https://wicg.github.io/nav-speculation/prefetch.html
  const std::optional<blink::DocumentToken> initiator_document_token_;

  // The `PrefetchServingPageMetricsContainer` associated with the current
  // navigation and to be set to the selected `PrefetchContainer` if any.
  base::WeakPtr<PrefetchServingPageMetricsContainer>
      serving_page_metrics_container_;

  // Called once |this| has decided whether to intercept or not intercept the
  // navigation.
  NavigationLoaderInterceptor::LoaderCallback loader_callback_;

  // The prefetch container that has already been used to serve a redirect. If
  // another request can be intercepted, this will be checked first to see if
  // its next redirect hop matches the request URL.
  PrefetchServingHandle redirect_serving_handle_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchURLLoaderInterceptor> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
