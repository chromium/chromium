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

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

LayoutFlowThread::LayoutFlowThread()
    : LayoutBlockFlow(nullptr), column_sets_invalidated_(false) {
  DCHECK(!RuntimeEnabledFeatures::FlowThreadLessEnabled());
}

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
  DCHECK(descendant.IsInsideMulticol());
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

  return LayoutBlockFlow::MapToVisualRectInAncestorSpaceInternal(
      ancestor, transform_state, visual_rect_flags);
}

PaintLayerType LayoutFlowThread::LayerTypeRequired() const {
  NOT_DESTROYED();
  return kNoPaintLayer;
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

void LayoutFlowThread::MultiColumnSetSearchAdapter::CollectIfNeeded(
    const MultiColumnSetInterval& interval) {
  if (result_)
    return;
  if (interval.Low() <= offset_ && interval.High() > offset_)
    result_ = interval.Data();
}

}  // namespace blink
