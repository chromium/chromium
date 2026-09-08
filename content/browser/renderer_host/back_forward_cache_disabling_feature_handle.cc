// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_disabling_feature_handle.h"

#include "base/check.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

BackForwardCacheDisablingFeatureHandle::
    BackForwardCacheDisablingFeatureHandle() = default;

BackForwardCacheDisablingFeatureHandle::BackForwardCacheDisablingFeatureHandle(
    BackForwardCacheDisablingFeatureHandle&& other) = default;

BackForwardCacheDisablingFeatureHandle&
BackForwardCacheDisablingFeatureHandle::operator=(
    BackForwardCacheDisablingFeatureHandle&& other) = default;

BackForwardCacheDisablingFeatureHandle::BackForwardCacheDisablingFeatureHandle(
    RenderFrameHostImpl* render_frame_host,
    blink::scheduler::WebSchedulerTrackedFeature feature)
    : render_frame_host_(render_frame_host->GetWeakPtr()), feature_(feature) {
  CHECK(render_frame_host_);
  render_frame_host_->OnBackForwardCacheDisablingFeatureUsed(feature_);
}

BackForwardCacheDisablingFeatureHandle::
    ~BackForwardCacheDisablingFeatureHandle() {
  Reset();
}

bool BackForwardCacheDisablingFeatureHandle::IsValid() const {
  return render_frame_host_.get();
}

void BackForwardCacheDisablingFeatureHandle::Reset() {
  if (render_frame_host_) {
    render_frame_host_->OnBackForwardCacheDisablingFeatureRemoved(feature_);
  }
  render_frame_host_ = nullptr;
}

}  // namespace content
