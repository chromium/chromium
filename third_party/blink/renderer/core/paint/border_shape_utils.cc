// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/border_shape_utils.h"

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/geometry_box_utils.h"

namespace blink {

std::optional<BorderShapeReferenceRects> ComputeBorderShapeReferenceRects(
    const PhysicalRect& rect,
    const ComputedStyle& style,
    const LayoutObject& layout_object) {
  if (!style.HasBorderShape() || !layout_object.IsBoxModelObject() ||
      layout_object.IsSVGChild()) {
    return std::nullopt;
  }

  const auto& box = To<LayoutBoxModelObject>(layout_object);
  const StyleBorderShape& border_shape = *style.BorderShape();

  auto make_rect = [&](GeometryBox geometry_box) {
    PhysicalRect expanded = rect;
    expanded.Expand(
        GeometryBoxUtils::ReferenceBoxBorderBoxOutsets(geometry_box, box));
    return expanded;
  };

  BorderShapeReferenceRects reference_rects;
  reference_rects.outer = make_rect(border_shape.OuterBox());
  reference_rects.inner = make_rect(border_shape.InnerBox());
  return reference_rects;
}

}  // namespace blink
