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
          State{
              FloatSize(), &ScrollPaintPropertyNode::Root(),
              State::Flags{false /* flattens_inherited_transform */,
                           false /* affected_by_outer_viewport_bounds_delta */,
                           false /* in_subtree_of_page_scale */}},
          true /* is_parent_alias */)));
  return *root;
}

const TransformPaintPropertyNode&
TransformPaintPropertyNode::NearestScrollTranslationNode() const {
  const auto* transform = this;
  while (!transform->ScrollNode()) {
    transform = transform->Parent();
    // The transform should never be null because the root transform has an
    // associated scroll node (see: TransformPaintPropertyNode::Root()).
    DCHECK(transform);
  }
  return *transform;
}

bool TransformPaintPropertyNode::Changed(
    PaintPropertyChangeType change,
    const TransformPaintPropertyNode& relative_to_node) const {
  for (const auto* node = this; node; node = node->Parent()) {
    if (node == &relative_to_node)
      return false;
    if (node->NodeChanged() >= change)
      return true;
  }

  // |this| is not a descendant of |relative_to_node|. We have seen no changed
  // flag from |this| to the root. Now check |relative_to_node| to the root.
  return relative_to_node.Changed(change, Root());
}

std::unique_ptr<JSONObject> TransformPaintPropertyNode::ToJSON() const {
  auto json = std::make_unique<JSONObject>();
  if (Parent())
    json->SetString("parent", String::Format("%p", Parent()));
  if (NodeChanged() != PaintPropertyChangeType::kUnchanged)
    json->SetString("changed", PaintPropertyChangeTypeToString(NodeChanged()));
  if (IsIdentityOr2DTranslation()) {
    if (!Translation2D().IsZero())
      json->SetString("translation2d", Translation2D().ToString());
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
  return json;
}

size_t TransformPaintPropertyNode::CacheMemoryUsageInBytes() const {
  size_t total_bytes = sizeof(*this);
  if (transform_cache_)
    total_bytes += sizeof(*transform_cache_);
  if (Parent())
    total_bytes += Parent()->CacheMemoryUsageInBytes();
  return total_bytes;
}

}  // namespace blink
