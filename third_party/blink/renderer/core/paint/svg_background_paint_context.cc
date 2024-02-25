// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_background_paint_context.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

SVGBackgroundPaintContext::SVGBackgroundPaintContext(
    const LayoutObject& layout_object)
    : object_(layout_object) {}

gfx::RectF SVGBackgroundPaintContext::ReferenceBox(
    GeometryBox geometry_box) const {
  const gfx::RectF reference_box = SVGResources::ReferenceBoxForEffects(
      object_, geometry_box, SVGResources::ForeignObjectQuirk::kDisabled);
  return gfx::ScaleRect(reference_box, Style().EffectiveZoom());
}

gfx::RectF SVGBackgroundPaintContext::VisualOverflowRect() const {
  const gfx::RectF visual_rect = object_.VisualRectInLocalSVGCoordinates();
  // <foreignObject> returns a visual rect thas has zoom applied already. We
  // also need to include overflow from descendants.
  if (auto* svg_fo = DynamicTo<LayoutSVGForeignObject>(object_)) {
    const PhysicalRect visual_overflow =
        svg_fo->Layer()->LocalBoundingBoxIncludingSelfPaintingDescendants();
    return gfx::UnionRects(visual_rect, gfx::RectF(visual_overflow));
  }
  return gfx::ScaleRect(visual_rect, Style().EffectiveZoom());
}

const ComputedStyle& SVGBackgroundPaintContext::Style() const {
  return object_.StyleRef();
}

}  // namespace blink
