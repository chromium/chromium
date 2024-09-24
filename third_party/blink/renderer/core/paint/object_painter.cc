// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_painter.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/outline_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

void ObjectPainter::PaintOutline(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));

  const ComputedStyle& style_to_use = layout_object_.StyleRef();
  if (!style_to_use.HasOutline() ||
      style_to_use.UsedVisibility() != EVisibility::kVisible) {
    return;
  }

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
      style_to_use.OutlineRectsShouldIncludeBlockInkOverflow());
  if (outline_rects.empty())
    return;

  OutlinePainter::PaintOutlineRects(paint_info, layout_object_, outline_rects,
                                    info, style_to_use);
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
      layout_object_.StyleRef().UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  KURL url = To<Element>(layout_object_.GetNode())->HrefURL();
  if (!url.IsValid())
    return;

  auto outline_rects = layout_object_.OutlineRects(
      nullptr, paint_offset, OutlineType::kIncludeBlockInkOverflow);
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
    fragment_name = url.FragmentIdentifier().ToString();
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

void ObjectPainter::RecordHitTestData(
    const PaintInfo& paint_info,
    const gfx::Rect& paint_rect,
    const DisplayItemClient& background_client) {
  // When HitTestOpaqueness is not enabled, we only need to record hit test
  // data for scrolling background when there are special hit test data.
  if (!RuntimeEnabledFeatures::HitTestOpaquenessEnabled() &&
      paint_info.IsPaintingBackgroundInContentsSpace() &&
      !ShouldRecordSpecialHitTestData(paint_info)) {
    return;
  }

  // Hit test data are only needed for compositing. This flag is used for for
  // printing and drag images which do not need hit testing.
  if (paint_info.ShouldOmitCompositingInfo()) {
    return;
  }

  // If an object is not visible, it does not participate in painting or hit
  // testing. TODO(crbug.com/1471738): Some pointer-events values actually
  // allow hit testing with visibility:hidden.
  if (layout_object_.StyleRef().UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  paint_info.context.GetPaintController().RecordHitTestData(
      background_client, paint_rect,
      layout_object_.EffectiveAllowedTouchAction(),
      layout_object_.InsideBlockingWheelEventHandler(), GetHitTestOpaqueness());
}

cc::HitTestOpaqueness ObjectPainter::GetHitTestOpaqueness() const {
  if (!RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    return cc::HitTestOpaqueness::kMixed;
  }

  // Effects (e.g. clip-path and mask) are not checked here even if they
  // affects hit test. They are checked during PaintArtifactCompositor update
  // based on paint properties.

  if (!layout_object_.VisibleToHitTesting() ||
      !layout_object_.GetFrame()->GetVisibleToHitTesting()) {
    return cc::HitTestOpaqueness::kTransparent;
  }
  // Border radius is not considered opaque for hit test because the hit
  // test may be inside or outside of the rounded corner.
  if (layout_object_.StyleRef().HasBorderRadius()) {
    return cc::HitTestOpaqueness::kMixed;
  }
  // SVG children are not considered opaque for hit test because SVG has
  // special hit test rules for stroke/fill/etc, and the children may
  // overflow the root.
  if (layout_object_.IsSVGChild()) {
    return cc::HitTestOpaqueness::kMixed;
  }
  return cc::HitTestOpaqueness::kOpaque;
}

bool ObjectPainter::ShouldRecordSpecialHitTestData(
    const PaintInfo& paint_info) {
  if (layout_object_.EffectiveAllowedTouchAction() != TouchAction::kAuto) {
    return true;
  }
  if (layout_object_.InsideBlockingWheelEventHandler()) {
    return true;
  }
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    if (layout_object_.StyleRef().UsedPointerEvents() ==
        EPointerEvents::kNone) {
      return true;
    }
    if (paint_info.context.GetPaintController()
            .CurrentChunkIsNonEmptyAndTransparentToHitTest()) {
      // A non-none value of pointer-events will make a transparent paint chunk
      // (due to pointer-events: none on an ancestor painted into the current
      // paint chunk) not transparent.
      return true;
    }
  }
  return false;
}

}  // namespace blink
