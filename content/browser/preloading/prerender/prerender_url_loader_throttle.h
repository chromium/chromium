// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_URL_LOADER_THROTTLE_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_URL_LOADER_THROTTLE_H_

#include "content/public/browser/frame_tree_node_id.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

// Cancels prerender if the prefetch ahead of prerender "failed".
//
// If `kPrerender2FallbackPrefetchSpecRules` is enabled, `PrerendererImpl`
// starts a prefetch ahead of prerender and a prerender follows it and wait for
// it, to reduce fetch requests. In most cases, failure of the prefetch also
// should cancel the prerender, because the prerender is also expected to fail.
//
// This class provides a cancellation hook to `PrerenderHost`. Judging what
// failure should be propagated is delegated to `PrerenderHost`.
//
// If the prefetch ahead of prerender "failed", `PrerenderURLLoaderThrottle` is
// added to the `ThrottlingURLLoader` for the corresponding prerendering
// navigation, and the `PrerenderURLLoaderThrottle` always cancels the network
// request before it starts, which cancels the prerender.
//
// The behavior is tested by
// //content/browser/preloading/prerenderer_impl_browsertest.cc.
//
// TODO(crbug.com/372186548): Cancel the request in
// `PrefetchURLLoaderInterceptor` and remove `PrerenderURLLoaderThrottle`, once
// other dependencies are finally resolved.
class PrerenderURLLoaderThrottle final : public blink::URLLoaderThrottle {
 public:
  explicit PrerenderURLLoaderThrottle(FrameTreeNodeId frame_tree_node_id);
  ~PrerenderURLLoaderThrottle() override = default;

  static std::unique_ptr<PrerenderURLLoaderThrottle> MaybeCreate(
      FrameTreeNodeId frame_tree_node_id);

  // `URLLoaderThrottle` implementation
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

 private:
  FrameTreeNodeId frame_tree_node_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_URL_LOADER_THROTTLE_H_
