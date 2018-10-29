// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_updater.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

static const LayoutBoxModelObject* ClippingContainerFromClipChainParent(
    const PaintLayer* clip_chain_parent) {
  return clip_chain_parent->GetLayoutObject().HasClipRelatedProperty()
             ? &clip_chain_parent->GetLayoutObject()
             : clip_chain_parent->ClippingContainer();
}

CompositingInputsUpdater::CompositingInputsUpdater(
    PaintLayer* root_layer,
    CompositingReasonFinder& compositing_reason_finder)
    : geometry_map_(kUseTransforms),
      root_layer_(root_layer),
      compositing_reason_finder_(compositing_reason_finder) {}

CompositingInputsUpdater::~CompositingInputsUpdater() = default;

void CompositingInputsUpdater::Update() {
  TRACE_EVENT0("blink", "CompositingInputsUpdater::update");
  UpdateRecursive(root_layer_, kDoNotForceUpdate, AncestorInfo());
}

void CompositingInputsUpdater::UpdateRecursive(PaintLayer* layer,
                                               UpdateType update_type,
                                               AncestorInfo info) {
  LayoutBoxModelObject& layout_object = layer->GetLayoutObject();
  const ComputedStyle& style = layout_object.StyleRef();

  const PaintLayer* previous_overflow_layer = layer->AncestorOverflowLayer();
  layer->UpdateAncestorOverflowLayer(info.last_overflow_clip_layer);
  if (info.last_overflow_clip_layer && layer->NeedsCompositingInputsUpdate() &&
      style.HasStickyConstrainedPosition()) {
    if (info.last_overflow_clip_layer != previous_overflow_layer) {
      // Old ancestor scroller should no longer have these constraints.
      DCHECK(!previous_overflow_layer ||
             !previous_overflow_layer->GetScrollableArea() ||
             !previous_overflow_layer->GetScrollableArea()
                  ->GetStickyConstraintsMap()
                  .Contains(layer));

      // If our ancestor scroller has changed and the previous one was the
      // root layer, we are no longer viewport constrained.
      if (previous_overflow_layer && previous_overflow_layer->IsRootLayer()) {
        layout_object.View()->GetFrameView()->RemoveViewportConstrainedObject(
            layout_object);
      }
    }

    if (info.last_overflow_clip_layer->IsRootLayer()) {
      layout_object.View()->GetFrameView()->AddViewportConstrainedObject(
          layout_object);
    }
    layout_object.UpdateStickyPositionConstraints();

    // Sticky position constraints and ancestor overflow scroller affect
    // the sticky layer position, so we need to update it again here.
    // TODO(flackr): This should be refactored in the future to be clearer
    // (i.e. update layer position and ancestor inputs updates in the
    // same walk)
    layer->UpdateLayerPosition();
  }

  geometry_map_.PushMappingsToAncestor(layer, layer->Parent());

  PaintLayer* enclosing_composited_layer = info.enclosing_composited_layer;
  PaintLayer* enclosing_squashing_composited_layer =
      info.enclosing_squashing_composited_layer;
  switch (layer->GetCompositingState()) {
    case kNotComposited:
      break;
    case kPaintsIntoOwnBacking:
      enclosing_composited_layer = layer;
      break;
    case kPaintsIntoGroupedBacking:
      enclosing_squashing_composited_layer =
          &layer->GroupedMapping()->OwningLayer();
      break;
  }

  if (layer->NeedsCompositingInputsUpdate()) {
    if (enclosing_composited_layer) {
      enclosing_composited_layer->GetCompositedLayerMapping()
          ->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);
    }
    if (enclosing_squashing_composited_layer) {
      enclosing_squashing_composited_layer->GetCompositedLayerMapping()
          ->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);
    }

    update_type = kForceUpdate;
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

  if (update_type == kForceUpdate)
    UpdateAncestorDependentCompositingInputs(layer, info);

  info.enclosing_composited_layer = enclosing_composited_layer;
  info.enclosing_squashing_composited_layer =
      enclosing_squashing_composited_layer;

  if (layer->IsRootLayer() || layout_object.HasOverflowClip())
    info.last_overflow_clip_layer = layer;

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
      !layout_object.IsLayoutView()) {
    info.clip_chain_parent_for_fixed = layer;
    info.escape_clip_to_for_fixed = info.escape_clip_to;
    info.scrolling_ancestor_for_fixed = info.scrolling_ancestor;
    info.needs_reparent_scroll_for_fixed = info.needs_reparent_scroll;
  }
  if (layout_object.IsLayoutView())
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

  if (style.IsStackingContext()) {
    info.escape_clip_to = nullptr;
    const LayoutBoxModelObject* clipping_container =
        ClippingContainerFromClipChainParent(layer);
    info.escape_clip_to_for_absolute =
        ClippingContainerFromClipChainParent(
            info.clip_chain_parent_for_absolute) != clipping_container
            ? info.clip_chain_parent_for_absolute
            : nullptr;
    info.escape_clip_to_for_fixed =
        ClippingContainerFromClipChainParent(
            info.clip_chain_parent_for_fixed) != clipping_container
            ? info.clip_chain_parent_for_fixed
            : nullptr;
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
    if (info.escape_clip_to_for_absolute && style.ZIndex() < 0 &&
        !info.escape_clip_to_for_absolute->GetLayoutObject()
             .StyleRef()
             .IsStackingContext())
      info.escape_clip_to_for_absolute = nullptr;
    if (info.escape_clip_to_for_fixed && style.ZIndex() < 0 &&
        !info.escape_clip_to_for_fixed->GetLayoutObject()
             .StyleRef()
             .IsStackingContext())
      info.escape_clip_to_for_fixed = nullptr;

    info.needs_reparent_scroll = info.needs_reparent_scroll_for_absolute =
        info.needs_reparent_scroll_for_fixed = false;
  }

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
  bool ignore_lcd_text =
      (layer->AncestorScrollingLayer() &&
       !layer->AncestorScrollingLayer()->IsRootLayer() &&
       layer->AncestorScrollingLayer()->NeedsCompositedScrolling());

  layer->SetPotentialCompositingReasonsFromNonStyle(
      compositing_reason_finder_.NonStyleDeterminedDirectReasons(
          layer, ignore_lcd_text));

  if (layer->GetScrollableArea()) {
    layer->GetScrollableArea()->UpdateNeedsCompositedScrolling(
        compositor->CanBeComposited(layer) &&
        layer->DirectCompositingReasons());
  }

  bool should_recurse =
      layer->ChildNeedsCompositingInputsUpdate() || update_type == kForceUpdate;

  layer->SetDescendantHasDirectOrScrollingCompositingReason(false);
  bool descendant_has_direct_compositing_reason = false;
  for (PaintLayer* child = layer->FirstChild(); child;
       child = child->NextSibling()) {
    if (should_recurse)
      UpdateRecursive(child, update_type, info);
    descendant_has_direct_compositing_reason |=
        child->DescendantHasDirectOrScrollingCompositingReason() ||
        child->NeedsCompositedScrolling() ||
        (compositor->CanBeComposited(child) &&
         child->DirectCompositingReasons());
  }
  layer->SetDescendantHasDirectOrScrollingCompositingReason(
      descendant_has_direct_compositing_reason);

  if (layer->IsRootLayer() && layer->ScrollsOverflow() &&
      layer->DescendantHasDirectOrScrollingCompositingReason() &&
      !layer->NeedsCompositedScrolling())
    layer->GetScrollableArea()->UpdateNeedsCompositedScrolling(true);

  layer->ClearChildNeedsCompositingInputsUpdate();

  geometry_map_.PopMappingsToAncestor(layer->Parent());

  if (layer->SelfPaintingStatusChanged()) {
    layer->ClearSelfPaintingStatusChanged();
    // If the floating object becomes non-self-painting, so some ancestor should
    // paint it; if it becomes self-painting, it should paint itself and no
    // ancestor should paint it.
    if (layout_object.IsFloating()) {
      LayoutBlockFlow::UpdateAncestorShouldPaintFloatingObject(
          *layer->GetLayoutBox());
    }
  }
}

void CompositingInputsUpdater::UpdateAncestorDependentCompositingInputs(
    PaintLayer* layer,
    const AncestorInfo& info) {
  if (layer->IsRootLayer()) {
    layer->UpdateAncestorDependentCompositingInputs(
        PaintLayer::AncestorDependentCompositingInputs());
    return;
  }

  PaintLayer::AncestorDependentCompositingInputs properties;
  LayoutBoxModelObject& layout_object = layer->GetLayoutObject();

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // The final value for |unclipped_absolute_bounding_box| needs to be
    // in absolute, unscrolled space, without any scroll applied.
    properties.unclipped_absolute_bounding_box =
        EnclosingIntRect(geometry_map_.AbsoluteRect(
            FloatRect(layer->BoundingBoxForCompositingOverlapTest())));

    bool affected_by_scroll = root_layer_->GetScrollableArea() &&
                              layer->IsAffectedByScrollOf(root_layer_);

    // At ths point, |unclipped_absolute_bounding_box| is in viewport space.
    // To convert to absolute space, add scroll offset for non-fixed layers.
    if (affected_by_scroll) {
      properties.unclipped_absolute_bounding_box.Move(
          RoundedIntSize(root_layer_->GetScrollableArea()->GetScrollOffset()));
    }

    ClipRect clip_rect;
    layer->Clipper(PaintLayer::kDoNotUseGeometryMapper)
        .CalculateBackgroundClipRect(
            ClipRectsContext(root_layer_,
                             &root_layer_->GetLayoutObject().FirstFragment(),
                             kAbsoluteClipRectsIgnoringViewportClip,
                             kIgnorePlatformOverlayScrollbarSize,
                             kIgnoreOverflowClipAndScroll),
            clip_rect);
    IntRect snapped_clip_rect = PixelSnappedIntRect(clip_rect.Rect());
    // |snapped_clip_rect| is in absolute space space, but with scroll applied.
    // To convert to absolute, unscrolled space, subtract scroll offsets for
    // fixed layers.
    if (root_layer_->GetScrollableArea() && !affected_by_scroll) {
      snapped_clip_rect.Move(
          RoundedIntSize(-root_layer_->GetScrollableArea()->GetScrollOffset()));
    }

    properties.clipped_absolute_bounding_box =
        properties.unclipped_absolute_bounding_box;
    properties.clipped_absolute_bounding_box.Intersect(snapped_clip_rect);
  }

  const PaintLayer* parent = layer->Parent();
  properties.opacity_ancestor =
      parent->IsTransparent() ? parent : parent->OpacityAncestor();
  properties.transform_ancestor =
      parent->Transform() ? parent : parent->TransformAncestor();
  properties.filter_ancestor =
      parent->HasFilterInducingProperty() ? parent : parent->FilterAncestor();
  properties.clip_path_ancestor = parent->GetLayoutObject().HasClipPath()
                                      ? parent
                                      : parent->ClipPathAncestor();
  properties.mask_ancestor =
      parent->GetLayoutObject().HasMask() ? parent : parent->MaskAncestor();

  EPosition position = layout_object.StyleRef().GetPosition();
  properties.nearest_fixed_position_layer =
      position == EPosition::kFixed ? layer
                                    : parent->NearestFixedPositionLayer();

  PaintLayer* clip_chain_parent = layer->Parent();
  if (position == EPosition::kAbsolute)
    clip_chain_parent = info.clip_chain_parent_for_absolute;
  else if (position == EPosition::kFixed)
    clip_chain_parent = info.clip_chain_parent_for_fixed;
  properties.clipping_container =
      ClippingContainerFromClipChainParent(clip_chain_parent);
  properties.clip_parent = info.escape_clip_to;

  properties.ancestor_scrolling_layer = info.scrolling_ancestor;
  if (info.needs_reparent_scroll && layout_object.StyleRef().IsStacked())
    properties.scroll_parent = info.scrolling_ancestor;

  layer->UpdateAncestorDependentCompositingInputs(properties);
}

#if DCHECK_IS_ON()

void CompositingInputsUpdater::AssertNeedsCompositingInputsUpdateBitsCleared(
    PaintLayer* layer) {
  DCHECK(!layer->ChildNeedsCompositingInputsUpdate());
  DCHECK(!layer->NeedsCompositingInputsUpdate());

  for (PaintLayer* child = layer->FirstChild(); child;
       child = child->NextSibling())
    AssertNeedsCompositingInputsUpdateBitsCleared(child);
}

#endif

}  // namespace blink
