// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"

namespace blink {

// The root of the transform tree. The root transform node references the root
// scroll node.
const TransformPaintPropertyNode& TransformPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(
      TransformPaintPropertyNode, root,
      base::AdoptRef(new TransformPaintPropertyNode(
          nullptr,
          State{gfx::Vector2dF(), &ScrollPaintPropertyNode::Root(), nullptr,
                State::Flags{false /* flattens_inherited_transform */,
                             false /* in_subtree_of_page_scale */}})));
  return *root;
}

bool TransformPaintPropertyNodeOrAlias::Changed(
    PaintPropertyChangeType change,
    const TransformPaintPropertyNodeOrAlias& relative_to_node) const {
  for (const auto* node = this; node; node = node->Parent()) {
    if (node == &relative_to_node)
      return false;
    if (node->NodeChanged() >= change)
      return true;
  }

  // |this| is not a descendant of |relative_to_node|. We have seen no changed
  // flag from |this| to the root. Now check |relative_to_node| to the root.
  return relative_to_node.Changed(change, TransformPaintPropertyNode::Root());
}

const TransformPaintPropertyNode&
TransformPaintPropertyNode::NearestScrollTranslationNode() const {
  const auto* transform = this;
  while (!transform->ScrollNode()) {
    transform = transform->UnaliasedParent();
    // The transform should never be null because the root transform has an
    // associated scroll node (see: TransformPaintPropertyNode::Root()).
    DCHECK(transform);
  }
  return *transform;
}

std::unique_ptr<JSONObject> TransformPaintPropertyNode::ToJSON() const {
  auto json = ToJSONBase();
  if (IsIdentityOr2DTranslation()) {
    if (!Translation2D().IsZero())
      json->SetString("translation2d", String(Translation2D().ToString()));
  } else {
    json->SetString("matrix", Matrix().ToString());
    json->SetString("origin", Origin().ToString());
  }
  if (!state_.flags.flattens_inherited_transform)
    json->SetBoolean("flattensInheritedTransform", false);
  if (!state_.flags.in_subtree_of_page_scale)
    json->SetBoolean("in_subtree_of_page_scale", false);
  if (state_.backface_visibility != BackfaceVisibility::kInherited) {
    json->SetString("backface",
                    state_.backface_visibility == BackfaceVisibility::kVisible
                        ? "visible"
                        : "hidden");
  }
  if (state_.rendering_context_id) {
    json->SetString("renderingContextId",
                    String::Format("%x", state_.rendering_context_id));
  }
  if (state_.direct_compositing_reasons != CompositingReason::kNone) {
    json->SetString(
        "directCompositingReasons",
        CompositingReason::ToString(state_.direct_compositing_reasons));
  }
  if (state_.compositor_element_id) {
    json->SetString("compositorElementId",
                    state_.compositor_element_id.ToString().c_str());
  }
  if (state_.scroll)
    json->SetString("scroll", String::Format("%p", state_.scroll.get()));

  if (state_.scroll_translation_for_fixed) {
    json->SetString(
        "scroll_translation_for_fixed",
        String::Format("%p", state_.scroll_translation_for_fixed.get()));
  }
  return json;
}

}  // namespace blink
