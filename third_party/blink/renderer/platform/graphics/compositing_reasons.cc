// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

struct CompositingReasonStringMap {
  CompositingReasons reason;
  const char* short_name;
  const char* description;
};

constexpr CompositingReasonStringMap kCompositingReasonsStringMap[] = {
    {CompositingReason::k3DTransform, "transform3D", "Has a 3d transform"},
    {CompositingReason::k3DScale, "scale3D", "Has a 3d scale"},
    {CompositingReason::k3DRotate, "rotate3D", "Has a 3d rotate"},
    {CompositingReason::k3DTranslate, "translate3D", "Has a 3d translate"},
    {CompositingReason::kTrivial3DTransform, "trivialTransform3D",
     "Has a trivial 3d transform"},
    {CompositingReason::kVideo, "video", "Is an accelerated video"},
    {CompositingReason::kCanvas, "canvas",
     "Is an accelerated canvas, or is a display list backed canvas that was "
     "promoted to a layer based on a performance heuristic."},
    {CompositingReason::kPlugin, "plugin", "Is an accelerated plugin"},
    {CompositingReason::kIFrame, "iFrame", "Is an accelerated iFrame"},
    {CompositingReason::kBackfaceVisibilityHidden, "backfaceVisibilityHidden",
     "Has backface-visibility: hidden"},
    {CompositingReason::kActiveTransformAnimation, "activeTransformAnimation",
     "Has an active accelerated transform animation or transition"},
    {CompositingReason::kActiveScaleAnimation, "activeScaleAnimation",
     "Has an active accelerated scale animation or transition"},
    {CompositingReason::kActiveRotateAnimation, "activeRotateAnimation",
     "Has an active accelerated rotate animation or transition"},
    {CompositingReason::kActiveTranslateAnimation, "activeTranslateAnimation",
     "Has an active accelerated translate animation or transition"},
    {CompositingReason::kActiveOpacityAnimation, "activeOpacityAnimation",
     "Has an active accelerated opacity animation or transition"},
    {CompositingReason::kActiveFilterAnimation, "activeFilterAnimation",
     "Has an active accelerated filter animation or transition"},
    {CompositingReason::kActiveBackdropFilterAnimation,
     "activeBackdropFilterAnimation",
     "Has an active accelerated backdrop filter animation or transition"},
    {CompositingReason::kFixedPosition, "fixedPosition", "Is fixed position"},
    {CompositingReason::kStickyPosition, "stickyPosition",
     "Is sticky position"},
    {CompositingReason::kOverflowScrolling, "overflowScrolling",
     "Is a scrollable overflow element"},
    {CompositingReason::kWillChangeTransform, "willChangeTransform",
     "Has a will-change: transform compositing hint"},
    {CompositingReason::kWillChangeScale, "willChangeScale",
     "Has a will-change: scale compositing hint"},
    {CompositingReason::kWillChangeRotate, "willChangeRotate",
     "Has a will-change: rotate compositing hint"},
    {CompositingReason::kWillChangeTranslate, "willChangeTranslate",
     "Has a will-change: translate compositing hint"},
    {CompositingReason::kWillChangeOpacity, "willChangeOpacity",
     "Has a will-change: opacity compositing hint"},
    {CompositingReason::kWillChangeFilter, "willChangeFilter",
     "Has a will-change: filter compositing hint"},
    {CompositingReason::kWillChangeBackdropFilter, "willChangeBackdropFilter",
     "Has a will-change: backdrop-filter compositing hint"},
    {CompositingReason::kWillChangeOther, "willChangeOther",
     "Has a will-change compositing hint other than transform and opacity"},
    {CompositingReason::kBackdropFilter, "backdropFilter",
     "Has a backdrop filter"},
    {CompositingReason::kBackdropFilterMask, "backdropFilterMask",
     "Is a mask for backdrop filter"},
    {CompositingReason::kRootScroller, "rootScroller",
     "Is the document.rootScroller"},
    {CompositingReason::kOverlap, "overlap",
     "Overlaps other composited content"},
    {CompositingReason::kOpacityWithCompositedDescendants,
     "opacityWithCompositedDescendants",
     "Has opacity that needs to be applied by compositor because of composited "
     "descendants"},
    {CompositingReason::kMaskWithCompositedDescendants,
     "maskWithCompositedDescendants",
     "Has a mask that needs to be known by compositor because of composited "
     "descendants"},
    {CompositingReason::kFilterWithCompositedDescendants,
     "filterWithCompositedDescendants",
     "Has a filter effect that needs to be known by compositor because of "
     "composited descendants"},
    {CompositingReason::kBlendingWithCompositedDescendants,
     "blendingWithCompositedDescendants",
     "Has a blending effect that needs to be known by compositor because of "
     "composited descendants"},
    {CompositingReason::kPerspectiveWith3DDescendants,
     "perspectiveWith3DDescendants",
     "Has a perspective transform that needs to be known by compositor because "
     "of 3d descendants"},
    {CompositingReason::kPreserve3DWith3DDescendants,
     "preserve3DWith3DDescendants",
     "Has a preserves-3d property that needs to be known by compositor because "
     "of 3d descendants"},
    {CompositingReason::kRoot, "root", "Is the root layer"},
    {CompositingReason::kLayerForHorizontalScrollbar,
     "layerForHorizontalScrollbar",
     "Secondary layer, the horizontal scrollbar layer"},
    {CompositingReason::kLayerForVerticalScrollbar, "layerForVerticalScrollbar",
     "Secondary layer, the vertical scrollbar layer"},
    {CompositingReason::kUndoOverscroll, "UndoOverscroll",
     "The layer is fixed to viewport and doesn't move with overscroll"},
    {CompositingReason::kLayerForOther, "layerForOther",
     "Layer for link highlight, frame overlay, etc."},
    {CompositingReason::kBackfaceInvisibility3DAncestor,
     "BackfaceInvisibility3DAncestor",
     "Ancestor in same 3D rendering context has a hidden backface"},
    {CompositingReason::kTransform3DSceneLeaf, "Transform3DSceneLeaf",
     "Leaf of a 3D scene, for flattening its descendants into that scene"},
    {CompositingReason::kViewTransitionSharedElement,
     "ViewTransitionSharedElement",
     "This element is shared during view transition"},
    {CompositingReason::kViewTransitionPseudoElement,
     "ViewTransitionContentElement",
     "This element is a part of a pseudo element tree representing the shared "
     "element transition"},
};

}  // anonymous namespace

std::vector<const char*> CompositingReason::ShortNames(
    CompositingReasons reasons) {
#define V(name)                                                             \
  static_assert(                                                            \
      CompositingReason::k##name ==                                         \
          kCompositingReasonsStringMap[CompositingReason::kE##name].reason, \
      "kCompositingReasonsStringMap needs update for "                      \
      "CompositingReason::k" #name);                                        \
  FOR_EACH_COMPOSITING_REASON(V)
#undef V

  std::vector<const char*> result;
  if (reasons == kNone)
    return result;
  for (auto& map : kCompositingReasonsStringMap) {
    if (reasons & map.reason)
      result.push_back(map.short_name);
  }
  return result;
}

std::vector<const char*> CompositingReason::Descriptions(
    CompositingReasons reasons) {
  std::vector<const char*> result;
  if (reasons == kNone)
    return result;
  for (auto& map : kCompositingReasonsStringMap) {
    if (reasons & map.reason)
      result.push_back(map.description);
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
