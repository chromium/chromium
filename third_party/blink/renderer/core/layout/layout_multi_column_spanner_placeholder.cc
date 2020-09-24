// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"

#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"

namespace blink {

static void CopyMarginProperties(ComputedStyle& placeholder_style,
                                 const ComputedStyle& spanner_style) {
  // We really only need the block direction margins, but there are no setters
  // for that in ComputedStyle. Just copy all margin sides. The inline ones
  // don't matter anyway.
  placeholder_style.SetMarginLeft(spanner_style.MarginLeft());
  placeholder_style.SetMarginRight(spanner_style.MarginRight());
  placeholder_style.SetMarginTop(spanner_style.MarginTop());
  placeholder_style.SetMarginBottom(spanner_style.MarginBottom());
}

LayoutMultiColumnSpannerPlaceholder*
LayoutMultiColumnSpannerPlaceholder::CreateAnonymous(
    const ComputedStyle& parent_style,
    LayoutBox& layout_object_in_flow_thread) {
  LayoutMultiColumnSpannerPlaceholder* new_spanner =
      new LayoutMultiColumnSpannerPlaceholder(&layout_object_in_flow_thread);
  Document& document = layout_object_in_flow_thread.GetDocument();
  new_spanner->SetDocumentForAnonymous(&document);
  new_spanner->UpdateProperties(parent_style);
  return new_spanner;
}

LayoutMultiColumnSpannerPlaceholder::LayoutMultiColumnSpannerPlaceholder(
    LayoutBox* layout_object_in_flow_thread)
    : LayoutBox(nullptr),
      layout_object_in_flow_thread_(layout_object_in_flow_thread) {}

void LayoutMultiColumnSpannerPlaceholder::
    LayoutObjectInFlowThreadStyleDidChange(const ComputedStyle* old_style) {
  LayoutBox* object_in_flow_thread = layout_object_in_flow_thread_;
  if (FlowThread()->RemoveSpannerPlaceholderIfNoLongerValid(
          object_in_flow_thread)) {
    // No longer a valid spanner, due to style changes. |this| is now dead.
    if (object_in_flow_thread->StyleRef().HasOutOfFlowPosition() &&
        !old_style->HasOutOfFlowPosition()) {
      // We went from being a spanner to being out-of-flow positioned. When an
      // object becomes out-of-flow positioned, we need to lay out its parent,
      // since that's where the now-out-of-flow object gets added to the right
      // containing block for out-of-flow positioned objects. Since neither a
      // spanner nor an out-of-flow object is guaranteed to have this parent in
      // its containing block chain, we need to mark it here, or we risk that
      // the object isn't laid out.
      object_in_flow_thread->Parent()->SetNeedsLayout(
          layout_invalidation_reason::kColumnsChanged);
    }
    return;
  }
  UpdateProperties(Parent()->StyleRef());
}

void LayoutMultiColumnSpannerPlaceholder::UpdateProperties(
    const ComputedStyle& parent_style) {
  scoped_refptr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(parent_style,
                                                     EDisplay::kBlock);
  CopyMarginProperties(*new_style, layout_object_in_flow_thread_->StyleRef());
  SetStyle(std::move(new_style));
}

void LayoutMultiColumnSpannerPlaceholder::InsertedIntoTree() {
  LayoutBox::InsertedIntoTree();
  // The object may previously have been laid out as a non-spanner, but since
  // it's a spanner now, it needs to be relaid out.
  layout_object_in_flow_thread_->SetNeedsLayoutAndIntrinsicWidthsRecalc(
      layout_invalidation_reason::kColumnsChanged);
}

void LayoutMultiColumnSpannerPlaceholder::WillBeRemovedFromTree() {
  if (layout_object_in_flow_thread_) {
    LayoutBox* ex_spanner = layout_object_in_flow_thread_;
    layout_object_in_flow_thread_->ClearSpannerPlaceholder();
    // Even if the placeholder is going away, the object in the flow thread
    // might live on. Since it's not a spanner anymore, it needs to be relaid
    // out.
    ex_spanner->SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kColumnsChanged);
  }
  LayoutBox::WillBeRemovedFromTree();
}

bool LayoutMultiColumnSpannerPlaceholder::NeedsPreferredWidthsRecalculation()
    const {
  return layout_object_in_flow_thread_->NeedsPreferredWidthsRecalculation();
}

void LayoutMultiColumnSpannerPlaceholder::RecalcVisualOverflow() {
  LayoutBox::RecalcVisualOverflow();
  ClearVisualOverflow();
  AddContentsVisualOverflow(
      layout_object_in_flow_thread_->VisualOverflowRect());
}

MinMaxSizes LayoutMultiColumnSpannerPlaceholder::PreferredLogicalWidths()
    const {
  // There should be no contribution from a spanner if the multicol container is
  // size-contained. Normally we'd stop at the object that has contain:size
  // applied, but for multicol, we descend into the children, in order to get
  // the flow thread to calculate the correct preferred width (to honor
  // column-count, column-width and column-gap). Since spanner placeholders are
  // siblings of the flow thread, we need this check.
  // TODO(crbug.com/953919): What should we return for display-locked content?
  if (MultiColumnBlockFlow()->ShouldApplySizeContainment())
    return MinMaxSizes();
  return layout_object_in_flow_thread_->PreferredLogicalWidths();
}

void LayoutMultiColumnSpannerPlaceholder::UpdateLayout() {
  DCHECK(NeedsLayout());

  // The placeholder, like any other block level object, has its logical top
  // calculated and set before layout. Copy this to the actual column-span:all
  // object before laying it out, so that it gets paginated correctly, in case
  // we have an enclosing fragmentation context.
  if (layout_object_in_flow_thread_->LogicalTop() != LogicalTop()) {
    layout_object_in_flow_thread_->SetLogicalTop(LogicalTop());
    if (FlowThread()->EnclosingFragmentationContext())
      layout_object_in_flow_thread_->SetChildNeedsLayout(kMarkOnlyThis);
  }

  // Lay out the actual column-span:all element.
  layout_object_in_flow_thread_->LayoutIfNeeded();

  // The spanner has now been laid out, so its height is known. Time to update
  // the placeholder's height as well, so that we take up the correct amount of
  // space in the multicol container.
  UpdateLogicalHeight();

  // Take the overflow from the spanner, so that it gets propagated to the
  // multicol container and beyond.
  ClearLayoutOverflow();
  AddLayoutOverflow(layout_object_in_flow_thread_->LayoutOverflowRect());

  ClearNeedsLayout();
}

void LayoutMultiColumnSpannerPlaceholder::ComputeLogicalHeight(
    LayoutUnit,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  computed_values.extent_ = layout_object_in_flow_thread_->LogicalHeight();
  computed_values.position_ = logical_top;
  computed_values.margins_.before_ = MarginBefore();
  computed_values.margins_.after_ = MarginAfter();
}

void LayoutMultiColumnSpannerPlaceholder::Paint(
    const PaintInfo& paint_info) const {
  if (!layout_object_in_flow_thread_->HasSelfPaintingLayer())
    layout_object_in_flow_thread_->Paint(paint_info);
}

bool LayoutMultiColumnSpannerPlaceholder::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestAction action) {
  return !layout_object_in_flow_thread_->HasSelfPaintingLayer() &&
         layout_object_in_flow_thread_->NodeAtPoint(result, hit_test_location,
                                                    accumulated_offset, action);
}

}  // namespace blink
