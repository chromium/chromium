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
         (!object.IsStackingContext() ||
          MayBeSkippedContainerForFloating(object)));

  LayoutObject* descendant = object.NextInPreOrder(&object);
  while (descendant) {
    if (!descendant->HasLayer() || !descendant->IsStacked()) {
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
    } else if (descendant->IsStackingContext() &&
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
    } else if (descendant->IsStackingContext() &&
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
      To<LayoutBoxModelObject>(object).HasSelfPaintingLayer()) {
    To<LayoutBoxModelObject>(object).Layer()->SetNeedsRepaint();
  } else if (object.IsFloating() && object.Parent() &&
             MayBeSkippedContainerForFloating(*object.Parent())) {
    // The following is for legacy layout only because LayoutNG allows an
    // inline to contain floats.
    object.PaintingLayer()->SetNeedsRepaint();
  }
}

void ObjectPaintInvalidator::
    InvalidatePaintIncludingNonCompositingDescendants() {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  SlowSetPaintingLayerNeedsRepaint();
  // This method may be used to invalidate paint of objects changing paint
  // invalidation container.
  // TODO(vmpstr): After paint containment isolation is in place, we might not
  // have to recurse past the paint containment boundary.
  TraverseNonCompositingDescendantsInPaintOrder(
      object_, [](const LayoutObject& object) {
        SetPaintingLayerNeedsRepaintDuringTraverse(object);
      });
}

#if DCHECK_IS_ON()
void ObjectPaintInvalidator::CheckPaintLayerNeedsRepaint() {
  DCHECK(!object_.PaintingLayer() ||
         object_.PaintingLayer()->SelfNeedsRepaint());
}
#endif

void ObjectPaintInvalidator::SlowSetPaintingLayerNeedsRepaint() {
  if (PaintLayer* painting_layer = object_.PaintingLayer())
    painting_layer->SetNeedsRepaint();
}

DISABLE_CFI_PERF
PaintInvalidationReason
ObjectPaintInvalidatorWithContext::ComputePaintInvalidationReason() {
  // This is before any early return to ensure the previous visibility status is
  // saved.
  bool previous_visibility_visible = object_.PreviousVisibilityVisible();
  object_.GetMutableForPainting().UpdatePreviousVisibilityVisible();
  if (object_.VisualRectRespectsVisibility() && !previous_visibility_visible &&
      object_.StyleRef().Visibility() != EVisibility::kVisible)
    return PaintInvalidationReason::kNone;

  if (!object_.ShouldCheckForPaintInvalidation() && !context_.subtree_flags) {
    // No paint invalidation flag. No paint invalidation is needed.
    return PaintInvalidationReason::kNone;
  }

  if (context_.subtree_flags &
      PaintInvalidatorContext::kSubtreeFullInvalidation)
    return PaintInvalidationReason::kSubtree;

  if (object_.ShouldDoFullPaintInvalidation())
    return object_.FullPaintInvalidationReason();

  if (context_.fragment_data->PaintOffset() != context_.old_paint_offset)
    return PaintInvalidationReason::kGeometry;

  if (object_.GetDocument().InForcedColorsMode() && object_.IsLayoutBlockFlow())
    return PaintInvalidationReason::kBackplate;

  // Force full paint invalidation if the object has background-clip:text to
  // update the background on any change in the subtree.
  if (object_.StyleRef().BackgroundClip() == EFillBox::kText)
    return PaintInvalidationReason::kBackground;

  // Incremental invalidation is only applicable to LayoutBoxes. Return
  // kIncremental. BoxPaintInvalidator may override this reason with a full
  // paint invalidation reason if needed.
  if (object_.IsBox())
    return PaintInvalidationReason::kIncremental;

  return PaintInvalidationReason::kNone;
}

DISABLE_CFI_PERF
void ObjectPaintInvalidatorWithContext::InvalidatePaintWithComputedReason(
    PaintInvalidationReason reason) {
  DCHECK(!(context_.subtree_flags &
           PaintInvalidatorContext::kSubtreeNoInvalidation));

  if (reason == PaintInvalidationReason::kNone) {
    if (!object_.ShouldInvalidateSelection())
      return;
    // See layout_selection.cc SetShouldInvalidateIfNeeded() for the reason
    // for the IsSVGText() condition here.
    if (!object_.CanBeSelectionLeaf() && !object_.IsSVGText())
      return;

    reason = PaintInvalidationReason::kSelection;
    if (const auto* selection_client =
            object_.GetSelectionDisplayItemClient()) {
      // Invalidate the selection display item client only.
      context_.painting_layer->SetNeedsRepaint();
      selection_client->Invalidate(reason);
      return;
    }
  }

  context_.painting_layer->SetNeedsRepaint();
  object_.InvalidateDisplayItemClients(reason);
}

}  // namespace blink
