// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

const EffectPaintPropertyNode& EffectPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(EffectPaintPropertyNode, root,
                    base::AdoptRef(new EffectPaintPropertyNode(
                        nullptr, State{&TransformPaintPropertyNode::Root(),
                                       &ClipPaintPropertyNode::Root()})));
  return *root;
}

bool EffectPaintPropertyNodeOrAlias::Changed(
    PaintPropertyChangeType change,
    const PropertyTreeState& relative_to_state,
    const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const {
  const auto& relative_effect = relative_to_state.Effect();
  const auto& relative_transform = relative_to_state.Transform();

  // Note that we can't unalias nodes in the loop conditions, since we need to
  // check NodeChanged() function on aliased nodes as well (since the parenting
  // might change).
  for (const auto* node = this; node && node != &relative_effect;
       node = node->Parent()) {
    if (node->NodeChanged() >= change)
      return true;

    // We shouldn't check state on aliased nodes, other than NodeChanged().
    if (node->IsParentAlias())
      continue;

    const auto* unaliased = static_cast<const EffectPaintPropertyNode*>(node);
    const auto& local_transform = unaliased->LocalTransformSpace();
    if (unaliased->HasFilterThatMovesPixels() &&
        &local_transform != transform_not_to_check &&
        local_transform.Changed(change, relative_transform)) {
      return true;
    }
    // We don't check for change of OutputClip here to avoid N^3 complexity.
    // The caller should check for clip change in other ways.
  }

  return false;
}

gfx::RectF EffectPaintPropertyNode::MapRect(const gfx::RectF& rect) const {
  if (state_.filter.IsEmpty())
    return rect;
  return state_.filter.MapRect(rect);
}

std::unique_ptr<JSONObject> EffectPaintPropertyNode::ToJSON() const {
  auto json = ToJSONBase();
  json->SetString("localTransformSpace",
                  String::Format("%p", state_.local_transform_space.get()));
  json->SetString("outputClip", String::Format("%p", state_.output_clip.get()));
  if (!state_.filter.IsEmpty())
    json->SetString("filter", state_.filter.ToString());
  if (auto* backdrop_filter = BackdropFilter())
    json->SetString("backdrop_filter", backdrop_filter->ToString());
  if (state_.opacity != 1.0f)
    json->SetDouble("opacity", state_.opacity);
  if (state_.blend_mode != SkBlendMode::kSrcOver)
    json->SetString("blendMode", SkBlendMode_Name(state_.blend_mode));
  if (state_.direct_compositing_reasons != CompositingReason::kNone) {
    json->SetString(
        "directCompositingReasons",
        CompositingReason::ToString(state_.direct_compositing_reasons));
  }
  if (state_.compositor_element_id) {
    json->SetString("compositorElementId",
                    state_.compositor_element_id.ToString().c_str());
  }
  return json;
}

}  // namespace blink
