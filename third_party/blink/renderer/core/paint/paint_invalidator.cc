// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_invalidator.h"

#include <optional>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

void PaintInvalidator::UpdatePaintingLayer(const LayoutObject& object,
                                           PaintInvalidatorContext& context) {
  if (object.HasLayer() &&
      To<LayoutBoxModelObject>(object).HasSelfPaintingLayer()) {
    context.painting_layer = To<LayoutBoxModelObject>(object).Layer();
  } else if (object.IsInlineRubyText()) {
    // Physical fragments and fragment items for ruby-text boxes are not
    // managed by inline parents.
    context.painting_layer = object.PaintingLayer();
  }

  if (object.IsFloating()) {
    context.painting_layer->SetNeedsPaintPhaseFloat();
  }

  if (!context.painting_layer->NeedsPaintPhaseDescendantOutlines() &&
      ((object != context.painting_layer->GetLayoutObject() &&
        object.StyleRef().HasOutline()))) {
    context.painting_layer->SetNeedsPaintPhaseDescendantOutlines();
  }
}

void PaintInvalidator::UpdateFromTreeBuilderContext(
    const PaintPropertyTreeBuilderFragmentContext& tree_builder_context,
    PaintInvalidatorContext& context) {
  DCHECK_EQ(tree_builder_context.current.paint_offset,
            context.fragment_data->PaintOffset());

  // For performance, we ignore subpixel movement of composited layers for paint
  // invalidation. This will result in imperfect pixel-snapped painting.
  // See crbug.com/833083 for details.
  if (!RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      tree_builder_context.current
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
  if (!object.ShouldCheckLayoutForPaintInvalidation())
    return;

  if (tree_builder_context.this_or_ancestor_opacity_is_zero ||
      context.inside_opaque_layout_shift_root) {
    object.GetMutableForPainting().SetShouldSkipNextLayoutShiftTracking(true);
    return;
  }

  auto& layout_shift_tracker = object.GetFrameView()->GetLayoutShiftTracker();
  if (!layout_shift_tracker.NeedsToTrack(object)) {
    object.GetMutableForPainting().SetShouldSkipNextLayoutShiftTracking(true);
    return;
  }

  PropertyTreeStateOrAlias property_tree_state(
      *tree_builder_context.current.transform,
      *tree_builder_context.current.clip, *tree_builder_context.current_effect);

  // Adjust old_paint_offset so that LayoutShiftTracker will see the change of
  // offset caused by change of paint offset translations and scroll offset
  // below the layout shift root. For more details, see
  // renderer/core/layout/layout-shift-tracker-old-paint-offset.md.
  PhysicalOffset adjusted_old_paint_offset =
      context.old_paint_offset -
      tree_builder_context.current
          .additional_offset_to_layout_shift_root_delta -
      PhysicalOffset::FromVector2dFRound(
          tree_builder_context.translation_2d_to_layout_shift_root_delta +
          tree_builder_context.current
              .scroll_offset_to_layout_shift_root_delta);
  PhysicalOffset new_paint_offset = tree_builder_context.current.paint_offset;

  if (object.IsText()) {
    const auto& text = To<LayoutText>(object);
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
        adjusted_old_paint_offset,
        tree_builder_context.translation_2d_to_layout_shift_root_delta,
        tree_builder_context.current.scroll_offset_to_layout_shift_root_delta,
        tree_builder_context.current.pending_scroll_anchor_adjustment,
        new_paint_offset, logical_height);
    return;
  }

  DCHECK(object.IsBox());
  const auto& box = To<LayoutBox>(object);

  PhysicalRect new_rect = box.VisualOverflowRectAllowingUnset();
  new_rect.Move(new_paint_offset);
  PhysicalRect old_rect = box.PreviousVisualOverflowRect();
  old_rect.Move(adjusted_old_paint_offset);

  // TODO(crbug.com/1178618): We may want to do better than this. For now, just
  // don't report anything inside multicol containers.
  const auto* block_flow = DynamicTo<LayoutBlockFlow>(&box);
  if (block_flow && block_flow->IsFragmentationContextRoot() &&
      block_flow->IsLayoutNGObject())
    context.inside_opaque_layout_shift_root = true;

  bool should_create_containing_block_scope =
      // TODO(crbug.com/1178618): Support multiple-fragments.
      context.fragment_data == &box.FirstFragment() && block_flow &&
      block_flow->ChildrenInline() && block_flow->FirstChild();
  if (should_create_containing_block_scope) {
    // For layout shift tracking of contained LayoutTexts.
    context.containing_block_scope_.emplace(box.PreviousSize(), box.Size(),
                                            old_rect, new_rect);
  }

  bool should_report_layout_shift = [&]() -> bool {
    if (box.ShouldSkipNextLayoutShiftTracking()) {
      box.GetMutableForPainting().SetShouldSkipNextLayoutShiftTracking(false);
      return false;
    }
    // If the layout shift root has changed, LayoutShiftTracker can't use the
    // current paint property tree to map the old rect.
    if (tree_builder_context.current.layout_shift_root_changed)
      return false;
    if (new_rect.IsEmpty() || old_rect.IsEmpty())
      return false;
    // Track self-painting layers separately because their ancestors'
    // PhysicalVisualOverflowRect may not cover them.
    if (object.HasLayer() &&
        To<LayoutBoxModelObject>(object).HasSelfPaintingLayer())
      return true;
    // Always track if the parent doesn't need to track (e.g. it has visibility:
    // hidden), while this object needs (e.g. it has visibility: visible).
    // This also includes non-anonymous child with an anonymous parent.
    if (object.Parent()->ShouldSkipNextLayoutShiftTracking())
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
           new_paint_offset - context.old_paint_offset;
  }();
  if (should_report_layout_shift) {
    layout_shift_tracker.NotifyBoxPrePaint(
        box, property_tree_state, old_rect, new_rect, adjusted_old_paint_offset,
        tree_builder_context.translation_2d_to_layout_shift_root_delta,
        tree_builder_context.current.scroll_offset_to_layout_shift_root_delta,
        tree_builder_context.current.pending_scroll_anchor_adjustment,
        new_paint_offset);
  }
}

bool PaintInvalidator::InvalidatePaint(
    const LayoutObject& object,
    const PrePaintInfo* pre_paint_info,
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

  UpdatePaintingLayer(object, context);

  // Assert that the container state in the invalidation context is consistent
  // with what the LayoutObject tree says. We cannot do this if we're fragment-
  // traversing an "orphaned" object (an object that has a fragment inside a
  // fragmentainer, even though not all its ancestor objects have it; this may
  // happen to OOFs, and also to floats, if they are inside a non-atomic
  // inline). In such cases we'll just have to live with the inconsitency, which
  // means that we'll lose any paint effects from such "missing" ancestors.
  DCHECK_EQ(context.painting_layer, object.PaintingLayer()) << object;

  if (AXObjectCache* cache = object.GetDocument().ExistingAXObjectCache())
    cache->InvalidateBoundingBox(&object);

  if (!object.ShouldCheckForPaintInvalidation() && !context.NeedsSubtreeWalk())
    return false;

  if (object.SubtreeShouldDoFullPaintInvalidation()) {
    context.subtree_flags |=
        PaintInvalidatorContext::kSubtreeFullInvalidation |
        PaintInvalidatorContext::kSubtreeFullInvalidationForStackedContents;
  }

  if (object.SubtreeShouldCheckForPaintInvalidation()) {
    context.subtree_flags |=
        PaintInvalidatorContext::kSubtreeInvalidationChecking;
  }

  if (pre_paint_info) {
    context.fragment_data = pre_paint_info->fragment_data;
    CHECK(context.fragment_data);
  } else {
    context.fragment_data = &object.GetMutableForPainting().FirstFragment();
  }

  if (tree_builder_context) {
    const auto& fragment_tree_builder_context =
        tree_builder_context->fragment_context;
    UpdateFromTreeBuilderContext(fragment_tree_builder_context, context);
    UpdateLayoutShiftTracking(object, fragment_tree_builder_context, context);
  } else {
    context.old_paint_offset = context.fragment_data->PaintOffset();
  }

  object.InvalidatePaint(context);

  auto reason = static_cast<const DisplayItemClient&>(object)
                    .GetPaintInvalidationReason();
  if (object.ShouldDelayFullPaintInvalidation() &&
      (!IsFullPaintInvalidationReason(reason) ||
       // Delay invalidation if the client has never been painted.
       reason == PaintInvalidationReason::kJustCreated))
    pending_delayed_paint_invalidations_.push_back(&object);

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled() &&
      object.ShouldCheckLayoutForPaintInvalidation() &&
      (IsLayoutPaintInvalidationReason(reason) ||
       reason == PaintInvalidationReason::kJustCreated ||
       // We don't invalidate paint of visibility:hidden objects, but observe
       // intersection for them.
       object.StyleRef().UsedVisibility() != EVisibility::kVisible)) {
    object.GetFrameView()->SetIntersectionObservationState(
        LocalFrameView::kDesired);
  }

  return reason != PaintInvalidationReason::kNone;
}

void PaintInvalidator::ProcessPendingDelayedPaintInvalidations() {
  for (const auto& target : pending_delayed_paint_invalidations_)
    target->GetMutableForPainting().SetShouldDelayFullPaintInvalidation();
}

}  // namespace blink
