/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/compositing/compositing_layer_assigner.h"

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

// We will only allow squashing if the bbox-area:squashed-area doesn't exceed
// the ratio |kSquashingSparsityTolerance|:1.
constexpr uint64_t kSquashingSparsityTolerance = 6;

CompositingLayerAssigner::CompositingLayerAssigner(
    PaintLayerCompositor* compositor)
    : compositor_(compositor), layers_changed_(false) {}

CompositingLayerAssigner::~CompositingLayerAssigner() = default;

void CompositingLayerAssigner::Assign(
    PaintLayer* update_root,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  TRACE_EVENT0("blink", "CompositingLayerAssigner::assign");

  SquashingState squashing_state;
  AssignLayersToBackingsInternal(update_root, squashing_state,
                                 layers_needing_paint_invalidation);
  if (squashing_state.has_most_recent_mapping) {
    squashing_state.most_recent_mapping->FinishAccumulatingSquashingLayers(
        squashing_state.next_squashed_layer_index,
        layers_needing_paint_invalidation);
  }
}

void CompositingLayerAssigner::SquashingState::
    UpdateSquashingStateForNewMapping(
        CompositedLayerMapping* new_composited_layer_mapping,
        bool has_new_composited_layer_mapping,
        Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  // The most recent backing is done accumulating any more squashing layers.
  if (has_most_recent_mapping) {
    most_recent_mapping->FinishAccumulatingSquashingLayers(
        next_squashed_layer_index, layers_needing_paint_invalidation);
  }

  next_squashed_layer_index = 0;
  bounding_rect = IntRect();
  most_recent_mapping = new_composited_layer_mapping;
  has_most_recent_mapping = has_new_composited_layer_mapping;
  have_assigned_backings_to_entire_squashing_layer_subtree = false;
}

bool CompositingLayerAssigner::SquashingWouldExceedSparsityTolerance(
    const PaintLayer* candidate,
    const CompositingLayerAssigner::SquashingState& squashing_state) {
  IntRect bounds = candidate->ClippedAbsoluteBoundingBox();
  IntRect new_bounding_rect = squashing_state.bounding_rect;
  new_bounding_rect.Unite(bounds);
  const uint64_t new_bounding_rect_area = new_bounding_rect.Size().Area();
  const uint64_t new_squashed_area =
      squashing_state.total_area_of_squashed_rects + bounds.Size().Area();
  return new_bounding_rect_area >
         kSquashingSparsityTolerance * new_squashed_area;
}

bool CompositingLayerAssigner::NeedsOwnBacking(const PaintLayer* layer) const {
  if (!compositor_->CanBeComposited(layer))
    return false;

  return RequiresCompositing(layer->GetCompositingReasons()) ||
         (compositor_->StaleInCompositingMode() && layer->IsRootLayer());
}

CompositingStateTransitionType
CompositingLayerAssigner::ComputeCompositedLayerUpdate(PaintLayer* layer) {
  CompositingStateTransitionType update = kNoCompositingStateChange;
  if (NeedsOwnBacking(layer)) {
    if (!layer->HasCompositedLayerMapping()) {
      update = kAllocateOwnCompositedLayerMapping;
    }
  } else {
    if (layer->HasCompositedLayerMapping())
      update = kRemoveOwnCompositedLayerMapping;

    if (!layer->SubtreeIsInvisible() && compositor_->CanBeComposited(layer) &&
        RequiresSquashing(layer->GetCompositingReasons())) {
      // We can't compute at this time whether the squashing layer update is a
      // no-op, since that requires walking the paint layer tree.
      update = kPutInSquashingLayer;
    } else if (layer->GroupedMapping() || layer->LostGroupedMapping()) {
      update = kRemoveFromSquashingLayer;
    }
  }
  return update;
}

SquashingDisallowedReasons
CompositingLayerAssigner::GetReasonsPreventingSquashing(
    const PaintLayer* layer,
    const CompositingLayerAssigner::SquashingState& squashing_state) {
  if (!squashing_state.have_assigned_backings_to_entire_squashing_layer_subtree)
    return SquashingDisallowedReason::kWouldBreakPaintOrder;

  DCHECK(squashing_state.has_most_recent_mapping);
  const PaintLayer& squashing_layer =
      squashing_state.most_recent_mapping->OwningLayer();

  if (layer->GetLayoutObject().IsVideo() ||
      squashing_layer.GetLayoutObject().IsVideo())
    return SquashingDisallowedReason::kSquashingVideoIsDisallowed;

  // Don't squash iframes, frames or plugins.
  // FIXME: this is only necessary because there is frame code that assumes that
  // composited frames are not squashed.
  if (layer->GetLayoutObject().IsLayoutEmbeddedContent() ||
      squashing_layer.GetLayoutObject().IsLayoutEmbeddedContent()) {
    return SquashingDisallowedReason::
        kSquashingLayoutEmbeddedContentIsDisallowed;
  }

  if (SquashingWouldExceedSparsityTolerance(layer, squashing_state))
    return SquashingDisallowedReason::kSquashingSparsityExceeded;

  if (layer->GetLayoutObject().StyleRef().HasBlendMode() ||
      squashing_layer.GetLayoutObject().StyleRef().HasBlendMode())
    return SquashingDisallowedReason::kSquashingBlendingIsDisallowed;

  if (layer->ClippingContainer() != squashing_layer.ClippingContainer() &&
      !squashing_layer.GetCompositedLayerMapping()->ContainingSquashedLayer(
          layer->ClippingContainer(),
          squashing_state.next_squashed_layer_index))
    return SquashingDisallowedReason::kClippingContainerMismatch;

  if (layer->ScrollsWithRespectTo(&squashing_layer))
    return SquashingDisallowedReason::kScrollsWithRespectToSquashingLayer;

  if (layer->ScrollParent() && layer->HasCompositingDescendant())
    return SquashingDisallowedReason::kScrollChildWithCompositedDescendants;

  if (layer->OpacityAncestor() != squashing_layer.OpacityAncestor())
    return SquashingDisallowedReason::kOpacityAncestorMismatch;

  if (layer->TransformAncestor() != squashing_layer.TransformAncestor())
    return SquashingDisallowedReason::kTransformAncestorMismatch;

  if (layer->HasFilterInducingProperty() ||
      layer->FilterAncestor() != squashing_layer.FilterAncestor())
    return SquashingDisallowedReason::kFilterMismatch;

  if (layer->NearestFixedPositionLayer() !=
      squashing_layer.NearestFixedPositionLayer())
    return SquashingDisallowedReason::kNearestFixedPositionMismatch;
  DCHECK_NE(layer->GetLayoutObject().StyleRef().GetPosition(),
            EPosition::kFixed);

  if ((squashing_layer.GetLayoutObject()
           .StyleRef()
           .SubtreeWillChangeContents() &&
       squashing_layer.GetLayoutObject()
           .StyleRef()
           .IsRunningAnimationOnCompositor()) ||
      squashing_layer.GetLayoutObject()
          .StyleRef()
          .ShouldCompositeForCurrentAnimations())
    return SquashingDisallowedReason::kSquashingLayerIsAnimating;

  if (layer->EnclosingPaginationLayer())
    return SquashingDisallowedReason::kFragmentedContent;

  if (layer->GetLayoutObject().HasClipPath() ||
      layer->ClipPathAncestor() != squashing_layer.ClipPathAncestor())
    return SquashingDisallowedReason::kClipPathMismatch;

  if (layer->GetLayoutObject().HasMask() ||
      layer->MaskAncestor() != squashing_layer.MaskAncestor())
    return SquashingDisallowedReason::kMaskMismatch;

  if (layer->NearestContainedLayoutLayer() !=
      squashing_layer.NearestContainedLayoutLayer())
    return SquashingDisallowedReason::kCrossesLayoutContainmentBoundary;

  return SquashingDisallowedReason::kNone;
}

void CompositingLayerAssigner::UpdateSquashingAssignment(
    PaintLayer* layer,
    SquashingState& squashing_state,
    const CompositingStateTransitionType composited_layer_update,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  // NOTE: In the future as we generalize this, the background of this layer may
  // need to be assigned to a different backing than the squashed PaintLayer's
  // own primary contents. This would happen when we have a composited negative
  // z-index element that needs to paint on top of the background, but below the
  // layer's main contents. For now, because we always composite layers when
  // they have a composited negative z-index child, such layers will never need
  // squashing so it is not yet an issue.
  if (composited_layer_update == kPutInSquashingLayer) {
    // A layer that is squashed with other layers cannot have its own
    // CompositedLayerMapping.
    DCHECK(!layer->HasCompositedLayerMapping());
    DCHECK(squashing_state.has_most_recent_mapping);

    bool changed_squashing_layer =
        squashing_state.most_recent_mapping->UpdateSquashingLayerAssignment(
            layer, squashing_state.next_squashed_layer_index);
    if (!changed_squashing_layer)
      return;

    // If we've modified the collection of squashed layers, we must update
    // the graphics layer geometry.
    squashing_state.most_recent_mapping->SetNeedsGraphicsLayerUpdate(
        kGraphicsLayerUpdateSubtree);

    layer->ClearClipRects();

    // Issue a paint invalidation, since |layer| may have been added to an
    // already-existing squashing layer.
    layers_needing_paint_invalidation.push_back(layer);
    layers_changed_ = true;
  } else if (composited_layer_update == kRemoveFromSquashingLayer) {
    if (layer->GroupedMapping()) {
      // Before removing |layer| from an already-existing squashing layer that
      // may have other content, issue a paint invalidation.
      compositor_->PaintInvalidationOnCompositingChange(layer);
      layer->GroupedMapping()->SetNeedsGraphicsLayerUpdate(
          kGraphicsLayerUpdateSubtree);
      layer->SetGroupedMapping(
          nullptr, PaintLayer::kInvalidateLayerAndRemoveFromMapping);
    }

    // If we need to issue paint invalidations, do so now that we've removed it
    // from a squashed layer.
    layers_needing_paint_invalidation.push_back(layer);
    layers_changed_ = true;

    layer->SetLostGroupedMapping(false);
  }
}

void CompositingLayerAssigner::AssignLayersToBackingsInternal(
    PaintLayer* layer,
    SquashingState& squashing_state,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  if (layer->NeedsCompositingLayerAssignment()) {
    DCHECK(layer->GetCompositingReasons() ||
           (layer->GetCompositingState() != kNotComposited) ||
           layer->LostGroupedMapping());
    if (RequiresSquashing(layer->GetCompositingReasons())) {
      SquashingDisallowedReasons reasons_preventing_squashing =
          GetReasonsPreventingSquashing(layer, squashing_state);
      if (reasons_preventing_squashing) {
        layer->SetCompositingReasons(layer->GetCompositingReasons() |
                                     CompositingReason::kSquashingDisallowed);
        layer->SetSquashingDisallowedReasons(reasons_preventing_squashing);
      }
    }

    CompositingStateTransitionType composited_layer_update =
        ComputeCompositedLayerUpdate(layer);

    if (compositor_->AllocateOrClearCompositedLayerMapping(
            layer, composited_layer_update)) {
      layers_needing_paint_invalidation.push_back(layer);
      layers_changed_ = true;
      if (ScrollingCoordinator* scrolling_coordinator =
              layer->GetScrollingCoordinator()) {
        if (layer->GetLayoutObject()
                .StyleRef()
                .HasViewportConstrainedPosition()) {
          scrolling_coordinator->FrameViewFixedObjectsDidChange(
              layer->GetLayoutObject().View()->GetFrameView());
        }
      }
    }

    if (composited_layer_update != kNoCompositingStateChange) {
      // A change in the compositing state of a ScrollTimeline's scroll source
      // can cause the compositor's view of the scroll source to become out of
      // date. We inform the WorkletAnimationController about any such changes
      // so that it can schedule a compositing animations update.
      Node* node = layer->GetLayoutObject().GetNode();
      if (node && ScrollTimeline::HasActiveScrollTimeline(node)) {
        node->GetDocument()
            .GetWorkletAnimationController()
            .ScrollSourceCompositingStateChanged(node);
      }
    }

    // Add this layer to a squashing backing if needed.
    UpdateSquashingAssignment(layer, squashing_state, composited_layer_update,
                              layers_needing_paint_invalidation);

    const bool layer_is_squashed =
        composited_layer_update == kPutInSquashingLayer ||
        (composited_layer_update == kNoCompositingStateChange &&
         layer->GroupedMapping());
    if (layer_is_squashed) {
      squashing_state.next_squashed_layer_index++;
      IntRect layer_bounds = layer->ClippedAbsoluteBoundingBox();
      squashing_state.total_area_of_squashed_rects +=
          layer_bounds.Size().Area();
      squashing_state.bounding_rect.Unite(layer_bounds);
    }
  }

  if (layer->StackingDescendantNeedsCompositingLayerAssignment()) {
    PaintLayerPaintOrderIterator iterator(*layer, kNegativeZOrderChildren);
    while (PaintLayer* child_node = iterator.Next()) {
      AssignLayersToBackingsInternal(child_node, squashing_state,
                                     layers_needing_paint_invalidation);
    }
  }

  // At this point, if the layer is to be separately composited, then its
  // backing becomes the most recent in paint-order.
  if (layer->NeedsCompositingLayerAssignment() &&
      layer->GetCompositingState() == kPaintsIntoOwnBacking) {
    DCHECK(!RequiresSquashing(layer->GetCompositingReasons()));
    squashing_state.UpdateSquashingStateForNewMapping(
        layer->GetCompositedLayerMapping(), layer->HasCompositedLayerMapping(),
        layers_needing_paint_invalidation);
  }

  if (layer->StackingDescendantNeedsCompositingLayerAssignment()) {
    PaintLayerPaintOrderIterator iterator(*layer,
                                          kNormalFlowAndPositiveZOrderChildren);
    while (PaintLayer* curr_layer = iterator.Next()) {
      AssignLayersToBackingsInternal(curr_layer, squashing_state,
                                     layers_needing_paint_invalidation);
    }
  }

  if (layer->NeedsCompositingLayerAssignment()) {
    if (squashing_state.has_most_recent_mapping &&
        &squashing_state.most_recent_mapping->OwningLayer() == layer) {
      squashing_state.have_assigned_backings_to_entire_squashing_layer_subtree =
          true;
    }
  }
  layer->ClearNeedsCompositingLayerAssignment();
}

}  // namespace blink
