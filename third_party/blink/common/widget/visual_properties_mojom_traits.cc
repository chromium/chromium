// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/widget/visual_properties_mojom_traits.h"

#include "cc/mojom/browser_controls_params.mojom.h"
#include "services/viz/public/mojom/compositing/local_surface_id.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom.h"
#include "ui/display/mojom/screen_infos.mojom.h"

namespace mojo {

bool StructTraits<
    blink::mojom::VisualPropertiesDataView,
    blink::VisualProperties>::Read(blink::mojom::VisualPropertiesDataView data,
                                   blink::VisualProperties* out) {
  if (!data.ReadScreenInfos(&out->screen_infos) ||
      !data.ReadMinSizeForAutoResize(&out->min_size_for_auto_resize) ||
      !data.ReadMaxSizeForAutoResize(&out->max_size_for_auto_resize) ||
      !data.ReadNewSize(&out->new_size) ||
      !data.ReadVisibleViewportSize(&out->visible_viewport_size) ||
      !data.ReadCompositorViewportPixelRect(
          &out->compositor_viewport_pixel_rect) ||
      !data.ReadBrowserControlsParams(&out->browser_controls_params) ||
      !data.ReadLocalSurfaceId(&out->local_surface_id) ||
      !data.ReadRootWidgetViewportSegments(
          &out->root_widget_viewport_segments) ||
      !data.ReadWindowControlsOverlayRect(&out->window_controls_overlay_rect) ||
      !data.ReadWindowShowState(&out->window_show_state) ||
      data.page_scale_factor() <= 0 || data.compositing_scale_factor() <= 0 ||
      data.cursor_accessibility_scale_factor() < 1) {
    return false;
  }
  out->auto_resize_enabled = data.auto_resize_enabled();
  out->resizable = data.resizable();
  out->scroll_focused_node_into_view = data.scroll_focused_node_into_view();
  out->is_fullscreen_granted = data.is_fullscreen_granted();
  out->display_mode = data.display_mode();
  out->capture_sequence_number = data.capture_sequence_number();
  out->zoom_level = data.zoom_level();
  out->css_zoom_factor = data.css_zoom_factor();
  out->page_scale_factor = data.page_scale_factor();
  out->compositing_scale_factor = data.compositing_scale_factor();
  out->cursor_accessibility_scale_factor =
      data.cursor_accessibility_scale_factor();
  out->is_pinch_gesture_active = data.is_pinch_gesture_active();
  out->virtual_keyboard_resize_height_physical_px =
      data.virtual_keyboard_resize_height_physical_px();
  return true;
}

}  // namespace mojo
