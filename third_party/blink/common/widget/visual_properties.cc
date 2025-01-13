// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/widget/visual_properties.h"

#include "base/ranges/algorithm.h"

namespace blink {

VisualProperties::VisualProperties() = default;
VisualProperties::VisualProperties(const VisualProperties& other) = default;
VisualProperties::~VisualProperties() = default;
VisualProperties& VisualProperties::operator=(const VisualProperties& other) =
    default;

bool VisualProperties::operator==(const VisualProperties& other) const {
  return screen_infos == other.screen_infos &&
         auto_resize_enabled == other.auto_resize_enabled &&
         min_size_for_auto_resize == other.min_size_for_auto_resize &&
         max_size_for_auto_resize == other.max_size_for_auto_resize &&
         new_size == other.new_size &&
         visible_viewport_size == other.visible_viewport_size &&
         compositor_viewport_pixel_rect ==
             other.compositor_viewport_pixel_rect &&
         browser_controls_params == other.browser_controls_params &&
         scroll_focused_node_into_view == other.scroll_focused_node_into_view &&
         local_surface_id == other.local_surface_id &&
         is_fullscreen_granted == other.is_fullscreen_granted &&
         display_mode == other.display_mode &&
         capture_sequence_number == other.capture_sequence_number &&
         zoom_level == other.zoom_level &&
         css_zoom_factor == other.css_zoom_factor &&
         page_scale_factor == other.page_scale_factor &&
         compositing_scale_factor == other.compositing_scale_factor &&
         cursor_accessibility_scale_factor ==
             other.cursor_accessibility_scale_factor &&
         root_widget_viewport_segments == other.root_widget_viewport_segments &&
         is_pinch_gesture_active == other.is_pinch_gesture_active &&
         window_controls_overlay_rect == other.window_controls_overlay_rect &&
         window_show_state == other.window_show_state &&
         resizable == other.resizable;
}

bool VisualProperties::operator!=(const VisualProperties& other) const {
  return !operator==(other);
}

}  // namespace blink
