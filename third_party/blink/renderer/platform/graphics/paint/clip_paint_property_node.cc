// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

PaintPropertyChangeType ClipPaintPropertyNode::State::ComputeChange(
    const State& other) const {
  if (local_transform_space != other.local_transform_space ||
      paint_clip_rect_ != other.paint_clip_rect_ ||
      !ClipPathEquals(other.clip_path) ||
      pixel_moving_filter != other.pixel_moving_filter) {
    return PaintPropertyChangeType::kChangedOnlyValues;
  }
  if (layout_clip_rect_excluding_overlay_scrollbars !=
      other.layout_clip_rect_excluding_overlay_scrollbars) {
    return PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  }
  return PaintPropertyChangeType::kUnchanged;
}

const ClipPaintPropertyNode& ClipPaintPropertyNode::Root() {
  DEFINE_STATIC_REF(ClipPaintPropertyNode, root,
                    base::AdoptRef(new ClipPaintPropertyNode(
                        nullptr, State(&TransformPaintPropertyNode::Root(),
                                       gfx::RectF(InfiniteIntRect()),
                                       FloatRoundedRect(InfiniteIntRect())))));
  return *root;
}

bool ClipPaintPropertyNodeOrAlias::Changed(
    PaintPropertyChangeType change,
    const PropertyTreeState& relative_to_state,
    const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const {
  for (const auto* node = this; node && node != &relative_to_state.Clip();
       node = node->Parent()) {
    if (node->NodeChanged() >= change)
      return true;
    if (node->IsParentAlias())
      continue;
    const auto* unaliased = static_cast<const ClipPaintPropertyNode*>(node);
    if (&unaliased->LocalTransformSpace() != transform_not_to_check &&
        unaliased->LocalTransformSpace().Changed(change,
                                                 relative_to_state.Transform()))
      return true;
  }

  return false;
}

void ClipPaintPropertyNodeOrAlias::ClearChangedToRoot(
    int sequence_number) const {
  for (auto* n = this; n && n->ChangedSequenceNumber() != sequence_number;
       n = n->Parent()) {
    n->ClearChanged(sequence_number);
    if (n->IsParentAlias())
      continue;
    static_cast<const ClipPaintPropertyNode*>(n)
        ->LocalTransformSpace()
        .ClearChangedToRoot(sequence_number);
  }
}

std::unique_ptr<JSONObject> ClipPaintPropertyNode::ToJSON() const {
  auto json = ToJSONBase();
  if (NodeChanged() != PaintPropertyChangeType::kUnchanged)
    json->SetString("changed", PaintPropertyChangeTypeToString(NodeChanged()));
  json->SetString("localTransformSpace",
                  String::Format("%p", state_.local_transform_space.get()));
  json->SetString("rect", String(state_.paint_clip_rect_.Rect().ToString()));
  if (state_.layout_clip_rect_excluding_overlay_scrollbars) {
    json->SetString(
        "rectExcludingOverlayScrollbars",
        String(state_.layout_clip_rect_excluding_overlay_scrollbars->Rect()
                   .ToString()));
  }
  if (state_.clip_path) {
    json->SetBoolean("hasClipPath", true);
  }
  if (state_.pixel_moving_filter) {
    json->SetString("pixelMovingFilter",
                    String::Format("%p", state_.pixel_moving_filter.get()));
  }
  return json;
}

}  // namespace blink
