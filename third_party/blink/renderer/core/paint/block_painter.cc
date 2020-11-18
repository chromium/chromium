// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/block_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/line_box_list_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"

namespace blink {

DISABLE_CFI_PERF
void BlockPainter::Paint(const PaintInfo& paint_info) {
  ScopedPaintState paint_state(layout_block_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  DCHECK(!layout_block_.ChildPaintBlockedByDisplayLock() ||
         paint_info.DescendantPaintingBlocked());

  auto paint_offset = paint_state.PaintOffset();
  auto& local_paint_info = paint_state.MutablePaintInfo();
  PaintPhase original_phase = local_paint_info.phase;

  if (original_phase == PaintPhase::kOutline) {
    local_paint_info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    local_paint_info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    // With CompositeAfterPaint we need to call PaintObject twice: once for the
    // background painting that does not scroll, and a second time for the
    // background painting that scrolls.
    // Without CompositeAfterPaint, this happens as the main graphics layer
    // paints the background, and then the scrolling contents graphics layer
    // paints the background.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      auto paint_location = layout_block_.GetBackgroundPaintLocation();
      if (!(paint_location & kBackgroundPaintInGraphicsLayer))
        local_paint_info.SetSkipsBackground(true);
      layout_block_.PaintObject(local_paint_info, paint_offset);
      local_paint_info.SetSkipsBackground(false);

      if (paint_location & kBackgroundPaintInScrollingContents) {
        local_paint_info.SetIsPaintingScrollingBackground(true);
        layout_block_.PaintObject(local_paint_info, paint_offset);
        local_paint_info.SetIsPaintingScrollingBackground(false);
      }
    } else {
      layout_block_.PaintObject(local_paint_info, paint_offset);
    }
    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      local_paint_info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase == PaintPhase::kMask) {
    layout_block_.PaintObject(local_paint_info, paint_offset);
  } else if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
             original_phase != PaintPhase::kSelfOutlineOnly &&
             // For now all scrollers with overlay overflow controls are
             // self-painting layers, so we don't need to traverse descendants
             // here.
             original_phase != PaintPhase::kOverlayOverflowControls) {
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
              paint_info.context.GetPaintController(), *overflow_clip,
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

  // We paint scrollbars after we painted the other things, so that the
  // scrollbars will sit above them.
  local_paint_info.phase = original_phase;
  if (auto* scrollable_area = layout_block_.GetScrollableArea()) {
    ScrollableAreaPainter(*scrollable_area)
        .PaintOverflowControls(local_paint_info, RoundedIntPoint(paint_offset));
  }
}

void BlockPainter::PaintChildren(const PaintInfo& paint_info) {
  if (paint_info.DescendantPaintingBlocked())
    return;

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
      paint_info.phase != PaintPhase::kSelectionDragImage &&
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
  if (paint_info.DescendantPaintingBlocked())
    return;
  for (const LayoutBox* child = order_iterator.First(); child;
       child = order_iterator.Next()) {
    PaintAllChildPhasesAtomically(*child, paint_info);
  }
}

void BlockPainter::PaintAllChildPhasesAtomically(const LayoutBox& child,
                                                 const PaintInfo& paint_info) {
  if (paint_info.DescendantPaintingBlocked())
    return;
  if (!child.HasSelfPaintingLayer() && !child.IsFloating())
    ObjectPainter(child).PaintAllPhasesAtomically(paint_info);
}

void BlockPainter::PaintInlineBox(const InlineBox& inline_box,
                                  const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kForcedColorsModeBackplate &&
      paint_info.phase != PaintPhase::kSelectionDragImage)
    return;

  // Text clips are painted only for the direct inline children of the object
  // that has a text clip style on it, not block children.
  DCHECK(paint_info.phase != PaintPhase::kTextClip);

  ObjectPainter(
      *LineLayoutAPIShim::ConstLayoutObjectFrom(inline_box.GetLineLayoutItem()))
      .PaintAllPhasesAtomically(paint_info);
}

DISABLE_CFI_PERF
void BlockPainter::PaintObject(const PaintInfo& paint_info,
                               const PhysicalOffset& paint_offset) {
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

  ScopedPaintTimingDetectorBlockPaintHook
      scoped_paint_timing_detector_block_paint_hook;
  if (paint_info.phase == PaintPhase::kForeground) {
    scoped_paint_timing_detector_block_paint_hook.EmplaceIfNeeded(
        layout_block_,
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
  }
  // If we're *printing or creating a paint preview of* the foreground, paint
  // the URL.
  if (paint_phase == PaintPhase::kForeground &&
      paint_info.ShouldAddUrlMetadata()) {
    ObjectPainter(layout_block_).AddURLRectIfNeeded(paint_info, paint_offset);
  }

  // If we're painting our background (either 1. kBlockBackground - background
  // of the current object and non-self-painting descendants, or 2.
  // kSelfBlockBackgroundOnly -  Paint background of the current object only),
  // paint those now. This is steps #1, 2, and 4 of the CSS spec (see above).
  if (ShouldPaintSelfBlockBackground(paint_phase))
    layout_block_.PaintBoxDecorationBackground(paint_info, paint_offset);

  // Draw a backplate behind all text if in forced colors mode.
  if (paint_phase == PaintPhase::kForcedColorsModeBackplate &&
      layout_block_.GetFrame()->GetDocument()->InForcedColorsMode() &&
      layout_block_.ChildrenInline()) {
    LineBoxListPainter(To<LayoutBlockFlow>(layout_block_).LineBoxes())
        .PaintBackplate(layout_block_, paint_info, paint_offset);
  }

  // If we're in any phase except *just* the self (outline or background) or a
  // mask, paint children now. This is step #5, 7, 8, and 9 of the CSS spec (see
  // above).
  if (paint_phase != PaintPhase::kSelfOutlineOnly &&
      paint_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      paint_phase != PaintPhase::kMask &&
      !paint_info.DescendantPaintingBlocked()) {
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
                                          const PhysicalOffset& paint_offset) {
  DCHECK(layout_block_.IsLayoutBlockFlow());
  if (!layout_block_.ChildrenInline()) {
    PaintContents(paint_info, paint_offset);
  } else if (ShouldPaintDescendantOutlines(paint_info.phase)) {
    ObjectPainter(layout_block_).PaintInlineChildrenOutlines(paint_info);
  } else {
    LineBoxListPainter(To<LayoutBlockFlow>(layout_block_).LineBoxes())
        .Paint(layout_block_, paint_info, paint_offset);
  }

  // If we don't have any floats to paint, or we're in the wrong paint phase,
  // then we're done for now.
  auto* floating_objects =
      To<LayoutBlockFlow>(layout_block_).GetFloatingObjects();
  const PaintPhase paint_phase = paint_info.phase;
  if (!floating_objects || !(paint_phase == PaintPhase::kFloat ||
                             paint_phase == PaintPhase::kSelectionDragImage ||
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
                               const PhysicalOffset& paint_offset) {
  LocalFrame* frame = layout_block_.GetFrame();

  if (layout_block_.ShouldPaintCursorCaret())
    frame->Selection().PaintCaret(paint_info.context, paint_offset);

  if (layout_block_.ShouldPaintDragCaret()) {
    frame->GetPage()->GetDragCaret().PaintDragCaret(frame, paint_info.context,
                                                    paint_offset);
  }
}

PhysicalRect BlockPainter::OverflowRectForCullRectTesting(
    bool is_printing) const {
  PhysicalRect overflow_rect;
  if (is_printing && layout_block_.IsAnonymousBlock() &&
      layout_block_.ChildrenInline()) {
    // For case <a href="..."><div>...</div></a>, when layout_block_ is the
    // anonymous container of <a>, the anonymous container's visual overflow is
    // empty, but we need to continue painting to output <a>'s PDF URL rect
    // which covers the continuations, as if we included <a>'s PDF URL rect into
    // layout_block_'s visual overflow.
    auto rects = layout_block_.OutlineRects(
        PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
    overflow_rect = UnionRect(rects);
  }
  overflow_rect.Unite(layout_block_.PhysicalVisualOverflowRect());

  bool include_layout_overflow =
      layout_block_.ScrollsOverflow() &&
      (layout_block_.UsesCompositedScrolling() ||
       RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  if (include_layout_overflow) {
    overflow_rect.Unite(layout_block_.PhysicalLayoutOverflowRect());
    overflow_rect.Move(
        -PhysicalOffset(layout_block_.PixelSnappedScrolledContentOffset()));
  }
  return overflow_rect;
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

  return paint_state.LocalRectIntersectsCullRect(
      OverflowRectForCullRectTesting(paint_state.GetPaintInfo().IsPrinting()));
}

void BlockPainter::PaintContents(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset) {
  DCHECK(!layout_block_.ChildrenInline());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  layout_block_.PaintChildren(paint_info_for_descendants, paint_offset);
}

}  // namespace blink
