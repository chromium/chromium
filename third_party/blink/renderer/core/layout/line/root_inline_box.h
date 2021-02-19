/*
 * Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ROOT_INLINE_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ROOT_INLINE_BOX_H_

#include <memory>

#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/line/inline_flow_box.h"
#include "third_party/blink/renderer/platform/text/bidi_context.h"

namespace blink {

class EllipsisBox;
class HitTestResult;
class LineLayoutBlockFlow;

struct BidiStatus;

enum ForceEllipsisOnLine { DoNotForceEllipsis, ForceEllipsis };

class RootInlineBox : public InlineFlowBox {
 public:
  explicit RootInlineBox(LineLayoutItem);

  void Destroy() final;

  bool IsRootInlineBox() const final { return true; }

  void DetachEllipsisBox();

  RootInlineBox* NextRootBox() const {
    return static_cast<RootInlineBox*>(next_line_box_);
  }
  RootInlineBox* PrevRootBox() const {
    return static_cast<RootInlineBox*>(prev_line_box_);
  }

  void Move(const LayoutSize&) final;

  LayoutUnit LineTop() const { return line_top_; }
  LayoutUnit LineBottom() const { return line_bottom_; }

  LayoutUnit LineTopWithLeading() const { return line_top_with_leading_; }
  LayoutUnit LineBottomWithLeading() const { return line_bottom_with_leading_; }

  LayoutUnit PaginationStrut() const { return pagination_strut_; }
  void SetPaginationStrut(LayoutUnit strut) { pagination_strut_ = strut; }

  LayoutUnit SelectionTop() const;
  LayoutUnit SelectionBottom() const;
  LayoutUnit SelectionHeight() const {
    return (SelectionBottom() - SelectionTop()).ClampNegativeToZero();
  }

  LayoutUnit BlockDirectionPointInLine() const;

  LayoutUnit AlignBoxesInBlockDirection(LayoutUnit height_of_block,
                                        GlyphOverflowAndFallbackFontsMap&,
                                        VerticalPositionCache&);
  void SetLineTopBottomPositions(
      LayoutUnit top,
      LayoutUnit bottom,
      LayoutUnit top_with_leading,
      LayoutUnit bottom_with_leading,
      LayoutUnit selection_bottom = LayoutUnit::Min()) {
    line_top_ = top;
    line_bottom_ = bottom;
    line_top_with_leading_ = top_with_leading;
    line_bottom_with_leading_ = bottom_with_leading;
    selection_bottom_ =
        selection_bottom == LayoutUnit::Min() ? bottom : selection_bottom;
  }

  LineBoxList* LineBoxes() const final;

  LineLayoutItem LineBreakObj() const { return line_break_obj_; }
  BidiStatus LineBreakBidiStatus() const;
  void SetLineBreakInfo(LineLayoutItem, unsigned break_pos, const BidiStatus&);

  unsigned LineBreakPos() const { return line_break_pos_; }
  void SetLineBreakPos(unsigned p) { line_break_pos_ = p; }

  using InlineBox::EndsWithBreak;
  using InlineBox::SetEndsWithBreak;

  void ChildRemoved(InlineBox*);

  bool LineCanAccommodateEllipsis(bool ltr,
                                  LayoutUnit block_edge,
                                  LayoutUnit line_box_edge,
                                  LayoutUnit ellipsis_width);
  // Return the truncatedWidth, the width of the truncated text + ellipsis.
  LayoutUnit PlaceEllipsis(const AtomicString& ellipsis_str,
                           bool ltr,
                           LayoutUnit block_left_edge,
                           LayoutUnit block_right_edge,
                           LayoutUnit ellipsis_width,
                           LayoutUnit logical_left_offset,
                           InlineBox** found_box,
                           ForceEllipsisOnLine = DoNotForceEllipsis);
  // Return the position of the EllipsisBox or -1.
  LayoutUnit PlaceEllipsisBox(bool ltr,
                              LayoutUnit block_left_edge,
                              LayoutUnit block_right_edge,
                              LayoutUnit ellipsis_width,
                              LayoutUnit& truncated_width,
                              InlineBox** found_box,
                              LayoutUnit logical_left_offset) final;

  using InlineBox::HasEllipsisBox;
  EllipsisBox* GetEllipsisBox() const;

  void ClearTruncation() final;

  LayoutUnit BaselinePosition(FontBaseline baseline_type) const final;
  LayoutUnit LineHeight() const final;

  void Paint(const PaintInfo&,
             const PhysicalOffset&,
             LayoutUnit line_top,
             LayoutUnit line_bottom) const override;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   LayoutUnit line_top,
                   LayoutUnit line_bottom) override;

  bool IsSelected() const final;
  InlineBox* FirstSelectedBox() const;
  InlineBox* LastSelectedBox() const;

  LineLayoutBlockFlow Block() const;

  const LayoutObject* ClosestLeafChildForPoint(const LayoutPoint&,
                                               bool only_editable_leaves) const;
  InlineBox* ClosestLeafChildForLogicalLeftPosition(
      LayoutUnit,
      bool only_editable_leaves = false) const;

  void AppendFloat(LayoutBox* floating_box) {
    DCHECK(!IsDirty());
    if (floats_)
      floats_->push_back(floating_box);
    else
      floats_ = std::make_unique<Vector<LayoutBox*>>(1, floating_box);
  }

  Vector<LayoutBox*>* FloatsPtr() {
    DCHECK(!IsDirty());
    return floats_.get();
  }

  void ExtractLineBoxFromLayoutObject() final;
  void AttachLineBoxToLayoutObject() final;
  void RemoveLineBoxFromLayoutObject() final;

  FontBaseline BaselineType() const {
    return static_cast<FontBaseline>(baseline_type_);
  }

  bool HasAnnotationsBefore() const { return has_annotations_before_; }
  bool HasAnnotationsAfter() const { return has_annotations_after_; }

  LayoutRect PaddedLayoutOverflowRect(LayoutUnit end_padding) const;

  void AscentAndDescentForBox(InlineBox*,
                              GlyphOverflowAndFallbackFontsMap&,
                              LayoutUnit& ascent,
                              LayoutUnit& descent,
                              bool& affects_ascent,
                              bool& affects_descent) const;
  LayoutUnit VerticalPositionForBox(InlineBox*, VerticalPositionCache&);
  bool IncludeLeadingForBox(InlineBox*) const;

  LayoutUnit LogicalTopVisualOverflow() const {
    return InlineFlowBox::LogicalTopVisualOverflow(LineTop());
  }
  LayoutUnit LogicalBottomVisualOverflow() const {
    return InlineFlowBox::LogicalBottomVisualOverflow(LineBottom());
  }
  LayoutUnit LogicalTopLayoutOverflow() const {
    return InlineFlowBox::LogicalTopLayoutOverflow(LineTop());
  }
  LayoutUnit LogicalBottomLayoutOverflow() const {
    return InlineFlowBox::LogicalBottomLayoutOverflow(LineBottom());
  }

  typedef void (*CustomInlineBoxRangeReverse)(
      Vector<InlineBox*>::iterator first,
      Vector<InlineBox*>::iterator last);
  void CollectLeafBoxesInLogicalOrder(
      Vector<InlineBox*>&,
      CustomInlineBoxRangeReverse custom_reverse_implementation =
          nullptr) const;

  const InlineBox* GetLogicalStartNonPseudoBox() const;
  const InlineBox* GetLogicalEndNonPseudoBox() const;

  const char* BoxName() const override;

 private:
  LayoutUnit BeforeAnnotationsAdjustment() const;

  // This folds into the padding at the end of InlineFlowBox on 64-bit.
  unsigned line_break_pos_;

  // Where this line ended.  The exact object and the position within that
  // object are stored so that we can create an InlineIterator beginning just
  // after the end of this line.
  LineLayoutItem line_break_obj_;
  scoped_refptr<BidiContext> line_break_context_;

  // Floats hanging off the line are pushed into this vector during layout. It
  // is only good for as long as the line has not been marked dirty.
  std::unique_ptr<Vector<LayoutBox*>> floats_;

  LayoutUnit line_top_;
  LayoutUnit line_bottom_;
  LayoutUnit line_top_with_leading_;
  LayoutUnit line_bottom_with_leading_;
  LayoutUnit selection_bottom_;
  LayoutUnit pagination_strut_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ROOT_INLINE_BOX_H_
