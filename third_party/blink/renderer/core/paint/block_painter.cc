// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/block_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/line_box_list_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"

namespace blink {

DISABLE_CFI_PERF
void BlockPainter::Paint(const PaintInfo& paint_info) {
  ScopedPaintState paint_state(layout_block_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  auto paint_offset = paint_state.PaintOffset();
  auto& local_paint_info = paint_state.MutablePaintInfo();
  PaintPhase original_phase = local_paint_info.phase;

  if (original_phase == PaintPhase::kOutline) {
    local_paint_info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    local_paint_info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    layout_block_.PaintObject(local_paint_info, paint_offset);
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      local_paint_info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase == PaintPhase::kMask) {
    layout_block_.PaintObject(local_paint_info, paint_offset);
  } else if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
             original_phase != PaintPhase::kSelfOutlineOnly) {
    ScopedBoxContentsPaintState contents_paint_state(paint_state,
                                                     layout_block_);
    layout_block_.PaintObject(contents_paint_state.GetPaintInfo(),
                              contents_paint_state.PaintOffset());
  }

  // Carets are painted in the foreground phase, outside of the contents
  // properties block. Note that caret painting does not seem to correspond to
  // any painting order steps within the CSS spec.
  if (original_phase == PaintPhase::kForeground &&
      layout_block_.ShouldPaintCarets()) {
    // Apply overflow clip if needed. TODO(wangxianzhu): Move PaintCarets()
    // under |contents_paint_state| in the above block and let the caret
    // painters paint in the space of scrolling contents.
    base::Optional<ScopedPaintChunkProperties> paint_chunk_properties;
    if (const auto* fragment = paint_state.FragmentToPaint()) {
      if (const auto* properties = fragment->PaintProperties()) {
        if (const auto* overflow_clip = properties->OverflowClip()) {
          paint_chunk_properties.emplace(
              paint_info.context.GetPaintController(), overflow_clip,
              layout_block_, DisplayItem::kCaret);
        }
      }
    }

    PaintCarets(paint_info, paint_offset);
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    local_paint_info.phase = PaintPhase::kSelfOutlineOnly;
    layout_block_.PaintObject(local_paint_info, paint_offset);
  }

  // Our scrollbar widgets paint exactly when we tell them to, so that they work
  // properly with z-index. We paint after we painted the background/border, so
  // that the scrollbars will sit above the background/border.
  local_paint_info.phase = original_phase;
  PaintOverflowControlsIfNeeded(local_paint_info, paint_offset);
}

void BlockPainter::PaintOverflowControlsIfNeeded(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  if (layout_block_.HasOverflowClip() &&
      layout_block_.StyleRef().Visibility() == EVisibility::kVisible &&
      ShouldPaintSelfBlockBackground(paint_info.phase)) {
    ScrollableAreaPainter(*layout_block_.Layer()->GetScrollableArea())
        .PaintOverflowControls(paint_info, RoundedIntPoint(paint_offset),
                               false /* painting_overlay_controls */);
  }
}

void BlockPainter::PaintChildren(const PaintInfo& paint_info) {
  // We may use legacy paint to paint the anonymous fieldset child. The layout
  // object for the rendered legend will be a child of that one, and has to be
  // skipped here, since it's handled by a special NG fieldset painter.
  bool may_contain_rendered_legend =
      layout_block_.IsAnonymousNGFieldsetContentWrapper();
  for (LayoutBox* child = layout_block_.FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (may_contain_rendered_legend && child->IsRenderedLegend()) {
      may_contain_rendered_legend = false;
      continue;
    }
    PaintChild(*child, paint_info);
  }
}

void BlockPainter::PaintChild(const LayoutBox& child,
                              const PaintInfo& paint_info) {
  if (child.HasSelfPaintingLayer() || child.IsColumnSpanAll())
    return;
  if (!child.IsFloating()) {
    child.Paint(paint_info);
    return;
  }
  // Paint the float now if we're in the right phase and if this is NG. NG
  // paints floats in regular tree order (the FloatingObjects list is only used
  // by legacy layout).
  if (paint_info.phase != PaintPhase::kFloat &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kTextClip)
    return;

  if (!layout_block_.IsLayoutNGObject())
    return;

  PaintInfo float_paint_info(paint_info);
  if (paint_info.phase == PaintPhase::kFloat)
    float_paint_info.phase = PaintPhase::kForeground;

  ObjectPainter(child).PaintAllPhasesAtomically(float_paint_info);
}

void BlockPainter::PaintChildrenAtomically(const OrderIterator& order_iterator,
                                           const PaintInfo& paint_info) {
  for (const LayoutBox* child = order_iterator.First(); child;
       child = order_iterator.Next()) {
    PaintAllChildPhasesAtomically(*child, paint_info);
  }
}

void BlockPainter::PaintAllChildPhasesAtomically(const LayoutBox& child,
                                                 const PaintInfo& paint_info) {
  if (!child.HasSelfPaintingLayer() && !child.IsFloating())
    ObjectPainter(child).PaintAllPhasesAtomically(paint_info);
}

void BlockPainter::PaintInlineBox(const InlineBox& inline_box,
                                  const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection)
    return;

  // Text clips are painted only for the direct inline children of the object
  // that has a text clip style on it, not block children.
  DCHECK(paint_info.phase != PaintPhase::kTextClip);

  ObjectPainter(
      *LineLayoutAPIShim::ConstLayoutObjectFrom(inline_box.GetLineLayoutItem()))
      .PaintAllPhasesAtomically(paint_info);
}

void BlockPainter::PaintScrollHitTestDisplayItem(const PaintInfo& paint_info) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());

  // Scroll hit test display items are only needed for compositing. This flag is
  // used for for printing and drag images which do not need hit testing.
  if (paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers)
    return;

  // The scroll hit test layer is in the unscrolled and unclipped space so the
  // scroll hit test layer can be enlarged beyond the clip. This will let us fix
  // crbug.com/753124 in the future where the scrolling element's border is hit
  // test differently if composited.

  const auto* fragment = paint_info.FragmentToPaint(layout_block_);
  const auto* properties = fragment ? fragment->PaintProperties() : nullptr;

  // If there is an associated scroll node, emit a scroll hit test display item.
  if (properties && properties->Scroll()) {
    DCHECK(properties->ScrollTranslation());
    // The local border box properties are used instead of the contents
    // properties so that the scroll hit test is not clipped or scrolled.
    ScopedPaintChunkProperties scroll_hit_test_properties(
        paint_info.context.GetPaintController(),
        fragment->LocalBorderBoxProperties(), layout_block_,
        DisplayItem::kScrollHitTest);
    ScrollHitTestDisplayItem::Record(paint_info.context, layout_block_,
                                     *properties->ScrollTranslation());
  }
}

DISABLE_CFI_PERF
void BlockPainter::PaintObject(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  const PaintPhase paint_phase = paint_info.phase;
  // This function implements some of the painting order algorithm (described
  // within the description of stacking context, here
  // https://www.w3.org/TR/css-position-3/#det-stacking-context). References are
  // made below to the step numbers described in that document.

  // If this block has been truncated, early-out here, because it will not be
  // displayed. A truncated block occurs when text-overflow: ellipsis is set on
  // a block, and there is not enough room to display all elements. The elements
  // that don't get shown are "Truncated".
  if (layout_block_.IsTruncated())
    return;

  // If we're *printing* the foreground, paint the URL.
  if (paint_phase == PaintPhase::kForeground && paint_info.IsPrinting()) {
    ObjectPainter(layout_block_)
        .AddPDFURLRectIfNeeded(paint_info, paint_offset);
  }

  // If we're painting our background (either 1. kBlockBackground - background
  // of the current object and non-self-painting descendants, or 2.
  // kSelfBlockBackgroundOnly -  Paint background of the current object only),
  // paint those now. This is steps #1, 2, and 4 of the CSS spec (see above).
  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    layout_block_.PaintBoxDecorationBackground(paint_info, paint_offset);
    // Record the scroll hit test after the background so background squashing
    // is not affected. Hit test order would be equivalent if this were
    // immediately before the background.
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
      PaintScrollHitTestDisplayItem(paint_info);
  }

  // If we're in any phase except *just* the self (outline or background) or a
  // mask, paint children now. This is step #5, 7, 8, and 9 of the CSS spec (see
  // above).
  if (paint_phase != PaintPhase::kSelfOutlineOnly &&
      paint_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      paint_phase != PaintPhase::kMask) {
    // Actually paint the contents.
    if (layout_block_.IsLayoutBlockFlow()) {
      // All floating descendants will be LayoutBlockFlow objects, and will get
      // painted here. That is step #5 of the CSS spec (see above).
      PaintBlockFlowContents(paint_info, paint_offset);
    } else {
      PaintContents(paint_info, paint_offset);
    }
  }

  // If we're painting the outline, paint it now. This is step #10 of the CSS
  // spec (see above).
  if (ShouldPaintSelfOutline(paint_phase))
    ObjectPainter(layout_block_).PaintOutline(paint_info, paint_offset);

  // If we're painting a visible mask, paint it now. (This does not correspond
  // to any painting order steps within the CSS spec.)
  if (paint_phase == PaintPhase::kMask &&
      layout_block_.StyleRef().Visibility() == EVisibility::kVisible) {
    layout_block_.PaintMask(paint_info, paint_offset);
  }
}

void BlockPainter::PaintBlockFlowContents(const PaintInfo& paint_info,
                                          const LayoutPoint& paint_offset) {
  DCHECK(layout_block_.IsLayoutBlockFlow());
  if (layout_block_.IsLayoutView() ||
      !paint_info.SuppressPaintingDescendants()) {
    if (!layout_block_.ChildrenInline()) {
      PaintContents(paint_info, paint_offset);
    } else if (ShouldPaintDescendantOutlines(paint_info.phase)) {
      ObjectPainter(layout_block_).PaintInlineChildrenOutlines(paint_info);
    } else {
      LineBoxListPainter(ToLayoutBlockFlow(layout_block_).LineBoxes())
          .Paint(layout_block_, paint_info, paint_offset);
    }
  }

  // If we don't have any floats to paint, or we're in the wrong paint phase,
  // then we're done for now.
  auto* floating_objects =
      ToLayoutBlockFlow(layout_block_).GetFloatingObjects();
  const PaintPhase paint_phase = paint_info.phase;
  if (!floating_objects || !(paint_phase == PaintPhase::kFloat ||
                             paint_phase == PaintPhase::kSelection ||
                             paint_phase == PaintPhase::kTextClip)) {
    return;
  }

  // LayoutNG paints floats in regular tree order, and doesn't use the
  // FloatingObjects list.
  if (layout_block_.IsLayoutNGObject())
    return;

  // If we're painting floats (not selections or textclips), change
  // the paint phase to foreground.
  PaintInfo float_paint_info(paint_info);
  if (paint_info.phase == PaintPhase::kFloat)
    float_paint_info.phase = PaintPhase::kForeground;

  // Paint all floats.
  for (const auto& floating_object : floating_objects->Set()) {
    if (!floating_object->ShouldPaint())
      continue;
    const LayoutBox* floating_layout_object =
        floating_object->GetLayoutObject();
    // TODO(wangxianzhu): Should this be a DCHECK?
    if (floating_layout_object->HasSelfPaintingLayer())
      continue;
    ObjectPainter(*floating_layout_object)
        .PaintAllPhasesAtomically(float_paint_info);
  }
}

void BlockPainter::PaintCarets(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  LocalFrame* frame = layout_block_.GetFrame();

  if (layout_block_.ShouldPaintCursorCaret())
    frame->Selection().PaintCaret(paint_info.context, paint_offset);

  if (layout_block_.ShouldPaintDragCaret()) {
    frame->GetPage()->GetDragCaret().PaintDragCaret(frame, paint_info.context,
                                                    paint_offset);
  }
}

DISABLE_CFI_PERF
bool BlockPainter::ShouldPaint(const ScopedPaintState& paint_state) const {
  // If there is no fragment to paint for this block, we still need to continue
  // the paint tree walk in case there are overflowing children that exist in
  // the current painting fragment of the painting layer. In the case we can't
  // check the overflow rect against the cull rect in the case because we don't
  // know the paint offset.
  if (!paint_state.FragmentToPaint())
    return true;

  LayoutRect overflow_rect;
  if (paint_state.GetPaintInfo().IsPrinting() &&
      layout_block_.IsAnonymousBlock() && layout_block_.ChildrenInline()) {
    // For case <a href="..."><div>...</div></a>, when layout_block_ is the
    // anonymous container of <a>, the anonymous container's visual overflow is
    // empty, but we need to continue painting to output <a>'s PDF URL rect
    // which covers the continuations, as if we included <a>'s PDF URL rect into
    // layout_block_'s visual overflow.
    Vector<LayoutRect> rects;
    layout_block_.AddElementVisualOverflowRects(rects, LayoutPoint());
    overflow_rect = UnionRect(rects);
  }
  overflow_rect.Unite(layout_block_.VisualOverflowRect());

  bool uses_composited_scrolling = layout_block_.HasOverflowModel() &&
                                   layout_block_.UsesCompositedScrolling();

  if (uses_composited_scrolling) {
    LayoutRect layout_overflow_rect = layout_block_.LayoutOverflowRect();
    overflow_rect.Unite(layout_overflow_rect);
  }
  layout_block_.FlipForWritingMode(overflow_rect);

  // Scrolling is applied in physical space, which is why it is after the flip
  // above.
  if (uses_composited_scrolling) {
    overflow_rect.Move(-layout_block_.ScrolledContentOffset());
  }

  return paint_state.LocalRectIntersectsCullRect(overflow_rect);
}

void BlockPainter::PaintContents(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  DCHECK(!layout_block_.ChildrenInline());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  layout_block_.PaintChildren(paint_info_for_descendants, paint_offset);
}

}  // namespace blink
