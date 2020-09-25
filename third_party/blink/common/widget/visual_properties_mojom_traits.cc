// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/widget/visual_properties_mojom_traits.h"

#include "cc/mojom/browser_controls_params.mojom.h"
#include "services/viz/public/mojom/compositing/local_surface_id.mojom.h"
#include "third_party/blink/public/mojom/widget/screen_info.mojom.h"

namespace mojo {

bool StructTraits<
    blink::mojom::VisualPropertiesDataView,
    blink::VisualProperties>::Read(blink::mojom::VisualPropertiesDataView data,
                                   blink::VisualProperties* out) {
  if (!data.ReadScreenInfo(&out->screen_info) ||
      !data.ReadMinSizeForAutoResize(&out->min_size_for_auto_resize) ||
      !data.ReadMaxSizeForAutoResize(&out->max_size_for_auto_resize) ||
      !data.ReadNewSize(&out->new_size) ||
      !data.ReadVisibleViewportSize(&out->visible_viewport_size) ||
      !data.ReadCompositorViewportPixelRect(
          &out->compositor_viewport_pixel_rect) ||
      !data.ReadBrowserControlsParams(&out->browser_controls_params) ||
      !data.ReadLocalSurfaceId(&out->local_surface_id) ||
      !data.ReadRootWidgetWindowSegments(&out->root_widget_window_segments))
    return false;
  out->auto_resize_enabled = data.auto_resize_enabled();
  out->scroll_focused_node_into_view = data.scroll_focused_node_into_view();
  out->is_fullscreen_granted = data.is_fullscreen_granted();
  out->display_mode = data.display_mode();
  out->capture_sequence_number = data.capture_sequence_number();
  out->zoom_level = data.zoom_level();
  out->page_scale_factor = data.page_scale_factor();
  out->is_pinch_gesture_active = data.is_pinch_gesture_active();
  return true;
}

}  // namespace mojo
