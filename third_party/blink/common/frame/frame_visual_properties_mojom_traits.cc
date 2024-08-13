// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/frame_visual_properties_mojom_traits.h"

#include "services/viz/public/mojom/compositing/local_surface_id.mojom.h"
#include "ui/display/mojom/screen_infos.mojom.h"
#include "ui/gfx/mojom/display_color_spaces.mojom.h"

namespace mojo {

bool StructTraits<blink::mojom::FrameVisualPropertiesDataView,
                  blink::FrameVisualProperties>::
    Read(blink::mojom::FrameVisualPropertiesDataView data,
         blink::FrameVisualProperties* out) {
  if (!data.ReadScreenInfos(&out->screen_infos) ||
      !data.ReadMinSizeForAutoResize(&out->min_size_for_auto_resize) ||
      !data.ReadMaxSizeForAutoResize(&out->max_size_for_auto_resize) ||
      !data.ReadVisibleViewportSize(&out->visible_viewport_size) ||
      !data.ReadCompositorViewport(&out->compositor_viewport) ||
      !data.ReadRectInLocalRoot(&out->rect_in_local_root) ||
      !data.ReadLocalFrameSize(&out->local_frame_size) ||
      !data.ReadRootWidgetViewportSegments(
          &out->root_widget_viewport_segments) ||
      !data.ReadLocalSurfaceId(&out->local_surface_id) ||
      data.page_scale_factor() <= 0 || data.compositing_scale_factor() <= 0 ||
      data.cursor_accessibility_scale_factor() < 1.f) {
    return false;
  }
  out->auto_resize_enabled = data.auto_resize_enabled();
  out->capture_sequence_number = data.capture_sequence_number();
  out->zoom_level = data.zoom_level();
  out->css_zoom_factor = data.css_zoom_factor();
  out->page_scale_factor = data.page_scale_factor();
  out->compositing_scale_factor = data.compositing_scale_factor();
  out->cursor_accessibility_scale_factor =
      data.cursor_accessibility_scale_factor();
  out->is_pinch_gesture_active = data.is_pinch_gesture_active();
  return true;
}

}  // namespace mojo
