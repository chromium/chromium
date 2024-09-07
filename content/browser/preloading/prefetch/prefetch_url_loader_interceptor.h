// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {

class BrowserContext;
class PrefetchContainer;
class PrefetchMatchResolver;

using PrefetchCompleteCallbackForTesting =
    base::RepeatingCallback<void(PrefetchContainer*)>;

// Intercepts navigations that can use prefetched resources.
class CONTENT_EXPORT PrefetchURLLoaderInterceptor final
    : public NavigationLoaderInterceptor {
 public:
  PrefetchURLLoaderInterceptor(
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
  // `tentative_resource_request`. The `PrefetchContainer` is first obtained
  // from `PrefetchService` and then goes through other checks in
  // `PrefetchUrlLoaderHelper`.
  // The |get_prefetch_callback| is called with this associated prefetch.

  // TODO(crbug.com/40274818): It might be better to store
  // PrefetchMatchResolver as part of PrefetchUrlLoaderInterceptor
  // as this is related to serving a navigation. It would simplify GetPrefetch
  // call.
  void GetPrefetch(const network::ResourceRequest& tentative_resource_request,
                   PrefetchMatchResolver& potential_prefetch_matches_container,
                   base::OnceCallback<void(PrefetchContainer::Reader)>
                       get_prefetch_callback) const;

  void OnGetPrefetchComplete(PrefetchContainer::Reader reader);

  // The frame tree node |this| is associated with, used to retrieve
  // |PrefetchService|.
  const FrameTreeNodeId frame_tree_node_id_;

  // Corresponds to the ID of "navigable's active document" used for "finding a
  // matching prefetch record" in the spec. This is used as a part of
  // `PrefetchContainer::Key` to make prefetches per-Document.
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
  PrefetchContainer::Reader redirect_reader_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchURLLoaderInterceptor> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
