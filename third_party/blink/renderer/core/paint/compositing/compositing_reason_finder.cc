// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/transform_utils.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"

namespace blink {

namespace {

bool ShouldPreferCompositingForLayoutView(const LayoutView& layout_view) {
  if (layout_view.GetFrame()->IsLocalRoot()) {
    return true;
  }

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

CompositingReasons BackfaceInvisibility3DAncestorReason(
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

CompositingReasons CompositingReasonsForWillChange(const ComputedStyle& style) {
  CompositingReasons reasons = CompositingReason::kNone;
  if (style.SubtreeWillChangeContents())
    return reasons;

  if (style.HasWillChangeTransformHint())
    reasons |= CompositingReason::kWillChangeTransform;
  if (style.HasWillChangeScaleHint())
    reasons |= CompositingReason::kWillChangeScale;
  if (style.HasWillChangeRotateHint())
    reasons |= CompositingReason::kWillChangeRotate;
  if (style.HasWillChangeTranslateHint())
    reasons |= CompositingReason::kWillChangeTranslate;
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

CompositingReasons CompositingReasonsFor3DTransform(
    const LayoutObject& layout_object) {
  // Note that we ask the layoutObject if it has a transform, because the style
  // may have transforms, but the layoutObject may be an inline that doesn't
  // support them.
  if (!layout_object.HasTransformRelatedProperty())
    return CompositingReason::kNone;

  const ComputedStyle& style = layout_object.StyleRef();
  CompositingReasons reasons =
      CompositingReasonFinder::PotentialCompositingReasonsFor3DTransform(style);
  if (reasons != CompositingReason::kNone && layout_object.IsBox()) {
    // In theory this should operate on fragment sizes, but using the box size
    // is probably good enough for a use counter.
    auto& box = To<LayoutBox>(layout_object);
    const PhysicalRect reference_box = ComputeReferenceBox(box);
    gfx::Transform matrix;
    style.ApplyTransform(matrix, &box, reference_box,
                         ComputedStyle::kIncludeTransformOperations,
                         ComputedStyle::kExcludeTransformOrigin,
                         ComputedStyle::kExcludeMotionPath,
                         ComputedStyle::kIncludeIndependentTransformProperties);

    // We want to track whether (a) this element is in a preserve-3d scene and
    // (b) has a matrix that puts it into the third dimension in some way.
    if (matrix.Creates3d()) {
      LayoutObject* parent_for_element =
          layout_object.NearestAncestorForElement();
      if (parent_for_element && parent_for_element->Preserves3D()) {
        UseCounter::Count(layout_object.GetDocument(),
                          WebFeature::kTransform3dScene);
      }
    }
  }
  return reasons;
}

CompositingReasons CompositingReasonsFor3DSceneLeaf(
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
  // This could be improved by skipping this if we know that the descendants
  // won't produce any quads in the render pass's quad list.
  if (layout_object.IsText()) {
    // A LayoutBR is both IsText() and IsForElement(), but we shouldn't
    // produce compositing reasons if IsText() is true.  Since we only need
    // this for objects that have interesting descendants, we can just return.
    return CompositingReason::kNone;
  }

  if (!layout_object.IsAnonymous() && !layout_object.StyleRef().Preserves3D()) {
    const LayoutObject* parent_object =
        layout_object.NearestAncestorForElement();
    if (parent_object && parent_object->StyleRef().Preserves3D()) {
      return CompositingReason::kTransform3DSceneLeaf;
    }
  }

  return CompositingReason::kNone;
}

CompositingReasons DirectReasonsForSVGChildPaintProperties(
    const LayoutObject& object) {
  DCHECK(object.IsSVGChild());
  if (object.IsText())
    return CompositingReason::kNone;

  // Even though SVG doesn't support 3D transforms, it might be the leaf of a 3D
  // scene that contains it.
  auto reasons = CompositingReasonsFor3DSceneLeaf(object);

  const ComputedStyle& style = object.StyleRef();
  reasons |= CompositingReasonFinder::CompositingReasonsForAnimation(object);
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

CompositingReasons CompositingReasonsForViewportScrollEffect(
    const LayoutObject& layout_object,
    const LayoutObject* container_for_fixed_position) {
  if (!layout_object.IsBox())
    return CompositingReason::kNone;

  // The viewport scroll effect should never apply to objects inside an
  // embedded frame tree.
  const LocalFrame* frame = layout_object.GetFrame();
  if (!frame->Tree().Top().IsOutermostMainFrame())
    return CompositingReason::kNone;

  DCHECK_EQ(frame->IsMainFrame(), frame->IsOutermostMainFrame());

  // Objects inside an iframe that's the root scroller should get the same
  // "pushed by top controls" behavior as for the main frame.
  auto& controller = frame->GetPage()->GlobalRootScrollerController();
  if (!frame->IsMainFrame() &&
      frame->GetDocument() != controller.GlobalRootScroller()) {
    return CompositingReason::kNone;
  }

  if (!To<LayoutBox>(layout_object).IsFixedToView(container_for_fixed_position))
    return CompositingReason::kNone;

  CompositingReasons reasons = CompositingReason::kNone;
  // This ensures that the scroll_translation_for_fixed will be initialized in
  // FragmentPaintPropertyTreeBuilder::UpdatePaintOffsetTranslation which in
  // turn ensures that a TransformNode is created (for fixed elements) in cc.
  if (frame->GetPage()->GetVisualViewport().GetOverscrollType() ==
      OverscrollType::kTransform) {
    reasons |= CompositingReason::kFixedPosition;
    if (!To<LayoutBox>(layout_object)
             .AnchorPositionScrollAdjustmentAfectedByViewportScrolling()) {
      reasons |= CompositingReason::kUndoOverscroll;
    }
  }

  if (layout_object.StyleRef().IsFixedToBottom()) {
    reasons |= CompositingReason::kFixedPosition |
               CompositingReason::kAffectedByOuterViewportBoundsDelta;
  }

  return reasons;
}

CompositingReasons CompositingReasonsForScrollDependentPosition(
    const PaintLayer& layer,
    const LayoutObject* container_for_fixed_position) {
  CompositingReasons reasons = CompositingReason::kNone;
  // Don't promote fixed position elements that are descendants of a non-view
  // container, e.g. transformed elements.  They will stay fixed wrt the
  // container rather than the enclosing frame.
  if (const auto* box = layer.GetLayoutBox()) {
    if (box->IsFixedToView(container_for_fixed_position)) {
      // We check for |HasOverflow| instead of |ScrollsOverflow| to ensure fixed
      // position elements are composited under overflow: hidden, which can
      // still have smooth scroll animations.
      LocalFrameView* frame_view = layer.GetLayoutObject().GetFrameView();
      if (frame_view->LayoutViewport()->HasOverflow())
        reasons |= CompositingReason::kFixedPosition;
    }

    if (box->NeedsAnchorPositionScrollAdjustment()) {
      reasons |= CompositingReason::kAnchorPosition;
    }
  }

  // Don't promote sticky position elements that cannot move with scrolls.
  // We check for |HasOverflow| instead of |ScrollsOverflow| to ensure sticky
  // position elements are composited under overflow: hidden, which can still
  // have smooth scroll animations.
  if (const auto* constraints = layer.GetLayoutObject().StickyConstraints()) {
    if (!constraints->is_fixed_to_view &&
        constraints->containing_scroll_container_layer->GetScrollableArea()
            ->HasOverflow())
      reasons |= CompositingReason::kStickyPosition;
  }

  return reasons;
}

bool ObjectTypeSupportsCompositedTransformAnimation(
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

// Defined by the Element Capture specification:
// https://screen-share.github.io/element-capture/#elements-eligible-for-restriction
bool IsEligibleForElementCapture(const LayoutObject& object) {
  // The element forms a stacking context.
  if (!object.IsStackingContext()) {
    return false;
  }

  // The element is flattened in 3D.
  if (!object.CreatesGroup()) {
    return false;
  }

  // The element forms a backdrop root.
  // See ViewTransitionUtils::IsViewTransitionParticipant and
  // NeedsEffectIgnoringClipPath for how View Transitions meets this
  // requirement.
  // TODO(https://issuetracker.google.com/291602746): handle backdrop root case.

  // The element has exactly one box fragment.
  if (object.IsBox() && To<LayoutBox>(object).PhysicalFragmentCount() > 1) {
    return false;
  }

  // Meets all of the conditions for element capture.
  return true;
}

}  // anonymous namespace

CompositingReasons CompositingReasonFinder::DirectReasonsForPaintProperties(
    const LayoutObject& object,
    const LayoutObject* container_for_fixed_position) {
  if (object.GetDocument().Printing())
    return CompositingReason::kNone;

  auto reasons = CompositingReasonsFor3DSceneLeaf(object);

  if (object.CanHaveAdditionalCompositingReasons())
    reasons |= object.AdditionalCompositingReasons();

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

  if (RequiresCompositingForRootScroller(object)) {
    reasons |= CompositingReason::kRootScroller;
  }

  reasons |= CompositingReasonsForScrollDependentPosition(
      *layer, container_for_fixed_position);

  reasons |= CompositingReasonsForViewportScrollEffect(
      object, container_for_fixed_position);

  if (style.HasBackdropFilter())
    reasons |= CompositingReason::kBackdropFilter;

  reasons |= BackfaceInvisibility3DAncestorReason(*layer);

  switch (style.StyleType()) {
    case kPseudoIdViewTransition:
    case kPseudoIdViewTransitionGroup:
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionNew:
    case kPseudoIdViewTransitionOld:
      reasons |= CompositingReason::kViewTransitionPseudoElement;
      break;
    default:
      break;
  }

  if (auto* transition =
          ViewTransitionUtils::GetTransition(object.GetDocument())) {
    // Note that `NeedsViewTransitionEffectNode` returns true for values that
    // are in the non-transition-pseudo tree DOM. That is, things like layout
    // view or the view transition elements that we are transitioning.
    if (transition->NeedsViewTransitionEffectNode(object)) {
      reasons |= CompositingReason::kViewTransitionElement;
    }
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  if (element && element->GetRestrictionTargetId()) {
    const bool is_eligible = IsEligibleForElementCapture(object);
    element->SetIsEligibleForElementCapture(is_eligible);
    if (is_eligible) {
      reasons |= CompositingReason::kElementCapture;
    }
  }

  return reasons;
}

bool CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
    const LayoutObject& object,
    CompositingReasons reasons) {
  DCHECK_EQ(reasons, DirectReasonsForPaintProperties(object));
  if (reasons != CompositingReason::kNone) {
    return true;
  }

  if (object.StyleRef().WillChangeScrollPosition())
    return true;

  // Though we don't treat hidden backface as a direct compositing reason, it's
  // very likely that the object will be composited, and it also indicates
  // preference of compositing, so we prefer composited scrolling here.
  if (object.StyleRef().BackfaceVisibility() == EBackfaceVisibility::kHidden)
    return true;

  if (auto* layout_view = DynamicTo<LayoutView>(object))
    return ShouldPreferCompositingForLayoutView(*layout_view);

  return false;
}

CompositingReasons
CompositingReasonFinder::PotentialCompositingReasonsFor3DTransform(
    const ComputedStyle& style) {
  CompositingReasons reasons = CompositingReason::kNone;

  if (style.Transform().HasNonPerspective3DOperation()) {
    if (style.Transform().HasNonTrivial3DComponent()) {
      reasons |= CompositingReason::k3DTransform;
    } else {
      // This reason is not used in TransformPaintPropertyNode for low-end
      // devices. See PaintPropertyTreeBuilder.
      reasons |= CompositingReason::kTrivial3DTransform;
    }
  }

  if (style.Translate() && style.Translate()->Z() != 0)
    reasons |= CompositingReason::k3DTranslate;

  if (style.Rotate() &&
      (style.Rotate()->X() != 0 || style.Rotate()->Y() != 0)) {
    reasons |= CompositingReason::k3DRotate;
  }

  if (style.Scale() && style.Scale()->Z() != 1)
    reasons |= CompositingReason::k3DScale;

  return reasons;
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
  if (style.HasCurrentScaleAnimation() &&
      ObjectTypeSupportsCompositedTransformAnimation(object))
    reasons |= CompositingReason::kActiveScaleAnimation;
  if (style.HasCurrentRotateAnimation() &&
      ObjectTypeSupportsCompositedTransformAnimation(object))
    reasons |= CompositingReason::kActiveRotateAnimation;
  if (style.HasCurrentTranslateAnimation() &&
      ObjectTypeSupportsCompositedTransformAnimation(object))
    reasons |= CompositingReason::kActiveTranslateAnimation;
  if (style.HasCurrentOpacityAnimation())
    reasons |= CompositingReason::kActiveOpacityAnimation;
  if (style.HasCurrentFilterAnimation())
    reasons |= CompositingReason::kActiveFilterAnimation;
  if (style.HasCurrentBackdropFilterAnimation())
    reasons |= CompositingReason::kActiveBackdropFilterAnimation;
  return reasons;
}

bool CompositingReasonFinder::RequiresCompositingForRootScroller(
    const LayoutObject& object) {
  // The root scroller needs composited scrolling layers even if it doesn't
  // actually have scrolling since CC has these assumptions baked in for the
  // viewport. Because this is only needed for CC, we can skip it if
  // compositing is not enabled.
  if (!object.GetFrame()->GetSettings()->GetAcceleratedCompositingEnabled()) {
    return false;
  }

  return object.IsGlobalRootScroller();
}

}  // namespace blink
