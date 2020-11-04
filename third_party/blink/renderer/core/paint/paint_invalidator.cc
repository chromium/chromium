// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_invalidator.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_child_iterator.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

const PaintInvalidatorContext*
PaintInvalidatorContext::ParentContextAccessor::ParentContext() const {
  return tree_walk_ ? &tree_walk_->ContextAt(parent_context_index_)
                           .paint_invalidator_context
                    : nullptr;
}

void PaintInvalidator::UpdatePaintingLayer(const LayoutObject& object,
                                           PaintInvalidatorContext& context,
                                           bool is_ng_painting) {
  if (object.HasLayer() &&
      ToLayoutBoxModelObject(object).HasSelfPaintingLayer()) {
    context.painting_layer = ToLayoutBoxModelObject(object).Layer();
  } else if (!is_ng_painting &&
             (object.IsColumnSpanAll() ||
              object.IsFloatingWithNonContainingBlockParent())) {
    // See |LayoutObject::PaintingLayer| for the special-cases of floating under
    // inline and multicolumn.
    // Post LayoutNG the |LayoutObject::IsFloatingWithNonContainingBlockParent|
    // check can be removed as floats will be painted by the correct layer.
    context.painting_layer = object.PaintingLayer();
  }

  auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(object);
  if (layout_block_flow && !object.IsLayoutNGBlockFlow() &&
      layout_block_flow->ContainsFloats())
    context.painting_layer->SetNeedsPaintPhaseFloat();

  if (object.IsFloating() &&
      (object.IsInLayoutNGInlineFormattingContext() ||
       IsLayoutNGContainingBlock(object.ContainingBlock())))
    context.painting_layer->SetNeedsPaintPhaseFloat();

  if (object != context.painting_layer->GetLayoutObject() &&
      object.StyleRef().HasOutline())
    context.painting_layer->SetNeedsPaintPhaseDescendantOutlines();
}

void PaintInvalidator::UpdateDirectlyCompositedContainer(
    const LayoutObject& object,
    PaintInvalidatorContext& context,
    bool is_ng_painting) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  if (object.CanBeCompositedForDirectReasons()) {
    context.directly_composited_container = ToLayoutBoxModelObject(&object);
    if (object.IsStackingContext() || object.IsSVGRoot())
      context.directly_composited_container_for_stacked_contents =
          ToLayoutBoxModelObject(&object);
  } else if (IsA<LayoutView>(object)) {
    // paint_invalidation_container_for_stacked_contents is only for stacked
    // descendants in its own frame, because it doesn't establish stacking
    // context for stacked contents in sub-frames.
    // Contents stacked in the root stacking context in this frame should use
    // this frame's PaintInvalidationContainer.
    context.directly_composited_container_for_stacked_contents =
        context.directly_composited_container =
            &object.DirectlyCompositableContainer();
  } else if (!is_ng_painting &&
             (object.IsColumnSpanAll() ||
              object.IsFloatingWithNonContainingBlockParent())) {
    // In these cases, the object may belong to an ancestor of the current
    // paint invalidation container, in paint order.
    // Post LayoutNG the |LayoutObject::IsFloatingWithNonContainingBlockParent|
    // check can be removed as floats will be painted by the correct layer.
    context.directly_composited_container =
        &object.DirectlyCompositableContainer();
  } else if (object.IsStacked() &&
             // This is to exclude some objects (e.g. LayoutText) inheriting
             // stacked style from parent but aren't actually stacked.
             object.HasLayer() &&
             !ToLayoutBoxModelObject(object)
                  .Layer()
                  ->IsReplacedNormalFlowStacking() &&
             context.directly_composited_container !=
                 context.directly_composited_container_for_stacked_contents) {
    // The current object is stacked, so we should use
    // directly_composited_container_for_stacked_contents as its paint
    // invalidation container on which the current object is painted.
    context.directly_composited_container =
        context.directly_composited_container_for_stacked_contents;
    if (context.subtree_flags &
        PaintInvalidatorContext::kSubtreeFullInvalidationForStackedContents) {
      context.subtree_flags |=
          PaintInvalidatorContext::kSubtreeFullInvalidation;
    }
  }

  if (object == context.directly_composited_container) {
    // When we hit a new directly composited container, we don't need to
    // continue forcing a check for paint invalidation, since we're
    // descending into a different invalidation container. (For instance if
    // our parents were moved, the entire container will just move.)
    if (object != context.directly_composited_container_for_stacked_contents) {
      // However, we need to keep kSubtreeFullInvalidationForStackedContents
      // if the current object isn't the direct composited container of stacked
      // contents.
      context.subtree_flags &=
          PaintInvalidatorContext::kSubtreeFullInvalidationForStackedContents;
    } else {
      context.subtree_flags = 0;
    }
  }

  DCHECK(context.directly_composited_container ==
         object.DirectlyCompositableContainer());
  DCHECK(context.painting_layer == object.PaintingLayer());
}

void PaintInvalidator::UpdateFromTreeBuilderContext(
    const PaintPropertyTreeBuilderFragmentContext& tree_builder_context,
    PaintInvalidatorContext& context) {
  DCHECK_EQ(tree_builder_context.current.paint_offset,
            context.fragment_data->PaintOffset());

  // For performance, we ignore subpixel movement of composited layers for paint
  // invalidation. This will result in imperfect pixel-snapped painting.
  // See crbug.com/833083 for details.
  if (tree_builder_context.current
          .directly_composited_container_paint_offset_subpixel_delta ==
      tree_builder_context.current.paint_offset -
          tree_builder_context.old_paint_offset) {
    context.old_paint_offset = tree_builder_context.current.paint_offset;
  } else {
    context.old_paint_offset = tree_builder_context.old_paint_offset;
  }

  context.transform_ = tree_builder_context.current.transform;
}

void PaintInvalidator::UpdateLayoutShiftTracking(
    const LayoutObject& object,
    const PaintPropertyTreeBuilderFragmentContext& tree_builder_context,
    PaintInvalidatorContext& context) {
  if (!object.ShouldCheckGeometryForPaintInvalidation())
    return;

  auto& layout_shift_tracker = object.GetFrameView()->GetLayoutShiftTracker();
  if (!layout_shift_tracker.NeedsToTrack(object))
    return;

  PropertyTreeStateOrAlias property_tree_state(
      *tree_builder_context.current.transform,
      *tree_builder_context.current.clip, *tree_builder_context.current_effect);

  if (object.IsText()) {
    const auto& text = ToLayoutText(object);
    LogicalOffset new_starting_point;
    LayoutUnit logical_height;
    text.LogicalStartingPointAndHeight(new_starting_point, logical_height);
    LogicalOffset old_starting_point = text.PreviousLogicalStartingPoint();
    if (new_starting_point == old_starting_point)
      return;
    text.SetPreviousLogicalStartingPoint(new_starting_point);
    if (old_starting_point == LayoutText::UninitializedLogicalStartingPoint())
      return;
    // If the layout shift root has changed, LayoutShiftTracker can't use the
    // current paint property tree to map the old rect.
    if (tree_builder_context.current.layout_shift_root_changed)
      return;

    layout_shift_tracker.NotifyTextPrePaint(
        text, property_tree_state, old_starting_point, new_starting_point,
        // Similar to the adjustment of old_paint_offset for LayoutBox.
        context.old_paint_offset -
            tree_builder_context.current
                .additional_offset_to_layout_shift_root_delta,
        tree_builder_context.current.paint_offset, logical_height);
    return;
  }

  DCHECK(object.IsBox());
  const auto& box = ToLayoutBox(object);

  PhysicalRect new_rect = box.PhysicalVisualOverflowRect();
  PhysicalRect old_rect = box.PreviousPhysicalVisualOverflowRect();
  bool should_report_layout_shift = [&]() -> bool {
    // If the layout shift root has changed, LayoutShiftTracker can't use the
    // current paint property tree to map the old rect.
    if (tree_builder_context.current.layout_shift_root_changed)
      return false;
    if (new_rect.IsEmpty() || old_rect.IsEmpty())
      return false;
    // Track self-painting layers separately because their ancestors'
    // PhysicalVisualOverflowRect may not cover them.
    if (object.HasLayer() &&
        ToLayoutBoxModelObject(object).HasSelfPaintingLayer())
      return true;
    // We don't report shift for anonymous objects but report for the children.
    if (object.Parent()->IsAnonymous())
      return true;
    // Report if the parent is in a different transform space.
    const auto* parent_context = context.ParentContext();
    if (!parent_context || !parent_context->transform_ ||
        parent_context->transform_ != tree_builder_context.current.transform)
      return true;
    // Report if this object has local movement (i.e. delta of paint offset is
    // different from that of the parent).
    return parent_context->fragment_data->PaintOffset() -
               parent_context->old_paint_offset !=
           tree_builder_context.current.paint_offset - context.old_paint_offset;
  }();

  bool should_create_containing_block_scope =
      // TODO(crbug.com/1104064): Support multiple-fragments when switching to
      // LayoutNGFragmentTraversal.
      context.fragment_data == &box.FirstFragment() &&
      box.IsLayoutBlockFlow() && box.ChildrenInline() && box.SlowFirstChild();
  if (!should_report_layout_shift && !should_create_containing_block_scope)
    return;

  new_rect.Move(tree_builder_context.current.paint_offset);
  old_rect.Move(context.old_paint_offset);
  // Adjust old_visual_rect so that LayoutShiftTracker can see the change of
  // offset caused by change of transforms below the 2d translation root.
  old_rect.Move(-tree_builder_context.current
                     .additional_offset_to_layout_shift_root_delta);

  if (should_create_containing_block_scope) {
    // For layout shift tracking of contained LayoutTexts.
    context.containing_block_scope_ =
        std::make_unique<LayoutShiftTracker::ContainingBlockScope>(
            PhysicalSizeToBeNoop(box.PreviousSize()),
            PhysicalSizeToBeNoop(box.Size()), old_rect, new_rect);
    if (!should_report_layout_shift)
      return;
  }

  // Adjust old_paint_offset similarly.
  PhysicalOffset old_paint_offset =
      context.old_paint_offset -
      tree_builder_context.current.additional_offset_to_layout_shift_root_delta;
  layout_shift_tracker.NotifyBoxPrePaint(
      box, property_tree_state, old_rect, new_rect, old_paint_offset,
      tree_builder_context.current.paint_offset);
}

bool PaintInvalidator::InvalidatePaint(
    const LayoutObject& object,
    const NGPrePaintInfo* pre_paint_info,
    const PaintPropertyTreeBuilderContext* tree_builder_context,
    PaintInvalidatorContext& context) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
               "PaintInvalidator::InvalidatePaint()", "object",
               object.DebugName().Ascii());

  if (object.IsSVGHiddenContainer() || object.IsLayoutTableCol())
    context.subtree_flags |= PaintInvalidatorContext::kSubtreeNoInvalidation;

  if (context.subtree_flags & PaintInvalidatorContext::kSubtreeNoInvalidation)
    return false;

  object.GetMutableForPainting().EnsureIsReadyForPaintInvalidation();

  UpdatePaintingLayer(object, context, /* is_ng_painting */ !!pre_paint_info);
  UpdateDirectlyCompositedContainer(object, context,
                                    /* is_ng_painting */ !!pre_paint_info);

  if (!object.ShouldCheckForPaintInvalidation() && !context.NeedsSubtreeWalk())
    return false;

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      object.GetFrame()->GetPage()->GetLinkHighlight().NeedsHighlightEffect(
          object)) {
    // We need to recollect the foreign layers for link highlight when the
    // geometry of the highlights may change. CompositeAfterPaint doesn't
    // need this because we collect foreign layers during
    // LocalFrameView::PaintTree() which is not controlled by the flag.
    object.GetFrameView()->SetForeignLayerListNeedsUpdate();
  }

  if (pre_paint_info) {
    FragmentData& fragment_data = pre_paint_info->fragment_data;
    context.fragment_data = &fragment_data;

    if (tree_builder_context) {
      DCHECK_EQ(tree_builder_context->fragments.size(), 1u);
      const auto& fragment_tree_builder_context =
          tree_builder_context->fragments[0];
      UpdateFromTreeBuilderContext(fragment_tree_builder_context, context);
      UpdateLayoutShiftTracking(object, fragment_tree_builder_context, context);
    } else {
      context.old_paint_offset = fragment_data.PaintOffset();
    }

    object.InvalidatePaint(context);
  } else {
    unsigned tree_builder_index = 0;

    for (auto* fragment_data = &object.GetMutableForPainting().FirstFragment();
         fragment_data;
         fragment_data = fragment_data->NextFragment(), tree_builder_index++) {
      context.fragment_data = fragment_data;

      DCHECK(!tree_builder_context ||
             tree_builder_index < tree_builder_context->fragments.size());

      if (tree_builder_context) {
        const auto& fragment_tree_builder_context =
            tree_builder_context->fragments[tree_builder_index];
        UpdateFromTreeBuilderContext(fragment_tree_builder_context, context);
        UpdateLayoutShiftTracking(object, fragment_tree_builder_context,
                                  context);
      } else {
        context.old_paint_offset = fragment_data->PaintOffset();
      }

      object.InvalidatePaint(context);
    }
  }

  auto reason = static_cast<const DisplayItemClient&>(object)
                    .GetPaintInvalidationReason();
  if (object.ShouldDelayFullPaintInvalidation() &&
      (!IsFullPaintInvalidationReason(reason) ||
       // Delay invalidation if the client has never been painted.
       reason == PaintInvalidationReason::kJustCreated))
    pending_delayed_paint_invalidations_.push_back(&object);

  if (object.SubtreeShouldDoFullPaintInvalidation()) {
    context.subtree_flags |=
        PaintInvalidatorContext::kSubtreeFullInvalidation |
        PaintInvalidatorContext::kSubtreeFullInvalidationForStackedContents;
  }

  if (object.SubtreeShouldCheckForPaintInvalidation()) {
    context.subtree_flags |=
        PaintInvalidatorContext::kSubtreeInvalidationChecking;
  }

  if (UNLIKELY(object.ContainsInlineWithOutlineAndContinuation())) {
    // Force subtree invalidation checking to ensure invalidation of focus rings
    // when continuation's geometry changes.
    context.subtree_flags |=
        PaintInvalidatorContext::kSubtreeInvalidationChecking;
  }

  if (AXObjectCache* cache = object.GetDocument().ExistingAXObjectCache())
    cache->InvalidateBoundingBox(&object);

  return reason != PaintInvalidationReason::kNone;
}

void PaintInvalidator::ProcessPendingDelayedPaintInvalidations() {
  for (auto* target : pending_delayed_paint_invalidations_)
    target->GetMutableForPainting().SetShouldDelayFullPaintInvalidation();
}

}  // namespace blink
