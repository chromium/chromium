// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_

#include <stdint.h>

#include <iosfwd>
#include <vector>

#include "base/containers/enum_set.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class CompositingReason {
  kMinValue,
  // Intrinsic reasons that can be known right away by the layer.
  k3DTransform = kMinValue,
  k3DScale,
  k3DRotate,
  k3DTranslate,
  kTrivial3DTransform,
  kIFrame,
  kActiveTransformAnimation,
  kActiveScaleAnimation,
  kActiveRotateAnimation,
  kActiveTranslateAnimation,
  kActiveOpacityAnimation,
  kActiveFilterAnimation,
  kActiveBackdropFilterAnimation,
  kAffectedByOuterViewportBoundsDelta,
  kAffectedBySafeAreaBottom,
  kFixedPosition,
  kUndoOverscroll,
  kStickyPosition,
  kAnchorPosition,
  kBackdropFilter,
  kBackdropFilterMask,
  kFixedBackdropInOverscrollAreaParent,
  kRootScroller,
  kViewport,
  kWillChangeTransform,
  kWillChangeScale,
  kWillChangeRotate,
  kWillChangeTranslate,
  kWillChangeOpacity,
  kWillChangeFilter,
  kWillChangeBackdropFilter,
  kWillChangeClipPath,
  kWillChangeMixBlendMode,
  kWillChangeMask,
  // This flag is needed only when none of the explicit kWillChange* reasons
  // are set.
  kWillChangeOther,
  // Reasons that depend on ancestor properties
  kBackfaceInvisibility3DAncestor,
  // TODO(crbug.com/40200456): Transform3DSceneLeaf today depends only on the
  // element and its properties, but in the future it could be optimized
  // to consider descendants and moved to the subtree group below.
  kTransform3DSceneLeaf,

  // Subtree reasons that require knowing what the status of your subtree is
  // before knowing the answer.
  kPerspectiveWith3DDescendants,
  kPreserve3DWith3DDescendants,

  // ViewTransition element.
  // See third_party/blink/renderer/core/view_transition/README.md.
  kViewTransitionElement,
  kViewTransitionPseudoElement,
  kViewTransitionElementDescendantWithClipPath,

  // For composited scrolling, determined after paint.
  kOverflowScrolling,

  // Element is participating in element capture.
  kElementCapture,

  // The following reasons are not used in paint properties, but are
  // determined after paint, for debugging. See PaintArtifactCompositor.
  // This is based on overlapping relationship among pending layers.
  kOverlap,
  // These are based on the type of paint chunks and display items.
  kBackfaceVisibilityHidden,
  kFixedAttachmentBackground,
  kCaret,
  kVideo,
  kCanvas,
  kCanvasChild,
  kPlugin,
  kScrollbar,
  kLinkHighlight,
  kDevToolsOverlay,
  kViewTransitionContent,
  kUnboundedElement,
  kMaxValue = kUnboundedElement,
};

using CompositingReasons = base::EnumSet<CompositingReason>;

// Various combinations of compositing reasons, for more intuitive logic.
class CompositingReasonCombos {
  STATIC_ONLY(CompositingReasonCombos);

 private:
  using enum CompositingReason;

 public:
  // Note that translate is not included, because we care about transforms
  // that are not IsIdentityOrTranslation().
  static constexpr CompositingReasons kPreventingSubpixelAccumulationReasons = {
      kWillChangeTransform, kWillChangeScale, kWillChangeRotate};

  static constexpr CompositingReasons
      kDirectReasonsForPaintOffsetTranslationProperty = {
          kFixedPosition,
          kAffectedByOuterViewportBoundsDelta,
          kUndoOverscroll,
          kVideo,
          kCanvas,
          kCanvasChild,
          kPlugin,
          kIFrame,
          kAffectedBySafeAreaBottom,
          kFixedBackdropInOverscrollAreaParent};

  static constexpr CompositingReasons kFixedPositionReasons = {
      kFixedPosition, kUndoOverscroll, kAffectedByOuterViewportBoundsDelta,
      kAffectedBySafeAreaBottom};

  // TODO(dbaron): kWillChangeOther probably shouldn't be in this list.
  // TODO(vmpstr): kViewTransitionElement is needed to make sure that the
  // capture escapes clips when view transition has a descendant that
  // naturally escapes clips. See crbug.com/348590918 for details.
  static constexpr CompositingReasons kDirectReasonsForTransformProperty = {
      k3DTransform,
      kTrivial3DTransform,
      kWillChangeTransform,
      kWillChangeOther,
      kPerspectiveWith3DDescendants,
      kPreserve3DWith3DDescendants,
      kActiveTransformAnimation,
      kViewTransitionElementDescendantWithClipPath,
      kViewTransitionElement};
  static constexpr CompositingReasons kDirectReasonsForScaleProperty = {
      k3DScale, kWillChangeScale, kActiveScaleAnimation};
  static constexpr CompositingReasons kDirectReasonsForRotateProperty = {
      k3DRotate, kWillChangeRotate, kActiveRotateAnimation};
  static constexpr CompositingReasons kDirectReasonsForTranslateProperty = {
      k3DTranslate, kWillChangeTranslate, kActiveTranslateAnimation};

  static constexpr CompositingReasons
      kDirectReasonsForScrollTranslationProperty = {kRootScroller,
                                                    kOverflowScrolling};

  static constexpr CompositingReasons kDirectReasonsForEffectProperty = {
      kActiveOpacityAnimation,
      kWillChangeOpacity,
      kBackdropFilter,
      kWillChangeBackdropFilter,
      kWillChangeMixBlendMode,
      kActiveBackdropFilterAnimation,
      kViewTransitionPseudoElement,
      kTransform3DSceneLeaf,
      kElementCapture,
      kCanvasChild,
      kUnboundedElement};
  static constexpr CompositingReasons kDirectReasonsForFilterProperty = {
      kActiveFilterAnimation, kWillChangeFilter};

  static constexpr CompositingReasons kDirectReasonsForBackdropFilter = {
      kBackdropFilter, kActiveBackdropFilterAnimation,
      kWillChangeBackdropFilter};
  // These will-change properties create a backdrop root if a child with
  // backdrop-filter is present, but otherwise do not create an effect node on
  // their own (and thus do not self-enforce)
  static constexpr CompositingReasons kAuxiliaryReasonsForBackdropRoot = {
      kWillChangeClipPath, kWillChangeMask};

  // These reasons also cause any effect or filter node that exists
  // to be composited. They don't cause creation of a node.
  // This is because 3D transforms and incorrect use of will-change:transform
  // are likely indicators that compositing of effects is expected
  // because certain changes to opacity, filter etc. will be made.
  // Note that kWillChangeScale, kWillChangeRotate, and
  // kWillChangeTranslate are not included since there is no
  // web-compatibility reason to include them.
  static constexpr CompositingReasons kAdditionalEffectCompositingTrigger = {
      k3DTransform, kTrivial3DTransform, kWillChangeTransform};

  // Cull rect expansion is required if the compositing reasons hint
  // requirement of high-performance movement, to avoid frequent change of
  // cull rect.
  static constexpr CompositingReasons kRequiresCullRectExpansion = [] {
    CompositingReasons reasons = {kStickyPosition, kAnchorPosition};
    reasons.PutAll(kDirectReasonsForTransformProperty);
    reasons.PutAll(kDirectReasonsForScaleProperty);
    reasons.PutAll(kDirectReasonsForRotateProperty);
    reasons.PutAll(kDirectReasonsForTranslateProperty);
    reasons.PutAll(kDirectReasonsForScrollTranslationProperty);
    return reasons;
  }();
};

PLATFORM_EXPORT std::vector<const char*> ShortNames(CompositingReasons);
PLATFORM_EXPORT std::vector<const char*> Descriptions(CompositingReasons);
PLATFORM_EXPORT String ToString(CompositingReasons);

PLATFORM_EXPORT std::ostream& operator<<(std::ostream& os, CompositingReason);
PLATFORM_EXPORT std::ostream& operator<<(std::ostream& os, CompositingReasons);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_
