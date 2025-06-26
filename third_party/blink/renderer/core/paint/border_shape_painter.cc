// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/border_shape_painter.h"

#include <numbers>

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_border_shape.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

// static
bool BorderShapePainter::Paint(GraphicsContext& context,
                               const PhysicalRect& rect,
                               const ComputedStyle& style) {
  const StyleBorderShape* border_shape = style.BorderShape();
  if (!border_shape) {
    return false;
  }

  if (!style.HasVisibleStroke()) {
    return true;
  }

  const float zoom = style.EffectiveZoom();
  const Path outer_path =
      border_shape->OuterShape().GetPath(gfx::RectF(rect), zoom, 1);
  StrokeData stroke_data;
  const AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBorder));
  const float zoomed_reference_box_normal_length =
      gfx::Vector2dF(rect.size.width, rect.size.height).Length() /
      std::numbers::sqrt2;
  const float unzoomed_reference_box_normal_length =
      zoomed_reference_box_normal_length / zoom;
  const float unzoomed_thickness =
      ValueForLength(style.StrokeWidth().length(),
                     LayoutUnit(unzoomed_reference_box_normal_length));
  const float thickness = unzoomed_thickness * zoom;
  stroke_data.SetThickness(thickness);
  stroke_data.SetLineJoin(style.JoinStyle());
  stroke_data.SetMiterLimit(style.StrokeMiterLimit());
  context.SetShouldAntialias(true);
  context.SetStrokeColor(style.StrokePaint().GetColor().GetColor());
  stroke_data.SetLineCap(style.CapStyle());
  context.SetStroke(stroke_data);
  context.StrokePath(outer_path, auto_dark_mode);
  return true;
}

}  // namespace blink
