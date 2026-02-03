// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/border_shape_utils.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
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

Path ComputeBorderShapeOuterPath(const ComputedStyle& style,
                                 const PhysicalRect& rect,
                                 const LayoutObject* layout_object) {
  std::optional<BorderShapeReferenceRects> shape_ref_rects;
  if (layout_object) {
    shape_ref_rects =
        ComputeBorderShapeReferenceRects(rect, style, *layout_object);
  }
  PhysicalRect outer_reference_rect =
      shape_ref_rects ? shape_ref_rects->outer : rect;
  return BorderShapePainter::OuterPath(style, outer_reference_rect);
}

DerivedStroke RelevantSideForBorderShape(const ComputedStyle& style) {
  DCHECK(style.HasBorderShape());

  BorderEdgeArray edges;
  style.GetBorderEdgeInfo(edges);
  style.BorderBlockStartWidth();
  PhysicalToLogical<BorderEdge> logical_edges(
      style.GetWritingDirection(), edges[static_cast<unsigned>(BoxSide::kTop)],
      edges[static_cast<unsigned>(BoxSide::kRight)],
      edges[static_cast<unsigned>(BoxSide::kBottom)],
      edges[static_cast<unsigned>(BoxSide::kLeft)]);

  const BorderEdge block_start_edge = logical_edges.BlockStart();
  const BorderEdge inline_start_edge = logical_edges.InlineStart();
  const BorderEdge block_end_edge = logical_edges.BlockEnd();
  const BorderEdge inline_end_edge = logical_edges.InlineEnd();

  const BorderEdge edges_in_order[4] = {block_start_edge, inline_start_edge,
                                        block_end_edge, inline_end_edge};
  for (const BorderEdge& edge : edges_in_order) {
    if (edge.BorderStyle() == EBorderStyle::kNone) {
      continue;
    }
    return DerivedStroke{static_cast<float>(edge.UsedWidth()), edge.GetColor()};
  }
  // Return block-start.
  return DerivedStroke{static_cast<float>(edges_in_order[0].UsedWidth()),
                       edges_in_order[0].GetColor()};
}

}  // namespace blink
