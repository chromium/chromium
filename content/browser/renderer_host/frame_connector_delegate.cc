// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_connector_delegate.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/content_switches_internal.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"

namespace content {

void FrameConnectorDelegate::SetView(RenderWidgetHostViewChildFrame* view) {
  view_ = view;
}

RenderWidgetHostViewBase*
FrameConnectorDelegate::GetParentRenderWidgetHostView() {
  return nullptr;
}

RenderWidgetHostViewBase*
FrameConnectorDelegate::GetRootRenderWidgetHostView() {
  return nullptr;
}

void FrameConnectorDelegate::SendIntrinsicSizingInfoToParent(
    blink::mojom::IntrinsicSizingInfoPtr) {}

void FrameConnectorDelegate::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  screen_info_ = visual_properties.screen_info;
  local_surface_id_ = visual_properties.local_surface_id;

  capture_sequence_number_ = visual_properties.capture_sequence_number;

  SetScreenSpaceRect(visual_properties.screen_space_rect);
  SetLocalFrameSize(visual_properties.local_frame_size);

  if (!view_)
    return;

  RenderWidgetHostImpl* render_widget_host = view_->host();
  DCHECK(render_widget_host);

  render_widget_host->SetAutoResize(visual_properties.auto_resize_enabled,
                                    visual_properties.min_size_for_auto_resize,
                                    visual_properties.max_size_for_auto_resize);
  render_widget_host->SetVisualPropertiesFromParentFrame(
      visual_properties.page_scale_factor,
      visual_properties.is_pinch_gesture_active,
      visual_properties.visible_viewport_size,
      visual_properties.compositor_viewport,
      visual_properties.root_widget_window_segments);

  render_widget_host->SynchronizeVisualProperties();
}

gfx::PointF FrameConnectorDelegate::TransformPointToRootCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& surface_id) {
  return gfx::PointF();
}

bool FrameConnectorDelegate::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    const viz::SurfaceId& local_surface_id,
    gfx::PointF* transformed_point) {
  return false;
}

bool FrameConnectorDelegate::BubbleScrollEvent(
    const blink::WebGestureEvent& event) {
  return false;
}

bool FrameConnectorDelegate::HasFocus() {
  return false;
}

blink::mojom::PointerLockResult FrameConnectorDelegate::LockMouse(
    bool request_unadjusted_movement) {
  NOTREACHED();
  return blink::mojom::PointerLockResult::kUnknownError;
}

blink::mojom::PointerLockResult FrameConnectorDelegate::ChangeMouseLock(
    bool request_unadjusted_movement) {
  NOTREACHED();
  return blink::mojom::PointerLockResult::kUnknownError;
}

void FrameConnectorDelegate::EnableAutoResize(const gfx::Size& min_size,
                                              const gfx::Size& max_size) {}

void FrameConnectorDelegate::DisableAutoResize() {}

bool FrameConnectorDelegate::IsInert() const {
  return false;
}

cc::TouchAction FrameConnectorDelegate::InheritedEffectiveTouchAction() const {
  return cc::TouchAction::kAuto;
}

bool FrameConnectorDelegate::IsHidden() const {
  return false;
}

bool FrameConnectorDelegate::IsThrottled() const {
  return false;
}

bool FrameConnectorDelegate::IsSubtreeThrottled() const {
  return false;
}

void FrameConnectorDelegate::SetLocalFrameSize(
    const gfx::Size& local_frame_size) {
  has_size_ = true;
  if (use_zoom_for_device_scale_factor_) {
    local_frame_size_in_pixels_ = local_frame_size;
    local_frame_size_in_dip_ = gfx::ScaleToRoundedSize(
        local_frame_size, 1.f / screen_info_.device_scale_factor);
  } else {
    local_frame_size_in_dip_ = local_frame_size;
    local_frame_size_in_pixels_ = gfx::ScaleToCeiledSize(
        local_frame_size, screen_info_.device_scale_factor);
  }
}

void FrameConnectorDelegate::SetScreenSpaceRect(
    const gfx::Rect& screen_space_rect) {
  if (use_zoom_for_device_scale_factor_) {
    screen_space_rect_in_pixels_ = screen_space_rect;
    screen_space_rect_in_dip_ = gfx::Rect(
        gfx::ScaleToFlooredPoint(screen_space_rect.origin(),
                                 1.f / screen_info_.device_scale_factor),
        gfx::ScaleToCeiledSize(screen_space_rect.size(),
                               1.f / screen_info_.device_scale_factor));
  } else {
    screen_space_rect_in_dip_ = screen_space_rect;
    screen_space_rect_in_pixels_ = gfx::ScaleToEnclosingRect(
        screen_space_rect, screen_info_.device_scale_factor);
  }
}

FrameConnectorDelegate::FrameConnectorDelegate(
    bool use_zoom_for_device_scale_factor)
    : use_zoom_for_device_scale_factor_(use_zoom_for_device_scale_factor) {}

}  // namespace content
