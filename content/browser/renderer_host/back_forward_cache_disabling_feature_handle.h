// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_DISABLING_FEATURE_HANDLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_DISABLING_FEATURE_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

class RenderFrameHostImpl;

// A handle that prevents a RenderFrameHost from entering the BackForwardCache
// while it is alive.
class CONTENT_EXPORT BackForwardCacheDisablingFeatureHandle {
 public:
  BackForwardCacheDisablingFeatureHandle();
  BackForwardCacheDisablingFeatureHandle(
      BackForwardCacheDisablingFeatureHandle&& other);
  BackForwardCacheDisablingFeatureHandle& operator=(
      BackForwardCacheDisablingFeatureHandle&& other);
  ~BackForwardCacheDisablingFeatureHandle();

  BackForwardCacheDisablingFeatureHandle(
      const BackForwardCacheDisablingFeatureHandle&) = delete;
  BackForwardCacheDisablingFeatureHandle& operator=(
      const BackForwardCacheDisablingFeatureHandle&) = delete;

  bool IsValid() const;

  // This will reduce the feature count for `feature_` for the first time, and
  // do nothing for further calls.
  void Reset();

 private:
  friend class RenderFrameHostImpl;
  BackForwardCacheDisablingFeatureHandle(
      RenderFrameHostImpl* render_frame_host,
      blink::scheduler::WebSchedulerTrackedFeature feature);

  base::WeakPtr<RenderFrameHostImpl> render_frame_host_;
  blink::scheduler::WebSchedulerTrackedFeature feature_ =
      blink::scheduler::WebSchedulerTrackedFeature::kDummy;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_DISABLING_FEATURE_HANDLE_H_
