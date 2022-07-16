// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

#include <memory>

namespace blink {

namespace {

bool HasCompositedTransformToAncestor(
    const TransformPaintPropertyNode& node,
    const TransformPaintPropertyNode& ancestor) {
  for (const auto* n = &node; n != &ancestor; n = n->UnaliasedParent()) {
    if (n->HasDirectCompositingReasons())
      return true;
  }
  return false;
}

// Returns the lowest common ancestor if there is no composited transform
// between the two transforms.
const TransformPaintPropertyNode* NonCompositedLowestCommonAncestor(
    const TransformPaintPropertyNode& transform1,
    const TransformPaintPropertyNode& transform2) {
  const auto& lca = transform1.LowestCommonAncestor(transform2).Unalias();
  if (HasCompositedTransformToAncestor(transform1, lca) ||
      HasCompositedTransformToAncestor(transform2, lca))
    return nullptr;
  return &lca;
}

bool ClipChainHasCompositedTransformTo(
    const ClipPaintPropertyNode& node,
    const ClipPaintPropertyNode& ancestor,
    const TransformPaintPropertyNode& transform) {
  for (const auto* n = &node; n != &ancestor; n = n->UnaliasedParent()) {
    if (!NonCompositedLowestCommonAncestor(n->LocalTransformSpace().Unalias(),
                                           transform))
      return true;
  }
  return false;
}
}  // namespace

const PropertyTreeState& PropertyTreeStateOrAlias::Root() {
  DEFINE_STATIC_LOCAL(
      const PropertyTreeState, root,
      (TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
       EffectPaintPropertyNode::Root()));
  return root;
}

bool PropertyTreeStateOrAlias::Changed(
    PaintPropertyChangeType change,
    const PropertyTreeState& relative_to) const {
  return Transform().Changed(change, relative_to.Transform()) ||
         Clip().Changed(change, relative_to, &Transform()) ||
         Effect().Changed(change, relative_to, &Transform());
}

absl::optional<PropertyTreeState> PropertyTreeState::CanUpcastWith(
    const PropertyTreeState& guest) const {
  // A number of criteria need to be met:
  //   1. The guest effect must be a descendant of the home effect. However this
  // check is enforced by the layerization recursion. Here we assume the guest
  // has already been upcasted to the same effect.
  //   2. The guest transform and the home transform have compatible backface
  // visibility.
  //   3. The guest transform space must be within compositing boundary of the
  // home transform space.
  //   4. The local space of each clip and effect node on the ancestor chain
  // must be within compositing boundary of the home transform space.
  DCHECK_EQ(&Effect(), &guest.Effect());

  if (Transform().IsBackfaceHidden() != guest.Transform().IsBackfaceHidden())
    return absl::nullopt;

  auto* upcast_transform =
      NonCompositedLowestCommonAncestor(Transform(), guest.Transform());
  if (!upcast_transform)
    return absl::nullopt;

  const auto& clip_lca = Clip().LowestCommonAncestor(guest.Clip()).Unalias();
  if (ClipChainHasCompositedTransformTo(Clip(), clip_lca, *upcast_transform) ||
      ClipChainHasCompositedTransformTo(guest.Clip(), clip_lca,
                                        *upcast_transform))
    return absl::nullopt;

  return PropertyTreeState(*upcast_transform, clip_lca, Effect());
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

std::ostream& operator<<(std::ostream& os,
                         const PropertyTreeStateOrAlias& state) {
  return os << state.ToString().Utf8();
}

}  // namespace blink
