// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

#include "third_party/blink/public/platform/platform.h"

namespace blink {

CompositingReasonFinder::CompositingReasonFinder(LayoutView& layout_view)
    : layout_view_(layout_view),
      compositing_triggers_(
          static_cast<CompositingTriggerFlags>(kAllCompositingTriggers)) {
  UpdateTriggers();
}

void CompositingReasonFinder::UpdateTriggers() {
  compositing_triggers_ = 0;

  Settings& settings = layout_view_.GetDocument().GetPage()->GetSettings();
  if (settings.GetPreferCompositingToLCDTextEnabled()) {
    compositing_triggers_ |= kScrollableInnerFrameTrigger;
    compositing_triggers_ |= kOverflowScrollTrigger;
    compositing_triggers_ |= kViewportConstrainedPositionedTrigger;
  }
}

bool CompositingReasonFinder::IsMainFrame() const {
  return layout_view_.GetDocument().IsInMainFrame();
}

CompositingReasons CompositingReasonFinder::DirectReasons(
    const PaintLayer* layer,
    bool ignore_lcd_text) const {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return CompositingReason::kNone;

  DCHECK_EQ(PotentialCompositingReasonsFromStyle(layer->GetLayoutObject()),
            layer->PotentialCompositingReasonsFromStyle());
  CompositingReasons style_determined_direct_compositing_reasons =
      layer->PotentialCompositingReasonsFromStyle() &
      CompositingReason::kComboAllDirectStyleDeterminedReasons;

  return style_determined_direct_compositing_reasons |
         NonStyleDeterminedDirectReasons(layer, ignore_lcd_text);
}

bool CompositingReasonFinder::RequiresCompositingForScrollableFrame() const {
  // Need this done first to determine overflow.
  DCHECK(!layout_view_.NeedsLayout());
  if (IsMainFrame())
    return false;

  if (!(compositing_triggers_ & kScrollableInnerFrameTrigger))
    return false;

  if (layout_view_.GetFrameView()->Size().IsEmpty())
    return false;

  return layout_view_.GetFrameView()->LayoutViewport()->ScrollsOverflow();
}

CompositingReasons
CompositingReasonFinder::PotentialCompositingReasonsFromStyle(
    LayoutObject& layout_object) const {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return CompositingReason::kNone;

  CompositingReasons reasons = CompositingReason::kNone;

  const ComputedStyle& style = layout_object.StyleRef();

  if (RequiresCompositingForTransform(layout_object))
    reasons |= CompositingReason::k3DTransform;

  if (style.BackfaceVisibility() == EBackfaceVisibility::kHidden)
    reasons |= CompositingReason::kBackfaceVisibilityHidden;

  reasons |= CompositingReasonsForAnimation(style);

  if (style.HasWillChangeCompositingHint() &&
      !style.SubtreeWillChangeContents())
    reasons |= CompositingReason::kWillChangeCompositingHint;

  if (style.HasInlineTransform())
    reasons |= CompositingReason::kInlineTransform;

  if (style.UsedTransformStyle3D() == ETransformStyle3D::kPreserve3d)
    reasons |= CompositingReason::kPreserve3DWith3DDescendants;

  if (style.HasPerspective())
    reasons |= CompositingReason::kPerspectiveWith3DDescendants;

  // If the implementation of CreatesGroup changes, we need to be aware of that
  // in this part of code.
  DCHECK((style.HasOpacity() || layout_object.HasMask() ||
          layout_object.HasClipPath() ||
          layout_object.HasFilterInducingProperty() || style.HasBlendMode()) ==
         layout_object.CreatesGroup());

  if (style.HasMask() || style.ClipPath())
    reasons |= CompositingReason::kMaskWithCompositedDescendants;

  if (style.HasFilterInducingProperty())
    reasons |= CompositingReason::kFilterWithCompositedDescendants;

  if (style.HasBackdropFilter())
    reasons |= CompositingReason::kBackdropFilter;

  // See Layer::updateTransform for an explanation of why we check both.
  if (layout_object.HasTransformRelatedProperty() && style.HasTransform())
    reasons |= CompositingReason::kTransformWithCompositedDescendants;

  if (style.HasOpacity())
    reasons |= CompositingReason::kOpacityWithCompositedDescendants;

  if (style.HasBlendMode())
    reasons |= CompositingReason::kBlendingWithCompositedDescendants;

  if (layout_object.HasReflection())
    reasons |= CompositingReason::kReflectionWithCompositedDescendants;

  if (layout_object.HasClipRelatedProperty())
    reasons |= CompositingReason::kClipsCompositingDescendants;

  DCHECK(!(reasons & ~CompositingReason::kComboAllStyleDeterminedReasons));
  return reasons;
}

bool CompositingReasonFinder::RequiresCompositingForTransform(
    const LayoutObject& layout_object) {
  // Note that we ask the layoutObject if it has a transform, because the style
  // may have transforms, but the layoutObject may be an inline that doesn't
  // support them.
  return layout_object.HasTransformRelatedProperty() &&
         layout_object.StyleRef().Has3DTransform() &&
         // Don't composite "trivial" 3D transforms such as translateZ(0) on
         // low-end devices. These devices are much more sensitive to memory
         // and per-composited-layer commit overhead.
         (!Platform::Current()->IsLowEndDevice() ||
          layout_object.StyleRef().Transform().HasNonTrivial3DComponent());
}

CompositingReasons CompositingReasonFinder::NonStyleDeterminedDirectReasons(
    const PaintLayer* layer,
    bool ignore_lcd_text) const {
  CompositingReasons direct_reasons = CompositingReason::kNone;
  LayoutObject& layout_object = layer->GetLayoutObject();

  // TODO(chrishtr): remove this hammer in favor of something more targeted.
  // See crbug.com/749349.
  if (layer->ClipParent() && layer->GetLayoutObject().IsOutOfFlowPositioned())
    direct_reasons |= CompositingReason::kOutOfFlowClipping;

  if (RequiresCompositingForRootScroller(*layer))
    direct_reasons |= CompositingReason::kRootScroller;

  // Composite |layer| if it is inside of an ancestor scrolling layer, but that
  // scrolling layer is not on the stacking context ancestor chain of |layer|.
  // See the definition of the scrollParent property in Layer for more detail.
  if (const PaintLayer* scrolling_ancestor = layer->AncestorScrollingLayer()) {
    if (scrolling_ancestor->NeedsCompositedScrolling() && layer->ScrollParent())
      direct_reasons |= CompositingReason::kOverflowScrollingParent;
  }

  if (RequiresCompositingForScrollDependentPosition(layer, ignore_lcd_text))
    direct_reasons |= CompositingReason::kScrollDependentPosition;

  // TODO(crbug.com/839341): Remove once we support main-thread AnimationWorklet
  // and don't need to promote the scroll-source.
  if (layer->GetScrollableArea() && layer->GetLayoutObject().GetNode() &&
      ScrollTimeline::HasActiveScrollTimeline(
          layer->GetLayoutObject().GetNode())) {
    direct_reasons |= CompositingReason::kScrollTimelineTarget;
  }

  // Video is special. It's the only PaintLayer type that can both have
  // PaintLayer children and whose children can't use its backing to render
  // into. These children (the controls) always need to be promoted into their
  // own layers to draw on top of the accelerated video.
  if (layer->CompositingContainer() &&
      layer->CompositingContainer()->GetLayoutObject().IsVideo())
    direct_reasons |= CompositingReason::kVideoOverlay;

  if (layer->IsRootLayer() && (RequiresCompositingForScrollableFrame() ||
                               layout_view_.GetFrame()->IsLocalRoot())) {
    direct_reasons |= CompositingReason::kRoot;
  }

  direct_reasons |= layout_object.AdditionalCompositingReasons();

  DCHECK(
      !(direct_reasons & CompositingReason::kComboAllStyleDeterminedReasons));
  return direct_reasons;
}

CompositingReasons CompositingReasonFinder::CompositingReasonsForAnimation(
    const ComputedStyle& style) {
  CompositingReasons reasons = CompositingReason::kNone;
  if (RequiresCompositingForTransformAnimation(style))
    reasons |= CompositingReason::kActiveTransformAnimation;
  if (RequiresCompositingForOpacityAnimation(style))
    reasons |= CompositingReason::kActiveOpacityAnimation;
  if (RequiresCompositingForFilterAnimation(style))
    reasons |= CompositingReason::kActiveFilterAnimation;
  if (RequiresCompositingForBackdropFilterAnimation(style))
    reasons |= CompositingReason::kActiveBackdropFilterAnimation;
  return reasons;
}

bool CompositingReasonFinder::RequiresCompositingForOpacityAnimation(
    const ComputedStyle& style) {
  return style.SubtreeWillChangeContents()
             ? style.IsRunningOpacityAnimationOnCompositor()
             : style.HasCurrentOpacityAnimation();
}

bool CompositingReasonFinder::RequiresCompositingForFilterAnimation(
    const ComputedStyle& style) {
  return style.SubtreeWillChangeContents()
             ? style.IsRunningFilterAnimationOnCompositor()
             : style.HasCurrentFilterAnimation();
}

bool CompositingReasonFinder::RequiresCompositingForBackdropFilterAnimation(
    const ComputedStyle& style) {
  return style.SubtreeWillChangeContents()
             ? style.IsRunningBackdropFilterAnimationOnCompositor()
             : style.HasCurrentBackdropFilterAnimation();
}

bool CompositingReasonFinder::RequiresCompositingForTransformAnimation(
    const ComputedStyle& style) {
  return style.SubtreeWillChangeContents()
             ? style.IsRunningTransformAnimationOnCompositor()
             : style.HasCurrentTransformAnimation();
}

bool CompositingReasonFinder::RequiresCompositingForRootScroller(
    const PaintLayer& layer) {
  // The root scroller needs composited scrolling layers even if it doesn't
  // actually have scrolling since CC has these assumptions baked in for the
  // viewport. Because this is only needed for CC, we can skip it if compositing
  // is not enabled.
  const auto& settings = *layer.GetLayoutObject().GetDocument().GetSettings();
  if (!settings.GetAcceleratedCompositingEnabled())
    return false;
  return root_scroller_util::IsGlobal(layer);
}

bool CompositingReasonFinder::RequiresCompositingForScrollDependentPosition(
    const PaintLayer* layer,
    bool ignore_lcd_text) const {
  if (!layer->GetLayoutObject().StyleRef().HasViewportConstrainedPosition() &&
      !layer->GetLayoutObject().StyleRef().HasStickyConstrainedPosition())
    return false;

  if (!(ignore_lcd_text ||
        (compositing_triggers_ & kViewportConstrainedPositionedTrigger)) &&
      (!RuntimeEnabledFeatures::CompositeOpaqueFixedPositionEnabled() ||
       !layer->BackgroundIsKnownToBeOpaqueInRect(
           LayoutRect(layer->BoundingBoxForCompositing())) ||
       layer->CompositesWithTransform() || layer->CompositesWithOpacity())) {
    return false;
  }
  // Don't promote fixed position elements that are descendants of a non-view
  // container, e.g. transformed elements.  They will stay fixed wrt the
  // container rather than the enclosing frame.
  EPosition position = layer->GetLayoutObject().StyleRef().GetPosition();
  if (position == EPosition::kFixed) {
    return layer->FixedToViewport() &&
           layout_view_.GetFrameView()->LayoutViewport()->ScrollsOverflow();
  }
  DCHECK_EQ(position, EPosition::kSticky);

  // Don't promote sticky position elements that cannot move with scrolls.
  if (!layer->SticksToScroller())
    return false;
  return layer->AncestorOverflowLayer()->ScrollsOverflow();
}

}  // namespace blink
