// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/overlay_surface_embedder.h"

#include "ui/compositor/layer.h"

namespace content {

OverlaySurfaceEmbedder::OverlaySurfaceEmbedder(OverlayWindow* window)
    : window_(window) {
  DCHECK(window_);
  // Add window background.
  window_background_layer_ = window_->GetWindowBackgroundLayer();
  window_background_layer_->SetBounds(
      gfx::Rect(gfx::Point(0, 0), window_->GetBounds().size()));

  // Add |window_background_layer_| to |window_| and stack it at the bottom.
  window_->GetLayer()->Add(window_background_layer_);
  window_->GetLayer()->StackAtBottom(window_background_layer_);

  video_layer_ = window_->GetVideoLayer();
  video_layer_->SetMasksToBounds(true);

  // The frame provided by the parent window's layer needs to show through
  // |video_layer_|.
  video_layer_->SetFillsBoundsOpaquely(false);
  // |video_layer_| bounds are set with the (0, 0) origin point. The
  // positioning of |window_| is dictated by itself.
  video_layer_->SetBounds(
      gfx::Rect(gfx::Point(0, 0), window_->GetBounds().size()));

  // Add |video_layer_| to |window_| and stack it above
  // |window_background_layer_|.
  window_->GetLayer()->Add(video_layer_);
  window_->GetLayer()->StackAbove(video_layer_, window_background_layer_);
}

OverlaySurfaceEmbedder::~OverlaySurfaceEmbedder() = default;

void OverlaySurfaceEmbedder::SetSurfaceId(const viz::SurfaceId& surface_id) {
  video_layer_ = window_->GetVideoLayer();
  // SurfaceInfo has information about the embedded surface.
  video_layer_->SetShowSurface(surface_id, window_->GetBounds().size(),
                               SK_ColorBLACK,
                               cc::DeadlinePolicy::UseDefaultDeadline(),
                               true /* stretch_content_to_fill_bounds */);
  video_layer_->SetOldestAcceptableFallback(surface_id);
}

void OverlaySurfaceEmbedder::UpdateLayerBounds() {
  // Update the size of window background.
  window_background_layer_ = window_->GetWindowBackgroundLayer();
  window_background_layer_->SetBounds(
      gfx::Rect(gfx::Point(0, 0), window_->GetBounds().size()));

  // Update the size and position of the video to stretch on the entire window.
  video_layer_ = window_->GetVideoLayer();
  video_layer_->SetBounds(window_->GetVideoBounds());
  video_layer_->SetSurfaceSize(window_->GetVideoBounds().size());
}

}  // namespace content
