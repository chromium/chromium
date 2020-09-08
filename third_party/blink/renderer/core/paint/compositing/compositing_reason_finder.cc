// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

CompositingReasons CompositingReasonFinder::DirectReasons(
    const PaintLayer& layer) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return CompositingReason::kNone;

  DCHECK_EQ(PotentialCompositingReasonsFromStyle(layer.GetLayoutObject()),
            layer.PotentialCompositingReasonsFromStyle());
  CompositingReasons style_determined_direct_compositing_reasons =
      layer.PotentialCompositingReasonsFromStyle() &
      CompositingReason::kComboAllDirectStyleDeterminedReasons;

  return style_determined_direct_compositing_reasons |
         NonStyleDeterminedDirectReasons(layer);
}

CompositingReasons
CompositingReasonFinder::PotentialCompositingReasonsFromStyle(
    const LayoutObject& layout_object) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return CompositingReason::kNone;

  CompositingReasons reasons = CompositingReason::kNone;

  const ComputedStyle& style = layout_object.StyleRef();

  reasons |= CompositingReasonsFor3DTransform(layout_object);

  if (style.BackfaceVisibility() == EBackfaceVisibility::kHidden)
    reasons |= CompositingReason::kBackfaceVisibilityHidden;

  reasons |= CompositingReasonsForAnimation(layout_object);
  reasons |= CompositingReasonsForWillChange(style);

  // If the implementation of CreatesGroup changes, we need to be aware of that
  // in this part of code.
  DCHECK((style.HasNonInitialOpacity() || layout_object.HasMask() ||
          layout_object.HasClipPath() ||
          layout_object.HasFilterInducingProperty() ||
          layout_object.HasNonInitialBackdropFilter() || style.HasBlendMode() ||
          (!style.HasAutoClip() && style.HasOutOfFlowPosition()) ||
          style.HasIsolation()) == layout_object.CreatesGroup());

  if (style.HasMask() || style.ClipPath())
    reasons |= CompositingReason::kMaskWithCompositedDescendants;

  if (style.HasFilterInducingProperty())
    reasons |= CompositingReason::kFilterWithCompositedDescendants;

  if (style.HasBackdropFilter())
    reasons |= CompositingReason::kBackdropFilter;

  if (style.HasOpacity())
    reasons |= CompositingReason::kOpacityWithCompositedDescendants;

  if (style.HasBlendMode())
    reasons |= CompositingReason::kBlendingWithCompositedDescendants;

  if (layout_object.HasReflection())
    reasons |= CompositingReason::kReflectionWithCompositedDescendants;

  DCHECK(!(reasons & ~CompositingReason::kComboAllStyleDeterminedReasons));
  return reasons;
}

static bool ShouldPreferCompositingForLayoutView(
    const LayoutView& layout_view) {
  auto has_direct_compositing_reasons = [](const LayoutObject* object) -> bool {
    return object && CompositingReasonFinder::DirectReasonsForPaintProperties(
                         *object) != CompositingReason::kNone;
  };
  if (has_direct_compositing_reasons(
          layout_view.GetFrame()->OwnerLayoutObject()))
    return true;
  if (auto* document_element = layout_view.GetDocument().documentElement()) {
    if (has_direct_compositing_reasons(document_element->GetLayoutObject()))
      return true;
  }
  if (auto* body = layout_view.GetDocument().FirstBodyElement()) {
    if (has_direct_compositing_reasons(body->GetLayoutObject()))
      return true;
  }
  return false;
}

static CompositingReasons BackfaceInvisibility3DAncestorReason(
    const PaintLayer& layer) {
  if (RuntimeEnabledFeatures::TransformInteropEnabled()) {
    if (auto* compositing_container = layer.CompositingContainer()) {
      if (compositing_container->GetLayoutObject()
              .StyleRef()
              .BackfaceVisibility() == EBackfaceVisibility::kHidden)
        return CompositingReason::kBackfaceInvisibility3DAncestor;
    }
  }
  return CompositingReason::kNone;
}

CompositingReasons CompositingReasonFinder::DirectReasonsForPaintProperties(
    const LayoutObject& object) {
  // TODO(wangxianzhu): Don't depend on PaintLayer for CompositeAfterPaint.
  if (!object.HasLayer()) {
    if (object.IsSVGChild())
      return DirectReasonsForSVGChildPaintProperties(object);
    return CompositingReason::kNone;
  }

  const ComputedStyle& style = object.StyleRef();
  auto reasons = CompositingReasonsForAnimation(object) |
                 CompositingReasonsForWillChange(style);

  reasons |= CompositingReasonsFor3DTransform(object);

  auto* layer = ToLayoutBoxModelObject(object).Layer();
  if (layer->Has3DTransformedDescendant()) {
    // Perspective (specified either by perspective or transform properties)
    // with 3d descendants need a render surface for flattening purposes.
    if (style.HasPerspective() || style.Transform().HasPerspective())
      reasons |= CompositingReason::kPerspectiveWith3DDescendants;
    if (style.Preserves3D())
      reasons |= CompositingReason::kPreserve3DWith3DDescendants;
  }

  if (RequiresCompositingForRootScroller(*layer))
    reasons |= CompositingReason::kRootScroller;

  if (RequiresCompositingForScrollDependentPosition(*layer))
    reasons |= CompositingReason::kScrollDependentPosition;

  if (style.HasBackdropFilter())
    reasons |= CompositingReason::kBackdropFilter;

  if (auto* scrollable_area = layer->GetScrollableArea()) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      bool force_prefer_compositing_to_lcd_text =
          reasons != CompositingReason::kNone ||
          // In CompositeAfterPaint though we don't treat hidden backface as
          // a direct compositing reason, it's very likely that the object
          // will be composited, and it also indicates preference of
          // compositing, so we prefer composited scrolling here.
          style.BackfaceVisibility() == EBackfaceVisibility::kHidden ||
          (object.IsLayoutView() &&
           ShouldPreferCompositingForLayoutView(To<LayoutView>(object)));

      if (scrollable_area->ComputeNeedsCompositedScrolling(
              force_prefer_compositing_to_lcd_text)) {
        reasons |= CompositingReason::kOverflowScrolling;
      }
    } else if (scrollable_area->NeedsCompositedScrolling()) {
      // For pre-CompositeAfterPaint, just let |reasons| reflect the current
      // composited scrolling status.
      reasons |= CompositingReason::kOverflowScrolling;
    }
  }

  reasons |= BackfaceInvisibility3DAncestorReason(*layer);

  if (object.CanHaveAdditionalCompositingReasons())
    reasons |= object.AdditionalCompositingReasons();

  return reasons;
}

CompositingReasons
CompositingReasonFinder::DirectReasonsForSVGChildPaintProperties(
    const LayoutObject& object) {
  DCHECK(object.IsSVGChild());
  if (RuntimeEnabledFeatures::CompositeSVGEnabled()) {
    const ComputedStyle& style = object.StyleRef();
    auto reasons = CompositingReasonsForAnimation(object) |
                   CompositingReasonsForWillChange(style);
    if (style.HasBackdropFilter())
      reasons |= CompositingReason::kBackdropFilter;
    return reasons;
  }
  return CompositingReason::kNone;
}

CompositingReasons CompositingReasonFinder::CompositingReasonsFor3DTransform(
    const LayoutObject& layout_object) {
  // Note that we ask the layoutObject if it has a transform, because the
  // style may have transforms, but the layoutObject may be an inline that
  // doesn't support them.
  if (!layout_object.HasTransformRelatedProperty())
    return CompositingReason::kNone;

  // Don't composite "trivial" 3D transforms such as translateZ(0).
  if (Platform::Current()->IsLowEndDevice()) {
    return layout_object.StyleRef().HasNonTrivial3DTransformOperation()
               ? CompositingReason::k3DTransform
               : CompositingReason::kNone;
  }

  if (layout_object.StyleRef().Has3DTransformOperation()) {
    return layout_object.StyleRef().HasNonTrivial3DTransformOperation()
               ? CompositingReason::k3DTransform
               : CompositingReason::kTrivial3DTransform;
  }

  return CompositingReason::kNone;
}

CompositingReasons CompositingReasonFinder::NonStyleDeterminedDirectReasons(
    const PaintLayer& layer) {
  CompositingReasons direct_reasons = CompositingReason::kNone;
  LayoutObject& layout_object = layer.GetLayoutObject();

  // TODO(chrishtr): remove this hammer in favor of something more targeted.
  // See crbug.com/749349.
  if (layer.ClipParent() && layer.GetLayoutObject().IsOutOfFlowPositioned())
    direct_reasons |= CompositingReason::kOutOfFlowClipping;

  if (RequiresCompositingForRootScroller(layer))
    direct_reasons |= CompositingReason::kRootScroller;

  // Composite |layer| if it is inside of an ancestor scrolling layer, but
  // that scrolling layer is not on the stacking context ancestor chain of
  // |layer|. See the definition of the scrollParent property in Layer for
  // more detail.
  if (const PaintLayer* scrolling_ancestor = layer.AncestorScrollingLayer()) {
    if ((scrolling_ancestor->NeedsCompositedScrolling() ||
         // If this is true, we'll force scrolling_ancestor to use composited
         // scrolling because this layer is composited.
         scrolling_ancestor->NeedsReorderOverlayOverflowControls()) &&
        layer.ScrollParent()) {
      DCHECK(!scrolling_ancestor->GetLayoutObject()
                  .IsStackingContext());
      direct_reasons |= CompositingReason::kOverflowScrollingParent;
    }
  }

  if (RequiresCompositingForScrollDependentPosition(layer))
    direct_reasons |= CompositingReason::kScrollDependentPosition;

  // Video is special. It's the only PaintLayer type that can both have
  // PaintLayer children and whose children can't use its backing to render
  // into. These children (the controls) always need to be promoted into their
  // own layers to draw on top of the accelerated video.
  if (layer.CompositingContainer() &&
      IsA<LayoutVideo>(layer.CompositingContainer()->GetLayoutObject()))
    direct_reasons |= CompositingReason::kVideoOverlay;

  // Special case for immersive-ar DOM overlay mode, see also
  // PaintLayerCompositor::GetXrOverlayLayer()
  if (const Node* node = layer.GetLayoutObject().GetNode()) {
    if (node->IsElementNode() && node->GetDocument().IsXrOverlay() &&
        node == Fullscreen::FullscreenElementFrom(node->GetDocument())) {
      direct_reasons |= CompositingReason::kXrOverlay;
    }
  }

  if (layer.IsRootLayer() && layout_object.GetFrame()->IsLocalRoot())
    direct_reasons |= CompositingReason::kRoot;

  if (layout_object.CanHaveAdditionalCompositingReasons())
    direct_reasons |= layout_object.AdditionalCompositingReasons();

  direct_reasons |= BackfaceInvisibility3DAncestorReason(layer);

  DCHECK(
      !(direct_reasons & CompositingReason::kComboAllStyleDeterminedReasons));
  return direct_reasons;
}

CompositingReasons CompositingReasonFinder::CompositingReasonsForAnimation(
    const LayoutObject& object) {
  CompositingReasons reasons = CompositingReason::kNone;
  const auto& style = object.StyleRef();
  if (style.SubtreeWillChangeContents())
    return reasons;

  // Transforms don't apply on non-replaced inline elements.
  bool supports_composited_animations =
      object.IsBox() ||
      (RuntimeEnabledFeatures::CompositeSVGEnabled() && object.IsSVGChild());
  if (supports_composited_animations && style.HasCurrentTransformAnimation())
    reasons |= CompositingReason::kActiveTransformAnimation;
  if (style.HasCurrentOpacityAnimation())
    reasons |= CompositingReason::kActiveOpacityAnimation;
  if (style.HasCurrentFilterAnimation())
    reasons |= CompositingReason::kActiveFilterAnimation;
  if (style.HasCurrentBackdropFilterAnimation())
    reasons |= CompositingReason::kActiveBackdropFilterAnimation;
  return reasons;
}

CompositingReasons CompositingReasonFinder::CompositingReasonsForWillChange(
    const ComputedStyle& style) {
  CompositingReasons reasons = CompositingReason::kNone;
  if (style.SubtreeWillChangeContents())
    return reasons;

  if (style.HasWillChangeTransformHint())
    reasons |= CompositingReason::kWillChangeTransform;
  if (style.HasWillChangeOpacityHint())
    reasons |= CompositingReason::kWillChangeOpacity;
  if (style.HasWillChangeFilterHint())
    reasons |= CompositingReason::kWillChangeFilter;
  if (style.HasWillChangeBackdropFilterHint())
    reasons |= CompositingReason::kWillChangeBackdropFilter;

  // kWillChangeOther is needed only when none of the explicit kWillChange*
  // reasons are set.
  if (reasons == CompositingReason::kNone &&
      style.HasWillChangeCompositingHint())
    reasons |= CompositingReason::kWillChangeOther;

  return reasons;
}

bool CompositingReasonFinder::RequiresCompositingForRootScroller(
    const PaintLayer& layer) {
  // The root scroller needs composited scrolling layers even if it doesn't
  // actually have scrolling since CC has these assumptions baked in for the
  // viewport. Because this is only needed for CC, we can skip it if
  // compositing is not enabled.
  const auto& settings = *layer.GetLayoutObject().GetDocument().GetSettings();
  if (!settings.GetAcceleratedCompositingEnabled())
    return false;

  return layer.GetLayoutObject().IsGlobalRootScroller();
}

bool CompositingReasonFinder::RequiresCompositingForScrollDependentPosition(
    const PaintLayer& layer) {
  const auto& layout_object = layer.GetLayoutObject();
  if (!layout_object.StyleRef().HasViewportConstrainedPosition() &&
      !layout_object.StyleRef().HasStickyConstrainedPosition())
    return false;

  // Don't promote fixed position elements that are descendants of a non-view
  // container, e.g. transformed elements.  They will stay fixed wrt the
  // container rather than the enclosing frame.
  EPosition position = layout_object.StyleRef().GetPosition();
  if (position == EPosition::kFixed) {
    return layer.FixedToViewport() &&
           layout_object.GetFrameView()->LayoutViewport()->ScrollsOverflow();
  }
  DCHECK_EQ(position, EPosition::kSticky);

  // Don't promote sticky position elements that cannot move with scrolls.
  if (!layer.SticksToScroller())
    return false;
  return layer.AncestorOverflowLayer()->ScrollsOverflow();
}

}  // namespace blink
