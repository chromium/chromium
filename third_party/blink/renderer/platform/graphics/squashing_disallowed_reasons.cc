// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/squashing_disallowed_reasons.h"

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

struct SquashingDisallowedReasonStringMap {
  SquashingDisallowedReasons reason;
  const char* short_name;
  const char* description;
};

constexpr SquashingDisallowedReasonStringMap
    kSquashingDisallowedReasonsStringMap[] = {
        {SquashingDisallowedReason::kScrollsWithRespectToSquashingLayer,
         "scrollsWithRespectToSquashingLayer",
         "Cannot be squashed since this layer scrolls with respect to the "
         "squashing layer"},
        {SquashingDisallowedReason::kSquashingSparsityExceeded,
         "squashingSparsityExceeded",
         "Cannot be squashed as the squashing layer would become too sparse"},
        {SquashingDisallowedReason::kClippingContainerMismatch,
         "squashingClippingContainerMismatch",
         "Cannot be squashed because this layer has a different clipping "
         "container than the squashing layer"},
        {SquashingDisallowedReason::kOpacityAncestorMismatch,
         "squashingOpacityAncestorMismatch",
         "Cannot be squashed because this layer has a different opacity "
         "ancestor than the squashing layer"},
        {SquashingDisallowedReason::kTransformAncestorMismatch,
         "squashingTransformAncestorMismatch",
         "Cannot be squashed because this layer has a different transform "
         "ancestor than the squashing layer"},
        {SquashingDisallowedReason::kFilterMismatch,
         "squashingFilterAncestorMismatch",
         "Cannot be squashed because this layer has a different filter "
         "ancestor than the squashing layer, or this layer has a filter"},
        {SquashingDisallowedReason::kWouldBreakPaintOrder,
         "squashingWouldBreakPaintOrder",
         "Cannot be squashed without breaking paint order"},
        {SquashingDisallowedReason::kSquashingVideoIsDisallowed,
         "squashingVideoIsDisallowed", "Squashing video is not supported"},
        {SquashingDisallowedReason::kSquashingLayoutEmbeddedContentIsDisallowed,
         "squashingLayoutEmbeddedContentIsDisallowed",
         "Squashing a frame, iframe or plugin is not supported."},
        {SquashingDisallowedReason::kSquashingBlendingIsDisallowed,
         "squashingBlendingDisallowed",
         "Squashing a layer with blending is not supported."},
        {SquashingDisallowedReason::kNearestFixedPositionMismatch,
         "squashingNearestFixedPositionMismatch",
         "Cannot be squashed because this layer has a different nearest fixed "
         "position layer than the squashing layer"},
        {SquashingDisallowedReason::kScrollChildWithCompositedDescendants,
         "scrollChildWithCompositedDescendants",
         "Squashing a scroll child with composited descendants is not "
         "supported."},
        {SquashingDisallowedReason::kSquashingLayerIsAnimating,
         "squashingLayerIsAnimating",
         "Cannot squash into a layer that is animating."},
        {SquashingDisallowedReason::kRenderingContextMismatch,
         "squashingLayerRenderingContextMismatch",
         "Cannot squash layers with different 3D contexts."},
        {SquashingDisallowedReason::kFragmentedContent,
         "SquashingDisallowedReasonFragmentedContent",
         "Cannot squash layers that are inside fragmentation contexts."},
        {SquashingDisallowedReason::kClipPathMismatch,
         "SquashingDisallowedReasonClipPathMismatch",
         "Cannot squash layers across clip-path boundaries."},
        {SquashingDisallowedReason::kMaskMismatch,
         "SquashingDisallowedReasonMaskMismatch",
         "Cannot squash layers across mask boundaries."},
        {SquashingDisallowedReason::kCrossesLayoutContainmentBoundary,
         "SquashingDisallowedReasonCrossesLayoutContainmentBoundary",
         "Cannot squash layer across layout containment boundary."}};

}  // anonymous namespace

Vector<const char*> SquashingDisallowedReason::ShortNames(
    SquashingDisallowedReasons reasons) {
#define V(name)                                                          \
  static_assert(SquashingDisallowedReason::k##name ==                    \
                    kSquashingDisallowedReasonsStringMap                 \
                        [SquashingDisallowedReason::kE##name]            \
                            .reason,                                     \
                "kSquashingDisallowedReasonsStringMap needs update for " \
                "SquashingDisallowedReason::k" #name);                   \
  FOR_EACH_COMPOSITING_REASON(V)
#undef V

  Vector<const char*> result;
  if (reasons == kNone)
    return result;
  for (auto& map : kSquashingDisallowedReasonsStringMap) {
    if (reasons & map.reason)
      result.push_back(map.short_name);
  }
  return result;
}

Vector<const char*> SquashingDisallowedReason::Descriptions(
    SquashingDisallowedReasons reasons) {
  Vector<const char*> result;
  if (reasons == kNone)
    return result;
  for (auto& map : kSquashingDisallowedReasonsStringMap) {
    if (reasons & map.reason)
      result.push_back(map.description);
  }
  return result;
}

}  // namespace blink
