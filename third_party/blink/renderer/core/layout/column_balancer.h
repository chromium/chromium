// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_BALANCER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_BALANCER_H_

#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// A column balancer traverses a portion of the subtree of a flow thread that
// belongs to one or more fragmentainer groups within one column set, in order
// to collect certain data to be used for column balancing. This is an abstract
// class that just walks the subtree and leaves it to subclasses to actually
// collect data.
class ColumnBalancer {
  STACK_ALLOCATED();

 protected:
  ColumnBalancer(const LayoutMultiColumnSet&,
                 LayoutUnit logical_top_in_flow_thread,
                 LayoutUnit logical_bottom_in_flow_thread);

  const LayoutMultiColumnSet& ColumnSet() const { return column_set_; }

  // The flow thread portion we're examining. It may be that of the entire
  // column set, or just of a fragmentainer group.
  const LayoutUnit LogicalTopInFlowThread() const {
    return logical_top_in_flow_thread_;
  }
  const LayoutUnit LogicalBottomInFlowThread() const {
    return logical_bottom_in_flow_thread_;
  }

  const MultiColumnFragmentainerGroup& GroupAtOffset(
      LayoutUnit offset_in_flow_thread) const {
    return column_set_.FragmentainerGroupAtFlowThreadOffset(
        offset_in_flow_thread, LayoutBox::kAssociateWithLatterPage);
  }

  LayoutUnit OffsetFromColumnLogicalTop(
      LayoutUnit offset_in_flow_thread) const {
    return offset_in_flow_thread -
           GroupAtOffset(offset_in_flow_thread)
               .ColumnLogicalTopForOffset(offset_in_flow_thread);
  }

  // Flow thread offset for the layout object that we're currently examining.
  LayoutUnit FlowThreadOffset() const { return flow_thread_offset_; }

  // Return true if the specified offset is at the top of a column, as long as
  // it's not the first column in the flow thread portion.
  bool IsFirstAfterBreak(LayoutUnit flow_thread_offset) const {
    if (flow_thread_offset <= logical_top_in_flow_thread_) {
      // The first column is either not after any break at all, or after a break
      // in a previous fragmentainer group.
      return false;
    }
    const auto& group = GroupAtOffset(flow_thread_offset);
    if (!group.IsLogicalHeightKnown())
      return false;
    return flow_thread_offset ==
           group.ColumnLogicalTopForOffset(flow_thread_offset);
  }

  bool IsLogicalTopWithinBounds(LayoutUnit logical_top_in_flow_thread) const {
    return logical_top_in_flow_thread >= logical_top_in_flow_thread_ &&
           logical_top_in_flow_thread < logical_bottom_in_flow_thread_;
  }

  bool IsLogicalBottomWithinBounds(
      LayoutUnit logical_bottom_in_flow_thread) const {
    return logical_bottom_in_flow_thread > logical_top_in_flow_thread_ &&
           logical_bottom_in_flow_thread <= logical_bottom_in_flow_thread_;
  }

  // Examine and collect column balancing data from a layout box that has been
  // found to intersect with the flow thread portion we're examining. Does not
  // recurse into children. flowThreadOffset() will return the offset from |box|
  // to the flow thread. Two hooks are provided here. The first one is called
  // right after entering and before traversing the subtree of the box, and the
  // second one right after having traversed the subtree.
  virtual void ExamineBoxAfterEntering(
      const LayoutBox&,
      LayoutUnit child_logical_height,
      EBreakBetween previous_break_after_value) = 0;
  virtual void ExamineBoxBeforeLeaving(const LayoutBox&,
                                       LayoutUnit child_logical_height) = 0;

  // Examine and collect column balancing data from a line that has been found
  // to intersect with the flow thread portion. Does not recurse into layout
  // objects on that line.
  virtual void ExamineLine(const RootInlineBox&) = 0;

  // Examine and collect column balancing data for everything in the flow thread
  // portion. Will trigger calls to examineBoxAfterEntering(),
  // examineBoxBeforeLeaving() and examineLine() for interesting boxes and
  // lines.
  void Traverse();

 private:
  void TraverseSubtree(const LayoutBox&);

  void TraverseLines(const LayoutBlockFlow&);
  void TraverseChildren(const LayoutObject&);

  const LayoutMultiColumnSet& column_set_;
  const LayoutUnit logical_top_in_flow_thread_;
  const LayoutUnit logical_bottom_in_flow_thread_;

  LayoutUnit flow_thread_offset_;
};

// After an initial layout pass, we know the height of the contents of a flow
// thread. Based on this, we can estimate an initial minimal column height. This
// class will collect the necessary information from the layout objects to make
// this estimate. This estimate may be used to perform another layout iteration.
// If we after such a layout iteration cannot fit the contents with the given
// column height without creating overflowing columns, we will have to stretch
// the columns by some amount and lay out again. We may need to do this several
// times (but typically not more times than the number of columns that we have).
// The amount to stretch is provided by the sibling of this class, named
// MinimumSpaceShortageFinder.
class InitialColumnHeightFinder final : public ColumnBalancer {
 public:
  InitialColumnHeightFinder(const LayoutMultiColumnSet&,
                            LayoutUnit logical_top_in_flow_thread,
                            LayoutUnit logical_bottom_in_flow_thread);

  LayoutUnit InitialMinimalBalancedHeight() const;

  // Height of the tallest piece of unbreakable content. This is the minimum
  // column logical height required to avoid fragmentation where it shouldn't
  // occur (inside unbreakable content, between orphans and widows, etc.). This
  // will be used as a hint to the column balancer to help set a good initial
  // column height.
  LayoutUnit TallestUnbreakableLogicalHeight() const {
    return tallest_unbreakable_logical_height_;
  }

 private:
  void ExamineBoxAfterEntering(
      const LayoutBox&,
      LayoutUnit child_logical_height,
      EBreakBetween previous_break_after_value) override;
  void ExamineBoxBeforeLeaving(const LayoutBox&,
                               LayoutUnit child_logical_height) override;
  void ExamineLine(const RootInlineBox&) override;

  // Record that there's a pagination strut that ends at the specified
  // |offsetInFlowThread|, which is an offset exactly at the top of some column.
  void RecordStrutBeforeOffset(LayoutUnit offset_in_flow_thread,
                               LayoutUnit strut);

  // Return the accumulated space used by struts at all column boundaries
  // preceding the specified flowthread offset.
  LayoutUnit SpaceUsedByStrutsAt(LayoutUnit offset_in_flow_thread) const;

  // Add a content run, specified by its end position. A content run is appended
  // at every forced/explicit break and at the end of the column set. The
  // content runs are used to determine where implicit/soft breaks will occur,
  // in order to calculate an initial column height.
  void AddContentRun(LayoutUnit end_offset_in_flow_thread);

  // Normally we'll just return 0 here, because in most cases we won't add more
  // content runs than used column-count. However, if we're at the initial
  // balancing pass for a multicol that lives inside another to-be-balanced
  // outer multicol container, and there is a sufficient number of forced column
  // breaks, we may exceed used column-count. In such cases, we're going to
  // assume a minimal number of fragmentainer groups (rows) that will eventually
  // be created, and when distributing implicit column breaks to calculate an
  // initial balanced height, we'll only focus on content that has any chance at
  // all to end up in the last row.
  unsigned FirstContentRunIndexInLastRow() const {
    unsigned column_count = ColumnSet().UsedColumnCount();
    if (content_runs_.size() <= column_count)
      return 0;
    unsigned last_run_index = content_runs_.size() - 1;
    return last_run_index / column_count * column_count;
  }

  // Return the index of the content run with the currently tallest columns,
  // taking all implicit breaks assumed so far into account.
  unsigned ContentRunIndexWithTallestColumns() const;

  // Given the current list of content runs, make assumptions about where we
  // need to insert implicit breaks (if there's room for any at all; depending
  // on the number of explicit breaks), and store the results. This is needed in
  // order to balance the columns.
  void DistributeImplicitBreaks();

  // A run of content without explicit (forced) breaks; i.e. a flow thread
  // portion between two explicit breaks, between flow thread start and an
  // explicit break, between an explicit break and flow thread end, or, in cases
  // when there are no explicit breaks at all: between flow thread portion start
  // and flow thread portion end. We need to know where the explicit breaks are,
  // in order to figure out where the implicit breaks will end up, so that we
  // get the columns properly balanced. A content run starts out as representing
  // one single column, and will represent one additional column for each
  // implicit break "inserted" there.
  class ContentRun {
    DISALLOW_NEW();

   public:
    explicit ContentRun(LayoutUnit break_offset)
        : break_offset_(break_offset), assumed_implicit_breaks_(0) {}

    unsigned AssumedImplicitBreaks() const { return assumed_implicit_breaks_; }
    void AssumeAnotherImplicitBreak() { assumed_implicit_breaks_++; }
    LayoutUnit BreakOffset() const { return break_offset_; }

    // Return the column height that this content run would require, considering
    // the implicit breaks assumed so far.
    LayoutUnit ColumnLogicalHeight(LayoutUnit start_offset) const {
      return LayoutUnit::FromFloatCeil(float(break_offset_ - start_offset) /
                                       float(assumed_implicit_breaks_ + 1));
    }

   private:
    LayoutUnit break_offset_;  // Flow thread offset where this run ends.
    unsigned assumed_implicit_breaks_;  // Number of implicit breaks in this run
                                        // assumed so far.
  };
  Vector<ContentRun, 32> content_runs_;

  // Shortest strut found at each column boundary (index 0 being the boundary
  // between the first and the second column, index 1 being the one between the
  // second and the third boundary, and so on). There may be several objects
  // that cross the same column boundary, and we're only interested in the
  // shortest one. For example, when having a float beside regular in-flow
  // content, we end up with two parallel fragmentation flows [1]. The shortest
  // strut found at a column boundary is the amount of space that we wasted at
  // said column boundary, and it needs to be deducted when estimating the
  // initial balanced column height, or we risk making the column row too tall.
  // An entry set to LayoutUnit::max() means that we didn't detect any object
  // crossing that boundary.
  //
  // [1] http://www.w3.org/TR/css3-break/#parallel-flows
  Vector<LayoutUnit, 32> shortest_struts_;

  LayoutUnit tallest_unbreakable_logical_height_;
  LayoutUnit last_break_seen_;
};

// If we have previously used InitialColumnHeightFinder to estimate an initial
// column height, and that didn't result in tall enough columns, we need
// subsequent layout passes where we increase the column height by the minimum
// space shortage at column breaks. This class finds the minimum space shortage
// after having laid out with the current column height.
class MinimumSpaceShortageFinder final : public ColumnBalancer {
 public:
  MinimumSpaceShortageFinder(const LayoutMultiColumnSet&,
                             LayoutUnit logical_top_in_flow_thread,
                             LayoutUnit logical_bottom_in_flow_thread);

  LayoutUnit MinimumSpaceShortage() const { return minimum_space_shortage_; }
  unsigned ForcedBreaksCount() const { return forced_breaks_count_; }

 private:
  void ExamineBoxAfterEntering(
      const LayoutBox&,
      LayoutUnit child_logical_height,
      EBreakBetween previous_break_after_value) override;
  void ExamineBoxBeforeLeaving(const LayoutBox&,
                               LayoutUnit child_logical_height) override;
  void ExamineLine(const RootInlineBox&) override;

  void RecordSpaceShortage(LayoutUnit shortage) {
    // Only positive values are interesting (and allowed) here. Zero space
    // shortage may be reported when we're at the top of a column and the
    // element has zero height.
    if (shortage > 0)
      minimum_space_shortage_ = std::min(minimum_space_shortage_, shortage);
  }

  // The smallest amout of space shortage that caused a column break.
  LayoutUnit minimum_space_shortage_;

  // Set when breaking before a block, and we're looking for the first
  // unbreakable descendant, in order to report correct space shortage for that
  // one.
  LayoutUnit pending_strut_;

  unsigned forced_breaks_count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_BALANCER_H_
