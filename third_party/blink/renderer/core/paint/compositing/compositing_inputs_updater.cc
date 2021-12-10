// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_updater.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

CompositingInputsUpdater::CompositingInputsUpdater(
    PaintLayer* root_layer,
    PaintLayer* compositing_inputs_root)
    : root_layer_(root_layer),
      compositing_inputs_root_(compositing_inputs_root) {
}

CompositingInputsUpdater::~CompositingInputsUpdater() = default;

bool CompositingInputsUpdater::LayerOrDescendantShouldBeComposited(
    PaintLayer* layer) {
  if (auto* layout_view = DynamicTo<LayoutView>(layer->GetLayoutObject())) {
    if (layout_view->GetFrameView()->ShouldThrottleRendering()) {
      if (auto* inner_compositor = layout_view->Compositor())
        return inner_compositor->StaleInCompositingMode();
      return false;
    }
  }
  DCHECK(!layer->GetLayoutObject().GetFrameView()->ShouldThrottleRendering());
  return layer->DescendantHasDirectOrScrollingCompositingReason() ||
         layer->NeedsCompositedScrolling() ||
         (layer->CanBeComposited() && layer->DirectCompositingReasons());
}

void CompositingInputsUpdater::Update() {
  TRACE_EVENT0("blink", "CompositingInputsUpdater::update");

  AncestorInfo info;
  UpdateType update_type = kDoNotForceUpdate;
  PaintLayer* layer =
      compositing_inputs_root_ ? compositing_inputs_root_ : root_layer_;

  // We don't need to do anything if the layer is under a locked display lock
  // that prevents updates.
  if (DisplayLockUtilities::LockedAncestorPreventingPrePaint(
          layer->GetLayoutObject())) {
    return;
  }

  ApplyAncestorInfoToSelfAndAncestorsRecursively(layer, update_type, info);
  UpdateSelfAndDescendantsRecursively(layer, update_type, info);

  if (LayerOrDescendantShouldBeComposited(layer)) {
    // Update all parent layers
    PaintLayer* parent_layer = layer->Parent();
    while (parent_layer &&
           !parent_layer->DescendantHasDirectOrScrollingCompositingReason()) {
      parent_layer->SetDescendantHasDirectOrScrollingCompositingReason(true);
      parent_layer = parent_layer->Parent();
    }
  }
}

void CompositingInputsUpdater::ApplyAncestorInfoToSelfAndAncestorsRecursively(
    PaintLayer* layer,
    UpdateType& update_type,
    AncestorInfo& info) {
  if (!layer)
    return;

  // We first recursively call ApplyAncestorInfoToSelfAndAncestorsRecursively()
  // to ensure that we start to compute AncestorInfo from the root layer (as we
  // need to do a top-down tree walk to incrementally update this information).
  ApplyAncestorInfoToSelfAndAncestorsRecursively(layer->Parent(), update_type,
                                                 info);
  UpdateAncestorInfo(layer, update_type, info);
}

void CompositingInputsUpdater::UpdateSelfAndDescendantsRecursively(
    PaintLayer* layer,
    UpdateType update_type,
    AncestorInfo info) {
  // UpdateAncestorInfo has been already computed in ApplyAncestorInfo() for
  // layers from root_layer_ down to compositing_inputs_root_ both included.
  if (layer != root_layer_ && layer != compositing_inputs_root_)
    UpdateAncestorInfo(layer, update_type, info);

  PaintLayerCompositor* compositor =
      layer->GetLayoutObject().View()->Compositor();

  // The sequence of updates to compositing triggers goes like this:
  // 1. Apply all triggers from kComboAllDirectNonStyleDeterminedReasons for
  //    |layer|. This may depend on ancestor composited scrolling (i.e. step
  //    2 for an ancestor PaintLayer).
  // 2. Put |layer| in composited scrolling mode if needed.
  // 3. Reset DescendantHasDirectCompositingReason to false for |layer|.
  // 4. Recurse into child PaintLayers.
  // 5. Set DescendantHasDirectCompositingReason to true if it was for any
  //    child.
  // 6. If |layer| is the root, composite if
  //    DescendantHasDirectCompositingReason is true for |layer|.

  layer->SetPotentialCompositingReasonsFromNonStyle(
      CompositingReasonFinder::NonStyleDeterminedDirectReasons(*layer));

  if (layer->GetScrollableArea()) {
    layer->GetScrollableArea()->UpdateNeedsCompositedScrolling(
        layer->CanBeComposited() && layer->DirectCompositingReasons());
  }

  // Note that prepaint may use the compositing information, so only skip
  // recursing it if we're skipping prepaint.
  bool recursion_blocked_by_display_lock =
      layer->GetLayoutObject().ChildPrePaintBlockedByDisplayLock();

  bool should_recurse = update_type == kForceUpdate;

  layer->SetDescendantHasDirectOrScrollingCompositingReason(false);
  bool descendant_has_direct_compositing_reason = false;

  auto* first_child =
      recursion_blocked_by_display_lock ? nullptr : layer->FirstChild();
  for (PaintLayer* child = first_child; child; child = child->NextSibling()) {
    if (should_recurse)
      UpdateSelfAndDescendantsRecursively(child, update_type, info);
    descendant_has_direct_compositing_reason |=
        LayerOrDescendantShouldBeComposited(child);
  }
  if (!descendant_has_direct_compositing_reason &&
      layer->GetLayoutObject().IsLayoutEmbeddedContent()) {
    if (LayoutView* embedded_layout_view =
            To<LayoutEmbeddedContent>(layer->GetLayoutObject())
                .ChildLayoutView()) {
      descendant_has_direct_compositing_reason |=
          LayerOrDescendantShouldBeComposited(embedded_layout_view->Layer());
    }
  }
  layer->SetDescendantHasDirectOrScrollingCompositingReason(
      descendant_has_direct_compositing_reason);

  if ((layer->IsRootLayer() || layer->NeedsReorderOverlayOverflowControls()) &&
      layer->ScrollsOverflow() &&
      layer->DescendantHasDirectOrScrollingCompositingReason() &&
      !layer->NeedsCompositedScrolling())
    layer->GetScrollableArea()->UpdateNeedsCompositedScrolling(true);

  compositor->ClearCompositingInputsRoot();
}

bool CompositingInputsUpdater::NeedsPaintOffsetTranslationForCompositing(
    PaintLayer* layer) {
  /// Allocate when the developer indicated compositing via a direct
  // method.
  if ((layer->CanBeComposited() && layer->DirectCompositingReasons()) ||
      layer->NeedsCompositedScrolling())
    return true;

  // Allocate when there is a need for a cc effect that applies to
  // descendants.
  // TODO(chrishtr): this should not be necessary, but currently at least
  // cc mask layers don't apply correctly otherwise.
  // compositing/clip-path-with-composited-descendants.html is one test
  // that demonstrates this.
  if ((layer->PotentialCompositingReasonsFromStyle() &
       CompositingReason::kComboCompositedDescendants) &&
      layer->DescendantHasDirectOrScrollingCompositingReason())
    return true;

  return false;
}

void CompositingInputsUpdater::UpdateAncestorInfo(PaintLayer* const layer,
                                                  UpdateType& update_type,
                                                  AncestorInfo& info) {
  LayoutBoxModelObject& layout_object = layer->GetLayoutObject();
  const ComputedStyle& style = layout_object.StyleRef();

  PaintLayer* enclosing_stacking_composited_layer =
      info.enclosing_stacking_composited_layer;
  PaintLayer* enclosing_squashing_composited_layer =
      info.enclosing_squashing_composited_layer;

  switch (layer->GetCompositingState()) {
    case kNotComposited:
      break;
    case kPaintsIntoOwnBacking:
      if (layout_object.IsStackingContext())
        enclosing_stacking_composited_layer = layer;
      break;
    case kPaintsIntoGroupedBacking:
      enclosing_squashing_composited_layer =
          &layer->GroupedMapping()->OwningLayer();
      break;
  }

  if (style.GetPosition() == EPosition::kAbsolute) {
    info.escape_clip_to = info.escape_clip_to_for_absolute;
    info.scrolling_ancestor = info.scrolling_ancestor_for_absolute;
    info.needs_reparent_scroll = info.needs_reparent_scroll_for_absolute;
  } else if (style.GetPosition() == EPosition::kFixed) {
    info.escape_clip_to = info.escape_clip_to_for_fixed;
    info.scrolling_ancestor = info.scrolling_ancestor_for_fixed;
    info.needs_reparent_scroll = info.needs_reparent_scroll_for_fixed;
  }

  if (layout_object.ShouldApplyLayoutContainment())
    info.nearest_contained_layout_layer = layer;

  info.enclosing_stacking_composited_layer =
      enclosing_stacking_composited_layer;
  info.enclosing_squashing_composited_layer =
      enclosing_squashing_composited_layer;

  // Handles sibling scroll problem, i.e. a non-stacking context scroller
  // needs to propagate scroll to its descendants that are siblings in
  // paint order. For example:
  // <div style="overflow:scroll;">
  //   <div style="position:relative;">Paint sibling.</div>
  // </div>
  if (layer->ScrollsOverflow()) {
    info.scrolling_ancestor = layer;
    info.needs_reparent_scroll = true;
  }
  if (layout_object.CanContainAbsolutePositionObjects()) {
    info.clip_chain_parent_for_absolute = layer;
    info.escape_clip_to_for_absolute = info.escape_clip_to;
    info.scrolling_ancestor_for_absolute = info.scrolling_ancestor;
    info.needs_reparent_scroll_for_absolute = info.needs_reparent_scroll;
  }

  // LayoutView isn't really the containing block for fixed-pos descendants
  // in the sense that they don't scroll along with its in-flow contents.
  // However LayoutView does clip them.
  if (layout_object.CanContainFixedPositionObjects() &&
      !IsA<LayoutView>(layout_object)) {
    info.clip_chain_parent_for_fixed = layer;
    info.escape_clip_to_for_fixed = info.escape_clip_to;
    info.scrolling_ancestor_for_fixed = info.scrolling_ancestor;
    info.needs_reparent_scroll_for_fixed = info.needs_reparent_scroll;
  }
  if (IsA<LayoutView>(layout_object))
    info.clip_chain_parent_for_fixed = layer;

  // CSS clip affects all descendants, not just containing-block descendants.
  // We don't have to set clip_chain_parent_for_absolute here because CSS clip
  // requires position:absolute, so the element must contain absolute-positioned
  // descendants.
  // However it is incorrect to let fixed-positioned descendants to inherit the
  // clip state from this element either, because the overflow clip and the
  // inherited clip of the current element shouldn't apply to them if the
  // current element is not a fixed-pos container. This is a known bug but too
  // difficult to fix in SPv1 compositing.
  if (layout_object.HasClip())
    info.clip_chain_parent_for_fixed = layer;

  if (layout_object.IsStackingContext()) {
    info.escape_clip_to = nullptr;
    info.escape_clip_to_for_absolute = nullptr;
    info.escape_clip_to_for_fixed = nullptr;
    // Workaround crbug.com/817175
    // We can't escape clip to a layer that paints after us, because in SPv1
    // cc needs to reverse engineer clip tree from the layer tree, and we
    // can't refer to a clip node that hasn't been built yet.
    // This will result in wrong clip in some rare cases, for example:
    // <div style="display:grid;">
    //   <div style="z-index:-1; overflow:hidden;">
    //     <div style="position:absolute;"></div>
    //   </div>
    // </div>
    if (info.escape_clip_to_for_absolute && style.EffectiveZIndex() < 0 &&
        !info.escape_clip_to_for_absolute->GetLayoutObject()
             .IsStackingContext())
      info.escape_clip_to_for_absolute = nullptr;
    if (info.escape_clip_to_for_fixed && style.EffectiveZIndex() < 0 &&
        !info.escape_clip_to_for_fixed->GetLayoutObject().IsStackingContext())
      info.escape_clip_to_for_fixed = nullptr;

    info.needs_reparent_scroll = info.needs_reparent_scroll_for_absolute =
        info.needs_reparent_scroll_for_fixed = false;
  }

  if (layout_object.IsStickyPositioned())
    info.is_under_position_sticky = true;
}

}  // namespace blink
