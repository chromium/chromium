// Copyright 2018 The Chromium Authors. All rights reserved.
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

void DelegatedFrameHostClientAndroid::SetBeginFrameSource(
    viz::BeginFrameSource* begin_frame_source) {
  render_widget_host_view_->SetBeginFrameSource(begin_frame_source);
}

void DelegatedFrameHostClientAndroid::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  render_widget_host_view_->DidReceiveCompositorFrameAck(resources);
}

void DelegatedFrameHostClientAndroid::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {
  render_widget_host_view_->DidPresentCompositorFrame(presentation_token,
                                                      feedback);
}

void DelegatedFrameHostClientAndroid::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  render_widget_host_view_->ReclaimResources(resources);
}

void DelegatedFrameHostClientAndroid::OnFrameTokenChanged(
    uint32_t frame_token) {
  render_widget_host_view_->OnFrameTokenChangedForView(frame_token);
}

void DelegatedFrameHostClientAndroid::WasEvicted() {
  render_widget_host_view_->WasEvicted();
}

}  // namespace content
