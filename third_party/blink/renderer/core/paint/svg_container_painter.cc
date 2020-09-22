// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_container_painter.h"

#include "base/optional.h"
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
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"

namespace blink {

void SVGContainerPainter::Paint(const PaintInfo& paint_info) {
  // Spec: groups w/o children still may render filter content.
  if (!layout_svg_container_.FirstChild() &&
      !layout_svg_container_.SelfWillPaint())
    return;

  // Spec: An empty viewBox on the <svg> element disables rendering.
  DCHECK(layout_svg_container_.GetElement());
  auto* svg_svg_element =
      DynamicTo<SVGSVGElement>(*layout_svg_container_.GetElement());
  if (svg_svg_element && svg_svg_element->HasEmptyViewBox())
    return;

  if (SVGModelObjectPainter(layout_svg_container_)
          .CullRectSkipsPainting(paint_info)) {
    return;
  }

  PaintInfo paint_info_before_filtering(paint_info);
  if (SVGModelObjectPainter::ShouldUseInfiniteCullRect(
          layout_svg_container_.StyleRef())) {
    paint_info_before_filtering.ApplyInfiniteCullRect();
  } else if (const auto* properties =
                 layout_svg_container_.FirstFragment().PaintProperties()) {
    if (const auto* transform = properties->Transform())
      paint_info_before_filtering.TransformCullRect(*transform);
  }

  ScopedSVGTransformState transform_state(
      paint_info_before_filtering, layout_svg_container_,
      layout_svg_container_.LocalToSVGParentTransform());
  {
    base::Optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
    if (layout_svg_container_.IsSVGViewportContainer() &&
        SVGLayoutSupport::IsOverflowHidden(layout_svg_container_)) {
      const auto* fragment =
          paint_info_before_filtering.FragmentToPaint(layout_svg_container_);
      if (!fragment)
        return;
      const auto* properties = fragment->PaintProperties();
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
    bool continue_rendering = true;
    if (paint_state.GetPaintInfo().phase == PaintPhase::kForeground)
      continue_rendering = paint_state.ApplyEffects();

    if (continue_rendering) {
      // When a filter applies to the container we need to make sure
      // that it is applied even if nothing is painted.
      if (paint_state.GetPaintInfo().phase == PaintPhase::kForeground &&
          layout_svg_container_.SelfWillPaint())
        paint_state.GetPaintInfo().context.GetPaintController().EnsureChunk();

      for (LayoutObject* child = layout_svg_container_.FirstChild(); child;
           child = child->NextSibling()) {
        if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(*child)) {
          SVGForeignObjectPainter(*foreign_object)
              .PaintLayer(paint_state.GetPaintInfo());
        } else {
          child->Paint(paint_state.GetPaintInfo());
        }
      }
    }
  }

  SVGModelObjectPainter(layout_svg_container_)
      .PaintOutline(paint_info_before_filtering);

  if (paint_info_before_filtering.ShouldAddUrlMetadata() &&
      paint_info_before_filtering.phase == PaintPhase::kForeground) {
    ObjectPainter(layout_svg_container_)
        .AddURLRectIfNeeded(paint_info_before_filtering, PhysicalOffset());
  }
}

}  // namespace blink
