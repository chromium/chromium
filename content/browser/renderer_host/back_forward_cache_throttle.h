// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

class NavigationRequest;

// This throttle is used for back-forward-cache navigations. It unhooks the
// Javascript eviction in every renderer process. The navigation is deferred
// until a confirmation is received from each renderer. This ensures no renderer
// will try to evict the document after the browser has activated it (due to
// races in IPCs).
//
// If the back-forward cache entry isn't restored at the end of the navigation,
// it becomes unsuitable for being restored later (as Javascript eviction is now
// disabled) and is thus dropped.
class BackForwardCacheThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<BackForwardCacheThrottle> MaybeCreateThrottleFor(
      NavigationRequest* navigation_request);

  ~BackForwardCacheThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 private:
  explicit BackForwardCacheThrottle(NavigationRequest* navigation_request);

  void OnAckReceived(base::TimeTicks ipc_start_time);

  // ID of the RenderFrameHost that is going to be restored from the
  // back-forward cache. If the navigation finishes and this RenderFrameHost is
  // still in the cache (navigation failed / canceled) we need to evict it, as
  // we unhooked eviction and thus left it in an intermediate state.
  GlobalFrameRoutingId main_rfh_id_;

  base::WeakPtrFactory<BackForwardCacheThrottle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_THROTTLE_H_
