// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host_client_aura.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"

namespace content {

DelegatedFrameHostClientAura::DelegatedFrameHostClientAura(
    RenderWidgetHostViewAura* render_widget_host_view)
    : render_widget_host_view_(render_widget_host_view) {}

DelegatedFrameHostClientAura::~DelegatedFrameHostClientAura() {}

ui::Layer* DelegatedFrameHostClientAura::DelegatedFrameHostGetLayer() const {
  return render_widget_host_view_->window()->layer();
}

bool DelegatedFrameHostClientAura::DelegatedFrameHostIsVisible() const {
  return !render_widget_host_view_->host()->is_hidden();
}

SkColor DelegatedFrameHostClientAura::DelegatedFrameHostGetGutterColor() const {
  // When making an element on the page fullscreen the element's background
  // may not match the page's, so use black as the gutter color to avoid
  // flashes of brighter colors during the transition.
  if (render_widget_host_view_->host()->delegate() &&
      render_widget_host_view_->host()->delegate()->IsFullscreen()) {
    return SK_ColorBLACK;
  }
  if (render_widget_host_view_->GetBackgroundColor())
    return *render_widget_host_view_->GetBackgroundColor();
  return SK_ColorWHITE;
}

void DelegatedFrameHostClientAura::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  render_widget_host_view_->OnFrameTokenChangedForView(frame_token,
                                                       activation_time);
}

float DelegatedFrameHostClientAura::GetDeviceScaleFactor() const {
  return render_widget_host_view_->device_scale_factor_;
}

void DelegatedFrameHostClientAura::InvalidateLocalSurfaceIdOnEviction() {
  render_widget_host_view_->InvalidateLocalSurfaceIdOnEviction();
}

std::vector<viz::SurfaceId>
DelegatedFrameHostClientAura::CollectSurfaceIdsForEviction() {
  return render_widget_host_view_->host()->CollectSurfaceIdsForEviction();
}

bool DelegatedFrameHostClientAura::ShouldShowStaleContentOnEviction() {
  return render_widget_host_view_->ShouldShowStaleContentOnEviction();
}

}  // namespace content
