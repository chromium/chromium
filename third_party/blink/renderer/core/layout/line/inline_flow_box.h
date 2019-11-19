/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_FLOW_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_FLOW_BOX_H_

#include <memory>
#include "third_party/blink/renderer/core/layout/line/inline_box.h"
#include "third_party/blink/renderer/core/layout/overflow_model.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"

namespace blink {

class HitTestResult;
class InlineTextBox;
class LineBoxList;
class SimpleFontData;
class VerticalPositionCache;

struct GlyphOverflow;

typedef HashMap<const InlineTextBox*,
                std::pair<Vector<const SimpleFontData*>, GlyphOverflow>>
    GlyphOverflowAndFallbackFontsMap;

class InlineFlowBox : public InlineBox {
 public:
  InlineFlowBox(LineLayoutItem line_layout_item)
      : InlineBox(line_layout_item),
        first_child_(nullptr),
        last_child_(nullptr),
        prev_line_box_(nullptr),
        next_line_box_(nullptr),
        include_logical_left_edge_(false),
        include_logical_right_edge_(false),
        descendants_have_same_line_height_and_baseline_(true),
        baseline_type_(kAlphabeticBaseline),
        has_annotations_before_(false),
        has_annotations_after_(false),
        line_break_bidi_status_eor_(WTF::unicode::kLeftToRight),
        line_break_bidi_status_last_strong_(WTF::unicode::kLeftToRight),
        line_break_bidi_status_last_(WTF::unicode::kLeftToRight),
        is_first_after_page_break_(false)
#if DCHECK_IS_ON()
        ,
        has_bad_child_list_(false)
#endif
  {
    // Internet Explorer and Firefox always create a marker for list items, even
    // when the list-style-type is none.  We do not make a marker in the
    // list-style-type: none case, since it is wasteful to do so.
    // However, in order to match other browsers we have to pretend like an
    // invisible marker exists.  The side effect of having an invisible marker
    // is that the quirks mode behavior of shrinking lines with no text children
    // must not apply. This change also means that gaps will exist between image
    // bullet list items.  Even when the list bullet is an image, the line is
    // still considered to be immune from the quirk.
    has_text_children_ =
        line_layout_item.StyleRef().Display() == EDisplay::kListItem;
    has_text_descendants_ = has_text_children_;
  }

#if DCHECK_IS_ON()
  ~InlineFlowBox() override;

  void DumpLineTreeAndMark(StringBuilder&,
                           const InlineBox* = nullptr,
                           const char* = nullptr,
                           const InlineBox* = nullptr,
                           const char* = nullptr,
                           const LayoutObject* = nullptr,
                           int = 0) const override;
#endif
  const char* BoxName() const override;

  InlineFlowBox* PrevForSameLayoutObject() const { return prev_line_box_; }
  InlineFlowBox* NextForSameLayoutObject() const { return next_line_box_; }
  void SetNextForSameLayoutObject(InlineFlowBox* n) { next_line_box_ = n; }
  void SetPreviousForSameLayoutObject(InlineFlowBox* p) { prev_line_box_ = p; }

  InlineBox* FirstChild() const { return first_child_; }
  InlineBox* LastChild() const { return last_child_; }

  bool IsLeaf() const final { return false; }

  InlineBox* FirstLeafChild() const;
  InlineBox* LastLeafChild() const;

  DISABLE_CFI_PERF
  void SetConstructed() final {
    InlineBox::SetConstructed();
    for (InlineBox* child = FirstChild(); child; child = child->NextOnLine())
      child->SetConstructed();
  }

  void AddToLine(InlineBox* child);
  void DeleteLine() final;
  void ExtractLine() final;
  void AttachLine() final;
  void Move(const LayoutSize&) override;

  virtual void ExtractLineBoxFromLayoutObject();
  virtual void AttachLineBoxToLayoutObject();
  virtual void RemoveLineBoxFromLayoutObject();

  void ClearTruncation() override;

  LayoutRect FrameRect() const;

  void Paint(const PaintInfo&,
             const LayoutPoint&,
             LayoutUnit line_top,
             LayoutUnit line_bottom) const override;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   LayoutUnit line_top,
                   LayoutUnit line_bottom) override;

  bool BoxShadowCanBeAppliedToBackground(const FillLayer&) const;

  virtual LineBoxList* LineBoxes() const;

  // logicalLeft = left in a horizontal line and top in a vertical line.
  LayoutUnit MarginBorderPaddingLogicalLeft() const {
    return MarginLogicalLeft() + BorderLogicalLeft() + PaddingLogicalLeft();
  }
  LayoutUnit MarginBorderPaddingLogicalRight() const {
    return MarginLogicalRight() + BorderLogicalRight() + PaddingLogicalRight();
  }
  LayoutUnit MarginLogicalLeft() const {
    if (!IncludeLogicalLeftEdge())
      return LayoutUnit();
    return IsHorizontal() ? BoxModelObject().MarginLeft()
                          : BoxModelObject().MarginTop();
  }
  LayoutUnit MarginLogicalRight() const {
    if (!IncludeLogicalRightEdge())
      return LayoutUnit();
    return IsHorizontal() ? BoxModelObject().MarginRight()
                          : BoxModelObject().MarginBottom();
  }
  LayoutUnit MarginLogicalWidth() const {
    return MarginLogicalLeft() + MarginLogicalRight();
  }
  LayoutUnit BorderLogicalLeft() const {
    if (!IncludeLogicalLeftEdge())
      return LayoutUnit();
    return LayoutUnit(
        IsHorizontal()
            ? GetLineLayoutItem().Style(IsFirstLineStyle())->BorderLeftWidth()
            : GetLineLayoutItem().Style(IsFirstLineStyle())->BorderTopWidth());
  }
  LayoutUnit BorderLogicalRight() const {
    if (!IncludeLogicalRightEdge())
      return LayoutUnit();
    return LayoutUnit(
        IsHorizontal()
            ? GetLineLayoutItem().Style(IsFirstLineStyle())->BorderRightWidth()
            : GetLineLayoutItem()
                  .Style(IsFirstLineStyle())
                  ->BorderBottomWidth());
  }
  int PaddingLogicalLeft() const {
    if (!IncludeLogicalLeftEdge())
      return 0;
    return (IsHorizontal() ? BoxModelObject().PaddingLeft()
                           : BoxModelObject().PaddingTop())
        .ToInt();
  }
  int PaddingLogicalRight() const {
    if (!IncludeLogicalRightEdge())
      return 0;
    return (IsHorizontal() ? BoxModelObject().PaddingRight()
                           : BoxModelObject().PaddingBottom())
        .ToInt();
  }

  bool IncludeLogicalLeftEdge() const { return include_logical_left_edge_; }
  bool IncludeLogicalRightEdge() const { return include_logical_right_edge_; }
  void SetEdges(bool include_left, bool include_right) {
    include_logical_left_edge_ = include_left;
    include_logical_right_edge_ = include_right;
  }

  // Helper functions used during line construction and placement.
  void DetermineSpacingForFlowBoxes(
      bool last_line,
      bool is_logically_last_run_wrapped,
      LineLayoutItem logically_last_run_layout_object);
  LayoutUnit GetFlowSpacingLogicalWidth();
  LayoutUnit PlaceBoxesInInlineDirection(LayoutUnit logical_left,
                                         bool& needs_word_spacing);

  void ComputeLogicalBoxHeights(RootInlineBox*,
                                LayoutUnit& max_position_top,
                                LayoutUnit& max_position_bottom,
                                LayoutUnit& max_ascent,
                                LayoutUnit& max_descent,
                                bool& set_max_ascent,
                                bool& set_max_descent,
                                bool no_quirks_mode,
                                GlyphOverflowAndFallbackFontsMap&,
                                FontBaseline,
                                VerticalPositionCache&);
  void AdjustMaxAscentAndDescent(LayoutUnit& max_ascent,
                                 LayoutUnit& max_descent,
                                 int max_position_top,
                                 int max_position_bottom);
  void PlaceBoxesInBlockDirection(LayoutUnit logical_top,
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
                                  FontBaseline);
  void FlipLinesInBlockDirection(LayoutUnit line_top, LayoutUnit line_bottom);
  FontBaseline DominantBaseline() const;

  LayoutUnit ComputeOverAnnotationAdjustment(LayoutUnit allowed_position) const;
  LayoutUnit ComputeUnderAnnotationAdjustment(
      LayoutUnit allowed_position) const;

  // Computes all layout overflow, plus visual overflow not due to replaced
  // children. Visual overflow due to replaced children is computed during
  // the RecalcVisualOverflow tree walk. Other visual overflow is computed
  // during layout for performance reasons.
  void ComputeOverflow(LayoutUnit line_top,
                       LayoutUnit line_bottom,
                       GlyphOverflowAndFallbackFontsMap&);
  // Adds visual flow to the current visual overflow for replaced children.
  void AddReplacedChildrenVisualOverflow(LayoutUnit line_top,
                                         LayoutUnit line_bottom);

  void RemoveChild(InlineBox* child, MarkLineBoxes);

  bool IsSelected() const override { return false; }

  bool CanAccommodateEllipsis(bool ltr,
                              LayoutUnit block_edge,
                              LayoutUnit ellipsis_width) const final;
  LayoutUnit PlaceEllipsisBox(bool ltr,
                              LayoutUnit block_left_edge,
                              LayoutUnit block_right_edge,
                              LayoutUnit ellipsis_width,
                              LayoutUnit& truncated_width,
                              InlineBox**,
                              LayoutUnit logical_left_offset) override;

  bool HasTextChildren() const { return has_text_children_; }
  bool HasTextDescendants() const { return has_text_descendants_; }
  void SetHasTextDescendants() { has_text_descendants_ = true; }

  void SetHasBadChildList();

  // Line visual and layout overflow are in the coordinate space of the block.
  // This means that they aren't purely physical directions. For horizontal-tb
  // and vertical-lr they will match physical directions, but for vertical-rl,
  // the left/right respectively are flipped when compared to their physical
  // counterparts.  For example minX is on the left in vertical-lr, but it is on
  // the right in vertical-rl.
  LayoutRect LayoutOverflowRect(LayoutUnit line_top,
                                LayoutUnit line_bottom) const {
    return LayoutOverflowIsSet()
               ? overflow_->layout_overflow->LayoutOverflowRect()
               : FrameRectIncludingLineHeight(line_top, line_bottom);
  }
  LayoutUnit LogicalTopLayoutOverflow(LayoutUnit line_top) const {
    if (LayoutOverflowIsSet()) {
      return IsHorizontal()
                 ? overflow_->layout_overflow->LayoutOverflowRect().Y()
                 : overflow_->layout_overflow->LayoutOverflowRect().X();
    }
    return line_top;
  }
  LayoutUnit LogicalBottomLayoutOverflow(LayoutUnit line_bottom) const {
    if (LayoutOverflowIsSet()) {
      return IsHorizontal()
                 ? overflow_->layout_overflow->LayoutOverflowRect().MaxY()
                 : overflow_->layout_overflow->LayoutOverflowRect().MaxX();
    }
    return line_bottom;
  }
  LayoutRect LogicalLayoutOverflowRect(LayoutUnit line_top,
                                       LayoutUnit line_bottom) const {
    LayoutRect result = LayoutOverflowRect(line_top, line_bottom);
    if (!GetLineLayoutItem().IsHorizontalWritingMode())
      result = result.TransposedRect();
    return result;
  }

  LayoutRect VisualOverflowRect(LayoutUnit line_top,
                                LayoutUnit line_bottom) const {
    return VisualOverflowIsSet()
               ? overflow_->visual_overflow->VisualOverflowRect()
               : FrameRectIncludingLineHeight(line_top, line_bottom);
  }
  PhysicalRect PhysicalVisualOverflowRect(LayoutUnit line_top,
                                          LayoutUnit line_bottom) const {
    LayoutRect rect = VisualOverflowRect(line_top, line_bottom);
    FlipForWritingMode(rect);
    return PhysicalRect(rect);
  }
  LayoutUnit LogicalLeftVisualOverflow() const {
    return VisualOverflowIsSet()
               ? (IsHorizontal()
                      ? overflow_->visual_overflow->VisualOverflowRect().X()
                      : overflow_->visual_overflow->VisualOverflowRect().Y())
               : LogicalLeft();
  }
  LayoutUnit LogicalRightVisualOverflow() const {
    return VisualOverflowIsSet()
               ? (IsHorizontal()
                      ? overflow_->visual_overflow->VisualOverflowRect().MaxX()
                      : overflow_->visual_overflow->VisualOverflowRect().MaxY())
               : static_cast<LayoutUnit>(LogicalRight().Ceil());
  }
  LayoutUnit LogicalTopVisualOverflow(LayoutUnit line_top) const {
    if (VisualOverflowIsSet()) {
      return IsHorizontal()
                 ? overflow_->visual_overflow->VisualOverflowRect().Y()
                 : overflow_->visual_overflow->VisualOverflowRect().X();
    }
    return line_top;
  }
  LayoutUnit LogicalBottomVisualOverflow(LayoutUnit line_bottom) const {
    if (VisualOverflowIsSet()) {
      return IsHorizontal()
                 ? overflow_->visual_overflow->VisualOverflowRect().MaxY()
                 : overflow_->visual_overflow->VisualOverflowRect().MaxX();
    }
    return line_bottom;
  }
  LayoutRect LogicalVisualOverflowRect(LayoutUnit line_top,
                                       LayoutUnit line_bottom) const {
    LayoutRect result = VisualOverflowRect(line_top, line_bottom);
    if (!GetLineLayoutItem().IsHorizontalWritingMode())
      result = result.TransposedRect();
    return result;
  }

  LayoutRect FrameRectIncludingLineHeight(LayoutUnit line_top,
                                          LayoutUnit line_bottom) const {
    if (IsHorizontal())
      return LayoutRect(X(), line_top, LogicalWidth(), line_bottom - line_top);
    return LayoutRect(line_top, Y(), line_bottom - line_top, LogicalWidth());
  }

  LayoutRect LogicalFrameRectIncludingLineHeight(LayoutUnit line_top,
                                                 LayoutUnit line_bottom) const {
    return LayoutRect(LogicalLeft(), line_top, LogicalWidth(),
                      line_bottom - line_top);
  }

  bool DescendantsHaveSameLineHeightAndBaseline() const {
    return descendants_have_same_line_height_and_baseline_;
  }
  void ClearDescendantsHaveSameLineHeightAndBaseline() {
    descendants_have_same_line_height_and_baseline_ = false;
    if (Parent() && Parent()->DescendantsHaveSameLineHeightAndBaseline())
      Parent()->ClearDescendantsHaveSameLineHeightAndBaseline();
  }

  bool IsFirstAfterPageBreak() const { return is_first_after_page_break_; }
  void SetIsFirstAfterPageBreak(bool is_first_after_page_break) {
    is_first_after_page_break_ = is_first_after_page_break;
  }

  bool OverrideVisualOverflowFromLogicalRect(
      const LayoutRect& logical_visual_overflow,
      LayoutUnit line_top,
      LayoutUnit line_bottom);

  void OverrideLayoutOverflowFromLogicalRect(
      const LayoutRect& logical_layout_overflow,
      LayoutUnit line_top,
      LayoutUnit line_bottom);

  LayoutUnit FarthestPositionForUnderline(LineLayoutItem decorating_box,
                                          FontVerticalPositionType,
                                          FontBaseline,
                                          LayoutUnit current) const;

 private:
  inline bool LayoutOverflowIsSet() const {
    return overflow_ && overflow_->layout_overflow;
  }
  inline bool VisualOverflowIsSet() const {
    return overflow_ && overflow_->visual_overflow;
  }
  void PlaceBoxRangeInInlineDirection(InlineBox* first_child,
                                      InlineBox* last_child,
                                      LayoutUnit& logical_left,
                                      LayoutUnit& min_logical_left,
                                      LayoutUnit& max_logical_right,
                                      bool& needs_word_spacing);
  void BeginPlacingBoxRangesInInlineDirection(LayoutUnit logical_left) {
    SetLogicalLeft(logical_left);
  }
  void EndPlacingBoxRangesInInlineDirection(LayoutUnit logical_left,
                                            LayoutUnit logical_right,
                                            LayoutUnit min_logical_left,
                                            LayoutUnit max_logical_right) {
    SetLogicalWidth(logical_right - logical_left);
    if (KnownToHaveNoOverflow() &&
        (min_logical_left < logical_left || max_logical_right > logical_right))
      ClearKnownToHaveNoOverflow();
  }

  void AddBoxShadowVisualOverflow(LayoutRect& logical_visual_overflow);
  void AddBorderOutsetVisualOverflow(LayoutRect& logical_visual_overflow);
  void AddOutlineVisualOverflow(LayoutRect& logical_visual_overflow);
  void AddTextBoxVisualOverflow(InlineTextBox*,
                                GlyphOverflowAndFallbackFontsMap&,
                                LayoutRect& logical_visual_overflow);
  void AddReplacedChildLayoutOverflow(const InlineBox*,
                                      LayoutRect& logical_layout_overflow);
  bool HasEmphasisMarkBefore(const InlineTextBox*) const;
  bool HasEmphasisMarkOver(const InlineTextBox*) const;
  bool HasEmphasisMarkUnder(const InlineTextBox*) const;

  void SetLayoutOverflow(const LayoutRect&, const LayoutRect&);
  void SetVisualOverflow(const LayoutRect&, const LayoutRect&);

  void SetLayoutOverflowFromLogicalRect(
      const LayoutRect& logical_layout_overflow,
      LayoutUnit line_top,
      LayoutUnit line_bottom);
  void SetVisualOverflowFromLogicalRect(
      const LayoutRect& logical_visual_overflow,
      LayoutUnit line_top,
      LayoutUnit line_bottom);

 protected:
  std::unique_ptr<SimpleOverflowModel> overflow_;

  bool IsInlineFlowBox() const final { return true; }

  InlineBox* first_child_;
  InlineBox* last_child_;

  // The next/previous box that also uses our LayoutObject.
  // RootInlineBox, a subclass of this class, uses these fields for
  // next/previous RootInlineBox.
  InlineFlowBox* prev_line_box_;
  InlineFlowBox* next_line_box_;

 private:
  unsigned include_logical_left_edge_ : 1;
  unsigned include_logical_right_edge_ : 1;
  unsigned has_text_children_ : 1;
  unsigned has_text_descendants_ : 1;
  unsigned descendants_have_same_line_height_and_baseline_ : 1;

 protected:
  // The following members are only used by RootInlineBox but moved here to keep
  // the bits packed.

  // Whether or not this line uses alphabetic or ideographic baselines by
  // default.
  unsigned baseline_type_ : 1;  // FontBaseline

  // If the line contains any ruby runs, then this will be true.
  unsigned has_annotations_before_ : 1;
  unsigned has_annotations_after_ : 1;

  unsigned line_break_bidi_status_eor_ : 5;          // WTF::unicode::Direction
  unsigned line_break_bidi_status_last_strong_ : 5;  // WTF::unicode::Direction
  unsigned line_break_bidi_status_last_ : 5;         // WTF::unicode::Direction

  unsigned is_first_after_page_break_ : 1;

// End of RootInlineBox-specific members.

#if DCHECK_IS_ON()
 private:
  unsigned has_bad_child_list_ : 1;
#endif
};

DEFINE_INLINE_BOX_TYPE_CASTS(InlineFlowBox);

inline void InlineFlowBox::SetHasBadChildList() {
#if DCHECK_IS_ON()
  has_bad_child_list_ = true;
#endif
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_FLOW_BOX_H_
