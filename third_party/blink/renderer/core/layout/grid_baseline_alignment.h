// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_BASELINE_ALIGNMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_BASELINE_ALIGNMENT_H_

#include "third_party/blink/renderer/core/layout/grid_layout_utils.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// These classes are used to implement the Baseline Alignment logic, as
// described in the CSS Box Alignment specification.
// https://drafts.csswg.org/css-align/#baseline-terms

// A baseline-sharing group is composed of boxes that participate in
// baseline alignment together. This is possible only if they:
//
//   * Share an alignment context along an axis perpendicular to their
//   baseline alignment axis.
//   * Have compatible baseline alignment preferences (i.e., the baselines
//   that want to align are on the same side of the alignment context).
//
// Once the BaselineGroup is instantiated, defined by a
// 'block-direction' (WritingMode) and a 'baseline-preference'
// (first/last baseline), it's ready to collect the items that will
// participate in the Baseline Alignment logic.
//
// The 'Update' method is used to store an item (if not already
// present) and update the max_ascent and max_descent associated to
// this baseline-sharing group.
class BaselineGroup {
  DISALLOW_NEW();

 public:
  void Update(const LayoutBox&, LayoutUnit ascent, LayoutUnit descent);
  LayoutUnit MaxAscent() const { return max_ascent_; }
  LayoutUnit MaxDescent() const { return max_descent_; }
  int size() const { return items_.size(); }

 private:
  friend class BaselineContext;
  BaselineGroup(WritingMode block_flow, ItemPosition child_preference);

  // Determines whether a baseline-sharing group is compatible with an
  // item, based on its 'block-flow' and 'baseline-preference'
  bool IsCompatible(WritingMode, ItemPosition) const;

  // Determines whether the baseline-sharing group's associated
  // block-flow is opposite (LR vs RL) to particular item's
  // writing-mode.
  bool IsOppositeBlockFlow(WritingMode block_flow) const;

  // Determines whether the baseline-sharing group's associated
  // block-flow is orthogonal (vertical vs horizontal) to particular
  // item's writing-mode.
  bool IsOrthogonalBlockFlow(WritingMode block_flow) const;

  WritingMode block_flow_;
  ItemPosition preference_;
  LayoutUnit max_ascent_;
  LayoutUnit max_descent_;
  HashSet<const LayoutBox*> items_;
};

// Boxes share an alignment context along a particular axis when they
// are:
//
//  * table cells in the same row, along the table's row (inline) axis
//  * table cells in the same column, along the table's column (block)
//  axis
//  * grid items in the same row, along the grid's row (inline) axis
//  * grid items in the same column, along the grid's colum (block) axis
//  * flex items in the same flex line, along the flex container's main
//  axis
//
// A Baseline alignment-context may handle several baseline-sharing
// groups. In order to create an instance, we need to pass the
// required data to define the first baseline-sharing group; a
// Baseline Context must have at least one baseline-sharing group.
//
// By adding new items to a Baseline Context, the baseline-sharing
// groups it handles are automatically updated, if there is one that
// is compatible with such item. Otherwise, a new baseline-sharing
// group is created, compatible with the new item.
class BaselineContext {
  USING_FAST_MALLOC(BaselineContext);

 public:
  BaselineContext(const LayoutBox& child,
                  ItemPosition preference,
                  LayoutUnit ascent,
                  LayoutUnit descent);
  Vector<BaselineGroup>& SharedGroups() { return shared_groups_; }
  const BaselineGroup& GetSharedGroup(const LayoutBox& child,
                                      ItemPosition preference) const;

  // Updates the baseline-sharing group compatible with the item.
  // We pass the item's baseline-preference to avoid dependencies with
  // the LayoutGrid class, which is the one managing the alignment
  // behavior of the Grid Items.
  void UpdateSharedGroup(const LayoutBox& child,
                         ItemPosition preference,
                         LayoutUnit ascent,
                         LayoutUnit descent);

 private:
  // Returns the baseline-sharing group compatible with an item.
  // We pass the item's baseline-preference to avoid dependencies with
  // the LayoutGrid class, which is the one managing the alignment
  // behavior of the Grid Items.
  // TODO Properly implement baseline-group compatibility
  // See https://github.com/w3c/csswg-drafts/issues/721
  BaselineGroup& FindCompatibleSharedGroup(const LayoutBox& child,
                                           ItemPosition preference);

  Vector<BaselineGroup> shared_groups_;
};

static inline bool IsBaselinePosition(ItemPosition position) {
  return position == ItemPosition::kBaseline ||
         position == ItemPosition::kLastBaseline;
}

// This is the class that implements the Baseline Alignment logic,
// using internally the BaselineContext and BaselineGroupd classes
// (described above).
//
// The first phase is to collect the items that will participate in
// baseline alignment together. During this phase the required
// baseline- sharing groups will be created for each Baseline
// alignment-context shared by the items participating in the baseline
// alignment.
//
// Additionally, the baseline-sharing groups' offsets, max-ascend and
// max-descent will be computed and stored. This class also computes
// the baseline offset for a particular item, based on the max-ascent
// for its associated baseline-sharing group.
class GridBaselineAlignment {
  DISALLOW_NEW();

 public:
  // Collects the items participating in baseline alignment and
  // updates the corresponding baseline-sharing group of the Baseline
  // Context the items belongs to.
  // All the baseline offsets are updated accordingly based on the
  // added item.
  void UpdateBaselineAlignmentContext(ItemPosition,
                                      unsigned shared_context,
                                      const LayoutBox&,
                                      GridAxis);

  // Returns the baseline offset of a particular item, based on the
  // max-ascent for its associated baseline-sharing group
  LayoutUnit BaselineOffsetForChild(ItemPosition,
                                    unsigned shared_context,
                                    const LayoutBox&,
                                    GridAxis) const;

  // Sets the Grid Container's writing-mode so that we can avoid the
  // dependecy of the LayoutGrid class for determining whether a grid
  // item is orthogonal or not.
  void SetBlockFlow(WritingMode block_flow) { block_flow_ = block_flow; }

  // Clearing the Baseline Alignment context and their internal
  // classes and data structures.
  void Clear(GridAxis);

 private:
  const BaselineGroup& GetBaselineGroupForChild(ItemPosition,
                                                unsigned shared_context,
                                                const LayoutBox&,
                                                GridAxis) const;
  LayoutUnit MarginOverForChild(const LayoutBox&, GridAxis) const;
  LayoutUnit MarginUnderForChild(const LayoutBox&, GridAxis) const;
  LayoutUnit LogicalAscentForChild(const LayoutBox&, GridAxis) const;
  LayoutUnit AscentForChild(const LayoutBox&, GridAxis) const;
  LayoutUnit DescentForChild(const LayoutBox&, LayoutUnit, GridAxis) const;
  bool IsDescentBaselineForChild(const LayoutBox&, GridAxis) const;
  bool IsHorizontalBaselineAxis(GridAxis) const;
  bool IsOrthogonalChildForBaseline(const LayoutBox&) const;
  bool IsParallelToBaselineAxisForChild(const LayoutBox&, GridAxis) const;

  typedef HashMap<unsigned,
                  std::unique_ptr<BaselineContext>,
                  DefaultHash<unsigned>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
      BaselineContextsMap;

  // Grid Container's WritingMode, used to determine grid item's orthogonality.
  WritingMode block_flow_;
  BaselineContextsMap row_axis_alignment_context_;
  BaselineContextsMap col_axis_alignment_context_;
};

}  // namespace blink

#endif  // BaselineContext_h
