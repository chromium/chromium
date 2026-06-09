// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_STATE_H_

#include "cc/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace gfx {
class Transform;
class Vector2dF;
}  // namespace gfx

namespace blink {

struct PLATFORM_EXPORT CanvasChildPaintState {
  bool operator==(const CanvasChildPaintState&) const;

  // Child element state.
  float effective_zoom = 1.f;
  gfx::Point3F transform_origin;
  gfx::SizeF box_size;

  // Canvas state.
  gfx::SizeF canvas_content_size;
  gfx::Size canvas_device_pixel_content_box;
  DOMNodeId canvas_node_id = kInvalidDOMNodeId;
  scoped_refptr<const cc::AnimatedImageFrameIndexMap>
      animated_image_frame_index_map;
  // NOTE: If adding more members, be sure to update operator==().
};

PLATFORM_EXPORT gfx::Transform GetElementTransform(
    const CanvasChildPaintState&,
    const gfx::Size& canvas_size,
    const gfx::Transform& draw_transform);

PLATFORM_EXPORT gfx::Vector2dF GetCanvasGridScaleFactor(
    const CanvasChildPaintState&,
    const gfx::Size& canvas_size);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_STATE_H_
