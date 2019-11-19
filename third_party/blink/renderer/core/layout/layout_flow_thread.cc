/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"

#include "third_party/blink/renderer/core/layout/fragmentainer_iterator.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"

namespace blink {

LayoutFlowThread::LayoutFlowThread()
    : LayoutBlockFlow(nullptr),
      column_sets_invalidated_(false),
      page_logical_size_changed_(false) {}

LayoutFlowThread* LayoutFlowThread::LocateFlowThreadContainingBlockOf(
    const LayoutObject& descendant,
    AncestorSearchConstraint constraint) {
  DCHECK(descendant.IsInsideFlowThread());
  LayoutObject* curr = const_cast<LayoutObject*>(&descendant);
  while (curr) {
    if (curr->IsSVGChild())
      return nullptr;
    if (curr->IsLayoutFlowThread())
      return ToLayoutFlowThread(curr);
    LayoutObject* container = curr->Container();
    // If we're inside something strictly unbreakable (due to having scrollbars
    // or being writing mode roots, for instance), it's also strictly
    // unbreakable in any outer fragmentation context. As such, what goes on
    // inside any fragmentation context on the inside of this is completely
    // opaque to ancestor fragmentation contexts.
    if (constraint == kIsolateUnbreakableContainers && container &&
        container->IsBox() &&
        ToLayoutBox(container)->GetPaginationBreakability() == kForbidBreaks)
      return nullptr;
    curr = curr->Parent();
    while (curr != container) {
      if (curr->IsLayoutFlowThread()) {
        // The nearest ancestor flow thread isn't in our containing block chain.
        // Then we aren't really part of any flow thread, and we should stop
        // looking. This happens when there are out-of-flow objects or column
        // spanners.
        return nullptr;
      }
      curr = curr->Parent();
    }
  }
  return nullptr;
}

void LayoutFlowThread::RemoveColumnSetFromThread(
    LayoutMultiColumnSet* column_set) {
  DCHECK(column_set);
  multi_column_set_list_.erase(column_set);
  InvalidateColumnSets();
  // Clear the interval tree right away, instead of leaving it around with dead
  // objects. Not that anyone _should_ try to access the interval tree when the
  // column sets are marked as invalid, but this is actually possible if other
  // parts of the engine has bugs that cause us to not lay out everything that
  // was marked for layout, so that LayoutObject::assertLaidOut() (and a LOT
  // of other assertions) fails.
  multi_column_set_interval_tree_.Clear();
}

void LayoutFlowThread::ValidateColumnSets() {
  column_sets_invalidated_ = false;
  // Called to get the maximum logical width for the columnSet.
  UpdateLogicalWidth();
  GenerateColumnSetIntervalTree();
}

bool LayoutFlowThread::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags visual_rect_flags) const {
  // A flow thread should never be an invalidation container.
  DCHECK_NE(ancestor, this);
  transform_state.Flatten();
  LayoutRect rect(transform_state.LastPlanarQuad().BoundingBox());
  rect = FragmentsBoundingBox(rect);
  transform_state.SetQuad(FloatQuad(FloatRect(rect)));
  return LayoutBlockFlow::MapToVisualRectInAncestorSpaceInternal(
      ancestor, transform_state, visual_rect_flags);
}

void LayoutFlowThread::UpdateLayout() {
  page_logical_size_changed_ = column_sets_invalidated_ && EverHadLayout();
  LayoutBlockFlow::UpdateLayout();
  page_logical_size_changed_ = false;
}

void LayoutFlowThread::ComputeLogicalHeight(
    LayoutUnit,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  computed_values.position_ = logical_top;
  computed_values.extent_ = LayoutUnit();

  for (LayoutMultiColumnSetList::const_iterator iter =
           multi_column_set_list_.begin();
       iter != multi_column_set_list_.end(); ++iter) {
    LayoutMultiColumnSet* column_set = *iter;
    computed_values.extent_ += column_set->LogicalHeightInFlowThread();
  }
}

void LayoutFlowThread::AbsoluteQuadsForDescendant(const LayoutBox& descendant,
                                                  Vector<FloatQuad>& quads,
                                                  MapCoordinatesFlags mode) {
  LayoutPoint offset_from_flow_thread;
  for (const LayoutObject* object = &descendant; object != this;) {
    const LayoutObject* container = object->Container();
    offset_from_flow_thread +=
        object->OffsetFromContainer(container).ToLayoutSize();
    object = container;
  }
  LayoutRect bounding_rect_in_flow_thread(offset_from_flow_thread,
                                          descendant.FrameRect().Size());
  // Set up a fragments relative to the descendant, in the flow thread
  // coordinate space, and convert each of them, individually, to absolute
  // coordinates.
  for (FragmentainerIterator iterator(*this, bounding_rect_in_flow_thread);
       !iterator.AtEnd(); iterator.Advance()) {
    LayoutRect fragment = bounding_rect_in_flow_thread;
    // We use inclusiveIntersect() because intersect() would reset the
    // coordinates for zero-height objects.
    LayoutRect clip_rect = iterator.ClipRectInFlowThread();
    fragment.InclusiveIntersect(clip_rect);
    fragment.MoveBy(-offset_from_flow_thread);
    quads.push_back(descendant.LocalRectToAbsoluteQuad(
        PhysicalRectToBeNoop(fragment), mode));
  }
}

void LayoutFlowThread::AddOutlineRects(
    Vector<PhysicalRect>& rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  Vector<PhysicalRect> rects_in_flowthread;
  LayoutBlockFlow::AddOutlineRects(rects_in_flowthread, additional_offset,
                                   include_block_overflows);
  // Convert the rectangles from the flow thread coordinate space to the visual
  // space. The approach here is very simplistic; just calculate a bounding box
  // in flow thread coordinates and convert it to one in visual
  // coordinates. While the solution can be made more sophisticated by
  // e.g. using FragmentainerIterator, the usefulness isn't obvious: our
  // multicol implementation has practically no support for overflow in the
  // block direction anyway. As far as the inline direction (the column
  // progression direction) is concerned, we'll just include the full height of
  // each column involved. Should be good enough.
  rects.push_back(PhysicalRectToBeNoop(
      FragmentsBoundingBox(UnionRect(rects_in_flowthread).ToLayoutRect())));
}

bool LayoutFlowThread::NodeAtPoint(HitTestResult& result,
                                   const HitTestLocation& hit_test_location,
                                   const PhysicalOffset& accumulated_offset,
                                   HitTestAction hit_test_action) {
  if (hit_test_action == kHitTestBlockBackground)
    return false;
  return LayoutBlockFlow::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, hit_test_action);
}

LayoutUnit LayoutFlowThread::PageLogicalHeightForOffset(
    LayoutUnit offset) const {
  DCHECK(IsPageLogicalHeightKnown());
  LayoutMultiColumnSet* column_set =
      ColumnSetAtBlockOffset(offset, kAssociateWithLatterPage);
  if (!column_set)
    return LayoutUnit(1);

  return column_set->PageLogicalHeightForOffset(offset);
}

LayoutUnit LayoutFlowThread::PageRemainingLogicalHeightForOffset(
    LayoutUnit offset,
    PageBoundaryRule page_boundary_rule) const {
  DCHECK(IsPageLogicalHeightKnown());
  LayoutMultiColumnSet* column_set =
      ColumnSetAtBlockOffset(offset, page_boundary_rule);
  if (!column_set)
    return LayoutUnit(1);

  return column_set->PageRemainingLogicalHeightForOffset(offset,
                                                         page_boundary_rule);
}

void LayoutFlowThread::GenerateColumnSetIntervalTree() {
  // FIXME: Optimize not to clear the interval all the time. This implies
  // manually managing the tree nodes lifecycle.
  multi_column_set_interval_tree_.Clear();
  multi_column_set_interval_tree_.InitIfNeeded();
  for (auto* column_set : multi_column_set_list_)
    multi_column_set_interval_tree_.Add(
        MultiColumnSetIntervalTree::CreateInterval(
            column_set->LogicalTopInFlowThread(),
            column_set->LogicalBottomInFlowThread(), column_set));
}

LayoutUnit LayoutFlowThread::NextLogicalTopForUnbreakableContent(
    LayoutUnit flow_thread_offset,
    LayoutUnit content_logical_height) const {
  LayoutMultiColumnSet* column_set =
      ColumnSetAtBlockOffset(flow_thread_offset, kAssociateWithLatterPage);
  if (!column_set)
    return flow_thread_offset;
  return column_set->NextLogicalTopForUnbreakableContent(
      flow_thread_offset, content_logical_height);
}

LayoutRect LayoutFlowThread::FragmentsBoundingBox(
    const LayoutRect& layer_bounding_box) const {
  DCHECK(!column_sets_invalidated_);

  LayoutRect result;
  for (auto* column_set : multi_column_set_list_)
    result.Unite(column_set->FragmentsBoundingBox(layer_bounding_box));

  return result;
}

void LayoutFlowThread::FlowThreadToContainingCoordinateSpace(
    LayoutUnit& block_position,
    LayoutUnit& inline_position) const {
  LayoutPoint position(inline_position, block_position);
  // First we have to make |position| physical, because that's what offsetLeft()
  // expects and returns.
  if (!IsHorizontalWritingMode())
    position = position.TransposedPoint();
  position = DeprecatedFlipForWritingMode(position);

  position.Move(ColumnOffset(position));

  // Make |position| logical again, and read out the values.
  position = DeprecatedFlipForWritingMode(position);
  if (!IsHorizontalWritingMode())
    position = position.TransposedPoint();
  block_position = position.Y();
  inline_position = position.X();
}

void LayoutFlowThread::MultiColumnSetSearchAdapter::CollectIfNeeded(
    const MultiColumnSetInterval& interval) {
  if (result_)
    return;
  if (interval.Low() <= offset_ && interval.High() > offset_)
    result_ = interval.Data();
}

}  // namespace blink
