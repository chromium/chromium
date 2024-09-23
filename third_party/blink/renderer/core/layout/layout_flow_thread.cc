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
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"

namespace blink {

LayoutFlowThread::LayoutFlowThread()
    : LayoutBlockFlow(nullptr), column_sets_invalidated_(false) {}

void LayoutFlowThread::Trace(Visitor* visitor) const {
  visitor->Trace(multi_column_set_list_);
  LayoutBlockFlow::Trace(visitor);
}

bool LayoutFlowThread::IsLayoutNGObject() const {
  NOT_DESTROYED();
  return false;
}

LayoutFlowThread* LayoutFlowThread::LocateFlowThreadContainingBlockOf(
    const LayoutObject& descendant,
    AncestorSearchConstraint constraint) {
  DCHECK(descendant.IsInsideFlowThread());
  LayoutObject* curr = const_cast<LayoutObject*>(&descendant);
  bool inner_is_ng_object = curr->IsLayoutNGObject();
  while (curr) {
    if (curr->IsSVGChild())
      return nullptr;
    // Always consider an in-flow legend child to be part of the flow
    // thread. The containing block of the rendered legend is actually the
    // multicol container itself (not its flow thread child), but since which
    // element is the rendered legend might change (if we insert another legend
    // in front of it, for instance), and such a change won't be detected by
    // this child, we'll just pretend that it's part of the flow thread. This
    // shouldn't have any negative impact on LayoutNG, and in the legacy engine,
    // a fieldset isn't allowed to be a multicol container anyway.
    if (curr->IsHTMLLegendElement() && !curr->IsOutOfFlowPositioned() &&
        !curr->IsColumnSpanAll() && curr->Parent()->IsLayoutFlowThread())
      return To<LayoutFlowThread>(curr->Parent());
    if (curr->IsLayoutFlowThread())
      return To<LayoutFlowThread>(curr);
    LayoutObject* container = curr->Container();
    // If we're inside something strictly unbreakable (due to having scrollbars
    // or being writing mode roots, for instance), it's also strictly
    // unbreakable in any outer fragmentation context. As such, what goes on
    // inside any fragmentation context on the inside of this is completely
    // opaque to ancestor fragmentation contexts.
    if (constraint == kIsolateUnbreakableContainers && container) {
      if (const auto* box = DynamicTo<LayoutBox>(container)) {
        // We're walking up the tree without knowing which fragmentation engine
        // is being used, so we have to detect any engine mismatch ourselves.
        if (box->IsLayoutNGObject() != inner_is_ng_object)
          return nullptr;
        if (box->IsMonolithic()) {
          return nullptr;
        }
      }
    }
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
  NOT_DESTROYED();
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
  NOT_DESTROYED();
  column_sets_invalidated_ = false;
  GenerateColumnSetIntervalTree();
}

bool LayoutFlowThread::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags visual_rect_flags) const {
  NOT_DESTROYED();
  // A flow thread should never be an invalidation container.
  DCHECK_NE(ancestor, this);
  transform_state.Flatten();
  gfx::RectF bounding_box = transform_state.LastPlanarQuad().BoundingBox();
  PhysicalRect rect(LayoutUnit(bounding_box.x()), LayoutUnit(bounding_box.y()),
                    LayoutUnit(bounding_box.width()),
                    LayoutUnit(bounding_box.height()));
  rect = FragmentsBoundingBox(rect);
  transform_state.SetQuad(gfx::QuadF(gfx::RectF(rect)));
  return LayoutBlockFlow::MapToVisualRectInAncestorSpaceInternal(
      ancestor, transform_state, visual_rect_flags);
}

PaintLayerType LayoutFlowThread::LayerTypeRequired() const {
  NOT_DESTROYED();
  return kNoPaintLayer;
}

void LayoutFlowThread::QuadsInAncestorForDescendant(
    const LayoutBox& descendant,
    Vector<gfx::QuadF>& quads,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) {
  NOT_DESTROYED();
  PhysicalOffset offset_from_flow_thread;
  for (const LayoutObject* object = &descendant; object != this;) {
    // Based on current intended usage, it should be impossible to end up in a
    // situation where the ancestor is inside the same fragmentation context as
    // the descendant. If needed, though, it should be fairly trivial to add
    // support for it.
    DCHECK(object != ancestor);

    const LayoutObject* container = object->Container();
    offset_from_flow_thread += object->OffsetFromContainer(container);
    object = container;
  }
  PhysicalRect bounding_rect_in_flow_thread(offset_from_flow_thread,
                                            descendant.Size());
  // Set up a fragments relative to the descendant, in the flow thread
  // coordinate space, and convert each of them, individually, to absolute
  // coordinates.
  for (FragmentainerIterator iterator(*this, bounding_rect_in_flow_thread);
       !iterator.AtEnd(); iterator.Advance()) {
    PhysicalRect fragment = bounding_rect_in_flow_thread;
    // We use InclusiveIntersect() because Intersect() would reset the
    // coordinates for zero-height objects.
    PhysicalRect clip_rect = iterator.ClipRectInFlowThread();
    fragment.InclusiveIntersect(clip_rect);
    fragment.offset -= offset_from_flow_thread;
    quads.push_back(
        descendant.LocalRectToAncestorQuad(fragment, ancestor, mode));
  }
}

void LayoutFlowThread::AddOutlineRects(
    OutlineRectCollector& collector,
    OutlineInfo* info,
    const PhysicalOffset& additional_offset,
    OutlineType include_block_overflows) const {
  NOT_DESTROYED();
  Vector<PhysicalRect> rects_in_flowthread;
  UnionOutlineRectCollector flow_collector;
  LayoutBlockFlow::AddOutlineRects(flow_collector, info, additional_offset,
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
  collector.AddRect(FragmentsBoundingBox(flow_collector.Rect()));
}

void LayoutFlowThread::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  // NGBoxFragmentPainter traverses a physical fragment tree, and doesn't call
  // Paint() for LayoutFlowThread.
  NOTREACHED();
}

bool LayoutFlowThread::NodeAtPoint(HitTestResult& result,
                                   const HitTestLocation& hit_test_location,
                                   const PhysicalOffset& accumulated_offset,
                                   HitTestPhase phase) {
  NOT_DESTROYED();
  if (phase == HitTestPhase::kSelfBlockBackground)
    return false;
  return LayoutBlockFlow::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, phase);
}

RecalcScrollableOverflowResult LayoutFlowThread::RecalcScrollableOverflow() {
  NOT_DESTROYED();
  // RecalcScrollableOverflow() traverses a physical fragment tree. So it's not
  // called for LayoutFlowThread, which has no physical fragments.
  NOTREACHED();
}

void LayoutFlowThread::GenerateColumnSetIntervalTree() {
  NOT_DESTROYED();
  // FIXME: Optimize not to clear the interval all the time. This implies
  // manually managing the tree nodes lifecycle.
  multi_column_set_interval_tree_.Clear();
  multi_column_set_interval_tree_.InitIfNeeded();
  for (const auto& column_set : multi_column_set_list_)
    multi_column_set_interval_tree_.Add(
        MultiColumnSetIntervalTree::CreateInterval(
            column_set->LogicalTopInFlowThread(),
            column_set->LogicalBottomInFlowThread(), column_set));
}

PhysicalRect LayoutFlowThread::FragmentsBoundingBox(
    const PhysicalRect& layer_bounding_box) const {
  NOT_DESTROYED();
  DCHECK(!column_sets_invalidated_);

  PhysicalRect result;
  for (const auto& column_set : multi_column_set_list_)
    result.Unite(column_set->FragmentsBoundingBox(layer_bounding_box));

  return result;
}

void LayoutFlowThread::MultiColumnSetSearchAdapter::CollectIfNeeded(
    const MultiColumnSetInterval& interval) {
  if (result_)
    return;
  if (interval.Low() <= offset_ && interval.High() > offset_)
    result_ = interval.Data();
}

}  // namespace blink
