// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"

#include <array>
#include <iostream>

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct ReasonMetadata {
  CompositingReason reason;  // Just for static_asserts, not used at runtime.
  const char* short_name;
  const char* description;
};
constexpr auto kReasonMetadata = std::to_array<ReasonMetadata>({
    {CompositingReason::k3DTransform, "3DTransform", "Has a 3d transform."},
    {CompositingReason::k3DScale, "3DScale", "Has a 3d scale."},
    {CompositingReason::k3DRotate, "3DRotate", "Has a 3d rotate."},
    {CompositingReason::k3DTranslate, "3DTranslate", "Has a 3d translate."},
    {CompositingReason::kTrivial3DTransform, "Trivial3DTransform",
     "Has a trivial 3d transform."},
    {CompositingReason::kIFrame, "IFrame", "Is an accelerated iFrame."},
    {CompositingReason::kActiveTransformAnimation, "ActiveTransformAnimation",
     "Has an active accelerated transform animation or transition."},
    {CompositingReason::kActiveScaleAnimation, "ActiveScaleAnimation",
     "Has an active accelerated scale animation or transition."},
    {CompositingReason::kActiveRotateAnimation, "ActiveRotateAnimation",
     "Has an active accelerated rotate animation or transition."},
    {CompositingReason::kActiveTranslateAnimation, "ActiveTranslateAnimation",
     "Has an active accelerated translate animation or transition."},
    {CompositingReason::kActiveOpacityAnimation, "ActiveOpacityAnimation",
     "Has an active accelerated opacity animation or transition."},
    {CompositingReason::kActiveFilterAnimation, "ActiveFilterAnimation",
     "Has an active accelerated filter animation or transition."},
    {CompositingReason::kActiveBackdropFilterAnimation,
     "ActiveBackdropFilterAnimation",
     "Has an active accelerated backdrop filter animation or transition."},
    {CompositingReason::kAffectedByOuterViewportBoundsDelta,
     "AffectedByOuterViewportBoundsDelta",
     "Is fixed position affected by outer viewport bounds delta."},
    {CompositingReason::kAffectedBySafeAreaBottom, "AffectedBySafeAreaBottom",
     "Is fixed position affected by safe area bottom."},
    {CompositingReason::kFixedPosition, "FixedPosition",
     "Is fixed position in a scrollable view."},
    {CompositingReason::kUndoOverscroll, "UndoOverscroll",
     "Is fixed position that should undo overscroll of the viewport."},
    {CompositingReason::kStickyPosition, "StickyPosition",
     "Is sticky position."},
    {CompositingReason::kAnchorPosition, "AnchorPosition",
     "Is an anchor-positioned element translated by its anchor's scroll "
     "offset."},
    {CompositingReason::kBackdropFilter, "BackdropFilter",
     "Has a backdrop filter."},
    {CompositingReason::kBackdropFilterMask, "BackdropFilterMask",
     "Is a mask for backdrop filter."},
    {CompositingReason::kFixedBackdropInOverscrollAreaParent,
     "FixedBackdropInOverscrollAreaParent",
     "Is a fixed backdrop inside an overscroll area parent scroller."},
    {CompositingReason::kRootScroller, "RootScroller",
     "Is the document.rootScroller."},
    {CompositingReason::kViewport, "Viewport", "Is for the visual viewport."},
    {CompositingReason::kWillChangeTransform, "WillChangeTransform",
     "Has a will-change: transform compositing hint."},
    {CompositingReason::kWillChangeScale, "WillChangeScale",
     "Has a will-change: scale compositing hint."},
    {CompositingReason::kWillChangeRotate, "WillChangeRotate",
     "Has a will-change: rotate compositing hint."},
    {CompositingReason::kWillChangeTranslate, "WillChangeTranslate",
     "Has a will-change: translate compositing hint."},
    {CompositingReason::kWillChangeOpacity, "WillChangeOpacity",
     "Has a will-change: opacity compositing hint."},
    {CompositingReason::kWillChangeFilter, "WillChangeFilter",
     "Has a will-change: filter compositing hint."},
    {CompositingReason::kWillChangeBackdropFilter, "WillChangeBackdropFilter",
     "Has a will-change: backdrop-filter compositing hint."},
    {CompositingReason::kWillChangeClipPath, "WillChangeClipPath",
     "Has a will-change: clip-path compositing hint."},
    {CompositingReason::kWillChangeMixBlendMode, "WillChangeMixBlendMode",
     "Has a will-change: mix-blend-mode compositing hint."},
    {CompositingReason::kWillChangeMask, "WillChangeMask",
     "Has a will-change: mask compositing hint."},
    {CompositingReason::kWillChangeOther, "WillChangeOther",
     "Has a will-change compositing hint other than transform, opacity, filter"
     " and backdrop-filter."},
    {CompositingReason::kBackfaceInvisibility3DAncestor,
     "BackfaceInvisibility3DAncestor",
     "Ancestor in same 3D rendering context has a hidden backface."},
    {CompositingReason::kTransform3DSceneLeaf, "Transform3DSceneLeaf",
     "Leaf of a 3D scene, for flattening its descendants into that scene."},
    {CompositingReason::kPerspectiveWith3DDescendants,
     "PerspectiveWith3DDescendants",
     "Has a perspective transform that needs to be known by compositor because "
     "of 3d descendants."},
    {CompositingReason::kPreserve3DWith3DDescendants,
     "Preserve3DWith3DDescendants",
     "Has a preserves-3d property that needs to be known by compositor because "
     "of 3d descendants."},
    {CompositingReason::kViewTransitionElement, "ViewTransitionElement",
     "This element is shared during view transition."},
    {CompositingReason::kViewTransitionPseudoElement,
     "ViewTransitionPseudoElement",
     "This element is a part of a pseudo-element tree representing the view "
     "transition."},
    {CompositingReason::kViewTransitionElementDescendantWithClipPath,
     "ViewTransitionElementDescendantWithClipPath",
     "This element's ancestor is shared during view transition and it has a "
     "clip-path"},
    {CompositingReason::kOverflowScrolling, "OverflowScrolling",
     "Is a scrollable overflow element using accelerated scrolling."},
    {CompositingReason::kElementCapture, "ElementCapture",
     "This element is undergoing element-level capture."},
    {CompositingReason::kOverlap, "Overlap",
     "Overlaps other composited content."},
    {CompositingReason::kBackfaceVisibilityHidden, "BackfaceVisibilityHidden",
     "Has backface-visibility: hidden."},
    {CompositingReason::kFixedAttachmentBackground, "FixedAttachmentBackground",
     "Is an accelerated background-attachment:fixed background."},
    {CompositingReason::kCaret, "Caret", "Is a caret in an editor."},
    {CompositingReason::kVideo, "Video", "Is an accelerated video."},
    {CompositingReason::kCanvas, "Canvas",
     "Is an accelerated canvas, or is a display list backed canvas that was "
     "promoted to a layer based on a performance heuristic."},
    {CompositingReason::kCanvasChild, "CanvasChild",
     "Is the direct child of a canvas with 'layoutSubtree' attribute."},
    {CompositingReason::kPlugin, "Plugin", "Is an accelerated plugin."},
    {CompositingReason::kScrollbar, "Scrollbar",
     "Is an accelerated scrollbar."},
    {CompositingReason::kLinkHighlight, "LinkHighlight",
     "Is a tap highlight on a link."},
    {CompositingReason::kDevToolsOverlay, "DevToolsOverlay",
     "Is DevTools overlay."},
    {CompositingReason::kViewTransitionContent, "ViewTransitionContent",
     "The layer containing the contents of a view transition element."},
    {CompositingReason::kUnboundedElement, "UnboundedElement",
     "Is an active unbounded element."},
});

constexpr size_t FindMismatch() {
  static_assert(kReasonMetadata.size() ==
                static_cast<size_t>(CompositingReason::kMaxValue) + 1);
  for (size_t i = 0; i < kReasonMetadata.size(); ++i) {
    if (static_cast<size_t>(kReasonMetadata[i].reason) != i) {
      return i;
    }
  }
  return kNotFound;
}

static_assert(FindMismatch() == kNotFound,
              "kReasonMetadata entries must match CompositingReason enum "
              "values and ordering.");

}  // anonymous namespace

std::vector<const char*> ShortNames(CompositingReasons reasons) {
  std::vector<const char*> result;
  for (auto r : reasons) {
    result.push_back(kReasonMetadata[static_cast<size_t>(r)].short_name);
  }
  return result;
}

std::vector<const char*> Descriptions(CompositingReasons reasons) {
  std::vector<const char*> result;
  for (auto r : reasons) {
    result.push_back(kReasonMetadata[static_cast<size_t>(r)].description);
  }
  return result;
}

String ToString(CompositingReasons reasons) {
  StringBuilder builder;
  builder.AppendRange(ShortNames(reasons), ",");
  return builder.ReleaseString();
}

std::ostream& operator<<(std::ostream& os, CompositingReason reason) {
  return os << ToString(CompositingReasons({reason}));
}

std::ostream& operator<<(std::ostream& os, CompositingReasons reasons) {
  return os << ToString(reasons);
}

}  // namespace blink
