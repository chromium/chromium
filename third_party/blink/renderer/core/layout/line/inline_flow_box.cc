/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc.
 *               All rights reserved.
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

#include "third_party/blink/renderer/core/layout/line/inline_flow_box.h"

#include <math.h>
#include <algorithm>
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_inline.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_ruby_text.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/line/glyph_overflow.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/line_orientation_utils.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/inline_flow_box_painter.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"

namespace blink {

struct SameSizeAsInlineFlowBox : public InlineBox {
  void* pointers[5];
  uint32_t bitfields : 23;
};

static_assert(sizeof(InlineFlowBox) == sizeof(SameSizeAsInlineFlowBox),
              "InlineFlowBox should stay small");

#if DCHECK_IS_ON()
InlineFlowBox::~InlineFlowBox() {
  if (!has_bad_child_list_)
    for (InlineBox* child = FirstChild(); child; child = child->NextOnLine())
      child->SetHasBadParent();
}
#endif

LayoutUnit InlineFlowBox::GetFlowSpacingLogicalWidth() {
  LayoutUnit tot_width =
      MarginBorderPaddingLogicalLeft() + MarginBorderPaddingLogicalRight();
  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    if (curr->IsInlineFlowBox())
      tot_width += ToInlineFlowBox(curr)->GetFlowSpacingLogicalWidth();
  }
  return tot_width;
}

LayoutRect InlineFlowBox::FrameRect() const {
  return LayoutRect(Location(), Size());
}

static void SetHasTextDescendantsOnAncestors(InlineFlowBox* box) {
  while (box && !box->HasTextDescendants()) {
    box->SetHasTextDescendants();
    box = box->Parent();
  }
}

static inline bool HasIdenticalLineHeightProperties(
    const ComputedStyle& parent_style,
    const ComputedStyle& child_style,
    bool is_root) {
  return parent_style.HasIdenticalAscentDescentAndLineGap(child_style) &&
         parent_style.LineHeight() == child_style.LineHeight() &&
         (parent_style.VerticalAlign() == EVerticalAlign::kBaseline ||
          is_root) &&
         child_style.VerticalAlign() == EVerticalAlign::kBaseline;
}

inline bool InlineFlowBox::HasEmphasisMarkBefore(
    const InlineTextBox* text_box) const {
  TextEmphasisPosition emphasis_mark_position;
  const auto& style =
      text_box->GetLineLayoutItem().StyleRef(IsFirstLineStyle());
  if (!text_box->GetEmphasisMarkPosition(style, emphasis_mark_position))
    return false;
  LineLogicalSide side = style.GetTextEmphasisLineLogicalSide();
  if (IsHorizontal() || !style.IsFlippedLinesWritingMode())
    return side == LineLogicalSide::kOver;
  return side == LineLogicalSide::kUnder;
}

inline bool InlineFlowBox::HasEmphasisMarkOver(
    const InlineTextBox* text_box) const {
  const auto& style =
      text_box->GetLineLayoutItem().StyleRef(IsFirstLineStyle());
  TextEmphasisPosition emphasis_mark_position;
  return text_box->GetEmphasisMarkPosition(style, emphasis_mark_position) &&
         style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver;
}

inline bool InlineFlowBox::HasEmphasisMarkUnder(
    const InlineTextBox* text_box) const {
  const auto& style =
      text_box->GetLineLayoutItem().StyleRef(IsFirstLineStyle());
  TextEmphasisPosition emphasis_mark_position;
  return text_box->GetEmphasisMarkPosition(style, emphasis_mark_position) &&
         style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kUnder;
}

void InlineFlowBox::AddToLine(InlineBox* child) {
  DCHECK(!child->Parent());
  DCHECK(!child->NextOnLine());
  DCHECK(!child->PrevOnLine());

  child->SetParent(this);
  if (!first_child_) {
    first_child_ = child;
    last_child_ = child;
  } else {
    last_child_->SetNextOnLine(child);
    child->SetPrevOnLine(last_child_);
    last_child_ = child;
  }
  child->SetFirstLineStyleBit(IsFirstLineStyle());
  child->SetIsHorizontal(IsHorizontal());
  if (child->IsText()) {
    if (child->GetLineLayoutItem().Parent() == GetLineLayoutItem())
      has_text_children_ = true;
    SetHasTextDescendantsOnAncestors(this);
  } else if (child->IsInlineFlowBox()) {
    if (ToInlineFlowBox(child)->HasTextDescendants())
      SetHasTextDescendantsOnAncestors(this);
  }

  if (DescendantsHaveSameLineHeightAndBaseline() &&
      !child->GetLineLayoutItem().IsOutOfFlowPositioned()) {
    const ComputedStyle& parent_style =
        GetLineLayoutItem().StyleRef(IsFirstLineStyle());
    const ComputedStyle& child_style =
        child->GetLineLayoutItem().StyleRef(IsFirstLineStyle());
    bool root = IsRootInlineBox();
    bool should_clear_descendants_have_same_line_height_and_baseline = false;
    if (child->GetLineLayoutItem().IsAtomicInlineLevel()) {
      should_clear_descendants_have_same_line_height_and_baseline = true;
    } else if (child->IsText()) {
      if (child->GetLineLayoutItem().IsBR() ||
          (child->GetLineLayoutItem().Parent() != GetLineLayoutItem())) {
        if (!HasIdenticalLineHeightProperties(parent_style, child_style, root))
          should_clear_descendants_have_same_line_height_and_baseline = true;
      }
      if (child_style.HasTextCombine() ||
          child_style.GetTextEmphasisMark() != TextEmphasisMark::kNone)
        should_clear_descendants_have_same_line_height_and_baseline = true;
    } else {
      if (child->GetLineLayoutItem().IsBR()) {
        // FIXME: This is dumb. We only turn off because current web test
        // results expect the <br> to be 0-height on the baseline.
        // Other than making a zillion tests have to regenerate results, there's
        // no reason to ditch the optimization here.
        should_clear_descendants_have_same_line_height_and_baseline = true;
      } else {
        DCHECK(IsInlineFlowBox());
        InlineFlowBox* child_flow_box = ToInlineFlowBox(child);
        // Check the child's bit, and then also check for differences in font,
        // line-height, vertical-align
        if (!child_flow_box->DescendantsHaveSameLineHeightAndBaseline() ||
            !HasIdenticalLineHeightProperties(parent_style, child_style,
                                              root) ||
            child_style.HasBorder() || child_style.MayHavePadding() ||
            child_style.HasTextCombine()) {
          should_clear_descendants_have_same_line_height_and_baseline = true;
        }
      }
    }

    if (should_clear_descendants_have_same_line_height_and_baseline)
      ClearDescendantsHaveSameLineHeightAndBaseline();
  }

  if (!child->GetLineLayoutItem().IsOutOfFlowPositioned()) {
    if (child->IsText()) {
      const ComputedStyle& child_style =
          child->GetLineLayoutItem().StyleRef(IsFirstLineStyle());
      if (child_style.LetterSpacing() < 0 || child_style.TextShadow() ||
          child_style.GetTextEmphasisMark() != TextEmphasisMark::kNone ||
          child_style.TextStrokeWidth() || child->IsLineBreak())
        child->ClearKnownToHaveNoOverflow();
    } else if (child->GetLineLayoutItem().IsAtomicInlineLevel()) {
      LineLayoutBox box = LineLayoutBox(child->GetLineLayoutItem());
      if (box.HasLayoutOverflow() || box.HasVisualOverflow() ||
          box.HasSelfPaintingLayer())
        child->ClearKnownToHaveNoOverflow();
    } else if (!child->GetLineLayoutItem().IsBR() &&
               (child->GetLineLayoutItem()
                    .Style(IsFirstLineStyle())
                    ->BoxShadow() ||
                child->BoxModelObject().HasSelfPaintingLayer() ||
                (child->GetLineLayoutItem().IsListMarker() &&
                 !LineLayoutListMarker(child->GetLineLayoutItem())
                      .IsInside()) ||
                child->GetLineLayoutItem()
                    .Style(IsFirstLineStyle())
                    ->HasBorderImageOutsets() ||
                child->GetLineLayoutItem()
                    .Style(IsFirstLineStyle())
                    ->HasOutline())) {
      child->ClearKnownToHaveNoOverflow();
    }

    if (KnownToHaveNoOverflow() && child->IsInlineFlowBox() &&
        !ToInlineFlowBox(child)->KnownToHaveNoOverflow())
      ClearKnownToHaveNoOverflow();
  }
}

void InlineFlowBox::RemoveChild(InlineBox* child, MarkLineBoxes mark_dirty) {
  if (mark_dirty == kMarkLineBoxesDirty && !IsDirty())
    DirtyLineBoxes();

  Root().ChildRemoved(child);

  if (child == first_child_)
    first_child_ = child->NextOnLine();
  if (child == last_child_)
    last_child_ = child->PrevOnLine();
  if (child->NextOnLine())
    child->NextOnLine()->SetPrevOnLine(child->PrevOnLine());
  if (child->PrevOnLine())
    child->PrevOnLine()->SetNextOnLine(child->NextOnLine());

  child->SetParent(nullptr);
}

void InlineFlowBox::DeleteLine() {
  InlineBox* child = FirstChild();
  InlineBox* next = nullptr;
  while (child) {
    DCHECK_EQ(this, child->Parent());
    next = child->NextOnLine();
#if DCHECK_IS_ON()
    child->SetParent(nullptr);
#endif
    child->DeleteLine();
    child = next;
  }
#if DCHECK_IS_ON()
  first_child_ = nullptr;
  last_child_ = nullptr;
#endif

  RemoveLineBoxFromLayoutObject();
  Destroy();
}

void InlineFlowBox::RemoveLineBoxFromLayoutObject() {
  LineBoxes()->RemoveLineBox(this);
}

void InlineFlowBox::ExtractLine() {
  if (!Extracted())
    ExtractLineBoxFromLayoutObject();
  for (InlineBox* child = FirstChild(); child; child = child->NextOnLine())
    child->ExtractLine();
}

void InlineFlowBox::ExtractLineBoxFromLayoutObject() {
  LineBoxes()->ExtractLineBox(this);
}

void InlineFlowBox::AttachLine() {
  if (Extracted())
    AttachLineBoxToLayoutObject();
  for (InlineBox* child = FirstChild(); child; child = child->NextOnLine())
    child->AttachLine();
}

void InlineFlowBox::AttachLineBoxToLayoutObject() {
  LineBoxes()->AttachLineBox(this);
}

void InlineFlowBox::Move(const LayoutSize& delta) {
  InlineBox::Move(delta);
  for (InlineBox* child = FirstChild(); child; child = child->NextOnLine()) {
    if (child->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;
    child->Move(delta);
  }
  // FIXME: Rounding error here since overflow was pixel snapped, but nobody
  // other than list markers passes non-integral values here.
  if (LayoutOverflowIsSet())
    overflow_->layout_overflow->Move(delta.Width(), delta.Height());
  if (VisualOverflowIsSet())
    overflow_->visual_overflow->Move(delta.Width(), delta.Height());
}

LineBoxList* InlineFlowBox::LineBoxes() const {
  return LineLayoutInline(GetLineLayoutItem()).LineBoxes();
}

static inline bool IsLastChildForLayoutObject(LineLayoutItem ancestor,
                                              LineLayoutItem child) {
  if (!child)
    return false;

  if (child == ancestor)
    return true;

  LineLayoutItem curr = child;
  LineLayoutItem parent = curr.Parent();
  while (parent && (!parent.IsLayoutBlock() || parent.IsInline())) {
    if (parent.SlowLastChild() != curr)
      return false;
    if (parent == ancestor)
      return true;

    curr = parent;
    parent = curr.Parent();
  }

  return true;
}

static bool IsAncestorAndWithinBlock(LineLayoutItem ancestor,
                                     LineLayoutItem child) {
  LineLayoutItem item = child;
  while (item && (!item.IsLayoutBlock() || item.IsInline())) {
    if (item == ancestor)
      return true;
    item = item.Parent();
  }
  return false;
}

void InlineFlowBox::DetermineSpacingForFlowBoxes(
    bool last_line,
    bool is_logically_last_run_wrapped,
    LineLayoutItem logically_last_run_layout_object) {
  // All boxes start off open.  They will not apply any margins/border/padding
  // on any side.
  bool include_left_edge = false;
  bool include_right_edge = false;

  // The root inline box never has borders/margins/padding.
  if (Parent()) {
    bool ltr = GetLineLayoutItem().StyleRef().IsLeftToRightDirection();

    // Check to see if all initial lines are unconstructed.  If so, then
    // we know the inline began on this line (unless we are a continuation).
    LineBoxList* line_box_list = LineBoxes();
    if (!line_box_list->First()->IsConstructed() &&
        !GetLineLayoutItem().IsInlineElementContinuation()) {
      if (GetLineLayoutItem().StyleRef().BoxDecorationBreak() ==
          EBoxDecorationBreak::kClone)
        include_left_edge = include_right_edge = true;
      else if (ltr && line_box_list->First() == this)
        include_left_edge = true;
      else if (!ltr && line_box_list->Last() == this)
        include_right_edge = true;
    }

    if (!line_box_list->Last()->IsConstructed()) {
      LineLayoutInline inline_flow = LineLayoutInline(GetLineLayoutItem());
      LineLayoutItem logically_last_run_layout_item(
          logically_last_run_layout_object);
      bool is_last_object_on_line =
          !IsAncestorAndWithinBlock(GetLineLayoutItem(),
                                    logically_last_run_layout_item) ||
          (IsLastChildForLayoutObject(GetLineLayoutItem(),
                                      logically_last_run_layout_item) &&
           !is_logically_last_run_wrapped);

      // We include the border under these conditions:
      // (1) The next line was not created, or it is constructed. We check the
      //     previous line for rtl.
      // (2) The logicallyLastRun is not a descendant of this layout object.
      // (3) The logicallyLastRun is a descendant of this layout object, but it
      //     is the last child of this layout object and it does not wrap to the
      //     next line.
      // (4) The decoration break is set to clone therefore there will be
      //     borders on every sides.
      if (GetLineLayoutItem().StyleRef().BoxDecorationBreak() ==
          EBoxDecorationBreak::kClone) {
        include_left_edge = include_right_edge = true;
      } else if (ltr) {
        if (!NextForSameLayoutObject() &&
            ((last_line || is_last_object_on_line) &&
             !inline_flow.Continuation()))
          include_right_edge = true;
      } else {
        if ((!PrevForSameLayoutObject() ||
             PrevForSameLayoutObject()->IsConstructed()) &&
            ((last_line || is_last_object_on_line) &&
             !inline_flow.Continuation()))
          include_left_edge = true;
      }
    }
  }

  SetEdges(include_left_edge, include_right_edge);

  // Recur into our children.
  for (InlineBox* curr_child = FirstChild(); curr_child;
       curr_child = curr_child->NextOnLine()) {
    if (curr_child->IsInlineFlowBox()) {
      InlineFlowBox* curr_flow = ToInlineFlowBox(curr_child);
      curr_flow->DetermineSpacingForFlowBoxes(last_line,
                                              is_logically_last_run_wrapped,
                                              logically_last_run_layout_object);
    }
  }
}

LayoutUnit InlineFlowBox::PlaceBoxesInInlineDirection(
    LayoutUnit logical_left,
    bool& needs_word_spacing) {
  // Set our x position.
  BeginPlacingBoxRangesInInlineDirection(logical_left);

  LayoutUnit start_logical_left = logical_left;
  logical_left += BorderLogicalLeft() + PaddingLogicalLeft();

  LayoutUnit min_logical_left = start_logical_left;
  LayoutUnit max_logical_right = logical_left;

  PlaceBoxRangeInInlineDirection(FirstChild(), nullptr, logical_left,
                                 min_logical_left, max_logical_right,
                                 needs_word_spacing);

  logical_left += BorderLogicalRight() + PaddingLogicalRight();
  EndPlacingBoxRangesInInlineDirection(start_logical_left, logical_left,
                                       min_logical_left, max_logical_right);
  return logical_left;
}

// TODO(wkorman): needsWordSpacing may not need to be a reference in the below.
// Seek a test case.
void InlineFlowBox::PlaceBoxRangeInInlineDirection(
    InlineBox* first_child,
    InlineBox* last_child,
    LayoutUnit& logical_left,
    LayoutUnit& min_logical_left,
    LayoutUnit& max_logical_right,
    bool& needs_word_spacing) {
  for (InlineBox* curr = first_child; curr && curr != last_child;
       curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsText()) {
      InlineTextBox* text = ToInlineTextBox(curr);
      LineLayoutText rt = text->GetLineLayoutItem();
      LayoutUnit space;
      if (rt.TextLength()) {
        if (needs_word_spacing &&
            IsSpaceOrNewline(rt.CharacterAt(text->Start())))
          space = LayoutUnit(rt.Style(IsFirstLineStyle())
                                 ->GetFont()
                                 .GetFontDescription()
                                 .WordSpacing());
        needs_word_spacing = !IsSpaceOrNewline(rt.CharacterAt(text->end()));
      }
      if (IsLeftToRightDirection()) {
        logical_left += space;
        text->SetLogicalLeft(logical_left);
      } else {
        text->SetLogicalLeft(logical_left);
        logical_left += space;
      }
      if (KnownToHaveNoOverflow())
        min_logical_left = std::min(logical_left, min_logical_left);
      logical_left += text->LogicalWidth();
      if (KnownToHaveNoOverflow())
        max_logical_right = std::max(logical_left, max_logical_right);
    } else {
      if (curr->GetLineLayoutItem().IsOutOfFlowPositioned()) {
        if (curr->GetLineLayoutItem()
                .Parent()
                .StyleRef()
                .IsLeftToRightDirection()) {
          curr->SetLogicalLeft(logical_left);
        } else {
          // Our offset that we cache needs to be from the edge of the right
          // border box and not the left border box. We have to subtract |x|
          // from the width of the block (which can be obtained from the root
          // line box).
          curr->SetLogicalLeft(Root().Block().LogicalWidth() - logical_left);
        }
        continue;  // The positioned object has no effect on the width.
      }
      if (curr->GetLineLayoutItem().IsLayoutInline()) {
        InlineFlowBox* flow = ToInlineFlowBox(curr);
        logical_left += flow->MarginLogicalLeft();
        if (KnownToHaveNoOverflow())
          min_logical_left = std::min(logical_left, min_logical_left);
        logical_left =
            flow->PlaceBoxesInInlineDirection(logical_left, needs_word_spacing);
        if (KnownToHaveNoOverflow())
          max_logical_right = std::max(logical_left, max_logical_right);
        logical_left += flow->MarginLogicalRight();
      } else if (!curr->GetLineLayoutItem().IsListMarker() ||
                 LineLayoutListMarker(curr->GetLineLayoutItem()).IsInside()) {
        // The box can have a different writing-mode than the overall line, so
        // this is a bit complicated. Just get all the physical margin and
        // overflow values by hand based off |isHorizontal|.
        LineLayoutBoxModel box = curr->BoxModelObject();
        LayoutUnit logical_left_margin;
        LayoutUnit logical_right_margin;
        if (IsHorizontal()) {
          logical_left_margin = box.MarginLeft();
          logical_right_margin = box.MarginRight();
        } else {
          logical_left_margin = box.MarginTop();
          logical_right_margin = box.MarginBottom();
        }

        logical_left += logical_left_margin;
        curr->SetLogicalLeft(logical_left);
        if (KnownToHaveNoOverflow())
          min_logical_left = std::min(logical_left, min_logical_left);
        logical_left += curr->LogicalWidth();
        if (KnownToHaveNoOverflow())
          max_logical_right = std::max(logical_left, max_logical_right);
        logical_left += logical_right_margin;
        // If we encounter any space after this inline block then ensure it is
        // treated as the space between two words.
        needs_word_spacing = true;
      }
    }
  }
}

FontBaseline InlineFlowBox::DominantBaseline() const {
  // Use "central" (Ideographic) baseline if writing-mode is vertical-* and
  // text-orientation is not sideways-*.
  // https://drafts.csswg.org/css-writing-modes/#text-baselines
  if (!IsHorizontal() && GetLineLayoutItem()
                             .Style(IsFirstLineStyle())
                             ->GetFontDescription()
                             .IsVerticalAnyUpright())
    return kIdeographicBaseline;
  return kAlphabeticBaseline;
}

void InlineFlowBox::AdjustMaxAscentAndDescent(LayoutUnit& max_ascent,
                                              LayoutUnit& max_descent,
                                              int max_position_top,
                                              int max_position_bottom) {
  LayoutUnit original_max_ascent(max_ascent);
  LayoutUnit original_max_descent(max_descent);
  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    // The computed lineheight needs to be extended for the
    // positioned elements
    if (curr->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;  // Positioned placeholders don't affect calculations.
    if (curr->VerticalAlign() == EVerticalAlign::kTop ||
        curr->VerticalAlign() == EVerticalAlign::kBottom) {
      int line_height = curr->LineHeight().Round();
      if (curr->VerticalAlign() == EVerticalAlign::kTop) {
        if (max_ascent + max_descent < line_height)
          max_descent = line_height - max_ascent;
      } else {
        if (max_ascent + max_descent < line_height)
          max_ascent = line_height - max_descent;
      }

      if (max_ascent + max_descent >=
          std::max(max_position_top, max_position_bottom))
        break;
      max_ascent = original_max_ascent;
      max_descent = original_max_descent;
    }

    if (curr->IsInlineFlowBox())
      ToInlineFlowBox(curr)->AdjustMaxAscentAndDescent(
          max_ascent, max_descent, max_position_top, max_position_bottom);
  }
}

void InlineFlowBox::ComputeLogicalBoxHeights(
    RootInlineBox* root_box,
    LayoutUnit& max_position_top,
    LayoutUnit& max_position_bottom,
    LayoutUnit& max_ascent,
    LayoutUnit& max_descent,
    bool& set_max_ascent,
    bool& set_max_descent,
    bool no_quirks_mode,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map,
    FontBaseline baseline_type,
    VerticalPositionCache& vertical_position_cache) {
  // The primary purpose of this function is to compute the maximal ascent and
  // descent values for a line.
  //
  // The maxAscent value represents the distance of the highest point of any box
  // (typically including line-height) from the root box's baseline. The
  // maxDescent value represents the distance of the lowest point of any box
  // (also typically including line-height) from the root box baseline. These
  // values can be negative.
  //
  // A secondary purpose of this function is to store the offset of every box's
  // baseline from the root box's baseline. This information is cached in the
  // logicalTop() of every box. We're effectively just using the logicalTop() as
  // scratch space.
  //
  // Because a box can be positioned such that it ends up fully above or fully
  // below the root line box, we only consider it to affect the maxAscent and
  // maxDescent values if some part of the box (EXCLUDING leading) is above (for
  // ascent) or below (for descent) the root box's baseline.
  bool affects_ascent = false;
  bool affects_descent = false;
  bool check_children = !DescendantsHaveSameLineHeightAndBaseline();

  DCHECK(root_box);
  if (!root_box)
    return;

  if (IsRootInlineBox()) {
    // Examine our root box.
    LayoutUnit ascent;
    LayoutUnit descent;
    root_box->AscentAndDescentForBox(root_box, text_box_data_map, ascent,
                                     descent, affects_ascent, affects_descent);
    if (no_quirks_mode || HasTextChildren() ||
        (!check_children && HasTextDescendants())) {
      if (max_ascent < ascent || !set_max_ascent) {
        max_ascent = ascent;
        set_max_ascent = true;
      }
      if (max_descent < descent || !set_max_descent) {
        max_descent = descent;
        set_max_descent = true;
      }
    }
  }

  if (!check_children)
    return;

  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;  // Positioned placeholders don't affect calculations.

    InlineFlowBox* inline_flow_box =
        curr->IsInlineFlowBox() ? ToInlineFlowBox(curr) : nullptr;

    bool affects_ascent = false;
    bool affects_descent = false;

    // The verticalPositionForBox function returns the distance between the
    // child box's baseline and the root box's baseline. The value is negative
    // if the child box's baseline is above the root box's baseline, and it is
    // positive if the child box's baseline is below the root box's baseline.
    DCHECK(root_box);
    curr->SetLogicalTop(
        root_box->VerticalPositionForBox(curr, vertical_position_cache));

    LayoutUnit ascent;
    LayoutUnit descent;
    root_box->AscentAndDescentForBox(curr, text_box_data_map, ascent, descent,
                                     affects_ascent, affects_descent);

    LayoutUnit box_height(ascent + descent);
    if (curr->VerticalAlign() == EVerticalAlign::kTop) {
      if (max_position_top < box_height)
        max_position_top = box_height;
    } else if (curr->VerticalAlign() == EVerticalAlign::kBottom) {
      if (max_position_bottom < box_height)
        max_position_bottom = box_height;
    } else if (!inline_flow_box || no_quirks_mode ||
               inline_flow_box->HasTextChildren() ||
               (inline_flow_box->DescendantsHaveSameLineHeightAndBaseline() &&
                inline_flow_box->HasTextDescendants()) ||
               inline_flow_box->BoxModelObject()
                   .HasInlineDirectionBordersOrPadding()) {
      // Note that these values can be negative. Even though we only affect the
      // maxAscent and maxDescent values if our box (excluding line-height) was
      // above (for ascent) or below (for descent) the root baseline, once you
      // factor in line-height the final box can end up being fully above or
      // fully below the root box's baseline! This is ok, but what it means is
      // that ascent and descent (including leading), can end up being negative.
      // The setMaxAscent and setMaxDescent booleans are used to ensure that
      // we're willing to initially set maxAscent/Descent to negative values.
      ascent -= curr->LogicalTop();
      descent += curr->LogicalTop();
      if (affects_ascent && (max_ascent < ascent || !set_max_ascent)) {
        max_ascent = ascent;
        set_max_ascent = true;
      }

      if (affects_descent && (max_descent < descent || !set_max_descent)) {
        max_descent = descent;
        set_max_descent = true;
      }
    }

    if (inline_flow_box)
      inline_flow_box->ComputeLogicalBoxHeights(
          root_box, max_position_top, max_position_bottom, max_ascent,
          max_descent, set_max_ascent, set_max_descent, no_quirks_mode,
          text_box_data_map, baseline_type, vertical_position_cache);
  }
}

void InlineFlowBox::PlaceBoxesInBlockDirection(
    LayoutUnit top,
    LayoutUnit max_height,
    LayoutUnit max_ascent,
    bool no_quirks_mode,
    LayoutUnit& line_top,
    LayoutUnit& line_bottom,
    LayoutUnit& selection_bottom,
    bool& set_line_top,
    LayoutUnit& line_top_including_margins,
    LayoutUnit& line_bottom_including_margins,
    bool& has_annotations_before,
    bool& has_annotations_after,
    FontBaseline baseline_type) {
  bool is_root_box = IsRootInlineBox();
  if (is_root_box) {
    const SimpleFontData* font_data =
        GetLineLayoutItem().Style(IsFirstLineStyle())->GetFont().PrimaryFont();
    DCHECK(font_data);
    if (!font_data)
      return;
    const FontMetrics& font_metrics = font_data->GetFontMetrics();
    // RootInlineBoxes are always placed at pixel boundaries in their logical y
    // direction. Not doing so results in incorrect layout of text decorations,
    // most notably underlines.
    SetLogicalTop(top + max_ascent - font_metrics.Ascent(baseline_type));
  }

  LayoutUnit adjustment_for_children_with_same_line_height_and_baseline;
  if (DescendantsHaveSameLineHeightAndBaseline()) {
    adjustment_for_children_with_same_line_height_and_baseline = LogicalTop();
    if (Parent())
      adjustment_for_children_with_same_line_height_and_baseline +=
          BoxModelObject().BorderAndPaddingOver();
  }

  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;  // Positioned placeholders don't affect calculations.

    if (DescendantsHaveSameLineHeightAndBaseline()) {
      curr->MoveInBlockDirection(
          adjustment_for_children_with_same_line_height_and_baseline);
      continue;
    }

    InlineFlowBox* inline_flow_box =
        curr->IsInlineFlowBox() ? ToInlineFlowBox(curr) : nullptr;
    bool child_affects_top_bottom_pos = true;
    if (curr->VerticalAlign() == EVerticalAlign::kTop) {
      curr->SetLogicalTop(top);
    } else if (curr->VerticalAlign() == EVerticalAlign::kBottom) {
      curr->SetLogicalTop((top + max_height - curr->LineHeight()));
    } else {
      if (!no_quirks_mode && inline_flow_box &&
          !inline_flow_box->HasTextChildren() &&
          !curr->BoxModelObject().HasInlineDirectionBordersOrPadding() &&
          !(inline_flow_box->DescendantsHaveSameLineHeightAndBaseline() &&
            inline_flow_box->HasTextDescendants()))
        child_affects_top_bottom_pos = false;
      LayoutUnit pos_adjust =
          max_ascent - curr->BaselinePosition(baseline_type);
      curr->SetLogicalTop(curr->LogicalTop() + top + pos_adjust);
    }

    LayoutUnit new_logical_top = curr->LogicalTop();
    LayoutUnit new_logical_top_including_margins = new_logical_top;
    LayoutUnit box_height = curr->LogicalHeight();
    LayoutUnit box_height_including_margins = box_height;
    LayoutUnit border_padding_height;
    if (curr->IsText() || curr->IsInlineFlowBox()) {
      const SimpleFontData* font_data = curr->GetLineLayoutItem()
                                            .Style(IsFirstLineStyle())
                                            ->GetFont()
                                            .PrimaryFont();
      DCHECK(font_data);
      if (!font_data)
        continue;

      const FontMetrics& font_metrics = font_data->GetFontMetrics();
      new_logical_top += curr->BaselinePosition(baseline_type) -
                         font_metrics.Ascent(baseline_type);
      if (curr->IsInlineFlowBox()) {
        LineLayoutBoxModel box_object =
            LineLayoutBoxModel(curr->GetLineLayoutItem());
        new_logical_top -= box_object.BorderAndPaddingOver();
        border_padding_height = box_object.BorderAndPaddingLogicalHeight();
      }
      new_logical_top_including_margins = new_logical_top;
    } else if (!curr->GetLineLayoutItem().IsBR()) {
      LineLayoutBox box = LineLayoutBox(curr->GetLineLayoutItem());
      new_logical_top_including_margins = new_logical_top;
      // TODO(kojii): isHorizontal() does not match to
      // m_layoutObject.isHorizontalWritingMode(). crbug.com/552954
      // DCHECK_EQ(curr->isHorizontal(),
      // curr->getLineLayoutItem().style()->isHorizontalWritingMode());
      // We may flip lines in case of verticalLR mode, so we can
      // assume verticalRL for now.
      LayoutUnit over_side_margin =
          curr->IsHorizontal() ? box.MarginTop() : box.MarginRight();
      LayoutUnit under_side_margin =
          curr->IsHorizontal() ? box.MarginBottom() : box.MarginLeft();
      new_logical_top += over_side_margin;
      box_height_including_margins += over_side_margin + under_side_margin;
    }

    curr->SetLogicalTop(new_logical_top);

    if (child_affects_top_bottom_pos) {
      if (curr->GetLineLayoutItem().IsRubyRun()) {
        // Treat the leading on the first and last lines of ruby runs as not
        // being part of the overall lineTop/lineBottom.
        // Really this is a workaround hack for the fact that ruby should have
        // been done as line layout and not done using inline-block.
        if (GetLineLayoutItem().StyleRef().IsFlippedLinesWritingMode() ==
            (curr->GetLineLayoutItem().StyleRef().GetRubyPosition() ==
             RubyPosition::kAfter))
          has_annotations_before = true;
        else
          has_annotations_after = true;

        LineLayoutRubyRun ruby_run =
            LineLayoutRubyRun(curr->GetLineLayoutItem());
        if (LineLayoutRubyBase ruby_base = ruby_run.RubyBase()) {
          LayoutUnit bottom_ruby_base_leading =
              (curr->LogicalHeight() - ruby_base.LogicalBottom()) +
              ruby_base.LogicalHeight() -
              (ruby_base.LastRootBox() ? ruby_base.LastRootBox()->LineBottom()
                                       : LayoutUnit());
          LayoutUnit top_ruby_base_leading =
              ruby_base.LogicalTop() +
              (ruby_base.FirstRootBox() ? ruby_base.FirstRootBox()->LineTop()
                                        : LayoutUnit());
          new_logical_top +=
              !GetLineLayoutItem().StyleRef().IsFlippedLinesWritingMode()
                  ? top_ruby_base_leading
                  : bottom_ruby_base_leading;
          box_height -= (top_ruby_base_leading + bottom_ruby_base_leading);
        }
      }
      if (curr->IsInlineTextBox()) {
        TextEmphasisPosition emphasis_mark_position;
        if (ToInlineTextBox(curr)->GetEmphasisMarkPosition(
                curr->GetLineLayoutItem().StyleRef(IsFirstLineStyle()),
                emphasis_mark_position)) {
          if (HasEmphasisMarkBefore(ToInlineTextBox(curr)))
            has_annotations_before = true;
          else
            has_annotations_after = true;
        }
      }

      if (!set_line_top) {
        set_line_top = true;
        line_top = new_logical_top;
        line_top_including_margins =
            std::min(line_top, new_logical_top_including_margins);
      } else {
        line_top = std::min(line_top, new_logical_top);
        line_top_including_margins =
            std::min(line_top, std::min(line_top_including_margins,
                                        new_logical_top_including_margins));
      }
      selection_bottom =
          std::max(selection_bottom,
                   new_logical_top + box_height - border_padding_height);
      line_bottom = std::max(line_bottom, new_logical_top + box_height);
      line_bottom_including_margins =
          std::max(line_bottom, std::max(line_bottom_including_margins,
                                         new_logical_top_including_margins +
                                             box_height_including_margins));
    }

    // Adjust boxes to use their real box y/height and not the logical height
    // (as dictated by line-height).
    if (inline_flow_box)
      inline_flow_box->PlaceBoxesInBlockDirection(
          top, max_height, max_ascent, no_quirks_mode, line_top, line_bottom,
          selection_bottom, set_line_top, line_top_including_margins,
          line_bottom_including_margins, has_annotations_before,
          has_annotations_after, baseline_type);
  }

  if (is_root_box) {
    if (no_quirks_mode || HasTextChildren() ||
        (DescendantsHaveSameLineHeightAndBaseline() && HasTextDescendants())) {
      if (!set_line_top) {
        set_line_top = true;
        line_top = LogicalTop();
        line_top_including_margins = line_top;
      } else {
        line_top = std::min(line_top, LogicalTop());
        line_top_including_margins =
            std::min(line_top, line_top_including_margins);
      }
      selection_bottom = std::max(selection_bottom, LogicalBottom());
      line_bottom = std::max(line_bottom, LogicalBottom());
      line_bottom_including_margins =
          std::max(line_bottom, line_bottom_including_margins);
    }

    if (GetLineLayoutItem().StyleRef().IsFlippedLinesWritingMode())
      FlipLinesInBlockDirection(line_top_including_margins,
                                line_bottom_including_margins);
  }
}

bool InlineFlowBox::OverrideVisualOverflowFromLogicalRect(
    const LayoutRect& logical_visual_overflow,
    LayoutUnit line_top,
    LayoutUnit line_bottom) {
  // If we are setting an overflow, then we can't pretend not to have an
  // overflow.
  ClearKnownToHaveNoOverflow();
  LayoutRect old_visual_overflow_rect =
      VisualOverflowRect(line_top, line_bottom);
  SetVisualOverflowFromLogicalRect(logical_visual_overflow, line_top,
                                   line_bottom);
  return VisualOverflowRect(line_top, line_bottom) != old_visual_overflow_rect;
}

void InlineFlowBox::OverrideLayoutOverflowFromLogicalRect(
    const LayoutRect& logical_layout_overflow,
    LayoutUnit line_top,
    LayoutUnit line_bottom) {
  // If we are setting an overflow, then we can't pretend not to have an
  // overflow.
  ClearKnownToHaveNoOverflow();
  SetLayoutOverflowFromLogicalRect(logical_layout_overflow, line_top,
                                   line_bottom);
}

LayoutUnit InlineFlowBox::FarthestPositionForUnderline(
    LineLayoutItem decorating_box,
    FontVerticalPositionType position_type,
    FontBaseline baseline_type,
    LayoutUnit farthest) const {
  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;  // Positioned placeholders don't affect calculations.

    // If the text decoration isn't in effect on the child, it must be outside
    // of |decorationObject|.
    if (!EnumHasFlags(curr->LineStyleRef().TextDecorationsInEffect(),
                      TextDecoration::kUnderline))
      continue;

    if (decorating_box && decorating_box.IsLayoutInline() &&
        !IsAncestorAndWithinBlock(decorating_box, curr->GetLineLayoutItem()))
      continue;

    if (curr->IsInlineFlowBox()) {
      farthest = ToInlineFlowBox(curr)->FarthestPositionForUnderline(
          decorating_box, position_type, baseline_type, farthest);
    } else if (curr->IsInlineTextBox()) {
      LayoutUnit position =
          ToInlineTextBox(curr)->VerticalPosition(position_type, baseline_type);
      if (IsLineOverSide(position_type))
        farthest = std::min(farthest, position);
      else
        farthest = std::max(farthest, position);
    }
  }
  return farthest;
}

void InlineFlowBox::FlipLinesInBlockDirection(LayoutUnit line_top,
                                              LayoutUnit line_bottom) {
  // Flip the box on the line such that the top is now relative to the
  // lineBottom instead of the lineTop.
  SetLogicalTop(line_bottom - (LogicalTop() - line_top) - LogicalHeight());

  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;  // Positioned placeholders aren't affected here.

    if (curr->IsInlineFlowBox())
      ToInlineFlowBox(curr)->FlipLinesInBlockDirection(line_top, line_bottom);
    else
      curr->SetLogicalTop(line_bottom - (curr->LogicalTop() - line_top) -
                          curr->LogicalHeight());
  }
}

inline void InlineFlowBox::AddBoxShadowVisualOverflow(
    LayoutRect& logical_visual_overflow) {
  const ComputedStyle& style = GetLineLayoutItem().StyleRef(IsFirstLineStyle());

  // box-shadow on the block element applies to the block and not to the lines,
  // unless it is modified by :first-line pseudo element.
  if (!Parent() &&
      (!IsFirstLineStyle() || &style == GetLineLayoutItem().Style()))
    return;

  WritingMode writing_mode = style.GetWritingMode();
  ShadowList* box_shadow = style.BoxShadow();
  if (!box_shadow)
    return;

  LayoutRectOutsets outsets(box_shadow->RectOutsetsIncludingOriginal());
  // Similar to how glyph overflow works, if our lines are flipped, then it's
  // actually the opposite shadow that applies, since the line is "upside down"
  // in terms of block coordinates.
  LayoutRectOutsets logical_outsets =
      LineOrientationLayoutRectOutsetsWithFlippedLines(outsets, writing_mode);

  LayoutRect shadow_bounds(LogicalFrameRect());
  shadow_bounds.Expand(logical_outsets);
  logical_visual_overflow.Unite(shadow_bounds);
}

inline void InlineFlowBox::AddBorderOutsetVisualOverflow(
    LayoutRect& logical_visual_overflow) {
  const ComputedStyle& style = GetLineLayoutItem().StyleRef(IsFirstLineStyle());

  // border-image-outset on the block element applies to the block and not to
  // the lines, unless it is modified by :first-line pseudo element.
  if (!Parent() &&
      (!IsFirstLineStyle() || &style == GetLineLayoutItem().Style()))
    return;

  if (!style.HasBorderImageOutsets())
    return;

  // Similar to how glyph overflow works, if our lines are flipped, then it's
  // actually the opposite border that applies, since the line is "upside down"
  // in terms of block coordinates. vertical-rl is the flipped line mode.
  LayoutRectOutsets logical_outsets =
      LineOrientationLayoutRectOutsetsWithFlippedLines(
          style.BorderImageOutsets(), style.GetWritingMode());

  if (!IncludeLogicalLeftEdge())
    logical_outsets.SetLeft(LayoutUnit());
  if (!IncludeLogicalRightEdge())
    logical_outsets.SetRight(LayoutUnit());

  LayoutRect border_outset_bounds(LogicalFrameRect());
  border_outset_bounds.Expand(logical_outsets);
  logical_visual_overflow.Unite(border_outset_bounds);
}

inline void InlineFlowBox::AddOutlineVisualOverflow(
    LayoutRect& logical_visual_overflow) {
  // Outline on root line boxes is applied to the block and not to the lines.
  if (!Parent())
    return;

  const ComputedStyle& style = GetLineLayoutItem().StyleRef(IsFirstLineStyle());
  if (!style.HasOutline())
    return;

  logical_visual_overflow.Inflate(style.OutlineOutsetExtent());
}

inline void InlineFlowBox::AddTextBoxVisualOverflow(
    InlineTextBox* text_box,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map,
    LayoutRect& logical_visual_overflow) {
  if (text_box->KnownToHaveNoOverflow())
    return;

  LayoutRectOutsets visual_rect_outsets;
  const ComputedStyle& style =
      text_box->GetLineLayoutItem().StyleRef(IsFirstLineStyle());

  GlyphOverflowAndFallbackFontsMap::iterator it =
      text_box_data_map.find(text_box);
  if (it != text_box_data_map.end()) {
    const GlyphOverflow& glyph_overflow = it->value.second;
    bool is_flipped_line = style.IsFlippedLinesWritingMode();
    visual_rect_outsets = EnclosingLayoutRectOutsets(FloatRectOutsets(
        is_flipped_line ? glyph_overflow.bottom : glyph_overflow.top,
        glyph_overflow.right,
        is_flipped_line ? glyph_overflow.top : glyph_overflow.bottom,
        glyph_overflow.left));
  }

  if (float stroke_width = style.TextStrokeWidth()) {
    visual_rect_outsets += LayoutUnit::FromFloatCeil(stroke_width / 2.0f);
  }

  TextEmphasisPosition emphasis_mark_position;
  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone &&
      text_box->GetEmphasisMarkPosition(style, emphasis_mark_position)) {
    LayoutUnit emphasis_mark_height = LayoutUnit(
        style.GetFont().EmphasisMarkHeight(style.TextEmphasisMarkString()));
    if (HasEmphasisMarkBefore(text_box)) {
      visual_rect_outsets.SetTop(
          std::max(visual_rect_outsets.Top(), emphasis_mark_height));
    } else {
      visual_rect_outsets.SetBottom(
          std::max(visual_rect_outsets.Bottom(), emphasis_mark_height));
    }
  }

  if (ShadowList* text_shadow = style.TextShadow()) {
    LayoutRectOutsets text_shadow_logical_outsets =
        LineOrientationLayoutRectOutsets(
            LayoutRectOutsets(text_shadow->RectOutsetsIncludingOriginal()),
            style.GetWritingMode());
    text_shadow_logical_outsets.ClampNegativeToZero();
    visual_rect_outsets += text_shadow_logical_outsets;
  }

  LayoutRect frame_rect = text_box->LogicalFrameRect();
  frame_rect.Expand(visual_rect_outsets);
  frame_rect = LayoutRect(EnclosingIntRect(frame_rect));
  logical_visual_overflow.Unite(frame_rect);

  if (logical_visual_overflow != text_box->LogicalFrameRect())
    text_box->SetLogicalOverflowRect(logical_visual_overflow);
}

inline void InlineFlowBox::AddReplacedChildLayoutOverflow(
    const InlineBox* inline_box,
    LayoutRect& logical_layout_overflow) {
  LineLayoutBox box = LineLayoutBox(inline_box->GetLineLayoutItem());

  // Layout overflow internal to the child box only propagates if the child box
  // doesn't have overflow clip set. Otherwise the child border box propagates
  // as layout overflow. This rectangle must include transforms and relative
  // positioning and be adjusted for writing-mode differences.
  LayoutRect child_logical_layout_overflow =
      box.LogicalLayoutOverflowRectForPropagation();
  child_logical_layout_overflow.Move(inline_box->LogicalLeft(),
                                     inline_box->LogicalTop());
  logical_layout_overflow.Unite(child_logical_layout_overflow);
}

void InlineFlowBox::AddReplacedChildrenVisualOverflow(LayoutUnit line_top,
                                                      LayoutUnit line_bottom) {
  LayoutRect logical_visual_overflow =
      LogicalVisualOverflowRect(line_top, line_bottom);
  bool visual_overflow_may_have_changed = false;
  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    const LineLayoutItem& item = curr->GetLineLayoutItem();
    if (item.IsOutOfFlowPositioned() || item.IsText())
      continue;

    if (item.IsLayoutInline()) {
      InlineFlowBox* flow = ToInlineFlowBox(curr);
      flow->AddReplacedChildrenVisualOverflow(line_top, line_bottom);
      // Propagate visual overflow only if it may be present.
      if (!KnownToHaveNoOverflow()) {
        if (!flow->BoxModelObject().HasSelfPaintingLayer()) {
          logical_visual_overflow.Unite(
              flow->LogicalVisualOverflowRect(line_top, line_bottom));
          visual_overflow_may_have_changed = true;
        }
      }

      continue;
    }

    LineLayoutBox box = LineLayoutBox(curr->GetLineLayoutItem());

    // Visual overflow only propagates if the box doesn't have a self-painting
    // layer. This rectangle does not include transforms or relative positioning
    // (since those objects always have self-painting layers), but it does need
    // to be adjusted for writing-mode differences.
    if (!box.HasSelfPaintingLayer()) {
      LayoutRect child_logical_visual_overflow =
          box.LogicalVisualOverflowRectForPropagation();
      child_logical_visual_overflow.Move(curr->LogicalLeft(),
                                         curr->LogicalTop());
      logical_visual_overflow.Unite(child_logical_visual_overflow);
      ClearKnownToHaveNoOverflow();
      visual_overflow_may_have_changed = true;
    }
  }
  if (visual_overflow_may_have_changed) {
    SetVisualOverflowFromLogicalRect(logical_visual_overflow, line_top,
                                     line_bottom);
  }
}

static void ComputeGlyphOverflow(
    InlineTextBox* text,
    const LineLayoutText& layout_text,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map) {
  HashSet<const SimpleFontData*> fallback_fonts;
  FloatRect glyph_bounds;
  GlyphOverflow glyph_overflow;
  float measured_width = layout_text.Width(
      text->Start(), text->Len(), LayoutUnit(), text->Direction(), false,
      &fallback_fonts, &glyph_bounds);
  const Font& font = layout_text.StyleRef().GetFont();
  glyph_overflow.SetFromBounds(glyph_bounds, font, measured_width);
  if (!fallback_fonts.IsEmpty()) {
    GlyphOverflowAndFallbackFontsMap::ValueType* it =
        text_box_data_map
            .insert(text, std::make_pair(Vector<const SimpleFontData*>(),
                                         GlyphOverflow()))
            .stored_value;
    DCHECK(it->value.first.IsEmpty());
    CopyToVector(fallback_fonts, it->value.first);
  }
  if (!glyph_overflow.IsApproximatelyZero()) {
    GlyphOverflowAndFallbackFontsMap::ValueType* it =
        text_box_data_map
            .insert(text, std::make_pair(Vector<const SimpleFontData*>(),
                                         GlyphOverflow()))
            .stored_value;
    it->value.second = glyph_overflow;
  }
}

void InlineFlowBox::ComputeOverflow(
    LayoutUnit line_top,
    LayoutUnit line_bottom,
    GlyphOverflowAndFallbackFontsMap& text_box_data_map) {
  // If we know we have no overflow, we can just bail.
  if (KnownToHaveNoOverflow()) {
    DCHECK(!LayoutOverflowIsSet() && !VisualOverflowIsSet());
    return;
  }

  overflow_.reset();

  // Visual overflow just includes overflow for stuff we need to issues paint
  // invalidations for ourselves. Self-painting layers are ignored.
  // Layout overflow is used to determine scrolling extent, so it still includes
  // child layers and also factors in transforms, relative positioning, etc.
  LayoutRect logical_layout_overflow;
  LayoutRect logical_visual_overflow(
      LogicalFrameRectIncludingLineHeight(line_top, line_bottom));

  AddBoxShadowVisualOverflow(logical_visual_overflow);
  AddBorderOutsetVisualOverflow(logical_visual_overflow);
  AddOutlineVisualOverflow(logical_visual_overflow);

  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;  // Positioned placeholders don't affect calculations.

    if (curr->GetLineLayoutItem().IsText()) {
      InlineTextBox* text = ToInlineTextBox(curr);
      LayoutRect text_box_overflow(text->LogicalFrameRect());
      if (text->IsLineBreak()) {
        text_box_overflow.SetWidth(
            LayoutUnit(text_box_overflow.Width() + text->NewlineSpaceWidth()));
      } else {
        if (text_box_data_map.IsEmpty()) {
          // An empty glyph map means that we're computing overflow without
          // a layout, so calculate the glyph overflow on the fly.
          GlyphOverflowAndFallbackFontsMap glyph_overflow_for_text;
          ComputeGlyphOverflow(text, text->GetLineLayoutItem(),
                               glyph_overflow_for_text);
          AddTextBoxVisualOverflow(text, glyph_overflow_for_text,
                                   text_box_overflow);
        } else {
          AddTextBoxVisualOverflow(text, text_box_data_map, text_box_overflow);
        }
      }
      logical_visual_overflow.Unite(text_box_overflow);
    } else if (curr->GetLineLayoutItem().IsLayoutInline()) {
      InlineFlowBox* flow = ToInlineFlowBox(curr);
      flow->ComputeOverflow(line_top, line_bottom, text_box_data_map);
      if (!flow->BoxModelObject().HasSelfPaintingLayer())
        logical_visual_overflow.Unite(
            flow->LogicalVisualOverflowRect(line_top, line_bottom));
      LayoutRect child_layout_overflow =
          flow->LogicalLayoutOverflowRect(line_top, line_bottom);
      child_layout_overflow.Unite(
          LogicalFrameRectIncludingLineHeight(line_top, line_bottom));
      if (flow->BoxModelObject().IsRelPositioned()) {
        child_layout_overflow.Move(
            flow->BoxModelObject().RelativePositionLogicalOffset());
      }
      logical_layout_overflow.Unite(child_layout_overflow);
    } else {
      if (logical_layout_overflow.IsEmpty()) {
        logical_layout_overflow =
            LogicalFrameRectIncludingLineHeight(line_top, line_bottom);
      }
      AddReplacedChildLayoutOverflow(curr, logical_layout_overflow);
    }
  }

  SetLayoutOverflowFromLogicalRect(logical_layout_overflow, line_top,
                                   line_bottom);
  SetVisualOverflowFromLogicalRect(logical_visual_overflow, line_top,
                                   line_bottom);
}

void InlineFlowBox::SetLayoutOverflow(const LayoutRect& rect,
                                      const LayoutRect& frame_box) {
  DCHECK(!KnownToHaveNoOverflow());
  if (frame_box.Contains(rect) || rect.IsEmpty())
    return;

  if (!LayoutOverflowIsSet()) {
    if (!overflow_)
      overflow_ = std::make_unique<SimpleOverflowModel>();
    overflow_->layout_overflow.emplace(frame_box);
  }

  overflow_->layout_overflow->SetLayoutOverflow(rect);
}

void InlineFlowBox::SetVisualOverflow(const LayoutRect& rect,
                                      const LayoutRect& frame_box) {
  DCHECK(!KnownToHaveNoOverflow());
  if (frame_box.Contains(rect) || rect.IsEmpty())
    return;

  if (!VisualOverflowIsSet()) {
    if (!overflow_)
      overflow_ = std::make_unique<SimpleOverflowModel>();
    overflow_->visual_overflow.emplace(frame_box);
  }

  overflow_->visual_overflow->SetVisualOverflow(rect);
}

void InlineFlowBox::SetVisualOverflowFromLogicalRect(
    const LayoutRect& logical_visual_overflow,
    LayoutUnit line_top,
    LayoutUnit line_bottom) {
  DCHECK(!KnownToHaveNoOverflow());
  LayoutRect frame_box = FrameRectIncludingLineHeight(line_top, line_bottom);
  LayoutRect visual_overflow(IsHorizontal()
                                 ? logical_visual_overflow
                                 : logical_visual_overflow.TransposedRect());
  SetVisualOverflow(visual_overflow, frame_box);
}

void InlineFlowBox::SetLayoutOverflowFromLogicalRect(
    const LayoutRect& logical_layout_overflow,
    LayoutUnit line_top,
    LayoutUnit line_bottom) {
  DCHECK(!KnownToHaveNoOverflow());
  LayoutRect frame_box = FrameRectIncludingLineHeight(line_top, line_bottom);

  LayoutRect layout_overflow(IsHorizontal()
                                 ? logical_layout_overflow
                                 : logical_layout_overflow.TransposedRect());
  SetLayoutOverflow(layout_overflow, frame_box);
}

bool InlineFlowBox::NodeAtPoint(HitTestResult& result,
                                const HitTestLocation& hit_test_location,
                                const PhysicalOffset& accumulated_offset,
                                LayoutUnit line_top,
                                LayoutUnit line_bottom) {
  PhysicalRect overflow_rect =
      PhysicalVisualOverflowRect(line_top, line_bottom);
  overflow_rect.Move(accumulated_offset);
  if (!hit_test_location.Intersects(overflow_rect))
    return false;

  // We need to hit test both our inline children (Inline Boxes) and culled
  // inlines (LayoutObjects). We check our inlines in the same order as line
  // layout but for each inline we additionally need to hit test its culled
  // inline parents. While hit testing culled inline parents, we can stop once
  // we reach a non-inline parent or a culled inline associated with a different
  // inline box.
  InlineBox* prev;
  for (InlineBox* curr = LastChild(); curr; curr = prev) {
    prev = curr->PrevOnLine();

    // Layers will handle hit testing themselves.
    if (!curr->BoxModelObject() ||
        !curr->BoxModelObject().HasSelfPaintingLayer()) {
      if (curr->NodeAtPoint(result, hit_test_location, accumulated_offset,
                            line_top, line_bottom)) {
        GetLineLayoutItem().UpdateHitTestResult(
            result, hit_test_location.Point() - accumulated_offset);
        return true;
      }
    }

    // If the current inline box's layout object and the previous inline box's
    // layout object are same, we should yield the hit-test to the previous
    // inline box.
    if (prev && curr->GetLineLayoutItem() == prev->GetLineLayoutItem())
      continue;

    // Hit test the culled inline if necessary.
    LineLayoutItem curr_layout_item = curr->GetLineLayoutItem();
    while (true) {
      // If the previous inline box is not a descendant of a current inline's
      // parent, the parent is a culled inline and we hit test it.
      // Otherwise, move to the previous inline box because we hit test first
      // all candidate inline boxes under the parent to take a pre-order tree
      // traversal in reverse.
      bool has_sibling =
          curr_layout_item.PreviousSibling() || curr_layout_item.NextSibling();
      LineLayoutItem culled_parent = curr_layout_item.Parent();
      DCHECK(culled_parent);

      if (culled_parent == GetLineLayoutItem() ||
          (has_sibling && prev &&
           prev->GetLineLayoutItem().IsDescendantOf(culled_parent)))
        break;

      if (culled_parent.IsLayoutInline() &&
          LineLayoutInline(culled_parent)
              .HitTestCulledInline(result, hit_test_location,
                                   accumulated_offset))
        return true;

      curr_layout_item = culled_parent;
    }
  }

  if (GetLineLayoutItem().IsBox() &&
      ToLayoutBox(LineLayoutAPIShim::LayoutObjectFrom(GetLineLayoutItem()))
          ->HitTestClippedOutByBorder(hit_test_location, overflow_rect.offset))
    return false;

  if (GetLineLayoutItem().StyleRef().HasBorderRadius()) {
    // TODO(layout-dev): LogicalFrameRect() seems incorrect.
    LayoutRect border_rect = LogicalFrameRect();
    border_rect.MoveBy(accumulated_offset.ToLayoutPoint());
    FloatRoundedRect border =
        GetLineLayoutItem().StyleRef().GetRoundedBorderFor(
            border_rect, IncludeLogicalLeftEdge(), IncludeLogicalRightEdge());
    if (!hit_test_location.Intersects(border))
      return false;
  }

  // Now check ourselves.
  LayoutRect layout_rect =
      InlineFlowBoxPainter(*this).FrameRectClampedToLineTopAndBottomIfNeeded();
  FlipForWritingMode(layout_rect);
  PhysicalRect rect(layout_rect);
  rect.Move(accumulated_offset);

  // Pixel snap hit testing.
  rect = PhysicalRect(PixelSnappedIntRect(rect));
  if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
      hit_test_location.Intersects(rect)) {
    // Don't add in m_topLeft here, we want coords in the containing block's
    // coordinate space.
    GetLineLayoutItem().UpdateHitTestResult(
        result, hit_test_location.Point() - accumulated_offset);
    if (result.AddNodeToListBasedTestResult(GetLineLayoutItem().GetNode(),
                                            hit_test_location,
                                            rect) == kStopHitTesting)
      return true;
  }

  return false;
}

void InlineFlowBox::Paint(const PaintInfo& paint_info,
                          const LayoutPoint& paint_offset,
                          LayoutUnit line_top,
                          LayoutUnit line_bottom) const {
  InlineFlowBoxPainter(*this).Paint(paint_info, paint_offset, line_top,
                                    line_bottom);
}

bool InlineFlowBox::BoxShadowCanBeAppliedToBackground(
    const FillLayer& last_background_layer) const {
  // The checks here match how paintFillLayer() decides whether to clip (if it
  // does, the shadow
  // would be clipped out, so it has to be drawn separately).
  StyleImage* image = last_background_layer.GetImage();
  bool has_fill_image = image && image->CanRender();
  return (!has_fill_image &&
          !GetLineLayoutItem().StyleRef().HasBorderRadius()) ||
         (!PrevForSameLayoutObject() && !NextForSameLayoutObject()) ||
         !Parent();
}

InlineBox* InlineFlowBox::FirstLeafChild() const {
  InlineBox* leaf = nullptr;
  for (InlineBox* child = FirstChild(); child && !leaf;
       child = child->NextOnLine())
    leaf = child->IsLeaf() ? child : ToInlineFlowBox(child)->FirstLeafChild();
  return leaf;
}

InlineBox* InlineFlowBox::LastLeafChild() const {
  InlineBox* leaf = nullptr;
  for (InlineBox* child = LastChild(); child && !leaf;
       child = child->PrevOnLine())
    leaf = child->IsLeaf() ? child : ToInlineFlowBox(child)->LastLeafChild();
  return leaf;
}

bool InlineFlowBox::CanAccommodateEllipsis(bool ltr,
                                           LayoutUnit block_edge,
                                           LayoutUnit ellipsis_width) const {
  for (InlineBox* box = FirstChild(); box; box = box->NextOnLine()) {
    if (!box->CanAccommodateEllipsis(ltr, block_edge, ellipsis_width))
      return false;
  }
  return true;
}

LayoutUnit InlineFlowBox::PlaceEllipsisBox(bool ltr,
                                           LayoutUnit block_left_edge,
                                           LayoutUnit block_right_edge,
                                           LayoutUnit ellipsis_width,
                                           LayoutUnit& truncated_width,
                                           InlineBox** found_box,
                                           LayoutUnit logical_left_offset) {
  LayoutUnit result(-1);
  // We iterate over all children, the foundBox variable tells us when we've
  // found the box containing the ellipsis.  All boxes after that one in the
  // flow are hidden.
  // If our flow is ltr then iterate over the boxes from left to right,
  // otherwise iterate from right to left. Varying the order allows us to
  // correctly hide the boxes following the ellipsis.
  InlineBox* box = ltr ? FirstChild() : LastChild();

  // NOTE: these will cross after foundBox = true.
  LayoutUnit visible_left_edge = block_left_edge;
  LayoutUnit visible_right_edge = block_right_edge;

  while (box) {
    bool had_found_box = *found_box;
    LayoutUnit curr_result = box->PlaceEllipsisBox(
        ltr, visible_left_edge, visible_right_edge, ellipsis_width,
        truncated_width, found_box, logical_left_offset);
    if (IsRootInlineBox() && *found_box && !had_found_box)
      *found_box = box;
    if (curr_result != -1 && result == -1)
      result = curr_result;

    // List markers will sit outside the box so don't let them contribute
    // width.
    LayoutUnit box_width = box->GetLineLayoutItem().IsListMarker()
                               ? LayoutUnit()
                               : box->LogicalWidth();
    if (ltr) {
      visible_left_edge += box_width;
      box = box->NextOnLine();
    } else {
      visible_right_edge -= box_width;
      box = box->PrevOnLine();
    }
  }
  return result;
}

void InlineFlowBox::ClearTruncation() {
  for (InlineBox* box = FirstChild(); box; box = box->NextOnLine())
    box->ClearTruncation();
}

LayoutUnit InlineFlowBox::ComputeOverAnnotationAdjustment(
    LayoutUnit allowed_position) const {
  LayoutUnit result;
  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;  // Positioned placeholders don't affect calculations.

    if (curr->IsInlineFlowBox())
      result = std::max(result,
                        ToInlineFlowBox(curr)->ComputeOverAnnotationAdjustment(
                            allowed_position));

    if (curr->GetLineLayoutItem().IsAtomicInlineLevel() &&
        curr->GetLineLayoutItem().IsRubyRun() &&
        curr->GetLineLayoutItem().StyleRef().GetRubyPosition() ==
            RubyPosition::kBefore) {
      LineLayoutRubyRun ruby_run = LineLayoutRubyRun(curr->GetLineLayoutItem());
      LineLayoutRubyText ruby_text = ruby_run.RubyText();
      if (!ruby_text)
        continue;

      if (!ruby_run.StyleRef().IsFlippedLinesWritingMode()) {
        LayoutUnit top_of_first_ruby_text_line =
            ruby_text.LogicalTop() + (ruby_text.FirstRootBox()
                                          ? ruby_text.FirstRootBox()->LineTop()
                                          : LayoutUnit());
        if (top_of_first_ruby_text_line >= 0)
          continue;
        top_of_first_ruby_text_line += curr->LogicalTop();
        result =
            std::max(result, allowed_position - top_of_first_ruby_text_line);
      } else {
        LayoutUnit bottom_of_last_ruby_text_line =
            ruby_text.LogicalTop() +
            (ruby_text.LastRootBox() ? ruby_text.LastRootBox()->LineBottom()
                                     : ruby_text.LogicalHeight());
        if (bottom_of_last_ruby_text_line <= curr->LogicalHeight())
          continue;
        bottom_of_last_ruby_text_line += curr->LogicalTop();
        result =
            std::max(result, bottom_of_last_ruby_text_line - allowed_position);
      }
    }

    if (curr->IsInlineTextBox()) {
      const ComputedStyle& style =
          curr->GetLineLayoutItem().StyleRef(IsFirstLineStyle());
      TextEmphasisPosition emphasis_mark_position;
      if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone &&
          ToInlineTextBox(curr)->GetEmphasisMarkPosition(
              style, emphasis_mark_position) &&
          HasEmphasisMarkOver(ToInlineTextBox(curr))) {
        if (!style.IsFlippedLinesWritingMode()) {
          int top_of_emphasis_mark =
              (curr->LogicalTop() - style.GetFont().EmphasisMarkHeight(
                                        style.TextEmphasisMarkString()))
                  .ToInt();
          result = std::max(result, allowed_position - top_of_emphasis_mark);
        } else {
          int bottom_of_emphasis_mark =
              (curr->LogicalBottom() + style.GetFont().EmphasisMarkHeight(
                                           style.TextEmphasisMarkString()))
                  .ToInt();
          result = std::max(result, bottom_of_emphasis_mark - allowed_position);
        }
      }
    }
  }
  return result;
}

LayoutUnit InlineFlowBox::ComputeUnderAnnotationAdjustment(
    LayoutUnit allowed_position) const {
  LayoutUnit result;
  for (InlineBox* curr = FirstChild(); curr; curr = curr->NextOnLine()) {
    if (curr->GetLineLayoutItem().IsOutOfFlowPositioned())
      continue;  // Positioned placeholders don't affect calculations.

    if (curr->IsInlineFlowBox())
      result = std::max(result,
                        ToInlineFlowBox(curr)->ComputeUnderAnnotationAdjustment(
                            allowed_position));

    if (curr->GetLineLayoutItem().IsAtomicInlineLevel() &&
        curr->GetLineLayoutItem().IsRubyRun() &&
        curr->GetLineLayoutItem().StyleRef().GetRubyPosition() ==
            RubyPosition::kAfter) {
      LineLayoutRubyRun ruby_run = LineLayoutRubyRun(curr->GetLineLayoutItem());
      LineLayoutRubyText ruby_text = ruby_run.RubyText();
      if (!ruby_text)
        continue;

      if (ruby_run.StyleRef().IsFlippedLinesWritingMode()) {
        LayoutUnit top_of_first_ruby_text_line =
            ruby_text.LogicalTop() + (ruby_text.FirstRootBox()
                                          ? ruby_text.FirstRootBox()->LineTop()
                                          : LayoutUnit());
        if (top_of_first_ruby_text_line >= 0)
          continue;
        top_of_first_ruby_text_line += curr->LogicalTop();
        result =
            std::max(result, allowed_position - top_of_first_ruby_text_line);
      } else {
        LayoutUnit bottom_of_last_ruby_text_line =
            ruby_text.LogicalTop() +
            (ruby_text.LastRootBox() ? ruby_text.LastRootBox()->LineBottom()
                                     : ruby_text.LogicalHeight());
        if (bottom_of_last_ruby_text_line <= curr->LogicalHeight())
          continue;
        bottom_of_last_ruby_text_line += curr->LogicalTop();
        result =
            std::max(result, bottom_of_last_ruby_text_line - allowed_position);
      }
    }

    if (curr->IsInlineTextBox()) {
      const ComputedStyle& style =
          curr->GetLineLayoutItem().StyleRef(IsFirstLineStyle());
      if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone &&
          HasEmphasisMarkUnder(ToInlineTextBox(curr))) {
        if (!style.IsFlippedLinesWritingMode()) {
          LayoutUnit bottom_of_emphasis_mark =
              curr->LogicalBottom() + style.GetFont().EmphasisMarkHeight(
                                          style.TextEmphasisMarkString());
          result = std::max(result, bottom_of_emphasis_mark - allowed_position);
        } else {
          LayoutUnit top_of_emphasis_mark =
              curr->LogicalTop() - style.GetFont().EmphasisMarkHeight(
                                       style.TextEmphasisMarkString());
          result = std::max(result, allowed_position - top_of_emphasis_mark);
        }
      }
    }
  }
  return result;
}

const char* InlineFlowBox::BoxName() const {
  return "InlineFlowBox";
}

#if DCHECK_IS_ON()

void InlineFlowBox::DumpLineTreeAndMark(StringBuilder& string_builder,
                                        const InlineBox* marked_box1,
                                        const char* marked_label1,
                                        const InlineBox* marked_box2,
                                        const char* marked_label2,
                                        const LayoutObject* obj,
                                        int depth) const {
  InlineBox::DumpLineTreeAndMark(string_builder, marked_box1, marked_label1,
                                 marked_box2, marked_label2, obj, depth);
  for (const InlineBox* box = FirstChild(); box; box = box->NextOnLine()) {
    box->DumpLineTreeAndMark(string_builder, marked_box1, marked_label1,
                             marked_box2, marked_label2, obj, depth + 1);
  }
}

#endif

}  // namespace blink
