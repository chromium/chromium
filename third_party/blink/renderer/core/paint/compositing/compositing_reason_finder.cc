// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

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
  reasons |= CompositingReasonsFor3DSceneLeaf(layout_object);

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

  if (style.HasMask() || style.HasClipPath())
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
    return object &&
           CompositingReasonFinder::
                   DirectReasonsForPaintPropertiesExceptScrolling(*object) !=
               CompositingReason::kNone;
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
  if (RuntimeEnabledFeatures::BackfaceVisibilityInteropEnabled()) {
    if (auto* compositing_container = layer.CompositingContainer()) {
      if (compositing_container->GetLayoutObject()
              .StyleRef()
              .BackfaceVisibility() == EBackfaceVisibility::kHidden)
        return CompositingReason::kBackfaceInvisibility3DAncestor;
    }
  }
  return CompositingReason::kNone;
}

CompositingReasons
CompositingReasonFinder::DirectReasonsForPaintPropertiesExceptScrolling(
    const LayoutObject& object) {
  if (object.GetDocument().Printing())
    return CompositingReason::kNone;

  auto reasons = CompositingReasonsFor3DSceneLeaf(object);

  // TODO(wangxianzhu): Don't depend on PaintLayer for CompositeAfterPaint.
  if (!object.HasLayer()) {
    if (object.IsSVGChild())
      reasons |= DirectReasonsForSVGChildPaintProperties(object);
    return reasons;
  }

  const ComputedStyle& style = object.StyleRef();
  reasons |= CompositingReasonsForAnimation(object) |
             CompositingReasonsForWillChange(style);

  reasons |= CompositingReasonsFor3DTransform(object);

  auto* layer = To<LayoutBoxModelObject>(object).Layer();
  if (layer->Has3DTransformedDescendant()) {
    // Perspective (specified either by perspective or transform properties)
    // with 3d descendants need a render surface for flattening purposes.
    if (style.HasPerspective() || style.Transform().HasPerspective())
      reasons |= CompositingReason::kPerspectiveWith3DDescendants;
    if (style.Preserves3D())
      reasons |= CompositingReason::kPreserve3DWith3DDescendants;
  }

  if (layer->IsRootLayer() && object.GetFrame()->IsLocalRoot())
    reasons |= CompositingReason::kRoot;

  if (RequiresCompositingForRootScroller(*layer))
    reasons |= CompositingReason::kRootScroller;

  reasons |= CompositingReasonsForScrollDependentPosition(*layer);

  if (RequiresCompositingForAffectedByOuterViewportBoundsDelta(object))
    reasons |= CompositingReason::kAffectedByOuterViewportBoundsDelta;

  if (style.HasBackdropFilter())
    reasons |= CompositingReason::kBackdropFilter;

  reasons |= BackfaceInvisibility3DAncestorReason(*layer);

  if (auto* element = DynamicTo<Element>(object.GetNode())) {
    if (element->ShouldCompositeForDocumentTransition())
      reasons |= CompositingReason::kDocumentTransitionSharedElement;
  }

  if (object.CanHaveAdditionalCompositingReasons())
    reasons |= object.AdditionalCompositingReasons();

  return reasons;
}

bool CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
    const LayoutObject& object,
    CompositingReasons reasons_except_scrolling) {
  DCHECK_EQ(reasons_except_scrolling,
            DirectReasonsForPaintPropertiesExceptScrolling(object));

  if (reasons_except_scrolling != CompositingReason::kNone)
    return true;

  // In CompositeAfterPaint though we don't treat hidden backface as a direct
  // compositing reason, it's very likely that the object will be composited,
  // and it also indicates preference of compositing, so we prefer composited
  // scrolling here.
  if (object.StyleRef().BackfaceVisibility() == EBackfaceVisibility::kHidden)
    return true;

  if (auto* layout_view = DynamicTo<LayoutView>(object))
    return ShouldPreferCompositingForLayoutView(*layout_view);

  return false;
}

CompositingReasons CompositingReasonFinder::DirectReasonsForPaintProperties(
    const LayoutObject& object,
    CompositingReasons reasons_except_scrolling) {
  DCHECK_EQ(reasons_except_scrolling,
            DirectReasonsForPaintPropertiesExceptScrolling(object));
  if (auto* box = DynamicTo<LayoutBox>(object)) {
    if (auto* scrollable_area = box->GetScrollableArea()) {
#if DCHECK_IS_ON()
      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
        scrollable_area->CheckNeedsCompositedScrollingIsUpToDate(
            ShouldForcePreferCompositingToLCDText(object,
                                                  reasons_except_scrolling));
      }
#endif
      if (scrollable_area->NeedsCompositedScrolling())
        return reasons_except_scrolling | CompositingReason::kOverflowScrolling;
    }
  }
  return reasons_except_scrolling;
}

CompositingReasons
CompositingReasonFinder::DirectReasonsForSVGChildPaintProperties(
    const LayoutObject& object) {
  DCHECK(object.IsSVGChild());
  if (object.IsText())
    return CompositingReason::kNone;

  // Even though SVG doesn't support 3D transforms, it might be the leaf
  // of a 3D scene that contains it.
  auto reasons = CompositingReasonsFor3DSceneLeaf(object);

  // Disable compositing of SVG, except in the cases where it is required for
  // correctness, if there is clip-path or mask to avoid hairline along the
  // edges. TODO(crbug.com/1171601): Fix the root cause.
  const ComputedStyle& style = object.StyleRef();
  if (style.HasClipPath() || style.HasMask())
    return reasons;

  reasons |= CompositingReasonsForAnimation(object);
  reasons |= CompositingReasonsForWillChange(style);
  // Exclude will-change for other properties some of which don't apply to SVG
  // children, e.g. 'top'.
  reasons &= ~CompositingReason::kWillChangeOther;
  if (style.HasBackdropFilter())
    reasons |= CompositingReason::kBackdropFilter;
  // Though SVG doesn't support 3D transforms, they are frequently used as a
  // compositing trigger for historical reasons.
  reasons |= CompositingReasonsFor3DTransform(object);
  return reasons;
}

CompositingReasons
CompositingReasonFinder::PotentialCompositingReasonsFor3DTransform(
    const ComputedStyle& style) {
  // Don't composite "trivial" 3D transforms such as translateZ(0).
  if (Platform::Current()->IsLowEndDevice()) {
    return style.HasNonTrivial3DTransformOperation()
               ? CompositingReason::k3DTransform
               : CompositingReason::kNone;
  }

  if (style.Has3DTransformOperation()) {
    return style.HasNonTrivial3DTransformOperation()
               ? CompositingReason::k3DTransform
               : CompositingReason::kTrivial3DTransform;
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
  return PotentialCompositingReasonsFor3DTransform(layout_object.StyleRef());
}

CompositingReasons CompositingReasonFinder::CompositingReasonsFor3DSceneLeaf(
    const LayoutObject& layout_object) {
  // An effect node (and, eventually, a render pass created due to
  // cc::RenderSurfaceReason::k3dTransformFlattening) is required for an
  // element that doesn't preserve 3D but is treated as a 3D object by its
  // parent.  See
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1256990#c2 for some
  // notes on why this is needed.  Briefly, we need to ensure that we don't
  // output quads with a 3d sorting_context of 0 in the middle of the quads
  // that need to be 3D sorted; this is needed to contain any such quads in a
  // separate render pass.
  //
  // Note that this is done even on elements that don't create a stacking
  // context, and this appears to work.
  //
  // This could be improved by skipping this if we know that the
  // descendants won't produce any quads in the render pass's quad list.
  if (layout_object.IsText()) {
    // A LayoutNGBR is both IsText() and IsForElement(), but we
    // shouldn't produce compositing reasons if IsText() is true.  Since
    // we only need this for objects that have interesting descendants,
    // we can just return.
    return CompositingReason::kNone;
  }

  if (layout_object.IsForElement() && !layout_object.StyleRef().Preserves3D()) {
    const LayoutObject* parent_object =
        layout_object.NearestAncestorForElement();
    if (parent_object && parent_object->StyleRef().Preserves3D()) {
      return CompositingReason::kTransform3DSceneLeaf;
    }
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

  direct_reasons |= CompositingReasonsForScrollDependentPosition(layer);

  if (RequiresCompositingForAffectedByOuterViewportBoundsDelta(layout_object))
    direct_reasons |= CompositingReason::kAffectedByOuterViewportBoundsDelta;

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

  if (auto* element = DynamicTo<Element>(layout_object.GetNode())) {
    if (element->ShouldCompositeForDocumentTransition())
      direct_reasons |= CompositingReason::kDocumentTransitionSharedElement;
  }

  direct_reasons |= BackfaceInvisibility3DAncestorReason(layer);

  DCHECK(
      !(direct_reasons & CompositingReason::kComboAllStyleDeterminedReasons));
  return direct_reasons;
}

static bool ObjectTypeSupportsCompositedTransformAnimation(
    const LayoutObject& object) {
  if (object.IsSVGChild()) {
    // Transforms are not supported on hidden containers, inlines, text, or
    // filter primitives.
    return !object.IsSVGHiddenContainer() && !object.IsLayoutInline() &&
           !object.IsText() && !object.IsSVGFilterPrimitive();
  }
  // Transforms don't apply on non-replaced inline elements.
  return object.IsBox();
}

CompositingReasons CompositingReasonFinder::CompositingReasonsForAnimation(
    const LayoutObject& object) {
  CompositingReasons reasons = CompositingReason::kNone;
  const auto& style = object.StyleRef();
  if (style.SubtreeWillChangeContents())
    return reasons;

  if (style.HasCurrentTransformAnimation() &&
      ObjectTypeSupportsCompositedTransformAnimation(object))
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

CompositingReasons
CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
    const PaintLayer& layer) {
  CompositingReasons reasons = CompositingReason::kNone;
  // Don't promote fixed position elements that are descendants of a non-view
  // container, e.g. transformed elements.  They will stay fixed wrt the
  // container rather than the enclosing frame.
  if (layer.FixedToViewport()) {
    // We check for |HasOverflow| instead of |ScrollsOverflow| to ensure fixed
    // position elements are composited under overflow: hidden, which can still
    // have smooth scroll animations.
    LocalFrameView* frame_view = layer.GetLayoutObject().GetFrameView();
    if (frame_view->LayoutViewport()->HasOverflow())
      reasons |= CompositingReason::kFixedPosition;
  }

  // Don't promote sticky position elements that cannot move with scrolls.
  // We check for |HasOverflow| instead of |ScrollsOverflow| to ensure sticky
  // position elements are composited under overflow: hidden, which can still
  // have smooth scroll animations.
  if (layer.SticksToScroller() &&
      layer.AncestorScrollContainerLayer()->GetScrollableArea()->HasOverflow())
    reasons |= CompositingReason::kStickyPosition;

  return reasons;
}

bool CompositingReasonFinder::
    RequiresCompositingForAffectedByOuterViewportBoundsDelta(
        const LayoutObject& layout_object) {
  if (!layout_object.IsBox())
    return false;

  if (layout_object.StyleRef().GetPosition() != EPosition::kFixed ||
      !layout_object.StyleRef().IsFixedToBottom())
    return false;

  // Objects inside an iframe that's the root scroller should get the same
  // "pushed by top controls" behavior as for the main frame.
  auto& controller =
      layout_object.GetFrame()->GetPage()->GlobalRootScrollerController();
  if (!layout_object.GetFrame()->IsMainFrame() &&
      layout_object.GetFrame()->GetDocument() !=
          controller.GlobalRootScroller())
    return false;

  // It's affected by viewport only if the container is the LayoutView.
  return IsA<LayoutView>(layout_object.Container());
}

}  // namespace blink
