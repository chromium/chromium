/*
 * Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/line/ellipsis_box.h"
#include "third_party/blink/renderer/core/layout/line/glyph_overflow.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/vertical_position_cache.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/root_inline_box_painter.h"
#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

struct SameSizeAsRootInlineBox : public InlineFlowBox {
  unsigned unsigned_variable;
  void* pointers[3];
  LayoutUnit layout_variables[6];
};

static_assert(sizeof(RootInlineBox) == sizeof(SameSizeAsRootInlineBox),
              "RootInlineBox should stay small");

typedef WTF::HashMap<const RootInlineBox*, EllipsisBox*> EllipsisBoxMap;
static EllipsisBoxMap* g_ellipsis_box_map = nullptr;

RootInlineBox::RootInlineBox(LineLayoutItem block)
    : InlineFlowBox(block), line_break_pos_(0), line_break_obj_(nullptr) {
  SetIsHorizontal(block.IsHorizontalWritingMode());
}

void RootInlineBox::Destroy() {
  DetachEllipsisBox();
  InlineFlowBox::Destroy();
}

void RootInlineBox::DetachEllipsisBox() {
  if (HasEllipsisBox()) {
    EllipsisBox* box = g_ellipsis_box_map->Take(this);
    box->SetParent(nullptr);
    box->Destroy();
    SetHasEllipsisBox(false);
  }
}

LineBoxList* RootInlineBox::LineBoxes() const {
  return Block().LineBoxes();
}

void RootInlineBox::ClearTruncation() {
  if (HasEllipsisBox()) {
    DetachEllipsisBox();
    InlineFlowBox::ClearTruncation();
  }
}

LayoutUnit RootInlineBox::BaselinePosition(FontBaseline baseline_type) const {
  return BoxModelObject().BaselinePosition(
      baseline_type, IsFirstLineStyle(),
      IsHorizontal() ? kHorizontalLine : kVerticalLine,
      kPositionOfInteriorLineBoxes);
}

LayoutUnit RootInlineBox::LineHeight() const {
  return BoxModelObject().LineHeight(
      IsFirstLineStyle(), IsHorizontal() ? kHorizontalLine : kVerticalLine,
      kPositionOfInteriorLineBoxes);
}

bool RootInlineBox::LineCanAccommodateEllipsis(bool ltr,
                                               LayoutUnit block_edge,
                                               LayoutUnit line_box_edge,
                                               LayoutUnit ellipsis_width) {
  // First sanity-check the unoverflowed width of the whole line to see if there
  // is sufficient room.
  LayoutUnit delta =
      ltr ? line_box_edge - block_edge : block_edge - line_box_edge;
  if (LogicalWidth() - delta < ellipsis_width)
    return false;

  // Next iterate over all the line boxes on the line. If we find a replaced
  // element that intersects then we refuse to accommodate the ellipsis.
  // Otherwise we're ok.
  return InlineFlowBox::CanAccommodateEllipsis(ltr, block_edge, ellipsis_width);
}

LayoutUnit RootInlineBox::PlaceEllipsis(const AtomicString& ellipsis_str,
                                        bool ltr,
                                        LayoutUnit block_left_edge,
                                        LayoutUnit block_right_edge,
                                        LayoutUnit ellipsis_width,
                                        LayoutUnit logical_left_offset,
                                        InlineBox** found_box,
                                        ForceEllipsisOnLine force_ellipsis) {
  // Create an ellipsis box if we don't already have one. If we already have one
  // we're just here to blank out (truncate) the text boxes.
  if (!*found_box) {
    EllipsisBox* ellipsis_box = new EllipsisBox(
        GetLineLayoutItem(), ellipsis_str, this, ellipsis_width,
        LogicalHeight(), Location(), !PrevRootBox(), IsHorizontal());

    if (!g_ellipsis_box_map)
      g_ellipsis_box_map = new EllipsisBoxMap();
    g_ellipsis_box_map->insert(this, ellipsis_box);
    SetHasEllipsisBox(true);
  }

  // FIXME: Do we need an RTL version of this?
  LayoutUnit adjusted_logical_left = logical_left_offset + LogicalLeft();
  if (force_ellipsis == ForceEllipsis && ltr &&
      (adjusted_logical_left + LogicalWidth() + ellipsis_width) <=
          block_right_edge) {
    if (HasEllipsisBox())
      GetEllipsisBox()->SetLogicalLeft(LogicalLeft() + LogicalWidth());
    return LogicalWidth() + ellipsis_width;
  }

  // Now attempt to find the nearest glyph horizontally and place just to the
  // right (or left in RTL) of that glyph.  Mark all of the objects that
  // intersect the ellipsis box as not painting (as being truncated).
  LayoutUnit truncated_width;
  LayoutUnit position =
      PlaceEllipsisBox(ltr, block_left_edge, block_right_edge, ellipsis_width,
                       truncated_width, found_box, logical_left_offset);
  if (HasEllipsisBox())
    GetEllipsisBox()->SetLogicalLeft(position);
  return truncated_width;
}

LayoutUnit RootInlineBox::PlaceEllipsisBox(bool ltr,
                                           LayoutUnit block_left_edge,
                                           LayoutUnit block_right_edge,
                                           LayoutUnit ellipsis_width,
                                           LayoutUnit& truncated_width,
                                           InlineBox** found_box,
                                           LayoutUnit logical_left_offset) {
  LayoutUnit result = InlineFlowBox::PlaceEllipsisBox(
      ltr, block_left_edge, block_right_edge, ellipsis_width, truncated_width,
      found_box, logical_left_offset);
  if (result == -1) {
    result = ltr ? std::max<LayoutUnit>(
                       LayoutUnit(),
                       block_right_edge - ellipsis_width - logical_left_offset)
                 : block_left_edge - logical_left_offset;
    truncated_width = block_right_edge - block_left_edge - logical_left_offset;
  }
  return result;
}

void RootInlineBox::Paint(const PaintInfo& paint_info,
                          const LayoutPoint& paint_offset,
                          LayoutUnit line_top,
                          LayoutUnit line_bottom) const {
  RootInlineBoxPainter(*this).Paint(paint_info, paint_offset, line_top,
                                    line_bottom);
}

bool RootInlineBox::NodeAtPoint(HitTestResult& result,
                                const HitTestLocation& hit_test_location,
                                const PhysicalOffset& accumulated_offset,
                                LayoutUnit line_top,
                                LayoutUnit line_bottom) {
  if (HasEllipsisBox() && VisibleToHitTestRequest(result.GetHitTestRequest())) {
    if (GetEllipsisBox()->NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, line_top,
                                      line_bottom)) {
      GetLineLayoutItem().UpdateHitTestResult(
          result, hit_test_location.Point() - accumulated_offset);
      return true;
    }
  }
  return InlineFlowBox::NodeAtPoint(result, hit_test_location,
                                    accumulated_offset, line_top, line_bottom);
}

void RootInlineBox::Move(const LayoutSize& delta) {
  InlineFlowBox::Move(delta);
  LayoutUnit block_direction_delta =
      IsHorizontal() ? delta.Height() : delta.Width();
  line_top_ += block_direction_delta;
  line_bottom_ += block_direction_delta;
  line_top_with_leading_ += block_direction_delta;
  line_bottom_with_leading_ += block_direction_delta;
  selection_bottom_ += block_direction_delta;
  if (HasEllipsisBox())
    GetEllipsisBox()->Move(delta);
}

void RootInlineBox::ChildRemoved(InlineBox* box) {
  if (box->GetLineLayoutItem() == line_break_obj_)
    SetLineBreakInfo(nullptr, 0, BidiStatus());

  for (RootInlineBox* prev = PrevRootBox();
       prev && prev->LineBreakObj() == box->GetLineLayoutItem();
       prev = prev->PrevRootBox()) {
    prev->SetLineBreakInfo(nullptr, 0, BidiStatus());
    prev->MarkDirty();
  }
}

static inline void ApplyLineHeightStep(uint8_t line_height_step,
                                       LayoutUnit& max_ascent,
                                       LayoutUnit& max_descent) {
  // Round up to the multiple of units, by adding spaces to over/under equally.
  // https://drafts.csswg.org/css-rhythm/#line-height-step
  int remainder = (max_ascent + max_descent).ToInt() % line_height_step;
  if (!remainder)
    return;
  DCHECK_GT(remainder, 0);
  int space = line_height_step - remainder;
  max_descent += space / 2;
  max_ascent += space - space / 2;
}

LayoutUnit RootInlineBox::AlignBoxesInBlockDirection(
    LayoutUnit height_of_block,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map,
    VerticalPositionCache& vertical_position_cache) {
  // SVG will handle vertical alignment on its own.
  if (IsSVGRootInlineBox())
    return LayoutUnit();

  LayoutUnit max_position_top;
  LayoutUnit max_position_bottom;
  LayoutUnit max_ascent;
  LayoutUnit max_descent;
  bool set_max_ascent = false;
  bool set_max_descent = false;

  // Figure out if we're in no-quirks mode.
  bool no_quirks_mode = GetLineLayoutItem().GetDocument().InNoQuirksMode();

  baseline_type_ = DominantBaseline();

  ComputeLogicalBoxHeights(this, max_position_top, max_position_bottom,
                           max_ascent, max_descent, set_max_ascent,
                           set_max_descent, no_quirks_mode, text_box_data_map,
                           BaselineType(), vertical_position_cache);

  if (max_ascent + max_descent <
      std::max(max_position_top, max_position_bottom))
    AdjustMaxAscentAndDescent(max_ascent, max_descent, max_position_top.ToInt(),
                              max_position_bottom.ToInt());

  if (uint8_t line_height_step =
          GetLineLayoutItem().StyleRef().LineHeightStep())
    ApplyLineHeightStep(line_height_step, max_ascent, max_descent);

  LayoutUnit max_height = LayoutUnit(max_ascent + max_descent);
  LayoutUnit line_top = height_of_block;
  LayoutUnit line_bottom = height_of_block;
  LayoutUnit line_top_including_margins = height_of_block;
  LayoutUnit line_bottom_including_margins = height_of_block;
  LayoutUnit selection_bottom = height_of_block;
  bool set_line_top = false;
  bool has_annotations_before = false;
  bool has_annotations_after = false;
  PlaceBoxesInBlockDirection(
      height_of_block, max_height, max_ascent, no_quirks_mode, line_top,
      line_bottom, selection_bottom, set_line_top, line_top_including_margins,
      line_bottom_including_margins, has_annotations_before,
      has_annotations_after, BaselineType());
  has_annotations_before_ = has_annotations_before;
  has_annotations_after_ = has_annotations_after;

  max_height = max_height.ClampNegativeToZero();

  SetLineTopBottomPositions(line_top, line_bottom, height_of_block,
                            height_of_block + max_height, selection_bottom);

  LayoutUnit annotations_adjustment = BeforeAnnotationsAdjustment();
  if (annotations_adjustment) {
    // FIXME: Need to handle pagination here. We might have to move to the next
    // page/column as a result of the ruby expansion.
    MoveInBlockDirection(annotations_adjustment);
    height_of_block += annotations_adjustment;
  }

  return height_of_block + max_height;
}

LayoutUnit RootInlineBox::BeforeAnnotationsAdjustment() const {
  LayoutUnit result;

  if (!GetLineLayoutItem().StyleRef().IsFlippedLinesWritingMode()) {
    // Annotations under the previous line may push us down.
    if (PrevRootBox() && PrevRootBox()->HasAnnotationsAfter())
      result = PrevRootBox()->ComputeUnderAnnotationAdjustment(LineTop());

    if (!HasAnnotationsBefore())
      return result;

    // Annotations over this line may push us further down.
    LayoutUnit highest_allowed_position =
        PrevRootBox()
            ? std::min(PrevRootBox()->LineBottom(), LineTop()) + result
            : static_cast<LayoutUnit>(Block().BorderBefore());
    result = ComputeOverAnnotationAdjustment(highest_allowed_position);
  } else {
    // Annotations under this line may push us up.
    if (HasAnnotationsBefore())
      result = ComputeUnderAnnotationAdjustment(
          PrevRootBox() ? PrevRootBox()->LineBottom()
                        : static_cast<LayoutUnit>(Block().BorderBefore()));

    if (!PrevRootBox() || !PrevRootBox()->HasAnnotationsAfter())
      return result;

    // We have to compute the expansion for annotations over the previous line
    // to see how much we should move.
    LayoutUnit lowest_allowed_position =
        std::max(PrevRootBox()->LineBottom(), LineTop()) - result;
    result =
        PrevRootBox()->ComputeOverAnnotationAdjustment(lowest_allowed_position);
  }

  return result;
}

bool RootInlineBox::IsSelected() const {
  // Walk over all of the selected boxes.
  for (InlineBox* box = FirstLeafChild(); box; box = box->NextLeafChild()) {
    if (box->IsSelected())
      return true;
  }
  return false;
}

InlineBox* RootInlineBox::FirstSelectedBox() const {
  for (InlineBox* box = FirstLeafChild(); box; box = box->NextLeafChild()) {
    if (box->IsSelected())
      return box;
  }

  return nullptr;
}

InlineBox* RootInlineBox::LastSelectedBox() const {
  for (InlineBox* box = LastLeafChild(); box; box = box->PrevLeafChild()) {
    if (box->IsSelected())
      return box;
  }

  return nullptr;
}

LayoutUnit RootInlineBox::SelectionTop() const {
  LayoutUnit selection_top = line_top_;
  if (has_annotations_before_)
    selection_top -= !GetLineLayoutItem().StyleRef().IsFlippedLinesWritingMode()
                         ? ComputeOverAnnotationAdjustment(line_top_)
                         : ComputeUnderAnnotationAdjustment(line_top_);

  if (GetLineLayoutItem().StyleRef().IsFlippedLinesWritingMode() ||
      !PrevRootBox())
    return selection_top;

  return std::min(selection_top, PrevRootBox()->SelectionBottom());
}

LayoutUnit RootInlineBox::SelectionBottom() const {
  LayoutUnit selection_bottom =
      GetLineLayoutItem().GetDocument().InNoQuirksMode() ? selection_bottom_
                                                         : line_bottom_;

  if (has_annotations_after_)
    selection_bottom +=
        !GetLineLayoutItem().StyleRef().IsFlippedLinesWritingMode()
            ? ComputeUnderAnnotationAdjustment(line_bottom_)
            : ComputeOverAnnotationAdjustment(line_bottom_);

  if (!GetLineLayoutItem().StyleRef().IsFlippedLinesWritingMode() ||
      !NextRootBox())
    return selection_bottom;

  return std::max(selection_bottom, NextRootBox()->SelectionTop());
}

LayoutUnit RootInlineBox::BlockDirectionPointInLine() const {
  return !Block().StyleRef().IsFlippedBlocksWritingMode()
             ? std::max(LineTop(), SelectionTop())
             : std::min(LineBottom(), SelectionBottom());
}

LineLayoutBlockFlow RootInlineBox::Block() const {
  return LineLayoutBlockFlow(GetLineLayoutItem());
}

static bool IsEditableLeaf(InlineBox* leaf) {
  return leaf && leaf->GetLineLayoutItem().GetNode() &&
         HasEditableStyle(*leaf->GetLineLayoutItem().GetNode());
}

const LayoutObject* RootInlineBox::ClosestLeafChildForPoint(
    const LayoutPoint& point_in_contents,
    bool only_editable_leaves) const {
  InlineBox* closest_box = ClosestLeafChildForLogicalLeftPosition(
      Block().IsHorizontalWritingMode() ? point_in_contents.X()
                                        : point_in_contents.Y(),
      only_editable_leaves);
  return LineLayoutAPIShim::LayoutObjectFrom(closest_box->GetLineLayoutItem());
}

InlineBox* RootInlineBox::ClosestLeafChildForLogicalLeftPosition(
    LayoutUnit left_position,
    bool only_editable_leaves) const {
  InlineBox* first_leaf = FirstLeafChild();
  InlineBox* last_leaf = LastLeafChild();

  if (first_leaf != last_leaf) {
    if (first_leaf->IsLineBreak())
      first_leaf = first_leaf->NextLeafChildIgnoringLineBreak();
    else if (last_leaf->IsLineBreak())
      last_leaf = last_leaf->PrevLeafChildIgnoringLineBreak();
  }

  if (first_leaf == last_leaf &&
      (!only_editable_leaves || IsEditableLeaf(first_leaf)))
    return first_leaf;

  // Avoid returning a list marker when possible.
  if (left_position <= first_leaf->LogicalLeft() &&
      !first_leaf->GetLineLayoutItem().IsListMarker() &&
      (!only_editable_leaves || IsEditableLeaf(first_leaf))) {
    // The leftPosition coordinate is less or equal to left edge of the
    // firstLeaf. Return it.
    return first_leaf;
  }

  if (left_position >= last_leaf->LogicalRight() &&
      !last_leaf->GetLineLayoutItem().IsListMarker() &&
      (!only_editable_leaves || IsEditableLeaf(last_leaf))) {
    // The leftPosition coordinate is greater or equal to right edge of the
    // lastLeaf. Return it.
    return last_leaf;
  }

  InlineBox* closest_leaf = nullptr;
  for (InlineBox* leaf = first_leaf; leaf;
       leaf = leaf->NextLeafChildIgnoringLineBreak()) {
    if (!leaf->GetLineLayoutItem().IsListMarker() &&
        (!only_editable_leaves || IsEditableLeaf(leaf))) {
      closest_leaf = leaf;
      if (left_position < leaf->LogicalRight()) {
        // The x coordinate is less than the right edge of the box.
        // Return it.
        return leaf;
      }
    }
  }

  return closest_leaf ? closest_leaf : last_leaf;
}

BidiStatus RootInlineBox::LineBreakBidiStatus() const {
  return BidiStatus(
      static_cast<WTF::unicode::CharDirection>(line_break_bidi_status_eor_),
      static_cast<WTF::unicode::CharDirection>(
          line_break_bidi_status_last_strong_),
      static_cast<WTF::unicode::CharDirection>(line_break_bidi_status_last_),
      line_break_context_);
}

void RootInlineBox::SetLineBreakInfo(LineLayoutItem obj,
                                     unsigned break_pos,
                                     const BidiStatus& status) {
  // When setting lineBreakObj, the LayoutObject must not be a LayoutInline
  // with no line boxes, otherwise all sorts of invariants are broken later.
  // This has security implications because if the LayoutObject does not point
  // to at least one line box, then that LayoutInline can be deleted later
  // without resetting the lineBreakObj, leading to use-after-free.
  SECURITY_DCHECK(!obj || obj.IsText() ||
                  !(obj.IsLayoutInline() && obj.IsBox() &&
                    !LineLayoutBox(obj).InlineBoxWrapper()));

  line_break_obj_ = obj;
  line_break_pos_ = break_pos;
  line_break_bidi_status_eor_ = status.eor;
  line_break_bidi_status_last_strong_ = status.last_strong;
  line_break_bidi_status_last_ = status.last;
  line_break_context_ = status.context;
}

EllipsisBox* RootInlineBox::GetEllipsisBox() const {
  if (!HasEllipsisBox())
    return nullptr;
  return g_ellipsis_box_map->at(this);
}

void RootInlineBox::RemoveLineBoxFromLayoutObject() {
  Block().LineBoxes()->RemoveLineBox(this);
}

void RootInlineBox::ExtractLineBoxFromLayoutObject() {
  Block().LineBoxes()->ExtractLineBox(this);
}

void RootInlineBox::AttachLineBoxToLayoutObject() {
  Block().LineBoxes()->AttachLineBox(this);
}

LayoutRect RootInlineBox::PaddedLayoutOverflowRect(
    LayoutUnit end_padding) const {
  LayoutRect line_layout_overflow = LayoutOverflowRect(LineTop(), LineBottom());
  if (!end_padding)
    return line_layout_overflow;

  if (IsHorizontal()) {
    if (IsLeftToRightDirection())
      line_layout_overflow.ShiftMaxXEdgeTo(std::max<LayoutUnit>(
          line_layout_overflow.MaxX(), LogicalRight() + end_padding));
    else
      line_layout_overflow.ShiftXEdgeTo(std::min<LayoutUnit>(
          line_layout_overflow.X(), LogicalLeft() - end_padding));
  } else {
    if (IsLeftToRightDirection())
      line_layout_overflow.ShiftMaxYEdgeTo(std::max<LayoutUnit>(
          line_layout_overflow.MaxY(), LogicalRight() + end_padding));
    else
      line_layout_overflow.ShiftYEdgeTo(std::min<LayoutUnit>(
          line_layout_overflow.Y(), LogicalLeft() - end_padding));
  }

  return line_layout_overflow;
}

static void SetAscentAndDescent(LayoutUnit& ascent,
                                LayoutUnit& descent,
                                LayoutUnit new_ascent,
                                LayoutUnit new_descent,
                                bool& ascent_descent_set) {
  if (!ascent_descent_set) {
    ascent_descent_set = true;
    ascent = new_ascent;
    descent = new_descent;
  } else {
    ascent = std::max(ascent, new_ascent);
    descent = std::max(descent, new_descent);
  }
}

void RootInlineBox::AscentAndDescentForBox(
    InlineBox* box,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map,
    LayoutUnit& ascent,
    LayoutUnit& descent,
    bool& affects_ascent,
    bool& affects_descent) const {
  bool ascent_descent_set = false;

  if (box->GetLineLayoutItem().IsAtomicInlineLevel()) {
    ascent = box->BaselinePosition(BaselineType());
    descent = box->LineHeight() - ascent;

    // Replaced elements always affect both the ascent and descent.
    affects_ascent = true;
    affects_descent = true;
    return;
  }

  Vector<const SimpleFontData*>* used_fonts = nullptr;
  if (box->IsText()) {
    GlyphOverflowAndFallbackFontsMap::iterator it =
        text_box_data_map.find(ToInlineTextBox(box));
    used_fonts = it == text_box_data_map.end() ? nullptr : &it->value.first;
  }

  bool include_leading = IncludeLeadingForBox(box);
  bool set_used_font_with_leading = false;

  if (used_fonts && !used_fonts->IsEmpty() &&
      (box->GetLineLayoutItem()
           .Style(IsFirstLineStyle())
           ->LineHeight()
           .IsNegative() &&
       include_leading)) {
    const SimpleFontData* primary_font = box->GetLineLayoutItem()
                                             .Style(IsFirstLineStyle())
                                             ->GetFont()
                                             .PrimaryFont();
    if (primary_font)
      used_fonts->push_back(primary_font);
    for (const SimpleFontData* font_data : *used_fonts) {
      const FontMetrics& font_metrics = font_data->GetFontMetrics();
      LayoutUnit used_font_ascent(font_metrics.Ascent(BaselineType()));
      LayoutUnit used_font_descent(font_metrics.Descent(BaselineType()));
      LayoutUnit half_leading(
          (font_metrics.LineSpacing() - font_metrics.Height()) / 2);
      LayoutUnit used_font_ascent_and_leading = used_font_ascent + half_leading;
      LayoutUnit used_font_descent_and_leading =
          font_metrics.LineSpacing() - used_font_ascent_and_leading;
      if (include_leading) {
        SetAscentAndDescent(ascent, descent, used_font_ascent_and_leading,
                            used_font_descent_and_leading, ascent_descent_set);
        set_used_font_with_leading = true;
      }
      if (!affects_ascent)
        affects_ascent = used_font_ascent - box->LogicalTop() > 0;
      if (!affects_descent)
        affects_descent = used_font_descent + box->LogicalTop() > 0;
    }
  }

  // If leading is included for the box, then we compute that box.
  if (include_leading && !set_used_font_with_leading) {
    LayoutUnit ascent_with_leading = box->BaselinePosition(BaselineType());
    LayoutUnit descent_with_leading = box->LineHeight() - ascent_with_leading;
    SetAscentAndDescent(ascent, descent, ascent_with_leading,
                        descent_with_leading, ascent_descent_set);

    // Examine the font box for inline flows and text boxes to see if any part
    // of it is above the baseline. If the top of our font box relative to the
    // root box baseline is above the root box baseline, then we are
    // contributing to the maxAscent value. Descent is similar. If any part of
    // our font box is below the root box's baseline, then we contribute to the
    // maxDescent value.
    affects_ascent = ascent_with_leading - box->LogicalTop() > 0;
    affects_descent = descent_with_leading + box->LogicalTop() > 0;
  }
}

LayoutUnit RootInlineBox::VerticalPositionForBox(
    InlineBox* box,
    VerticalPositionCache& vertical_position_cache) {
  if (box->GetLineLayoutItem().IsText())
    return box->Parent()->LogicalTop();

  LineLayoutBoxModel box_model = box->BoxModelObject();
  DCHECK(box_model.IsInline());
  if (!box_model.IsInline())
    return LayoutUnit();

  // This method determines the vertical position for inline elements.
  bool first_line = IsFirstLineStyle();
  if (first_line &&
      !box_model.GetDocument().GetStyleEngine().UsesFirstLineRules())
    first_line = false;

  // Check the cache.
  bool is_layout_inline = box_model.IsLayoutInline();
  if (is_layout_inline && !first_line) {
    LayoutUnit vertical_position =
        LayoutUnit(vertical_position_cache.Get(box_model, BaselineType()));
    if (vertical_position != kPositionUndefined)
      return vertical_position;
  }

  LayoutUnit vertical_position;
  EVerticalAlign vertical_align = box_model.StyleRef().VerticalAlign();
  if (vertical_align == EVerticalAlign::kTop ||
      vertical_align == EVerticalAlign::kBottom)
    return LayoutUnit();

  LineLayoutItem parent = box_model.Parent();
  if (parent.IsLayoutInline() &&
      parent.StyleRef().VerticalAlign() != EVerticalAlign::kTop &&
      parent.StyleRef().VerticalAlign() != EVerticalAlign::kBottom)
    vertical_position = box->Parent()->LogicalTop();

  if (vertical_align != EVerticalAlign::kBaseline) {
    const Font& font = parent.Style(first_line)->GetFont();
    const SimpleFontData* font_data = font.PrimaryFont();
    DCHECK(font_data);
    if (!font_data)
      return LayoutUnit();

    const FontMetrics& font_metrics = font_data->GetFontMetrics();
    int font_size = font.GetFontDescription().ComputedPixelSize();

    LineDirectionMode line_direction =
        parent.IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine;

    if (vertical_align == EVerticalAlign::kSub) {
      vertical_position += font_size / 5 + 1;
    } else if (vertical_align == EVerticalAlign::kSuper) {
      vertical_position -= font_size / 3 + 1;
    } else if (vertical_align == EVerticalAlign::kTextTop) {
      vertical_position += box_model.BaselinePosition(
                               BaselineType(), first_line, line_direction) -
                           font_metrics.Ascent(BaselineType());
    } else if (vertical_align == EVerticalAlign::kMiddle) {
      vertical_position = vertical_position -
                          LayoutUnit(font_metrics.XHeight() / 2) -
                          box_model.LineHeight(first_line, line_direction) / 2 +
                          box_model.BaselinePosition(BaselineType(), first_line,
                                                     line_direction);
    } else if (vertical_align == EVerticalAlign::kTextBottom) {
      vertical_position += font_metrics.Descent(BaselineType());
      // lineHeight - baselinePosition is always 0 for replaced elements (except
      // inline blocks), so don't bother wasting time in that case.
      if (!box_model.IsAtomicInlineLevel() ||
          box_model.IsInlineBlockOrInlineTable())
        vertical_position -= (box_model.LineHeight(first_line, line_direction) -
                              box_model.BaselinePosition(
                                  BaselineType(), first_line, line_direction));
    } else if (vertical_align == EVerticalAlign::kBaselineMiddle) {
      vertical_position +=
          -box_model.LineHeight(first_line, line_direction) / 2 +
          box_model.BaselinePosition(BaselineType(), first_line,
                                     line_direction);
    } else if (vertical_align == EVerticalAlign::kLength) {
      LayoutUnit line_height;
      // Per http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align:
      // 'Percentages: refer to the 'line-height' of the element itself'.
      if (box_model.StyleRef().GetVerticalAlignLength().IsPercentOrCalc())
        line_height = LayoutUnit(box_model.StyleRef().ComputedLineHeight());
      else
        line_height = box_model.LineHeight(first_line, line_direction);
      vertical_position -= ValueForLength(
          box_model.StyleRef().GetVerticalAlignLength(), line_height);
    }
  }

  // Store the cached value.
  if (is_layout_inline && !first_line)
    vertical_position_cache.Set(box_model, BaselineType(),
                                vertical_position.ToInt());

  return vertical_position;
}

bool RootInlineBox::IncludeLeadingForBox(InlineBox* box) const {
  return !(box->GetLineLayoutItem().IsAtomicInlineLevel() ||
           (box->GetLineLayoutItem().IsText() && !box->IsText()));
}

void RootInlineBox::CollectLeafBoxesInLogicalOrder(
    Vector<InlineBox*>& leaf_boxes_in_logical_order,
    CustomInlineBoxRangeReverse custom_reverse_implementation) const {
  InlineBox* leaf = FirstLeafChild();

  // FIXME: The reordering code is a copy of parts from BidiResolver::
  // createBidiRunsForLine, operating directly on InlineBoxes, instead of
  // BidiRuns. Investigate on how this code could possibly be shared.
  unsigned char min_level = 128;
  unsigned char max_level = 0;

  // First find highest and lowest levels, and initialize
  // leafBoxesInLogicalOrder with the leaf boxes in visual order.
  for (; leaf; leaf = leaf->NextLeafChild()) {
    min_level = std::min(min_level, leaf->BidiLevel());
    max_level = std::max(max_level, leaf->BidiLevel());
    leaf_boxes_in_logical_order.push_back(leaf);
  }

  if (GetLineLayoutItem().StyleRef().RtlOrdering() == EOrder::kVisual)
    return;

  // Reverse of reordering of the line (L2 according to Bidi spec):
  // L2. From the highest level found in the text to the lowest odd level on
  // each line, reverse any contiguous sequence of characters that are at that
  // level or higher.

  // Reversing the reordering of the line is only done up to the lowest odd
  // level.
  if (!(min_level % 2))
    ++min_level;

  Vector<InlineBox*>::iterator end = leaf_boxes_in_logical_order.end();
  while (min_level <= max_level) {
    Vector<InlineBox*>::iterator it = leaf_boxes_in_logical_order.begin();
    while (it != end) {
      while (it != end) {
        if ((*it)->BidiLevel() >= min_level)
          break;
        ++it;
      }
      Vector<InlineBox*>::iterator first = it;
      while (it != end) {
        if ((*it)->BidiLevel() < min_level)
          break;
        ++it;
      }
      Vector<InlineBox*>::iterator last = it;
      if (custom_reverse_implementation)
        (*custom_reverse_implementation)(first, last);
      else
        std::reverse(first, last);
    }
    ++min_level;
  }
}

const InlineBox* RootInlineBox::GetLogicalStartNonPseudoBox() const {
  Vector<InlineBox*> leaf_boxes_in_logical_order;
  CollectLeafBoxesInLogicalOrder(leaf_boxes_in_logical_order);
  for (InlineBox* box : leaf_boxes_in_logical_order) {
    if (box->GetLineLayoutItem().NonPseudoNode())
      return box;
  }
  return nullptr;
}

const InlineBox* RootInlineBox::GetLogicalEndNonPseudoBox() const {
  Vector<InlineBox*> leaf_boxes_in_logical_order;
  CollectLeafBoxesInLogicalOrder(leaf_boxes_in_logical_order);
  for (wtf_size_t i = leaf_boxes_in_logical_order.size(); i > 0; --i) {
    if (leaf_boxes_in_logical_order[i - 1]
            ->GetLineLayoutItem()
            .NonPseudoNode()) {
      return leaf_boxes_in_logical_order[i - 1];
    }
  }
  return nullptr;
}

const char* RootInlineBox::BoxName() const {
  return "RootInlineBox";
}

}  // namespace blink
