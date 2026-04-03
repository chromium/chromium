// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_STATE_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

struct PLATFORM_EXPORT CanvasChildPaintState {
  bool operator==(const CanvasChildPaintState&) const = default;

  // Child element state.
  float effective_zoom = 1.f;
  gfx::Point3F transform_origin;
  gfx::SizeF box_size;

  // Canvas state.
  gfx::Size canvas_size;
  gfx::SizeF canvas_content_size;
  gfx::Vector2dF canvas_grid_scale_factor;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_STATE_H_
