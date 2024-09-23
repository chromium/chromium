// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

#define V(name) #name,
constexpr const char* kShortNames[] = {FOR_EACH_COMPOSITING_REASON(V)};
#undef V

struct ReasonAndDescription {
  CompositingReasons reason;
  const char* description;
};
constexpr ReasonAndDescription kReasonDescriptionMap[] = {
    {CompositingReason::k3DTransform, "Has a 3d transform."},
    {CompositingReason::k3DScale, "Has a 3d scale."},
    {CompositingReason::k3DRotate, "Has a 3d rotate."},
    {CompositingReason::k3DTranslate, "Has a 3d translate."},
    {CompositingReason::kTrivial3DTransform, "Has a trivial 3d transform."},
    {CompositingReason::kIFrame, "Is an accelerated iFrame."},
    {CompositingReason::kActiveTransformAnimation,
     "Has an active accelerated transform animation or transition."},
    {CompositingReason::kActiveScaleAnimation,
     "Has an active accelerated scale animation or transition."},
    {CompositingReason::kActiveRotateAnimation,
     "Has an active accelerated rotate animation or transition."},
    {CompositingReason::kActiveTranslateAnimation,
     "Has an active accelerated translate animation or transition."},
    {CompositingReason::kActiveOpacityAnimation,
     "Has an active accelerated opacity animation or transition."},
    {CompositingReason::kActiveFilterAnimation,
     "Has an active accelerated filter animation or transition."},
    {CompositingReason::kActiveBackdropFilterAnimation,
     "Has an active accelerated backdrop filter animation or transition."},
    {CompositingReason::kAffectedByOuterViewportBoundsDelta,
     "Is fixed position affected by outer viewport bounds delta."},
    {CompositingReason::kFixedPosition,
     "Is fixed position in a scrollable view."},
    {CompositingReason::kUndoOverscroll,
     "Is fixed position that should undo overscroll of the viewport."},
    {CompositingReason::kStickyPosition, "Is sticky position."},
    {CompositingReason::kAnchorPosition,
     "Is an anchor-positioned element translated by its anchor's scroll "
     "offset."},
    {CompositingReason::kBackdropFilter, "Has a backdrop filter."},
    {CompositingReason::kBackdropFilterMask, "Is a mask for backdrop filter."},
    {CompositingReason::kRootScroller, "Is the document.rootScroller."},
    {CompositingReason::kViewport, "Is for the visual viewport."},
    {CompositingReason::kWillChangeTransform,
     "Has a will-change: transform compositing hint."},
    {CompositingReason::kWillChangeScale,
     "Has a will-change: scale compositing hint."},
    {CompositingReason::kWillChangeRotate,
     "Has a will-change: rotate compositing hint."},
    {CompositingReason::kWillChangeTranslate,
     "Has a will-change: translate compositing hint."},
    {CompositingReason::kWillChangeOpacity,
     "Has a will-change: opacity compositing hint."},
    {CompositingReason::kWillChangeFilter,
     "Has a will-change: filter compositing hint."},
    {CompositingReason::kWillChangeBackdropFilter,
     "Has a will-change: backdrop-filter compositing hint."},
    {CompositingReason::kWillChangeOther,
     "Has a will-change compositing hint other than transform, opacity, filter"
     " and backdrop-filter."},
    {CompositingReason::kBackfaceInvisibility3DAncestor,
     "Ancestor in same 3D rendering context has a hidden backface."},
    {CompositingReason::kTransform3DSceneLeaf,
     "Leaf of a 3D scene, for flattening its descendants into that scene."},
    {CompositingReason::kPerspectiveWith3DDescendants,
     "Has a perspective transform that needs to be known by compositor because "
     "of 3d descendants."},
    {CompositingReason::kPreserve3DWith3DDescendants,
     "Has a preserves-3d property that needs to be known by compositor because "
     "of 3d descendants."},
    {CompositingReason::kViewTransitionElement,
     "This element is shared during view transition."},
    {CompositingReason::kViewTransitionPseudoElement,
     "This element is a part of a pseudo element tree representing the view "
     "transition."},
    {CompositingReason::kViewTransitionElementDescendantWithClipPath,
     "This element's ancestor is shared during view transition and it has a "
     "clip-path"},
    {CompositingReason::kOverflowScrolling,
     "Is a scrollable overflow element using accelerated scrolling."},
    {CompositingReason::kElementCapture,
     "This element is undergoing element-level capture."},
    {CompositingReason::kOverlap, "Overlaps other composited content."},
    {CompositingReason::kBackfaceVisibilityHidden,
     "Has backface-visibility: hidden."},
    {CompositingReason::kFixedAttachmentBackground,
     "Is an accelerated background-attachment:fixed background."},
    {CompositingReason::kCaret, "Is a caret in an editor."},
    {CompositingReason::kVideo, "Is an accelerated video."},
    {CompositingReason::kCanvas,
     "Is an accelerated canvas, or is a display list backed canvas that was "
     "promoted to a layer based on a performance heuristic."},
    {CompositingReason::kPlugin, "Is an accelerated plugin."},
    {CompositingReason::kScrollbar, "Is an accelerated scrollbar."},
    {CompositingReason::kLinkHighlight, "Is a tap highlight on a link."},
    {CompositingReason::kDevToolsOverlay, "Is DevTools overlay."},
    {CompositingReason::kViewTransitionContent,
     "The layer containing the contents of a view transition element."},
};

}  // anonymous namespace

std::vector<const char*> CompositingReason::ShortNames(
    CompositingReasons reasons) {
  std::vector<const char*> result;
  if (reasons == kNone) {
    return result;
  }
  for (size_t i = 0; i < std::size(kShortNames); i++) {
    if (reasons & (UINT64_C(1) << i)) {
      result.push_back(kShortNames[i]);
    }
  }
  return result;
}

std::vector<const char*> CompositingReason::Descriptions(
    CompositingReasons reasons) {
#define V(name)                                                      \
  static_assert(                                                     \
      CompositingReason::k##name ==                                  \
          kReasonDescriptionMap[CompositingReason::kE##name].reason, \
      "kReasonDescriptionMap needs update for CompositingReason::k" #name);

  FOR_EACH_COMPOSITING_REASON(V)
#undef V

  std::vector<const char*> result;
  if (reasons == kNone) {
    return result;
  }
  for (auto& map : kReasonDescriptionMap) {
    if (reasons & map.reason) {
      result.push_back(map.description);
    }
  }
  return result;
}

String CompositingReason::ToString(CompositingReasons reasons) {
  StringBuilder builder;
  for (const char* name : ShortNames(reasons)) {
    if (builder.length())
      builder.Append(',');
    builder.Append(name);
  }
  return builder.ToString();
}

}  // namespace blink
