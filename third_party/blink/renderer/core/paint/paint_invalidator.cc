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
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
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
template <typename Rect, typename Point>
void PaintInvalidator::ExcludeCompositedLayerSubpixelAccumulation(
    const LayoutObject& object,
    const PaintInvalidatorContext& context,
    Rect& rect) {
  // TODO(wangxianzhu): How to handle sub-pixel location animation for SPv2?
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  // One of the following conditions happened in crbug.com/837226.
  if (!context.paint_invalidation_container ||
      !context.paint_invalidation_container->FirstFragment()
           .HasLocalBorderBoxProperties() ||
      !context.tree_builder_context_)
    return;

  if (!(context.paint_invalidation_container->Layer()->GetCompositingReasons() &
        CompositingReason::kComboAllDirectReasons))
    return;

  if (object != context.paint_invalidation_container &&
      context.paint_invalidation_container->FirstFragment()
              .PostScrollTranslation() !=
          context.tree_builder_context_->current.transform) {
    // Subpixel accumulation doesn't propagate through non-translation
    // transforms. Also skip all transforms, to avoid the runtime cost of
    // verifying whether the transform is a translation.
    return;
  }

  // Exclude the subpixel accumulation so that the paint invalidator won't
  // see changed visual rects during composited animation with subpixels, to
  // avoid full layer invalidation. The subpixel accumulation will be added
  // back in ChunkToLayerMapper::AdjustVisualRectBySubpixelOffset(). Should
  // make sure the code is synced.
  // TODO(wangxianzhu): Avoid exposing subpixel accumulation to platform code.
  rect.MoveBy(Point(LayoutPoint(
      -context.paint_invalidation_container->Layer()->SubpixelAccumulation())));
}

// This function is templatized to avoid FloatRect<->LayoutRect conversions
// which affect performance.
template <typename Rect, typename Point>
LayoutRect PaintInvalidator::MapLocalRectToVisualRect(
    const LayoutObject& object,
    const Rect& local_rect,
    const PaintInvalidatorContext& context,
    bool disable_flip) {
  DCHECK(context.NeedsVisualRectUpdate(object));
  if (local_rect.IsEmpty())
    return LayoutRect();

  bool is_svg_child = object.IsSVGChild();

  // TODO(wkorman): The flip below is required because local visual rects are
  // currently in "physical coordinates with flipped block-flow direction"
  // (see LayoutBoxModelObject.h) but we need them to be in physical
  // coordinates.
  Rect rect = local_rect;
  // Writing-mode flipping doesn't apply to non-root SVG.
  if (!is_svg_child) {
    if (!disable_flip) {
      if (object.IsBox()) {
        ToLayoutBox(object).FlipForWritingMode(rect);
      } else if (!(context.subtree_flags &
                   PaintInvalidatorContext::kSubtreeSlowPathRect)) {
        // For SPv2 and the GeometryMapper path, we also need to convert the
        // rect for non-boxes into physical coordinates before applying paint
        // offset. (Otherwise we'll call mapToVisualrectInAncestorSpace() which
        // requires physical coordinates for boxes, but "physical coordinates
        // with flipped block-flow direction" for non-boxes for which we don't
        // need to flip.)
        // TODO(wangxianzhu): Avoid containingBlock().
        object.ContainingBlock()->FlipForWritingMode(rect);
      }
    }

    // Unite visual rect with clip path bounding rect.
    // It is because the clip path display items are owned by the layout object
    // who has the clip path, and uses its visual rect as bounding rect too.
    // Usually it is done at layout object level and included as a part of
    // local visual overflow, but clip-path can be a reference to SVG, and we
    // have to wait until pre-paint to ensure clean layout.
    // Note: SVG children don't need this adjustment because their visual
    // overflow rects are already adjusted by clip path.
    if (base::Optional<FloatRect> clip_path_bounding_box =
            ClipPathClipper::LocalClipPathBoundingBox(object)) {
      Rect box(EnclosingIntRect(*clip_path_bounding_box));
      rect.Unite(box);
    }
  }

  // Visual rects are in the space of their local transform node. For SVG, the
  // input rect is in local SVG coordinates in which paint offset doesn't apply.
  if (!is_svg_child)
    rect.MoveBy(Point(context.fragment_data->PaintOffset()));
  ExcludeCompositedLayerSubpixelAccumulation<Rect, Point>(object, context,
                                                          rect);
  // Use EnclosingIntRect to ensure the final visual rect will cover the rect
  // in source coordinates no matter if the painting will snap to pixels.
  return LayoutRect(EnclosingIntRect(rect));
}

void PaintInvalidatorContext::MapLocalRectToVisualRect(
    const LayoutObject& object,
    LayoutRect& rect) const {
  rect = PaintInvalidator::MapLocalRectToVisualRect<LayoutRect, LayoutPoint>(
      object, rect, *this);
}

const PaintInvalidatorContext*
PaintInvalidatorContext::ParentContextAccessor::ParentContext() const {
  return tree_walk_ ? &tree_walk_->ContextAt(parent_context_index_)
                           .paint_invalidator_context
                    : nullptr;
}

LayoutRect PaintInvalidator::ComputeVisualRect(
    const LayoutObject& object,
    const PaintInvalidatorContext& context) {
  if (object.IsSVGChild()) {
    FloatRect local_rect = SVGLayoutSupport::LocalVisualRect(object);
    return MapLocalRectToVisualRect<FloatRect, FloatPoint>(object, local_rect,
                                                           context);
  }
  LayoutRect local_rect = object.LocalVisualRect();
  return MapLocalRectToVisualRect<LayoutRect, LayoutPoint>(object, local_rect,
                                                           context);
}

static LayoutRect ComputeFragmentLocalSelectionRect(
    const NGPaintFragment& fragment) {
  if (!fragment.PhysicalFragment().IsText())
    return LayoutRect();
  const FrameSelection& frame_selection =
      fragment.GetLayoutObject()->GetFrame()->Selection();
  const LayoutSelectionStatus status =
      frame_selection.ComputeLayoutSelectionStatus(fragment);
  if (status.start == status.end)
    return LayoutRect();
  return fragment.ComputeLocalSelectionRectForText(status).ToLayoutRect();
}

LayoutRect PaintInvalidator::MapFragmentLocalRectToVisualRect(
    const LayoutRect& local_rect,
    const LayoutObject& object,
    const NGPaintFragment& fragment,
    const PaintInvalidatorContext& context) {
  LayoutRect rect = local_rect;
  if (!object.IsBox())
    rect.Move(fragment.InlineOffsetToContainerBox().ToLayoutSize());
  bool disable_flip = true;
  return MapLocalRectToVisualRect<LayoutRect, LayoutPoint>(
      object, rect, context, disable_flip);
}

void PaintInvalidator::UpdatePaintingLayer(const LayoutObject& object,
                                           PaintInvalidatorContext& context) {
  if (object.HasLayer() &&
      ToLayoutBoxModelObject(object).HasSelfPaintingLayer()) {
    context.painting_layer = ToLayoutBoxModelObject(object).Layer();
  } else if (object.IsColumnSpanAll() ||
             object.IsFloatingWithNonContainingBlockParent()) {
    // See LayoutObject::paintingLayer() for the special-cases of floating under
    // inline and multicolumn.
    context.painting_layer = object.PaintingLayer();
  }

  if (object.IsLayoutBlockFlow() && ToLayoutBlockFlow(object).ContainsFloats())
    context.painting_layer->SetNeedsPaintPhaseFloat();

  // Table collapsed borders are painted in PaintPhaseDescendantBlockBackgrounds
  // on the table's layer.
  if (object.IsTable() && ToLayoutTable(object).HasCollapsedBorders())
    context.painting_layer->SetNeedsPaintPhaseDescendantBlockBackgrounds();

  // The following flags are for descendants of the layer object only.
  if (object == context.painting_layer->GetLayoutObject())
    return;

  if (object.IsTableSection()) {
    const auto& section = ToLayoutTableSection(object);
    if (section.Table()->HasColElements())
      context.painting_layer->SetNeedsPaintPhaseDescendantBlockBackgrounds();
  }

  if (object.StyleRef().HasOutline())
    context.painting_layer->SetNeedsPaintPhaseDescendantOutlines();

  if (object.HasBoxDecorationBackground()
      // We also paint overflow controls in background phase.
      || (object.HasOverflowClip() &&
          ToLayoutBox(object).GetScrollableArea()->HasOverflowControls())) {
    context.painting_layer->SetNeedsPaintPhaseDescendantBlockBackgrounds();
  } else if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled()) {
    // Hit testing rects for touch action paint in the background phase.
    if (object.EffectiveWhitelistedTouchAction() !=
        TouchAction::kTouchActionAuto) {
      context.painting_layer->SetNeedsPaintPhaseDescendantBlockBackgrounds();
    }
  }
}

void PaintInvalidator::UpdatePaintInvalidationContainer(
    const LayoutObject& object,
    PaintInvalidatorContext& context) {
  if (object.IsPaintInvalidationContainer()) {
    context.paint_invalidation_container = ToLayoutBoxModelObject(&object);
    if (object.StyleRef().IsStackingContext())
      context.paint_invalidation_container_for_stacked_contents =
          ToLayoutBoxModelObject(&object);
  } else if (object.IsLayoutView()) {
    // paintInvalidationContainerForStackedContents is only for stacked
    // descendants in its own frame, because it doesn't establish stacking
    // context for stacked contents in sub-frames.
    // Contents stacked in the root stacking context in this frame should use
    // this frame's paintInvalidationContainer.
    context.paint_invalidation_container_for_stacked_contents =
        context.paint_invalidation_container;
  } else if (object.IsFloatingWithNonContainingBlockParent() ||
             object.IsColumnSpanAll()) {
    // In these cases, the object may belong to an ancestor of the current
    // paint invalidation container, in paint order.
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

  LayoutRect new_visual_rect = ComputeVisualRect(object, context);
  // Make the empty visual rect more meaningful for debugging and testing.
  if (new_visual_rect.IsEmpty())
    new_visual_rect.SetLocation(fragment_data.PaintOffset());
  fragment_data.SetVisualRect(new_visual_rect);

  // For LayoutNG, update NGPaintFragments.
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  // TODO(kojii): multi-col needs additional logic. What's needed is to be
  // figured out.
  if (object.IsLayoutNGMixin()) {
    if (NGPaintFragment* fragment = ToLayoutBlockFlow(object).PaintFragment())
      fragment->SetVisualRect(new_visual_rect);

    // Also check IsInline below. Inline block is LayoutBlockFlow but is also in
    // an inline formatting context.
  }

  if (object.IsInline()) {
    // An inline LayoutObject can produce multiple NGPaintFragment. Compute
    // VisualRect for each fragment from |new_visual_rect|.
    auto fragments = NGPaintFragment::InlineFragmentsFor(&object);
    if (fragments.IsInLayoutNGInlineFormattingContext()) {
      for (NGPaintFragment* fragment : fragments) {
        LayoutRect local_selection_rect =
            ComputeFragmentLocalSelectionRect(*fragment);
        LayoutRect local_visual_rect =
            UnionRect(fragment->SelfInkOverflow(), local_selection_rect);
        fragment->SetVisualRect(MapFragmentLocalRectToVisualRect(
            local_visual_rect, object, *fragment, context));

        LayoutRect selection_visual_rect = MapFragmentLocalRectToVisualRect(
            local_selection_rect, object, *fragment, context);
        const bool should_invalidate =
            object.ShouldInvalidateSelection() ||
            selection_visual_rect != fragment->SelectionVisualRect();
        const bool rect_exists = !selection_visual_rect.IsEmpty() ||
                                 !fragment->SelectionVisualRect().IsEmpty();
        if (should_invalidate && rect_exists) {
          context.painting_layer->SetNeedsRepaint();
          ObjectPaintInvalidator(object).InvalidateDisplayItemClient(
              *fragment, PaintInvalidationReason::kSelection);
          fragment->SetSelectionVisualRect(selection_visual_rect);
        }
      }
    }
  }
}

void PaintInvalidator::InvalidatePaint(
    LocalFrameView& frame_view,
    const PaintPropertyTreeBuilderContext* tree_builder_context,

    PaintInvalidatorContext& context) {
  LayoutView* layout_view = frame_view.GetLayoutView();
  CHECK(layout_view);

  context.paint_invalidation_container =
      context.paint_invalidation_container_for_stacked_contents =
          &layout_view->ContainerForPaintInvalidation();
  context.painting_layer = layout_view->Layer();
  context.fragment_data = &layout_view->FirstFragment();
  if (tree_builder_context) {
    context.tree_builder_context_ = &tree_builder_context->fragments[0];
#if DCHECK_IS_ON()
    context.tree_builder_context_actually_needed_ =
        tree_builder_context->is_actually_needed;
#endif
  }
}

static void InvalidateChromeClient(
    const LayoutBoxModelObject& paint_invalidation_container) {
  if (paint_invalidation_container.GetDocument().Printing() &&
      !RuntimeEnabledFeatures::PrintBrowserEnabled())
    return;

  DCHECK(paint_invalidation_container.IsLayoutView());
  DCHECK(!paint_invalidation_container.IsPaintInvalidationContainer());

  auto* frame_view = paint_invalidation_container.GetFrameView();
  DCHECK(!frame_view->GetFrame().OwnerLayoutObject());
  if (auto* client = frame_view->GetChromeClient()) {
    client->InvalidateRect(IntRect(IntPoint(), frame_view->Size()));
  }
}

void PaintInvalidator::UpdateEmptyVisualRectFlag(
    const LayoutObject& object,
    PaintInvalidatorContext& context) {
  bool is_paint_invalidation_container =
      object == context.paint_invalidation_container;

  // Content under transforms needs to invalidate, even if visual
  // rects before and after update were the same. This is because
  // we don't know whether this transform will end up composited in
  // SPv2, so such transforms are painted even if not visible
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

void PaintInvalidator::InvalidatePaint(
    const LayoutObject& object,
    const PaintPropertyTreeBuilderContext* tree_builder_context,
    PaintInvalidatorContext& context) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
               "PaintInvalidator::InvalidatePaint()", "object",
               object.DebugName().Ascii());

  if (object.IsSVGHiddenContainer()) {
    context.subtree_flags |= PaintInvalidatorContext::kSubtreeNoInvalidation;
  }
  if (context.subtree_flags & PaintInvalidatorContext::kSubtreeNoInvalidation)
    return;

  object.GetMutableForPainting().EnsureIsReadyForPaintInvalidation();

  UpdatePaintingLayer(object, context);

  // TODO(chrishtr): refactor to remove these slow paths by expanding their
  // LocalVisualRect to include repeated locations.
  if (object.IsTableSection()) {
    const auto& section = ToLayoutTableSection(object);
    if (section.IsRepeatingHeaderGroup() || section.IsRepeatingFooterGroup())
      context.subtree_flags |= PaintInvalidatorContext::kSubtreeSlowPathRect;
  }
  if (object.IsFixedPositionObjectInPagedMedia())
    context.subtree_flags |= PaintInvalidatorContext::kSubtreeSlowPathRect;

  UpdatePaintInvalidationContainer(object, context);
  UpdateEmptyVisualRectFlag(object, context);

  if (!object.ShouldCheckForPaintInvalidation() && !context.NeedsSubtreeWalk())
    return;

  unsigned tree_builder_index = 0;

  for (auto *fragment_data = &object.GetMutableForPainting().FirstFragment();
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
      !IsFullPaintInvalidationReason(reason))
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

  // The object is under a frame for WebViewPlugin, SVG images etc. Need to
  // inform the chrome client of the invalidation so that the client will
  // initiate painting of the contents.
  // TODO(wangxianzhu): Do we need this for SPv2?
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      !context.paint_invalidation_container->IsPaintInvalidationContainer() &&
      reason != PaintInvalidationReason::kNone)
    InvalidateChromeClient(*context.paint_invalidation_container);
}

void PaintInvalidator::ProcessPendingDelayedPaintInvalidations() {
  for (auto* target : pending_delayed_paint_invalidations_)
    target->GetMutableForPainting().SetShouldDelayFullPaintInvalidation();
}

}  // namespace blink
