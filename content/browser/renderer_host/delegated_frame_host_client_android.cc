// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host_client_android.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"

namespace content {

DelegatedFrameHostClientAndroid::DelegatedFrameHostClientAndroid(
    RenderWidgetHostViewAndroid* render_widget_host_view)
    : render_widget_host_view_(render_widget_host_view) {}

DelegatedFrameHostClientAndroid::~DelegatedFrameHostClientAndroid() {}

void DelegatedFrameHostClientAndroid::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  render_widget_host_view_->OnFrameTokenChangedForView(frame_token,
                                                       activation_time);
}

void DelegatedFrameHostClientAndroid::WasEvicted() {
  render_widget_host_view_->WasEvicted();
}

void DelegatedFrameHostClientAndroid::OnSurfaceIdChanged() {
  render_widget_host_view_->OnSurfaceIdChanged();
}

std::vector<viz::SurfaceId>
DelegatedFrameHostClientAndroid::CollectSurfaceIdsForEviction() const {
  return render_widget_host_view_->host()->CollectSurfaceIdsForEviction();
}

}  // namespace content
