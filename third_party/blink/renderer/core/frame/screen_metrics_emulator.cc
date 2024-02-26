// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/screen_metrics_emulator.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/widget/visual_properties.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"

namespace blink {

ScreenMetricsEmulator::ScreenMetricsEmulator(
    WebFrameWidgetImpl* frame_widget,
    const display::ScreenInfos& screen_infos,
    const gfx::Size& widget_size,
    const gfx::Size& visible_viewport_size,
    const gfx::Rect& view_screen_rect,
    const gfx::Rect& window_screen_rect)
    : frame_widget_(frame_widget),
      original_screen_infos_(screen_infos),
      original_widget_size_(widget_size),
      original_visible_viewport_size_(visible_viewport_size),
      original_view_screen_rect_(view_screen_rect),
      original_window_screen_rect_(window_screen_rect) {}

const display::ScreenInfo& ScreenMetricsEmulator::GetOriginalScreenInfo()
    const {
  return original_screen_infos_.current();
}

void ScreenMetricsEmulator::Trace(Visitor* vistor) const {
  vistor->Trace(frame_widget_);
}

void ScreenMetricsEmulator::DisableAndApply() {
  frame_widget_->SetScreenMetricsEmulationParameters(false, emulation_params_);
  frame_widget_->SetScreenRects(original_view_screen_rect_,
                                original_window_screen_rect_);
  frame_widget_->SetViewportSegments(original_root_viewport_segments_);
  frame_widget_->SetScreenInfoAndSize(original_screen_infos_,
                                      original_widget_size_,
                                      original_visible_viewport_size_);
  // The posture service will restore the original device posture coming from
  // the platform.
  frame_widget_->DisableDevicePostureOverrideForEmulation();
}

void ScreenMetricsEmulator::ChangeEmulationParams(
    const DeviceEmulationParams& params) {
  emulation_params_ = params;
  Apply();
}

gfx::Point ScreenMetricsEmulator::ViewRectOrigin() {
  gfx::Point widget_pos = original_view_rect().origin();
  if (emulation_params_.view_position)
    widget_pos = emulation_params_.view_position.value();
  else if (!emulating_desktop())
    widget_pos = gfx::Point();
  return widget_pos;
}

void ScreenMetricsEmulator::Apply() {
  // The WidgetScreenRect gets derived from the widget size of the main frame
  // widget, not from the original WidgetScreenRect.
  gfx::Size widget_size = original_widget_size_;
  // The WindowScreenRect gets derived from the original WindowScreenRect,
  // though.
  gfx::Size window_size = original_window_rect().size();

  // If either the width or height are specified by the emulator, then we use
  // that size, and assume that they have the scale pre-applied to them.
  if (emulation_params_.view_size.width()) {
    widget_size.set_width(emulation_params_.view_size.width());
  } else {
    widget_size.set_width(
        base::ClampRound(widget_size.width() / emulation_params_.scale));
  }
  if (emulation_params_.view_size.height()) {
    widget_size.set_height(emulation_params_.view_size.height());
  } else {
    widget_size.set_height(
        base::ClampRound(widget_size.height() / emulation_params_.scale));
  }

  // For mobile emulation, the window size is changed to match the widget size,
  // as there are no window decorations around the widget.
  if (!emulating_desktop())
    window_size = widget_size;

  gfx::Point widget_pos = original_view_rect().origin();
  gfx::Point window_pos = original_window_rect().origin();

  if (emulation_params_.view_position) {
    // The emulated widget position overrides the widget and window positions.
    widget_pos = emulation_params_.view_position.value();
    window_pos = widget_pos;
  } else if (!emulating_desktop()) {
    // For mobile emulation, the widget and window are moved to 0,0 if not
    // explicitly specified.
    widget_pos = gfx::Point();
    window_pos = widget_pos;
  }

  const display::ScreenInfo& original_screen_info =
      original_screen_infos_.current();
  gfx::Rect screen_rect = original_screen_info.rect;

  if (!emulation_params_.screen_size.IsEmpty()) {
    // The emulated screen size overrides the real one, and moves the screen's
    // origin to 0,0.
    screen_rect = gfx::Rect(gfx::Size(emulation_params_.screen_size));
  } else if (!emulating_desktop()) {
    // For mobile emulation, the screen is adjusted to match the position and
    // size of the widget rect, if not explicitly specified.
    screen_rect = gfx::Rect(widget_pos, widget_size);
  }

  float device_scale_factor = original_screen_info.device_scale_factor;

  if (emulation_params_.device_scale_factor)
    device_scale_factor = emulation_params_.device_scale_factor;

  display::mojom::blink::ScreenOrientation orientation_type =
      original_screen_info.orientation_type;
  uint16_t orientation_angle = original_screen_info.orientation_angle;
  if (emulation_params_.screen_orientation_type !=
      display::mojom::blink::ScreenOrientation::kUndefined) {
    orientation_type = emulation_params_.screen_orientation_type;
    orientation_angle = emulation_params_.screen_orientation_angle;
  }

  // Pass three emulation parameters to the blink side:
  // - we keep the real device scale factor in compositor to produce sharp image
  //   even when emulating different scale factor;
  DeviceEmulationParams modified_emulation_params = emulation_params_;
  modified_emulation_params.device_scale_factor =
      original_screen_info.device_scale_factor;
  frame_widget_->SetScreenMetricsEmulationParameters(
      true, std::move(modified_emulation_params));

  frame_widget_->SetScreenRects(gfx::Rect(widget_pos, widget_size),
                                gfx::Rect(window_pos, window_size));

  // If there are no emulated viewport segments, use the emulated widget size
  // instead. When we switch from emulated segments to not having any, we should
  // have a single segment that matches the widget size.
  bool has_emulated_segments = emulation_params_.viewport_segments.size();
  if (has_emulated_segments) {
    frame_widget_->SetViewportSegments(emulation_params_.viewport_segments);
  } else {
    std::vector<gfx::Rect> emulated_segments{
        {0, 0, widget_size.width(), widget_size.height()}};
    frame_widget_->SetViewportSegments(emulated_segments);
  }

  frame_widget_->OverrideDevicePostureForEmulation(
      emulation_params_.device_posture);

  display::ScreenInfos emulated_screen_infos = original_screen_infos_;
  display::ScreenInfo& emulated_screen_info =
      emulated_screen_infos.mutable_current();
  emulated_screen_info.device_scale_factor = device_scale_factor;
  emulated_screen_info.rect = screen_rect;
  emulated_screen_info.available_rect = screen_rect;
  emulated_screen_info.orientation_type = orientation_type;
  emulated_screen_info.orientation_angle = orientation_angle;
  frame_widget_->SetScreenInfoAndSize(emulated_screen_infos,
                                      /*widget_size=*/widget_size,
                                      /*visible_viewport_size=*/widget_size);
}

void ScreenMetricsEmulator::UpdateVisualProperties(
    const VisualProperties& visual_properties) {
  // Device emulation isn't supported for widgets that have auto resize mode
  // enabled.
  DCHECK(!frame_widget_->AutoResizeMode());

  original_screen_infos_ = visual_properties.screen_infos;
  original_widget_size_ = visual_properties.new_size;
  original_visible_viewport_size_ = visual_properties.visible_viewport_size;
  original_root_viewport_segments_ =
      visual_properties.root_widget_viewport_segments;
  Apply();

  // Appy the compositor viewport rect and surface id allocation. The screen
  // info is kept the same as the current ScreenInfo state. The screen info
  // already was updated in |Apply| via |SetScreenInfoAndSize|.
  frame_widget_->UpdateSurfaceAndCompositorRect(
      visual_properties.local_surface_id.value_or(viz::LocalSurfaceId()),
      visual_properties.compositor_viewport_pixel_rect);
}

void ScreenMetricsEmulator::OnUpdateScreenRects(
    const gfx::Rect& view_screen_rect,
    const gfx::Rect& window_screen_rect) {
  original_view_screen_rect_ = view_screen_rect;
  original_window_screen_rect_ = window_screen_rect;
  if (emulating_desktop()) {
    Apply();
  }
}

}  // namespace blink
