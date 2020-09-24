/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_MULTI_COLUMN_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_MULTI_COLUMN_SET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// A set of columns in a multicol container. A column set is inserted as an
// anonymous child of the actual multicol container (i.e. the layoutObject whose
// style computes to non-auto column-count and/or column-width), next to the
// flow thread. There'll be one column set for each contiguous run of column
// content. The only thing that can interrupt a contiguous run of column content
// is a column spanner, which means that if there are no spanners, there'll
// only be one column set.
//
// Since a spanner interrupts an otherwise contiguous run of column content,
// inserting one may result in the creation of additional new column sets. A
// placeholder for the spanning layoutObject has to be placed in between the
// column sets that come before and after the spanner, if there's actually
// column content both before and after the spanner.
//
// A column set has no children on its own, but is merely used to slice a
// portion of the tall "single-column" flow thread into actual columns visually,
// to convert from flow thread coordinates to visual ones. It is in charge of
// both positioning columns correctly relatively to the parent multicol
// container, and to calculate the correct translation for each column's
// contents, and to paint any rules between them. LayoutMultiColumnSet objects
// are used for painting, hit testing, and any other type of operation that
// requires mapping from flow thread coordinates to visual coordinates.
//
// Columns are normally laid out in the inline progression direction, but if the
// multicol container is inside another fragmentation context (e.g. paged media,
// or an another multicol container), we may need to group the columns, so
// that we get one MultiColumnFragmentainerGroup for each outer fragmentainer
// (page / column) that the inner multicol container lives in. Each
// fragmentainer group has its own column height, but the column height is
// uniform within a group.
class CORE_EXPORT LayoutMultiColumnSet final : public LayoutBlockFlow {
 public:
  static LayoutMultiColumnSet* CreateAnonymous(
      LayoutFlowThread&,
      const ComputedStyle& parent_style);

  const MultiColumnFragmentainerGroup& FirstFragmentainerGroup() const {
    return fragmentainer_groups_.First();
  }
  const MultiColumnFragmentainerGroup& LastFragmentainerGroup() const {
    return fragmentainer_groups_.Last();
  }
  unsigned FragmentainerGroupIndexAtFlowThreadOffset(LayoutUnit,
                                                     PageBoundaryRule) const;
  MultiColumnFragmentainerGroup& FragmentainerGroupAtFlowThreadOffset(
      LayoutUnit flow_thread_offset,
      PageBoundaryRule rule) {
    return fragmentainer_groups_[FragmentainerGroupIndexAtFlowThreadOffset(
        flow_thread_offset, rule)];
  }
  const MultiColumnFragmentainerGroup& FragmentainerGroupAtFlowThreadOffset(
      LayoutUnit flow_thread_offset,
      PageBoundaryRule rule) const {
    return fragmentainer_groups_[FragmentainerGroupIndexAtFlowThreadOffset(
        flow_thread_offset, rule)];
  }
  const MultiColumnFragmentainerGroup& FragmentainerGroupAtVisualPoint(
      const LayoutPoint&) const;
  const MultiColumnFragmentainerGroupList& FragmentainerGroups() const {
    return fragmentainer_groups_;
  }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutMultiColumnSet ||
           LayoutBlockFlow::IsOfType(type);
  }
  bool CanHaveChildren() const final { return false; }

  // Return the width and height of a single column or page in the set.
  LayoutUnit PageLogicalWidth() const { return FlowThread()->LogicalWidth(); }
  LayoutUnit PageLogicalHeightForOffset(LayoutUnit) const;
  LayoutUnit PageRemainingLogicalHeightForOffset(LayoutUnit,
                                                 PageBoundaryRule) const;
  bool IsPageLogicalHeightKnown() const;

  // Return true if there's nothing with the current state of column balancing
  // that prevents us from inserting additional fragmentainer groups, if needed.
  bool NewFragmentainerGroupsAllowed() const;

  LayoutUnit TallestUnbreakableLogicalHeight() const {
    return tallest_unbreakable_logical_height_;
  }
  void PropagateTallestUnbreakableLogicalHeight(LayoutUnit value) {
    tallest_unbreakable_logical_height_ =
        std::max(value, tallest_unbreakable_logical_height_);
  }

  LayoutUnit NextLogicalTopForUnbreakableContent(
      LayoutUnit flow_thread_offset,
      LayoutUnit content_logical_height) const;

  LayoutFlowThread* FlowThread() const { return flow_thread_; }

  LayoutBlockFlow* MultiColumnBlockFlow() const {
    return To<LayoutBlockFlow>(Parent());
  }
  LayoutMultiColumnFlowThread* MultiColumnFlowThread() const {
    return ToLayoutMultiColumnFlowThread(FlowThread());
  }

  LayoutMultiColumnSet* NextSiblingMultiColumnSet() const;
  LayoutMultiColumnSet* PreviousSiblingMultiColumnSet() const;

  // Return true if we need to create additional fragmentainer group(s) to hold
  // a column at the specified flow thread block offset.
  bool NeedsNewFragmentainerGroupAt(LayoutUnit bottom_offset_in_flow_thread,
                                    PageBoundaryRule) const;

  MultiColumnFragmentainerGroup& AppendNewFragmentainerGroup();

  // Logical top relative to the content edge of the multicol container.
  LayoutUnit LogicalTopFromMulticolContentEdge() const;

  LayoutUnit LogicalTopInFlowThread() const;
  LayoutUnit LogicalBottomInFlowThread() const;
  LayoutUnit LogicalHeightInFlowThread() const {
    // Due to negative margins, logical bottom may actually end up above logical
    // top, but we never want to return negative logical heights.
    return (LogicalBottomInFlowThread() - LogicalTopInFlowThread())
        .ClampNegativeToZero();
  }

  // Return the amount of flow thread contents that the specified fragmentainer
  // group can hold without overflowing.
  LayoutUnit FragmentainerGroupCapacity(
      const MultiColumnFragmentainerGroup& group) const {
    return group.ColumnLogicalHeight() * UsedColumnCount();
  }

  LayoutRect FlowThreadPortionRect() const;

  // The used CSS value of column-count, i.e. how many columns there are room
  // for without overflowing.
  unsigned UsedColumnCount() const {
    return MultiColumnFlowThread()->ColumnCount();
  }

  bool HeightIsAuto() const;

  // Find the column that contains the given block offset, and return the
  // translation needed to get from flow thread coordinates to visual
  // coordinates.
  LayoutSize FlowThreadTranslationAtOffset(LayoutUnit,
                                           PageBoundaryRule,
                                           CoordinateSpaceConversion) const;

  LayoutPoint VisualPointToFlowThreadPoint(
      const LayoutPoint& visual_point) const;

  // (Re-)calculate the column height if it's auto. This is first and foremost
  // needed by sets that are to balance the column height, but even when it
  // isn't to be balanced, this is necessary if the multicol container's height
  // is constrained.
  bool RecalculateColumnHeight();

  // Reset previously calculated column height. Will mark for layout if needed.
  void ResetColumnHeight();

  void StoreOldPosition() { old_logical_top_ = LogicalTop(); }
  bool IsInitialHeightCalculated() const { return initial_height_calculated_; }

  // Layout of flow thread content that's to be rendered inside this column set
  // begins. This happens at the beginning of flow thread layout, and when
  // advancing from a previous column set or spanner to this one.
  void BeginFlow(LayoutUnit offset_in_flow_thread);

  // Layout of flow thread content that was to be rendered inside this column
  // set has finished. This happens at end of flow thread layout, and when
  // advancing to the next column set or spanner.
  void EndFlow(LayoutUnit offset_in_flow_thread);

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void UpdateLayout() override;

  MinMaxSizes ComputeIntrinsicLogicalWidths() const final;

  void AttachToFlowThread();
  void DetachFromFlowThread();

  // The top of the page nearest to the specified block offset. All in
  // flowthread coordinates.
  LayoutUnit PageLogicalTopForOffset(LayoutUnit offset) const;

  LayoutRect FragmentsBoundingBox(
      const LayoutRect& bounding_box_in_flow_thread) const;

  LayoutUnit ColumnGap() const;

  // The "CSS actual" value of column-count. This includes overflowing columns,
  // if any.
  unsigned ActualColumnCount() const;

  const char* GetName() const override { return "LayoutMultiColumnSet"; }

  // Sets |columnRuleBounds| to the bounds of each column rule rect's painted
  // extent, adjusted by paint offset, before pixel snapping. Returns true if
  // column rules should be painted at all.
  bool ComputeColumnRuleBounds(const LayoutPoint& paint_offset,
                               Vector<LayoutRect>& column_rule_bounds) const;

  void UpdateFromNG();

 protected:
  LayoutMultiColumnSet(LayoutFlowThread*);

 private:
  PhysicalRect LocalVisualRectIgnoringVisibility() const final;

  void InsertedIntoTree() final;
  void WillBeRemovedFromTree() final;

  bool IsSelfCollapsingBlock() const override { return false; }

  void ComputeLogicalHeight(LayoutUnit logical_height,
                            LayoutUnit logical_top,
                            LogicalExtentComputedValues&) const override;
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  void PaintObject(const PaintInfo&,
                   const PhysicalOffset& paint_offset) const override;

  void ComputeVisualOverflow(bool recompute_floats) final;

  void AddVisualOverflowFromChildren();
  void AddLayoutOverflowFromChildren() override;

  MultiColumnFragmentainerGroupList fragmentainer_groups_;
  LayoutFlowThread* flow_thread_;

  // Height of the tallest piece of unbreakable content. This is the minimum
  // column logical height required to avoid fragmentation where it shouldn't
  // occur (inside unbreakable content, between orphans and widows, etc.).
  // We only store this so that outer fragmentation contexts (if any) can query
  // this when calculating their own minimum. Note that we don't store this
  // value in every fragmentainer group (but rather here, in the column set),
  // since we only need the largest one among them.
  LayoutUnit tallest_unbreakable_logical_height_;

  // Logical top in previous layout pass.
  LayoutUnit old_logical_top_;

  bool initial_height_calculated_;

  unsigned last_actual_column_count_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutMultiColumnSet, IsLayoutMultiColumnSet());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_MULTI_COLUMN_SET_H_
