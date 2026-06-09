// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_child_paint_state.h"

#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

bool CanvasChildPaintState::operator==(
    const CanvasChildPaintState& other) const {
  return effective_zoom == other.effective_zoom &&
         transform_origin == other.transform_origin &&
         box_size == other.box_size &&
         canvas_content_size == other.canvas_content_size &&
         canvas_device_pixel_content_box ==
             other.canvas_device_pixel_content_box &&
         canvas_node_id == other.canvas_node_id &&
         (!!animated_image_frame_index_map ==
          !!other.animated_image_frame_index_map) &&
         (!animated_image_frame_index_map ||
          *animated_image_frame_index_map ==
              *other.animated_image_frame_index_map);
}

gfx::Transform GetElementTransform(const CanvasChildPaintState& paint_state,
                                   const gfx::Size& canvas_size,
                                   const gfx::Transform& draw_transform) {
  gfx::Vector2dF physical_to_canvas_grid =
      GetCanvasGridScaleFactor(paint_state, canvas_size);
  float physical_to_css = 1.0f / paint_state.effective_zoom;
  float canvas_grid_to_css_x = physical_to_css / physical_to_canvas_grid.x();
  float canvas_grid_to_css_y = physical_to_css / physical_to_canvas_grid.y();

  // 1. Change of basis for a transform in canvas pixel grid coordinates to a
  // canvas in css coordinates. The general formula is:
  //   T_css = S_canvas_to_css * T_canvas * S_canvas_to_css^-1
  gfx::Transform css_transform;
  css_transform.Scale(canvas_grid_to_css_x, canvas_grid_to_css_y);
  css_transform.PreConcat(draw_transform);
  css_transform.Scale(1.0f / canvas_grid_to_css_x, 1.0f / canvas_grid_to_css_y);

  // 2. Apply the transform relative to the transform origin.
  gfx::Transform result;
  result.Translate3d(-paint_state.transform_origin.x(),
                     -paint_state.transform_origin.y(),
                     -paint_state.transform_origin.z());
  result.PreConcat(css_transform);
  result.Translate3d(paint_state.transform_origin.x(),
                     paint_state.transform_origin.y(),
                     paint_state.transform_origin.z());

  return result;
}

gfx::Vector2dF GetCanvasGridScaleFactor(
    const CanvasChildPaintState& paint_state,
    const gfx::Size& canvas_size) {
  // As a special case, if the canvas is sized to its devicePixelContentBox,
  // make sure the element's physical pixels are mapped 1:1 to the canvas
  // grid to avoid any inadvertent fuzziness due to rounding.
  gfx::Vector2dF canvas_grid_scale_factor = {1.f, 1.f};
  if (canvas_size != paint_state.canvas_device_pixel_content_box &&
      !paint_state.canvas_content_size.IsEmpty()) {
    canvas_grid_scale_factor = {
        canvas_size.width() / paint_state.canvas_content_size.width(),
        canvas_size.height() / paint_state.canvas_content_size.height()};
  }
  return canvas_grid_scale_factor;
}

}  // namespace blink
