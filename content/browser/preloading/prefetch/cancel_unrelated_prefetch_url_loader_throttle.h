// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_CANCEL_UNRELATED_PREFETCH_URL_LOADER_THROTTLE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_CANCEL_UNRELATED_PREFETCH_URL_LOADER_THROTTLE_H_

#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

// Cancels unrelated prefetches to prevent network congestion.
//
// Conditions:
//
// - The loading is for outermost main frame navigation.
// - The navigaiton is non prerender (as prerender is background work).
// - A prefetch is not servable yet. (Note that it implies the navigation is not
// using the prefetch. See
// `PrefetchService::CancelUnrelatedPrefetchForNavigation()`.)
//
// This is a hook for navigation. Why this is `URLLoaderThrottle` rather than
// `NavigationThrottle`?:
//
// - `NavigationRequest::WillStartRequest()` is too early. Prefetch matching
//   follows it.
// - Other hooks are too late.
// - Another option was doing something like prefetch matching at, e.g., the
//   head of `NavigationRequest`, rather than utilizing existing prefetch
//   matching. But we rejected it to keep
//   `PrefetchURLLoaderInterceptor`/`PrefetchMatchResolver` "single source of
//   truth" of prefetch matching, because a variant and differences bring
//   problems.
class CONTENT_EXPORT CancelUnrelatedPrefetchURLLoaderThrottle final
    : public blink::URLLoaderThrottle {
 public:
  explicit CancelUnrelatedPrefetchURLLoaderThrottle(
      FrameTreeNodeId frame_tree_node_id);
  ~CancelUnrelatedPrefetchURLLoaderThrottle() override = default;

  static std::unique_ptr<CancelUnrelatedPrefetchURLLoaderThrottle> MaybeCreate(
      FrameTreeNodeId frame_tree_node_id);

  // `URLLoaderThrottle` implementation
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

 private:
  const FrameTreeNodeId frame_tree_node_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_CANCEL_UNRELATED_PREFETCH_URL_LOADER_THROTTLE_H_
