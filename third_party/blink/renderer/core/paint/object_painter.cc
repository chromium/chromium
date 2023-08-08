// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_painter.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/outline_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

void ObjectPainter::PaintOutline(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));

  const ComputedStyle& style_to_use = layout_object_.StyleRef();
  if (!style_to_use.HasOutline() ||
      style_to_use.Visibility() != EVisibility::kVisible)
    return;

  // Only paint the focus ring by hand if the theme isn't able to draw the focus
  // ring.
  if (style_to_use.OutlineStyleIsAuto() &&
      !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(
          layout_object_.GetNode(), style_to_use)) {
    return;
  }

  LayoutObject::OutlineInfo info;
  auto outline_rects = layout_object_.OutlineRects(
      &info, paint_offset,
      style_to_use.OutlineRectsShouldIncludeBlockVisualOverflow());
  if (outline_rects.empty())
    return;

  OutlinePainter::PaintOutlineRects(paint_info, layout_object_, outline_rects,
                                    info, style_to_use,
                                    layout_object_.GetDocument());
}

void ObjectPainter::PaintInlineChildrenOutlines(const PaintInfo& paint_info) {
  DCHECK(ShouldPaintDescendantOutlines(paint_info.phase));

  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  for (LayoutObject* child = layout_object_.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsLayoutInline() &&
        !To<LayoutInline>(child)->HasSelfPaintingLayer())
      child->Paint(paint_info_for_descendants);
  }
}

void ObjectPainter::AddURLRectIfNeeded(const PaintInfo& paint_info,
                                       const PhysicalOffset& paint_offset) {
  DCHECK(paint_info.ShouldAddUrlMetadata());
  if (!layout_object_.GetNode() || !layout_object_.GetNode()->IsLink() ||
      layout_object_.StyleRef().Visibility() != EVisibility::kVisible) {
    return;
  }

  KURL url = To<Element>(layout_object_.GetNode())->HrefURL();
  if (!url.IsValid())
    return;

  auto outline_rects = layout_object_.OutlineRects(
      nullptr, paint_offset, NGOutlineType::kIncludeBlockVisualOverflow);
  gfx::Rect bounding_rect = ToPixelSnappedRect(UnionRect(outline_rects));
  if (bounding_rect.IsEmpty()) {
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_object_,
          DisplayItem::kPrintedContentPDFURLRect))
    return;

  DrawingRecorder recorder(paint_info.context, layout_object_,
                           DisplayItem::kPrintedContentPDFURLRect,
                           bounding_rect);

  Document& document = layout_object_.GetDocument();
  String fragment_name;
  if (url.HasFragmentIdentifier() &&
      EqualIgnoringFragmentIdentifier(url, document.BaseURL())) {
    fragment_name = url.FragmentIdentifier();
    if (!document.FindAnchor(fragment_name)) {
      return;
    }
  }

  for (auto physical_rect : outline_rects) {
    gfx::Rect rect = ToPixelSnappedRect(physical_rect);
    if (fragment_name) {
      paint_info.context.SetURLFragmentForRect(fragment_name, rect);
    } else {
      paint_info.context.SetURLForRect(url, rect);
    }
  }
}

void ObjectPainter::PaintAllPhasesAtomically(const PaintInfo& paint_info) {
  // Pass kSelectionDragImage and kTextClip to the descendants so that
  // they will paint for selection and text clip respectively. We don't need
  // complete painting for these phases.
  if (paint_info.phase == PaintPhase::kSelectionDragImage ||
      paint_info.phase == PaintPhase::kTextClip) {
    layout_object_.Paint(paint_info);
    return;
  }

  if (paint_info.phase != PaintPhase::kForeground)
    return;

  PaintInfo info(paint_info);
  info.phase = PaintPhase::kBlockBackground;
  layout_object_.Paint(info);
  info.phase = PaintPhase::kForcedColorsModeBackplate;
  layout_object_.Paint(info);
  info.phase = PaintPhase::kFloat;
  layout_object_.Paint(info);
  info.phase = PaintPhase::kForeground;
  layout_object_.Paint(info);
  info.phase = PaintPhase::kOutline;
  layout_object_.Paint(info);
}

}  // namespace blink
