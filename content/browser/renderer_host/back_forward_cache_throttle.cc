// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_throttle.h"

#include "base/barrier_closure.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// static
std::unique_ptr<BackForwardCacheThrottle>
BackForwardCacheThrottle::MaybeCreateThrottleFor(
    NavigationRequest* navigation_request) {
  if (navigation_request->IsServedFromBackForwardCache()) {
    return base::WrapUnique(new BackForwardCacheThrottle(navigation_request));
  }
  return nullptr;
}

BackForwardCacheThrottle::BackForwardCacheThrottle(
    NavigationRequest* navigation_request)
    : NavigationThrottle(navigation_request) {
  DCHECK(navigation_request->IsServedFromBackForwardCache());
}

BackForwardCacheThrottle::~BackForwardCacheThrottle() {
  RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(main_rfh_id_);
  // RFH could have been deleted. E.g. eviction timer fired
  if (rfh && rfh->IsInBackForwardCache()) {
    // rfh is still in the cache so the navigation must have failed. But we have
    // already disabled eviction so the safest thing to do here to recover is to
    // evict.
    rfh->EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::
            kNavigationCancelledWhileRestoring);
  }
}

NavigationThrottle::ThrottleCheckResult
BackForwardCacheThrottle::WillStartRequest() {
  NavigationRequest* navigation_request =
      NavigationRequest::From(navigation_handle());
  BackForwardCacheImpl::Entry* bfcache_entry =
      navigation_request->frame_tree_node()
          ->navigator()
          .controller()
          .GetBackForwardCache()
          .GetEntry(navigation_request->nav_entry_id());

  if (!bfcache_entry) {
    // Go on for now. NavigationRequest will later restart the navigation
    // without bfcache.
    return PROCEED;
  }

  main_rfh_id_ = bfcache_entry->render_frame_host->GetGlobalFrameRoutingId();

  auto cb = base::BarrierClosure(
      bfcache_entry->render_view_hosts.size(),
      base::BindOnce(&BackForwardCacheThrottle::OnAckReceived,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
  for (auto* rvh : bfcache_entry->render_view_hosts) {
    rvh->PrepareToLeaveBackForwardCache(cb);
  }
  return DEFER;
}

const char* BackForwardCacheThrottle::GetNameForLogging() {
  return "BackForwardCacheThrottle";
}

void BackForwardCacheThrottle::OnAckReceived(base::TimeTicks ipc_start_time) {
  base::UmaHistogramTimes("BackForwardCache.Restore.DisableEvictionDelay",
                          base::TimeTicks::Now() - ipc_start_time);
  Resume();
}

}  // namespace content
