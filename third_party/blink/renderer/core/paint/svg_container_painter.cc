// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_container_painter.h"

#include <optional>

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_foreign_object_painter.h"
#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"

namespace blink {

namespace {

bool HasReferenceFilterEffect(const ObjectPaintProperties& properties) {
  return properties.Filter() &&
         properties.Filter()->Filter().HasReferenceFilter();
}

}  // namespace

bool SVGContainerPainter::CanUseCullRect() const {
  // LayoutSVGHiddenContainer's visual rect is always empty but we need to
  // paint its descendants so we cannot skip painting.
  if (layout_svg_container_.IsSVGHiddenContainer())
    return false;

  if (layout_svg_container_.SVGDescendantMayHaveTransformRelatedAnimation())
    return false;

  return SVGModelObjectPainter::CanUseCullRect(
      layout_svg_container_.StyleRef());
}

void SVGContainerPainter::Paint(const PaintInfo& paint_info) {
  // Spec: An empty viewBox on the <svg> element disables rendering.
  DCHECK(layout_svg_container_.GetElement());
  auto* svg_svg_element =
      DynamicTo<SVGSVGElement>(*layout_svg_container_.GetElement());
  if (svg_svg_element && svg_svg_element->HasEmptyViewBox())
    return;

  const auto* properties =
      layout_svg_container_.FirstFragment().PaintProperties();
  PaintInfo paint_info_before_filtering(paint_info);
  if (CanUseCullRect()) {
    if (!paint_info.GetCullRect().IntersectsTransformed(
            layout_svg_container_.LocalToSVGParentTransform(),
            layout_svg_container_.VisualRectInLocalSVGCoordinates()))
      return;
    if (properties) {
      // TODO(https://crbug.com/1278452): Also consider Translate, Rotate,
      // Scale, and Offset, probably via a single transform operation to
      // FirstFragment().PreTransform().
      if (const auto* transform = properties->Transform())
        paint_info_before_filtering.TransformCullRect(*transform);
    }
  } else {
    paint_info_before_filtering.ApplyInfiniteCullRect();
  }

  ScopedSVGTransformState transform_state(paint_info_before_filtering,
                                          layout_svg_container_);
  {
    std::optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
    if (layout_svg_container_.IsSVGViewportContainer() &&
        SVGLayoutSupport::IsOverflowHidden(layout_svg_container_)) {
      // TODO(crbug.com/814815): The condition should be a DCHECK, but for now
      // we may paint the object for filters during PrePaint before the
      // properties are ready.
      if (properties && properties->OverflowClip()) {
        scoped_paint_chunk_properties.emplace(
            paint_info_before_filtering.context.GetPaintController(),
            *properties->OverflowClip(), layout_svg_container_,
            paint_info_before_filtering.DisplayItemTypeForClipping());
      }
    }

    ScopedSVGPaintState paint_state(layout_svg_container_,
                                    paint_info_before_filtering);
    // When a filter applies to the container we need to make sure
    // that it is applied even if nothing is painted.
    if (paint_info_before_filtering.phase == PaintPhase::kForeground &&
        properties && HasReferenceFilterEffect(*properties))
      paint_info_before_filtering.context.GetPaintController().EnsureChunk();

    PaintInfo& child_paint_info = transform_state.ContentPaintInfo();
    std::optional<SvgContextPaints> child_context_paints;
    if (IsA<SVGUseElement>(layout_svg_container_.GetElement())) {
      SVGObjectPainter object_painter(layout_svg_container_,
                                      child_paint_info.GetSvgContextPaints());
      // Note that this discards child_paint_info.svg_context_paints_.transform,
      // which is correct because <use> establishes a new coordinate space for
      // context paints.
      child_context_paints.emplace(
          object_painter.ResolveContextPaint(
              layout_svg_container_.StyleRef().FillPaint()),
          object_painter.ResolveContextPaint(
              layout_svg_container_.StyleRef().StrokePaint()));
      child_paint_info.SetSvgContextPaints(&(*child_context_paints));
    }

    for (LayoutObject* child = layout_svg_container_.FirstChild(); child;
         child = child->NextSibling()) {
      if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(child)) {
        SVGForeignObjectPainter(*foreign_object)
            .PaintLayer(paint_info_before_filtering);
      } else {
        child->Paint(child_paint_info);
      }
    }
  }

  // Only paint an outline if there are children.
  if (layout_svg_container_.FirstChild()) {
    SVGModelObjectPainter(layout_svg_container_)
        .PaintOutline(paint_info_before_filtering);
  }

  if (paint_info_before_filtering.ShouldAddUrlMetadata() &&
      paint_info_before_filtering.phase == PaintPhase::kForeground) {
    ObjectPainter(layout_svg_container_)
        .AddURLRectIfNeeded(paint_info_before_filtering, PhysicalOffset());
  }
}

}  // namespace blink
