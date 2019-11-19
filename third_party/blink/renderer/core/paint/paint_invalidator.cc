// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_invalidator.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/find_paint_offset_and_visual_rect_needing_update.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

// If needed, exclude composited layer's subpixel accumulation to avoid full
// layer raster invalidations during animation with subpixels.
// See crbug.com/833083 for details.
bool PaintInvalidatorContext::ShouldExcludeCompositedLayerSubpixelAccumulation(
    const LayoutObject& object) const {
  // TODO(wangxianzhu): How to handle sub-pixel location animation for CAP?
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return false;

  // One of the following conditions happened in crbug.com/837226.
  if (!paint_invalidation_container ||
      !paint_invalidation_container->FirstFragment()
           .HasLocalBorderBoxProperties() ||
      !tree_builder_context_)
    return false;

  if (!(paint_invalidation_container->Layer()->GetCompositingReasons() &
        CompositingReason::kComboAllDirectReasons))
    return false;

  if (object != paint_invalidation_container &&
      &paint_invalidation_container->FirstFragment().PostScrollTranslation() !=
          tree_builder_context_->current.transform) {
    // Subpixel accumulation doesn't propagate through non-translation
    // transforms. Also skip all transforms, to avoid the runtime cost of
    // verifying whether the transform is a translation.
    return false;
  }

  // Will exclude the subpixel accumulation so that the paint invalidator won't
  // see changed visual rects during composited animation with subpixels, to
  // avoid full layer invalidation. The subpixel accumulation will be added
  // back in ChunkToLayerMapper::AdjustVisualRectBySubpixelOffset(). Should
  // make sure the code is synced.
  // TODO(wangxianzhu): Avoid exposing subpixel accumulation to platform code.
  return true;
}

IntRect PaintInvalidatorContext::MapLocalRectToVisualRect(
    const LayoutObject& object,
    const PhysicalRect& local_rect) const {
  DCHECK(NeedsVisualRectUpdate(object));

  if (local_rect.IsEmpty())
    return IntRect();

  DCHECK(!object.IsSVGChild() ||
         // This function applies to SVG children derived from non-SVG layout
         // objects, for carets, selections, etc.
         object.IsBoxModelObject() || object.IsText());

  // Unite visual rect with clip path bounding rect.
  // It is because the clip path display items are owned by the layout object
  // who has the clip path, and uses its visual rect as bounding rect too.
  // Usually it is done at layout object level and included as a part of
  // local visual overflow, but clip-path can be a reference to SVG, and we
  // have to wait until pre-paint to ensure clean layout.
  PhysicalRect rect = local_rect;
  if (base::Optional<FloatRect> clip_path_bounding_box =
          ClipPathClipper::LocalClipPathBoundingBox(object))
    rect.Unite(PhysicalRect(EnclosingIntRect(*clip_path_bounding_box)));

  rect.Move(fragment_data->PaintOffset());
  if (ShouldExcludeCompositedLayerSubpixelAccumulation(object))
    rect.Move(-paint_invalidation_container->Layer()->SubpixelAccumulation());
  // Use EnclosingIntRect to ensure the final visual rect will cover the rect
  // in source coordinates no matter if the painting will snap to pixels.
  return EnclosingIntRect(rect);
}

IntRect PaintInvalidatorContext::MapLocalRectToVisualRectForSVGChild(
    const LayoutObject& object,
    const FloatRect& local_rect) const {
  DCHECK(object.IsSVGChild());
  DCHECK(NeedsVisualRectUpdate(object));

  if (local_rect.IsEmpty())
    return IntRect();

  // Visual rects are in the space of their local transform node. For SVG, the
  // input rect is in local SVG coordinates in which paint offset doesn't apply.
  // We also don't need to adjust for clip path here because SVG the local
  // visual rect has already been adjusted by clip path.
  auto rect = local_rect;
  if (ShouldExcludeCompositedLayerSubpixelAccumulation(object)) {
    rect.Move(FloatSize(
        -paint_invalidation_container->Layer()->SubpixelAccumulation()));
  }
  // Use EnclosingIntRect to ensure the final visual rect will cover the rect
  // in source coordinates no matter if the painting will snap to pixels.
  return EnclosingIntRect(rect);
}

const PaintInvalidatorContext*
PaintInvalidatorContext::ParentContextAccessor::ParentContext() const {
  return tree_walk_ ? &tree_walk_->ContextAt(parent_context_index_)
                           .paint_invalidator_context
                    : nullptr;
}

IntRect PaintInvalidator::ComputeVisualRect(
    const LayoutObject& object,
    const PaintInvalidatorContext& context) {
  if (object.IsSVGChild()) {
    return context.MapLocalRectToVisualRectForSVGChild(
        object, SVGLayoutSupport::LocalVisualRect(object));
  }

  return context.MapLocalRectToVisualRect(object, object.LocalVisualRect());
}

void PaintInvalidator::UpdatePaintingLayer(const LayoutObject& object,
                                           PaintInvalidatorContext& context) {
  if (object.HasLayer() &&
      ToLayoutBoxModelObject(object).HasSelfPaintingLayer()) {
    context.painting_layer = ToLayoutBoxModelObject(object).Layer();
  } else if (object.IsColumnSpanAll() ||
             object.IsFloatingWithNonContainingBlockParent()) {
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

  // Table collapsed borders are painted in PaintPhaseDescendantBlockBackgrounds
  // on the table's layer.
  if (object.IsTable() &&
      ToInterface<LayoutNGTableInterface>(object).HasCollapsedBorders())
    context.painting_layer->SetNeedsPaintPhaseDescendantBlockBackgrounds();

  // The following flags are for descendants of the layer object only.
  if (object == context.painting_layer->GetLayoutObject())
    return;

  if (object.IsTableSection()) {
    const auto& section = ToInterface<LayoutNGTableSectionInterface>(object);
    if (section.TableInterface()->HasColElements())
      context.painting_layer->SetNeedsPaintPhaseDescendantBlockBackgrounds();
  }

  if (object.StyleRef().HasOutline())
    context.painting_layer->SetNeedsPaintPhaseDescendantOutlines();

  if (object.HasBoxDecorationBackground()
      // We also paint non-overlay overflow controls in background phase.
      || (object.HasOverflowClip() && ToLayoutBox(object)
                                          .GetScrollableArea()
                                          ->HasNonOverlayOverflowControls())) {
    context.painting_layer->SetNeedsPaintPhaseDescendantBlockBackgrounds();
  } else {
    // Hit testing rects for touch action paint in the background phase.
    if (object.HasEffectiveAllowedTouchAction())
      context.painting_layer->SetNeedsPaintPhaseDescendantBlockBackgrounds();
  }
}

void PaintInvalidator::UpdatePaintInvalidationContainer(
    const LayoutObject& object,
    PaintInvalidatorContext& context) {
  if (object.IsPaintInvalidationContainer()) {
    context.paint_invalidation_container = ToLayoutBoxModelObject(&object);
    if (object.StyleRef().IsStackingContext() || object.IsSVGRoot())
      context.paint_invalidation_container_for_stacked_contents =
          ToLayoutBoxModelObject(&object);
  } else if (object.IsLayoutView()) {
    // paint_invalidation_container_for_stacked_contents is only for stacked
    // descendants in its own frame, because it doesn't establish stacking
    // context for stacked contents in sub-frames.
    // Contents stacked in the root stacking context in this frame should use
    // this frame's PaintInvalidationContainer.
    context.paint_invalidation_container_for_stacked_contents =
        context.paint_invalidation_container =
            &object.ContainerForPaintInvalidation();
  } else if (object.IsColumnSpanAll() ||
             object.IsFloatingWithNonContainingBlockParent()) {
    // In these cases, the object may belong to an ancestor of the current
    // paint invalidation container, in paint order.
    // Post LayoutNG the |LayoutObject::IsFloatingWithNonContainingBlockParent|
    // check can be removed as floats will be painted by the correct layer.
    context.paint_invalidation_container =
        &object.ContainerForPaintInvalidation();
  } else if (object.StyleRef().IsStacked() &&
             // This is to exclude some objects (e.g. LayoutText) inheriting
             // stacked style from parent but aren't actually stacked.
             object.HasLayer() &&
             !ToLayoutBoxModelObject(object)
                  .Layer()
                  ->IsReplacedNormalFlowStacking() &&
             context.paint_invalidation_container !=
                 context.paint_invalidation_container_for_stacked_contents) {
    // The current object is stacked, so we should use
    // m_paintInvalidationContainerForStackedContents as its paint invalidation
    // container on which the current object is painted.
    context.paint_invalidation_container =
        context.paint_invalidation_container_for_stacked_contents;
    if (context.subtree_flags &
        PaintInvalidatorContext::kSubtreeFullInvalidationForStackedContents) {
      context.subtree_flags |=
          PaintInvalidatorContext::kSubtreeFullInvalidation;
    }
  }

  if (object == context.paint_invalidation_container) {
    // When we hit a new paint invalidation container, we don't need to
    // continue forcing a check for paint invalidation, since we're
    // descending into a different invalidation container. (For instance if
    // our parents were moved, the entire container will just move.)
    if (object != context.paint_invalidation_container_for_stacked_contents) {
      // However, we need to keep kSubtreeVisualRectUpdate and
      // kSubtreeFullInvalidationForStackedContents flags if the current
      // object isn't the paint invalidation container of stacked contents.
      context.subtree_flags &=
          (PaintInvalidatorContext::kSubtreeVisualRectUpdate |
           PaintInvalidatorContext::kSubtreeFullInvalidationForStackedContents);
    } else {
      context.subtree_flags = 0;
    }
  }

  DCHECK(context.paint_invalidation_container ==
         object.ContainerForPaintInvalidation());
  DCHECK(context.painting_layer == object.PaintingLayer());
}

void PaintInvalidator::UpdateVisualRect(const LayoutObject& object,
                                        FragmentData& fragment_data,
                                        PaintInvalidatorContext& context) {
  if (!context.NeedsVisualRectUpdate(object))
    return;

  DCHECK(context.tree_builder_context_);
  DCHECK(context.tree_builder_context_->current.paint_offset ==
         fragment_data.PaintOffset());

  fragment_data.SetVisualRect(ComputeVisualRect(object, context));

  object.GetFrameView()->GetLayoutShiftTracker().NotifyObjectPrePaint(
      object,
      PropertyTreeState(*context.tree_builder_context_->current.transform,
                        *context.tree_builder_context_->current.clip,
                        *context.tree_builder_context_->current_effect),
      context.old_visual_rect, fragment_data.VisualRect(),
      // Don't report a diff for a LayoutView. Any paint offset translation
      // it has was inherited from the parent frame, and movements of a
      // frame relative to its parent are tracked in the parent frame's
      // LayoutShiftTracker, not the child frame's.
      object.IsLayoutView()
          ? FloatSize()
          : context.tree_builder_context_->paint_offset_delta);
}

void PaintInvalidator::UpdateEmptyVisualRectFlag(
    const LayoutObject& object,
    PaintInvalidatorContext& context) {
  bool is_paint_invalidation_container =
      object == context.paint_invalidation_container;

  // Content under transforms needs to invalidate, even if visual
  // rects before and after update were the same. This is because
  // we don't know whether this transform will end up composited in
  // CAP, so such transforms are painted even if not visible
  // due to ancestor clips. This does not apply in SPv1 mode when
  // crossing paint invalidation container boundaries.
  if (is_paint_invalidation_container) {
    // Remove the flag when crossing paint invalidation container boundaries.
    context.subtree_flags &=
        ~PaintInvalidatorContext::kInvalidateEmptyVisualRect;
  } else if (object.StyleRef().HasTransform()) {
    context.subtree_flags |=
        PaintInvalidatorContext::kInvalidateEmptyVisualRect;
  }
}

bool PaintInvalidator::InvalidatePaint(
    const LayoutObject& object,
    const PaintPropertyTreeBuilderContext* tree_builder_context,
    PaintInvalidatorContext& context) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
               "PaintInvalidator::InvalidatePaint()", "object",
               object.DebugName().Ascii());

  if (object.IsSVGHiddenContainer())
    context.subtree_flags |= PaintInvalidatorContext::kSubtreeNoInvalidation;

  if (context.subtree_flags & PaintInvalidatorContext::kSubtreeNoInvalidation)
    return false;

  object.GetMutableForPainting().EnsureIsReadyForPaintInvalidation();

  UpdatePaintingLayer(object, context);
  UpdatePaintInvalidationContainer(object, context);
  UpdateEmptyVisualRectFlag(object, context);

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

  unsigned tree_builder_index = 0;

  for (auto* fragment_data = &object.GetMutableForPainting().FirstFragment();
       fragment_data;
       fragment_data = fragment_data->NextFragment(), tree_builder_index++) {
    context.old_visual_rect = fragment_data->VisualRect();
    context.fragment_data = fragment_data;

    DCHECK(!tree_builder_context ||
           tree_builder_index < tree_builder_context->fragments.size());

    {
#if DCHECK_IS_ON()
      context.tree_builder_context_actually_needed_ =
          tree_builder_context && tree_builder_context->is_actually_needed;
      FindObjectVisualRectNeedingUpdateScope finder(object, *fragment_data,
                                                    context);
#endif
      if (tree_builder_context) {
        context.tree_builder_context_ =
            &tree_builder_context->fragments[tree_builder_index];
        context.old_paint_offset =
            context.tree_builder_context_->old_paint_offset;
      } else {
        context.tree_builder_context_ = nullptr;
        context.old_paint_offset = fragment_data->PaintOffset();
      }

      UpdateVisualRect(object, *fragment_data, context);
    }

    object.InvalidatePaint(context);
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

  if (context.subtree_flags && context.NeedsVisualRectUpdate(object)) {
    // If any subtree flag is set, we also need to pass needsVisualRectUpdate
    // requirement to the subtree.
    // TODO(vmpstr): Investigate why this is true. Specifically, when crossing
    // an isolation boundary, is it safe to clear this subtree requirement.
    context.subtree_flags |= PaintInvalidatorContext::kSubtreeVisualRectUpdate;
  }

  if (context.NeedsVisualRectUpdate(object) &&
      object.ContainsInlineWithOutlineAndContinuation()) {
    // Force subtree visual rect update and invalidation checking to ensure
    // invalidation of focus rings when continuation's geometry changes.
    context.subtree_flags |=
        PaintInvalidatorContext::kSubtreeVisualRectUpdate |
        PaintInvalidatorContext::kSubtreeInvalidationChecking;
  }

  return reason != PaintInvalidationReason::kNone;
}

void PaintInvalidator::ProcessPendingDelayedPaintInvalidations() {
  for (auto* target : pending_delayed_paint_invalidations_)
    target->GetMutableForPainting().SetShouldDelayFullPaintInvalidation();
}

}  // namespace blink
