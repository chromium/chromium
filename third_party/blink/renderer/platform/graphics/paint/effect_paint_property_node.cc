// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

const EffectPaintPropertyNode& EffectPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(EffectPaintPropertyNode, root,
                    base::AdoptRef(new EffectPaintPropertyNode(
                        nullptr,
                        State{&TransformPaintPropertyNode::Root(),
                              &ClipPaintPropertyNode::Root()},
                        true /* is_parent_alias */)));
  return *root;
}

FloatRect EffectPaintPropertyNode::MapRect(const FloatRect& input_rect) const {
  if (state_.filter.IsEmpty())
    return input_rect;

  FloatRect rect = input_rect;
  rect.MoveBy(-state_.filters_origin);
  FloatRect result = state_.filter.MapRect(rect);
  result.MoveBy(state_.filters_origin);
  return result;
}

bool EffectPaintPropertyNode::Changed(
    PaintPropertyChangeType change,
    const PropertyTreeState& relative_to_state,
    const TransformPaintPropertyNode* transform_not_to_check) const {
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

    const auto& local_transform = node->LocalTransformSpace();
    if (node->HasFilterThatMovesPixels() &&
        &local_transform != transform_not_to_check &&
        local_transform.Changed(change, relative_transform)) {
      return true;
    }
    // We don't check for change of OutputClip here to avoid N^3 complexity.
    // The caller should check for clip change in other ways.
  }

  return false;
}

std::unique_ptr<JSONObject> EffectPaintPropertyNode::ToJSON() const {
  auto json = std::make_unique<JSONObject>();
  if (Parent())
    json->SetString("parent", String::Format("%p", Parent()));
  if (NodeChanged() != PaintPropertyChangeType::kUnchanged)
    json->SetString("changed", PaintPropertyChangeTypeToString(NodeChanged()));
  json->SetString("localTransformSpace",
                  String::Format("%p", state_.local_transform_space.get()));
  json->SetString("outputClip", String::Format("%p", state_.output_clip.get()));
  if (state_.color_filter != kColorFilterNone)
    json->SetInteger("colorFilter", state_.color_filter);
  if (!state_.filter.IsEmpty())
    json->SetString("filter", state_.filter.ToString());
  if (!state_.backdrop_filter.IsEmpty())
    json->SetString("backdrop_filter", state_.backdrop_filter.ToString());
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
  if (state_.filters_origin != FloatPoint())
    json->SetString("filtersOrigin", state_.filters_origin.ToString());
  return json;
}

}  // namespace blink
