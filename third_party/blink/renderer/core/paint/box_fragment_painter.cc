// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/outline_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/box_background_paint_context.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/box_decoration_data.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/fieldset_painter.h"
#include "third_party/blink/renderer/core/paint/fragment_painter.h"
#include "third_party/blink/renderer/core/paint/frame_set_painter.h"
#include "third_party/blink/renderer/core/paint/inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/mathml_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/table_painters.h"
#include "third_party/blink/renderer/core/paint/text_combine_painter.h"
#include "third_party/blink/renderer/core/paint/text_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/theme_painter.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/url_metadata_utils.h"
#include "third_party/blink/renderer/core/paint/view_painter.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"

namespace blink {

namespace {

inline bool HasSelection(const LayoutObject* layout_object) {
  return layout_object->GetSelectionState() != SelectionState::kNone;
}

inline bool IsVisibleToPaint(const PhysicalFragment& fragment,
                             const ComputedStyle& style) {
  if (fragment.IsHiddenForPaint())
    return false;
  if (style.UsedVisibility() != EVisibility::kVisible) {
    auto display = style.Display();
    // Hidden section/row backgrounds still paint into cells.
    if (display != EDisplay::kTableRowGroup && display != EDisplay::kTableRow &&
        display != EDisplay::kTableColumn &&
        display != EDisplay::kTableColumnGroup) {
      return false;
    }
  }

  // When |LineTruncator| sets |IsHiddenForPaint|, it sets to the fragment in
  // the line. However, when it has self-painting layer, the fragment stored in
  // |LayoutBlockFlow| will be painted. Check |IsHiddenForPaint| of the fragment
  // in the inline formatting context.
  if (fragment.IsAtomicInline() && fragment.HasSelfPaintingLayer())
      [[unlikely]] {
    const LayoutObject* layout_object = fragment.GetLayoutObject();
    if (layout_object->IsInLayoutNGInlineFormattingContext()) {
      InlineCursor cursor;
      cursor.MoveTo(*layout_object);
      if (cursor && cursor.Current().IsHiddenForPaint())
        return false;
    }
  }

  return true;
}

inline bool IsVisibleToPaint(const FragmentItem& item,
                             const ComputedStyle& style) {
  return !item.IsHiddenForPaint() &&
         style.UsedVisibility() == EVisibility::kVisible;
}

inline bool IsVisibleToHitTest(const ComputedStyle& style,
                               const HitTestRequest& request) {
  return request.IgnorePointerEventsNone() ||
         style.UsedPointerEvents() != EPointerEvents::kNone;
}

inline bool IsVisibleToHitTest(const FragmentItem& item,
                               const HitTestRequest& request) {
  const ComputedStyle& style = item.Style();
  if (!item.IsSvgText()) {
    return IsVisibleToPaint(item, style) && IsVisibleToHitTest(style, request);
  }

  if (item.IsHiddenForPaint())
    return false;
  PointerEventsHitRules hit_rules(PointerEventsHitRules::kSvgTextHitTesting,
                                  request, style.UsedPointerEvents());
  if (hit_rules.require_visible &&
      style.UsedVisibility() != EVisibility::kVisible) {
    return false;
  }
  if (hit_rules.can_hit_bounding_box ||
      (hit_rules.can_hit_stroke &&
       (style.HasStroke() || !hit_rules.require_stroke)) ||
      (hit_rules.can_hit_fill && (style.HasFill() || !hit_rules.require_fill)))
    return IsVisibleToHitTest(style, request);
  return false;
}

inline bool IsVisibleToHitTest(const PhysicalFragment& fragment,
                               const HitTestRequest& request) {
  const ComputedStyle& style = fragment.Style();
  return IsVisibleToPaint(fragment, style) &&
         IsVisibleToHitTest(style, request);
}

// Hit tests inline ancestor elements of |fragment| who do not have their own
// box fragments.
// @param physical_offset Physical offset of |fragment| in the paint layer.
bool HitTestCulledInlineAncestors(
    HitTestResult& result,
    const InlineCursor& parent_cursor,
    const LayoutObject* current,
    const LayoutObject* limit,
    const InlineCursorPosition& previous_sibling,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset fallback_accumulated_offset) {
  DCHECK(current != limit && current->IsDescendantOf(limit));

  // Check ancestors only when |current| is the first fragment in this line.
  if (previous_sibling && current == previous_sibling.GetLayoutObject())
    return false;

  for (LayoutObject* parent = current->Parent(); parent && parent != limit;
       current = parent, parent = parent->Parent()) {
    // |culled_parent| is a culled inline element to be hit tested, since it's
    // "between" |fragment| and |fragment->Parent()| but doesn't have its own
    // box fragment.
    // To ensure the correct hit test ordering, |culled_parent| must be hit
    // tested only once after all of its descendants are hit tested:
    // - Shortcut: when |current_layout_object| is the only child (of
    // |culled_parent|), since it's just hit tested, we can safely hit test its
    // parent;
    // - General case: we hit test |culled_parent| only when it is not an
    // ancestor of |previous_sibling|; otherwise, |previous_sibling| has to be
    // hit tested first.
    // TODO(crbug.com/849331): It's wrong for bidi inline fragmentation. Fix it.
    const bool has_sibling =
        current->PreviousSibling() || current->NextSibling();
    if (has_sibling && previous_sibling &&
        previous_sibling.GetLayoutObject()->IsDescendantOf(parent))
      break;

    if (auto* parent_layout_inline = DynamicTo<LayoutInline>(parent)) {
      if (parent_layout_inline->HitTestCulledInline(result, hit_test_location,
                                                    fallback_accumulated_offset,
                                                    parent_cursor)) {
        return true;
      }
    }
  }

  return false;
}

bool HitTestCulledInlineAncestors(HitTestResult& result,
                                  const PhysicalBoxFragment& container,
                                  const InlineCursor& parent_cursor,
                                  const FragmentItem& item,
                                  const InlineCursorPosition& previous_sibling,
                                  const HitTestLocation& hit_test_location,
                                  const PhysicalOffset& physical_offset) {
  // Ellipsis can appear under a different parent from the ellipsized object
  // that it can confuse culled inline logic.
  if (item.IsEllipsis()) [[unlikely]] {
    return false;
  }
  // To be passed as |accumulated_offset| to LayoutInline::HitTestCulledInline,
  // where it equals the physical offset of the containing block in paint layer.
  const PhysicalOffset fallback_accumulated_offset =
      physical_offset - item.OffsetInContainerFragment();
  return HitTestCulledInlineAncestors(
      result, parent_cursor, item.GetLayoutObject(),
      // Limit the traversal up to the container fragment, or its container if
      // the fragment is not a CSSBox.
      container.GetSelfOrContainerLayoutObject(), previous_sibling,
      hit_test_location, fallback_accumulated_offset);
}

// Returns a vector of backplates that surround the paragraphs of text within
// line_boxes.
//
// This function traverses descendants of an inline formatting context in
// pre-order DFS and build up backplates behind inline text boxes, each split at
// the paragraph level. Store the results in paragraph_backplates.
Vector<PhysicalRect> BuildBackplate(InlineCursor* descendants,
                                    const PhysicalOffset& paint_offset) {
  // The number of consecutive forced breaks that split the backplate by
  // paragraph.
  static constexpr int kMaxConsecutiveLineBreaks = 2;

  struct Backplates {
    STACK_ALLOCATED();

   public:
    void AddTextRect(const PhysicalRect& box_rect) {
      if (consecutive_line_breaks >= kMaxConsecutiveLineBreaks) {
        // This is a paragraph point.
        paragraph_backplates.push_back(current_backplate);
        current_backplate = PhysicalRect();
      }
      consecutive_line_breaks = 0;

      current_backplate.Unite(box_rect);
    }

    void AddLineBreak() { consecutive_line_breaks++; }

    Vector<PhysicalRect> paragraph_backplates;
    PhysicalRect current_backplate;
    int consecutive_line_breaks = 0;
  } backplates;

  // Build up and paint backplates of all child inline text boxes. We are not
  // able to simply use the linebox rect to compute the backplate because the
  // backplate should only be painted for inline text and not for atomic
  // inlines.
  for (; *descendants; descendants->MoveToNext()) {
    if (const FragmentItem* child_item = descendants->CurrentItem()) {
      if (child_item->IsHiddenForPaint())
        continue;
      if (child_item->IsText()) {
        if (child_item->IsLineBreak()) {
          backplates.AddLineBreak();
          continue;
        }

        PhysicalRect box_rect(
            child_item->OffsetInContainerFragment() + paint_offset,
            child_item->Size());
        backplates.AddTextRect(box_rect);
      }
      continue;
    }
    NOTREACHED_IN_MIGRATION();
  }

  if (!backplates.current_backplate.IsEmpty())
    backplates.paragraph_backplates.push_back(backplates.current_backplate);
  return backplates.paragraph_backplates;
}

bool HitTestAllPhasesInFragment(const PhysicalBoxFragment& fragment,
                                const HitTestLocation& hit_test_location,
                                PhysicalOffset accumulated_offset,
                                HitTestResult* result) {
  // Hit test all phases of inline blocks, inline tables, replaced elements and
  // non-positioned floats as if they created their own (pseudo- [1]) stacking
  // context. https://www.w3.org/TR/CSS22/zindex.html#painting-order
  //
  // [1] As if it creates a new stacking context, but any positioned descendants
  // and descendants which actually create a new stacking context should be
  // considered part of the parent stacking context, not this new one.

  if (!fragment.CanTraverse()) {
    if (!fragment.IsFirstForNode() && !CanPaintMultipleFragments(fragment))
      return false;
    return fragment.GetMutableLayoutObject()->HitTestAllPhases(
        *result, hit_test_location, accumulated_offset);
  }

  if (!fragment.MayIntersect(*result, hit_test_location, accumulated_offset))
    return false;

  return BoxFragmentPainter(To<PhysicalBoxFragment>(fragment))
      .HitTestAllPhases(*result, hit_test_location, accumulated_offset);
}

bool NodeAtPointInFragment(const PhysicalBoxFragment& fragment,
                           const HitTestLocation& hit_test_location,
                           PhysicalOffset accumulated_offset,
                           HitTestPhase phase,
                           HitTestResult* result) {
  if (!fragment.CanTraverse()) {
    if (!fragment.IsFirstForNode() && !CanPaintMultipleFragments(fragment))
      return false;
    return fragment.GetMutableLayoutObject()->NodeAtPoint(
        *result, hit_test_location, accumulated_offset, phase);
  }

  if (!fragment.MayIntersect(*result, hit_test_location, accumulated_offset))
    return false;

  return BoxFragmentPainter(fragment).NodeAtPoint(*result, hit_test_location,
                                                  accumulated_offset, phase);
}

// Return an ID for this fragmentainer, which is unique within the fragmentation
// context. We need to provide this ID when block-fragmenting, so that we can
// cache the painting of each individual fragment.
unsigned FragmentainerUniqueIdentifier(const PhysicalBoxFragment& fragment) {
  if (const auto* break_token = fragment.GetBreakToken()) {
    return break_token->SequenceNumber() + 1;
  }
  return 0;
}

bool ShouldPaintCursorCaret(const PhysicalBoxFragment& fragment) {
  return fragment.GetLayoutObject()->GetFrame()->Selection().ShouldPaintCaret(
      fragment);
}

bool ShouldPaintDragCaret(const PhysicalBoxFragment& fragment) {
  return fragment.GetLayoutObject()
      ->GetFrame()
      ->GetPage()
      ->GetDragCaret()
      .ShouldPaintCaret(fragment);
}

bool ShouldPaintCarets(const PhysicalBoxFragment& fragment) {
  return ShouldPaintCursorCaret(fragment) || ShouldPaintDragCaret(fragment);
}

PaintInfo FloatPaintInfo(const PaintInfo& paint_info) {
  PaintInfo float_paint_info(paint_info);
  if (paint_info.phase == PaintPhase::kFloat)
    float_paint_info.phase = PaintPhase::kForeground;
  return float_paint_info;
}

// Helper function for painting a child fragment, when there's any likelihood
// that we need legacy fallback. If it's guaranteed that legacy fallback won't
// be necessary, on the other hand, there's no need to call this function. In
// such cases, call sites may just as well invoke BoxFragmentPainter::Paint()
// on their own.
void PaintFragment(const PhysicalBoxFragment& fragment,
                   const PaintInfo& paint_info) {
  if (fragment.CanTraverse()) {
    BoxFragmentPainter(fragment).Paint(paint_info);
    return;
  }

  if (!fragment.IsFirstForNode() && !CanPaintMultipleFragments(fragment))
    return;

  // We are about to enter legacy paint code. This means that the node is
  // monolithic. However, that doesn't necessarily mean that it only has one
  // fragment. Repeated table headers / footers may cause multiple fragments,
  // for instance. Set the FragmentData, to use the right paint offset.
  PaintInfo modified_paint_info(paint_info);
  modified_paint_info.SetFragmentDataOverride(fragment.GetFragmentData());

  auto* layout_object = fragment.GetLayoutObject();
  DCHECK(layout_object);
  if (fragment.IsPaintedAtomically() && layout_object->IsLayoutReplaced()) {
    ObjectPainter(*layout_object).PaintAllPhasesAtomically(modified_paint_info);
  } else {
    layout_object->Paint(modified_paint_info);
  }
}

}  // anonymous namespace

PhysicalRect BoxFragmentPainter::InkOverflowIncludingFilters() const {
  if (box_item_)
    return box_item_->SelfInkOverflowRect();
  const auto& fragment = GetPhysicalFragment();
  DCHECK(!fragment.IsInlineBox());
  return To<LayoutBox>(fragment.GetLayoutObject())
      ->VisualOverflowRectIncludingFilters();
}

InlinePaintContext& BoxFragmentPainter::EnsureInlineContext() {
  if (!inline_context_)
    inline_context_ = &inline_context_storage_.emplace();
  return *inline_context_;
}

void BoxFragmentPainter::Paint(const PaintInfo& paint_info) {
  if (GetPhysicalFragment().IsHiddenForPaint()) {
    return;
  }
  auto* layout_object = box_fragment_.GetLayoutObject();
  if (GetPhysicalFragment().IsPaintedAtomically() &&
      !box_fragment_.HasSelfPaintingLayer() &&
      paint_info.phase != PaintPhase::kOverlayOverflowControls) {
    PaintAllPhasesAtomically(paint_info);
  } else if (layout_object && layout_object->IsSVGForeignObject()) {
    ScopedSVGPaintState paint_state(*layout_object, paint_info);
    PaintTiming::From(layout_object->GetDocument()).MarkFirstContentfulPaint();
    PaintInternal(paint_info);
  } else {
    PaintInternal(paint_info);
  }
}

void BoxFragmentPainter::PaintInternal(const PaintInfo& paint_info) {
  // Avoid initialization of Optional ScopedPaintState::chunk_properties_
  // and ScopedPaintState::adjusted_paint_info_.
  STACK_UNINITIALIZED ScopedPaintState paint_state(box_fragment_, paint_info);
  if (!ShouldPaint(paint_state))
    return;

  if (!box_fragment_.IsFirstForNode() &&
      !CanPaintMultipleFragments(box_fragment_))
    return;

  PaintInfo& info = paint_state.MutablePaintInfo();
  const PhysicalOffset paint_offset = paint_state.PaintOffset();
  const PaintPhase original_phase = info.phase;
  bool painted_overflow_controls = false;

  // For text-combine-upright:all, we need to realize canvas here for scaling
  // to fit text content in 1em and shear for "font-style: oblique -15deg".
  std::optional<DrawingRecorder> recorder;
  std::optional<GraphicsContextStateSaver> graphics_context_state_saver;
  const auto* const text_combine =
      DynamicTo<LayoutTextCombine>(box_fragment_.GetLayoutObject());
  if (text_combine) [[unlikely]] {
    if (text_combine->NeedsAffineTransformInPaint()) {
      if (original_phase == PaintPhase::kForeground)
        PaintCaretsIfNeeded(paint_state, paint_info, paint_offset);
      if (!paint_info.context.InDrawingRecorder()) {
        if (DrawingRecorder::UseCachedDrawingIfPossible(
                paint_info.context, GetDisplayItemClient(), paint_info.phase))
          return;
        recorder.emplace(paint_info.context, GetDisplayItemClient(),
                         paint_info.phase,
                         text_combine->VisualRectForPaint(paint_offset));
      }
      graphics_context_state_saver.emplace(paint_info.context);
      paint_info.context.ConcatCTM(
          text_combine->ComputeAffineTransformForPaint(paint_offset));
    }
  }

  ScopedPaintTimingDetectorBlockPaintHook
      scoped_paint_timing_detector_block_paint_hook;
  if (original_phase == PaintPhase::kForeground &&
      box_fragment_.GetLayoutObject()->IsBox()) {
    scoped_paint_timing_detector_block_paint_hook.EmplaceIfNeeded(
        To<LayoutBox>(*box_fragment_.GetLayoutObject()),
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
  }

  if (original_phase == PaintPhase::kOutline) {
    info.phase = PaintPhase::kDescendantOutlinesOnly;
  } else if (ShouldPaintSelfBlockBackground(original_phase)) {
    info.phase = PaintPhase::kSelfBlockBackgroundOnly;
    // We need to call PaintObject twice: one for painting background in the
    // border box space, and the other for painting background in the scrolling
    // contents space.
    const LayoutBox& box = To<LayoutBox>(*box_fragment_.GetLayoutObject());
    auto paint_location = box.GetBackgroundPaintLocation();
    if (!(paint_location & kBackgroundPaintInBorderBoxSpace))
      info.SetSkipsBackground(true);
    PaintObject(info, paint_offset);
    info.SetSkipsBackground(false);

    if ((RuntimeEnabledFeatures::HitTestOpaquenessEnabled() &&
         // We need to record hit test data for the scrolling contents.
         box.ScrollsOverflow()) ||
        (paint_location & kBackgroundPaintInContentsSpace)) {
      if (!(paint_location & kBackgroundPaintInContentsSpace)) {
        DCHECK(RuntimeEnabledFeatures::HitTestOpaquenessEnabled());
        info.SetSkipsBackground(true);
      }
      // If possible, paint overflow controls before scrolling background to
      // make it easier to merge scrolling background and scrolling contents
      // into the same layer. The function checks if it's appropriate to paint
      // overflow controls now.
      painted_overflow_controls = PaintOverflowControls(info, paint_offset);

      info.SetIsPaintingBackgroundInContentsSpace(true);
      PaintObject(info, paint_offset);
      info.SetIsPaintingBackgroundInContentsSpace(false);
      info.SetSkipsBackground(false);
    }

    if (ShouldPaintDescendantBlockBackgrounds(original_phase))
      info.phase = PaintPhase::kDescendantBlockBackgroundsOnly;
  }

  if (original_phase != PaintPhase::kSelfBlockBackgroundOnly &&
      original_phase != PaintPhase::kSelfOutlineOnly &&
      // kOverlayOverflowControls is for the current object itself, so we don't
      // need to traverse descendants here.
      original_phase != PaintPhase::kOverlayOverflowControls) {
    if (original_phase == PaintPhase::kMask ||
        !box_fragment_.GetLayoutObject()->IsBox()) {
      PaintObject(info, paint_offset);
    } else {
      ScopedBoxContentsPaintState contents_paint_state(
          paint_state, To<LayoutBox>(*box_fragment_.GetLayoutObject()));
      PaintObject(contents_paint_state.GetPaintInfo(),
                  contents_paint_state.PaintOffset());
    }
  }

  // If the caret's node's fragment's containing block is this block, and
  // the paint action is PaintPhaseForeground, then paint the caret.
  if (original_phase == PaintPhase::kForeground) {
    if (!recorder) [[likely]] {
      DCHECK(!text_combine || !text_combine->NeedsAffineTransformInPaint());
      PaintCaretsIfNeeded(paint_state, paint_info, paint_offset);
    }
  }

  if (ShouldPaintSelfOutline(original_phase)) {
    info.phase = PaintPhase::kSelfOutlineOnly;
    PaintObject(info, paint_offset);
  }

  if (text_combine && TextCombinePainter::ShouldPaint(*text_combine))
      [[unlikely]] {
    if (recorder) {
      // Paint text decorations and emphasis marks without scaling and share.
      DCHECK(text_combine->NeedsAffineTransformInPaint());
      graphics_context_state_saver->Restore();
    } else if (!paint_info.context.InDrawingRecorder()) {
      if (DrawingRecorder::UseCachedDrawingIfPossible(
              paint_info.context, GetDisplayItemClient(), paint_info.phase))
        return;
      recorder.emplace(paint_info.context, GetDisplayItemClient(),
                       paint_info.phase,
                       text_combine->VisualRectForPaint(paint_offset));
    }
    TextCombinePainter::Paint(info, paint_offset, *text_combine);
  }

  // If we haven't painted overflow controls, paint scrollbars after we painted
  // the other things, so that the scrollbars will sit above them.
  if (!painted_overflow_controls) {
    info.phase = original_phase;
    PaintOverflowControls(info, paint_offset);
  }
}

bool BoxFragmentPainter::PaintOverflowControls(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  if (!box_fragment_.IsScrollContainer())
    return false;

  return ScrollableAreaPainter(
             *GetPhysicalFragment().Layer()->GetScrollableArea())
      .PaintOverflowControls(paint_info, paint_offset,
                             box_fragment_.GetFragmentData());
}

void BoxFragmentPainter::RecordScrollHitTestData(
    const PaintInfo& paint_info,
    const DisplayItemClient& background_client) {
  if (!box_fragment_.GetLayoutObject()->IsBox())
    return;
  BoxPainter(To<LayoutBox>(*box_fragment_.GetLayoutObject()))
      .RecordScrollHitTestData(paint_info, background_client,
                               box_fragment_.GetFragmentData());
}

bool BoxFragmentPainter::ShouldRecordHitTestData(const PaintInfo& paint_info) {
  // Some conditions are checked in ObjectPainter::RecordHitTestData().
  // Table rows/sections do not participate in hit testing.
  return !GetPhysicalFragment().IsTableRow() &&
         !GetPhysicalFragment().IsTableSection();
}

void BoxFragmentPainter::PaintObject(const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset,
                                     bool suppress_box_decoration_background) {
  const PaintPhase paint_phase = paint_info.phase;
  const PhysicalBoxFragment& fragment = GetPhysicalFragment();
  if (fragment.IsFrameSet()) {
    FrameSetPainter(fragment, display_item_client_)
        .PaintObject(paint_info, paint_offset);
    return;
  }
  const ComputedStyle& style = fragment.Style();
  const bool is_visible = IsVisibleToPaint(fragment, style);
  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    if (is_visible) {
      PaintBoxDecorationBackground(paint_info, paint_offset,
                                   suppress_box_decoration_background);
    }
    // We're done. We don't bother painting any children.
    if (paint_phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (paint_phase == PaintPhase::kMask && is_visible) {
    PaintMask(paint_info, paint_offset);
    return;
  }

  if (paint_phase == PaintPhase::kForeground) {
    // PaintLineBoxes() calls AddURLRectsForInlineChildrenRecursively(). So we
    // don't need to call AddURLRectIfNeeded() for LayoutInline.
    if (paint_info.ShouldAddUrlMetadata()) {
      const auto* layout_object = fragment.GetLayoutObject();
      if (layout_object && !layout_object->IsLayoutInline()) {
        FragmentPainter(fragment, GetDisplayItemClient())
            .AddURLRectIfNeeded(paint_info, paint_offset);
      }
    }
    if (is_visible && fragment.HasExtraMathMLPainting())
      MathMLPainter(fragment).Paint(paint_info, paint_offset);
  }

  // Paint children.
  if (paint_phase != PaintPhase::kSelfOutlineOnly &&
      (!fragment.Children().empty() || fragment.HasItems() ||
       inline_box_cursor_) &&
      !paint_info.DescendantPaintingBlocked()) {
    if (is_visible && paint_phase == PaintPhase::kForeground &&
        fragment.IsCSSBox() && style.HasColumnRule()) [[unlikely]] {
      PaintColumnRules(paint_info, paint_offset);
    }

    if (paint_phase != PaintPhase::kFloat) {
      if (inline_box_cursor_) [[unlikely]] {
        // Use the descendants cursor for this painter if it is given.
        // Self-painting inline box paints only parts of the container block.
        // Adjust |paint_offset| because it is the offset of the inline box, but
        // |descendants_| has offsets to the contaiing block.
        DCHECK(box_item_);
        InlineCursor descendants = inline_box_cursor_->CursorForDescendants();
        const PhysicalOffset paint_offset_to_inline_formatting_context =
            paint_offset - box_item_->OffsetInContainerFragment();
        PaintInlineItems(paint_info.ForDescendants(),
                         paint_offset_to_inline_formatting_context,
                         box_item_->OffsetInContainerFragment(), &descendants);
      } else if (items_) {
        DCHECK(fragment.IsBlockFlow());
        PaintLineBoxes(paint_info, paint_offset);
      } else if (fragment.IsPaginatedRoot()) {
        PaintCurrentPageContainer(paint_info);
      } else if (!fragment.IsInlineFormattingContext()) {
        PaintBlockChildren(paint_info, paint_offset);
      }
    }

    if (paint_phase == PaintPhase::kFloat ||
        paint_phase == PaintPhase::kSelectionDragImage ||
        paint_phase == PaintPhase::kTextClip) {
      if (fragment.HasFloatingDescendantsForPaint())
        PaintFloats(paint_info);
    }
  }

  if (!is_visible)
    return;

  // Collapsed borders paint *after* children have painted their backgrounds.
  if (box_fragment_.IsTable() &&
      paint_phase == PaintPhase::kDescendantBlockBackgroundsOnly) {
    TablePainter(box_fragment_)
        .PaintCollapsedBorders(paint_info, paint_offset,
                               VisualRect(paint_offset));
  }

  if (ShouldPaintSelfOutline(paint_phase)) {
    if (HasPaintedOutline(style, fragment.GetNode())) {
      FragmentPainter(fragment, GetDisplayItemClient())
          .PaintOutline(paint_info, paint_offset, style);
    }
  }
}

void BoxFragmentPainter::PaintCaretsIfNeeded(
    const ScopedPaintState& paint_state,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  if (!ShouldPaintCarets(box_fragment_))
    return;

  // Apply overflow clip if needed.
  // reveal-caret-of-multiline-contenteditable.html needs this.
  std::optional<ScopedPaintChunkProperties> paint_chunk_properties;
  if (const auto* fragment = paint_state.FragmentToPaint()) {
    if (const auto* properties = fragment->PaintProperties()) {
      if (const auto* overflow_clip = properties->OverflowClip()) {
        paint_chunk_properties.emplace(
            paint_info.context.GetPaintController(), *overflow_clip,
            *box_fragment_.GetLayoutObject(), DisplayItem::kCaret);
      }
    }
  }

  LocalFrame* frame = box_fragment_.GetLayoutObject()->GetFrame();
  if (ShouldPaintCursorCaret(box_fragment_))
    frame->Selection().PaintCaret(paint_info.context, paint_offset);

  if (ShouldPaintDragCaret(box_fragment_)) {
    frame->GetPage()->GetDragCaret().PaintDragCaret(frame, paint_info.context,
                                                    paint_offset);
  }
}

void BoxFragmentPainter::PaintLineBoxes(const PaintInfo& paint_info,
                                        const PhysicalOffset& paint_offset) {
  const LayoutObject* layout_object = box_fragment_.GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsLayoutBlock());
  DCHECK(box_fragment_.IsInlineFormattingContext());

  // When the layout-tree gets into a bad state, we can end up trying to paint
  // a fragment with inline children, without a paint fragment. See:
  // http://crbug.com/1022545
  if (!items_ || layout_object->NeedsLayout()) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  // MathML operators paint text (for example enlarged/stretched) content
  // themselves using MathMLPainter.
  if (box_fragment_.IsMathMLOperator()) [[unlikely]] {
    return;
  }

  // Trying to rule out a null GraphicsContext, see: https://crbug.com/1040298
  CHECK(&paint_info.context);

  // Check if there were contents to be painted and return early if none.
  // The union of |ContentsInkOverflow()| and |LocalRect()| covers the rect to
  // check, in both cases of:
  // 1. Painting non-scrolling contents.
  // 2. Painting scrolling contents.
  // For 1, check with |ContentsInkOverflow()|, except when there is no
  // overflow, in which case check with |LocalRect()|. For 2, check with
  // |ScrollableOverflow()|, but this can be approximiated with
  // |ContentsInkOverflow()|.
  PhysicalRect content_ink_rect = box_fragment_.LocalRect();
  content_ink_rect.Unite(box_fragment_.ContentsInkOverflowRect());
  if (!paint_info.IntersectsCullRect(content_ink_rect, paint_offset)) {
    return;
  }

  DCHECK(items_);
  EnsureInlineContext();
  InlineCursor children(box_fragment_, *items_);
  std::optional<ScopedSVGPaintState> paint_state;
  if (box_fragment_.IsSvgText())
    paint_state.emplace(*box_fragment_.GetLayoutObject(), paint_info);

  PaintInfo child_paint_info(paint_info.ForDescendants());

  // Only paint during the foreground/selection phases.
  if (child_paint_info.phase != PaintPhase::kForeground &&
      child_paint_info.phase != PaintPhase::kForcedColorsModeBackplate &&
      child_paint_info.phase != PaintPhase::kSelectionDragImage &&
      child_paint_info.phase != PaintPhase::kTextClip &&
      child_paint_info.phase != PaintPhase::kMask &&
      child_paint_info.phase != PaintPhase::kDescendantOutlinesOnly &&
      child_paint_info.phase != PaintPhase::kOutline) {
    if (ShouldPaintDescendantBlockBackgrounds(child_paint_info.phase))
        [[unlikely]] {
      // When block-in-inline, block backgrounds need to be painted.
      PaintBoxDecorationBackgroundForBlockInInline(&children, child_paint_info,
                                                   paint_offset);
    }
    return;
  }

  if (child_paint_info.phase == PaintPhase::kForeground &&
      child_paint_info.ShouldAddUrlMetadata()) {
    // TODO(crbug.com/1392701): Avoid walking the LayoutObject tree (which is
    // what AddURLRectsForInlineChildrenRecursively() does). We should walk the
    // fragment tree instead (if we can figure out how to deal with culled
    // inlines - or get rid of them). Walking the LayoutObject tree means that
    // we'll visit every link in the container for each fragment generated,
    // leading to duplicate entries. This is only fine as long as the absolute
    // offsets is the same every time a given link is visited. Otherwise links
    // might end up as unclickable in the resulting PDF. So make sure that the
    // paint offset relative to the first fragment generated by this
    // container. This matches legacy engine behavior.
    PhysicalOffset paint_offset_for_first_fragment =
        paint_offset - OffsetInStitchedFragments(box_fragment_);
    AddURLRectsForInlineChildrenRecursively(*layout_object, child_paint_info,
                                            paint_offset_for_first_fragment);
  }

  // If we have no lines then we have no work to do.
  if (!children)
    return;

  if (child_paint_info.phase == PaintPhase::kForcedColorsModeBackplate &&
      box_fragment_.GetDocument().InForcedColorsMode()) {
    PaintBackplate(&children, child_paint_info, paint_offset);
    return;
  }

  DCHECK(children.HasRoot());
  PaintLineBoxChildItems(&children, child_paint_info, paint_offset);
}

void BoxFragmentPainter::PaintCurrentPageContainer(
    const PaintInfo& paint_info) {
  DCHECK(box_fragment_.IsPaginatedRoot());

  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  // The correct page box fragment for the given page has been selected, and
  // that's all that's going to be painted now. The cull rect used during
  // printing is for the paginated content only, in the stitched coordinate
  // system with all the page areas stacked after oneanother. However, no
  // paginated content will be painted here (that's in separate paint layers),
  // only page box decorations and margin fragments.
  paint_info_for_descendants.SetCullRect(CullRect::Infinite());

  PaintInfo paint_info_for_page_container = paint_info_for_descendants;
  // We only want the page container to paint itself and return (and then handle
  // its children on our own here, further below).
  paint_info_for_page_container.SetDescendantPaintingBlocked();

  const PaginationState* pagination_state =
      box_fragment_.GetDocument().View()->GetPaginationState();
  wtf_size_t page_index = pagination_state->CurrentPageIndex();

  const auto& page_container =
      To<PhysicalBoxFragment>(*box_fragment_.Children()[page_index]);
  BoxFragmentPainter(page_container).Paint(paint_info_for_page_container);

  // Paint children of the page container - that is the page border box
  // fragment, and any surrounding page margin boxes. Paint sorted by
  // z-index. We sort a vector of fragment indices, rather than sorting a
  // temporary list of fragments directly, as that would involve oilpan
  // allocations and garbage for no reason.
  //
  // TODO(crbug.com/363031541) Although the page background and borders (and
  // outlines, etc) are painted at the correct time, the paginated document
  // contents (the page areas) will be painted on top of everything, since the
  // document root element, and anything contained by the initial containing
  // block, are separate layers.
  base::span<const PhysicalFragmentLink> children = page_container.Children();
  std::vector<wtf_size_t> indices;
  indices.resize(children.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::stable_sort(
      indices.begin(), indices.end(), [&children](wtf_size_t a, wtf_size_t b) {
        return children[a]->Style().ZIndex() < children[b]->Style().ZIndex();
      });
  for (wtf_size_t index : indices) {
    const PhysicalFragmentLink& child = children[index];
    const auto& child_fragment = To<PhysicalBoxFragment>(*child);
    DCHECK(!child_fragment.HasSelfPaintingLayer());
    BoxFragmentPainter(child_fragment).Paint(paint_info_for_descendants);
  }
}

void BoxFragmentPainter::PaintBlockChildren(const PaintInfo& paint_info,
                                            PhysicalOffset paint_offset) {
  DCHECK(!box_fragment_.IsInlineFormattingContext());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  for (const PhysicalFragmentLink& child : box_fragment_.Children()) {
    const PhysicalFragment& child_fragment = *child;
    DCHECK(child_fragment.IsBox());
    if (child_fragment.HasSelfPaintingLayer() || child_fragment.IsFloating())
      continue;
    PaintBlockChild(child, paint_info, paint_info_for_descendants,
                    paint_offset);
  }
}

void BoxFragmentPainter::PaintBlockChild(
    const PhysicalFragmentLink& child,
    const PaintInfo& paint_info,
    const PaintInfo& paint_info_for_descendants,
    PhysicalOffset paint_offset) {
  const PhysicalFragment& child_fragment = *child;
  DCHECK(child_fragment.IsBox());
  DCHECK(!child_fragment.HasSelfPaintingLayer());
  DCHECK(!child_fragment.IsFloating());
  const auto& box_child_fragment = To<PhysicalBoxFragment>(child_fragment);
  if (box_child_fragment.CanTraverse()) {
    if (box_child_fragment.IsFragmentainerBox()) {
      // It's normally FragmentData that provides us with the paint offset.
      // FragmentData is (at least currently) associated with a LayoutObject.
      // If we have no LayoutObject, we have no FragmentData, so we need to
      // calculate the offset on our own (which is very simple, anyway).
      // Bypass Paint() and jump directly to PaintObject(), to skip the code
      // that assumes that we have a LayoutObject (and FragmentData).
      PhysicalOffset child_offset = paint_offset + child.offset;

      // This is a fragmentainer, and when a node inside a fragmentation context
      // paints multiple block fragments, we need to distinguish between them
      // somehow, for paint caching to work. Therefore, establish a display item
      // scope here.
      unsigned identifier = FragmentainerUniqueIdentifier(box_child_fragment);
      ScopedDisplayItemFragment scope(paint_info.context, identifier);
      BoxFragmentPainter(box_child_fragment)
          .PaintObject(paint_info, child_offset);
      return;
    }

    BoxFragmentPainter(box_child_fragment).Paint(paint_info_for_descendants);
    return;
  }

  PaintFragment(box_child_fragment, paint_info_for_descendants);
}

void BoxFragmentPainter::PaintFloatingItems(const PaintInfo& paint_info,
                                            InlineCursor* cursor) {
  while (*cursor) {
    const FragmentItem* item = cursor->Current().Item();
    DCHECK(item);
    const PhysicalBoxFragment* child_fragment = item->BoxFragment();
    if (!child_fragment) {
      cursor->MoveToNext();
      continue;
    }
    if (child_fragment->HasSelfPaintingLayer()) {
      cursor->MoveToNextSkippingChildren();
      continue;
    }
    if (child_fragment->IsFloating()) {
      PaintInfo float_paint_info = FloatPaintInfo(paint_info);
      PaintFragment(*child_fragment, float_paint_info);
    } else if (child_fragment->IsBlockInInline() &&
               child_fragment->HasFloatingDescendantsForPaint()) {
      BoxFragmentPainter(*child_fragment).Paint(paint_info);
    }
    DCHECK(child_fragment->IsInlineBox() || !cursor->Current().HasChildren());
    cursor->MoveToNext();
  }
}

void BoxFragmentPainter::PaintFloatingChildren(
    const PhysicalFragment& container,
    const PaintInfo& paint_info) {
  DCHECK(container.HasFloatingDescendantsForPaint());
  const PaintInfo* local_paint_info = &paint_info;
  std::optional<ScopedPaintState> paint_state;
  std::optional<ScopedBoxContentsPaintState> contents_paint_state;
  if (const auto* box = DynamicTo<LayoutBox>(container.GetLayoutObject())) {
    paint_state.emplace(To<PhysicalBoxFragment>(container), paint_info);
    contents_paint_state.emplace(*paint_state, *box);
    local_paint_info = &contents_paint_state->GetPaintInfo();
  }

  DCHECK(container.HasFloatingDescendantsForPaint());

  for (const PhysicalFragmentLink& child : container.Children()) {
    const PhysicalFragment& child_fragment = *child;
    if (child_fragment.HasSelfPaintingLayer())
      continue;

    if (child_fragment.IsFloating()) {
      PaintFragment(To<PhysicalBoxFragment>(child_fragment),
                    FloatPaintInfo(*local_paint_info));
      continue;
    }

    // Any non-floated children which paint atomically shouldn't be traversed.
    if (child_fragment.IsPaintedAtomically())
      continue;

    // The selection paint traversal is special. We will visit all fragments
    // (including floats) in the normal paint traversal. There isn't any point
    // performing the special float traversal here.
    if (local_paint_info->phase == PaintPhase::kSelectionDragImage)
      continue;

    if (!child_fragment.HasFloatingDescendantsForPaint())
      continue;

    if (child_fragment.HasNonVisibleOverflow()) {
      // We need to properly visit this fragment for painting, rather than
      // jumping directly to its children (which is what we normally do when
      // looking for floats), in order to set up the clip rectangle.
      BoxFragmentPainter(To<PhysicalBoxFragment>(child_fragment))
          .Paint(*local_paint_info);
      continue;
    }

    if (child_fragment.IsFragmentainerBox()) {
      // This is a fragmentainer, and when node inside a fragmentation context
      // paints multiple block fragments, we need to distinguish between them
      // somehow, for paint caching to work. Therefore, establish a display item
      // scope here.
      unsigned identifier = FragmentainerUniqueIdentifier(
          To<PhysicalBoxFragment>(child_fragment));
      ScopedDisplayItemFragment scope(paint_info.context, identifier);
      PaintFloatingChildren(child_fragment, *local_paint_info);
    } else {
      PaintFloatingChildren(child_fragment, *local_paint_info);
    }
  }

  // Now process the inline formatting context, if any.
  //
  // TODO(mstensho): Clean up this. Now that floats no longer escape their
  // inline formatting context when fragmented, we should only have to one of
  // these things; either walk the inline items, OR walk the box fragment
  // children (above).
  if (const PhysicalBoxFragment* box =
          DynamicTo<PhysicalBoxFragment>(&container)) {
    if (const FragmentItems* items = box->Items()) {
      InlineCursor cursor(*box, *items);
      PaintFloatingItems(*local_paint_info, &cursor);
      return;
    }
    if (inline_box_cursor_) {
      DCHECK(box->IsInlineBox());
      InlineCursor descendants = inline_box_cursor_->CursorForDescendants();
      PaintFloatingItems(*local_paint_info, &descendants);
      return;
    }
    DCHECK(!box->IsInlineBox());
  }
}

void BoxFragmentPainter::PaintFloats(const PaintInfo& paint_info) {
  DCHECK(GetPhysicalFragment().HasFloatingDescendantsForPaint() ||
         !GetPhysicalFragment().IsInlineFormattingContext());
  PaintFloatingChildren(GetPhysicalFragment(), paint_info);
}

void BoxFragmentPainter::PaintMask(const PaintInfo& paint_info,
                                   const PhysicalOffset& paint_offset) {
  DCHECK_EQ(PaintPhase::kMask, paint_info.phase);
  const PhysicalBoxFragment& physical_box_fragment = GetPhysicalFragment();
  const ComputedStyle& style = physical_box_fragment.Style();
  if (!style.HasMask() || !IsVisibleToPaint(physical_box_fragment, style))
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, GetDisplayItemClient(), paint_info.phase))
    return;

  if (physical_box_fragment.IsFieldsetContainer()) {
    FieldsetPainter(box_fragment_).PaintMask(paint_info, paint_offset);
    return;
  }

  DrawingRecorder recorder(paint_info.context, GetDisplayItemClient(),
                           paint_info.phase, VisualRect(paint_offset));
  PhysicalRect paint_rect(paint_offset, box_fragment_.Size());
  // TODO(eae): Switch to LayoutNG version of BoxBackgroundPaintContext.
  BoxBackgroundPaintContext bg_paint_context(
      *static_cast<const LayoutBoxModelObject*>(
          box_fragment_.GetLayoutObject()));
  PaintMaskImages(paint_info, paint_rect, *box_fragment_.GetLayoutObject(),
                  bg_paint_context, box_fragment_.SidesToInclude());
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void BoxFragmentPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    bool suppress_box_decoration_background) {
  // TODO(mstensho): Break dependency on LayoutObject functionality.
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();

  if (IsA<LayoutView>(layout_object) ||
      box_fragment_.GetBoxType() == PhysicalFragment::kPageContainer) {
    // The root background has a designated painter. For regular layout, this is
    // the LayoutView. For paginated layout, it's the background of the page box
    // that covers the entire area of a given page.
    ViewPainter(box_fragment_).PaintBoxDecorationBackground(paint_info);
    return;
  }

  PhysicalRect paint_rect;
  const DisplayItemClient* background_client = nullptr;
  std::optional<ScopedBoxContentsPaintState> contents_paint_state;
  gfx::Rect visual_rect;
  if (paint_info.IsPaintingBackgroundInContentsSpace()) {
    // For the case where we are painting the background in the contents space,
    // we need to include the entire overflow rect.
    const LayoutBox& layout_box = To<LayoutBox>(layout_object);
    paint_rect = layout_box.ScrollableOverflowRect();

    contents_paint_state.emplace(paint_info, paint_offset, layout_box,
                                 box_fragment_.GetFragmentData());
    paint_rect.Move(contents_paint_state->PaintOffset());

    // The background painting code assumes that the borders are part of the
    // paintRect so we expand the paintRect by the border size when painting the
    // background into the scrolling contents layer.
    paint_rect.Expand(layout_box.BorderOutsets());

    background_client = &layout_box.GetScrollableArea()
                             ->GetScrollingBackgroundDisplayItemClient();
    visual_rect = layout_box.GetScrollableArea()->ScrollingBackgroundVisualRect(
        paint_offset);
  } else {
    paint_rect.offset = paint_offset;
    paint_rect.size = box_fragment_.Size();
    background_client = &GetDisplayItemClient();
    visual_rect = VisualRect(paint_offset);
  }

  if (!suppress_box_decoration_background &&
      !(paint_info.IsPaintingBackgroundInContentsSpace() &&
        paint_info.ShouldSkipBackground())) {
    PaintBoxDecorationBackgroundWithRect(
        contents_paint_state ? contents_paint_state->GetPaintInfo()
                             : paint_info,
        visual_rect, paint_rect, *background_client);

    Element* element = DynamicTo<Element>(layout_object.GetNode());
    if (element && element->GetRegionCaptureCropId()) {
      paint_info.context.GetPaintController().RecordRegionCaptureData(
          *background_client, *(element->GetRegionCaptureCropId()),
          ToPixelSnappedRect(paint_rect));
    }
  }

  if (ShouldRecordHitTestData(paint_info)) {
    ObjectPainter(layout_object)
        .RecordHitTestData(paint_info, ToPixelSnappedRect(paint_rect),
                           *background_client);
  }

  // Record the scroll hit test after the non-scrolling background so
  // background squashing is not affected. Hit test order would be equivalent
  // if this were immediately before the non-scrolling background.
  if (!paint_info.IsPaintingBackgroundInContentsSpace())
    RecordScrollHitTestData(paint_info, *background_client);
}

void BoxFragmentPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const gfx::Rect& visual_rect,
    const PhysicalRect& paint_rect,
    const DisplayItemClient& background_client) {
  BoxDecorationData box_decoration_data(paint_info, box_fragment_);
  if (!box_decoration_data.ShouldPaint() &&
      (!box_fragment_.IsTable() ||
       !TablePainter(box_fragment_).WillCheckColumnBackgrounds())) {
    return;
  }

  const auto& box = To<LayoutBox>(*box_fragment_.GetLayoutObject());
  std::optional<DisplayItemCacheSkipper> cache_skipper;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      ShouldSkipPaintUnderInvalidationChecking(box)) {
    cache_skipper.emplace(paint_info.context);
  }

  if (box.CanCompositeBackgroundAttachmentFixed() &&
      BoxBackgroundPaintContext::HasBackgroundFixedToViewport(box)) {
    PaintCompositeBackgroundAttachmentFixed(paint_info, background_client,
                                            box_decoration_data);
    if (box_decoration_data.ShouldPaintBorder()) {
      PaintBoxDecorationBackgroundWithDecorationData(
          paint_info, visual_rect, paint_rect, background_client,
          DisplayItem::kBoxDecorationBackground,
          box_decoration_data.BorderOnly());
    }
  } else {
    PaintBoxDecorationBackgroundWithDecorationData(
        paint_info, visual_rect, paint_rect, background_client,
        DisplayItem::kBoxDecorationBackground, box_decoration_data);
  }
}

void BoxFragmentPainter::PaintCompositeBackgroundAttachmentFixed(
    const PaintInfo& paint_info,
    const DisplayItemClient& background_client,
    const BoxDecorationData& box_decoration_data) {
  const auto& box = To<LayoutBox>(*box_fragment_.GetLayoutObject());
  DCHECK(box.CanCompositeBackgroundAttachmentFixed());
  const FragmentData* fragment_data = box_fragment_.GetFragmentData();
  if (!fragment_data) {
    return;
  }

  // Paint the background-attachment:fixed background in the view's transform
  // space, clipped by BackgroundClip.
  DCHECK(!box_decoration_data.IsPaintingBackgroundInContentsSpace());
  DCHECK(!box_decoration_data.HasAppearance());
  DCHECK(!box_decoration_data.ShouldPaintShadow());
  DCHECK(box_decoration_data.ShouldPaintBackground());
  DCHECK(fragment_data->PaintProperties());
  DCHECK(fragment_data->PaintProperties()->BackgroundClip());
  PropertyTreeStateOrAlias state(
      box.View()->FirstFragment().LocalBorderBoxProperties().Transform(),
      *fragment_data->PaintProperties()->BackgroundClip(),
      paint_info.context.GetPaintController()
          .CurrentPaintChunkProperties()
          .Effect());
  const ScrollableArea* layout_viewport = box.GetFrameView()->LayoutViewport();
  DCHECK(layout_viewport);
  gfx::Rect background_rect(layout_viewport->VisibleContentRect().size());
  ScopedPaintChunkProperties fixed_background_properties(
      paint_info.context.GetPaintController(), state, background_client,
      DisplayItem::kFixedAttachmentBackground);
  PaintBoxDecorationBackgroundWithDecorationData(
      paint_info, background_rect, PhysicalRect(background_rect),
      background_client, DisplayItem::kFixedAttachmentBackground,
      box_decoration_data.BackgroundOnly());
}

void BoxFragmentPainter::PaintBoxDecorationBackgroundWithDecorationData(
    const PaintInfo& paint_info,
    const gfx::Rect& visual_rect,
    const PhysicalRect& paint_rect,
    const DisplayItemClient& background_client,
    DisplayItem::Type display_item_type,
    const BoxDecorationData& box_decoration_data) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, background_client, display_item_type)) {
    return;
  }

  DrawingRecorder recorder(paint_info.context, background_client,
                           display_item_type, visual_rect);

  if (GetPhysicalFragment().IsFieldsetContainer()) {
    FieldsetPainter(box_fragment_)
        .PaintBoxDecorationBackground(paint_info, paint_rect,
                                      box_decoration_data);
  } else if (GetPhysicalFragment().IsTablePart()) {
    if (box_fragment_.IsTableCell()) {
      TableCellPainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_rect,
                                        box_decoration_data);
    } else if (box_fragment_.IsTableRow()) {
      TableRowPainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_rect,
                                        box_decoration_data);
    } else if (box_fragment_.IsTableSection()) {
      TableSectionPainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_rect,
                                        box_decoration_data);
    } else {
      DCHECK(box_fragment_.IsTable());
      TablePainter(box_fragment_)
          .PaintBoxDecorationBackground(paint_info, paint_rect,
                                        box_decoration_data);
    }
  } else {
    PaintBoxDecorationBackgroundWithRectImpl(paint_info, paint_rect,
                                             box_decoration_data);
  }
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void BoxFragmentPainter::PaintBoxDecorationBackgroundWithRectImpl(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
  const LayoutObject& layout_object = *box_fragment_.GetLayoutObject();
  const LayoutBox& layout_box = To<LayoutBox>(layout_object);

  const ComputedStyle& style = box_fragment_.Style();

  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (box_decoration_data.ShouldPaintShadow()) {
    PaintNormalBoxShadow(paint_info, paint_rect, style,
                         box_fragment_.SidesToInclude(),
                         !box_decoration_data.ShouldPaintBackground());
  }

  bool needs_end_layer = false;
  if (!box_decoration_data.IsPaintingBackgroundInContentsSpace() &&
      BleedAvoidanceIsClipping(
          box_decoration_data.GetBackgroundBleedAvoidance())) {
    state_saver.Save();
    FloatRoundedRect border = RoundedBorderGeometry::PixelSnappedRoundedBorder(
        style, paint_rect, box_fragment_.SidesToInclude());
    paint_info.context.ClipRoundedRect(border);

    if (box_decoration_data.GetBackgroundBleedAvoidance() ==
        kBackgroundBleedClipLayer) {
      paint_info.context.BeginLayer();
      needs_end_layer = true;
    }
  }

  gfx::Rect snapped_paint_rect = ToPixelSnappedRect(paint_rect);
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.HasAppearance() &&
      !theme_painter.Paint(layout_box, paint_info, snapped_paint_rect);
  if (!theme_painted) {
    if (box_decoration_data.ShouldPaintBackground()) {
      PaintBackground(paint_info, paint_rect,
                      box_decoration_data.BackgroundColor(),
                      box_decoration_data.GetBackgroundBleedAvoidance());
    }
    if (box_decoration_data.HasAppearance()) {
      theme_painter.PaintDecorations(layout_box.GetNode(),
                                     layout_box.GetDocument(), style,
                                     paint_info, snapped_paint_rect);
    }
  }

  if (box_decoration_data.ShouldPaintShadow()) {
    if (layout_box.IsTableCell()) {
      PhysicalRect inner_rect = paint_rect;
      inner_rect.Contract(layout_box.BorderOutsets());
      // PaintInsetBoxShadowWithInnerRect doesn't subtract borders before
      // painting. We have to use it here after subtracting collapsed borders
      // above. PaintInsetBoxShadowWithBorderRect below subtracts the borders
      // specified on the style object, which doesn't account for border
      // collapsing.
      BoxPainterBase::PaintInsetBoxShadowWithInnerRect(paint_info, inner_rect,
                                                       style);
    } else {
      PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect, style,
                                        box_fragment_.SidesToInclude());
    }
  }

  // The theme will tell us whether or not we should also paint the CSS
  // border.
  if (box_decoration_data.ShouldPaintBorder()) {
    if (!theme_painted) {
      theme_painted =
          box_decoration_data.HasAppearance() &&
          !LayoutTheme::GetTheme().Painter().PaintBorderOnly(
              layout_box.GetNode(), style, paint_info, snapped_paint_rect);
    }
    if (!theme_painted) {
      Node* generating_node = layout_object.GeneratingNode();
      const Document& document = layout_object.GetDocument();
      PaintBorder(*box_fragment_.GetLayoutObject(), document, generating_node,
                  paint_info, paint_rect, style,
                  box_decoration_data.GetBackgroundBleedAvoidance(),
                  box_fragment_.SidesToInclude());
    }
  }

  if (needs_end_layer)
    paint_info.context.EndLayer();
}

void BoxFragmentPainter::PaintBoxDecorationBackgroundForBlockInInline(
    InlineCursor* children,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  while (*children) {
    const FragmentItem* item = children->Current().Item();
    if (const PhysicalLineBoxFragment* line = item->LineBoxFragment()) {
      if (!line->IsBlockInInline()) {
        children->MoveToNextSkippingChildren();
        continue;
      }
    } else if (const PhysicalBoxFragment* fragment = item->BoxFragment()) {
      if (fragment->HasSelfPaintingLayer()) {
        children->MoveToNextSkippingChildren();
        continue;
      }
      if (fragment->IsBlockInInline())
        PaintBoxItem(*item, *fragment, *children, paint_info, paint_offset);
    }
    children->MoveToNext();
  }
}

void BoxFragmentPainter::PaintColumnRules(const PaintInfo& paint_info,
                                          const PhysicalOffset& paint_offset) {
  const ComputedStyle& style = box_fragment_.Style();
  DCHECK(box_fragment_.IsCSSBox());
  DCHECK(style.HasColumnRule());

  // https://www.w3.org/TR/css-multicol-1/#propdef-column-rule-style
  // interpret column-rule-style as in the collapsing border model
  EBorderStyle rule_style =
      ComputedStyle::CollapsedBorderStyle(style.ColumnRuleStyle());

  if (DrawingRecorder::UseCachedDrawingIfPossible(paint_info.context,
                                                  GetDisplayItemClient(),
                                                  DisplayItem::kColumnRules))
    return;

  DrawingRecorder recorder(paint_info.context, GetDisplayItemClient(),
                           DisplayItem::kColumnRules, gfx::Rect());

  const Color& rule_color =
      LayoutObject::ResolveColor(style, GetCSSPropertyColumnRuleColor());
  LayoutUnit rule_thickness(style.ColumnRuleWidth());

  // Count all the spanners
  int span_count = 0;
  for (const PhysicalFragmentLink& child : box_fragment_.Children()) {
    if (!child->IsColumnBox()) {
      span_count++;
    }
  }

  PhysicalRect previous_column;
  bool past_first_column_in_row = false;
  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
  for (const PhysicalFragmentLink& child : box_fragment_.Children()) {
    if (!child->IsColumnBox()) {
      // Column spanner. Continue in the next row, if there are 2 columns or
      // more there.
      past_first_column_in_row = false;
      previous_column = PhysicalRect();

      span_count--;
      CHECK_GE(span_count, 0);
      continue;
    }

    PhysicalRect current_column(child.offset, child->Size());
    if (!past_first_column_in_row) {
      // Rules are painted *between* columns. Need to see if we have a second
      // one before painting anything.
      past_first_column_in_row = true;
      previous_column = current_column;
      continue;
    }

    PhysicalRect rule;
    BoxSide box_side;
    if (style.IsHorizontalWritingMode()) {
      LayoutUnit center;
      if (style.IsLeftToRightDirection()) {
        center = (previous_column.X() + current_column.Right()) / 2;
        box_side = BoxSide::kLeft;
      } else {
        center = (current_column.X() + previous_column.Right()) / 2;
        box_side = BoxSide::kRight;
      }

      // Paint column rules as tall as the entire multicol container, but only
      // when we're past all spanners.
      LayoutUnit rule_length;
      if (!span_count) {
        const LayoutUnit column_box_bottom = box_fragment_.Size().height -
                                             box_fragment_.Borders().bottom -
                                             box_fragment_.Padding().bottom -
                                             box_fragment_.OwnerLayoutBox()
                                                 ->ComputeLogicalScrollbars()
                                                 .block_end;
        rule_length = column_box_bottom - previous_column.offset.top;
        // For the case when the border or the padding is included in the
        // multicol container.
        // TODO(layout-dev): Get rid of this clamping, and fix any underlying
        // issues
        rule_length = std::max(rule_length, previous_column.Height());
      } else {
        rule_length = previous_column.Height();
      }

      DCHECK_GE(rule_length, current_column.Height());
      rule.offset.top = previous_column.offset.top;
      rule.size.height = rule_length;
      rule.offset.left = center - rule_thickness / 2;
      rule.size.width = rule_thickness;
    } else {
      // Vertical writing-mode.
      LayoutUnit center;
      if (style.IsLeftToRightDirection()) {
        // Top to bottom.
        center = (previous_column.Y() + current_column.Bottom()) / 2;
        box_side = BoxSide::kTop;
      } else {
        // Bottom to top.
        center = (current_column.Y() + previous_column.Bottom()) / 2;
        box_side = BoxSide::kBottom;
      }

      LayoutUnit rule_length;
      LayoutUnit rule_left = previous_column.offset.left;
      if (!span_count) {
        if (style.GetWritingMode() == WritingMode::kVerticalLr) {
          const LayoutUnit column_box_right = box_fragment_.Size().width -
                                              box_fragment_.Borders().right -
                                              box_fragment_.Padding().right -
                                              box_fragment_.OwnerLayoutBox()
                                                  ->ComputeLogicalScrollbars()
                                                  .block_end;
          rule_length = column_box_right - previous_column.offset.left;
        } else {
          // Vertical-rl writing-mode
          const LayoutUnit column_box_left = box_fragment_.ContentOffset().left;
          rule_length = previous_column.Width() +
                        (previous_column.offset.left - column_box_left);
          rule_left = column_box_left;
        }

        // TODO(layout-dev): Get rid of this clamping, and fix any underlying
        // issues
        rule_length = std::max(rule_length, previous_column.Width());
        rule_left = std::min(rule_left, previous_column.offset.left);
      } else {
        rule_length = previous_column.Width();
      }

      DCHECK_GE(rule_length, current_column.Width());
      rule.offset.left = rule_left;
      rule.size.width = rule_length;
      rule.offset.top = center - rule_thickness / 2;
      rule.size.height = rule_thickness;
    }

    rule.Move(paint_offset);
    gfx::Rect snapped_rule = ToPixelSnappedRect(rule);
    BoxBorderPainter::DrawBoxSide(paint_info.context, snapped_rule, box_side,
                                  rule_color, rule_style, auto_dark_mode);
    recorder.UniteVisualRect(snapped_rule);

    previous_column = current_column;
  }
}

// TODO(kojii): This logic is kept in sync with BoxPainter. Not much efforts to
// eliminate LayoutObject dependency were done yet.
void BoxFragmentPainter::PaintBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const Color& background_color,
    BackgroundBleedAvoidance bleed_avoidance) {
  const auto& layout_box = To<LayoutBox>(*box_fragment_.GetLayoutObject());
  if (layout_box.BackgroundTransfersToView())
    return;
  if (layout_box.BackgroundIsKnownToBeObscured())
    return;

  const ComputedStyle* style_to_use = &box_fragment_.Style();
  Color background_color_to_use = background_color;
  if (box_fragment_.GetBoxType() == PhysicalFragment::kPageBorderBox) {
    // The page border box fragment paints the document background.
    // See https://drafts.csswg.org/css-page-3/#painting
    const Document& document = box_fragment_.GetDocument();
    const Element* root = document.documentElement();
    if (!root || !root->GetLayoutObject()) {
      // We're going to need a document element, and it needs to have a box.
      // If there's no such thing, we have nothing to paint.
      return;
    }
    style_to_use = document.GetLayoutView()->Style();
    background_color_to_use =
        style_to_use->VisitedDependentColor(GetCSSPropertyBackgroundColor());
  }

  BoxBackgroundPaintContext bg_paint_context(box_fragment_);
  PaintFillLayers(paint_info, background_color_to_use,
                  style_to_use->BackgroundLayers(), paint_rect,
                  bg_paint_context, bleed_avoidance);
}

void BoxFragmentPainter::PaintAllPhasesAtomically(const PaintInfo& paint_info) {
  // Self-painting AtomicInlines should go to normal paint logic.
  DCHECK(!(GetPhysicalFragment().IsPaintedAtomically() &&
           box_fragment_.HasSelfPaintingLayer()));

  // Pass PaintPhaseSelection and PaintPhaseTextClip is handled by the regular
  // foreground paint implementation. We don't need complete painting for these
  // phases.
  PaintPhase phase = paint_info.phase;
  if (phase == PaintPhase::kSelectionDragImage ||
      phase == PaintPhase::kTextClip)
    return PaintInternal(paint_info);

  if (phase != PaintPhase::kForeground)
    return;

  PaintInfo local_paint_info(paint_info);
  local_paint_info.phase = PaintPhase::kBlockBackground;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kForcedColorsModeBackplate;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kFloat;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kForeground;
  PaintInternal(local_paint_info);

  local_paint_info.phase = PaintPhase::kOutline;
  PaintInternal(local_paint_info);
}

void BoxFragmentPainter::PaintInlineItems(const PaintInfo& paint_info,
                                          const PhysicalOffset& paint_offset,
                                          const PhysicalOffset& parent_offset,
                                          InlineCursor* cursor) {
  while (*cursor) {
    const FragmentItem* item = cursor->CurrentItem();
    DCHECK(item);
    if (item->IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      // TODO(crbug.com/1099613): This should not happen, as long as it is
      // really layout-clean.
      NOTREACHED_IN_MIGRATION();
      cursor->MoveToNextSkippingChildren();
      continue;
    }
    switch (item->Type()) {
      case FragmentItem::kText:
      case FragmentItem::kGeneratedText:
        if (!item->IsHiddenForPaint())
          PaintTextItem(*cursor, paint_info, paint_offset, parent_offset);
        cursor->MoveToNext();
        break;
      case FragmentItem::kBox:
        if (!item->IsHiddenForPaint())
          PaintBoxItem(*item, *cursor, paint_info, paint_offset, parent_offset);
        cursor->MoveToNextSkippingChildren();
        break;
      case FragmentItem::kLine:
        // Nested kLine items are used for ruby annotations.
        if (RuntimeEnabledFeatures::RubyLineBreakableEnabled()) {
          InlineCursor line_box_cursor = cursor->CursorForDescendants();
          PaintInlineItems(paint_info, paint_offset, parent_offset,
                           &line_box_cursor);
          cursor->MoveToNextSkippingChildren();
        } else {
          NOTREACHED_IN_MIGRATION();
          cursor->MoveToNext();
        }
        break;
      case FragmentItem::kInvalid:
        NOTREACHED();
    }
  }
}

// Paint a line box. This function records hit test data of the line box in
// case the line box overflows the container or the line box is in a different
// chunk from the hit test data recorded for the container box's background.
// It also paints the backgrounds of the `::first-line` line box. Other line
// boxes don't have their own background.
inline void BoxFragmentPainter::PaintLineBox(
    const PhysicalFragment& line_box_fragment,
    const DisplayItemClient& display_item_client,
    const FragmentItem& line_box_item,
    const PaintInfo& paint_info,
    const PhysicalOffset& child_offset) {
  if (paint_info.phase != PaintPhase::kForeground)
    return;

  PhysicalRect border_box = line_box_fragment.LocalRect();
  border_box.offset += child_offset;
  const wtf_size_t line_fragment_id = line_box_item.FragmentId();
  DCHECK_GE(line_fragment_id, FragmentItem::kInitialLineFragmentId);
  ScopedDisplayItemFragment display_item_fragment(paint_info.context,
                                                  line_fragment_id);

  bool paints_hit_test_data =
      !RuntimeEnabledFeatures::HitTestOpaquenessEnabled() ||
      !RuntimeEnabledFeatures::HitTestOpaquenessOmitLineBoxEnabled();
  if (paints_hit_test_data && ShouldRecordHitTestData(paint_info)) {
    ObjectPainter(*GetPhysicalFragment().GetLayoutObject())
        .RecordHitTestData(paint_info, ToPixelSnappedRect(border_box),
                           display_item_client);
  }

  Element* element = DynamicTo<Element>(line_box_fragment.GetNode());
  if (element && element->GetRegionCaptureCropId()) {
    paint_info.context.GetPaintController().RecordRegionCaptureData(
        display_item_client, *(element->GetRegionCaptureCropId()),
        ToPixelSnappedRect(border_box));
  }

  // Paint the background of the `::first-line` line box.
  if (LineBoxFragmentPainter::NeedsPaint(line_box_fragment)) {
    LineBoxFragmentPainter line_box_painter(line_box_fragment, line_box_item,
                                            GetPhysicalFragment());
    line_box_painter.PaintBackgroundBorderShadow(paint_info, child_offset);
  }
}

void BoxFragmentPainter::PaintLineBoxChildItems(
    InlineCursor* children,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) {
  const bool is_horizontal = box_fragment_.Style().IsHorizontalWritingMode();
  for (; *children; children->MoveToNextSkippingChildren()) {
    const FragmentItem* child_item = children->CurrentItem();
    DCHECK(child_item);
    if (child_item->IsFloating())
      continue;

    // Check if CullRect intersects with this child, only in block direction
    // because soft-wrap and <br> needs to paint outside of InkOverflow() in
    // inline direction.
    const PhysicalOffset& child_offset =
        paint_offset + child_item->OffsetInContainerFragment();
    const PhysicalRect child_rect = child_item->InkOverflowRect();
    if (is_horizontal) {
      LayoutUnit y = child_rect.offset.top + child_offset.top;
      if (!paint_info.GetCullRect().IntersectsVerticalRange(
              y, y + child_rect.size.height))
        continue;
    } else {
      LayoutUnit x = child_rect.offset.left + child_offset.left;
      if (!paint_info.GetCullRect().IntersectsHorizontalRange(
              x, x + child_rect.size.width))
        continue;
    }

    if (child_item->Type() == FragmentItem::kLine) {
      const PhysicalLineBoxFragment* line_box_fragment =
          child_item->LineBoxFragment();
      DCHECK(line_box_fragment);
      PaintLineBox(*line_box_fragment, *child_item->GetDisplayItemClient(),
                   *child_item, paint_info, child_offset);
      InlinePaintContext::ScopedLineBox scoped_line_box(*children,
                                                        inline_context_);
      InlineCursor line_box_cursor = children->CursorForDescendants();
      PaintInlineItems(paint_info, paint_offset,
                       child_item->OffsetInContainerFragment(),
                       &line_box_cursor);
      continue;
    }

    if (const PhysicalBoxFragment* child_fragment = child_item->BoxFragment()) {
      DCHECK(!child_fragment->IsOutOfFlowPositioned());
      if (child_fragment->IsListMarker()) {
        PaintBoxItem(*child_item, *child_fragment, *children, paint_info,
                     paint_offset);
        continue;
      }
    }

    NOTREACHED_IN_MIGRATION();
  }
}

void BoxFragmentPainter::PaintBackplate(InlineCursor* line_boxes,
                                        const PaintInfo& paint_info,
                                        const PhysicalOffset& paint_offset) {
  if (paint_info.phase != PaintPhase::kForcedColorsModeBackplate)
    return;

  // Only paint backplates behind text when forced-color-adjust is auto and the
  // element is visible.
  const ComputedStyle& style = GetPhysicalFragment().Style();
  if (style.ForcedColorAdjust() != EForcedColorAdjust::kAuto ||
      style.UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, GetDisplayItemClient(),
          DisplayItem::kForcedColorsModeBackplate))
    return;

  Color backplate_color = GetPhysicalFragment()
                              .GetLayoutObject()
                              ->GetDocument()
                              .GetStyleEngine()
                              .ForcedBackgroundColor();
  const auto& backplates = BuildBackplate(line_boxes, paint_offset);
  DrawingRecorder recorder(paint_info.context, GetDisplayItemClient(),
                           DisplayItem::kForcedColorsModeBackplate,
                           ToEnclosingRect(UnionRect(backplates)));
  for (const auto backplate : backplates) {
    paint_info.context.FillRect(
        gfx::RectF(backplate), backplate_color,
        PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
  }
}

void BoxFragmentPainter::PaintTextItem(const InlineCursor& cursor,
                                       const PaintInfo& paint_info,
                                       const PhysicalOffset& paint_offset,
                                       const PhysicalOffset& parent_offset) {
  DCHECK(cursor.CurrentItem());
  const FragmentItem& item = *cursor.CurrentItem();
  DCHECK(item.IsText()) << item;

  // Only paint during the foreground/selection phases.
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelectionDragImage &&
      paint_info.phase != PaintPhase::kTextClip &&
      paint_info.phase != PaintPhase::kMask)
    return;

  // Skip if this child does not intersect with CullRect.
  if (!paint_info.IntersectsCullRect(
          item.InkOverflowRect(),
          paint_offset + item.OffsetInContainerFragment()) &&
      // Don't skip <br>, it doesn't have ink but need to paint selection.
      !(item.IsLineBreak() && HasSelection(item.GetLayoutObject()))) {
    return;
  }

  ScopedDisplayItemFragment display_item_fragment(paint_info.context,
                                                  item.FragmentId());
  DCHECK(inline_context_);
  InlinePaintContext::ScopedInlineItem scoped_item(item, inline_context_);
  TextFragmentPainter text_painter(cursor, parent_offset, inline_context_);
  text_painter.Paint(paint_info, paint_offset);
}

// Paint non-culled box item.
void BoxFragmentPainter::PaintBoxItem(const FragmentItem& item,
                                      const PhysicalBoxFragment& child_fragment,
                                      const InlineCursor& cursor,
                                      const PaintInfo& paint_info,
                                      const PhysicalOffset& paint_offset) {
  DCHECK_EQ(item.Type(), FragmentItem::kBox);
  DCHECK_EQ(&item, cursor.Current().Item());
  DCHECK_EQ(item.PostLayoutBoxFragment(), &child_fragment);
  DCHECK(!child_fragment.IsHiddenForPaint());
  if (child_fragment.HasSelfPaintingLayer() || child_fragment.IsFloating())
    return;

  // Skip if this child does not intersect with CullRect.
  if (!paint_info.IntersectsCullRect(
          child_fragment.InkOverflowRect(),
          paint_offset + item.OffsetInContainerFragment())) {
    return;
  }

  if (child_fragment.IsAtomicInline() || child_fragment.IsListMarker()) {
    // Establish a display item fragment scope here, in case there are multiple
    // fragment items for the same layout object. This is unusual for atomic
    // inlines, but might happen e.g. if an text-overflow ellipsis is associated
    // with the layout object.
    ScopedDisplayItemFragment display_item_fragment(paint_info.context,
                                                    item.FragmentId());
    PaintFragment(child_fragment, paint_info);
    return;
  }

  if (child_fragment.IsInlineBox()) {
    DCHECK(inline_context_);
    InlineBoxFragmentPainter(cursor, item, child_fragment, inline_context_)
        .Paint(paint_info, paint_offset);
    return;
  }

  // Block-in-inline
  DCHECK(!child_fragment.GetLayoutObject()->IsInline());
  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  PaintBlockChild({&child_fragment, item.OffsetInContainerFragment()},
                  paint_info, paint_info_for_descendants, paint_offset);
}

void BoxFragmentPainter::PaintBoxItem(const FragmentItem& item,
                                      const InlineCursor& cursor,
                                      const PaintInfo& paint_info,
                                      const PhysicalOffset& paint_offset,
                                      const PhysicalOffset& parent_offset) {
  DCHECK_EQ(item.Type(), FragmentItem::kBox);
  DCHECK_EQ(&item, cursor.Current().Item());

  if (const PhysicalBoxFragment* child_fragment = item.BoxFragment()) {
    child_fragment = child_fragment->PostLayout();
    if (child_fragment)
      PaintBoxItem(item, *child_fragment, cursor, paint_info, paint_offset);
    return;
  }

  // Skip if this child does not intersect with CullRect.
  if (!paint_info.IntersectsCullRect(
          item.InkOverflowRect(),
          paint_offset + item.OffsetInContainerFragment())) {
    return;
  }

  // This |item| is a culled inline box.
  DCHECK(item.GetLayoutObject()->IsLayoutInline());
  InlineCursor children = cursor.CursorForDescendants();
  // Pass the given |parent_offset| because culled inline boxes do not affect
  // the sub-pixel snapping behavior. TODO(kojii): This is for the
  // compatibility, we may want to revisit in future.
  PaintInlineItems(paint_info, paint_offset, parent_offset, &children);
}

bool BoxFragmentPainter::ShouldPaint(
    const ScopedPaintState& paint_state) const {
  DCHECK(!box_fragment_.IsInlineBox());
  // When printing, the root fragment's background (i.e. the document's
  // background) should extend onto every page, regardless of the overflow
  // rectangle.
  if (box_fragment_.IsPaginatedRoot())
    return true;
  return paint_state.LocalRectIntersectsCullRect(
      box_fragment_.InkOverflowRect());
}

void BoxFragmentPainter::PaintTextClipMask(const PaintInfo& paint_info,
                                           const gfx::Rect& mask_rect,
                                           const PhysicalOffset& paint_offset,
                                           bool object_has_multiple_boxes) {
  PaintInfo mask_paint_info(paint_info.context, CullRect(mask_rect),
                            PaintPhase::kTextClip,
                            paint_info.DescendantPaintingBlocked());
  if (!object_has_multiple_boxes) {
    PaintObject(mask_paint_info, paint_offset);
    return;
  }

  DCHECK(inline_box_cursor_);
  DCHECK(box_item_);
  DCHECK(inline_context_);
  InlineBoxFragmentPainter inline_box_painter(*inline_box_cursor_, *box_item_,
                                              inline_context_);
  PaintTextClipMask(mask_paint_info,
                    paint_offset - box_item_->OffsetInContainerFragment(),
                    &inline_box_painter);
}

void BoxFragmentPainter::PaintTextClipMask(
    const PaintInfo& paint_info,
    PhysicalOffset paint_offset,
    InlineBoxFragmentPainter* inline_box_painter) {
  const ComputedStyle& style = box_fragment_.Style();
  if (style.BoxDecorationBreak() == EBoxDecorationBreak::kSlice) {
    LayoutUnit offset_on_line;
    LayoutUnit total_width;
    inline_box_painter->ComputeFragmentOffsetOnLine(
        style.Direction(), &offset_on_line, &total_width);
    if (style.IsHorizontalWritingMode())
      paint_offset.left += offset_on_line;
    else
      paint_offset.top += offset_on_line;
  }
  inline_box_painter->Paint(paint_info, paint_offset);
}

PhysicalRect BoxFragmentPainter::AdjustRectForScrolledContent(
    GraphicsContext& context,
    const PhysicalBoxStrut& borders,
    const PhysicalRect& rect) const {
  const PhysicalBoxFragment& physical = GetPhysicalFragment();

  // Clip to the overflow area.
  context.Clip(gfx::RectF(physical.OverflowClipRect(rect.offset)));

  PhysicalRect scrolled_paint_rect = rect;
  // Adjust the paint rect to reflect a scrolled content box with borders at
  // the ends.
  scrolled_paint_rect.offset -=
      PhysicalOffset(physical.PixelSnappedScrolledContentOffset());
  scrolled_paint_rect.size =
      physical.ScrollSize() +
      PhysicalSize(borders.HorizontalSum(), borders.VerticalSum());
  return scrolled_paint_rect;
}

BoxPainterBase::FillLayerInfo BoxFragmentPainter::GetFillLayerInfo(
    const Color& color,
    const FillLayer& bg_layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool is_painting_background_in_contents_space) const {
  const PhysicalBoxFragment& fragment = GetPhysicalFragment();
  return BoxPainterBase::FillLayerInfo(
      fragment.GetLayoutObject()->GetDocument(), fragment.Style(),
      fragment.IsScrollContainer(), color, bg_layer, bleed_avoidance,
      box_fragment_.SidesToInclude(),
      fragment.GetLayoutObject()->IsLayoutInline(),
      is_painting_background_in_contents_space);
}

template <typename T>
bool BoxFragmentPainter::HitTestContext::AddNodeToResult(
    Node* node,
    const PhysicalBoxFragment* box_fragment,
    const T& bounds_rect,
    const PhysicalOffset& offset) const {
  if (node && !result->InnerNode())
    result->SetNodeAndPosition(node, box_fragment, location.Point() - offset);
  return result->AddNodeToListBasedTestResult(node, location, bounds_rect) ==
         kStopHitTesting;
}

template <typename T>
bool BoxFragmentPainter::HitTestContext::AddNodeToResultWithContentOffset(
    Node* node,
    const PhysicalBoxFragment& container,
    const T& bounds_rect,
    PhysicalOffset offset) const {
  if (container.IsScrollContainer())
    offset += PhysicalOffset(container.PixelSnappedScrolledContentOffset());
  return AddNodeToResult(node, &container, bounds_rect, offset);
}

bool BoxFragmentPainter::NodeAtPoint(HitTestResult& result,
                                     const HitTestLocation& hit_test_location,
                                     const PhysicalOffset& physical_offset,
                                     HitTestPhase phase) {
  HitTestContext hit_test{phase, hit_test_location, physical_offset, &result};
  return NodeAtPoint(hit_test, physical_offset);
}

bool BoxFragmentPainter::NodeAtPoint(HitTestResult& result,
                                     const HitTestLocation& hit_test_location,
                                     const PhysicalOffset& physical_offset,
                                     const PhysicalOffset& inline_root_offset,
                                     HitTestPhase phase) {
  HitTestContext hit_test{phase, hit_test_location, inline_root_offset,
                          &result};
  return NodeAtPoint(hit_test, physical_offset);
}

bool BoxFragmentPainter::NodeAtPoint(const HitTestContext& hit_test,
                                     const PhysicalOffset& physical_offset) {
  const PhysicalBoxFragment& fragment = GetPhysicalFragment();
  // Creating a BoxFragmentPainter is a significant cost, especially in broad
  // trees. Should check before getting here, whether the fragment might
  // intersect or not.
  DCHECK(fragment.MayIntersect(*hit_test.result, hit_test.location,
                               physical_offset));

  if (!fragment.IsFirstForNode() && !CanPaintMultipleFragments(fragment))
    return false;

  if (hit_test.phase == HitTestPhase::kForeground &&
      !box_fragment_.HasSelfPaintingLayer() &&
      HitTestOverflowControl(hit_test, physical_offset))
    return true;

  const PhysicalSize& size = fragment.Size();
  const ComputedStyle& style = fragment.Style();
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  bool skip_children =
      layout_object &&
      (layout_object == hit_test.result->GetHitTestRequest().GetStopNode() ||
       layout_object->ChildPaintBlockedByDisplayLock());
  if (!skip_children && box_fragment_.ShouldClipOverflowAlongEitherAxis()) {
    // PaintLayer::HitTestFragmentsWithPhase() checked the fragments'
    // foreground rect for intersection if a layer is self painting,
    // so only do the overflow clip check here for non-self-painting layers.
    if (!box_fragment_.HasSelfPaintingLayer() &&
        !hit_test.location.Intersects(GetPhysicalFragment().OverflowClipRect(
            physical_offset, kExcludeOverlayScrollbarSizeForHitTesting))) {
      skip_children = true;
    }
    if (!skip_children && style.HasBorderRadius()) {
      PhysicalRect bounds_rect(physical_offset, size);
      skip_children = !hit_test.location.Intersects(
          RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(style,
                                                                bounds_rect));
    }
  }

  if (!skip_children) {
    if (!box_fragment_.IsScrollContainer()) {
      if (HitTestChildren(hit_test, physical_offset))
        return true;
    } else {
      const PhysicalOffset scrolled_offset =
          physical_offset -
          PhysicalOffset(
              GetPhysicalFragment().PixelSnappedScrolledContentOffset());
      HitTestContext adjusted_hit_test{hit_test.phase, hit_test.location,
                                       scrolled_offset, hit_test.result};
      if (HitTestChildren(adjusted_hit_test, scrolled_offset))
        return true;
    }
  }

  if (style.HasBorderRadius() &&
      HitTestClippedOutByBorder(hit_test.location, physical_offset))
    return false;

  bool pointer_events_bounding_box = false;
  bool hit_test_self = fragment.IsInSelfHitTestingPhase(hit_test.phase);
  if (hit_test_self) {
    // Table row and table section are never a hit target.
    // SVG <text> is not a hit target except if 'pointer-events: bounding-box'.
    if (GetPhysicalFragment().IsTableRow() ||
        GetPhysicalFragment().IsTableSection()) {
      hit_test_self = false;
    } else if (fragment.IsSvgText()) {
      pointer_events_bounding_box =
          fragment.Style().UsedPointerEvents() == EPointerEvents::kBoundingBox;
      hit_test_self = pointer_events_bounding_box;
    }
  }

  // Now hit test ourselves.
  if (hit_test_self) {
    if (!IsVisibleToHitTest(fragment, hit_test.result->GetHitTestRequest()))
        [[unlikely]] {
      return false;
    }
    if (fragment.IsOpaque()) [[unlikely]] {
      return false;
    }
  } else if (fragment.IsOpaque() && hit_test.result->HasListBasedResult() &&
             IsVisibleToHitTest(fragment, hit_test.result->GetHitTestRequest()))
      [[unlikely]] {
    // Opaque fragments should not hit, but they are still ancestors in the DOM
    // tree. They should be added to the list-based result as ancestors if
    // descendants hit.
    hit_test_self = true;
  }
  if (hit_test_self) {
    PhysicalRect bounds_rect(physical_offset, size);
    if (hit_test.result->GetHitTestRequest().IsHitTestVisualOverflow())
        [[unlikely]] {
      // We'll include overflow from children here (in addition to self-overflow
      // caused by filters), because we want to record a match if we hit the
      // overflow of a child below the stop node. This matches legacy behavior
      // in LayoutBox::NodeAtPoint(); see call to
      // PhysicalVisualOverflowRectIncludingFilters().
      bounds_rect = InkOverflowIncludingFilters();
      bounds_rect.Move(physical_offset);
    }
    if (pointer_events_bounding_box) [[unlikely]] {
      bounds_rect = PhysicalRect::EnclosingRect(
          GetPhysicalFragment().GetLayoutObject()->ObjectBoundingBox());
    }
    // TODO(kojii): Don't have good explanation why only inline box needs to
    // snap, but matches to legacy and fixes crbug.com/976606.
    if (fragment.IsInlineBox())
      bounds_rect = PhysicalRect(ToPixelSnappedRect(bounds_rect));
    if (hit_test.location.Intersects(bounds_rect)) {
      // We set offset in container block instead of offset in |fragment| like
      // |BoxFragmentPainter::HitTestTextFragment()|.
      // See http://crbug.com/1043471
      DCHECK(!box_item_ || box_item_->BoxFragment() == &fragment);
      if (box_item_ && box_item_->IsInlineBox()) {
        DCHECK(inline_box_cursor_);
        if (hit_test.AddNodeToResultWithContentOffset(
                fragment.NodeForHitTest(),
                inline_box_cursor_->ContainerFragment(), bounds_rect,
                physical_offset - box_item_->OffsetInContainerFragment()))
          return true;
      } else {
        if (UpdateHitTestResultForView(bounds_rect, hit_test))
          return true;
        if (hit_test.AddNodeToResult(fragment.NodeForHitTest(), &box_fragment_,
                                     bounds_rect, physical_offset))
          return true;
      }
    }
  }

  return false;
}

bool BoxFragmentPainter::UpdateHitTestResultForView(
    const PhysicalRect& bounds_rect,
    const HitTestContext& hit_test) const {
  const LayoutObject* layout_object = GetPhysicalFragment().GetLayoutObject();
  if (!layout_object || !layout_object->IsLayoutView() ||
      hit_test.result->InnerNode()) {
    return false;
  }
  auto* element = layout_object->GetDocument().documentElement();
  if (!element)
    return false;
  const auto children = GetPhysicalFragment().Children();
  auto it = base::ranges::find(children, element, &PhysicalFragment::GetNode);
  if (it == children.end())
    return false;
  return hit_test.AddNodeToResultWithContentOffset(
      element, To<PhysicalBoxFragment>(**it), bounds_rect, it->Offset());
}

bool BoxFragmentPainter::HitTestAllPhases(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset) {
  // Logic taken from LayoutObject::HitTestAllPhases().
  if (NodeAtPoint(result, hit_test_location, accumulated_offset,
                  HitTestPhase::kForeground)) {
    return true;
  }
  if (NodeAtPoint(result, hit_test_location, accumulated_offset,
                  HitTestPhase::kFloat)) {
    return true;
  }
  if (NodeAtPoint(result, hit_test_location, accumulated_offset,
                  HitTestPhase::kDescendantBlockBackgrounds)) {
    return true;
  }
  if (NodeAtPoint(result, hit_test_location, accumulated_offset,
                  HitTestPhase::kSelfBlockBackground)) {
    return true;
  }
  return false;
}

bool BoxFragmentPainter::HitTestTextItem(const HitTestContext& hit_test,
                                         const FragmentItem& text_item,
                                         const InlineBackwardCursor& cursor) {
  DCHECK(text_item.IsText());

  if (hit_test.phase != HitTestPhase::kForeground) {
    return false;
  }
  if (!IsVisibleToHitTest(text_item, hit_test.result->GetHitTestRequest())) {
    return false;
  }

  if (text_item.IsSvgText() && text_item.HasSvgTransformForBoundingBox()) {
    const gfx::QuadF quad = text_item.SvgUnscaledQuad();
    if (!hit_test.location.Intersects(quad)) {
      return false;
    }
    return hit_test.AddNodeToResultWithContentOffset(
        text_item.NodeForHitTest(), cursor.ContainerFragment(), quad,
        hit_test.inline_root_offset);
  }

  const auto* const text_combine =
      DynamicTo<LayoutTextCombine>(box_fragment_.GetLayoutObject());

  // TODO(layout-dev): Clip to line-top/bottom.
  PhysicalRect rect;
  if (text_combine) [[unlikely]] {
    rect = text_combine->ComputeTextBoundsRectForHitTest(
        text_item, hit_test.inline_root_offset);
  } else {
    rect = text_item.ComputeTextBoundsRectForHitTest(
        hit_test.inline_root_offset,
        hit_test.result->GetHitTestRequest().IsHitTestVisualOverflow());
  }
  if (!hit_test.location.Intersects(rect))
    return false;

  return hit_test.AddNodeToResultWithContentOffset(
      text_item.NodeForHitTest(), cursor.ContainerFragment(), rect,
      hit_test.inline_root_offset);
}

bool BoxFragmentPainter::HitTestLineBoxFragment(
    const HitTestContext& hit_test,
    const PhysicalLineBoxFragment& fragment,
    const InlineBackwardCursor& cursor,
    const PhysicalOffset& physical_offset) {
  DCHECK_EQ(cursor.Current()->LineBoxFragment(), &fragment);
  PhysicalRect overflow_rect = cursor.Current().InkOverflowRect();
  overflow_rect.Move(physical_offset);
  if (!hit_test.location.Intersects(overflow_rect))
    return false;

  if (HitTestChildren(hit_test, GetPhysicalFragment(),
                      cursor.CursorForDescendants(), physical_offset)) {
    return true;
  }

  if (hit_test.phase != HitTestPhase::kForeground)
    return false;

  if (!IsVisibleToHitTest(box_fragment_, hit_test.result->GetHitTestRequest()))
    return false;

  const PhysicalOffset overflow_location =
      cursor.Current().SelfInkOverflowRect().offset + physical_offset;
  if (HitTestClippedOutByBorder(hit_test.location, overflow_location))
    return false;

  const PhysicalRect bounds_rect(physical_offset, fragment.Size());
  const ComputedStyle& containing_box_style = box_fragment_.Style();
  if (containing_box_style.HasBorderRadius() &&
      !hit_test.location.Intersects(
          RoundedBorderGeometry::PixelSnappedRoundedBorder(containing_box_style,
                                                           bounds_rect)))
    return false;

  if (cursor.ContainerFragment().IsSvgText())
    return false;

  // Now hit test ourselves.
  if (!hit_test.location.Intersects(bounds_rect))
    return false;

  // Floats will be hit-tested in |kHitTestFloat| phase, but
  // |LayoutObject::HitTestAllPhases| does not try it if |kHitTestForeground|
  // succeeds. Pretend the location is not in this linebox if it hits floating
  // descendants. TODO(kojii): Computing this is redundant, consider
  // restructuring. Changing the caller logic isn't easy because currently
  // floats are in the bounds of line boxes only in NG.
  if (fragment.HasFloatingDescendantsForPaint()) {
    DCHECK_NE(hit_test.phase, HitTestPhase::kFloat);
    HitTestResult result;
    HitTestContext hit_test_float{HitTestPhase::kFloat, hit_test.location,
                                  hit_test.inline_root_offset, &result};
    if (HitTestChildren(hit_test_float, GetPhysicalFragment(),
                        cursor.CursorForDescendants(), physical_offset)) {
      return false;
    }
  }

  // |physical_offset| is inside line, but
  //  * Outside of children
  //  * In child without no foreground descendant, e.g. block with size.
  if (cursor.Current()->LineBoxFragment()->IsBlockInInline()) {
    // "fast/events/ondragenter.html" reaches here.
    return false;
  }

  return hit_test.AddNodeToResultWithContentOffset(
      fragment.NodeForHitTest(), box_fragment_, bounds_rect,
      physical_offset - cursor.Current().OffsetInContainerFragment());
}

bool BoxFragmentPainter::HitTestInlineChildBoxFragment(
    const HitTestContext& hit_test,
    const PhysicalBoxFragment& fragment,
    const InlineBackwardCursor& backward_cursor,
    const PhysicalOffset& physical_offset) {
  bool is_in_atomic_painting_pass;

  // Note: Floats should only be hit tested in the |kFloat| phase, so we
  // shouldn't enter a float when |phase| doesn't match. However, as floats may
  // scatter around in the entire inline formatting context, we should always
  // enter non-floating inline child boxes to search for floats in the
  // |kHitTestFloat| phase, unless the child box forms another context.
  if (fragment.IsFloating()) {
    if (hit_test.phase != HitTestPhase::kFloat)
      return false;
    is_in_atomic_painting_pass = true;
  } else {
    is_in_atomic_painting_pass = hit_test.phase == HitTestPhase::kForeground;
  }

  if (fragment.IsPaintedAtomically()) {
    if (!is_in_atomic_painting_pass) {
      return false;
    }
    return HitTestAllPhasesInFragment(fragment, hit_test.location,
                                      physical_offset, hit_test.result);
  }
  InlineCursor cursor(backward_cursor);
  const FragmentItem* item = cursor.Current().Item();
  DCHECK(item);
  DCHECK_EQ(item->BoxFragment(), &fragment);
  if (!fragment.MayIntersect(*hit_test.result, hit_test.location,
                             physical_offset)) {
    return false;
  }

  if (fragment.IsInlineBox()) {
    return BoxFragmentPainter(cursor, *item, fragment, inline_context_)
        .NodeAtPoint(hit_test, physical_offset);
  }

  DCHECK(fragment.IsBlockInInline());
  return BoxFragmentPainter(fragment).NodeAtPoint(hit_test, physical_offset);
}

bool BoxFragmentPainter::HitTestChildBoxItem(
    const HitTestContext& hit_test,
    const PhysicalBoxFragment& container,
    const FragmentItem& item,
    const InlineBackwardCursor& cursor) {
  DCHECK_EQ(&item, cursor.Current().Item());

  // Box fragments for SVG's inline boxes don't have correct geometries.
  if (!item.GetLayoutObject()->IsSVGInline()) {
    const PhysicalBoxFragment* child_fragment = item.BoxFragment();
    DCHECK(child_fragment);
    const PhysicalOffset child_offset =
        hit_test.inline_root_offset + item.OffsetInContainerFragment();
    return HitTestInlineChildBoxFragment(hit_test, *child_fragment, cursor,
                                         child_offset);
  }

  DCHECK(item.GetLayoutObject()->IsLayoutInline());
  if (InlineCursor descendants = cursor.CursorForDescendants()) {
    if (HitTestItemsChildren(hit_test, container, descendants))
      return true;
  }

  DCHECK(cursor.ContainerFragment().IsSvgText());
  if (item.Style().UsedPointerEvents() != EPointerEvents::kBoundingBox)
    return false;
  // Now hit test ourselves.
  if (hit_test.phase != HitTestPhase::kForeground ||
      !IsVisibleToHitTest(item, hit_test.result->GetHitTestRequest()))
    return false;
  // In SVG <text>, we should not refer to the geometry of kBox
  // FragmentItems because they don't have final values.
  auto bounds_rect =
      PhysicalRect::EnclosingRect(item.GetLayoutObject()->ObjectBoundingBox());
  return hit_test.location.Intersects(bounds_rect) &&
         hit_test.AddNodeToResultWithContentOffset(
             item.NodeForHitTest(), cursor.ContainerFragment(), bounds_rect,
             bounds_rect.offset);
}

bool BoxFragmentPainter::HitTestChildren(
    const HitTestContext& hit_test,
    const PhysicalOffset& accumulated_offset) {
  if (inline_box_cursor_) [[unlikely]] {
    InlineCursor descendants = inline_box_cursor_->CursorForDescendants();
    if (descendants) {
      return HitTestChildren(hit_test, GetPhysicalFragment(), descendants,
                             accumulated_offset);
    }
    return false;
  }
  if (items_) {
    const PhysicalBoxFragment& fragment = GetPhysicalFragment();
    InlineCursor cursor(fragment, *items_);
    return HitTestChildren(hit_test, fragment, cursor, accumulated_offset);
  }
  // Check descendants of this fragment because floats may be in the
  // |FragmentItems| of the descendants.
  if (hit_test.phase == HitTestPhase::kFloat) {
    return box_fragment_.HasFloatingDescendantsForPaint() &&
           HitTestFloatingChildren(hit_test, box_fragment_, accumulated_offset);
  }
  return HitTestBlockChildren(*hit_test.result, hit_test.location,
                              accumulated_offset, hit_test.phase);
}

bool BoxFragmentPainter::HitTestChildren(
    const HitTestContext& hit_test,
    const PhysicalBoxFragment& container,
    const InlineCursor& children,
    const PhysicalOffset& accumulated_offset) {
  if (children.HasRoot())
    return HitTestItemsChildren(hit_test, container, children);
  // Hits nothing if there were no children.
  return false;
}

bool BoxFragmentPainter::HitTestBlockChildren(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    PhysicalOffset accumulated_offset,
    HitTestPhase phase) {
  if (phase == HitTestPhase::kDescendantBlockBackgrounds)
    phase = HitTestPhase::kSelfBlockBackground;
  auto children = box_fragment_.Children();
  for (const PhysicalFragmentLink& child : base::Reversed(children)) {
    const auto& block_child = To<PhysicalBoxFragment>(*child);
    if (block_child.IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      continue;
    }
    if (block_child.HasSelfPaintingLayer() || block_child.IsFloating())
      continue;

    const PhysicalOffset child_offset = accumulated_offset + child.offset;

    if (block_child.IsPaintedAtomically()) {
      if (phase != HitTestPhase::kForeground)
        continue;
      if (!HitTestAllPhasesInFragment(block_child, hit_test_location,
                                      child_offset, &result))
        continue;
    } else {
      if (!NodeAtPointInFragment(block_child, hit_test_location, child_offset,
                                 phase, &result))
        continue;
    }

    if (result.InnerNode())
      return true;

    if (Node* node = block_child.NodeForHitTest()) {
      result.SetNodeAndPosition(node, &block_child,
                                hit_test_location.Point() - accumulated_offset);
      return true;
    }

    // Our child may have been an anonymous-block, update the hit-test node
    // to include our node if needed.
    Node* node = box_fragment_.NodeForHitTest();
    if (!node)
      return true;

    // Note: |accumulated_offset| includes container scrolled offset added
    // in |BoxFragmentPainter::NodeAtPoint()|. See http://crbug.com/1268782
    const PhysicalOffset scrolled_offset =
        box_fragment_.IsScrollContainer()
            ? PhysicalOffset(box_fragment_.PixelSnappedScrolledContentOffset())
            : PhysicalOffset();
    result.SetNodeAndPosition(
        node, &box_fragment_,
        hit_test_location.Point() - accumulated_offset - scrolled_offset);
    return true;
  }

  return false;
}

// static
bool BoxFragmentPainter::ShouldHitTestCulledInlineAncestors(
    const HitTestContext& hit_test,
    const FragmentItem& item) {
  if (hit_test.phase != HitTestPhase::kForeground)
    return false;
  if (item.Type() == FragmentItem::kLine) {
    return false;
  }
  if (hit_test.result->GetHitTestRequest().ListBased()) {
    // For list base hit test, we should include culled inline into list.
    // DocumentOrShadowRoot-prototype-elementFromPoint.html requires this.
    return true;
  }
  if (item.IsBlockInInline()) {
    // To handle, empty size <div>, we skip hit testing on culled inline box.
    // See "fast/events/ondragenter.html".
    //
    // Culled inline should be handled by item in another line for block-in-
    // inline, e.g. <span>a<div>b</div></span>.
    return false;
  }
  return true;
}

bool BoxFragmentPainter::HitTestItemsChildren(
    const HitTestContext& hit_test,
    const PhysicalBoxFragment& container,
    const InlineCursor& children) {
  DCHECK(children.HasRoot());
  for (InlineBackwardCursor cursor(children); cursor;) {
    const FragmentItem* item = cursor.Current().Item();
    DCHECK(item);
    if (item->IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      // TODO(crbug.com/1099613): This should not happen, as long as it is
      // really layout-clean.
      NOTREACHED_IN_MIGRATION();
      cursor.MoveToPreviousSibling();
      continue;
    }

    if (item->HasSelfPaintingLayer()) {
      cursor.MoveToPreviousSibling();
      continue;
    }

    if (item->IsText()) {
      if (HitTestTextItem(hit_test, *item, cursor))
        return true;
    } else if (item->Type() == FragmentItem::kLine) {
      const PhysicalLineBoxFragment* child_fragment = item->LineBoxFragment();
      if (child_fragment) {  // Top-level kLine items.
        const PhysicalOffset child_offset =
            hit_test.inline_root_offset + item->OffsetInContainerFragment();
        if (HitTestLineBoxFragment(hit_test, *child_fragment, cursor,
                                   child_offset)) {
          return true;
        }
      } else {  // Nested kLine items for ruby annotations.
        DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
        if (HitTestItemsChildren(hit_test, container,
                                 cursor.CursorForDescendants())) {
          return true;
        }
      }
    } else if (item->Type() == FragmentItem::kBox) {
      if (HitTestChildBoxItem(hit_test, container, *item, cursor))
        return true;
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    cursor.MoveToPreviousSibling();

    if (ShouldHitTestCulledInlineAncestors(hit_test, *item)) {
      // Hit test culled inline boxes between |fragment| and its parent
      // fragment.
      const PhysicalOffset child_offset =
          hit_test.inline_root_offset + item->OffsetInContainerFragment();
      if (HitTestCulledInlineAncestors(*hit_test.result, container, children,
                                       *item, cursor.Current(),
                                       hit_test.location, child_offset))
        return true;
    }
  }

  return false;
}

bool BoxFragmentPainter::HitTestFloatingChildren(
    const HitTestContext& hit_test,
    const PhysicalFragment& container,
    const PhysicalOffset& accumulated_offset) {
  DCHECK_EQ(hit_test.phase, HitTestPhase::kFloat);
  DCHECK(container.HasFloatingDescendantsForPaint());

  if (const auto* box = DynamicTo<PhysicalBoxFragment>(&container)) {
    if (const FragmentItems* items = box->Items()) {
      InlineCursor children(*box, *items);
      if (HitTestFloatingChildItems(hit_test, children, accumulated_offset))
        return true;
      // Even if this turned out to be an inline formatting context, we need to
      // continue walking the box fragment children now. If a float is
      // block-fragmented, it is resumed as a regular box fragment child, rather
      // than becoming a fragment item.
    }
  }

  auto children = container.Children();
  for (const PhysicalFragmentLink& child : base::Reversed(children)) {
    const PhysicalFragment& child_fragment = *child.fragment;
    if (child_fragment.IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      continue;
    }
    if (child_fragment.HasSelfPaintingLayer())
      continue;

    const PhysicalOffset child_offset = accumulated_offset + child.offset;

    if (child_fragment.IsFloating()) {
      if (HitTestAllPhasesInFragment(To<PhysicalBoxFragment>(child_fragment),
                                     hit_test.location, child_offset,
                                     hit_test.result)) {
        return true;
      }
      continue;
    }

    if (child_fragment.IsPaintedAtomically())
      continue;

    if (!child_fragment.HasFloatingDescendantsForPaint())
      continue;

    if (child_fragment.HasNonVisibleOverflow()) {
      // We need to properly visit this fragment for hit-testing, rather than
      // jumping directly to its children (which is what we normally do when
      // looking for floats), in order to set up the clip rectangle.
      if (NodeAtPointInFragment(To<PhysicalBoxFragment>(child_fragment),
                                hit_test.location, child_offset,
                                HitTestPhase::kFloat, hit_test.result)) {
        return true;
      }
      continue;
    }

    if (HitTestFloatingChildren(hit_test, child_fragment, child_offset))
      return true;
  }
  return false;
}

bool BoxFragmentPainter::HitTestFloatingChildItems(
    const HitTestContext& hit_test,
    const InlineCursor& children,
    const PhysicalOffset& accumulated_offset) {
  for (InlineBackwardCursor cursor(children); cursor;
       cursor.MoveToPreviousSibling()) {
    const FragmentItem* item = cursor.Current().Item();
    DCHECK(item);
    if (item->IsLayoutObjectDestroyedOrMoved()) [[unlikely]] {
      continue;
    }
    if (item->Type() == FragmentItem::kBox) {
      if (const PhysicalBoxFragment* child_box = item->BoxFragment()) {
        if (child_box->HasSelfPaintingLayer())
          continue;

        const PhysicalOffset child_offset =
            accumulated_offset + item->OffsetInContainerFragment();
        if (child_box->IsFloating()) {
          if (HitTestAllPhasesInFragment(*child_box, hit_test.location,
                                         child_offset, hit_test.result))
            return true;
          continue;
        }

        // Atomic inline is |IsPaintedAtomically|. |HitTestChildBoxFragment|
        // handles floating descendants in the |kHitTestForeground| phase.
        if (child_box->IsPaintedAtomically())
          continue;
        DCHECK(child_box->IsInlineBox() || child_box->IsBlockInInline());

        // If |child_box| is an inline box, look into descendants because inline
        // boxes do not have |HasFloatingDescendantsForPaint()| flag.
        if (!child_box->IsInlineBox()) {
          if (child_box->HasFloatingDescendantsForPaint()) {
            if (HitTestFloatingChildren(hit_test, *child_box, child_offset))
              return true;
          }
          continue;
        }
      }
      DCHECK(item->GetLayoutObject()->IsLayoutInline());
    } else if (item->Type() == FragmentItem::kLine) {
      const PhysicalLineBoxFragment* child_line = item->LineBoxFragment();
      if (child_line && !child_line->HasFloatingDescendantsForPaint()) {
        continue;
      }
    } else {
      continue;
    }

    InlineCursor descendants = cursor.CursorForDescendants();
    if (HitTestFloatingChildItems(hit_test, descendants, accumulated_offset))
      return true;
  }

  return false;
}

bool BoxFragmentPainter::HitTestClippedOutByBorder(
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& border_box_location) const {
  const ComputedStyle& style = box_fragment_.Style();
  PhysicalRect rect(PhysicalOffset(), GetPhysicalFragment().Size());
  rect.Move(border_box_location);
  return !hit_test_location.Intersects(
      RoundedBorderGeometry::PixelSnappedRoundedBorder(
          style, rect, box_fragment_.SidesToInclude()));
}

bool BoxFragmentPainter::HitTestOverflowControl(
    const HitTestContext& hit_test,
    PhysicalOffset accumulated_offset) {
  const auto* layout_box =
      DynamicTo<LayoutBox>(box_fragment_.GetLayoutObject());
  return layout_box &&
         layout_box->HitTestOverflowControl(*hit_test.result, hit_test.location,
                                            accumulated_offset);
}

gfx::Rect BoxFragmentPainter::VisualRect(const PhysicalOffset& paint_offset) {
  if (const auto* layout_box =
          DynamicTo<LayoutBox>(box_fragment_.GetLayoutObject()))
    return BoxPainter(*layout_box).VisualRect(paint_offset);

  DCHECK(box_item_);
  PhysicalRect ink_overflow = box_item_->InkOverflowRect();
  ink_overflow.Move(paint_offset);
  return ToEnclosingRect(ink_overflow);
}

}  // namespace blink
