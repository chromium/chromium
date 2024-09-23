// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

#include <memory>

namespace blink {

namespace {

using IsCompositedScrollFunction =
    PropertyTreeState::IsCompositedScrollFunction;

const TransformPaintPropertyNode* NearestCompositedScrollTranslation(
    const TransformPaintPropertyNode& scroll_translation,
    IsCompositedScrollFunction is_composited_scroll) {
  for (auto* t = &scroll_translation; t->Parent();
       t = &t->UnaliasedParent()->NearestScrollTranslationNode()) {
    if (is_composited_scroll(*t)) {
      return t;
    }
  }
  return nullptr;
}

bool InSameTransformCompositingBoundary(
    const TransformPaintPropertyNode& t1,
    const TransformPaintPropertyNode& t2,
    IsCompositedScrollFunction is_composited_scroll) {
  const auto* composited_ancestor1 = t1.NearestDirectlyCompositedAncestor();
  const auto* composited_ancestor2 = t2.NearestDirectlyCompositedAncestor();
  if (composited_ancestor1 != composited_ancestor2) {
    return false;
  }
  // There may be indirectly composited scroll translations below the common
  // nearest directly composited ancestor. Check if t1 and t2 have the same
  // nearest composited scroll translation.
  const auto& scroll_translation1 = t1.NearestScrollTranslationNode();
  const auto& scroll_translation2 = t2.NearestScrollTranslationNode();
  if (&scroll_translation1 == &scroll_translation2) {
    return true;
  }
  return NearestCompositedScrollTranslation(scroll_translation1,
                                            is_composited_scroll) ==
         NearestCompositedScrollTranslation(scroll_translation2,
                                            is_composited_scroll);
}

bool ClipChainInTransformCompositingBoundary(
    const ClipPaintPropertyNode& node,
    const ClipPaintPropertyNode& ancestor,
    const TransformPaintPropertyNode& transform,
    IsCompositedScrollFunction is_composited_scroll) {
  for (const auto* n = &node; n != &ancestor; n = n->UnaliasedParent()) {
    if (!InSameTransformCompositingBoundary(transform,
                                            n->LocalTransformSpace().Unalias(),
                                            is_composited_scroll)) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::optional<PropertyTreeState> PropertyTreeState::CanUpcastWith(
    const PropertyTreeState& guest,
    IsCompositedScrollFunction is_composited_scroll) const {
  // A number of criteria need to be met:
  //   1. The guest effect must be a descendant of the home effect. However this
  // check is enforced by the layerization recursion. Here we assume the guest
  // has already been upcasted to the same effect.
  //   2. The guest transform and the home transform have compatible backface
  // visibility.
  //   3. The guest transform space must be within compositing boundary of the
  // home transform space.
  //   4. The local space of each clip on the ancestor chain must be within
  // compositing boundary of the home transform space.
  DCHECK_EQ(&Effect(), &guest.Effect());

  const TransformPaintPropertyNode* upcast_transform = nullptr;
  // Fast-path for the common case of the transform state being equal.
  if (&Transform() == &guest.Transform()) {
    upcast_transform = &Transform();
  } else {
    if (!InSameTransformCompositingBoundary(Transform(), guest.Transform(),
                                            is_composited_scroll)) {
      return std::nullopt;
    }
    if (Transform().IsBackfaceHidden() !=
        guest.Transform().IsBackfaceHidden()) {
      return std::nullopt;
    }
    upcast_transform =
        &Transform().LowestCommonAncestor(guest.Transform()).Unalias();
  }

  const ClipPaintPropertyNode* upcast_clip = nullptr;
  if (&Clip() == &guest.Clip()) {
    upcast_clip = &Clip();
  } else {
    upcast_clip = &Clip().LowestCommonAncestor(guest.Clip()).Unalias();
    if (!ClipChainInTransformCompositingBoundary(
            Clip(), *upcast_clip, *upcast_transform, is_composited_scroll) ||
        !ClipChainInTransformCompositingBoundary(guest.Clip(), *upcast_clip,
                                                 *upcast_transform,
                                                 is_composited_scroll)) {
      return std::nullopt;
    }
  }

  return PropertyTreeState(*upcast_transform, *upcast_clip, Effect());
}

String PropertyTreeStateOrAlias::ToString() const {
  return String::Format("t:%p c:%p e:%p", transform_, clip_, effect_);
}

#if DCHECK_IS_ON()

String PropertyTreeStateOrAlias::ToTreeString() const {
  return "transform:\n" + Transform().ToTreeString() + "\nclip:\n" +
         Clip().ToTreeString() + "\neffect:\n" + Effect().ToTreeString();
}

#endif

std::unique_ptr<JSONObject> PropertyTreeStateOrAlias::ToJSON() const {
  std::unique_ptr<JSONObject> result = std::make_unique<JSONObject>();
  result->SetObject("transform", transform_->ToJSON());
  result->SetObject("clip", clip_->ToJSON());
  result->SetObject("effect", effect_->ToJSON());
  return result;
}

}  // namespace blink
