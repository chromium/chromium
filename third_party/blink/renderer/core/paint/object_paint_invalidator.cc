// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/find_paint_offset_and_visual_rect_needing_update.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"

namespace blink {

template <typename Functor>
static void TraverseNonCompositingDescendantsInPaintOrder(const LayoutObject&,
                                                          const Functor&);

static bool MayBeSkippedContainerForFloating(const LayoutObject& object) {
  return !object.IsInLayoutNGInlineFormattingContext() &&
         !object.IsLayoutBlock();
}

template <typename Functor>
static void
TraverseNonCompositingDescendantsBelongingToAncestorPaintInvalidationContainer(
    const LayoutObject& object,
    const Functor& functor) {
  // |object| is a paint invalidation container, but is not a stacking context
  // (legacy layout only: or is a non-block), so the paint invalidation
  // container of stacked descendants may not belong to |object| but belong to
  // an ancestor. This function traverses all such descendants. See (legacy
  // layout only: Case 1a and) Case 2 below for details.
  DCHECK(object.IsPaintInvalidationContainer() &&
         (!object.StyleRef().IsStackingContext() ||
          MayBeSkippedContainerForFloating(object)));

  LayoutObject* descendant = object.NextInPreOrder(&object);
  while (descendant) {
    if (!descendant->HasLayer() || !descendant->StyleRef().IsStacked()) {
      // Case 1: The descendant is not stacked (or is stacked but has not been
      // allocated a layer yet during style change), so either it's a paint
      // invalidation container in the same situation as |object|, or its paint
      // invalidation container is in such situation. Keep searching until a
      // stacked layer is found.
      if (MayBeSkippedContainerForFloating(object) &&
          descendant->IsFloating()) {
        // The following is for legacy layout only because LayoutNG allows an
        // inline to contain floats.
        // Case 1a (rare): However, if the descendant is a floating object below
        // a composited non-block object, the subtree may belong to an ancestor
        // in paint order, thus recur into the subtree. Note that for
        // performance, we don't check whether the floating object's container
        // is above or under |object|, so we may traverse more than expected.
        // Example:
        // <span id="object" class="position: relative; will-change: transform">
        //   <div id="descendant" class="float: left"></div>"
        // </span>
        TraverseNonCompositingDescendantsInPaintOrder(*descendant, functor);
        descendant = descendant->NextInPreOrderAfterChildren(&object);
      } else {
        descendant = descendant->NextInPreOrder(&object);
      }
    } else if (!descendant->IsPaintInvalidationContainer()) {
      // Case 2: The descendant is stacked and is not composited.
      // The invalidation container of its subtree is our ancestor,
      // thus recur into the subtree.
      TraverseNonCompositingDescendantsInPaintOrder(*descendant, functor);
      descendant = descendant->NextInPreOrderAfterChildren(&object);
    } else if (descendant->StyleRef().IsStackingContext() &&
               !MayBeSkippedContainerForFloating(*descendant)) {
      // Case 3: The descendant is an invalidation container and is a stacking
      // context.  No objects in the subtree can have invalidation container
      // outside of it, thus skip the whole subtree.
      // Legacy layout only: This excludes non-block because there might be
      // floating objects under the descendant belonging to some ancestor in
      // paint order (Case 1a).
      descendant = descendant->NextInPreOrderAfterChildren(&object);
    } else {
      // Case 4: The descendant is an invalidation container but not a stacking
      // context, or the descendant is a non-block stacking context.
      // This is the same situation as |object|, thus keep searching.
      descendant = descendant->NextInPreOrder(&object);
    }
  }
}

template <typename Functor>
static void TraverseNonCompositingDescendantsInPaintOrder(
    const LayoutObject& object,
    const Functor& functor) {
  functor(object);
  LayoutObject* descendant = object.NextInPreOrder(&object);
  while (descendant) {
    if (!descendant->IsPaintInvalidationContainer()) {
      functor(*descendant);
      descendant = descendant->NextInPreOrder(&object);
    } else if (descendant->StyleRef().IsStackingContext() &&
               !MayBeSkippedContainerForFloating(*descendant)) {
      // The descendant is an invalidation container and is a stacking context.
      // No objects in the subtree can have invalidation container outside of
      // it, thus skip the whole subtree.
      // Legacy layout only: This excludes non-blocks because there might be
      // floating objects under the descendant belonging to some ancestor in
      // paint order (Case 1a).
      descendant = descendant->NextInPreOrderAfterChildren(&object);
    } else {
      // If a paint invalidation container is not a stacking context, or the
      // descendant is a non-block stacking context, some of its descendants may
      // belong to the parent container.
      TraverseNonCompositingDescendantsBelongingToAncestorPaintInvalidationContainer(
          *descendant, functor);
      descendant = descendant->NextInPreOrderAfterChildren(&object);
    }
  }
}

static void SetPaintingLayerNeedsRepaintDuringTraverse(
    const LayoutObject& object) {
  if (object.HasLayer() &&
      ToLayoutBoxModelObject(object).HasSelfPaintingLayer()) {
    ToLayoutBoxModelObject(object).Layer()->SetNeedsRepaint();
  } else if (object.IsFloating() && object.Parent() &&
             MayBeSkippedContainerForFloating(*object.Parent())) {
    // The following is for legacy layout only because LayoutNG allows an
    // inline to contain floats.
    object.PaintingLayer()->SetNeedsRepaint();
  }
}

void ObjectPaintInvalidator::
    InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
        PaintInvalidationReason reason) {
  // This is valid because we want to invalidate the client in the display item
  // list of the current backing.
  DisableCompositingQueryAsserts disabler;

  SlowSetPaintingLayerNeedsRepaint();
  TraverseNonCompositingDescendantsInPaintOrder(
      object_, [reason](const LayoutObject& object) {
        SetPaintingLayerNeedsRepaintDuringTraverse(object);
        object.InvalidateDisplayItemClients(reason);
      });
}

void ObjectPaintInvalidator::
    InvalidatePaintIncludingNonCompositingDescendants() {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  SlowSetPaintingLayerNeedsRepaint();
  // This method may be used to invalidate paint of objects changing paint
  // invalidation container. Visual rects don't have to be cleared, since they
  // are relative to the transform ancestor.
  // TODO(vmpstr): After paint containment isolation is in place, we might not
  // have to recurse past the paint containment boundary.
  TraverseNonCompositingDescendantsInPaintOrder(
      object_, [](const LayoutObject& object) {
        SetPaintingLayerNeedsRepaintDuringTraverse(object);
      });
}

void ObjectPaintInvalidator::
    InvalidatePaintIncludingNonSelfPaintingLayerDescendants() {
  SlowSetPaintingLayerNeedsRepaint();
  // This method may be used to invalidate paint of objects changing paint
  // invalidation container. Clear previous visual rect on the original paint
  // invalidation container to avoid under-invalidation if the visual rect on
  // the new paint invalidation container happens to be the same as the old one.
  struct Helper {
    static void Traverse(const LayoutObject& object) {
      object.GetMutableForPainting().ClearPreviousVisualRects();
      for (LayoutObject* child = object.SlowFirstChild(); child;
           child = child->NextSibling()) {
        if (!child->HasLayer() ||
            !ToLayoutBoxModelObject(child)->Layer()->IsSelfPaintingLayer())
          Traverse(*child);
      }
    }
  };
  Helper::Traverse(object_);
}

void ObjectPaintInvalidator::InvalidateDisplayItemClient(
    const DisplayItemClient& client,
    PaintInvalidationReason reason) {
  // It's caller's responsibility to ensure PaintingLayer's NeedsRepaint is set.
  // Don't set the flag here because getting PaintLayer has cost and the caller
  // can use various ways (e.g. PaintInvalidatinContext::painting_layer) to
  // reduce the cost.
  DCHECK(!object_.PaintingLayer() ||
         object_.PaintingLayer()->SelfNeedsRepaint());

  client.Invalidate(reason);

  if (LocalFrameView* frame_view = object_.GetFrameView())
    frame_view->TrackObjectPaintInvalidation(client, reason);
}

void ObjectPaintInvalidator::SlowSetPaintingLayerNeedsRepaint() {
  if (PaintLayer* painting_layer = object_.PaintingLayer())
    painting_layer->SetNeedsRepaint();
}

DISABLE_CFI_PERF
PaintInvalidationReason
ObjectPaintInvalidatorWithContext::ComputePaintInvalidationReason() {
  // This is before any early return to ensure the background obscuration status
  // is saved.
  if (!object_.ShouldCheckForPaintInvalidation() &&
      (!context_.subtree_flags ||
       context_.subtree_flags ==
           PaintInvalidatorContext::kSubtreeVisualRectUpdate)) {
    // No paint invalidation flag, or just kSubtreeVisualRectUpdate (which has
    // been handled in PaintInvalidator). No paint invalidation is needed.
    return PaintInvalidationReason::kNone;
  }

  if (context_.subtree_flags &
      PaintInvalidatorContext::kSubtreeFullInvalidation)
    return PaintInvalidationReason::kSubtree;

  if (object_.ShouldDoFullPaintInvalidation())
    return object_.FullPaintInvalidationReason();

  if (object_.GetDocument().InForcedColorsMode() &&
      object_.IsLayoutBlockFlow() && !context_.old_visual_rect.IsEmpty())
    return PaintInvalidationReason::kBackplate;

  if (!(context_.subtree_flags &
        PaintInvalidatorContext::kInvalidateEmptyVisualRect) &&
      context_.old_visual_rect.IsEmpty() &&
      context_.fragment_data->VisualRect().IsEmpty())
    return PaintInvalidationReason::kNone;

  if (object_.PaintedOutputOfObjectHasNoEffectRegardlessOfSize())
    return PaintInvalidationReason::kNone;

  // Force full paint invalidation if the outline may be affected by descendants
  // and this object is marked for checking paint invalidation for any reason.
  if (object_.OutlineMayBeAffectedByDescendants() ||
      object_.PreviousOutlineMayBeAffectedByDescendants()) {
    object_.GetMutableForPainting()
        .UpdatePreviousOutlineMayBeAffectedByDescendants();
    return PaintInvalidationReason::kOutline;
  }

  // If the size is zero on one of our bounds then we know we're going to have
  // to do a full invalidation of either old bounds or new bounds.
  if (context_.old_visual_rect.IsEmpty())
    return PaintInvalidationReason::kAppeared;
  if (context_.fragment_data->VisualRect().IsEmpty())
    return PaintInvalidationReason::kDisappeared;

  // If we shifted, we don't know the exact reason so we are conservative and
  // trigger a full invalidation. Shifting could be caused by some layout
  // property (left / top) or some in-flow layoutObject inserted / removed
  // before us in the tree.
  if (context_.fragment_data->VisualRect().Location() !=
      context_.old_visual_rect.Location())
    return PaintInvalidationReason::kGeometry;

  // Most paintings are pixel-snapped so subpixel change of paint offset doesn't
  // directly cause full raster invalidation.
  if (RoundedIntPoint(context_.fragment_data->PaintOffset()) !=
      RoundedIntPoint(context_.old_paint_offset))
    return PaintInvalidationReason::kGeometry;

  // Incremental invalidation is only applicable to LayoutBoxes. Return
  // PaintInvalidationIncremental no matter if oldVisualRect and newVisualRect
  // are equal because a LayoutBox may need paint invalidation if its border box
  // changes. BoxPaintInvalidator may also override this reason with a full
  // paint invalidation reason if needed.
  if (object_.IsBox())
    return PaintInvalidationReason::kIncremental;

  if (context_.old_visual_rect != context_.fragment_data->VisualRect())
    return PaintInvalidationReason::kGeometry;

  return PaintInvalidationReason::kNone;
}

DISABLE_CFI_PERF
PaintInvalidationReason ObjectPaintInvalidatorWithContext::InvalidateSelection(
    PaintInvalidationReason reason) {
  // Update selection rect when we are doing full invalidation with geometry
  // change (in case that the object is moved, composite status changed, etc.)
  // or shouldInvalidationSelection is set (in case that the selection itself
  // changed).
  bool full_invalidation = IsFullPaintInvalidationReason(reason);
  if (!full_invalidation && !object_.ShouldInvalidateSelection())
    return reason;

  IntRect old_selection_rect = object_.SelectionVisualRect();
  IntRect new_selection_rect;
#if DCHECK_IS_ON()
  FindVisualRectNeedingUpdateScope finder(object_, context_, old_selection_rect,
                                          new_selection_rect);
#endif
  if (context_.NeedsVisualRectUpdate(object_)) {
    new_selection_rect = context_.MapLocalRectToVisualRect(
        object_, object_.LocalSelectionVisualRect());
  } else {
    new_selection_rect = old_selection_rect;
  }

  object_.GetMutableForPainting().SetSelectionVisualRect(new_selection_rect);

  if (full_invalidation)
    return reason;
  // We should invalidate LayoutSVGText always.
  // See layout_selection.cc SetShouldInvalidateIfNeeded for more detail.
  if (object_.IsSVGText())
    return PaintInvalidationReason::kSelection;
  auto invalidation_rect = UnionRect(new_selection_rect, old_selection_rect);
  if (invalidation_rect.IsEmpty())
    return reason;

  object_.GetMutableForPainting().SetPartialInvalidationVisualRect(
      UnionRect(object_.PartialInvalidationVisualRect(), invalidation_rect));
  return PaintInvalidationReason::kSelection;
}

DISABLE_CFI_PERF
PaintInvalidationReason
ObjectPaintInvalidatorWithContext::InvalidatePartialRect(
    PaintInvalidationReason reason) {
  if (IsFullPaintInvalidationReason(reason))
    return reason;

  PhysicalRect local_rect = object_.PartialInvalidationLocalRect();
  if (local_rect.IsEmpty())
    return reason;

  auto visual_rect = context_.MapLocalRectToVisualRect(object_, local_rect);
  if (visual_rect.IsEmpty())
    return reason;

  object_.GetMutableForPainting().SetPartialInvalidationVisualRect(
      UnionRect(object_.PartialInvalidationVisualRect(), visual_rect));

  return PaintInvalidationReason::kRectangle;
}

DISABLE_CFI_PERF
void ObjectPaintInvalidatorWithContext::InvalidatePaintWithComputedReason(
    PaintInvalidationReason reason) {
  DCHECK(!(context_.subtree_flags &
           PaintInvalidatorContext::kSubtreeNoInvalidation));

  // InvalidatePartialRect is before InvalidateSelection because the latter will
  // accumulate selection visual rects to the partial rect mapped in the former.
  reason = InvalidatePartialRect(reason);
  reason = InvalidateSelection(reason);
  if (reason == PaintInvalidationReason::kNone)
    return;

  context_.painting_layer->SetNeedsRepaint();
  object_.InvalidateDisplayItemClients(reason);
}

}  // namespace blink
