// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host_client_ios.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "ui/compositor/layer.h"

namespace content {

DelegatedFrameHostClientIOS::DelegatedFrameHostClientIOS(
    RenderWidgetHostViewIOS* render_widget_host_view)
    : render_widget_host_view_(render_widget_host_view) {}

DelegatedFrameHostClientIOS::~DelegatedFrameHostClientIOS() {}

ui::Layer* DelegatedFrameHostClientIOS::DelegatedFrameHostGetLayer() const {
  // TODO(crbug.com/40254930): Fix me
  // return render_widget_host_view_->layer();
  return nullptr;
}

bool DelegatedFrameHostClientIOS::DelegatedFrameHostIsVisible() const {
  return !render_widget_host_view_->host()->is_hidden();
}

SkColor DelegatedFrameHostClientIOS::DelegatedFrameHostGetGutterColor() const {
  // When making an element on the page fullscreen the element's background
  // may not match the page's, so use black as the gutter color to avoid
  // flashes of brighter colors during the transition.
  if (render_widget_host_view_->host()->delegate() &&
      render_widget_host_view_->host()->delegate()->IsFullscreen()) {
    return SK_ColorBLACK;
  }
  if (render_widget_host_view_->GetBackgroundColor()) {
    return *render_widget_host_view_->GetBackgroundColor();
  }
  return SK_ColorWHITE;
}

void DelegatedFrameHostClientIOS::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  render_widget_host_view_->OnFrameTokenChangedForView(frame_token,
                                                       activation_time);
}

float DelegatedFrameHostClientIOS::GetDeviceScaleFactor() const {
  return 1.0f;
}

void DelegatedFrameHostClientIOS::InvalidateLocalSurfaceIdOnEviction() {
  // TODO(crbug.com/40254930): Fix me
  // render_widget_host_view_->InvalidateLocalSurfaceIdOnEviction();
}

viz::FrameEvictorClient::EvictIds
DelegatedFrameHostClientIOS::CollectSurfaceIdsForEviction() {
  viz::FrameEvictorClient::EvictIds ids;
  ids.embedded_ids =
      render_widget_host_view_->host()->CollectSurfaceIdsForEviction();
  return ids;
}

bool DelegatedFrameHostClientIOS::ShouldShowStaleContentOnEviction() {
  return false;
}

}  // namespace content
