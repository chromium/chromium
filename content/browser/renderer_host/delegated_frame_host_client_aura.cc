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
  // If the ui compositor is no longer visible, we may have evicted it, so
  // assign it a new LSI for when it is made visible again.
  auto* host = render_widget_host_view_->window()->GetHost();
  if (DelegatedFrameHost::ShouldIncludeUiCompositorForEviction() && host &&
      !host->compositor()->IsVisible()) {
    host->window()->InvalidateLocalSurfaceId();
    host->compositor()->SetLocalSurfaceIdFromParent(
        host->window()->GetLocalSurfaceId());
  }

  render_widget_host_view_->InvalidateLocalSurfaceIdOnEviction();
}

viz::FrameEvictorClient::EvictIds
DelegatedFrameHostClientAura::CollectSurfaceIdsForEviction() {
  viz::FrameEvictorClient::EvictIds ids;
  ids.embedded_ids =
      render_widget_host_view_->host()->CollectSurfaceIdsForEviction();

  // If the ui compositor is no longer visible, include its surface ID for
  // eviction as well. The surface ID may be invalid if we already evicted the
  // ui compositor. This can happen, for example, if we have multiple tabs that
  // were unlocked frames (not visible but not evicted) for the same ui
  // compositor which is now not visible, and we evict them. If the surface ID
  // is invalid, it means we already evicted the ui compositor so it is safe to
  // skip doing it again.
  auto* host = render_widget_host_view_->window()->GetHost();
  if (DelegatedFrameHost::ShouldIncludeUiCompositorForEviction() && host &&
      !host->compositor()->IsVisible() &&
      host->window()->GetSurfaceId().is_valid()) {
    ids.ui_compositor_id = host->window()->GetSurfaceId();
  }
  return ids;
}

bool DelegatedFrameHostClientAura::ShouldShowStaleContentOnEviction() {
  return render_widget_host_view_->ShouldShowStaleContentOnEviction();
}

}  // namespace content
